#include "ChanneldEditorCommands.h"

#define LOCTEXT_NAMESPACE "FChanneldEditorModule"

void FChanneldEditorCommands::RegisterCommands()
{
	UI_COMMAND(PluginCommand, "Channeld", "Tools and utilities provided by ChanneldUE", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleNetworkingCommand, "Enable Channeld Networking", "Turn off to use UE's native networking", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(LaunchChanneldCommand, "Launch Channeld", "Launch channeld in a separate process", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopChanneldCommand, "Stop Channeld", "Stop launched channeld service", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LaunchServersCommand, "Launch Servers", "Launch PIE servers using current map in separate processes", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ServerSettingsCommand, "Server Settings...", "Configure server numbers, map, view, and additioanl args", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopServersCommand, "Stop Servers", "Stop launched PIE servers", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GenerateReplicatorCommand, "Generate Replication Code", "Generate replication code for all replicated actors", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddRepComponentsToBPsCommand, "Add Replication Components To Blueprint Actors", "Add replication components to blueprint actors", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
