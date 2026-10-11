// CoreFramework UI System
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "BaseTabButton.generated.h"

class UCommonTextBlock;

/**
 * Button class used by UBaseMenuWidget for its tab bar. Create a Blueprint child, style it,
 * and add a Common Text Block named exactly TabLabelText (or override On Tab Label Changed).
 */
UCLASS(Abstract, Blueprintable)
class COREFRAMEWORK_API UBaseTabButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Base UI|Tab")
	void SetTabLabel(const FText& InLabel);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Base UI|Tab", meta = (DisplayName = "On Tab Label Changed"))
	void BP_OnTabLabelChanged(const FText& InLabel);

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TabLabelText;
};
