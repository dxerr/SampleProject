using UnrealBuildTool;
using System.Collections.Generic;

public class ExFrameWorkEditorTarget : TargetRules
{
	public ExFrameWorkEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ExFrameWork");
	}
}
