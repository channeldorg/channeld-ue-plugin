
using UnrealBuildTool;
using System.IO;

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
            //"OnlineSubsystem",
            //"OnlineSubsystemUtils",
            "Sockets",
            "PacketHandler",
            "ChanneldProtobuf",
            "ReplicationGraph",
            "PrometheusUE",
            }
        );

        PublicSystemIncludePaths.AddRange(
            new string[]
            {
                ModuleDirectory,
                //Path.Combine(ModuleDirectory, "channeldpb"),
            }
            );

        PublicIncludePaths.Add(ModuleDirectory);

        bUseRTTI = false;
    }
}
