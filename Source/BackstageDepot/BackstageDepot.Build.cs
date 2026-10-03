using UnrealBuildTool;
public class BackstageDepot : ModuleRules
{
    public BackstageDepot(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","InputCore","Slate","SlateCore"});
    }
}
