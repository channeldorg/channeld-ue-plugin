// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SpinBox.h"
#include "ChanneldIntegerSpinBox.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Integer Spin Box"))
class CHANNELDEDITOR_API UChanneldIntegerSpinBox : public USpinBox
{
	GENERATED_BODY()
protected:

	//within the spin box this is an optional value since we don't need it to restore
	//non-numeric input; if you choose you can just use the native float Value
	//and convert it to int when required
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Data")
	int32 IntegerValue;

	//this is an entirely optional value that your designers can use to
	//create alternative displays of the current, validated IntegerValue
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Data")
	FText DisplayValue;

public:
	UChanneldIntegerSpinBox();

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	UFUNCTION(BlueprintCallable, Category = "User Input")
	void HandleOnFloatValueChanged(float InValue);

	UFUNCTION(BlueprintCallable, Category = "User Input")
	bool ValidateIntegerValue(float& InValue);

	UFUNCTION(BlueprintCallable, Category = "User Input")
	int32 GetIntFromFloat(const float& FloatValue);

	UFUNCTION(BlueprintCallable, Category = "User Input")
	void UpdateDisplayValue();
};
