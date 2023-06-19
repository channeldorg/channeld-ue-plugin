// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "SChanneldClassPicker.h"
#include "ChanneldClassPicker.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChangeClassDelegate, const UClass*, Class);

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Class Picker"))
class CHANNELDEDITOR_API UChanneldClassPicker : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	/** The font used to draw the class name */
	UPROPERTY(EditAnywhere, Category = "Display")
	FSlateFontInfo Font;

	/** The meta class that the selected class must be a child-of */
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	const UClass* MetaClass = UObject::StaticClass();

	/** An interface that the selected class must implement */
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	const UClass* RequiredInterface = nullptr;

	/** Whether or not abstract classes are allowed */
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	bool bAllowAbstract = false;

	/** Should only base blueprints be displayed? */
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	bool bIsBlueprintBaseOnly = false;

	/** Should we be able to select "None" as a class? */
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	bool bAllowNone = true;

	/** Should we show the view options button at the bottom of the class picker?*/
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	bool bShowViewOptions = true;

	/** Should we show the class picker in tree mode or list mode?*/
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	bool bShowTree = false;

	/** Should we prettify class names on the class picker? (ie show their display name) */
	UPROPERTY(EditAnywhere, Category = "Class Filter")
	bool bShowDisplayNames = true;

	UPROPERTY(BlueprintAssignable)
	FOnChangeClassDelegate OnChangeClass;

	UPROPERTY(BlueprintReadOnly, Category = "Value")
	const UClass* SelectedClass = nullptr;

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	UFUNCTION(BlueprintCallable)
	void SetSelectedClass(UClass* InClass);

protected:
	TSharedPtr<SChanneldClassPicker> MyClassPicker;

	void HandleSetClass(const UClass* InClass);
};
