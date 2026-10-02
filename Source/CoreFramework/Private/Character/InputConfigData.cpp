// InputConfigData.cpp
// Character Framework - data-driven Enhanced Input configuration.

#include "Character/InputConfigData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UInputConfigData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!DefaultMappingContext)
	{
		Context.AddError(FText::FromString(TEXT("InputConfigData: DefaultMappingContext is not set.")));
		Result = EDataValidationResult::Invalid;
	}

	if (!Actions.Move)
	{
		Context.AddError(FText::FromString(TEXT("InputConfigData: the Move action is required.")));
		Result = EDataValidationResult::Invalid;
	}

	if (!Actions.Look)
	{
		Context.AddError(FText::FromString(TEXT("InputConfigData: the Look action is required.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
