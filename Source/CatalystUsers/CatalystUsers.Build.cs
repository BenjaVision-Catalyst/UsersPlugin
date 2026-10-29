using UnrealBuildTool;
public class CatalystUsers : ModuleRules
{
    public CatalystUsers(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CatalystConnectObjects" });
        PrivateDependencyModuleNames.AddRange(new[] { "CatalystFoundation", "CatalystConnect" });
    }
}
