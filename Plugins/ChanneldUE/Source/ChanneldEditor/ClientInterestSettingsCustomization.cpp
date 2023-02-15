#include "ClientInterestSettingsCustomization.h"

#include "ChanneldTypes.h"
#include "SlateBasics.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"


TSharedRef<IPropertyTypeCustomization> FClientInterestSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FClientInterestSettingsCustomization());
}

#define LOCTEXT_NAMESPACE "FChanneldUEModule"

void FClientInterestSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
                                                           FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Get the property handler of the type property:
	auto TypePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, AreaType));
	check(TypePropertyHandle.IsValid());

	// retrieve its value as a text to display
	TypePropertyHandle->GetValueAsDisplayText(AreaTypeText);

	OnTypeChanged(TypePropertyHandle);

	// attached an event when the property value changed
	TypePropertyHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &FClientInterestSettingsCustomization::OnTypeChanged, TypePropertyHandle));

	// then change the HeaderRow to add some Slate widget
	// clang-format off
	HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
	.ValueContent()[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
			.Text(FText::Format(LOCTEXT("ClientInterestSettingsAreaTypeString", "The area type is \"{0}\""), AreaTypeText))
		]
	];
}

void FClientInterestSettingsCustomization::OnTypeChanged(TSharedPtr<IPropertyHandle> TypePropertyHandle)
{
	if (TypePropertyHandle.IsValid() && TypePropertyHandle->IsValidHandle())
	{
		TypePropertyHandle->GetValueAsDisplayText(AreaTypeText);
		uint8 uAreaType;
		TypePropertyHandle->GetValue(uAreaType);
		AreaType = static_cast<EClientInterestAreaType>(uAreaType);
	}
}

void FClientInterestSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	auto TypePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, AreaType));
	auto SpotsPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Spots));
	auto ExtentPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Extent));
	auto RadiusPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Radius));
	auto AnglePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FClientInterestSettingsPreset, Angle));
	check(TypePropertyHandle.IsValid() && SpotsPropertyHandle.IsValid() && ExtentPropertyHandle.IsValid() && RadiusPropertyHandle.IsValid() && AnglePropertyHandle.IsValid());

	StructBuilder.AddCustomRow(LOCTEXT("ClientInterestSettingsPresetKey", "ClientInterestSettingsPreset"))
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		// .BorderBackgroundColor(FLinearColor(255.f, 152.f, 0))
		.Content()
		[
			SNew(SWrapBox)
			.UseAllottedWidth(true)
			+ SWrapBox::Slot()
			.Padding(5, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					TypePropertyHandle->CreatePropertyNameWidget()
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					TypePropertyHandle->CreatePropertyValueWidget()
				]
			]
			+ SWrapBox::Slot()
			.Padding(5, 0)
			[
				SNew(SBox)
				.IsEnabled_Lambda([=] {return AreaType == EClientInterestAreaType::Spots; })
				.MinDesiredWidth(200.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SpotsPropertyHandle->CreatePropertyNameWidget()
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SpotsPropertyHandle->CreateDefaultPropertyButtonWidgets()
					]
				]
			]
			+ SWrapBox::Slot()
			.Padding(5, 0)
			[
				SNew(SBox)
				.IsEnabled_Lambda([=] {return AreaType == EClientInterestAreaType::Box; })
				.MinDesiredWidth(200.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ExtentPropertyHandle->CreatePropertyNameWidget()
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						StructBuilder.GenerateStructValueWidget(ExtentPropertyHandle.ToSharedRef())
						// ExtentPropertyHandle->CreatePropertyValueWidget()
					]
				]
			]
			+ SWrapBox::Slot()
			.Padding(5, 0)
			[
				SNew(SBox)
				.IsEnabled_Lambda([=] {return AreaType == EClientInterestAreaType::Sphere || AreaType == EClientInterestAreaType::Cone; })
				.MinDesiredWidth(70.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						RadiusPropertyHandle->CreatePropertyNameWidget()
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						RadiusPropertyHandle->CreatePropertyValueWidget()
					]
				]
			]
			+ SWrapBox::Slot()
			.Padding(5, 0)
			[
				SNew(SBox)
				.IsEnabled_Lambda([=] {return AreaType == EClientInterestAreaType::Cone; })
				.MinDesiredWidth(70.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						AnglePropertyHandle->CreatePropertyNameWidget()
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						AnglePropertyHandle->CreatePropertyValueWidget()
					]
				]
			]
		]
	];

	/*
	StructBuilder.AddCustomRow(LOCTEXT("ClientInterestSettingsAreaTypeKey", "AreaType"))
	[
		TypePropertyHandle->CreatePropertyValueWidget()
	];

	if (AreaType == EClientInterestAreaType::Spots)
	{
		StructBuilder.AddCustomRow(LOCTEXT("ClientInterestSettingsSpotsKey", "Spots"))
		[
			SpotsPropertyHandle->CreatePropertyValueWidget()
		];
	}
	else if (AreaType == EClientInterestAreaType::Box)
	{
		StructBuilder.AddCustomRow(LOCTEXT("ClientInterestSettingsExtentKey", "Extent"))
		[
			ExtentPropertyHandle->CreatePropertyValueWidget()
		];
	}
	else if (AreaType == EClientInterestAreaType::Sphere || AreaType == EClientInterestAreaType::Cone)
	{
		StructBuilder.AddCustomRow(LOCTEXT("ClientInterestSettingsRadiusKey", "Radius"))
		[
			RadiusPropertyHandle->CreatePropertyValueWidget()
		];
		
		if (AreaType == EClientInterestAreaType::Cone)
		{
			StructBuilder.AddCustomRow(LOCTEXT("ClientInterestSettingsAngleKey", "Angle"))
			[
				AnglePropertyHandle->CreatePropertyValueWidget()
			];
		}
	}
	*/
}

#undef LOCTEXT_NAMESPACE
