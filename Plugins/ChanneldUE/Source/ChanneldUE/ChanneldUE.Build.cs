
using UnrealBuildTool;

public class ChanneldUE : ModuleRules
{

    public ChanneldUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore",
            "OnlineSubsystem",
            //"OnlineSubsystemNull",
            //"OnlineSubsystemSteam",
            "OnlineSubsystemUtils",
            "Sockets",
            "PacketHandler",
            "Protobuf",
            //"SteamShared",
            //"SteamSockets",
			}
        );

        bUseRTTI = true;
    }
}
