// CoreFramework UI System - Settings
#include "UISystem/Settings/KeyBindingButton.h"

#include "Components/Image.h"
#include "InputSystem/Manager/InputFrameSubsystem.h"

#define LOCTEXT_NAMESPACE "CoreFrameworkSettings"

UKeyBindingButton::UKeyBindingButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnboundText = LOCTEXT("KeyUnbound", "-");
}

void UKeyBindingButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		Key = PreviewKey;
	}
	RefreshDisplay();
}

void UKeyBindingButton::SetKey(const FKey& InKey)
{
	Key = InKey;
	RefreshDisplay();
}

void UKeyBindingButton::SetListening(bool bInListening, const FText& ListeningText)
{
	bListening = bInListening;
	CurrentListeningText = ListeningText;
	RefreshDisplay();
}

void UKeyBindingButton::RefreshDisplay()
{
	bool bShowsGlyph = false;

	if (!bListening && Key.IsValid() && KeyGlyph && (Key.IsGamepadKey() ? bGlyphForGamepadKeys : bGlyphForKeyboardKeys))
	{
		const UInputFrameSubsystem* Input = IsDesignTime() ? nullptr : UInputFrameSubsystem::Get(this);
		FSlateBrush Brush;
		if (UInputFrameSubsystem::FindKeyGlyph(Key, Input ? Input->GetCurrentGamepadName() : NAME_None, Brush))
		{
			if (GlyphSize.X > 0.0 && GlyphSize.Y > 0.0)
			{
				Brush.ImageSize = GlyphSize;
			}
			KeyGlyph->SetBrush(Brush);
			bShowsGlyph = true;
		}
	}

	if (KeyGlyph)
	{
		KeyGlyph->SetVisibility(bShowsGlyph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bListening)
	{
		SetButtonLabel(CurrentListeningText);
	}
	else if (bShowsGlyph)
	{
		SetButtonLabel(FText::GetEmpty());
	}
	else
	{
		SetButtonLabel(Key.IsValid() ? UInputFrameSubsystem::GetKeyDisplayText(Key) : UnboundText);
	}

	BP_OnKeyDisplayChanged(Key, bListening, bShowsGlyph);
}

#undef LOCTEXT_NAMESPACE
