/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Xml;
using System.Xml.Serialization;

namespace StatsViewer
{
	/// <summary>
	/// Summary description for SavedSettings.
	/// </summary>
	public class SavedSettings
	{
		/// <summary>
		/// Holds the last IP address that was connected to
		/// </summary>
		[XmlAttribute]
		public string LastIpAddress = "127.0.0.1";
		/// <summary>
		/// Holds the last port that was connected to
		/// </summary>
		[XmlAttribute]
		public int LastPort = 13002;  // Must match default "StatsTrafficPort" in UDP stat provider

		/// <summary>
		/// Default ctor for XML serialization
		/// </summary>
		public SavedSettings()
		{
		}
	}
}
