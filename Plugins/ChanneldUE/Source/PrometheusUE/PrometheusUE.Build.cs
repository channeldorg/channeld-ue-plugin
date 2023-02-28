
using UnrealBuildTool;
using System.IO;
public class PrometheusUE : ModuleRules
{

    public PrometheusUE(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PublicSystemIncludePaths.AddRange(new string[] 
        {
            Path.Combine(ModuleDirectory, "ThirdParty"),
            Path.Combine(ModuleDirectory, "ThirdParty/civetweb"),
        });
        
        PublicDefinitions.AddRange(
            new string[] {
                "LANG_CXX11",
                "OPENSSL_API_1_1",
            }
        );

        ShadowVariableWarningLevel = WarningLevel.Off;
        bEnableUndefinedIdentifierWarnings = false;
        bEnableExceptions = true;
    }
}
