// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;

namespace P4PopulateDepot
{
	/// <summary>
	/// Class that contains a number of utility functions.
	/// </summary>
	public partial class Utils
	{
		public string MainModuleName = "";

		public Localise Phrases = null;
		Progress ProgressDlg = null;

		/// <summary>
		/// Initializes a new instance of the <see cref="Utils"/> class.
		/// </summary>
		/// <param name="Language">The language used for initializing loc system.</param>
		/// <param name="ParentLanguage">The parent language used for initializing loc system.</param>
		public Utils(string Language, string ParentLanguage)
		{
			MainModuleName = Application.ExecutablePath;

			// Init the localization system
			Phrases = new Localise();
			Phrases.Init(Language, ParentLanguage);
		}

		public string GetPhrase(string Phrase)
		{
			return (Phrases.GetPhrase(Phrase));
		}

		/*
		 * Game specific install info
		 */
		public class GameManifestOptions
		{
			// App to launch at the end of the install process to create ini files
			[XmlElementAttribute]
			public string AppToCreateInis { get; set; }

			// App to optionally launch after the install process
			[XmlElementAttribute]
			public string AppToElevate { get; set; }

			// Command line of the above app
			[XmlElementAttribute]
			public string AppCommandLine { get; set; }

			// Full descriptive name of the application
			[XmlElementAttribute]
			public string Name { get; set; }

			public GameManifestOptions()
			{
				AppToCreateInis = "Win32\\UDK.exe";
				AppToElevate = "UDKLift.exe";
				AppCommandLine = "editor";
				Name = "Unreal Development Kit";
			}
		}

		/*
		 * Details about the links to install
		 */
		public class LinkShortcutOptions
		{
			[XmlElementAttribute]
			public string DisplayPath { get; set; }

			[XmlElementAttribute]
			public string UrlFilePath { get; set; }

			[XmlElementAttribute]
			public string Name { get; set; }

			public LinkShortcutOptions()
			{
				DisplayPath = "";
				UrlFilePath = "";
				Name = "";
			}
		}

		/*
		 * Which files to include/exclude from the UDK/Game
		 */
		public class ManifestOptions
		{
			[CategoryAttribute("InstallInfo")]
			[DescriptionAttribute("The root name of the installer to be created. This will be the default install folder and the prefix to the package name.")]
			[XmlElementAttribute]
			public string RootName { get; set; }

			[CategoryAttribute("InstallInfo")]
			[DescriptionAttribute("The descriptive name to be used for the shortcuts folder, ARP display name and in more descriptive dialogs.")]
			[XmlElementAttribute]
			public string FullName { get; set; }

			[CategoryAttribute("InstallInfo")]
			[DescriptionAttribute("The application to launch on game install completion.")]
			[XmlElementAttribute]
			public string AppToLaunch { get; set; }

			[CategoryAttribute("InstallInfo")]
			[DescriptionAttribute("Whether to show the email subscription option on the install options page.")]
			[XmlElementAttribute]
			public bool ShowEmailSubscription { get; set; }

			[CategoryAttribute("InstallInfo")]
			[XmlElement]
			public List<LinkShortcutOptions> LinkShortcuts { get; set; }

			[CategoryAttribute("GameInstallInfo")]
			[XmlElement]
			public List<GameManifestOptions> GameInfo { get; set; }

			[CategoryAttribute("FileManifests")]
			[DescriptionAttribute("The files to exclude from original the build.")]
			[XmlArrayAttribute]
			public string[] MainFilesToExclude { get; set; }

			[CategoryAttribute("FileManifests")]
			[DescriptionAttribute("The files to EXCLUDE from the game build that were in the original build.")]
			[XmlArrayAttribute]
			public string[] GameFilesToExclude { get; set; }

			[CategoryAttribute("FileManifests")]
			[DescriptionAttribute("The folders that are required to exist for the game to package properly.")]
			[XmlArrayAttribute]
			public string[] RequiredFolders { get; set; }

			[CategoryAttribute("FileManifests")]
			[DescriptionAttribute("The files to INCLUDE for the game build.")]
			[XmlArrayAttribute]
			public string[] GameFilesToInclude { get; set; }

			[XmlElementAttribute]
			public bool bManifestCreated = false;

			public ManifestOptions()
			{
				RootName = "UDK";
				FullName = "Unreal Development Kit";
				AppToLaunch = "Win32\\UDK.exe";
				ShowEmailSubscription = true;

				GameInfo = new List<GameManifestOptions>() { };

				MainFilesToExclude = new string[] { };
				GameFilesToExclude = new string[] { };
				GameFilesToInclude = new string[] { };
			}
		}


		public class FileProperties
		{
			[XmlAttribute]
			public string FileName;
			[XmlAttribute]
			public long Size;

			public FileProperties()
			{
				FileName = "";
				Size = 0;
			}

			public FileProperties(string InName, long InSize)
			{
				FileName = InName;
				Size = InSize;
			}
		}

		public class FolderProperties
		{
			[XmlAttribute]
			public string FolderName;
			[XmlArray]
			public List<FolderProperties> Folders;
			[XmlArray]
			public List<FileProperties> Files;
			[XmlIgnore]
			public long Size;

			public FolderProperties()
			{
				FolderName = ".";
				Folders = new List<FolderProperties>();
				Files = new List<FileProperties>();
				Size = 0;
			}

			public FolderProperties(string InFolder)
			{
				FolderName = InFolder.TrimStart("\\".ToCharArray());
				Folders = new List<FolderProperties>();
				Files = new List<FileProperties>();
				Size = 0;
			}

			public void AddFolder(FolderProperties InFolder)
			{
				// Add the folder if it does not already exist.
				if (FindFolder(InFolder.FolderName) == null)
				{
					Folders.Add(InFolder);
				}
			}

			public void AddFile(FileProperties InFile)
			{
				// Add the file if it does not already exist.
				if (FindFile(InFile.FileName) == null)
				{
					Files.Add(InFile);
				}
				
			}

			// Find a folder in this folder's subfolders
			public FolderProperties FindFolder(string FolderName)
			{
				FolderProperties Folder = null;
				foreach(FolderProperties FolderPropterty in Folders)
				{
					if(string.Compare(FolderPropterty.FolderName, FolderName, true) == 0)
					{
						Folder = FolderPropterty;
					}
				}

				return (Folder);
			}

			// Find a file in this folder's files
			public FileProperties FindFile(string FileName)
			{
				FileProperties File = null;
				foreach(FileProperties FilePropterty in Files)
				{
					if(string.Compare(FilePropterty.FileName, FileName, true) == 0)
					{
						File = FilePropterty;
					}
				}

				return (File);
			}

			// Recursive function to return all files in FolderName and below
			public void FindAllFiles(string FolderName, Regex RX, List<string> AllowedBaseFolders)
			{
				DirectoryInfo DirInfo = new DirectoryInfo(FolderName);

				// The root of a drive contains a "." folder like all others, but unlike all the others, it's hidden
				bool bIsDirHidden = ((DirInfo.Attributes & FileAttributes.Hidden) == FileAttributes.Hidden) && (FolderName != ".");
				if(DirInfo.Exists && !bIsDirHidden)
				{
					foreach(DirectoryInfo Dir in DirInfo.GetDirectories())
					{
						if(AllowedBaseFolders == null || AllowedBaseFolders.Contains(Dir.Name.ToLower()))
						{
							if(FindFolder(Dir.Name) == null)
							{
								FolderProperties Folder = new FolderProperties(Dir.Name);
								AddFolder(Folder);

								Folder.FindAllFiles(FolderName + "\\" + Dir.Name, RX, null);
							}			
						}
					}

					if(AllowedBaseFolders == null || AllowedBaseFolders.Contains(DirInfo.Name.ToLower()))
					{
						foreach(FileInfo File in DirInfo.GetFiles())
						{
							bool bIsFileInvalid = (File.Attributes & FileAttributes.Hidden) == FileAttributes.Hidden;

							// Only add the file if it matches the regexp
							if(RX != null)
							{
								Match RegExpMatch = RX.Match(File.Name);
								// If it doesn't match, mark the file as invalid
								bIsFileInvalid |= !RegExpMatch.Success;
							}

							if(!bIsFileInvalid)
							{
								if (FindFile(File.Name) == null)
								{
									AddFile(new FileProperties(File.Name, File.Length));
								}
							}
						}
					}
				}
			}


			// Remove all the folders and files that have been marked for delete (size set to -1)
			public bool Clean()
			{
				List<FolderProperties> CleanFolders = new List<FolderProperties>();

				foreach(FolderProperties FP in Folders)
				{
					if(FP.Size >= 0)
					{
						if(FP.Clean())
						{
							CleanFolders.Add(FP);
						}
					}
				}

				List<FileProperties> CleanFiles = new List<FileProperties>();

				foreach(FileProperties FP in Files)
				{
					if(FP.Size >= 0)
					{
						CleanFiles.Add(FP);
					}
				}

				Folders = CleanFolders;
				Files = CleanFiles;

				if(Folders.Count == 0 && Files.Count == 0)
				{
					return (false);
				}

				return (true);
			}

			// Returns a count of the number of folders in the tree
			public int FolderCount()
			{
				int FolderCount = 1;

				foreach(FolderProperties Folder in Folders)
				{
					FolderCount += Folder.FolderCount();
				}

				return (FolderCount);
			}

			// Returns a count of the number of files in the tree
			public int FileCount()
			{
				int FileCount = Files.Count;

				foreach(FolderProperties Folder in Folders)
				{
					FileCount += Folder.FileCount();
				}

				return (FileCount);
			}
		}

		protected void XmlSerializer_UnknownAttribute(object sender, XmlAttributeEventArgs e)
		{
		}

		protected void XmlSerializer_UnknownNode(object sender, XmlNodeEventArgs e)
		{
		}

		private T ReadXml<T>(string FileName) where T : new()
		{
			T Instance = new T();
			try
			{
				using(Stream XmlStream = new FileStream(FileName, FileMode.Open, FileAccess.Read, FileShare.None))
				{
					// Creates an instance of the XmlSerializer class so we can read the settings object
					XmlSerializer ObjSer = new XmlSerializer(typeof(T));
					// Add our callbacks for a busted XML file
					ObjSer.UnknownNode += new XmlNodeEventHandler(XmlSerializer_UnknownNode);
					ObjSer.UnknownAttribute += new XmlAttributeEventHandler(XmlSerializer_UnknownAttribute);

					// Create an object graph from the XML data
					Instance = (T)ObjSer.Deserialize(XmlStream);
				}
			}
			catch(Exception E)
			{
				Console.WriteLine(E.Message);
			}

			return (Instance);
		}

		private bool WriteXml<T>(string FileName, T Instance)
		{
			// Make sure the file we're writing is actually writable
			FileInfo Info = new FileInfo(FileName);
			if(Info.Exists)
			{
				Info.IsReadOnly = false;
			}

			// Write out the xml stream
			Stream XmlStream = null;
			try
			{
				using(XmlStream = new FileStream(FileName, FileMode.Create, FileAccess.Write, FileShare.None))
				{
					XmlSerializer ObjSer = new XmlSerializer(typeof(T));

					// Add our callbacks for a busted XML file
					ObjSer.UnknownNode += new XmlNodeEventHandler(XmlSerializer_UnknownNode);
					ObjSer.UnknownAttribute += new XmlAttributeEventHandler(XmlSerializer_UnknownAttribute);

					ObjSer.Serialize(XmlStream, Instance);
				}
			}
			catch(Exception E)
			{
				Console.WriteLine(E.ToString());
				return (false);
			}

			return (true);
		}

		/// <summary>
		/// Converts a size in bytes to a more human readable format.
		/// </summary>
		/// <param name="bytes">The byte size to convert.</param>
		/// <returns>A more human readable format ex: 1.2GBytes, </returns>
		public string FormatBytes(long bytes)
		{
			string[] Suffix = { "Bytes", "KBytes", "MBytes", "GBytes" };
			int i;
			double dblSByte = 0;
			for(i = 0; (long)(bytes / 1024) > 0; i++, bytes /= 1024)
			{
				dblSByte = bytes / 1024.0;
			}
			return String.Format("{0:0.00} {1}", dblSByte, Suffix[i]);
		}

		/// <summary>
		/// Loads the project options.
		/// </summary>
		/// <param name="ManifestOptionsPath">The manifest options path.</param>
		public void LoadManifestOptions(string ManifestOptionsPath)
		{
			UDKSettings = ReadXml<ManifestOptions>(ManifestOptionsPath);
		}

		/// <summary>
		/// Loads the file manifest used to drive all file processing in this utility.  This function will also discover items EmptyProject items not found in the original UDK manifest.
		/// </summary>
		/// <param name="manifest">The manifest file path.</param>
		public void LoadManifestInfo(string manifest)
		{
			ManifestInfo = ReadXml<FolderProperties>(manifest);
			FilterOutMissingFiles(ManifestInfo);

			// Remove any files or folders marked for exclusion
			ManifestInfo.Clean();

			// Add files that may not be included like the manifest file itself
			FileInfo ManifestFileInfo = new FileInfo(manifest);
			string ManifestRelFilePath = manifest.Replace(GetProjectRoot(), string.Empty);
			ManifestInfo.AddFile(new FileProperties(ManifestRelFilePath, ManifestFileInfo.Length));

			// For custom projects, there are some files that do not show up in the original manifest.  We add those here.
			//@todo - This could introduce items that are not in the manifest and that do not belong to the empty project, if this is a problem we'll need to revisit.
			FolderProperties DevFolder = ManifestInfo.FindFolder("Development");
			FolderProperties SrcFolder = DevFolder != null ? DevFolder.FindFolder("Src") : null;

			if (SrcFolder != null)
			{
				SrcFolder.FindAllFiles(Path.Combine(GetProjectRoot(), "Development\\Src"), null, null);
			}

			FolderProperties GameFolder = ManifestInfo.FindFolder("UDKGame");
			FolderProperties ContentFolder = GameFolder != null ? GameFolder.FindFolder("Content") : null;

			if (ContentFolder != null)
			{
				FolderProperties MapsFolder = new FolderProperties("Maps");
				ContentFolder.AddFolder(MapsFolder);

				MapsFolder.FindAllFiles(Path.Combine(GetProjectRoot(), "UDKGame\\Content\\Maps"), null, null);
			}

			FolderProperties ScriptFolder = GameFolder != null ? GameFolder.FindFolder("Script") : null;

			if (ContentFolder != null)
			{
				ScriptFolder.FindAllFiles(Path.Combine(GetProjectRoot(), "UDKGame\\Script"), null, null);
			}
		}

		//  Used to store manifest info
		public FolderProperties ManifestInfo = null;
		//  Used to store settings for UDK
		public ManifestOptions UDKSettings = null;


		/// <summary>
		/// Function that will check to make sure all manifest entries exist on disk, if not we will mark them for removal in the data structure(size -1)..
		/// </summary>
		/// <param name="InFolderProperties">The in FolderProperties structure to check.</param>
		public void FilterOutMissingFiles(FolderProperties InFolderProperties)
		{
			FilterOutMissingFiles(GetProjectRoot(), ManifestInfo);
		}

		/// <summary>
		/// Recursive function that will check to make sure all manifest entries exist on disk, if not we will mark them for removal in the data structure(size -1).
		/// </summary>
		/// <param name="AncestorPath">Information about the parent hierarchy.</param>
		/// <param name="InFolderProperties">The FolderProperties structure to check.</param>
		public void FilterOutMissingFiles(string AncestorPath, FolderProperties InFolderProperties)
        {
            if (InFolderProperties.FolderName != ".")
            {
                AncestorPath = Path.Combine(AncestorPath, InFolderProperties.FolderName);
            }

            foreach(FileProperties FileToCheck in InFolderProperties.Files)
            {
				if (FileToCheck.Size != -1 && !File.Exists(Path.Combine(AncestorPath, FileToCheck.FileName)))
                {
					FileToCheck.Size = -1;
                }
            }

            foreach (FolderProperties Folder in InFolderProperties.Folders)
            {
				if (Folder.Size != -1 && !Directory.Exists(AncestorPath))
                {
					Folder.Size = -1;
                }

				if (Folder.Size != -1)
				{
					FilterOutMissingFiles(AncestorPath, Folder);
				}
            }
        }

		/// <summary>
		/// Function used to obtain a simple flattened list of files to process from ManifestInfo.
		/// </summary>
		/// <returns>List of file paths with no duplicate entries.</returns>
		public List<string> GetAllManifestFilePaths()
		{
			List<string> Files = new List<string>();
			GetAllFilePaths(GetProjectRoot(), ManifestInfo, Files);
			return Files;
		}


		/// <summary>
		/// Recursive function to get a flattened list of files from a FolderProperties instance.
		/// </summary>
		/// <param name="AncestorPath">Information about the parent hierarchy of the current folder.</param>
		/// <param name="InFolderProperties">The folder to process.</param>
		/// <param name="Result">Stores the resulting list of unique file paths.</param>
		public void GetAllFilePaths(string AncestorPath, FolderProperties InFolderProperties, List<string> Result)
		{
			if (InFolderProperties.FolderName != ".")
			{
				AncestorPath = Path.Combine(AncestorPath, InFolderProperties.FolderName);
			}

			foreach (FileProperties FileToCheck in InFolderProperties.Files)
			{
				if (FileToCheck.Size != -1)
				{
					string FinalFilePath = Path.Combine(AncestorPath, FileToCheck.FileName);
					if (!Result.Contains(FinalFilePath))
					{
						Result.Add(FinalFilePath);
					}	
				}
			}

			foreach (FolderProperties Folder in InFolderProperties.Folders)
			{
				if (Folder.Size != -1 )
				{
					GetAllFilePaths(AncestorPath, Folder, Result);
				}
			}
		}

		/// <summary>
		/// Recursive function to calculate the size of all content described in a FolderProperties instance.
		/// </summary>
		/// <param name="StartFolder">The FolderProperties instance to get the size for.</param>
		/// <returns>Total size in bytes.</returns>
		public long TotalSize(FolderProperties StartFolder)
		{
			long RetSize = 0;

			// Loop through all the files in this folder and accumulate their size.
			foreach (FileProperties FilePropterty in StartFolder.Files)
			{
				RetSize += FilePropterty.Size;
			}

			foreach (FolderProperties Folder in StartFolder.Folders)
			{
				RetSize += TotalSize(Folder);
			}
			
			return (RetSize);
		}


		static public void CenterFormToPrimaryMonitor(Form InForm)
		{
			Screen PrimaryScreen = Screen.PrimaryScreen;

			int NewX = (PrimaryScreen.Bounds.Left + PrimaryScreen.Bounds.Right - InForm.Width) / 2;
			int NewY = (PrimaryScreen.Bounds.Top + PrimaryScreen.Bounds.Bottom - InForm.Height) / 2;

			InForm.SetDesktopLocation(NewX, NewY);
		}

		public static bool Launch(string Executable, string WorkFolder, string CommandLine)
		{
			string FileName = Path.Combine(WorkFolder, "Binaries\\" + Executable);
			FileInfo ExecutableInfo = new FileInfo(FileName);

			if (ExecutableInfo.Exists)
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo();

				StartInfo.FileName = FileName;
				StartInfo.Arguments = CommandLine;
				StartInfo.WorkingDirectory = Path.Combine(WorkFolder, "Binaries\\");

				Process LaunchedProcess = Process.Start(StartInfo);
			}

			return (true);
		}


		/// <summary>
		/// Determines whether one folder path is a child of another folder path.  This function will traverse all the way to the drive root.
		/// </summary>
		/// <param name="InPath1">The expected child path.</param>
		/// <param name="InPath2">The expected ancestor path.</param>
		/// <returns>
		///   <c>true</c> if InPath1 is a child of InPath2; otherwise, <c>false</c>.
		/// </returns>
		public bool IsSubDirectory(DirectoryInfo InPath1, DirectoryInfo InPath2)
		{
			bool IsSubDir = false;
			DirectoryInfo LocalPath1 = InPath1;
			DirectoryInfo LocalPath2 = InPath2;
			while(LocalPath1.Parent != null)
			{
				if(LocalPath1.Parent.FullName.TrimEnd('\\').ToLower() == LocalPath2.FullName.TrimEnd('\\').ToLower())
				{
					IsSubDir = true;
					break;
				}
				LocalPath1 = LocalPath1.Parent;
			}
			return IsSubDir;
		}

		/// <summary>
		/// Gets the project root folder.
		/// </summary>
		/// <returns>Returns the root of the project folder that this utility resides in.</returns>
		public static string GetProjectRoot()
		{
			string ProjRoot = Path.Combine(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location), "..\\");
			ProjRoot = Path.GetFullPath(ProjRoot);
			return ProjRoot;
		}


		/// <summary>
		/// Creates a progress bar.
		/// </summary>
		/// <param name="Title">The title.</param>
		/// <param name="Heading">The heading.</param>
		/// <param name="LabelDescription">The label description.</param>
		/// <param name="LabelDetail">The label detail.</param>
		public void CreateProgressBar(string Title, string Heading, string LabelDescription, string LabelDetail)
		{
			ProgressDlg = new Progress();
			ProgressDlg.Text = Title;
			ProgressDlg.LabelHeading.Text = Heading;
			ProgressDlg.LabelDescription.Text = LabelDescription;
			ProgressDlg.LabelDetail.Text = LabelDetail;

			ProgressDlg.Show();
			Application.DoEvents();
		}

		delegate void UpdateProgressBarCallback(string LabelDescription, string LabelDetail);

		/// <summary>
		/// Updates the progress bar.
		/// </summary>
		/// <param name="LabelDescription">The label description.</param>
		/// <param name="LabelDetail">The label detail.</param>
		public void UpdateProgressBar( string LabelDescription, string LabelDetail)
		{
			if (ProgressDlg == null)
			{
				return;
			}

			// Check to see if this is being updated from another thread.
			if (ProgressDlg.LabelDescription.InvokeRequired || ProgressDlg.LabelDetail.InvokeRequired)
			{
				UpdateProgressBarCallback CB = new UpdateProgressBarCallback(UpdateProgressBar);
				ProgressDlg.Invoke(CB, new object[] { LabelDescription, LabelDetail });
			}
			else
			{
				if (!string.IsNullOrEmpty(LabelDescription))
				{
					ProgressDlg.LabelDescription.Text = LabelDescription;
				}
				if (!string.IsNullOrEmpty(LabelDetail))
				{
					ProgressDlg.LabelDetail.Text = LabelDetail;
				}
			}

			Application.DoEvents();
		}

		/// <summary>
		/// Destroys the progress bar.
		/// </summary>
		public void DestroyProgressBar()
		{
			ProgressDlg.Close();
		}

	}

}
