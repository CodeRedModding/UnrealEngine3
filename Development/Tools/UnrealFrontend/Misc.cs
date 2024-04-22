/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Xml.Serialization;
using System.Windows;
using System.Runtime.InteropServices;

using Regex = System.Text.RegularExpressions.Regex;

namespace UnrealFrontend
{
	/// Useful delegate type; most delegates in UFE are void.
	public delegate void VoidDelegate();

	/// C# serialization uses .Add() to insert value into a list during deserialization.
	/// This makes it difficult to initialize lists to default values.
	/// This class makes it easier to have default values for things.
	public class SerializeWrapper<T>
	{
		public SerializeWrapper()
		{ }

		public SerializeWrapper(T InContent)
		{
			Content = InContent;
		}

		public T Content { get; set; }
	}

	/// Currently just wraps a String.
	public class UnrealMap : IEquatable<UnrealMap>
	{
		private static readonly UnrealMap mNoMap = new UnrealMap();
		public static UnrealMap NoMap { get { return mNoMap; } }

		public UnrealMap():this(""){}
		public UnrealMap(String InName)
		{
			Name = InName;
		}

		public String Name{ get; set; }

		#region IEquatable<UnrealMap> Members

		bool IEquatable<UnrealMap>.Equals(UnrealMap other)
		{
			return this.Name.Equals(other.Name);
		}

		#endregion
	}

	/// Comparison function for maps; compares based on the map name.
	class UnrealMapComparer : IEqualityComparer<UnrealMap>
	{
		public readonly static UnrealMapComparer SharedInstance = new UnrealMapComparer();

		bool IEqualityComparer<UnrealMap>.Equals(UnrealMap x, UnrealMap y)
		{
			return x.Name.Equals(y.Name);
		}

		int IEqualityComparer<UnrealMap>.GetHashCode(UnrealMap obj)
		{
			return obj.Name.GetHashCode();
		}
	}

	/// Comparison function for files; Accepts to file paths, compares from filename only.
	public class FileComparer : IComparer<String>
	{
		public readonly static FileComparer SharedInstance = new FileComparer();

		public int Compare(String FilePath1, String FilePath2)
		{
			String File1 = System.IO.Path.GetFileName(FilePath1);
			String File2 = System.IO.Path.GetFileName(FilePath2);

			return File1.CompareTo(File2);
		}
	}

	/// Name of language paired with a bool; made for convenient binding.
	public class LangOption : System.ComponentModel.INotifyPropertyChanged
	{
		private String mName = "INT";
		public String Name
		{
			get { return mName; }
			set
			{
				if (mName != value)
				{
					mName = value;
					NotifyPropertyChanged("Name");
				}
			}
		}

		private bool mIsEnabled = false;
		public bool IsEnabled
		{
			get { return mIsEnabled; }
			set
			{
				if (mIsEnabled != value)
				{
					mIsEnabled = value;
					NotifyPropertyChanged("IsEnabled");
				}
			}
		}

		#region INotifyPropertyChanged

		public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
		private void NotifyPropertyChanged(String PropertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new System.ComponentModel.PropertyChangedEventArgs(PropertyName));

				Session.Current.SaveSessionSettings();
			}
		}

		#endregion
	}

	public static class ListUtils
	{
		/// <summary>
		/// Ensure that at least 1 item in the list is selected
		/// </summary>
		/// <param name="sender"> An object that generated this event </param>
		/// <param name="e"> The selection changed event arguments </param>
		public static void EnsureListSelection(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
		{
			System.Windows.Controls.ListView Offender = sender as System.Windows.Controls.ListView;
			if (Offender != null &&
				Offender.Items.Count > 0 &&
				-1 == Offender.SelectedIndex &&
				e.RemovedItems.Count > 0)
			{
				// The list is not empty.
				// Something was unselected and now the list has nothing selected.
				// Enforce selection if possible.
				if (Offender.Items.Contains(e.RemovedItems[0]))
				{
					Offender.SelectedItem = e.RemovedItems[0];
				}
				else
				{
					Offender.SelectedIndex = 0;
				}

			}
		}
	}

	/// Convenience function: e.g. PathUtils.Combine ("C:", "Path", "to", "Something")
	public static class PathUtils
	{
		public static String Combine( String PathSegmentA, String PathSegmentB, String PathSegmentC )
		{
			return Path.Combine(Path.Combine(PathSegmentA, PathSegmentB), PathSegmentC);
		}

		public static String Combine(String PathSegmentA, String PathSegmentB, String PathSegmentC, String PathSegmentD)
		{
			return Path.Combine( Path.Combine(Path.Combine(PathSegmentA, PathSegmentB), PathSegmentC), PathSegmentD );
		}
	}

	public class XmlUtils
	{
		/// <summary>
		/// Save a profile to the profiles location.
		/// </summary>
		/// <param name="InProfile"></param>
		/// <param name="NameToSaveAs"></param>
		/// <returns></returns>
		public bool WriteProfile( Profile InProfile, String NameToSaveAs )
		{
			String OutFileLocation = Settings.ProfileDirLocation;
			OutFileLocation = Path.Combine(OutFileLocation, NameToSaveAs);

			UnrealControls.XmlHandler.WriteXml<ProfileData>(InProfile.Data, OutFileLocation, "");
			return ( true );
		}

		/// <summary>
		/// Load a profile from a file given the absolute path to file.
		/// </summary>
		public Profile ReadProfile( String AbsoluteFilepathToProfile )
		{
			String ProfileName = Path.GetFileNameWithoutExtension(AbsoluteFilepathToProfile);
			ProfileData LoadedData = UnrealControls.XmlHandler.ReadXml<ProfileData>(AbsoluteFilepathToProfile);
			Profile RestoredProfile = new Profile(ProfileName, LoadedData);
			return RestoredProfile;
		}

		/// Load all profiles from a specified directory given an absolute path to the directory.
		public List<Profile> ReadProfiles(String ProfilesDir)
		{
			String LoadLocation = ProfilesDir;

			if (!Directory.Exists(LoadLocation))
			{
				Directory.CreateDirectory(LoadLocation);
			}

			List<Profile> LoadedProfiles = new List<Profile>();


			List<String> ProfileFiles = FileUtils.GetFilesInProfilesDirectory(LoadLocation);
			foreach(String SomeFile in ProfileFiles)
			{
				LoadedProfiles.Add( ReadProfile(SomeFile) );
			}

			return LoadedProfiles;
		}

		/// Return a deep copy of the profile data.
		public static ProfileData CloneProfile(ProfileData ProfileDataToClone)
		{
			try
			{
				using (MemoryStream CloneStream = new MemoryStream())
				{
					// Use a memory stream to serialize the profile data.
					XmlSerializer ProfileSerializer = new XmlSerializer(typeof(ProfileData));
					ProfileSerializer.Serialize(CloneStream, ProfileDataToClone);

					CloneStream.Seek(0, SeekOrigin.Begin);

					// Unmarshal the data (creates a copy) and return it.
					ProfileData NewProfileData = (ProfileData)ProfileSerializer.Deserialize(CloneStream);
					return NewProfileData;
				}		
			}
			catch(Exception e)
			{
				System.Diagnostics.Debug.WriteLine(e.ToString());
				return null;
			}

		}
	}

	public static class FileUtils
	{
		public static String MapExtensionsFromGameName(String InGameName)
		{
			String CustomMapExtension = "";

			if (Session.Current.StudioSettings.GameSpecificMapExtensions.TryGetValue(InGameName, out CustomMapExtension)
				&& CustomMapExtension.Trim() != "")
			{
				return Session.Current.StudioSettings.GameSpecificMapExtensions[InGameName];
			}
			else
			{
				return InGameName.ToLower().Replace("game", "");
			}
		}

		public static bool ContainsInvalidPathCharacter(String PathToTest)
		{
			return (-1 < PathToTest.IndexOfAny(System.IO.Path.GetInvalidPathChars()));
		}

		public static bool ContainsInvalidFilenameCharacter(String FilenameToTest)
		{
			return (-1 < FilenameToTest.IndexOfAny(System.IO.Path.GetInvalidFileNameChars()));
		}

		public static String StripInvalidFilenameSearchCharacters(String FilenameToTest)
		{
			String InvalidChars = new String(System.IO.Path.GetInvalidFileNameChars());
			InvalidChars = InvalidChars.Replace("*", "").Replace("?", "");

			return Regex.Replace(FilenameToTest, "[" + Regex.Escape(InvalidChars) + "]", "");
		}

		public static List<String> GetFilesInProfilesDirectory( String ProfilesDirectory )
		{
			String ProfilesLocation = ProfilesDirectory;

			if (!Directory.Exists(ProfilesLocation))
			{
				return new List<String>();
			}
			else
			{
				return new List<String>(Directory.GetFiles(ProfilesLocation));
			}
		}

		/// <summary>
		/// Is the proposed filename found in the list of ExistingFilenames
		/// </summary>
		public static bool IsFilenameUnique( ICollection<String> ExistingFilenames, String ProposedFilename )
		{
			foreach(String SomeFilename in ExistingFilenames)
			{
				if ( SomeFilename.Equals( ProposedFilename, StringComparison.InvariantCultureIgnoreCase ) )
				{
					return false;
				}
			}

			return true;
		}

		/// <summary>
		/// Takes a ProposedName for a profile, and returns a name that is guaranteed to be valid (valid filename and not a duplicate)
		/// </summary>
		public static String SuggestValidProfileName(String ProposedName)
		{
			String ValidatedName = ProposedName;
			// Enforce valid name for file on disk
			if (ContainsInvalidFilenameCharacter(ProposedName))
			{
				String InavlidCharsRegex = "[" + Regex.Escape( new String( System.IO.Path.GetInvalidFileNameChars() ) ) + "]";
				ValidatedName = Regex.Replace(ProposedName, InavlidCharsRegex, "");
			}			

			List<String> ExistingFilenames = Session.Current.Profiles.ToList().ConvertAll<String>(SomeProfile => SomeProfile.Name);
			
			// If this name is not unique, keep generating a name ending with an incremented number until we hit a unique one.
			if ( !FileUtils.IsFilenameUnique(ExistingFilenames, ProposedName) )
			{
				// Enforce no duplicates
				int CloneNumber = 0;
				String TempProposedName = String.Format("{0} - Copy", ProposedName);

				while (!FileUtils.IsFilenameUnique(ExistingFilenames, TempProposedName))
				{
					++CloneNumber;
					TempProposedName = String.Format("{0} - Copy({1})", ProposedName, CloneNumber);
				}
				ValidatedName = TempProposedName;
			}


			return ValidatedName;

		}

	}


	// Define other methods and classes here
	public static class Win32Helper_UFE
	{
		[DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.Winapi)]
		[return: MarshalAs(UnmanagedType.Bool)]
		public static extern bool IsWow64Process(
			 [In] IntPtr hProcess,
			 [Out] out bool wow64Process
			 );

		public static bool Is64Bit()
		{
			// If this passes, then it is a 64-bit app so must be 64-bit operating system...
			if (IntPtr.Size == 8)
			{
				return true;
			}

			bool bRetVal = false;
			if (IsWow64Process(System.Diagnostics.Process.GetCurrentProcess().Handle, out bRetVal) == false)
			{
				// If this fails, the function failed so we *assume* it's 32-bit
				bRetVal = false;
			}
			// Otherwise, bRetVal will be true for 32-bit running on 64-bit os
			return bRetVal;
		}
	}

	/// Tools for managing window flashing.
	public static class WindowUtils
	{
		[DllImport("user32.dll")]
		static extern bool FlashWindowEx(ref FLASHWINFO pWindowFlashInfo);

		[StructLayout(LayoutKind.Sequential)]
		public struct FLASHWINFO
		{
			public UInt32 cbSize;
			public IntPtr hwnd;
			public UInt32 dwFlags;
			public UInt32 uCount;
			public UInt32 dwTimeout;
		}

		private const UInt32 FLASHW_STOP = 0; 
		private const UInt32 FLASHW_ALL = 3;

		public static void FlashWindow_Stop( Window InWindowToFlash )
		{
			FlashWindow_Internal(InWindowToFlash, FLASHW_STOP, 0);
		}

		public static void FlashWindow_Begin( Window InWindowToFlash, int NumFlashes )
		{
			FlashWindow_Internal(InWindowToFlash, FLASHW_ALL, NumFlashes);
		}

		private static void FlashWindow_Internal( Window InWindowToFlash, UInt32 Action, int NumFlashes )
		{
			IntPtr HWND = new System.Windows.Interop.WindowInteropHelper(InWindowToFlash).Handle;
			FLASHWINFO pFlashWindowInfo = new FLASHWINFO()
			{
				cbSize = Convert.ToUInt32(Marshal.SizeOf( typeof(FLASHWINFO) )),
				hwnd = HWND,
				dwFlags = Action,
				uCount = (UInt32)NumFlashes,
				dwTimeout = 0,
			};

			FlashWindowEx(ref pFlashWindowInfo);
		}

	}

}
