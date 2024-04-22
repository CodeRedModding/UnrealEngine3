/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Windows;
using System.Xml.Serialization;
using ConsoleInterface;
using System.Linq;
using StringBuilder = System.Text.StringBuilder;
using System.IO;
using UnrealFrontend.Pipeline;

namespace UnrealFrontend
{
	/// A profile is a collection of all the settings pertaining to getting a game from a PC to a target device.
	public class Profile : DependencyObject, System.ComponentModel.INotifyPropertyChanged
	{
		public enum Configuration
		{
			Debug_32 = 0,
			Debug_64,
			Release_32,
			Release_64,
			Shipping_32,
			Shipping_64,
			Test_32,
			Test_64,
			DebugScript,
			ReleaseScript,
			FinalReleaseScript
		}

		public enum Languages
		{
			OnlyDefault,
			OnlyNonDefault,
			All
		}

		/// Create a new profile called InName; all the settings come from InProfileData.
		public Profile(String InName, ProfileData InProfileData)
		{
			Name = InName;
			mData = InProfileData;

			TargetsList = new TargetsList(this);
			if (mData != null)
			{
				foreach(String ActiveTargetName in mData.mActiveTargets)
				{
					TargetsList.Targets.Add(new Target( ActiveTargetName, true ));
				}
			}
		
			
			Validate();
		}

		/// The actual profile data is separated into a different class for easy serialization.
		ProfileData mData = null;
		public ProfileData Data { get { return mData; } }

		/// The list of targets is transient data; it is not serialized.
		public TargetsList TargetsList {get; private set;}

		/// URL for the game-specific cooking help
		public String CookingHelpUrl { get { return Session.Current.StudioSettings.GameSpecificCookerOptionsHelpUrls[this.SelectedGameName]; } }
		/// The which appears as the link to game-specific cooking help. E.g.: "GearGame Help"
		public String CookingHelpString { get { return (CookingHelpUrl.Length > 0) ? String.Format("{0} help", this.SelectedGameName) : ""; } }

		/// <summary>
		/// Called when the profile is about to be serialized.
		/// This is an opportunity to update any saved fields
		/// that depend on transient data.
		/// </summary>
		public void OnAboutToSave()
		{
			// Update active targets. We cannot just save the targets, because
			// they fluctuate during the runtime. We just save the names
			// of the ones that the user likes.
			this.TargetsList.SaveActiveTargets();
		}

		/// Ensure that the state of the profile is consistent with itself.
		public void Validate()
		{
			Validate_Internal();
			ValidateMaps_Internal();
		}

		/// The name of this profile
		public String Name
		{
			get { return (String)GetValue(NameProperty); }
			set { SetValue(NameProperty, value); }
		}
		public static readonly DependencyProperty NameProperty =
			DependencyProperty.Register("Name", typeof(String), typeof(Profile), new UIPropertyMetadata("UnnamedProfile"));

		/// Filename used to save this profile to disk.
		public String Filename { get { return Name + ".xml"; } }

		public bool SupportsNetworkFileLoader { get { return TargetPlatformType == PlatformType.IPhone; } }

        public bool NeedsTextureFormat { get { return TargetPlatformType == PlatformType.Android && Android_TextureFilter == EAndroidTextureFilter.NONE; } }

        public bool IsAndroidProfile { get { return TargetPlatformType == PlatformType.Android; } }

        public bool IsAndroidDistribution { get { return TargetPlatformType == PlatformType.Android && (Android_PackagingMode == EAndroidPackageMode.GoogleDistribution || Android_PackagingMode == EAndroidPackageMode.AmazonDistribution); } }

        public bool IsShipping { get { return LaunchConfiguration == Configuration.Shipping_32 || LaunchConfiguration == Configuration.Shipping_64; } }

		public bool IsMacOSXProfile { get { return TargetPlatformType == PlatformType.MacOSX; } }

		/// A list of steps that UFE should execute to get the game from the PC to the target device(s).
		public Pipeline.UFEPipeline Pipeline
		{
			get { return mData.mPipeline; }
			set
			{
				if (mData.mPipeline != value)
				{
					mData.mPipeline = value;
					NotifyPropertyChanged("Pipeline");
				}
			}
		}

		/// True when the profile is being renamed.
		private bool mIsProfileBeingRenamed = false;
		public bool IsProfileBeingRenamed
		{
			get { return mIsProfileBeingRenamed; }
			set { AssignBool("IsProfileBeingRenamed", ref mIsProfileBeingRenamed, value); }
		}


		/// Wrap the mData members and add support for INotifyPropertyChanged so that
		/// we can bind to these values from WPF. See ProfileData.
		#region mData wrapper
		public List<String> ActiveTargets
		{
			get{ return mData.mActiveTargets; }
			set{ mData.mActiveTargets = value; }
		}

		public ObservableCollection<UnrealMap> Cooking_MapsToCook
		{
			get { return mData.mCooking_MapsToCook; }
			set
			{
				if (mData.mCooking_MapsToCook != value)
				{
					mData.mCooking_MapsToCook = value;
					ValidateMaps_Internal();
					NotifyPropertyChanged("Cooking_MapsToCook");
				}
			}
		}

		public String Cooking_MapsToCookAsString
		{
			get
			{
				StringBuilder MapNamesString = new StringBuilder();
				foreach (UnrealMap SomeMap in this.Cooking_MapsToCook)
				{
					MapNamesString.AppendFormat("{0} ", SomeMap.Name);
				}
				return MapNamesString.ToString();
			}
		}


		public String DLC_Name
		{
			get { return mData.mDLC_Name; }
			set
			{
				if (mData.mDLC_Name != value)
				{
					mData.mDLC_Name = value;
					NotifyPropertyChanged("DLC_Name");
				}
			}
		}
		public bool LaunchDefaultMap
		{
			get { return mData.mLaunchDefaultMap; }
			set { AssignBool("LaunchDefaultMap", ref mData.mLaunchDefaultMap, value); }
		}

		public UnrealMap MapToPlay
		{
			get { return mData.mMapToPlay; }
			set
			{
				if (mData.mMapToPlay != value)
				{
					mData.mMapToPlay = (value == null) ? UnrealMap.NoMap : value;
					ValidateMaps_Internal();
					NotifyPropertyChanged("MapToPlay");
					
				}
			}
		}	

		public String Launch_ExtraOptions
		{
			get { return mData.mLaunch_ExtraOptions; }
			set
			{
				if (mData.mLaunch_ExtraOptions != value)
				{
					mData.mLaunch_ExtraOptions = value;
					NotifyPropertyChanged("Launch_ExtraOptions");
				}
			}
		}

		public String Launch_Url
		{
			get { return mData.mLaunch_Url; }
			set
			{
				if (mData.mLaunch_Url != value)
				{
					mData.mLaunch_Url = value;
					NotifyPropertyChanged("Launch_Url");
				}
			}
		}

		public int Launch_UseUrl
		{
			get { return mData.mLaunch_UseUrl;  }
			set
			{
				if( mData.mLaunch_UseUrl != value )
				{
					mData.mLaunch_UseUrl = value;
					NotifyPropertyChanged("Launch_UseUrl");
				}
			}
		}
		
		public bool UseExecCommands
		{
			get { return mData.mUseExecCommands; }
			set { AssignBool("UseExecCommands", ref mData.mUseExecCommands, value); }
		}

		public bool UseMProfExe
		{
			get { return mData.mUseMProfExe; }
			set { AssignBool("UseMProfExe", ref mData.mUseMProfExe, value ); }
		}

		public String Launch_ExecCommands
		{
			get { return mData.mLaunch_ExecCommands; }
			set
			{
				if (mData.mLaunch_ExecCommands != value)
				{
					mData.mLaunch_ExecCommands = value;
					NotifyPropertyChanged("Launch_ExecCommands");
				}
			}
		}

		public String Cooking_AdditionalOptions
		{
			get { return mData.mCooking_AdditionalOptions; }
			set
			{
				if (mData.mCooking_AdditionalOptions != value)
				{
					mData.mCooking_AdditionalOptions = value;
					NotifyPropertyChanged("Cooking_AdditionalOptions");
				}
			}
		}

		public bool Cooking_UseFastCook
		{
			get { return mData.mCooking_UseFastCook; }
			set { AssignBool("Cooking_UseFastCook", ref mData.mCooking_UseFastCook, value); }
		}
		public static String DefaultDirectory
		{
			get
			{
				return ( UnrealControls.DefaultTargetDirectory.GetDefaultTargetDirectory() );
			}
		}

		public string Targets_ConsoleBaseDir
		{
			get { return mData.mTargets_ConsoleBaseDir; }
			set
			{
				if (mData.mTargets_ConsoleBaseDir != value)
				{
					mData.mTargets_ConsoleBaseDir = value;
					NotifyPropertyChanged("Targets_ConsoleBaseDir");
				}
			}
		}


		public bool IsCookDLCProfile
		{
			get { return mData.mIsDLCCookProfile; }
			set
			{
				if (mData.mIsDLCCookProfile != value)
				{
					mData.mIsDLCCookProfile = value;
					NotifyPropertyChanged("IsCookDLCProfile");
				}
			}
		}

		// Launch flags

		public bool Launch_NoVSync
		{
			get { return mData.mLaunch_NoVSync; }
			set { AssignBool("Launch_NoVSync", ref mData.mLaunch_NoVSync, value); }
		}

		public bool Launch_ClearUCWindow
		{
			get { return mData.mLaunch_ClearUCWindow; }
			set { AssignBool("Launch_ClearUCWindow", ref mData.mLaunch_ClearUCWindow, value); }
		}

		public bool Launch_CaptureFPSChartInfo
		{
			get { return mData.mLaunch_CaptureFPSChartInfo; }
			set { AssignBool("Launch_CaptureFPSChartInfo", ref mData.mLaunch_CaptureFPSChartInfo, value); }
		}

		public bool Mobile_UseNetworkFileLoader
		{
			get { return mData.mMobile_UseNetworkFileLoader; }
			set { AssignBool("Mobile_UseNetworkFileLoader", ref mData.mMobile_UseNetworkFileLoader, value); }
		}

		public bool Sync_CopyDebugInfo
		{
			get { return mData.mSync_CopyDebugInfo; }
			set { AssignBool("Sync_CopyDebugInfo", ref mData.mSync_CopyDebugInfo, value); }
		}

		public Pipeline.EPackageMode Mobile_PackagingMode
		{
			get { return mData.mMobile_PackagingMode; }
			set
			{
				if (mData.mMobile_PackagingMode != value)
				{
					mData.mMobile_PackagingMode = value;
					NotifyPropertyChanged("Mobile_PackagingMode");
				}
			}
		}
		
		private List<EPackageMode> mValidPackageModes = UnrealFrontend.Pipeline.PackageIOS.GetValidPackageModes();
		public List<EPackageMode> ValidPackageModes
		{
			get { return mValidPackageModes; }
			set
			{
				if (mValidPackageModes != value)
				{
					mValidPackageModes = value;
					NotifyPropertyChanged("ValidPackageModes");
				}
			}
		}

        public bool Android_SkipDownloader
        {
            get { return mData.mAndroid_SkipDownloader && IsAndroidProfile && IsShipping; }
            set
            {
                if (mData.mAndroid_SkipDownloader != value)
                {
                    mData.mAndroid_SkipDownloader = value;
                    NotifyPropertyChanged("Android_SkipDownloader");
                }
            }
        }

        public Pipeline.EAndroidPackageMode Android_PackagingMode
        {
            get { return mData.mAndroid_PackagingMode; }
            set
            {
                if (mData.mAndroid_PackagingMode != value)
                {
                    mData.mAndroid_PackagingMode = value;
                    NotifyPropertyChanged("Android_PackagingMode");
                    NotifyPropertyChanged("IsAndroidDistribution");
                }
            }
        }

        public Pipeline.EAndroidArchitecture Android_Architecture
        {
            get { return mData.mAndroid_Architecture; }
            set
            {
                if (mData.mAndroid_Architecture != value)
                {
                    mData.mAndroid_Architecture = value;
                    NotifyPropertyChanged("Android_Architecture");
                }
            }
        }

        public Pipeline.EAndroidTextureFilter Android_TextureFilter
        {
            get { return mData.mAndroid_TextureFilter; }
            set
            {
                if (mData.mAndroid_TextureFilter != value)
                {
                    mData.mAndroid_TextureFilter = value;
                    NotifyPropertyChanged("Android_TextureFilter");
					NotifyPropertyChanged("NeedsTextureFormat");
                }
            }
        }

        private List<EAndroidPackageMode> mValidAndroidPackageModes = UnrealFrontend.Pipeline.SyncAndroid.GetValidPackageModes();
        public List<EAndroidPackageMode> ValidAndroidPackageModes
        {
            get { return mValidAndroidPackageModes; }
            set
            {
                if (mValidAndroidPackageModes != value)
                {
                    mValidAndroidPackageModes = value;
                    NotifyPropertyChanged("ValidAndroidPackageModes");
                }
            }
        }

        private List<EAndroidArchitecture> mValidAndroidArchitectures = UnrealFrontend.Pipeline.SyncAndroid.GetValidArchitectures();
        public List<EAndroidArchitecture> ValidAndroidArchitectures
        {
            get { return mValidAndroidArchitectures; }
            set
            {
                if (mValidAndroidArchitectures != value)
                {
                    mValidAndroidArchitectures = value;
                    NotifyPropertyChanged("ValidAndroidArchitectures");
                }
            }
        }

        private List<EAndroidTextureFilter> mValidAndroidTextureFilters = UnrealFrontend.Pipeline.SyncAndroid.GetValidTextureFilters();
        public List<EAndroidTextureFilter> ValidAndroidTextureFilters
        {
            get { return mValidAndroidTextureFilters; }
            set
            {
                if (mValidAndroidTextureFilters != value)
                {
                    mValidAndroidTextureFilters = value;
                    NotifyPropertyChanged("ValidAndroidTextureFilters");
                }
            }
        }

		public Pipeline.EMacPackageMode Mac_PackagingMode
		{
			get { return mData.mMac_PackagingMode; }
			set
			{
				if (mData.mMac_PackagingMode != value)
				{
					mData.mMac_PackagingMode = value;
					NotifyPropertyChanged("Mac_PackagingMode");
				}
			}
		}

		private List<EMacPackageMode> mValidMacPackageModes = UnrealFrontend.Pipeline.PackageMac.GetValidPackageModes();
		public List<EMacPackageMode> ValidMacPackageModes
		{
			get { return mValidMacPackageModes; }
			set
			{
				if (mValidMacPackageModes != value)
				{
					mValidMacPackageModes = value;
					NotifyPropertyChanged("ValidMacPackageModes");
				}
			}
		}

		public bool Cooking_UseSeekFreeData
		{
			get { return mData.mCooking_UseSeekFreeData; }
			//set { AssignBool("Cooking_UseSeekFreeData", ref mData.mCooking_UseSeekFreeData, value); }
		}	

		// Targets flags

		public SerializeWrapper<ObservableCollection<LangOption>> Cooking_LanguagesToCookAndSync
		{
			get { return mData.mCooking_LanguagesToCookAndSync; }
			set
			{
				if (mData.mCooking_LanguagesToCookAndSync != value)
				{
					mData.mCooking_LanguagesToCookAndSync = value;
					NotifyPropertyChanged("Cooking_LanguagesToCookAndSync");
				}
			}
		}

		public SerializeWrapper<ObservableCollection<LangOption>> Cooking_TextureFormat
		{
			get { return mData.mCooking_TextureFormat; }
			set
			{
				if (mData.mCooking_TextureFormat != value)
				{
					mData.mCooking_TextureFormat = value;
					NotifyPropertyChanged("Cooking_TextureFormat");
				}
			}
		}


		public Configuration LaunchConfiguration
		{
			get { return mData.mLaunchConfiguration; }
			set
			{
				if (mData.mLaunchConfiguration != value)
				{
					mData.mLaunchConfiguration = value;
					NotifyPropertyChanged("LaunchConfiguration");
                    if (TargetPlatformType == PlatformType.Android)
                    {
                        NotifyPropertyChanged("IsShipping");
                        NotifyPropertyChanged("Android_SkipDownloader");
                    }
				}
			}
		}
		
		public Configuration CommandletConfiguration
		{
			get { return mData.mCommandletConfiguration; }
			set
			{
				if (mData.mCommandletConfiguration != value)
				{
					mData.mCommandletConfiguration = value;
					NotifyPropertyChanged("CommandletConfiguration");
				}
			}
		}

		public Configuration ScriptConfiguration
		{
			get { return mData.mScriptConfiguration; }
			set
			{
				if (mData.mScriptConfiguration != value)
				{
					mData.mScriptConfiguration = value;
					NotifyPropertyChanged("ScriptConfiguration");
				}
			}
		}

		public string SelectedGameName
		{
			get { return mData.mSelectedGameName; }
			set
			{
				if (mData.mSelectedGameName != value)
				{
					mData.mSelectedGameName = value;
					ValidateMaps_Internal();
					NotifyPropertyChanged("SelectedGameName");
					NotifyPropertyChanged("CookingHelpUrl");
					NotifyPropertyChanged("CookingHelpString");
				}
			}
		}

		public bool IsNonPcProfile{ get { return this.TargetPlatformType != PlatformType.PC && this.TargetPlatformType != PlatformType.PCConsole && this.TargetPlatformType != PlatformType.PCServer; } }

		public bool IsIPhoneProfile { get { return this.TargetPlatformType == PlatformType.IPhone; } }

		public bool SupportsPDBCopy { get { return this.TargetPlatformType == PlatformType.Xbox360 || this.TargetPlatformType == PlatformType.PS3; } }

		public bool SupportsEncryptedSockets { get { return this.TargetPlatformType == PlatformType.Xbox360; } }

		public ConsoleInterface.PlatformType TargetPlatformType
		{
			get { return mData.mTargetPlatformType; }
			set
			{
				if (mData.mTargetPlatformType != value)
				{
					mData.mTargetPlatformType = value;
					Validate_Internal();
					NotifyPropertyChanged("TargetPlatformType");
					NotifyPropertyChanged("NeedsTextureFormat");
                    NotifyPropertyChanged("Android_SkipDownloader");
                    NotifyPropertyChanged("IsAndroidDistribution");
					NotifyPropertyChanged("IsNonPcProfile");
					NotifyPropertyChanged("SupportsNetworkFileLoader");
					NotifyPropertyChanged("SupportsEncryptedSockets");
					NotifyPropertyChanged("IsIPhoneProfile");
					NotifyPropertyChanged("SupportsPDBCopy");
					NotifyPropertyChanged("IsMacOSXProfile");
                    NotifyPropertyChanged("IsAndroidProfile");
				}
			}
		}

		public ConsoleInterface.Platform TargetPlatform
		{			
			get
			{
				ConsoleInterface.Platform OutTargetPlatform = null;
				ConsoleInterface.DLLInterface.TryGetPlatform( this.TargetPlatformType, ref OutTargetPlatform ) ;
				return OutTargetPlatform;
			}
		}

		#endregion

		
		/// Reconcile possible contradictions in the state of the profile.
		private void Validate_Internal()
		{
			// Game
			if( !Session.Current.KnownGames.Contains( this.SelectedGameName ) && Session.Current.KnownGames.Count > 0 )
			{
				this.SelectedGameName = Session.Current.KnownGames[0];
			}
		
			// Validate target directory
			if (this.Targets_ConsoleBaseDir.Trim().Length == 0 || FileUtils.ContainsInvalidPathCharacter(Targets_ConsoleBaseDir) )
			{
				this.Targets_ConsoleBaseDir = DefaultDirectory;
			}

			// Ensure that the TargetPlatform is supported
			if ( !Session.Current.KnownPlatformTypes.Contains( this.TargetPlatformType ) )
			{
				if( Session.Current.KnownPlatformTypes.Count > 0 )
				{
					this.TargetPlatformType = Session.Current.KnownPlatformTypes[0];
				}
			}

			// Validate configurations (as we may load invalid ones from an old session)
			UnrealFrontend.ConfigManager.PlatformConfigs TargetPlatformConfigs = ConfigManager.ConfigsFor(this.TargetPlatformType);
			List<Profile.Configuration> LaunchConfig_ValidOptions = TargetPlatformConfigs.LaunchConfigs.GetConfigsFor(Session.Current.UDKMode);
			List<Profile.Configuration> CommandletConfig_ValidOptions = TargetPlatformConfigs.CommandletConfigs.GetConfigsFor(Session.Current.UDKMode);
			List<Profile.Configuration> ScriptConfig_ValidOptions = TargetPlatformConfigs.ScriptConfigs.GetConfigsFor(Session.Current.UDKMode);

			if (!LaunchConfig_ValidOptions.Contains(LaunchConfiguration))
			{
				LaunchConfiguration = LaunchConfig_ValidOptions[0];
			}

			if (!CommandletConfig_ValidOptions.Contains(CommandletConfiguration))
			{
				CommandletConfiguration = CommandletConfig_ValidOptions[0];
			}

			if (!ScriptConfig_ValidOptions.Contains(ScriptConfiguration))
			{
				ScriptConfiguration = ScriptConfig_ValidOptions[0];
			}

			// Validate mobile package mode
			ValidPackageModes = UnrealFrontend.Pipeline.PackageIOS.GetValidPackageModes();
			if ( !ValidPackageModes.Contains(this.Mobile_PackagingMode) )
			{
				Mobile_PackagingMode = ValidPackageModes[0];
			}

			// Validate Pipeline
			if (this.Pipeline.TargetPlatformType != this.TargetPlatformType)
			{
				this.Pipeline = UnrealFrontend.Pipeline.UFEPipeline.GetPipelineFor(this);
			}
			
			// If this pipeline was just deserialized...
			if (this.Pipeline.OwnerProfile == null)
			{
				// ... make sure that it is not malformed
				this.Pipeline = UnrealFrontend.Pipeline.UFEPipeline.ValidatePipeline(this, this.Pipeline);
			}
		
		}

		/// Reconcile contradictions specific to maps to cook/play.
		private void ValidateMaps_Internal()
		{

			// Validate Maps to cook
			List<UnrealMap> UniqueMapsToCook = new List<UnrealMap>(Enumerable.Distinct<UnrealMap>(Cooking_MapsToCook, UnrealMapComparer.SharedInstance));
			if (UniqueMapsToCook.Count != Cooking_MapsToCook.Count)
			{
				// Some duplicate maps were removed
				Cooking_MapsToCook = new ObservableCollection<UnrealMap>(UniqueMapsToCook);
			}

			// Validate map to play
			// We must have the exact object that is MapToPlay selected.
			int IndexOfMapToPlay = Cooking_MapsToCook.IndexOf(this.MapToPlay);
			if (IndexOfMapToPlay != -1)
			{
				this.MapToPlay = Cooking_MapsToCook[IndexOfMapToPlay];
			}
			else if( Cooking_MapsToCook.Count > 0 )
			{
				this.MapToPlay = Cooking_MapsToCook[0];
			}
			else
			{
				this.MapToPlay = UnrealMap.NoMap;
			}
			
		}


		#region INotifyPropertyChanged

		private void AssignBool(String InPropName, ref bool InOutProp, bool InNewValue)
		{
			if (InOutProp != InNewValue)
			{
				InOutProp = InNewValue;
				NotifyPropertyChanged(InPropName);
			}
		}

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

		/// Create a new Profile by deep-copying this profile's data.
		public Profile Clone()
		{
			Profile NewProfile = new Profile(this.Name, XmlUtils.CloneProfile(this.Data));
			// The TargetList is transient data that is not handled by the serialization-based cloning.
			NewProfile.TargetsList = this.TargetsList.Clone(NewProfile);
			return NewProfile;
		}
	}

	public class ProfileData
	{
		public ProfileData()
		{
			mCooking_LanguagesToCookAndSync = new SerializeWrapper<ObservableCollection<LangOption>>(new ObservableCollection<LangOption>
			{
				new LangOption{ Name = "INT", IsEnabled=true },
				new LangOption{ Name = "FRA"},
				new LangOption{ Name = "ITA"},
				new LangOption{ Name = "DEU"},
				new LangOption{ Name = "ESN"},
				new LangOption{ Name = "ESM"},
				new LangOption{ Name = "PTB"},
				new LangOption{ Name = "RUS"},
				new LangOption{ Name = "POL"},
				new LangOption{ Name = "HUN"},
				new LangOption{ Name = "CZE"},
				new LangOption{ Name = "SLO"},
				new LangOption{ Name = "JPN"},
				new LangOption{ Name = "KOR"},
				new LangOption{ Name = "CHN"}
			});

			mCooking_TextureFormat = new SerializeWrapper<ObservableCollection<LangOption>>(new ObservableCollection<LangOption>
			{
				new LangOption{ Name = "DXT", IsEnabled=true },
				new LangOption{ Name = "ATITC"},
				new LangOption{ Name = "PVRTC"},
                new LangOption{ Name = "ETC"}
			});

			mActiveTargets = new List<String>();
			mCooking_MapsToCook = new ObservableCollection<UnrealMap>();
			mPipeline = new Pipeline.UFEPipeline();
		}

		/// List of steps that UFE will take to get the game from the PC to a target device.
		public Pipeline.UFEPipeline mPipeline;

		/// List of device names that the user is targeting.
		public List<String> mActiveTargets = new List<String>();

		/// List of maps that will be cooked during the cook step.
		public ObservableCollection<UnrealMap> mCooking_MapsToCook;

		/// When true, use the MapToPlay.
		public bool mLaunchDefaultMap;

		/// Map to launch when the game starts.
		public UnrealMap mMapToPlay = UnrealMap.NoMap;

		/// Command line arguments to pass to the game when it is launched
		public String mLaunch_ExtraOptions = "";

		/// Url to use when launching the game
		public String mLaunch_Url = "";

		/// When set to 1, the user specified url will be used instead of the launch map option
		public int mLaunch_UseUrl = 0;

		/// When true, exec commands will be placed in a text file, and executed after Unreal starts.
		public bool mUseExecCommands;

		/// When true, use the MProf version of the game binary
		public bool mUseMProfExe;

		/// The exec commands to execute, when UseExecCommands == true.
		public String mLaunch_ExecCommands = "";

		/// When true, use the FASTCOOK option
		public bool mCooking_UseFastCook = false;

		/// Command line arguments to pass to the cooker
		public String mCooking_AdditionalOptions = "";

		/// Name of DLC to be cooked (Only valid when profile is a DLC cooking profile)
		public String mDLC_Name = "";

		/// Directory to which to copy on the console.
		public string mTargets_ConsoleBaseDir = Profile.DefaultDirectory;

		public bool mIsDLCCookProfile = false;
		// Launch flags

		/// Disable vsync when launching.
		public bool mLaunch_NoVSync = true;

		/// Clear the content of any active Unreal Console sessions when automatically connecting to them.
		public bool mLaunch_ClearUCWindow = false;

		/// Causes the launched game to gather FPS stats 
		public bool mLaunch_CaptureFPSChartInfo = false;

		/// When true, the game will load content from the PC via the network (mobile-specific)
		public bool mMobile_UseNetworkFileLoader = false;

		/// When true, pdb's required for symbol lookup are copied to targets.  (not supported on all platforms)
		public bool mSync_CopyDebugInfo = true;

		/// Packaging mode to use during the package step of the mobile pipeline (mobile-specific)
		public EPackageMode mMobile_PackagingMode = Pipeline.EPackageMode.FastIterationAndSign;

        /// Packaging mode to use during the package step of the Android pipeline (Android-specific)
        public EAndroidPackageMode mAndroid_PackagingMode = Pipeline.EAndroidPackageMode.Development;

        /// Whether or not to skip the downloader code to avoid having to upload an apk to the portal
        public bool mAndroid_SkipDownloader = true;

        // Architecture to determine which apk to deploy in the Android pipeline (Android-specific)
        public EAndroidArchitecture mAndroid_Architecture = Pipeline.EAndroidArchitecture.ALL;

        // Texture Filter to determine which apk to deploy in the Android pipeline (Android-specific)
        public EAndroidTextureFilter mAndroid_TextureFilter = Pipeline.EAndroidTextureFilter.NONE;

		/// Packaging mode to use during the package step of the Mac pipeline (Mac-specific)
		public EMacPackageMode mMac_PackagingMode = Pipeline.EMacPackageMode.Normal;

		// Cooking flags
		public bool mCooking_UseSeekFreeData = false;

		// Targets flags

		/// Copy debug info; this setting is currently ignored and the info is always copied.
		public bool mTargets_CopyFilesRequiredForSymbolLookup = true;

		public SerializeWrapper<ObservableCollection<LangOption>> mCooking_LanguagesToCookAndSync = null;

		/// Texture format to cook for Android
//		public String mCooking_TextureFormat = "";
		public SerializeWrapper<ObservableCollection<LangOption>> mCooking_TextureFormat = null;

		/// <summary>
		/// The Platform and Game and Code Configurations determine much about
		/// which options are available in the rest of the profile.
		/// </summary>
		#region Platform, Game, Code Config Settins

		public Profile.Configuration mLaunchConfiguration = Profile.Configuration.Release_32;

		public Profile.Configuration mCommandletConfiguration = Profile.Configuration.Release_32;

		public Profile.Configuration mScriptConfiguration = Profile.Configuration.ReleaseScript;

		public string mSelectedGameName = "";

		public ConsoleInterface.PlatformType mTargetPlatformType = ConsoleInterface.PlatformType.PC;

		#endregion

		
	}
}
