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


namespace UnSetup
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
		/// Structure that we serialize project template config info files into.  ex. ConfigInfo.xml
		/// </summary>
		public class ConfigOptions
		{

			[DescriptionAttribute("The config file sections to exclude.")]
			[XmlArrayAttribute]
			public string[] ConfigSectionsToExclude { get; set; }

			[DescriptionAttribute("The config file keys to exclude from each section.")]
			[XmlArrayAttribute]
			public string[] ConfigKeysToExculde { get; set; }

			[DescriptionAttribute("The config file values to exclude from each section.")]
			[XmlArrayAttribute]
			public string[] ConfigValuesToExclude { get; set; }

			[DescriptionAttribute("The key and value pairs to add to a specified section")]
			[XmlArrayAttribute]
			public ConfigKeyValues[] ConfigKeyValuesToAdd { get; set; }

			public ConfigOptions()
			{
				ConfigSectionsToExclude = new string[] { };
				ConfigKeysToExculde = new string[] { };
				ConfigValuesToExclude = new string[] { };
				ConfigKeyValuesToAdd = new ConfigKeyValues[] { };
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

		// Used to cache manifest file info for name validation checks
		private FolderProperties NameValidationRootFolderProperty = null;


		/// <summary>
		/// Validates the name of the custom project that was provided by the user.  
		/// </summary>
		/// <param name="ProjectName">Name of the project.</param>
		/// <returns>
		///   <c>true</c> if the project name meets requirements; otherwise, <c>false</c>.
		/// </returns>
		public bool ValidateProjectName(string ProjectName)
		{
			if (ProjectName == null || ProjectName == string.Empty)
			{
				return false;
			}

			Regex Rx = new Regex("^[a-zA-Z][a-zA-Z0-9_]*$");
			Match MatchResult = Rx.Match(ProjectName);
			if(!MatchResult.Success)
			{
				return false;
			}

			// The short name will be used to generate the custom project script package folder in the Development\src directory.  We don't 
			//  want any conflicts between existing script package names and this string.  So, if the name is valid at this point
			//  we will check the manifest info to see if there are any conflicting directories in the src folder.
			if (NameValidationRootFolderProperty == null)
			{
				// We cache off the manifest info so we only have to read it the first time this function is called.
				NameValidationRootFolderProperty = ReadXml<FolderProperties>(ManifestFileName);
			}

			FolderProperties DevFolder = NameValidationRootFolderProperty.FindFolder("Development");
			FolderProperties SrcFolder = DevFolder != null ? DevFolder.FindFolder("Src") : null;

			if(SrcFolder != null)
			{
				// If we find a development\src folder in the manifest, we will check to see if our user provided short name conflicts with one 
				//   of the folder names in there.  If it does, we can't consider the name valid.
				if (SrcFolder.FindFolder(ProjectName) != null)
				{
					return false;
				}
			}

			return true;
		}

		/// <summary>
		/// Calculates and Returns the size of the project the user currently has selected.  The corresponding manifest file is used to calculate this size.
		/// </summary>
		/// <returns>Size of the selected project</returns>
		public UInt64 GetProjectInstallSize()
		{
			UInt64 ReturnVal = 0;

			bool IsGameInstall = IsFileInPackage("Binaries\\UnSetup.Game.xml");
			string LocalManifest;
			string LocalManifestPath;

			if (IsGameInstall)
			{
				LocalManifest = GameManifestFileName;
			}
			else
			{
				LocalManifest = ManifestFileName;
			}
			LocalManifestPath = Path.Combine(Environment.CurrentDirectory, LocalManifest);

			bool bCalcInstallSize = true;
			if (!File.Exists(LocalManifestPath))
			{
				bCalcInstallSize = ExtractSingleFile(LocalManifest);
			}

			if (bCalcInstallSize && File.Exists(LocalManifestPath))
			{
				FolderProperties LocalRoot = ReadXml<FolderProperties>(LocalManifestPath);

				// If this is the empty project we will mark some files for exclusion based on the rules
				//  we find for that template in the ProjectTemplates folder
				if (!IsGameInstall && bIsCustomProject)
				{
					string TemplateFolderName = "UDKGame\\ProjectTemplates\\Template1";
					string TemplateInfoFile = Path.Combine(TemplateFolderName, "TemplateInfo.xml");
					ExtractSingleFile(TemplateInfoFile);
					ManifestOptions TemplateOptions = null;
					TemplateOptions = ReadXml<ManifestOptions>(TemplateInfoFile);
					
					foreach (string FileSpec in TemplateOptions.MainFilesToExclude)
					{
						FilterOutFileSpec(LocalRoot, FileSpec);
					}
				}

				ReturnVal = LocalRoot.GetFolderSize();
			}

			return ReturnVal;

		}


		/// <summary>
		/// Recursively iterates through a provided folder structure and extracts all sub files that are not marked for removal(size -1).
		/// </summary>
		/// <param name="AncestorPath">The full path for the passed in folder.</param>
		/// <param name="InFolderProperties">The folder to extract files and folders for.</param>
		/// <returns>
		///   <c>true</c> if there were no errors encountered extracting files; otherwise, <c>false</c>.
		/// </returns>
        public bool RecursiveExtract(string AncestorPath, FolderProperties InFolderProperties)
        {
            bool ReturnResult = true;
            if (InFolderProperties.FolderName != ".")
            {
                AncestorPath = Path.Combine(AncestorPath, InFolderProperties.FolderName);
            }

            foreach(FileProperties File in InFolderProperties.Files)
            {
                if (File.Size != -1)
                {
                    ReturnResult &= ExtractSingleFile(Path.Combine(AncestorPath, File.FileName));
                }
            }

            foreach (FolderProperties Folder in InFolderProperties.Folders)
            {
                if (Folder.Size != -1)
                {
                    ReturnResult &= RecursiveExtract(AncestorPath, Folder);
                }
            }
            return ReturnResult;
        }


		/// <summary>
		/// Copies template files from the UDKGame\ProjectTemplates folder to a target directory.  This function
		/// replicates the template folder directory structure in the target directory.  During the copy step,
		/// special processing is done to ini and script files based user provided info.
		/// </summary>
		/// <param name="TargetDirectory">The target directory we will copy to.</param>
		/// <param name="TemplateFolder">The template folder to copy from.</param>
		/// <returns>
		///   <c>true</c> if there were no errors encountered copying files; otherwise, <c>false</c>.
		/// </returns>
		public bool CopyTemplateFiles(string TargetDirectory, string TemplateFolder)
		{

			bool bSuccess = true;

			// We create all sub directories but we will not continue if the root destination directory does not exist
			if (!Directory.Exists(TargetDirectory))
			{
				bSuccess = false;
				return bSuccess;
			}

			// The template folder will have been extracted to the destination folder, we consider this the source we will copy from
			string SourceDirectory = Path.Combine(TargetDirectory, TemplateFolder);

			// Return early if the source directory does not exist
			if (!Directory.Exists(SourceDirectory))
			{
				bSuccess = false;
				return bSuccess;
			}

			// Get all the sub directories from the source template folder, and make sure the corresponding destination folders exist
			string[] SourceSubDirectories = Directory.GetDirectories(SourceDirectory, "*", SearchOption.AllDirectories);
			string TemplateGameFolder = Path.Combine("Src", "TemplateGame");
			foreach (string SourceDirectoryPath in SourceSubDirectories)
			{
				string TargetDirectoryPath;
				TargetDirectoryPath = SourceDirectoryPath.Replace(SourceDirectory, TargetDirectory);

				// We check to see if this is the TemplateGame folder.  If so, we tweak the path to use the user provided name instead.
				if (TargetDirectoryPath.Contains(TemplateGameFolder))
				{
					TargetDirectoryPath = TargetDirectoryPath.Replace(TemplateGameFolder, Path.Combine("Src", CustomProjectShortName + "Game"));
				}

				// Create all the directories we expect to see for our copy in the destination folder with the exception of the TemplateGame folder.
				if (!Directory.Exists(TargetDirectoryPath))
				{
					try
					{
						Directory.CreateDirectory(TargetDirectoryPath);
					}
					catch (Exception Ex)
					{
						// Failed to create a directory:
						bSuccess = false;
						Console.WriteLine(Ex.Message);
					}
				}
			}

			string[] SourceFiles = Directory.GetFiles(SourceDirectory, "*.*", SearchOption.AllDirectories);

			// Copy all the files from the SourceDirectory directory into the targetDirectory
			for (int SourceFileIndex = 0; SourceFileIndex < SourceFiles.Length; SourceFileIndex++)
			{
				string SourceFilePath = SourceFiles[SourceFileIndex];
				FileInfo SourceFileInfo = new FileInfo( SourceFilePath );

				// Ignore the files in the root of the template folder.
				if (SourceFileInfo.DirectoryName == SourceDirectory)
				{
					continue;
				}

				string TargetFilePath = SourceFilePath.Replace(SourceDirectory, TargetDirectory);

				// We check to see if this file lives in the TemplateGame folder.  If so, we tweak the path to use the user provided name instead.
				if (TargetFilePath.Contains(TemplateGameFolder))
				{
					TargetFilePath = TargetFilePath.Replace(TemplateGameFolder, Path.Combine("Src", CustomProjectShortName + "Game"));
					if (SourceFileInfo.Name.StartsWith("Template"))
					{
						TargetFilePath = TargetFilePath.Replace("Template", CustomProjectShortName);
					}
				}

				// Delete the destination file if it already exists
				FileInfo Dest = new FileInfo( TargetFilePath );
				if( Dest.Exists )
				{
					Dest.IsReadOnly = false;
					Dest.Delete();
				}
							
				bool bIsEditableIniFile = SourceFileInfo.Extension == ".ini" && SourceFileInfo.DirectoryName.Contains(Path.Combine("UDKGame", "Config"));
				bool bIsEditableScriptFile = SourceFileInfo.Extension == ".uc" && SourceFileInfo.DirectoryName.Contains(TemplateGameFolder);
				if (bIsEditableIniFile || bIsEditableScriptFile)
				{
					// Handle ini and TemplateGame script files in a special way.  Here we read them in, change all instances of the template name to the user provided game name.
					string FileContents = null;
					try
					{
						FileContents = File.ReadAllText(SourceFilePath);
					}
					catch(Exception Ex)
					{
						bSuccess = false;
						Console.WriteLine(Ex.Message);
					}

					if(FileContents != null)
					{
						FileContents = FileContents.Replace("TEMPLATE_FULL_NAME", CustomProjectShortName);
						FileContents = FileContents.Replace("TEMPLATE_SHORT_NAME", CustomProjectShortName);

						try
						{
							File.WriteAllText(TargetFilePath, FileContents);
						}
						catch (Exception Ex)
						{
							bSuccess = false;
							Console.WriteLine(Ex.Message);
						}
					}
				}
				else
				{
					try
					{
						SourceFileInfo.CopyTo(TargetFilePath, true);
					}
					catch (Exception Ex)
					{
						bSuccess = false;
						Console.WriteLine(Ex.Message);
					}
				}
			}

			return bSuccess;
		}



		/// <summary>
		/// Entry point into custom project file extraction.  This function will only extract the files needed for the project.
		/// Here we also manage the opening and clozing of the zip package.
		/// </summary>
		/// <param name="DestFolder">The destination folder to extract to.</param>
		/// <returns>
		///   <c>true</c> if there were no errors encountered extracting files; otherwise, <c>false</c>.
		/// </returns>
		public bool ExtractCustomProjectFiles(string DestFolder)
		{
			// Save off the working directory and replace it with the destination until this function completes
			string OldDirectory = Environment.CurrentDirectory;
			Environment.CurrentDirectory = DestFolder;
			
			OpenPackagedZipFile( MainModuleName, UDKStartZipSignature );

            bool UnzipResult = true;
			
			// To figure out what files need to be extracted we
			//  first grab the manifest file.  We read this in and manipulate it based
			//  on the rules we find in the project template folder that the user selected.
            UnzipResult &= ExtractSingleFile(ManifestFileName);
            
            FolderProperties RootFolderProperty = null;
			RootFolderProperty = ReadXml<FolderProperties>( ManifestFileName );

			string TemplateFolderName = "UDKGame\\ProjectTemplates\\Template1";
            string TemplateInfoFile = Path.Combine(TemplateFolderName, "TemplateInfo.xml");
			ExtractSingleFile(TemplateInfoFile);
            ManifestOptions TemplateOptions = null;
			TemplateOptions = ReadXml<ManifestOptions>(TemplateInfoFile);
            
            int FilterCount = 0;
            foreach (string FileSpec in TemplateOptions.MainFilesToExclude)
            {
                FilterCount += FilterOutFileSpec(RootFolderProperty, FileSpec);
            }

            // Now go through all files and extract the ones that are not marked for exclusion(size set to -1)
            UnzipResult &= RecursiveExtract("", RootFolderProperty);

            ClosePackagedZipFile();

			// Copy the files located in the template folder to the corresponding folders in the destination path
			CopyTemplateFiles(DestFolder, TemplateFolderName);

			// Convert the config files in-place based on the rules we find in the template folder
			string ConfigFolder = Path.Combine(DestFolder, "UDKGame\\Config");
			ProcessGameConfigFiles(ConfigFolder, ConfigFolder);

			Environment.CurrentDirectory = OldDirectory;

			// Max out the progress bar as we are done.
			UpdateProgressBar("", 1, 1);
            return UnzipResult;
		}


		/// <summary>
		/// Used to process config files.  This function will load the ConfigInfo.xml associated with the project template
		/// and use the rules provided in that file to manipulate the config files in the source directory and write to the
		/// destination directory.  If the source and destination are the same this will overwrite the old config files.
		/// </summary>
		/// <param name="InSrcPath">The source path that contains the ini files we wish to modify.</param>
		/// <param name="InDestPath">The desired destination directory for modified config files.  If this is the same as the source directory, the original files will be overwritten. </param>
		private void ProcessGameConfigFiles(string InSrcPath, string InDestPath)
		{
			if (!Directory.Exists(InSrcPath))
			{
				return;
			}

			if( InDestPath != InSrcPath )
			{
				string[] SourceSubDirectories = Directory.GetDirectories(InSrcPath, "*", SearchOption.AllDirectories);

				// Make sure all the target sub directories exist
				foreach (string SourceDirectoryPath in SourceSubDirectories)
				{
					string TargetDirectoryPath = SourceDirectoryPath.Replace(InSrcPath, InDestPath);
					try
					{
						Directory.CreateDirectory(TargetDirectoryPath);
					}
					catch (Exception Ex)
					{
						Console.WriteLine(Ex.Message);
					}
				}
			}

			// Load the rules we will use to modify config files
			ConfigOptions LocalConfigOptions = null;
			string TemplateFolderName = "UDKGame\\ProjectTemplates\\Template1";
			string ConfigInfoFile = Path.Combine(TemplateFolderName, "ConfigInfo.xml");
			LocalConfigOptions = ReadXml<ConfigOptions>(ConfigInfoFile);

			string[] SrcFiles = Directory.GetFiles(InSrcPath, "*.ini", SearchOption.AllDirectories);

			for (int SourceFileIndex = 0; SourceFileIndex < SrcFiles.Length; SourceFileIndex++)
			{
				string TargetFilePath = SrcFiles[SourceFileIndex].Replace(InSrcPath, InDestPath);

				ConfigFile AConfigFile = new ConfigFile();
				AConfigFile.LoadConfigFile(SrcFiles[SourceFileIndex]);

				// Loop through all the sections and Find a list of keys that we want to remove.
				List<string> RemoveKeys = new List<string>();
				foreach (KeyValuePair<string, List<BaseEntry>> Section in AConfigFile.Sections)
				{
					string ConfigSectionName = Section.Key;

					// Using LINQ to see if the section name contains any of the items we wish to exclude
					if (LocalConfigOptions.ConfigSectionsToExclude.Any(s => ConfigSectionName.Contains(s)))
					{
						// We want to remove this item so we add it to the remove list that we will process later
						RemoveKeys.Add(ConfigSectionName);

						// We continue the loop because there is no need to process the key value pairs of a section that will be removed
						continue;
					}

					// If this config section is not marked for removal, we will loop through its list and remove any references that appear in our remove lists
					for (int i = Section.Value.Count - 1; i >= 0; i--)
					{
						if (Section.Value[i].GetType() == typeof(ConfigEntry))
						{
							ConfigEntry Entry = (ConfigEntry)Section.Value[i];

							bool bKeyValNeedsRemoval = false;

							// We use LINQ to figure out if the entry key contains any of the strings specified in the key removal list
							bKeyValNeedsRemoval = LocalConfigOptions.ConfigKeysToExculde.Any(s => Entry.Key.Contains(s));

							if (!bKeyValNeedsRemoval)
							{
								// We use LINQ to figure out if the entry value contains any of the string specified in the value removal list
								bKeyValNeedsRemoval = LocalConfigOptions.ConfigValuesToExclude.Any(s => Entry.Value.Contains(s));
							}

							if (bKeyValNeedsRemoval)
							{
								Section.Value.RemoveAt(i);
							}
						}
					}
				}

				// Perform the removal of config section entries
				foreach (string ConfigSectionName in RemoveKeys)
				{
					AConfigFile.Sections.Remove(ConfigSectionName);
				}

				foreach (ConfigKeyValues Pair in LocalConfigOptions.ConfigKeyValuesToAdd)
				{
					if (AConfigFile.Sections.ContainsKey(Pair.SectionName))
					{
						List<BaseEntry> SectionEntries = AConfigFile.Sections[Pair.SectionName];
						string FinalVal = Pair.ValueString;
						if (FinalVal.Contains("$(GameName)"))
						{
							FinalVal = FinalVal.Replace("$(GameName)", CustomProjectShortName);
						}

						// If the keyname starts with a plus or minus we can just safely add it without looking through the list for an existing key
						if (Pair.KeyName.StartsWith("+") || Pair.KeyName.StartsWith("-"))
						{
							SectionEntries.Insert(0, new ConfigEntry(Pair.KeyName, FinalVal));
						}
						else
						{
							bool bNeedToAdd = true;
							// We loop through the entries and try to find a matching key, If we find it we replace the current value.  Otherwise we add a new entry
							for (int i = 0; i < SectionEntries.Count; i++)
							{
								if (SectionEntries[i].GetType() == typeof(ConfigEntry))
								{
									ConfigEntry Entry = (ConfigEntry)SectionEntries[i];
									if (Entry.Key == Pair.KeyName)
									{
										Entry.Value = FinalVal;
										bNeedToAdd = false;
									}
								}
							}

							if (bNeedToAdd)
							{
								SectionEntries.Insert(0, new ConfigEntry(Pair.KeyName, FinalVal));
							}
						}
					}
				}
				AConfigFile.SaveConfigFile(TargetFilePath);
			}
		}

	}
}
