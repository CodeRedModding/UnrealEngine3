/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using UnrealFrontend.Pipeline;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
	public partial class MainWindow : Window
	{
		public MainWindow()
		{
			// Parse the XAML and instantiate components (generated code).
			InitializeComponent();

			Session.Current.OnSessionStarting();

			this.UISettings = new UISettings();
			RestoreUISettings();

			mLogWindow.Document = Session.Current.OutpuDocument;

			mProfilesList.SelectionChanged += new SelectionChangedEventHandler(OnProfileSelectionChanged);
			mProfilesList.SelectedItem = Session.Current.Settings.FindProfileToSelectOnStartup(Session.Current.Profiles);
			mProfilesList.SelectedItem = Session.Current.OverrideProfile ?? mProfilesList.SelectedItem;
			mProfilesPanel.IsEnabled = null == Session.Current.OverrideProfile;

			this.Closed += new EventHandler(MainWindow_Closed);

			this.KeyDown += new KeyEventHandler(MainWindow_OnKeyDown);

			Session.Current.PropertyChanged += CurrentSession_PropertyChanged;
			this.Activated += new EventHandler(MainWindow_Activated);

			// Display location so multiple instances of UFE2 can be distinguished.
			this.Title = "Unreal Frontend - " + System.Windows.Forms.Application.StartupPath.Replace("\\Binaries", "");

			if (Session.Current.CommandLineArgs.Autostart && null != CurrentProfile)
			{
				EventWaitHandle completedEvent = null;
				if (Session.Current.CommandLineArgs.Autoquit)
				{
					completedEvent = Session.Current.QuitOnCompleteSignal = new ManualResetEvent(false); 
				}
				Session.Current.RunProfile(CurrentProfile, 0, ERunOptions.None, completedEvent);
			}
		}

		void MainWindow_Activated(object sender, EventArgs e)
		{
			WindowUtils.FlashWindow_Stop(this);
		}

		/// Flash when some non-trivial work got finished and UFE is not in foreground.
		void CurrentSession_PropertyChanged(object sender, System.ComponentModel.PropertyChangedEventArgs e)
		{
			if (e.PropertyName == "IsWorking")
			{
				if (!Session.Current.IsWorking)
				{
					// We just finished some work
					if (this.Topmost == false && Session.Current.LastTaskElapsedSeconds > 20)
					{
						// The window was in the background, and the task took some time.
						// Get the user's attention.
						WindowUtils.FlashWindow_Begin(this, 5);
					}

					if (null != Session.Current.QuitOnCompleteSignal && Session.Current.QuitOnCompleteSignal.WaitOne(0))
					{
						Close();
					}
				}
			}
		}

		/// Shutting down...
		void MainWindow_Closed(object sender, EventArgs e)
		{
			mProfileEditor.OnClosing();

			Session.Current.Settings.LastActiveProfile = CurrentProfile.Name;
			Session.Current.OnSessionClosing();
			SaveUISettings();
		}

		void MainWindow_OnKeyDown(object sender, KeyEventArgs e)
		{
			int StepIndex = 0;
			foreach( Pipeline.Step s in CurrentProfile.Pipeline.Steps )
			{
				if( s.KeyBinding == e.Key )
				{
					Session.Current.QueueExecuteStep(CurrentProfile, StepIndex, false);
					break;
				}
				++StepIndex;
			}
		}

		void RestoreUISettings()
		{
			XmlUtils XmlHelper = new XmlUtils();
			if ( System.IO.File.Exists(Settings.UISettingsFilename) )
			{
				UISettings = UnrealControls.XmlHandler.ReadXml<UISettings>(Settings.UISettingsFilename);
			}

			UISettings.Validate();

			this.Left = UISettings.WindowLeft;
			this.Top = UISettings.WindowTop;
			this.Width = UISettings.WindowWidth;
			this.Height = UISettings.WindowHeight;

			this.mProfilesColumn.Width = new System.Windows.GridLength( UISettings.ProfileListWidth, GridUnitType.Pixel );
			this.mProfilesRow.Height = new System.Windows.GridLength( UISettings.ProfilesSectionHeight, GridUnitType.Pixel );
		}

		void SaveUISettings()
		{
			UISettings.WindowLeft = this.Left;
			UISettings.WindowTop = this.Top;
			UISettings.WindowWidth = this.ActualWidth;
			UISettings.WindowHeight = this.ActualHeight;

			UISettings.ProfileListWidth = this.mProfilesColumn.Width.Value;
			UISettings.ProfilesSectionHeight = this.mProfilesRow.Height.Value;

			XmlUtils XmlHelper = new XmlUtils();
			UnrealControls.XmlHandler.WriteXml<UISettings>(this.UISettings, Settings.UISettingsFilename, "");
		}

		#region ProfileManagement

		void OnCloneProfile(object sender, RoutedEventArgs e)
		{
			// Create a clone of the profile.
			Profile ClonedProfile = CurrentProfile.Clone();
			ClonedProfile.Name = FileUtils.SuggestValidProfileName(CurrentProfile.Name);
			Session.Current.Profiles.Add(ClonedProfile);
			Session.Current.SaveSessionSettings();
		}

		void OnRenameProfile(object sender, RoutedEventArgs e)
		{
			this.CurrentProfile.IsProfileBeingRenamed = true;
		}

		void OnDeleteProfile(object sender, RoutedEventArgs e)
		{
			if (Session.Current.Profiles.Count > 1)
			{
                System.Windows.MessageBoxResult r = System.Windows.MessageBox.Show( "Are you sure you want to delete this profile?", "Deleting profile...", System.Windows.MessageBoxButton.YesNo );
                if ( r == System.Windows.MessageBoxResult.No )
                {
                    return;
                }
				Profile ProfileToDelete = this.CurrentProfile;
				Session.Current.Profiles.Remove(ProfileToDelete);
				System.IO.File.Delete(System.IO.Path.Combine(Settings.ProfileDirLocation, ProfileToDelete.Filename));
				Session.Current.SaveSessionSettings();
			}
		}

		void OnExportProfile(object sender, RoutedEventArgs e)
		{
			System.Windows.Forms.SaveFileDialog Dialog = new System.Windows.Forms.SaveFileDialog();
			Dialog.FileName = CurrentProfile.Name; 
			Dialog.DefaultExt = ".xml"; 
			Dialog.Filter = "Xml Files (.xml)|*.xml|All Files (*.*)|*.*";
			if (Dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
			{
				CurrentProfile.OnAboutToSave();
				XmlUtils XmlHelper = new XmlUtils();
				XmlHelper.WriteProfile(CurrentProfile, Dialog.FileName);		
			}
		}

		void OnImportProfile(object sender, RoutedEventArgs e)
		{
			System.Windows.Forms.OpenFileDialog Dialog = new System.Windows.Forms.OpenFileDialog();
			Dialog.FileName = "";
			Dialog.DefaultExt = ".xml";
			Dialog.Filter = "Xml Files (.xml)|*.xml|All Files (*.*)|*.*";
			if (Dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
			{
				XmlUtils XmlHelper = new XmlUtils();
				Profile LoadedProfile = XmlHelper.ReadProfile(Dialog.FileName);
				if (LoadedProfile != null)
				{
					LoadedProfile.Name = FileUtils.SuggestValidProfileName(LoadedProfile.Name);
					Session.Current.Profiles.Add(LoadedProfile);
					Session.Current.SaveSessionSettings();
				}
			}	
		}

		void OnDragDrop(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.FileDrop, false) == true && e.Effects.HasFlag(DragDropEffects.Move))
			{
				string[] FileList = (string[])e.Data.GetData(DataFormats.FileDrop);
				foreach (string FileName in FileList)
				{
					XmlUtils XmlHelper = new XmlUtils();
					Profile LoadedProfile = XmlHelper.ReadProfile(FileName);
					if (LoadedProfile != null)
					{
						LoadedProfile.Name = FileUtils.SuggestValidProfileName(LoadedProfile.Name);
						Session.Current.Profiles.Add(LoadedProfile);
					}
				}
				Session.Current.SaveSessionSettings();
			}
		}

		/// Shortcuts keys for the profiles list.
		void mProfilesList_KeyDown(object sender, System.Windows.Input.KeyEventArgs e)
		{
			if (e.Key == System.Windows.Input.Key.F2)
			{
				OnRenameProfile(sender, e);
				e.Handled = true;
			}
		}

		#endregion

		/// Called whenever profile list selection changes.
		void OnProfileSelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			// Ensure the list always has a profile selected, if at all possible.
			if (e.RemovedItems.Count > 0 && e.AddedItems.Count == 0)
			{
				Profile LastSelected = (Profile)e.RemovedItems[0];
				if (Session.Current.Profiles.Contains(LastSelected))
				{
					// Attempt to reselect the profile that was unselected
					mProfilesList.SelectedItem = LastSelected;
				}
				else if (Session.Current.Profiles.Count > 0)
				{
					// Attempt to select any valid profile
					mProfilesList.SelectedItem = Session.Current.Profiles[0];
				}
				e.Handled = true;
			}
			// We update the current profile here instead of using a binding
			// to ensure that it can never be set to null.
			CurrentProfile = (Profile)mProfilesList.SelectedItem;
			
		}

		/// The currently selected profile.
		public Profile CurrentProfile
		{
			get { return (Profile)GetValue(CurrentProfileProperty); }
			set { SetValue(CurrentProfileProperty, value); }
		}
		public static readonly DependencyProperty CurrentProfileProperty =
			DependencyProperty.Register("CurrentProfile", typeof(Profile), typeof(MainWindow), new UIPropertyMetadata(null));

		/// Windows size, position, splitters sizes, etc.
		public UISettings UISettings { get; set; }

		private void mProfilesList_MouseDown(object sender, MouseButtonEventArgs e)
		{
			if (e.LeftButton == MouseButtonState.Pressed)
			{
				ProfileListItem Selected = sender as ProfileListItem;
				string[] FileNames = new String[1];
				FileNames[0] = Settings.ProfileDirLocation + "\\" + Selected.TheProfile.Filename;

				DataObject data = new DataObject(DataFormats.FileDrop, FileNames);
				data.SetData(DataFormats.StringFormat, FileNames);
				DragDrop.DoDragDrop(this, data, DragDropEffects.Copy);
			}
		}
	}
}
