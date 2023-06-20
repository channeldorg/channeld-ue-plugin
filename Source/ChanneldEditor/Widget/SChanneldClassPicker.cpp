// Copyright Epic Games, Inc. All Rights Reserved.

#include "SChanneldClassPicker.h"
#include "Engine/Blueprint.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"

#include "DragAndDrop/ClassDragDropOp.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

class FChanneldEditorClassFilter : public IClassViewerFilter
{
public:
	/** The meta class for the property that classes must be a child-of. */
	const UClass* ClassPropertyMetaClass;

	/** The interface that must be implemented. */
	const UClass* InterfaceThatMustBeImplemented;

	/** Whether or not abstract classes are allowed. */
	bool bAllowAbstract;

	/** Classes that can be picked */
	TArray<const UClass*> AllowedClassFilters;

	/** Classes that can't be picked */
	TArray<const UClass*> DisallowedClassFilters;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass,
	                            TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return IsClassAllowedHelper(InClass);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
	                                    const TSharedRef<const IUnloadedBlueprintData> InBlueprint,
	                                    TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return IsClassAllowedHelper(InBlueprint);
	}

private:
	template <typename TClass>
	bool IsClassAllowedHelper(TClass InClass)
	{
		bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated) &&
			(bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract));

		if (bMatchesFlags && InClass->IsChildOf(ClassPropertyMetaClass)
			&& (!InterfaceThatMustBeImplemented || InClass->ImplementsInterface(InterfaceThatMustBeImplemented)))
		{
			auto PredicateFn = [InClass](const UClass* Class)
			{
				return InClass->IsChildOf(Class);
			};

			if (DisallowedClassFilters.FindByPredicate(PredicateFn) == nullptr &&
				(AllowedClassFilters.Num() == 0 || AllowedClassFilters.FindByPredicate(PredicateFn) != nullptr))
			{
				return true;
			}
		}

		return false;
	}
};

void SChanneldClassPicker::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	OutMinDesiredWidth = 125.0f;
	OutMaxDesiredWidth = 400.0f;
}

void SChanneldClassPicker::Construct(const FArguments& InArgs)
{
	// check(InArgs._MetaClass);
	// check(InArgs._SelectedClass.IsSet());
	check(InArgs._OnSetClass.IsBound());

	Font = InArgs._Font;
	
	MetaClass = InArgs._MetaClass ? InArgs._MetaClass : UObject::StaticClass();
	RequiredInterface = InArgs._RequiredInterface;
	bAllowAbstract = InArgs._AllowAbstract;
	bIsBlueprintBaseOnly = InArgs._IsBlueprintBaseOnly;
	bAllowNone = InArgs._AllowNone;
	bAllowOnlyPlaceable = false;
	bShowViewOptions = InArgs._ShowViewOptions;
	bShowTree = InArgs._ShowTree;
	bShowDisplayNames = InArgs._ShowDisplayNames;
	AllowedClassFilters.Empty();
	DisallowedClassFilters.Empty();
	SelectedClass = InArgs._SelectedClass;
	OnSetClass = InArgs._OnSetClass;

	CreateClassFilter();

	SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SChanneldClassPicker::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SChanneldClassPicker::GetDisplayValueAsString)
		.ButtonContent()
	[
		SNew(STextBlock)
			.Text(this, &SChanneldClassPicker::GetDisplayValueAsString)
			.Font(Font)
	];

	ChildSlot
	[
		ComboButton.ToSharedRef()
	];
}

/** Util to give better names for BP generated classes */
static FString GetClassDisplayName(const UObject* Object, bool bShowDisplayNames)
{
	const UClass* Class = Cast<UClass>(Object);
	if (Class != NULL)
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		if (BP != NULL)
		{
			return BP->GetName();
		}
		if (bShowDisplayNames && Class->HasMetaData(TEXT("DisplayName")))
		{
			return Class->GetMetaData(TEXT("DisplayName"));
		}
	}
	return (Object) ? Object->GetName() : "None";
}

FText SChanneldClassPicker::GetDisplayValueAsString() const
{
	if (SelectedClass.Get()	== nullptr)
	{
		return LOCTEXT("None", "None");
	}
	return FText::FromString(GetClassDisplayName(SelectedClass.Get(), bShowDisplayNames));
}

void SChanneldClassPicker::CreateClassFilter()
{
	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = bAllowNone;
	ClassViewerOptions.bIsBlueprintBaseOnly = bIsBlueprintBaseOnly;
	ClassViewerOptions.bIsPlaceableOnly = bAllowOnlyPlaceable;
	ClassViewerOptions.NameTypeToDisplay = (bShowDisplayNames
		                                        ? EClassViewerNameTypeToDisplay::DisplayName
		                                        : EClassViewerNameTypeToDisplay::ClassName);
	ClassViewerOptions.DisplayMode = bShowTree ? EClassViewerDisplayMode::TreeView : EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = bShowViewOptions;

	TSharedPtr<FChanneldEditorClassFilter> ChanneldEdClassFilter = MakeShareable(new FChanneldEditorClassFilter);
	ClassViewerOptions.ClassFilter = ChanneldEdClassFilter;

	ChanneldEdClassFilter->ClassPropertyMetaClass = MetaClass;
	ChanneldEdClassFilter->InterfaceThatMustBeImplemented = RequiredInterface;
	ChanneldEdClassFilter->bAllowAbstract = bAllowAbstract;
	ChanneldEdClassFilter->AllowedClassFilters = AllowedClassFilters;
	ChanneldEdClassFilter->DisallowedClassFilters = DisallowedClassFilters;

	ClassFilter = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassFilter(
		ClassViewerOptions);
	ClassFilterFuncs = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateFilterFuncs();
}

TSharedRef<SWidget> SChanneldClassPicker::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SChanneldClassPicker::OnClassPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			  .AutoHeight()
			  .MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(
					ClassViewerOptions, OnPicked)
			]
		];
}

void SChanneldClassPicker::OnClassPicked(UClass* InClass)
{
	SelectedClass = InClass;

	if (!InClass)
	{
		SendToObjects(TEXT("None"));
	}
	else
	{
		SendToObjects(InClass->GetPathName());
	}

	ComboButton->SetIsOpen(false);
}

void SChanneldClassPicker::SendToObjects(const FString& NewValue)
{
	if (!NewValue.IsEmpty() && NewValue != TEXT("None"))
	{
		UClass* NewClass = FindObject<UClass>(ANY_PACKAGE, *NewValue);
		if (!NewClass)
		{
			NewClass = LoadObject<UClass>(nullptr, *NewValue);
		}
		OnSetClass.Execute(NewClass);
	}
	else
	{
		OnSetClass.Execute(nullptr);
	}
}

static UObject* LoadDragDropObject(TSharedPtr<FAssetDragDropOp> UnloadedClassOp)
{
	FString AssetPath;

	// Find the class/blueprint path
	if (UnloadedClassOp->HasAssets())
	{
		AssetPath = UnloadedClassOp->GetAssets()[0].ObjectPath.ToString();
	}
	else if (UnloadedClassOp->HasAssetPaths())
	{
		AssetPath = UnloadedClassOp->GetAssetPaths()[0];
	}

	// Check to see if the asset can be found, otherwise load it.
	UObject* Object = FindObject<UObject>(nullptr, *AssetPath);
	if (Object == nullptr)
	{
		// Load the package.
		GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);

		Object = LoadObject<UObject>(nullptr, *AssetPath);

		GWarn->EndSlowTask();
	}

	return Object;
}

void SChanneldClassPicker::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		UObject* Object = LoadDragDropObject(UnloadedClassOp);

		bool bOK = false;

		if (UClass* Class = Cast<UClass>(Object))
		{
			bOK = ClassFilter->IsClassAllowed(ClassViewerOptions, Class, ClassFilterFuncs.ToSharedRef());
		}
		else if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			if (Blueprint->GeneratedClass)
			{
				bOK = ClassFilter->IsClassAllowed(ClassViewerOptions, Blueprint->GeneratedClass,
				                                  ClassFilterFuncs.ToSharedRef());
			}
		}

		if (bOK)
		{
			UnloadedClassOp->SetToolTip(FText::GetEmpty(), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
		}
		else
		{
			UnloadedClassOp->SetToolTip(FText::GetEmpty(),
			                            FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
		}
	}
}

void SChanneldClassPicker::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		UnloadedClassOp->ResetToDefaultToolTip();
	}
}

FReply SChanneldClassPicker::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FClassDragDropOp> ClassOperation = DragDropEvent.GetOperationAs<FClassDragDropOp>();
	if (ClassOperation.IsValid())
	{
		// We can only drop one item into the combo box, so drop the first one.
		FString ClassPath = ClassOperation->ClassesToDrop[0]->GetPathName();

		// Set the property, it will be verified as valid.
		SendToObjects(ClassPath);

		return FReply::Handled();
	}

	TSharedPtr<FAssetDragDropOp> UnloadedClassOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (UnloadedClassOp.IsValid())
	{
		bool bAllAssetWereLoaded = true;

		FString AssetPath;

		// Find the class/blueprint path
		if (UnloadedClassOp->HasAssets())
		{
			AssetPath = UnloadedClassOp->GetAssets()[0].ObjectPath.ToString();
		}
		else if (UnloadedClassOp->HasAssetPaths())
		{
			AssetPath = UnloadedClassOp->GetAssetPaths()[0];
		}

		// Check to see if the asset can be found, otherwise load it.
		UObject* Object = FindObject<UObject>(nullptr, *AssetPath);
		if (Object == nullptr)
		{
			// Load the package.
			GWarn->BeginSlowTask(LOCTEXT("OnDrop_LoadPackage", "Fully Loading Package For Drop"), true, false);

			Object = LoadObject<UObject>(nullptr, *AssetPath);

			GWarn->EndSlowTask();
		}

		if (UClass* Class = Cast<UClass>(Object))
		{
			if (ClassFilter->IsClassAllowed(ClassViewerOptions, Class, ClassFilterFuncs.ToSharedRef()))
			{
				// This was pointing to a class directly
				SendToObjects(Class->GetPathName());
			}
		}
		else if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
		{
			if (Blueprint->GeneratedClass)
			{
				if (ClassFilter->IsClassAllowed(ClassViewerOptions, Blueprint->GeneratedClass,
				                                ClassFilterFuncs.ToSharedRef()))
				{
					// This was pointing to a blueprint, get generated class
					SendToObjects(Blueprint->GeneratedClass->GetPathName());
				}
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
