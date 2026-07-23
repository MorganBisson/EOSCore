using UnrealBuildTool;

public class EOSCore : ModuleRules
{
	public EOSCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings"
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"OnlineSubsystem",
				"OnlineSubsystemUtils"
			});
	}
}
