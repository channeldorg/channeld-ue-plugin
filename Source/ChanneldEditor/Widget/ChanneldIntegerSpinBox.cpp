// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldIntegerSpinBox.h"

UChanneldIntegerSpinBox::UChanneldIntegerSpinBox():
    USpinBox(),
    IntegerValue(GetIntFromFloat(Value)),
    DisplayValue(FText::AsNumber(IntegerValue))
    {}

TSharedRef<SWidget> UChanneldIntegerSpinBox::RebuildWidget()
{
     MySpinBox = SNew(SSpinBox<float>)
         .Style(&WidgetStyle)
         .Font(Font)
         .ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
         .SelectAllTextOnCommit(SelectAllTextOnCommit)
         .OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnFloatValueChanged))
         .OnValueCommitted(BIND_UOBJECT_DELEGATE(FOnFloatValueCommitted, HandleOnValueCommitted))
         .OnBeginSliderMovement(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnBeginSliderMovement))
         .OnEndSliderMovement(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnEndSliderMovement))
         ;
     return BuildDesignTimeWidget(MySpinBox.ToSharedRef());
}

void UChanneldIntegerSpinBox::HandleOnFloatValueChanged(float InValue) {
     if (ValidateIntegerValue(InValue)) {
         HandleOnValueChanged(InValue);
     }
}

bool UChanneldIntegerSpinBox::ValidateIntegerValue(float& InValue) {
     //checking to make sure the value is different prevents an infinite loop via SetValue
     if (Value != InValue) {
         //round up or down, depending on where we're going
         InValue = Value < InValue ? FMath::CeilToFloat(InValue) : FMath::FloorToFloat(InValue);

         //then we have to set the value back to the spinner
         SetValue(InValue);

         //we can store the integer value if desired
         IntegerValue = GetIntFromFloat(InValue);
         //you can use CeilToFlot or Floor if you prefer
         //IntegerValue = FMath::CeilToFloat(InValue);
         //IntegerValue = FMath::Floor(InValue);

         //finally, update the optional FText display value
         UpdateDisplayValue();
         return true;

     }
     return false;

}

int32 UChanneldIntegerSpinBox::GetIntFromFloat(const float& FloatValue) {
     if (FloatValue >= 0.0f) {
         return (int)(FloatValue + 0.5f);

     }
     return (int)(FloatValue - 0.5f);

}

void UChanneldIntegerSpinBox::UpdateDisplayValue() {
     DisplayValue = FText::AsNumber(IntegerValue);

}