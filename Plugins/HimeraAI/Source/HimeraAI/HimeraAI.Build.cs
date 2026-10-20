using System.IO;
using UnrealBuildTool;

public class HimeraAI : ModuleRules
{
    public HimeraAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // The vendored core uses C++20 (defaulted operator==, std::map::contains).
        CppStandard = CppStandardVersion.Cpp20;

        // The core reports bad input with C++ exceptions. The integration layer catches
        // every one of them, so none cross into Blueprint/engine code.
        bEnableExceptions = true;

        // Core/*.cpp are plain standard C++; keep them out of unity builds so that
        // file-local names cannot collide with engine sources.
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "NavigationSystem"
        });

        // himera/*.hpp is private: no engine or game module sees the core headers.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Core", "include"));
    }
}
