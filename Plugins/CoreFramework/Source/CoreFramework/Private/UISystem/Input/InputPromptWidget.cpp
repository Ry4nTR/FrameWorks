// CoreFramework UI System - Input
#include "UISystem/Input/InputPromptWidget.h"

#include "CommonInputBaseTypes.h"
#include "CommonTextBlock.h"
#include "CommonUITypes.h"
#include "Components/Image.h"
#include "InputAction.h"
#include "InputSystem/Core/InputFrameSettings.h"
#include "InputSystem/Manager/InputFrameSubsystem.h"

UInputPromptWidget::UInputPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A prompt never reacts to the pawn and never takes focus.
	bAutoRegisterAsBinder = false;
	SetIsFocusable(false);
}

void UInputPromptWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		// No local player in the designer: show the look of the preview device with the platform default icons.
		ApplyPrompt(bPreviewGamepadInDesigner, NAME_None, nullptr);
	}
}

void UInputPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(this))
	{
		Input->OnInputDeviceChanged.AddUniqueDynamic(this, &UInputPromptWidget::HandleDeviceChanged);
		Input->OnInputBindingsChanged.AddUniqueDynamic(this, &UInputPromptWidget::HandleBindingsChanged);
		BoundInput = Input;
	}
	RefreshPrompt();
}

void UInputPromptWidget::NativeDestruct()
{
	if (UInputFrameSubsystem* Input = BoundInput.Get())
	{
		Input->OnInputDeviceChanged.RemoveDynamic(this, &UInputPromptWidget::HandleDeviceChanged);
		Input->OnInputBindingsChanged.RemoveDynamic(this, &UInputPromptWidget::HandleBindingsChanged);
	}
	BoundInput.Reset();

	Super::NativeDestruct();
}

void UInputPromptWidget::SetInputAction(UInputAction* NewAction)
{
	InputAction = NewAction;
	RefreshPrompt();
}

void UInputPromptWidget::SetUIAction(FDataTableRowHandle NewUIAction)
{
	UIAction = NewUIAction;
	RefreshPrompt();
}

void UInputPromptWidget::SetKeys(FKey NewKeyboardKey, FKey NewGamepadKey)
{
	KeyboardKey = NewKeyboardKey;
	GamepadKey = NewGamepadKey;
	RefreshPrompt();
}

void UInputPromptWidget::SetLabel(const FText& NewLabel)
{
	Label = NewLabel;
	RefreshPrompt();
}

void UInputPromptWidget::SetCollapseWhenNoKey(bool bCollapse)
{
	if (bCollapseWhenNoKey == bCollapse)
	{
		return;
	}

	bCollapseWhenNoKey = bCollapse;
	if (!bCollapseWhenNoKey && !IsDesignTime())
	{
		// The rule may have collapsed the prompt: it no longer applies.
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	RefreshPrompt();
}

void UInputPromptWidget::RefreshPrompt()
{
	const UInputFrameSubsystem* Input = BoundInput.Get();
	if (!Input)
	{
		Input = UInputFrameSubsystem::Get(this);
	}

	if (Input)
	{
		ApplyPrompt(Input->IsUsingGamepad(), Input->GetCurrentGamepadName(), Input);
	}
	else
	{
		ApplyPrompt(IsDesignTime() && bPreviewGamepadInDesigner, NAME_None, nullptr);
	}
}

void UInputPromptWidget::HandleDeviceChanged(ECommonInputType InputType, FName GamepadName)
{
	RefreshPrompt();
}

void UInputPromptWidget::HandleBindingsChanged()
{
	RefreshPrompt();
}

FKey UInputPromptWidget::ResolveKey(bool bGamepad, const UInputFrameSubsystem* Input) const
{
	const FKey& Explicit = bGamepad ? GamepadKey : KeyboardKey;
	if (Explicit.IsValid())
	{
		return Explicit;
	}

	if (InputAction && Input)
	{
		const FKey ActionKey = Input->GetKeyForAction(InputAction, bGamepad);
		if (ActionKey.IsValid())
		{
			return ActionKey;
		}
	}

	if (!UIAction.IsNull())
	{
		if (const FCommonInputActionDataBase* Row = UIAction.GetRow<FCommonInputActionDataBase>(TEXT("InputPromptWidget")))
		{
			const FName GamepadName = Input ? Input->GetCurrentGamepadName() : NAME_None;
			const FKey RowKey = Row->GetInputTypeInfo(bGamepad ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard, GamepadName).GetKey();
			if (RowKey.IsValid())
			{
				return RowKey;
			}
		}
	}

	return EKeys::Invalid;
}

void UInputPromptWidget::ApplyPrompt(bool bGamepad, FName GamepadName, const UInputFrameSubsystem* Input)
{
	DisplayedKey = ResolveKey(bGamepad, Input);
	bShowingGlyph = false;

	EInputPromptKeyDisplay Display = EInputPromptKeyDisplay::Glyph;
	if (!bGamepad)
	{
		Display = KeyboardDisplay == EInputPromptKeyDisplay::ProjectDefault ? UInputFrameSettings::Get()->KeyboardPromptDisplay : KeyboardDisplay;
	}

	bool bShowText = false;
	if (DisplayedKey.IsValid() && Display != EInputPromptKeyDisplay::None && Display != EInputPromptKeyDisplay::ProjectDefault)
	{
		FSlateBrush Brush;
		if (Display == EInputPromptKeyDisplay::Glyph && GlyphImage && UInputFrameSubsystem::FindKeyGlyph(DisplayedKey, GamepadName, Brush))
		{
			if (GlyphSize.X > 0.0 && GlyphSize.Y > 0.0)
			{
				Brush.ImageSize = GlyphSize;
			}
			GlyphImage->SetBrush(Brush);
			bShowingGlyph = true;
		}
		else
		{
			bShowText = true;
		}
	}

	if (GlyphImage)
	{
		GlyphImage->SetVisibility(bShowingGlyph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (KeyText)
	{
		KeyText->SetText(bShowText ? UInputFrameSubsystem::GetKeyDisplayText(DisplayedKey) : FText::GetEmpty());
		KeyText->SetVisibility(bShowText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LabelText)
	{
		LabelText->SetText(Label);
		LabelText->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (bCollapseWhenNoKey && !IsDesignTime())
	{
		SetVisibility(DisplayedKey.IsValid() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	BP_OnPromptRefreshed(DisplayedKey, bShowingGlyph, bGamepad);
}
