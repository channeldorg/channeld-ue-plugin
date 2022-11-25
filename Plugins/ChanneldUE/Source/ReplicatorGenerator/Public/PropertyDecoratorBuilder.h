#pragma once

class FPropertyDecorator;

class FPropertyDecoratorBuilder
{
public:
	virtual ~FPropertyDecoratorBuilder() = default;

	virtual TSharedPtr<FPropertyDecoratorBuilder> SetNextBuilder(TSharedPtr<FPropertyDecoratorBuilder>);

	
	virtual bool IsSpecialTarget(FProperty*) = 0;
	virtual FPropertyDecorator* GetPropertyDecorator(FProperty*);
	
	// template<typename PropertyType>
	// bool HandleTypeOf<PropertyType>();
	//
protected:
	TSharedPtr<FPropertyDecoratorBuilder> NextBuilder;
	virtual FPropertyDecorator* ConstructPropertyDecorator(FProperty*);
	virtual FPropertyDecorator* DoNext(FProperty*);

};

