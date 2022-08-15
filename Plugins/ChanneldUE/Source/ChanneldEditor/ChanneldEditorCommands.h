// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ChanneldEditorStyle.h"

class FEditorStyle;

class FChanneldEditorCommands : public TCommands<FChanneldEditorCommands>
{
public:

	FChanneldEditorCommands()
		: TCommands<FChanneldEditorCommands>(TEXT("ChanneldEditor"), NSLOCTEXT("Contexts", "ChanneldEditor", "ChanneldEditor Plugin"), NAME_None, FChanneldEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> PluginCommand;
	TSharedPtr<FUICommandInfo> LaunchChanneldCommand;
	TSharedPtr<FUICommandInfo> LaunchServersCommand;
	TSharedPtr<FUICommandInfo> StopServersCommand;
};
