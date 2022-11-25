#pragma once
#include "PropertyDecorator.h"

class FRPCDecorator
{
public:
	FRPCDecorator(FName);
	FRPCDecorator(UFunction*);
	
protected:
	UFunction* OriginalFunction;
	FName FunctionName;
	TArray<TSharedPtr<FPropertyDecorator>> FunctionParams;
};
