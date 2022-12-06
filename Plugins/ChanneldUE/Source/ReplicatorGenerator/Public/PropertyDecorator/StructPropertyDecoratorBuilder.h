#pragma once
#include "PropertyDecoratorBuilder.h"
#include "StructPropertyDecorator.h"

class FStructPropertyDecoratorBuilder : public FPropertyDecoratorBuilder, public FGCObject
{
public:
	virtual ~FStructPropertyDecoratorBuilder() override
	{
	}

	virtual bool IsSpecialProperty(FProperty* Property) override
	{
		return Property->IsA<FStructProperty>();
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		TArray<UScriptStruct*> GlobalStructArray;
		GlobalStructs.GetKeys(GlobalStructArray);
		Collector.AddReferencedObjects(GlobalStructArray);
	}

	TArray<TSharedPtr<FStructPropertyDecorator>> GetGlobalStructs();

protected:
	TMap<UScriptStruct*, TSharedPtr<FStructPropertyDecorator>> GlobalStructs;

	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty*, IPropertyDecoratorOwner*) override;
};
