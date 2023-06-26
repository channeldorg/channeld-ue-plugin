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
				"ChanneldUE",
				"AnalyticsET",
				"JsonUtilities",
			}
		);
        if (Target.Version.MajorVersion == 5)
        {
            PrivateDependencyModuleNames.Add("RenderCore"); 
            PrivateDependencyModuleNames.Add("DeveloperToolSettings"); 
            PrivateDependencyModuleNames.Add("AssetRegistry"); 
        }
		string CompilerConfigurationName;
		switch (Target.Configuration)
		{
			case UnrealTargetConfiguration.Debug:
			{
				CompilerConfigurationName = "COMPILER_CONFIGURATION_NAME=\"Debug\"";
				break;
			}
			case UnrealTargetConfiguration.DebugGame:
			{
				CompilerConfigurationName = "COMPILER_CONFIGURATION_NAME=\"DebugGame\"";
				break;
			}
			case UnrealTargetConfiguration.Development:
			{
				CompilerConfigurationName = "COMPILER_CONFIGURATION_NAME=\"Development\"";
				break;
			}
			case UnrealTargetConfiguration.Shipping:
			{
				CompilerConfigurationName = "COMPILER_CONFIGURATION_NAME=\"Shipping\"";
				break;
			}
			case UnrealTargetConfiguration.Test:
			{
				CompilerConfigurationName = "COMPILER_CONFIGURATION_NAME=\"Test\"";
				break;
			}
			default:
			{
				CompilerConfigurationName = "COMPILER_CONFIGURATION_NAME=\"Unknown\"";
				break;
			}
		}

		PrivateDefinitions.AddRange(
			new string[]
			{
				CompilerConfigurationName,
				"PLUGIN_DIR=R\"(" + PluginDirectory + ")\"",
			}
		);
	}
}