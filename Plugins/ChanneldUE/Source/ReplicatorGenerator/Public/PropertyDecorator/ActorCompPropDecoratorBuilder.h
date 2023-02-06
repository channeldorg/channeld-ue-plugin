#pragma once
#include "PropertyDecoratorBuilder.h"

class FActorCompPropDecoratorBuilder : public FPropertyDecoratorBuilder
{
public:
	virtual bool IsSpecialProperty(FProperty*) override;

protected:
	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty*, IPropertyDecoratorOwner*) override;
};
