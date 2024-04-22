/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Diagnostics;

using Color = System.Drawing.Color;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for ProfileEditor.xaml
	/// </summary>
	public partial class ProfileEditor : UserControl, System.ComponentModel.INotifyPropertyChanged
	{
		public ProfileEditor()
		{
			InitializeComponent();

			this.Loaded += ProfileEditor_Loaded;			
		}

		void ProfileEditor_Loaded(object sender, RoutedEventArgs e)
		{
			System.Windows.Input.Keyboard.Focus(this.mOpenConfigPopupButton);
		}

		public void OnClosing()
		{
			mAddMapsPopup.OnShuttingDown();
		}

		void OnConfigSummaryClicked(object sender, RoutedEventArgs e)
		{
			mConfigDialog.Profile = this.Profile;
			mConfigDialog.Visibility = Visibility.Visible;
			mConfigDialog.OnSummoned();
		}

		void OnApplicationSettingsClicked(object sender, RoutedEventArgs e)
		{
			// Run IPhonePackager to configure the mobile application settings.
			String IPhonePackagePath = Path.GetFullPath( Path.Combine("IPhone", "IPhonePackager.exe") );

			String MobileConfigString = Pipeline.PackageIOS.GetMobileConfigurationString(Profile.LaunchConfiguration);
			ProcessStartInfo ProcInfo = new ProcessStartInfo(IPhonePackagePath, string.Format("gui {0} {1}", Profile.SelectedGameName, MobileConfigString));
			ProcInfo.CreateNoWindow = false;
			ProcInfo.WorkingDirectory = Path.GetDirectoryName(IPhonePackagePath);
			ProcInfo.UseShellExecute = true;
			ProcInfo.Verb = "open";

			Session.Current.SessionLog.AddLine(Color.Green, String.Format("Spawning: {0} {1}", IPhonePackagePath, ProcInfo.Arguments));
			System.Diagnostics.Process.Start(ProcInfo).Dispose();
		}

		void OnMacApplicationSettingsClicked(object sender, RoutedEventArgs e)
		{
			// Run MacPackager to configure the application settings.
			String MacPackagePath = Path.GetFullPath(Path.Combine("Mac", "MacPackager.exe"));

			String MacConfigString = Pipeline.PackageMac.GetMacConfigurationString(Profile.LaunchConfiguration);
			string GUIMode = (Profile.Mac_PackagingMode == Pipeline.EMacPackageMode.Normal) ? "gui" : "guimas";
			ProcessStartInfo ProcInfo = new ProcessStartInfo(MacPackagePath, string.Format("{0} {1} {2}", GUIMode, Profile.SelectedGameName, MacConfigString));
			ProcInfo.CreateNoWindow = false;
			ProcInfo.WorkingDirectory = Path.GetDirectoryName(MacPackagePath);
			ProcInfo.UseShellExecute = true;
			ProcInfo.Verb = "open";

			Session.Current.SessionLog.AddLine(Color.Green, String.Format("Spawning: {0} {1}", MacPackagePath, ProcInfo.Arguments));
			System.Diagnostics.Process.Start(ProcInfo).Dispose();
		}

		/// Handler for URL clicks.
		void OnRequestNavigate(object Sender, System.Windows.Navigation.RequestNavigateEventArgs e)
		{
			String NavigateToUrl = e.Uri.ToString();
			System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(NavigateToUrl));
			e.Handled = true;
		}

		void OnAddMapsToCook(object sender, RoutedEventArgs e)
		{
			mAddMapsPopup.Profile = this.Profile;
			mAddMapsPopup.Visibility = Visibility.Visible;
			mAddMapsPopup.OnSummoned();
		}

		/// Shortcuts for the maps list
		void OnMapsListKeyDown(object sender, System.Windows.Input.KeyEventArgs e)
		{
			if (e.Key == System.Windows.Input.Key.Delete)
			{
				OnRemoveSelectedMaps(sender, e);
				e.Handled = true;
			}
		}

		void OnRemoveSelectedMaps(object sender, RoutedEventArgs e)
		{
			List<UnrealMap> SelectedMaps = new List<Object>((IEnumerable<object>)mMapsList.SelectedItems).ConvertAll<UnrealMap>(SomeItem => (UnrealMap)SomeItem);
			if (SelectedMaps.Count > 0)
			{
				foreach (UnrealMap SomeMapName in SelectedMaps)
				{
					Profile.Cooking_MapsToCook.Remove(SomeMapName);
				}

				Profile.Validate();
			}

			// Hack: Force the binding to update.
			mMapToPlayCombo.GetBindingExpression(ComboBox.SelectedItemProperty).UpdateTarget();
		}

		void OnRefreshTargets(object sender, RoutedEventArgs e)
		{
			Profile.TargetsList.QueueUpdateTargets( true );
		}


		public static void OnProfileChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
		{
			ProfileEditor This = (ProfileEditor)d;
			
			// Hack: Force the binding to update.
			This.mMapToPlayCombo.GetBindingExpression(ComboBox.SelectedItemProperty).UpdateTarget();

			This.Profile.TargetsList.QueueUpdateTargets(false);

			// Close both popups when we switch profiles.
			This.mAddMapsPopup.Visibility = Visibility.Collapsed;
			This.mConfigDialog.OnCancel();
		}

		/// The Profile we are currently editing
		public Profile Profile
		{
			get { return (Profile)GetValue(ProfileProperty); }
			set { SetValue(ProfileProperty, value); }
		}
		public static readonly DependencyProperty ProfileProperty =
			DependencyProperty.Register("Profile", typeof(Profile), typeof(ProfileEditor), new UIPropertyMetadata(null, OnProfileChanged));






		#region INotifyPropertyChanged

		public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
		private void NotifyPropertyChanged(String PropertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new System.ComponentModel.PropertyChangedEventArgs(PropertyName));
			}
		}

		#endregion

	}
}
