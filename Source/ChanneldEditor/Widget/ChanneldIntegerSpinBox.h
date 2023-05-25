#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SSpinBox.h"
#include "Components/Widget.h"
#include "ChanneldIntegerSpinBox.generated.h"

UCLASS()
class CHANNELDEDITOR_API UChanneldIntegerSpinBox : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpinBoxValueChangedEvent, int32, InValue);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpinBoxValueCommittedEvent, int32, InValue, ETextCommit::Type, CommitMethod);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSpinBoxBeginSliderMovement);

public:

	/** Value stored in this spin box */
	UPROPERTY(EditAnywhere, Category=Content)
	int32 Value;

	/** A bindable delegate to allow logic to drive the value of the widget */
	UPROPERTY()
	FGetInt32 ValueDelegate;

public:
	/** The Style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FSpinBoxStyle WidgetStyle;

	UPROPERTY()
	USlateWidgetStyleAsset* Style_DEPRECATED;

	/** The minimum required fractional digits - default 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetMinFractionalDigits, BlueprintGetter = GetMinFractionalDigits, Category = "Slider", meta = (ClampMin = 0, UIMin = 0))
	int32 MinFractionalDigits;

	/** The maximume required fractional digits - default 6 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetMaxFractionalDigits, BlueprintGetter = GetMaxFractionalDigits, Category = "Slider", meta = (ClampMin = 0, UIMin = 0))
	int32 MaxFractionalDigits;

	/** Whether this spin box should use the delta snapping logic for typed values - default false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetAlwaysUsesDeltaSnap, BlueprintGetter = GetAlwaysUsesDeltaSnap, Category = "Slider")
	bool bAlwaysUsesDeltaSnap;

	/** The amount by which to change the spin box value as the slider moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetDelta, BlueprintGetter = GetDelta, Category = "Slider")
	int32 Delta;

	/** The exponent by which to increase the delta as the mouse moves. 1 is constant (never increases the delta). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slider")
	int32 SliderExponent;
	
	/** Font color and opacity (overrides style) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display")
	FSlateFontInfo Font;

	/** The justification the value text should appear as. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TEnumAsByte<ETextJustify::Type> Justification;

	/** The minimum width of the spin box */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display", AdvancedDisplay, DisplayName = "Minimum Desired Width")
	int32 MinDesiredWidth;

	/** Whether to remove the keyboard focus from the spin box when the value is committed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", AdvancedDisplay)
	bool ClearKeyboardFocusOnCommit;

	/** Whether to select the text in the spin box when the value is committed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", AdvancedDisplay)
	bool SelectAllTextOnCommit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Style")
	FSlateColor ForegroundColor;

public:
	/** Called when the value is changed interactively by the user */
	UPROPERTY(BlueprintAssignable, Category="SpinBox|Events")
	FOnSpinBoxValueChangedEvent OnValueChanged;

	/** Called when the value is committed. Occurs when the user presses Enter or the text box loses focus. */
	UPROPERTY(BlueprintAssignable, Category="SpinBox|Events")
	FOnSpinBoxValueCommittedEvent OnValueCommitted;

	/** Called right before the slider begins to move */
	UPROPERTY(BlueprintAssignable, Category="SpinBox|Events")
	FOnSpinBoxBeginSliderMovement OnBeginSliderMovement;

	/** Called right after the slider handle is released by the user */
	UPROPERTY(BlueprintAssignable, Category="SpinBox|Events")
	FOnSpinBoxValueChangedEvent OnEndSliderMovement;

public:

	/** Get the current value of the spin box. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	int32 GetValue() const;

	/** Set the value of the spin box. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	void SetValue(int32 NewValue);

public:

	/** Get the current Min Fractional Digits for the spin box. */
	UFUNCTION(BlueprintCallable, BlueprintGetter, Category = "Behavior")
	int32 GetMinFractionalDigits() const;

	/** Set the Min Fractional Digits for the spin box. */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "Behavior")
	void SetMinFractionalDigits(int32 NewValue);

	/** Get the current Max Fractional Digits for the spin box. */
	UFUNCTION(BlueprintCallable, BlueprintGetter, Category = "Behavior")
	int32 GetMaxFractionalDigits() const;

	/** Set the Max Fractional Digits for the spin box. */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "Behavior")
	void SetMaxFractionalDigits(int32 NewValue);

	/** Get whether the spin box uses delta snap on type. */
	UFUNCTION(BlueprintCallable, BlueprintGetter, Category = "Behavior")
	bool GetAlwaysUsesDeltaSnap() const;

	/** Set whether the spin box uses delta snap on type. */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "Behavior")
	void SetAlwaysUsesDeltaSnap(bool bNewValue);

	/** Get the current delta for the spin box. */
	UFUNCTION(BlueprintCallable, BlueprintGetter, Category = "Behavior")
	int32 GetDelta() const;

	/** Set the delta for the spin box. */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "Behavior")
	void SetDelta(int32 NewValue);

	/** Get the current minimum value that can be manually set in the spin box. */
	UFUNCTION(BlueprintCallable, Category="Behavior")
	int32 GetMinValue() const;

	/** Set the minimum value that can be manually set in the spin box. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetMinValue(int32 NewValue);

	/** Clear the minimum value that can be manually set in the spin box. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ClearMinValue();

	/** Get the current maximum value that can be manually set in the spin box. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	int32 GetMaxValue() const;

	/** Set the maximum value that can be manually set in the spin box. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetMaxValue(int32 NewValue);

	/** Clear the maximum value that can be manually set in the spin box. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ClearMaxValue();

	/** Get the current minimum value that can be specified using the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	int32 GetMinSliderValue() const;

	/** Set the minimum value that can be specified using the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetMinSliderValue(int32 NewValue);

	/** Clear the minimum value that can be specified using the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ClearMinSliderValue();

	/** Get the current maximum value that can be specified using the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	int32 GetMaxSliderValue() const;

	/** Set the maximum value that can be specified using the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void SetMaxSliderValue(int32 NewValue);

	/** Clear the maximum value that can be specified using the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ClearMaxSliderValue();

	/**  */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetForegroundColor(FSlateColor InForegroundColor);

public:

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	//~ End UWidget Interface

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget

	void HandleOnValueChanged(int32 InValue);
	void HandleOnValueCommitted(int32 InValue, ETextCommit::Type CommitMethod);
	void HandleOnBeginSliderMovement();
	void HandleOnEndSliderMovement(int32 InValue);

protected:
	/** Whether the optional MinValue attribute of the widget is set */
	UPROPERTY(EditAnywhere, Category = Content, meta=(InlineEditConditionToggle))
	uint32 bOverride_MinValue : 1;

	/** Whether the optional MaxValue attribute of the widget is set */
	UPROPERTY(EditAnywhere, Category = Content, meta=(InlineEditConditionToggle))
	uint32 bOverride_MaxValue : 1;

	/** Whether the optional MinSliderValue attribute of the widget is set */
	UPROPERTY(EditAnywhere, Category = Content, meta=(InlineEditConditionToggle))
	uint32 bOverride_MinSliderValue : 1;

	/** Whether the optional MaxSliderValue attribute of the widget is set */
	UPROPERTY(EditAnywhere, Category = Content, meta=(InlineEditConditionToggle))
	uint32 bOverride_MaxSliderValue : 1;

	/** The minimum allowable value that can be manually entered into the spin box */
	UPROPERTY(EditAnywhere, Category = Content, DisplayName = "Minimum Value", meta = (editcondition = "bOverride_MinValue"))
	int32 MinValue;

	/** The maximum allowable value that can be manually entered into the spin box */
	UPROPERTY(EditAnywhere, Category = Content, DisplayName = "Maximum Value", meta = (editcondition = "bOverride_MaxValue"))
	int32 MaxValue;

	/** The minimum allowable value that can be specified using the slider */
	UPROPERTY(EditAnywhere, Category = Content, DisplayName = "Minimum Slider Value", meta = (editcondition = "bOverride_MinSliderValue"))
	int32 MinSliderValue;

	/** The maximum allowable value that can be specified using the slider */
	UPROPERTY(EditAnywhere, Category = Content, DisplayName = "Maximum Slider Value", meta = (editcondition = "bOverride_MaxSliderValue"))
	int32 MaxSliderValue;

protected:
	TSharedPtr<SSpinBox<int32>> MySpinBox;

	PROPERTY_BINDING_IMPLEMENTATION(int32, Value);
};
