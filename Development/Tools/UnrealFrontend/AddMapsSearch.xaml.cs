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
using System.Collections.ObjectModel;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for AddMapsSearch.xaml
	/// </summary>
	public partial class AddMapsSearch : UserControl
	{
		public AddMapsSearch()
		{
			InitializeComponent();
			MapsAutocompleteList = new ObservableCollection<String>();
		}

		/// Maps that match the filter. (dependence property).
		public ObservableCollection<String> MapsAutocompleteList
		{
			get { return (ObservableCollection<String>)GetValue(MapsAutocompleteListProperty); }
			set { SetValue(MapsAutocompleteListProperty, value); }
		}
		public static readonly DependencyProperty MapsAutocompleteListProperty =
			DependencyProperty.Register("MapsAutocompleteList", typeof(ObservableCollection<String>), typeof(AddMapsSearch), new UIPropertyMetadata(null));

		/// The profile we are editing (dependence property).
		public Profile Profile
		{
			get { return (Profile)GetValue(ProfileProperty); }
			set { SetValue(ProfileProperty, value); }
		}
		public static readonly DependencyProperty ProfileProperty =
			DependencyProperty.Register("Profile", typeof(Profile), typeof(AddMapsSearch), new UIPropertyMetadata(null));

		
		/// A thread to asynchronously search for maps based on the specified filter.
		private WorkerThread AutocompleteMapsWorker = new WorkerThread("UFE2_AutocompleteMapsWorker");

		public void OnShuttingDown()
		{
			AutocompleteMapsWorker.BeginShutdown();
		}

		/// Invoked every time this dialog is opened.
		public void OnSummoned()
		{
			Keyboard.Focus(this);
			mFilterTextbox.Focusable = true;

			// HACK: Working around a WPF focus bug.
			// Queue up the focus change for the next WPF frame. Once our Visibility
			// has had time to propagate, we can actually focus the textbox.
			System.Windows.Threading.Dispatcher UIDispatcher = Application.Current.Dispatcher;
			UIDispatcher.BeginInvoke(new VoidDelegate(() =>
			{
		        Keyboard.Focus(mFilterTextbox);
		        mFilterTextbox.Text = "*";
		        mFilterTextbox.SelectAll();
		    }),
			System.Windows.Threading.DispatcherPriority.ApplicationIdle,
			null);
		}

		/// Support shortcuts: Enter - accept, Esc - cancel.
		void OnKeyDown(object sender, KeyEventArgs e)
		{
			if (e.Key == Key.Enter)
			{
				AddSelectedMaps();
				e.Handled = true;
			}
			else if (e.Key==Key.Escape)
			{
				Cancel();
				e.Handled = true;
			}
			
		}

		void OnMapDoubleClicked(object sender, MouseButtonEventArgs e)
		{
			AddSelectedMaps();
			e.Handled = true;
		}

		void OnAddSelectedMaps(object sender, RoutedEventArgs e)
		{
			AddSelectedMaps();
			e.Handled = true;
		}

		/// Import the filter strings from a file instead of the text box
		void OnImportMapsList(object sender, RoutedEventArgs e)
		{
			System.Windows.Forms.OpenFileDialog Dialog = new System.Windows.Forms.OpenFileDialog();
			Dialog.FileName = ""; 
			Dialog.DefaultExt = ".txt"; 
			Dialog.Filter = "Text Files (.txt)|*.txt|All Files (*.*)|*.*"; 
			if (Dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK) 
			{
				List<String> FileList = new List<String>();

				//parse the file...
				System.IO.Stream FileStream = Dialog.OpenFile();
				using (System.IO.StreamReader InStream = new System.IO.StreamReader(FileStream))
				{
					//add each line as a filter string
					string Text;
					while ((Text = InStream.ReadLine()) != null)
					{
						FileList.Add(Text);
					}
				}
				FileStream.Close();

				//put the filename in the filter box just for reference
				mFilterTextbox.Text = Dialog.SafeFileName;
				CreateFilteredMapNames(FileList);
			}
		}

		/// Add selected maps to the Profile's MapsToCook list.
		void AddSelectedMaps()
		{
			if (mMapsAutocompleteList.SelectedItems.Count == 0)
			{
				// Flush the worker to make sure we found all the maps.
				AutocompleteMapsWorker.Flush();
				mMapsAutocompleteList.SelectAll();
			}

			foreach (String MapName in mMapsAutocompleteList.SelectedItems)
			{
				Profile.Cooking_MapsToCook.Add(new UnrealMap(MapName));
			}

			mFilterTextbox.Clear();

			this.Visibility = Visibility.Collapsed;
			Profile.Validate();
		}


		void OnCancel(object sender, RoutedEventArgs e)
		{				
			Cancel();
			e.Handled = true;
		}

		void Cancel()
		{
			mFilterTextbox.Clear();
			this.Visibility = Visibility.Collapsed;
		}
		
		///Accepts a list filter strings and creates an acceptable map list
		void CreateFilteredMapNames(List<String> NamesToCheck)
		{
			Profile CurrentProfile = this.Profile;
			System.Windows.Threading.Dispatcher UIDispatcher = Application.Current.Dispatcher;
			ObservableCollection<String> DiscoveredMaps = this.MapsAutocompleteList;

			try
			{
				string GameContentDir = "..\\" + CurrentProfile.SelectedGameName + "\\Content";
				string EngineContentDir = "..\\Engine\\Content";

				// Add script directory when cooking for DLC
				string ScriptDir = "..\\" + CurrentProfile.SelectedGameName + "\\Script";

				List<String> Dirs = new List<String>();
				Dirs.Add(GameContentDir);
				Dirs.Add(EngineContentDir);
				if (CurrentProfile.IsCookDLCProfile)
				{
					Dirs.Add(ScriptDir);
				}

				List<String> SearchStrings = new List<String>();
				foreach (String FilterString in NamesToCheck)
				{
					// Append the extension of the current game's maps if an extension isn't already provided
					String Extensions = FileUtils.MapExtensionsFromGameName(Profile.SelectedGameName);
					if (!System.IO.Path.HasExtension(FilterString))
					{
						String[] ExtensionList = Extensions.Split(',');
						foreach (String Ext in ExtensionList)
						{
							SearchStrings.Add(FilterString + "." + Ext);
						}
					}
					else
					{
						SearchStrings.Add(FilterString);
					}
				}

				if (System.IO.Directory.Exists(GameContentDir))
				{
					AutocompleteMapsWorker.QueueWork(() =>
					{
						BuildCookMapEntriesString(DiscoveredMaps, UIDispatcher, Dirs, SearchStrings);
					});
				}
			}
			catch (Exception Error)
			{
				System.Diagnostics.Trace.WriteLine(Error.ToString());
			}
		}

		/// Called when text changes in the filter textbox.
		/// Triggers an update of the maps matching the filter.
		void OnAddMapsTextChanged(object sender, TextChangedEventArgs e)
		{
			// Ensure that the search string is a valid path.
			String FilterString = FileUtils.StripInvalidFilenameSearchCharacters(mFilterTextbox.Text);
			if (FilterString != mFilterTextbox.Text)
			{
				mFilterTextbox.Text = FilterString;
			}

			// Assume that the search string is a partial name unless it explicitly contains a *.
			FilterString = FilterString.Trim();
			if (FilterString.Length > 0 && !FilterString.Contains("*") && !FilterString.Contains("?"))
			{
				FilterString = String.Format("*{0}*", FilterString);
			}

			List<String> NamesToCheck = new List<String>();
			NamesToCheck.Add(FilterString);
			CreateFilteredMapNames(NamesToCheck);
         
		}


		/// <summary>
		/// Builds the list of maps to be cooked.
		/// </summary>
		/// <param name="EntryBldr">The <see cref="System.StringBuilder"/> containing the resulting list of maps.</param>
		/// <param name="Dir">The directory to search for files in.</param>
		/// <param name="SearchString">The string used to filter files in <see cref="Dir"/>.</param>
		void BuildCookMapEntriesString(ObservableCollection<String> DiscoveredMaps, System.Windows.Threading.Dispatcher UIDispatcher, List<String> Dirs, List<String> SearchStrings)
		{
			List<String> Files = new List<String>();
			foreach( String Dir in Dirs )
			{
				foreach( String SearchString in SearchStrings )
				{
					Files.AddRange( System.IO.Directory.GetFiles(Dir, SearchString, System.IO.SearchOption.AllDirectories) );
				}
			}
			Files.Sort(FileComparer.SharedInstance);

			const int MaxNumSuggestions = int.MaxValue;
			int NumFilesFound = Files.Count;
			int NumFilesToShow = Math.Min(MaxNumSuggestions, NumFilesFound);

			UIDispatcher.BeginInvoke(new VoidDelegate(() =>
			{
				DiscoveredMaps.Clear();
				mSummary.Text = String.Format("Found {0} files.", NumFilesToShow);
			}));

			for (int CurFileIndex = 0; CurFileIndex < NumFilesToShow; ++CurFileIndex)
			{
				String SomeFile = Files[CurFileIndex];
				if (SomeFile.IndexOf("Autosaves", StringComparison.OrdinalIgnoreCase) == -1)
				{
					String DiscoveredMap = System.IO.Path.GetFileName(SomeFile);
					UIDispatcher.BeginInvoke(new VoidDelegate(() => DiscoveredMaps.Add(DiscoveredMap)));
				}
			}
		}
	}
}
