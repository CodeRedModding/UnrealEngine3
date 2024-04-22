// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Serialization;


namespace P4PopulateDepot
{
	public partial class Utils
	{
		/// <summary>
		/// Structure that we serialize config info file key/value pairs into.
		/// </summary>
		public class ConfigKeyValues
		{
			[DescriptionAttribute("The config section the key and value pair belong to.")]
			[XmlElementAttribute]
			public string SectionName { get; set; }

			[DescriptionAttribute("The key string.")]
			[XmlElementAttribute]
			public string KeyName { get; set; }

			[DescriptionAttribute("The value string.")]
			[XmlElementAttribute]
			public string ValueString { get; set; }

			public ConfigKeyValues()
			{
				SectionName = String.Empty;
				KeyName = String.Empty;
				ValueString = String.Empty;
			}
		}


		/// <summary>
		/// Base interface used for all classes used to describe config file(ini file) entries
		/// </summary>
		public interface BaseEntry
		{

			/// <summary>
			/// Converts the instance to a string representation appropriate for the config file.
			/// </summary>
			/// <returns>A string representing the instance.</returns>
			string AsString();

			/// <summary>
			/// Converts from a string representation of the object and populates the instance.
			/// </summary>
			/// <param name="InString">String representation of the a config entry.</param>
			void FromString(string InString);
		}


		/// <summary>
		/// Class used to store the contents of a config file comment entry
		/// </summary>
		public class CommentEntry : BaseEntry
		{
			public string RawText;

			public CommentEntry()
			{
				RawText = string.Empty;
			}

			public CommentEntry(string InRawText)
			{
				RawText = InRawText;
			}

			public string AsString()
			{
				return RawText;
			}

			public void FromString(string InString)
			{
				RawText = InString;
			}
		}


		/// <summary>
		/// Class used to store the contents of a config file key/value entry
		/// </summary>
		public class ConfigEntry : BaseEntry
		{
			public string Key;
			public string Value;

			public ConfigEntry()
			{
				Key = string.Empty;
				Value = string.Empty;
			}

			public ConfigEntry(string InKey, string InValue)
			{
				Key = InKey;
				Value = InValue;
			}

			public string AsString()
			{
				if (Key == string.Empty)
				{
					return string.Empty;
				}
				return Key + "=" + Value;
			}

			public void FromString(string InString)
			{
				int index = InString.IndexOf("=");
				if (index > 0)
				{
					Key = InString.Substring(0, index).TrimEnd();

					// Handle empty values in a special way
					if (index < InString.Length - 1)
					{
						Value = InString.Substring(index + 1, InString.Length - index - 1).TrimStart();
					}
					else
					{
						Value = string.Empty;
					}
				}
				else
				{
					// We got an unexpected format for the key/value
					Key = string.Empty;
					Value = string.Empty;
				}
			}
		}


		/// <summary>
		/// Class that represents an entire config file structure
		/// </summary>
		public class ConfigFile
		{
			// Characters that start a comment line
			private static readonly char[] CommentStarters = ";#/".ToCharArray();

			Dictionary<string, List<BaseEntry>> ConfigSections = new Dictionary<string, List<BaseEntry>>();

			public ConfigFile()
			{
			}

			/// <summary>
			/// Loads a config file and populates the instance structures.
			/// </summary>
			/// <param name="InFileName">Name of the config file to load.</param>
			public void LoadConfigFile(string InFileName)
			{
				if (!File.Exists(InFileName))
				{
					return;
				}

				using (StreamReader reader = new StreamReader(InFileName))
				{
					string LineStr, CurrSection = null;
					while (reader.Peek() != -1)
					{
						LineStr = reader.ReadLine().Trim();

						if (LineStr.Length == 0)
						{
							// We just treat empty lines as blank comments.
							if (CurrSection != null)
							{
								ConfigSections[CurrSection].Add(new CommentEntry(string.Empty));
							}
						}
						else if (CommentStarters.Contains(LineStr[0]))
						{
							if (CurrSection != null)
							{
								ConfigSections[CurrSection].Add(new CommentEntry(LineStr));
							}
						}
						else if (CurrSection != null && (LineStr.IndexOf("=") > 0))
						{
							ConfigEntry AnEntry = new ConfigEntry();
							AnEntry.FromString(LineStr);

							ConfigSections[CurrSection].Add(AnEntry);
						}
						else if (LineStr.Length > 2 && LineStr.StartsWith("[") && LineStr.EndsWith("]"))
						{
							CurrSection = LineStr.Substring(1, LineStr.Length - 2).Trim();

							if (!ConfigSections.ContainsKey(CurrSection))
							{
								ConfigSections.Add(CurrSection, new List<BaseEntry>());
							}
						}
					}
				}
			}


			/// <summary>
			/// Saves the config file using the provided StreamWriter.
			/// </summary>
			/// <param name="writer">The StreamWriter to use.</param>
			void SaveConfigFile(StreamWriter InStreamWriter)
			{
				lock (ConfigSections)
				{
					if (ConfigSections.Count > 0)
					{
						foreach (KeyValuePair<string, List<BaseEntry>> Section in ConfigSections)
						{
							int ConfigEntryCount = 0;
							for (int i = 0; i < Section.Value.Count; i++)
							{
								if (Section.Value[i].GetType() == typeof(ConfigEntry))
								{
									ConfigEntryCount++;
								}
							}

							// Write out the section only if it has any config entries
							if (ConfigEntryCount > 0)
							{
								InStreamWriter.WriteLine("[" + Section.Key + "]");
								foreach (BaseEntry entry in Section.Value)
								{
									InStreamWriter.WriteLine(entry.AsString());
								}
							}
						}
					}
				}
			}

			/// <summary>
			/// Saves the config file to the provided destination path.
			/// </summary>
			/// <param name="writer">The file path to save the config file to.</param>
			public void SaveConfigFile(string InPath)
			{
				StreamWriter Writer = File.CreateText(InPath);
				SaveConfigFile(Writer);
				Writer.Close();
			}

			/// <summary>
			/// Saves the config file using the provided Stream.
			/// </summary>
			/// <param name="writer">The Stream to use.</param>
			public void SaveConfigFile(Stream InStream)
			{
				StreamWriter Writer = new StreamWriter(InStream);
				SaveConfigFile(Writer);
			}

			public Dictionary<string, List<BaseEntry>> Sections
			{
				get { return ConfigSections; }
			}
		}


		public void EnableEditorSourceControlSupport()
		{
			string EditorCfgPath = Path.Combine(GetProjectRoot(), "UDKGame\\Config\\UDKEditorUserSettings.ini");

			if (!File.Exists(EditorCfgPath))
			{
				return;
			}

			if (P4ConInfo.P4Con == null || !P4ConInfo.P4Con.IsValidConnection(true, true))
			{
				return;
			}

			ConfigFile EditorCfgFile = new ConfigFile();
			EditorCfgFile.LoadConfigFile(EditorCfgPath);

			string SrcControlSectionName = "SourceControl";
			List<BaseEntry> SectionEntries = null;
			if (EditorCfgFile.Sections.ContainsKey(SrcControlSectionName))
			{
				SectionEntries = EditorCfgFile.Sections[SrcControlSectionName];
			}
			else
			{
				SectionEntries = new List<BaseEntry>();
				EditorCfgFile.Sections.Add(SrcControlSectionName, SectionEntries);
			}

			ConfigEntry SCCDisabledEntry = (ConfigEntry)SectionEntries.Find(Entry => (Entry.GetType() == typeof(ConfigEntry) && ((ConfigEntry)Entry).Key == "Disabled") );
			ConfigEntry SCCPortNameEntry = (ConfigEntry)SectionEntries.Find(Entry => (Entry.GetType() == typeof(ConfigEntry) && ((ConfigEntry)Entry).Key == "PortName") );
			ConfigEntry SCCUserNameEntry = (ConfigEntry)SectionEntries.Find(Entry => (Entry.GetType() == typeof(ConfigEntry) && ((ConfigEntry)Entry).Key == "UserName") );
			ConfigEntry SCCClientSpecEntry = (ConfigEntry)SectionEntries.Find(Entry => (Entry.GetType() == typeof(ConfigEntry) && ((ConfigEntry)Entry).Key == "ClientSpecName") );

			if (SCCDisabledEntry != null)
			{
				SCCDisabledEntry.Value = "false";
			}
			else
			{
				SectionEntries.Add(new ConfigEntry("Disabled", "false"));
			}

			if (SCCPortNameEntry != null)
			{
				SCCPortNameEntry.Value = P4ConInfo.Port;
			}
			else
			{
				SectionEntries.Add(new ConfigEntry("PortName", P4ConInfo.Port));
			}

			if (SCCUserNameEntry != null)
			{
				SCCUserNameEntry.Value = P4ConInfo.User;
			}
			else
			{
				SectionEntries.Add(new ConfigEntry("UserName", P4ConInfo.User));
			}

			if (SCCClientSpecEntry != null)
			{
				SCCClientSpecEntry.Value = P4ConInfo.Client;
			}
			else
			{
				SectionEntries.Add(new ConfigEntry("ClientSpecName", P4ConInfo.Client));
			}

			EditorCfgFile.SaveConfigFile(EditorCfgPath);
		}
	}
}
