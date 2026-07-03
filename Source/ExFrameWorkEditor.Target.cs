using UnrealBuildTool;
using System.Collections.Generic;

public class ExFrameWorkEditorTarget : TargetRules
{
	public ExFrameWorkEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ExFrameWork");
	}
}
