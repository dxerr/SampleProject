// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemEOS : ModuleRules
{
	public OnlineSubsystemEOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("ONLINESUBSYSTEMEOS_PACKAGE=1");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"EOSSDK",
				"EOSShared"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreOnline",
				"CoreUObject",
				"Engine",
				"EOSVoiceChat",
				"Json",
				"NetCore",
				"OnlineBase",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"Sockets",
				"SocketSubsystemEOS",
				"VoiceChat"
			}
		);

		PrivateDefinitions.Add("USE_XBL_XSTS_TOKEN=" + (bUseXblXstsToken ? "1" : "0"));
		PrivateDefinitions.Add("USE_PSN_ID_TOKEN=" + (bUsePsnIdToken ? "1" : "0"));
		PrivateDefinitions.Add("ADD_USER_LOGIN_INFO=" + (bAddUserLoginInfo ? "1" : "0"));
		PrivateDefinitions.Add("EOS_AUTH_TOKEN_SAVEGAME_STORAGE=" + (bAuthTokenSavegameStorage ? "1" : "0"));
	}

	protected virtual bool bUseXblXstsToken { get { return false; } }
	protected virtual bool bUsePsnIdToken { get { return false; } }
	// [Ex] Modified (UE5.8 재적용): PC(Win64) Device ID 로그인 시 UserLoginInfo가 비어
	// EOS_InvalidParameters가 발생하는 버그 우회 — 기본값 true로 ADD_USER_LOGIN_INFO=1 강제.
	// (콘솔 플랫폼 확장은 별도 서브클래스에서 이미 true override 중이라 영향 없음)
	protected virtual bool bAddUserLoginInfo { get { return true; } }
	protected virtual bool bAuthTokenSavegameStorage { get { return false; } }
}
