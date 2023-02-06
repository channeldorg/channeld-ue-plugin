using UnrealBuildTool;
using System.IO;

public class ChanneldProtobufEditor : ModuleRules
{
	public ChanneldProtobufEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			}
		);
		PublicDefinitions.AddRange(
			new string[]
			{
				"PROTOC_PATH=R\"(" + Path.Combine(PluginDirectory, "Source", "ChanneldProtobuf", "ThirdParty", "bin", "protoc.exe") + ")\"",
				"PROTOBUF_INCLUDE_PATH=R\"(" + Path.Combine(PluginDirectory, "Source", "ChanneldProtobuf", "ThirdParty") + ")\"",
			}
		);
	}
}