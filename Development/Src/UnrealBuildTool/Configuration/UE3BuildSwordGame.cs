/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace UnrealBuildTool
{
	class UE3BuildSwordGame : UE3BuildGame
	{
		/** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		public string GetGameName()
		{
			return "SwordGame";
		}

		/** Returns a subplatform (e.g. dll) to disambiguate object files */
		virtual public string GetSubPlatform()
		{
			return ( "" );
		}

		/** Get the desired OnlineSubsystem. */
		public string GetDesiredOnlineSubsystem( CPPEnvironment CPPEnv, UnrealTargetPlatform Platform )
		{
			string ForcedOSS = UE3BuildTarget.ForceOnlineSubsystem( Platform );
			if( ForcedOSS != null )
			{
				return ( ForcedOSS );
			}
			else
			{
				switch( Platform )
				{
				case UnrealTargetPlatform.IPhone:
					if (UE3BuildTarget.SupportsOSSGameCenter())
					{
						return ("GameCenter");
					}
					break;
				}
			}

			return "PC";
		}

		/** Returns true if the game wants to have PC ES2 simulator (ie ES2 Dynamic RHI) enabled */
		public bool ShouldCompileES2()
		{
			return true;
		}

		/** Returns whether PhysX should be compiled on mobile platforms */
		public bool ShouldCompilePhysXMobile()
		{
			return false;
		}

		/** Allows the game add any global environment settings before building */
		public void GetGameSpecificGlobalEnvironment(CPPEnvironment GlobalEnvironment, UnrealTargetPlatform Platform)
		{
            UE3BuildConfiguration.bCompileScaleform = false;
			GlobalEnvironment.Definitions.Add("CHAIR_INTERNAL=1");
			GlobalEnvironment.Definitions.Add("IS_SWORDGAME=1");
		}

		/** Allows the game to add any Platform/Configuration environment settings before building */
		public void GetGameSpecificPlatformConfigurationEnvironment(CPPEnvironment GlobalEnvironment, LinkEnvironment FinalLinkEnvironment)
		{
		}
		
		/** Returns the xex.xml file for the given game */
		public FileItem GetXEXConfigFile()
		{
			return FileItem.GetExistingItemByPath("SwordGame/Live/xex.xml");
		}

        /** Allows the game to add any additional environment settings before building */
		public void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects)
		{
			GameCPPEnvironment.IncludePaths.Add("SwordGame/Inc");
			GameProjects.Add(new UE3ProjectDesc("SwordGame/SwordGame.vcxproj"));

			if (UE3BuildConfiguration.bBuildEditor &&
				(GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64))
			{
				GameProjects.Add(new UE3ProjectDesc("SwordEditor/SwordEditor.vcxproj"));
				GameCPPEnvironment.IncludePaths.Add("SwordEditor/Inc");
			}

			GameCPPEnvironment.Definitions.Add("GAMENAME=SWORDGAME");
			GameCPPEnvironment.Definitions.Add("SUPPORTS_TILT_PUSHER=1");
		}
	}
}
