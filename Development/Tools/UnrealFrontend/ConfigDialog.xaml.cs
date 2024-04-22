/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.ComponentModel;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for ConfigDialog.xaml
	/// </summary>
	public partial class ConfigDialog : UserControl, INotifyPropertyChanged
	{
		public ConfigDialog()
		{
			InitializeComponent();
		}

		private void OnCancel(object sender, RoutedEventArgs e)
		{
			OnCancel();
		}

		private void OnAccept(object sender, RoutedEventArgs e)
		{
			OnAccept();
		}

		void OnKeyDown(object sender, KeyEventArgs e)
		{
			if (e.Key == Key.Enter)
			{
				OnAccept();
			}
			else if (e.Key == Key.Escape)
			{
				OnCancel();
			}
		}

		/// Prevent lists from having no selection
		private void EnsureListSelection(object sender, SelectionChangedEventArgs e)
		{
			ListUtils.EnsureListSelection(sender, e);
		}

		public bool ShouldShowDLCCook
		{
			get 
			{
				return	TargetPlatformType == ConsoleInterface.PlatformType.Xbox360 
					||	TargetPlatformType == ConsoleInterface.PlatformType.PS3 
					||	TargetPlatformType == ConsoleInterface.PlatformType.PC 
					||	TargetPlatformType == ConsoleInterface.PlatformType.PCConsole;
			}
		}

		private bool mIsDLCCookConfig = false;
		public bool IsDLCCookConfig
		{
			get { return mIsDLCCookConfig; }
			set
			{
				if (mIsDLCCookConfig != value)
				{
					mIsDLCCookConfig = value;
					NotifyPropertyChanged("IsDLCCookConfig");
				}
			}
		}
		/// The profile that we are editing (dependency property)
		public Profile Profile
		{
			get { return (Profile)GetValue(ProfileProperty); }
			set { SetValue(ProfileProperty, value); }
		}
		public static readonly DependencyProperty ProfileProperty =
			DependencyProperty.Register("Profile", typeof(Profile), typeof(ConfigDialog), new UIPropertyMetadata(null));

		/// The user opened this dialog; copy the setting from current Profile for non-destructive editing.
		public void OnSummoned()
		{
			System.Windows.Input.Keyboard.Focus(mConfig_GameList);

			this.TargetPlatformType = Profile.TargetPlatformType;
			this.SelectedGameName = Profile.SelectedGameName;
			this.LaunchConfiguration = Profile.LaunchConfiguration;
			this.CommandletConfiguration = Profile.CommandletConfiguration;
			this.ScriptConfiguration = Profile.ScriptConfiguration;
			this.IsDLCCookConfig = Profile.IsCookDLCProfile;

			this.ValidateConfigurations();

			// HACK: Working around a WPF focus bug.
			// Queue up the focus change for the next WPF frame. Once our Visibility
			// has had time to propagate, we can actually focus the textbox.
			System.Windows.Threading.Dispatcher UIDispatcher = Application.Current.Dispatcher;
			UIDispatcher.BeginInvoke(new VoidDelegate(() =>
			{
				Keyboard.Focus(this.mConfig_GameList);
			}),
			System.Windows.Threading.DispatcherPriority.ApplicationIdle,
			null);
		}

		/// User accepted the change; commit all the configuration to the Profile we are editing.
		private void OnAccept()
		{
			if (Profile.SelectedGameName != this.SelectedGameName)
			{
				// If we changed the game there might be maps to cook from the previous game.
				// They no longer make sense, so clear them.
				Profile.Cooking_MapsToCook.Clear();
			}

			this.Visibility = Visibility.Collapsed;
			Profile.SelectedGameName = this.SelectedGameName;
			Profile.TargetPlatformType = this.TargetPlatformType;
			Profile.LaunchConfiguration = this.LaunchConfiguration;
			Profile.CommandletConfiguration = this.CommandletConfiguration;
			Profile.ScriptConfiguration = this.ScriptConfiguration;
			Profile.IsCookDLCProfile = this.IsDLCCookConfig && this.ShouldShowDLCCook;

			Profile.TargetsList.QueueUpdateTargets(true);
		}

		/// Dismiss dialog
		public void OnCancel()
		{
			this.Visibility = Visibility.Collapsed;
		}

		/// <summary>
		/// The Profile determines which options are available given its current state.
		/// For example, selecting a platform limits the available Config Settings.
		/// </summary>
		#region Valid Config Options

		private List<Profile.Configuration> mLaunchConfig_ValidOptions;
		public List<Profile.Configuration> LaunchConfig_ValidOptions
		{
			get { return mLaunchConfig_ValidOptions; }
			set
			{
				if (mLaunchConfig_ValidOptions != value)
				{
					mLaunchConfig_ValidOptions = value;
					NotifyPropertyChanged("LaunchConfig_ValidOptions");
				}
			}
		}

		private List<Profile.Configuration> mCommandletConfig_ValidOptions;
		public List<Profile.Configuration> CommandletConfig_ValidOptions
		{
			get { return mCommandletConfig_ValidOptions; }
			set
			{
				if (mCommandletConfig_ValidOptions != value)
				{
					mCommandletConfig_ValidOptions = value;
					NotifyPropertyChanged("CommandletConfig_ValidOptions");
				}
			}
		}

		private List<Profile.Configuration> mScriptConfig_ValidOptions;
		public List<Profile.Configuration> ScriptConfig_ValidOptions
		{
			get { return mScriptConfig_ValidOptions; }
			set
			{
				if (mScriptConfig_ValidOptions != value)
				{
					mScriptConfig_ValidOptions = value;
					NotifyPropertyChanged("ScriptConfig_ValidOptions");
				}
			}
		}

		#endregion


		/// <summary>
		/// The current configuration: Game, Platform, EXEs
		/// </summary>
		#region Current Configuration
		
		private String mSelectedGameName = "";
		public String SelectedGameName
		{
			get { return mSelectedGameName; }
			set
			{
				if (mSelectedGameName != value)
				{
					mSelectedGameName = value;
					ValidateConfigurations();
					NotifyPropertyChanged("SelectedGameName");
				}
			}
		}
		

		private ConsoleInterface.PlatformType mTargetPlatformType = ConsoleInterface.PlatformType.All;
		public ConsoleInterface.PlatformType TargetPlatformType
		{
			get { return mTargetPlatformType; }
			set
			{
				if (mTargetPlatformType != value)
				{
					mTargetPlatformType = value;
					ValidateConfigurations();
					NotifyPropertyChanged("TargetPlatformType");
					NotifyPropertyChanged("ShouldShowDLCCook");
				}
			}
		}


		private Profile.Configuration mLaunchConfiguration = Profile.Configuration.Release_32;
		public Profile.Configuration LaunchConfiguration
		{
			get { return mLaunchConfiguration; }
			set
			{
				if (mLaunchConfiguration != value)
				{
					mLaunchConfiguration = value;
					NotifyPropertyChanged("LaunchConfiguration");
				}
			}
		}

		private Profile.Configuration mCommandletConfiguration = Profile.Configuration.Release_32;
		public Profile.Configuration CommandletConfiguration
		{
			get { return mCommandletConfiguration; }
			set
			{
				if (mCommandletConfiguration != value)
				{
					mCommandletConfiguration = value;
					NotifyPropertyChanged("CommandletConfiguration");
				}
			}
		}

		private Profile.Configuration mScriptConfiguration = Profile.Configuration.ReleaseScript;
		public Profile.Configuration ScriptConfiguration
		{
			get { return mScriptConfiguration; }
			set
			{
				if (mScriptConfiguration != value)
				{
					mScriptConfiguration = value;
					NotifyPropertyChanged("ScriptConfiguration");
				}
			}
		}

		#endregion


		private void ValidateConfigurations()
		{
			// Ensure that each configuration is found in the list of valid configurations for the current
			// platform type.
			UnrealFrontend.ConfigManager.PlatformConfigs TargetPlatformConfigs = ConfigManager.ConfigsFor(this.TargetPlatformType);
			LaunchConfig_ValidOptions = TargetPlatformConfigs.LaunchConfigs.GetConfigsFor(Session.Current.UDKMode);
			CommandletConfig_ValidOptions = TargetPlatformConfigs.CommandletConfigs.GetConfigsFor(Session.Current.UDKMode);
			ScriptConfig_ValidOptions = TargetPlatformConfigs.ScriptConfigs.GetConfigsFor(Session.Current.UDKMode);

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
}
