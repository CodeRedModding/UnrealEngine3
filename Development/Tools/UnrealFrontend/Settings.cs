/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnrealControls;

namespace UnrealFrontend
{
	public class Settings
	{
		public static String SettingsFilename { get { return "UnrealFrontend.Settings.xml"; } }
		public static String ProfileDirLocation { get { return System.IO.Path.Combine(System.Windows.Forms.Application.StartupPath, "UnrealFrontend.Profiles"); } }
		public static String UISettingsFilename { get { return "UnrealFrontend.UISettings.xml"; } }
		public static String StudioSettingsFilename { get { return "UnrealFrontend.StudioSettings.xml"; } }
		
		public Settings(List<String> InKnownGames)
		{
			LastActiveProfile = "";
		}

		public Settings( ) : this( new List<String>() )
		{
		}

		public String LastActiveProfile {get; set;}

		public Profile FindProfileToSelectOnStartup( System.Collections.ObjectModel.ObservableCollection<Profile> InProfiles )
		{
			// Restore the profile we had selected last session
			if (LastActiveProfile != null)
			{
				String LastActiveProfileName = LastActiveProfile.Trim();
				Profile LastActiveProfileInstance = InProfiles.FirstOrDefault(SomeProfile => SomeProfile.Name == LastActiveProfileName);
				if (LastActiveProfileInstance != null)
				{
					return LastActiveProfileInstance;
				}
			}

			return InProfiles[0];
		}
	}

	public class StudioSettings
	{
		public StudioSettings( )
		{
			GameSpecificCookerOptionsHelpUrls = new SerializableDictionary<String, String>();
			GameSpecificMapExtensions = new SerializableDictionary<String, String>();

			UnPropHostDirectory = "\\\\prop-06\\Builds";
		}

		public void AddGamesIfMissing(List<String> InKnownGames)
		{
			foreach (String GameName in InKnownGames)
			{
				if ( ! GameSpecificCookerOptionsHelpUrls.ContainsKey(GameName))
				{
					GameSpecificCookerOptionsHelpUrls.Add(GameName, "");
				}

				if (!GameSpecificMapExtensions.ContainsKey(GameName))
				{
					GameSpecificMapExtensions.Add(GameName, "");
				}
			}
		}

		// Host PC name for unprop destination
		public String UnPropHostDirectory { get; set; }

		// GameName -> Web URL with game-specific cooker option help.
		// e.g. "GearGame" -> "http://udn.epicgames.com/GearsCookerHelp"
		public SerializableDictionary<String, String> GameSpecificCookerOptionsHelpUrls { get; set; }

		public SerializableDictionary<String, String> GameSpecificMapExtensions { get; set; }
	}

}
