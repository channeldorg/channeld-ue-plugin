
using UnrealBuildTool;
using System.IO;
public class ChanneldEditor : ModuleRules
{
    public ChanneldEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ChanneldUE",
				// ... add other public dependencies that you statically link with here ...
			}
        );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "UnrealEd",
                "PropertyEditor",
                "LevelEditor",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "EditorStyle", 
                "ReplicatorGenerator",
                "UATHelper",
                "Kismet",
                "ChanneldProtobufEditor",
                // ... add private dependencies that you statically link with here ...	
			}
        );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );
        
        switch (Target.Configuration)
        {
            case UnrealTargetConfiguration.Debug:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"Debug\"");
                break;
            }
            case UnrealTargetConfiguration.DebugGame:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"DebugGame\"");
                break;
            }
            case UnrealTargetConfiguration.Development:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"Development\"");
                break;
            }
            default:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"\"");
                break;
            }
        };
        
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
    }
}
