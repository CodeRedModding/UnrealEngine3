/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Xml;
using System.Xml.Serialization;

namespace PackageDiffFrontEnd
{
	/// <summary>
	/// Summary description for PackageDiffSettings.
	/// </summary>
	public class PackageDiffSettings
	{
		/// <summary>
		/// This is the set of supported game names
		/// </summary>
		[XmlArray("AvailableGames")]
		public string[] AvailableGames = new string[0];
		/// <summary>
		/// This is the set of supported PC build configurations
		/// </summary>
		[XmlArray("AvailableConfigurations")]
		public string[] AvailableConfigurations = new string[0];

		/// <summary>
		/// Needed for XML serialization. Does nothing
		/// </summary>
		public PackageDiffSettings()
		{
		}

		/// <summary>
		/// Constructor which initializes the available options for the various controls in the app.
		/// </summary>
		/// <param name="Ignored">Ignored. Used to have a specialized ctor</param>
		public PackageDiffSettings(int Ignored)
		{
			// Build the default set of games
			//
			// TODO: generate this list based on the game executable in the Binaries directory
			//
			AvailableGames = new string[4];
			AvailableGames[0] = "ExampleGame";
			AvailableGames[1] = "GearGame";
			AvailableGames[2] = "UTGame";
			AvailableGames[3] = "WarGame";

			// Build the default set of platforms
			AvailableConfigurations = new string[2];
			AvailableConfigurations[0] = "Release";
			AvailableConfigurations[1] = "Debug";
		}
	}
}
