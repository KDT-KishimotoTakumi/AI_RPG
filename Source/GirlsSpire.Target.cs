using UnrealBuildTool;

public class GirlsSpireTarget : TargetRules
{
    public GirlsSpireTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("GirlsSpire");
    }
}
