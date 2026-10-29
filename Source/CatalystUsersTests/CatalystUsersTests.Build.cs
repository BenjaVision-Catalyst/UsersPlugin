using UnrealBuildTool;
public class CatalystUsersTests : ModuleRules
{
    public CatalystUsersTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "CatalystUsers", "CatalystConnect", "CatalystConnectObjects" });
    }
}
