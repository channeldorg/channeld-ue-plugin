#include "ChanneldSettingsDetails.h"

#include "ChanneldEditor.h"
#include "ChanneldSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"


TSharedRef<IDetailCustomization> FChanneldSettingsDetails::MakeInstance()
{
	return MakeShareable(new FChanneldSettingsDetails);
}

void FChanneldSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Spatial|Client Interest");
	auto PresetHandles = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UChanneldSettings, ClientInterestPresets))->AsArray();
	uint32 NumPresets;
	PresetHandles->GetNumElements(NumPresets);
	for (uint32 i = 0; i < NumPresets; i++)
	{
		auto PresetHandle = PresetHandles->GetElement(i);
		auto AreaTypeHandle = PresetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, AreaType));
		auto SpotsHandle = PresetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, SpotsAndDists));
		auto ExtentHandle = PresetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Extent));
		auto RadiusHandle = PresetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Radius));
		auto AngleHandle = PresetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Angle));
		AreaTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, AreaTypeHandle, SpotsHandle, ExtentHandle, RadiusHandle, AngleHandle]()
		{
			IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get();
			if (!DetailBuilder)
			{
				return;
			}
			
			uint8 uAreaType;
			AreaTypeHandle->GetValue(uAreaType);
			EClientInterestAreaType AreaType = static_cast<EClientInterestAreaType>(uAreaType);
			UE_LOG(LogChanneldEditor, Log, TEXT("AreaType: %d"), AreaType);
			if (AreaType == EClientInterestAreaType::StaticLocations)
			{
				// DetailBuilder->HideProperty(RadiusHandle);
				// DetailBuilder->HideProperty(ExtentHandle);
				// DetailBuilder->HideProperty(AngleHandle);
				RadiusHandle->MarkHiddenByCustomization();
				ExtentHandle->MarkHiddenByCustomization();
				AngleHandle->MarkHiddenByCustomization();
				
			}
			else if (AreaType == EClientInterestAreaType::Box)
			{
				SpotsHandle->MarkHiddenByCustomization();
				RadiusHandle->MarkHiddenByCustomization();
				AngleHandle->MarkHiddenByCustomization();
			}
			else if (AreaType == EClientInterestAreaType::Sphere)
			{
				SpotsHandle->MarkHiddenByCustomization();
				ExtentHandle->MarkHiddenByCustomization();
				AngleHandle->MarkHiddenByCustomization();
			}
			else if (AreaType == EClientInterestAreaType::Cone)
			{
				SpotsHandle->MarkHiddenByCustomization();
				ExtentHandle->MarkHiddenByCustomization();
			}
			else
			{
				SpotsHandle->MarkHiddenByCustomization();
				RadiusHandle->MarkHiddenByCustomization();
				ExtentHandle->MarkHiddenByCustomization();
				AngleHandle->MarkHiddenByCustomization();
			}
			
			DetailBuilder->ForceRefreshDetails();
		}));
	}
}

void FChanneldSettingsDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	CachedDetailBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}
