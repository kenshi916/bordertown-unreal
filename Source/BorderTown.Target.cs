using UnrealBuildTool;
using System.Collections.Generic;

public class BorderTownTarget : TargetRules
{
    public BorderTownTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("BorderTown");
        ExtraModuleNames.Add("BackstageDepot");
    }
}
