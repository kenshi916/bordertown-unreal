using UnrealBuildTool;
using System.Collections.Generic;

public class BorderTownEditorTarget : TargetRules
{
    public BorderTownEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("BorderTown");
        ExtraModuleNames.Add("BackstageDepot");
    }
}
