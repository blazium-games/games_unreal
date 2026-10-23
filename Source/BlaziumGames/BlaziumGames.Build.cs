using UnrealBuildTool;

public class BlaziumGames : ModuleRules
{
	public BlaziumGames(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});
		PrivateDependencyModuleNames.Add("Projects");
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/blazium_api64.dll");
			PublicDelayLoadDLLs.Add("blazium_api64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64)
		{
			RuntimeDependencies.Add("$(PluginDir)/Binaries/" + Target.Platform.ToString() + "/libblazium_api.so");
		}
	}
}
