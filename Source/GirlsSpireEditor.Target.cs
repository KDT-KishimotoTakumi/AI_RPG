using UnrealBuildTool;

public class GirlsSpireEditorTarget : TargetRules
{
    public GirlsSpireEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("GirlsSpire");
        if (System.Environment.GetEnvironmentVariable("GIRLS_SPIRE_COVERAGE") == "1")
        {
            bOverrideBuildEnvironment = true;
            WindowsPlatform.Compiler = WindowsCompiler.Clang;
            bUseUnityBuild = false;
            bUsePCHFiles = false;
            AdditionalCompilerArguments = "-fprofile-instr-generate -fcoverage-mapping";
            // LauncherのMSVC版CoreにはClang専用Windowsラッパーがない。
            // Win64で同じABIのOSインポートへ接続し、エンジン本体の変更を避ける。
            AdditionalLinkerArguments =
                " /alternatename:__imp_?DeleteCriticalSection@Windows@@YAXPEAU_RTL_CRITICAL_SECTION@@@Z=__imp_DeleteCriticalSection" +
                " /alternatename:__imp_?EnterCriticalSection@Windows@@YAXPEAU_RTL_CRITICAL_SECTION@@@Z=__imp_EnterCriticalSection" +
                " /alternatename:__imp_?LeaveCriticalSection@Windows@@YAXPEAU_RTL_CRITICAL_SECTION@@@Z=__imp_LeaveCriticalSection" +
                " /alternatename:__imp_?InitializeCriticalSectionAndSpinCount@Windows@@YAHPEAU_RTL_CRITICAL_SECTION@@K@Z=__imp_InitializeCriticalSectionAndSpinCount" +
                " /alternatename:__imp_?GetCurrentThreadId@Windows@@YAKXZ=__imp_GetCurrentThreadId" +
                " /alternatename:__imp_?FlsGetValue@Windows@@YAPEAXK@Z=__imp_FlsGetValue" +
                " /alternatename:__imp_?QueryPerformanceCounter@Windows@@YAHPEAT_LARGE_INTEGER@@@Z=__imp_QueryPerformanceCounter";
        }
    }
}
