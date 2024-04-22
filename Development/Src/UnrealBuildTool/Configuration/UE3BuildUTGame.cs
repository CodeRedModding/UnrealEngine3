/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace UnrealBuildTool
{
	class UE3BuildUDKGame : UE3BuildGame
	{
		/** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		public string GetGameName()
		{
			return "UDKGame";
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
				case UnrealTargetPlatform.Win64:
				case UnrealTargetPlatform.Win32:
					if( UE3BuildTarget.SupportsOSSSteamworks() )
					{
						return ( "Steamworks" );
					}
					break;

				case UnrealTargetPlatform.IPhone:
					if (UE3BuildTarget.SupportsOSSGameCenter())
					{
						return ("GameCenter");
					}
					break;

				}
			}

			return ( "PC" );
		}

		/** Returns true if the game wants to have PC ES2 simulator (ie ES2 Dynamic RHI) enabled */
		public bool ShouldCompileES2()
		{
			return true;
		}

        /** Returns whether PhysX should be compiled on mobile platforms */
        public bool ShouldCompilePhysXMobile()
        {
            return UE3BuildConfiguration.bCompilePhysXWithMobile;
        }
		
		/** Allows the game add any global environment settings before building */
        public void GetGameSpecificGlobalEnvironment(CPPEnvironment GlobalEnvironment, UnrealTargetPlatform Platform)
        {
			//UT Relies on USE_ACTOR_VISIBILITY_HISTORY for visibility checks, but it is expensive.
			//TURN ON IF NOT NEEDED.
			//GlobalEnvironment.Definitions.Add("USE_ACTOR_VISIBILITY_HISTORY=1");
		}

        /** Allows the game to add any Platform/Configuration environment settings before building */
        public void GetGameSpecificPlatformConfigurationEnvironment(CPPEnvironment GlobalEnvironment, LinkEnvironment FinalLinkEnvironment)
        {

        }

        /** Returns the xex.xml file for the given game */
		public FileItem GetXEXConfigFile()
		{
			return FileItem.GetExistingItemByPath("UTGame/Live/xex.xml");
		}

        /** Allows the game to add any additional environment settings before building */
		public void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects)
        {
		GameProjects.Add(new UE3ProjectDesc("UDKBase/UDKBase.vcxproj"));
            GameCPPEnvironment.IncludePaths.Add("UDKBase/Inc");

		GameProjects.Add(new UE3ProjectDesc("UTGame/UTGame.vcxproj"));

			if (UE3BuildConfiguration.bBuildEditor && 
				(GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64 ))
			{
 				GameProjects.Add(new UE3ProjectDesc("UTEditor/UTEditor.vcxproj"));
 				GameCPPEnvironment.IncludePaths.Add("UTEditor/Inc");
			}

            GameCPPEnvironment.Definitions.Add("GAMENAME=UTGAME");
            GameCPPEnvironment.Definitions.Add("IS_UTGAME=1");
            GameCPPEnvironment.Definitions.Add("SUPPORTS_TILT_PUSHER=1");
        }
	}

	class UE3BuildUDKGameDLL : UE3BuildUDKGame
	{
		/** Returns a subplatform (e.g. dll) to disambiguate object files */
		override public string GetSubPlatform()
		{
			return ( "DLL" );
		}
	}
}
