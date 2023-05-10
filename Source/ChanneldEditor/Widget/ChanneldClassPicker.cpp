// Fill out your copyright notice in the Description page of Project Settings.


#include "ChanneldClassPicker.h"

TSharedRef<SWidget> UChanneldClassPicker::RebuildWidget()
{
	MyClassPicker = SNew(SChanneldClassPicker)
	.SelectedClass(SelectedClass)
	.Font(Font)
	.MetaClass(MetaClass)
	.RequiredInterface(RequiredInterface)
	.AllowAbstract(bAllowAbstract)
	.AllowNone(bAllowNone)
	.IsBlueprintBaseOnly(bIsBlueprintBaseOnly)
	.ShowViewOptions(bShowViewOptions)
	.ShowTree(bShowTree)
	.ShowDisplayNames(bShowDisplayNames)
	.OnSetClass(FOnSetClass::CreateUObject(this, &UChanneldClassPicker::HandleSetClass))
	;
	
	return MyClassPicker.ToSharedRef();
}

void UChanneldClassPicker::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyClassPicker.Reset();
}

void UChanneldClassPicker::HandleSetClass(const UClass* InClass)
{
	SelectedClass = InClass;
	OnChangeClass.Broadcast(InClass);
}
