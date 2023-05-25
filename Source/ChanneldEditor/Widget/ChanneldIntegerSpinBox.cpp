// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChanneldIntegerSpinBox.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UChanneldIntegerSpinBox

static FSpinBoxStyle* DefaultSpinBoxStyle = nullptr;

UChanneldIntegerSpinBox::UChanneldIntegerSpinBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));
	}

	Value = 0;
	MinValue = 0;
	MaxValue = 0;
	MinSliderValue = 0;
	MaxSliderValue = 0;	
	MinFractionalDigits = 1;
	MaxFractionalDigits = 6;
	bAlwaysUsesDeltaSnap = false;
	Delta = 0;
	SliderExponent = 1;
	MinDesiredWidth = 0;
	ClearKeyboardFocusOnCommit = false;
	SelectAllTextOnCommit = true;
	ForegroundColor = FSlateColor(FLinearColor::Black);

	if (DefaultSpinBoxStyle == nullptr)
	{
		// HACK: THIS SHOULD NOT COME FROM CORESTYLE AND SHOULD INSTEAD BE DEFINED BY ENGINE TEXTURES/PROJECT SETTINGS
		DefaultSpinBoxStyle = new FSpinBoxStyle(FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"));

		// Unlink UMG default colors from the editor settings colors.
		DefaultSpinBoxStyle->UnlinkColors();
	}

	WidgetStyle = *DefaultSpinBoxStyle;
}

void UChanneldIntegerSpinBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySpinBox.Reset();
}

TSharedRef<SWidget> UChanneldIntegerSpinBox::RebuildWidget()
{
	MySpinBox = SNew(SSpinBox<int32>)
	.Style(&WidgetStyle)
	.Font(Font)
	.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
	.SelectAllTextOnCommit(SelectAllTextOnCommit)
	.Justification(Justification)
	.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnInt32ValueChanged, HandleOnValueChanged))
	.OnValueCommitted(BIND_UOBJECT_DELEGATE(FOnInt32ValueCommitted, HandleOnValueCommitted))
	.OnBeginSliderMovement(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnBeginSliderMovement))
	.OnEndSliderMovement(BIND_UOBJECT_DELEGATE(FOnInt32ValueChanged, HandleOnEndSliderMovement))
	;
	
	return MySpinBox.ToSharedRef();
}

void UChanneldIntegerSpinBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MySpinBox->SetDelta(Delta);
	MySpinBox->SetSliderExponent(SliderExponent);
	MySpinBox->SetMinDesiredWidth(MinDesiredWidth);

	MySpinBox->SetForegroundColor(ForegroundColor);

	MySpinBox->SetMinFractionalDigits(MinFractionalDigits);
	MySpinBox->SetMaxFractionalDigits(MaxFractionalDigits);
	MySpinBox->SetAlwaysUsesDeltaSnap(bAlwaysUsesDeltaSnap);

	// Set optional values
	bOverride_MinValue ? SetMinValue(MinValue) : ClearMinValue();
	bOverride_MaxValue ? SetMaxValue(MaxValue) : ClearMaxValue();
	bOverride_MinSliderValue ? SetMinSliderValue(MinSliderValue) : ClearMinSliderValue();
	bOverride_MaxSliderValue ? SetMaxSliderValue(MaxSliderValue) : ClearMaxSliderValue();

	// Always set the value last so that the max/min values are taken into account.
	// TAttribute<int32> ValueBinding = PROPERTY_BINDING(int32, Value);
	// MySpinBox->SetValue(ValueBinding);
}

int32 UChanneldIntegerSpinBox::GetValue() const
{
	if (MySpinBox.IsValid())
	{
		return MySpinBox->GetValue();
	}

	return Value;
}

void UChanneldIntegerSpinBox::SetValue(int32 InValue)
{
	Value = InValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetValue(InValue);
	}
}

int32 UChanneldIntegerSpinBox::GetMinFractionalDigits() const
{
	if (MySpinBox.IsValid())
	{
		return MySpinBox->GetMinFractionalDigits();
	}

	return MinFractionalDigits;
}

void UChanneldIntegerSpinBox::SetMinFractionalDigits(int32 NewValue)
{
	MinFractionalDigits = FMath::Max(0, NewValue);
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinFractionalDigits(MinFractionalDigits);
	}
}

int32 UChanneldIntegerSpinBox::GetMaxFractionalDigits() const
{
	if (MySpinBox.IsValid())
	{
		return MySpinBox->GetMaxFractionalDigits();
	}

	return MaxFractionalDigits;
}

void UChanneldIntegerSpinBox::SetMaxFractionalDigits(int32 NewValue)
{
	MaxFractionalDigits = FMath::Max(0, NewValue);
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxFractionalDigits(MaxFractionalDigits);
	}
}

bool UChanneldIntegerSpinBox::GetAlwaysUsesDeltaSnap() const
{
	if (MySpinBox.IsValid())
	{
		return MySpinBox->GetAlwaysUsesDeltaSnap();
	}

	return bAlwaysUsesDeltaSnap;
}

void UChanneldIntegerSpinBox::SetAlwaysUsesDeltaSnap(bool bNewValue)
{
	bAlwaysUsesDeltaSnap = bNewValue;

	if (MySpinBox.IsValid())
	{
		MySpinBox->SetAlwaysUsesDeltaSnap(bNewValue);
	}
}

int32 UChanneldIntegerSpinBox::GetDelta() const
{
	if (MySpinBox.IsValid())
	{
		return MySpinBox->GetDelta();
	}

	return Delta;
}

void UChanneldIntegerSpinBox::SetDelta(int32 NewValue)
{
	Delta = NewValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetDelta(NewValue);
	}
}

// MIN VALUE
int32 UChanneldIntegerSpinBox::GetMinValue() const
{
	int32 ReturnVal = TNumericLimits<int32>::Lowest();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMinValue();
	}
	else if (bOverride_MinValue)
	{
		ReturnVal = MinValue;
	}

	return ReturnVal;
}

void UChanneldIntegerSpinBox::SetMinValue(int32 InMinValue)
{
	bOverride_MinValue = true;
	MinValue = InMinValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinValue(InMinValue);
	}
}

void UChanneldIntegerSpinBox::ClearMinValue()
{
	bOverride_MinValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinValue(TOptional<int32>());
	}
}

// MAX VALUE
int32 UChanneldIntegerSpinBox::GetMaxValue() const
{
	int32 ReturnVal = TNumericLimits<int32>::Max();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMaxValue();
	}
	else if (bOverride_MaxValue)
	{
		ReturnVal = MaxValue;
	}

	return ReturnVal;
}

void UChanneldIntegerSpinBox::SetMaxValue(int32 InMaxValue)
{
	bOverride_MaxValue = true;
	MaxValue = InMaxValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxValue(InMaxValue);
	}
}
void UChanneldIntegerSpinBox::ClearMaxValue()
{
	bOverride_MaxValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxValue(TOptional<int32>());
	}
}

// MIN SLIDER VALUE
int32 UChanneldIntegerSpinBox::GetMinSliderValue() const
{
	int32 ReturnVal = TNumericLimits<int32>::Min();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMinSliderValue();
	}
	else if (bOverride_MinSliderValue)
	{
		ReturnVal = MinSliderValue;
	}

	return ReturnVal;
}

void UChanneldIntegerSpinBox::SetMinSliderValue(int32 InMinSliderValue)
{
	bOverride_MinSliderValue = true;
	MinSliderValue = InMinSliderValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinSliderValue(InMinSliderValue);
	}
}

void UChanneldIntegerSpinBox::ClearMinSliderValue()
{
	bOverride_MinSliderValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMinSliderValue(TOptional<int32>());
	}
}

// MAX SLIDER VALUE
int32 UChanneldIntegerSpinBox::GetMaxSliderValue() const
{
	int32 ReturnVal = TNumericLimits<int32>::Max();

	if (MySpinBox.IsValid())
	{
		ReturnVal = MySpinBox->GetMaxSliderValue();
	}
	else if (bOverride_MaxSliderValue)
	{
		ReturnVal = MaxSliderValue;
	}

	return ReturnVal;
}

void UChanneldIntegerSpinBox::SetMaxSliderValue(int32 InMaxSliderValue)
{
	bOverride_MaxSliderValue = true;
	MaxSliderValue = InMaxSliderValue;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxSliderValue(InMaxSliderValue);
	}
}

void UChanneldIntegerSpinBox::ClearMaxSliderValue()
{
	bOverride_MaxSliderValue = false;
	if (MySpinBox.IsValid())
	{
		MySpinBox->SetMaxSliderValue(TOptional<int32>());
	}
}

void UChanneldIntegerSpinBox::SetForegroundColor(FSlateColor InForegroundColor)
{
	ForegroundColor = InForegroundColor;
	if ( MySpinBox.IsValid() )
	{
		MySpinBox->SetForegroundColor(ForegroundColor);
	}
}

// Event handlers
void UChanneldIntegerSpinBox::HandleOnValueChanged(int32 InValue)
{
	if ( !IsDesignTime() )
	{
		OnValueChanged.Broadcast(InValue);
	}
}

void UChanneldIntegerSpinBox::HandleOnValueCommitted(int32 InValue, ETextCommit::Type CommitMethod)
{
	if ( !IsDesignTime() )
	{
		OnValueCommitted.Broadcast(InValue, CommitMethod);
	}
}

void UChanneldIntegerSpinBox::HandleOnBeginSliderMovement()
{
	if ( !IsDesignTime() )
	{
		OnBeginSliderMovement.Broadcast();
	}
}

void UChanneldIntegerSpinBox::HandleOnEndSliderMovement(int32 InValue)
{
	if ( !IsDesignTime() )
	{
		OnEndSliderMovement.Broadcast(InValue);
	}
}

void UChanneldIntegerSpinBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FSpinBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FSpinBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}
	}
}


#if WITH_EDITOR

const FText UChanneldIntegerSpinBox::GetPaletteCategory()
{
	return LOCTEXT("Input", "Input");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
