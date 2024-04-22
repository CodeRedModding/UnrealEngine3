/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	class UE3BuildDukeGame : UE3BuildGame
	{
        /** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		public string GetGameName()
		{
			return "DukeGame";
		}
		
		/** Returns a subplatform (e.g. dll) to disambiguate object files */
		public string GetSubPlatform()
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

			return ( "PC" );
		}

		/** Returns true if the game wants to have PC ES2 simulator (ie ES2 Dynamic RHI) enabled */
		public bool ShouldCompileES2()
		{
			return false;
		}

        /** Returns whether PhysX should be compiled on mobile platforms */
        public bool ShouldCompilePhysXMobile()
        {
            return UE3BuildConfiguration.bCompilePhysXWithMobile;
        }

        /** Allows the game add any global environment settings before building */
        public void GetGameSpecificGlobalEnvironment(CPPEnvironment GlobalEnvironment, UnrealTargetPlatform Platform)
        {

        }

        /** Allows the game to add any Platform/Configuration environment settings before building */
        public void GetGameSpecificPlatformConfigurationEnvironment(CPPEnvironment GlobalEnvironment, LinkEnvironment FinalLinkEnvironment)
        {

        }

        /** Returns the xex.xml file for the given game */
		public FileItem GetXEXConfigFile()
		{
			return FileItem.GetExistingItemByPath("DukeGame/Live/xex.xml");
		}

        /** Allows the game to add any additional environment settings before building */
		public void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects)
		{
			GameCPPEnvironment.IncludePaths.Add("DukeGame/Inc");
			GameProjects.Add(new UE3ProjectDesc("DukeGame/DukeGame.vcxproj"));

			if (UE3BuildConfiguration.bBuildEditor &&
				(GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64))
			{
 				GameProjects.Add(new UE3ProjectDesc("DukeEditor/DukeEditor.vcxproj"));
 				GameCPPEnvironment.IncludePaths.Add("DukeEditor/Inc");
			}

			GameCPPEnvironment.Definitions.Add("GAMENAME=DUKEGAME");
			GameCPPEnvironment.Definitions.Add("IS_DUKEGAME=1");

		}
	}
}
