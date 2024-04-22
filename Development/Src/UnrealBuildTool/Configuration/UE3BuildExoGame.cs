/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	class UE3BuildExoGame : UE3BuildGame
	{
        /** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		public string GetGameName()
		{
			return "ExoGame";
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
			if ( ForcedOSS != null )
			{
				return ( ForcedOSS );
			}

			return ((Platform == UnrealTargetPlatform.IPhone) ? "GameCenter" : "PC");
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
            if (Platform == UnrealTargetPlatform.IPhone)
            {
                //GlobalEnvironment.Definitions.Add("FORCE_SCALEFORM_SHIPPING=1");
            }

        }

        /** Allows the game to add any Platform/Configuration environment settings before building */
        public void GetGameSpecificPlatformConfigurationEnvironment(CPPEnvironment GlobalEnvironment, LinkEnvironment FinalLinkEnvironment)
        {

        }

        /** Returns the xex.xml file for the given game */
		public FileItem GetXEXConfigFile()
		{
			return FileItem.GetExistingItemByPath("ExoGame/Live/xex.xml");
		}

        /** Allows the game to add any additional environment settings before building */
		public void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects)
		{
			GameCPPEnvironment.IncludePaths.Add("ExoGame/Inc");
			GameProjects.Add( new UE3ProjectDesc( "ExoGame/ExoGame.vcxproj") );

			if (UE3BuildConfiguration.bBuildEditor &&
				(GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64))
			{
				GameProjects.Add( new UE3ProjectDesc("ExoEditor/ExoEditor.vcxproj") );
				GameCPPEnvironment.IncludePaths.Add("ExoEditor/Inc");
			}

			GameCPPEnvironment.Definitions.Add("GAMENAME=EXOGAME");
            GameCPPEnvironment.Definitions.Add("SUPPORTS_TILT_PUSHER=1");
        }
	}
}
