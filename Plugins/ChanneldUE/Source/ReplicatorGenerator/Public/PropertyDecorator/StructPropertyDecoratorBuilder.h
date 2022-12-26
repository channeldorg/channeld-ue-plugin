#pragma once
#include "PropertyDecoratorBuilder.h"
#include "StructPropertyDecorator.h"

class FStructPropertyDecoratorBuilder : public FPropertyDecoratorBuilder, public FGCObject
{
public:
	virtual ~FStructPropertyDecoratorBuilder() override = default;

	virtual bool IsSpecialProperty(FProperty* Property) override
	{
		return Property->IsA<FStructProperty>();
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	TArray<TSharedPtr<FStructPropertyDecorator>> GetGlobalStructs();

	void ClearGlobalStructs();

protected:
	TMap<UScriptStruct*, TSharedPtr<FStructPropertyDecorator>> GlobalStructs;

	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty*, IPropertyDecoratorOwner*) override;
};
