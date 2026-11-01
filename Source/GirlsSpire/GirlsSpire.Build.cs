using UnrealBuildTool;

public class GirlsSpire : ModuleRules
{
    public GirlsSpire(ReadOnlyTargetRules Target) : base(Target)
    {
        // 匿名名前空間の補助関数を翻訳単位ごとに分離し、Gitの変更状態でビルド結果を変えない。
        bUseUnity = false;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;
        PrivateIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "InputCore" });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("AutomationDriver");
        if (System.Environment.GetEnvironmentVariable("GIRLS_SPIRE_COVERAGE") == "1")
        {
            OptimizeCode = CodeOptimization.Never;
            PrivateDefinitions.Add("GIRLS_SPIRE_COVERAGE=1");
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(
                System.Environment.GetEnvironmentVariable("LLVM_PATH"),
                "lib/clang/20/lib/windows/clang_rt.profile-x86_64.lib"));
        }
    }
}
