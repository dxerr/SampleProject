using UnrealBuildTool;
using System.Collections.Generic;

public class ExFrameWorkTarget : TargetRules
{
	public ExFrameWorkTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ExFrameWork");
	}
}
