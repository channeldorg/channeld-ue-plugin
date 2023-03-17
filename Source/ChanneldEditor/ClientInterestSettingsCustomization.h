#pragma once

#include "ChanneldTypes.h"
#include "PropertyEditor/Public/IPropertyTypeCustomization.h"

class FClientInterestSettingsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// ~Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// ~End IPropertyTypeCustomization Interface

protected:
	FText AreaTypeText;
	EClientInterestAreaType AreaType = EClientInterestAreaType::None;
	
	void OnTypeChanged(TSharedPtr<IPropertyHandle> TypePropertyHandle);
};