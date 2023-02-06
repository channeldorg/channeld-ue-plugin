using UnrealBuildTool;

public class ReplicatorGenerator : ModuleRules
{
    public ReplicatorGenerator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "Json",
                "EditorSubsystem",
                "ChanneldUE"
            }
        );
        
        switch (Target.Configuration)
        {
                
            case UnrealTargetConfiguration.Unknown:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"Unknown\"");
                break;
            }
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
            case UnrealTargetConfiguration.Shipping:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"Shipping\"");
                break;
            }
            case UnrealTargetConfiguration.Test:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"Test\"");
                break;
            }
            default:
            {
                PublicDefinitions.Add("COMPILER_CONFIGURATION_NAME=\"\"");
                break;
            }
        };
    }
}