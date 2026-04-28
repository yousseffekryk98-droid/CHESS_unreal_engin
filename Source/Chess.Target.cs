// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ChessTarget : TargetRules
{
	public ChessTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Updated to V6 for Unreal Engine 5.7 compatibility
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		ExtraModuleNames.AddRange( new string[] { "Chess" } );
	}
}