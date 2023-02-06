#pragma once

class IPropertyDecoratorOwner
{
public:
	virtual bool IsBlueprintType() = 0;

	virtual FString GetProtoPackageName() = 0;

	virtual FString GetProtoNamespace() = 0;

	virtual FString GetProtoStateMessageType() = 0;

	virtual TArray<FString> GetAdditionalIncludes() = 0;

	virtual FString GetCode_GetWorldRef() = 0;

};
