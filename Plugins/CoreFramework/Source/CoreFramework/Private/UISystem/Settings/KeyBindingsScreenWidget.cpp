// CoreFramework UI System - Settings
#include "UISystem/Settings/KeyBindingsScreenWidget.h"

#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "InputSystem/Core/InputFrameSettings.h"
#include "InputSystem/Manager/InputFrameSubsystem.h"
#include "UISystem/Foundation/BaseButton.h"
#include "UISystem/Settings/BaseSettingRowWidget.h"
#include "UISystem/Settings/KeyBindingButton.h"
#include "UISystem/Settings/KeyBindingRowWidget.h"

#define LOCTEXT_NAMESPACE "CoreFrameworkSettings"

UKeyBindingsScreenWidget::UKeyBindingsScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputMode = EUIInputMode::UIOnly;
	bIsBackHandler = true;

	ListeningKeyboardText = LOCTEXT("ListeningKeyboard", "Press a key");
	ListeningGamepadText = LOCTEXT("ListeningGamepad", "Press a button");
	SwappedMessage = LOCTEXT("RebindSwapped", "{Key} was used by {Action}: the two keys were swapped.");
	UnboundMessage = LOCTEXT("RebindUnbound", "{Key} was used by {Action}, which now has no key.");
	RejectedMessage = LOCTEXT("RebindRejected", "{Key} is already used by {Action}.");
	ReservedKeyMessage = LOCTEXT("RebindReserved", "{Key} cannot be assigned.");
	ResetMessage = LOCTEXT("RebindReset", "All controls are back to their defaults.");
	UnavailableMessage = LOCTEXT("RebindUnavailable", "Key bindings are not available.");
}

UInputFrameSubsystem* UKeyBindingsScreenWidget::GetInput() const
{
	return BoundInput.IsValid() ? BoundInput.Get() : UInputFrameSubsystem::Get(this);
}

void UKeyBindingsScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ResetAllButton)
	{
		ResetAllButton->OnClicked().AddUObject(this, &UKeyBindingsScreenWidget::HandleResetAllClicked);
	}
}

void UKeyBindingsScreenWidget::NativeOnActivated()
{
	if (UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(this))
	{
		BoundInput = Input;
		Input->OnInputBindingsChanged.AddUniqueDynamic(this, &UKeyBindingsScreenWidget::HandleBindingsChanged);
	}

	SetStatus(FText::GetEmpty());
	RebuildRows();

	// Super last: the default focus (first row) needs the rows.
	Super::NativeOnActivated();
}

void UKeyBindingsScreenWidget::NativeOnDeactivated()
{
	StopListening();

	if (UInputFrameSubsystem* Input = BoundInput.Get())
	{
		Input->OnInputBindingsChanged.RemoveDynamic(this, &UKeyBindingsScreenWidget::HandleBindingsChanged);
	}

	Super::NativeOnDeactivated();
}

void UKeyBindingsScreenWidget::NativeDestruct()
{
	StopListening();
	Super::NativeDestruct();
}

UWidget* UKeyBindingsScreenWidget::ResolveDefaultFocusWidget() const
{
	if (UWidget* Explicit = Super::ResolveDefaultFocusWidget())
	{
		return Explicit;
	}
	for (const UKeyBindingRowWidget* Row : Rows)
	{
		if (Row)
		{
			if (UWidget* Target = Row->GetFocusTarget())
			{
				return Target;
			}
		}
	}
	return ResetAllButton;
}

// ---------------------------------------------------------------------------------------------------------------------
// Rows
// ---------------------------------------------------------------------------------------------------------------------

void UKeyBindingsScreenWidget::RebuildRows()
{
	StopListening();
	Rows.Reset();

	if (!RowsContainer)
	{
		return;
	}
	RowsContainer->ClearChildren();

	const UInputFrameSubsystem* Input = GetInput();
	if (!Input || !Input->AreKeyBindingsAvailable())
	{
		SetStatus(UnavailableMessage);
		UE_LOG(LogCoreInput, Warning, TEXT("%s: no key bindings. Enable Project Settings > Enhanced Input > User Settings and list the contexts in CoreFramework Input > Remappable Contexts."), *GetName());
		return;
	}
	if (!RowClass)
	{
		UE_LOG(LogCoreInput, Error, TEXT("%s: Row Class is not set."), *GetName());
		return;
	}

	FText CurrentCategory;
	bool bFirstRow = true;
	for (const FInputFrameKeyBinding& Binding : Input->GetKeyBindings())
	{
		// A header each time the category changes (actions keep the order of the mapping contexts).
		if (CategoryHeaderClass && !Binding.Category.IsEmpty() && (bFirstRow || !Binding.Category.EqualTo(CurrentCategory)))
		{
			if (UBaseSettingRowWidget* Header = CreateWidget<UBaseSettingRowWidget>(this, CategoryHeaderClass))
			{
				Header->SetLabel(Binding.Category);
				RowsContainer->AddChild(Header);
			}
		}
		CurrentCategory = Binding.Category;
		bFirstRow = false;

		UKeyBindingRowWidget* Row = CreateWidget<UKeyBindingRowWidget>(this, RowClass);
		if (!Row)
		{
			continue;
		}
		RowsContainer->AddChild(Row);
		Row->SetBinding(Binding);
		Row->SetColumnsVisible(bShowKeyboardColumn, bShowGamepadColumn);
		Row->OnBindingClicked.AddUniqueDynamic(this, &UKeyBindingsScreenWidget::HandleBindingClicked);
		Rows.Add(Row);
	}

	BP_OnRowsBuilt();
}

void UKeyBindingsScreenWidget::RefreshRows()
{
	const UInputFrameSubsystem* Input = GetInput();
	if (!Input)
	{
		return;
	}

	for (UKeyBindingRowWidget* Row : Rows)
	{
		FInputFrameKeyBinding Binding;
		if (Row && Input->GetKeyBinding(Row->GetMappingName(), Binding))
		{
			Row->SetBinding(Binding);
		}
	}
}

void UKeyBindingsScreenWidget::HandleBindingsChanged()
{
	RefreshRows();
}

// ---------------------------------------------------------------------------------------------------------------------
// Rebinding
// ---------------------------------------------------------------------------------------------------------------------

void UKeyBindingsScreenWidget::HandleBindingClicked(UKeyBindingRowWidget* Row, bool bGamepad)
{
	BeginRebind(Row, bGamepad);
}

void UKeyBindingsScreenWidget::BeginRebind(UKeyBindingRowWidget* Row, bool bGamepad)
{
	UInputFrameSubsystem* Input = GetInput();
	if (!Row || !Input || IsListening())
	{
		return;
	}

	TWeakObjectPtr<UKeyBindingsScreenWidget> WeakThis(this);
	const bool bStarted = Input->BeginKeyCapture(bGamepad, [WeakThis](const FKey& Key, bool bCancelled)
	{
		if (UKeyBindingsScreenWidget* Screen = WeakThis.Get())
		{
			Screen->FinishRebind(Key, bCancelled);
		}
	});

	if (bStarted)
	{
		ListeningRow = Row;
		bListeningGamepad = bGamepad;
		Row->SetListening(bGamepad, true, bGamepad ? ListeningGamepadText : ListeningKeyboardText);
		SetStatus(FText::GetEmpty());
	}
}

void UKeyBindingsScreenWidget::FinishRebind(const FKey& Key, bool bCancelled)
{
	UKeyBindingRowWidget* Row = ListeningRow.Get();
	const bool bGamepad = bListeningGamepad;
	ListeningRow.Reset();

	if (!Row)
	{
		return;
	}
	Row->SetListening(bGamepad, false, FText::GetEmpty());

	EKeyRebindResult Result = EKeyRebindResult::Unchanged;
	FText Message;
	if (!bCancelled)
	{
		if (UInputFrameSubsystem* Input = GetInput())
		{
			FText OtherAction;
			Result = Input->RebindKey(Row->GetMappingName(), bGamepad, Key, OtherAction);

			FFormatNamedArguments Args;
			Args.Add(TEXT("Key"), UInputFrameSubsystem::GetKeyDisplayText(Key));
			Args.Add(TEXT("Action"), OtherAction);

			switch (Result)
			{
			case EKeyRebindResult::Success:
				if (!OtherAction.IsEmpty())
				{
					const EKeyRebindConflictPolicy Policy = UInputFrameSettings::Get()->ConflictPolicy;
					if (Policy == EKeyRebindConflictPolicy::Swap)
					{
						Message = FText::Format(SwappedMessage, Args);
					}
					else if (Policy == EKeyRebindConflictPolicy::Unbind)
					{
						Message = FText::Format(UnboundMessage, Args);
					}
				}
				break;
			case EKeyRebindResult::Rejected:
				Message = FText::Format(RejectedMessage, Args);
				break;
			case EKeyRebindResult::ReservedKey:
				Message = FText::Format(ReservedKeyMessage, Args);
				break;
			default:
				break;
			}
		}
	}

	RefreshRows();
	SetStatus(Message);

	// The capture swallowed all input: give focus back to the box that was edited.
	if (UKeyBindingButton* Button = Row->GetBindingButton(bGamepad))
	{
		Button->SetFocus();
	}

	BP_OnRebindFinished(bCancelled ? EKeyRebindResult::Unchanged : Result, Message);
}

void UKeyBindingsScreenWidget::StopListening()
{
	if (UKeyBindingRowWidget* Row = ListeningRow.Get())
	{
		Row->SetListening(bListeningGamepad, false, FText::GetEmpty());
	}
	ListeningRow.Reset();

	if (UInputFrameSubsystem* Input = BoundInput.Get())
	{
		if (Input->IsCapturingKey())
		{
			Input->CancelKeyCapture();
		}
	}
}

void UKeyBindingsScreenWidget::ResetAllBindings()
{
	StopListening();
	if (UInputFrameSubsystem* Input = GetInput())
	{
		Input->ResetAllKeyBindings();
		RefreshRows();
		SetStatus(ResetMessage);
	}
}

void UKeyBindingsScreenWidget::HandleResetAllClicked()
{
	ResetAllBindings();
}

void UKeyBindingsScreenWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

#undef LOCTEXT_NAMESPACE
