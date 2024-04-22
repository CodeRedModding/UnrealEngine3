/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class InstallOptions : Form
	{
		[DllImport( "kernel32.dll", CharSet = CharSet.Auto )]
		[return: MarshalAs( UnmanagedType.Bool )]
		public static extern bool GetDiskFreeSpaceEx(
			string lpDirectoryName,
			out UInt64 lpFreeBytesAvailable,
			out UInt64 lpTotalNumberOfBytes,
			out UInt64 lpTotalNumberOfFreeBytes );

		private string StartMenuLocation = "";
		private bool bIsGame;
		private bool bHasUserChangedInstallLocation = false;
		private int PanelHeightWithOptions;
		private int PanelHeightNoOptions;

		public InstallOptions( bool IsGame )
		{
			bIsGame = IsGame;
			InitializeComponent();

			// Store off some panel height values
			PanelHeightWithOptions = this.Height;
			PanelHeightNoOptions = this.Height - 100;

			InstallOptionsFooterLineLabel.Text = Program.Util.UnSetupVersionString;
			InstallLocationGroupBox.Text = Program.Util.GetPhrase( "GBInstallLocation" );
			ChooseInstallLocationButton.Text = Program.Util.GetPhrase( "GBInstallLocationBrowse" );
			InstallButton.Text = Program.Util.GetPhrase( "ButInstall" );
			InstallCancelButton.Text = Program.Util.GetPhrase( "GQCancel" );
			EmailLabel.Text = Program.Util.GetPhrase( "LabEmail" );
			OptionalEmailLabel.Text = Program.Util.GetPhrase( "PrivacyStatement" );
			PrivacyPolicyTextBox.Text = Program.Util.GetPhrase( "PrivacyStatement2" );

			InvalidEmailLabel.Visible = false;
			InvalidEmailLabel.Text = Program.Util.GetPhrase( "IOInvalidEmail" );

			// Show the email subscription options if necessary
			EmailGroupBox.Visible = Program.Util.Manifest.ShowEmailSubscription;

			InvalidProjectNameLabel.Visible = false;
			InvalidProjectNameLabel.Text = Program.Util.GetPhrase( "IOInvalidProjectName" );
			ProjectNameLabel.Text = Program.Util.GetPhrase( "IOProjectNameLabel" );

			SetProjectSpecificValues();
		}

		private bool IsShowingExtraOptions()
		{
			bool RetVal = ProjectGroupBox.Visible;
			return RetVal;
		}

		private void ShowExtraOptions()
		{
			// Show the custom game specific settings
			ProjectGroupBox.Visible = true;
			ProjectNameLabel.Visible = true;
			ProjectNameTextBox.Visible = true;
			this.Height = PanelHeightWithOptions;
		}

		private void HideExtraOptions()
		{
			// Hide the custom game specific settings
			ProjectGroupBox.Visible = false;
			ProjectNameLabel.Visible = false;
			ProjectNameTextBox.Visible = false;
			InvalidProjectNameLabel.Visible = false;
			this.Height = PanelHeightNoOptions;
		}

		private void SetProjectSpecificValues()
		{
			// Initialize the UDK/Game specific UI components and startmenu locations
			if( !bIsGame )
			{
				if( Program.Util.bIsCustomProject && !IsShowingExtraOptions() )
				{
					ShowExtraOptions();
				}
				else if( !Program.Util.bIsCustomProject && IsShowingExtraOptions() )
				{
					HideExtraOptions();
				}

				// Show the back button
				InstallBackButton.Visible = true;

				Text = Program.Util.GetPhrase( "IOInstallOptionsGame" ) + Program.Util.Manifest.RootName;
				InstallOptionsTitleLabel.Text = Text;

				if( Program.Util.bIsCustomProject )
				{
					string InstallLocation = Program.Util.CustomProjectShortName;
					StartMenuLocation = Path.Combine( Program.Util.Manifest.FullName + "\\", InstallLocation );

					InstallLocationTextbox.Text = Path.Combine( "C:\\" + Program.Util.Manifest.RootName + "\\", InstallLocation );

					ProjectNameTextBox.Text = Program.Util.CustomProjectShortName;
				}
				else
				{
					string InstallLocation = Program.Util.Manifest.RootName + "-" + Program.Util.UnSetupTimeStamp;
					StartMenuLocation = Path.Combine( Program.Util.Manifest.FullName + "\\", InstallLocation );

					InstallLocationTextbox.Text = Path.Combine( "C:\\" + Program.Util.Manifest.RootName + "\\", InstallLocation );
				}

				bHasUserChangedInstallLocation = false;
			}
			else
			{
				HideExtraOptions();

				// Hide the back button
				InstallBackButton.Visible = false;

				StartMenuLocation = Program.Util.GetGameLongName();
				Text = Program.Util.GetPhrase( Program.Util.GetPhrase( "IOInstallOptionsGame" ) + StartMenuLocation );
				InstallOptionsTitleLabel.Text = Text;

				InstallLocationTextbox.Text = Path.Combine( "C:\\" + Program.Util.Manifest.RootName + "\\", StartMenuLocation );
			}

		}

		private bool ValidateInstallLocation()
		{
			string Location = GetInstallLocation();

			try
			{
				if( !Path.IsPathRooted( Location ) )
				{
					return ( false );
				}

				DirectoryInfo DirInfo = new DirectoryInfo( Location );
				if( !DirInfo.Exists )
				{
					Directory.CreateDirectory( DirInfo.FullName );
				}

				InstallLocationTextbox.Text = DirInfo.FullName;

				UInt64 FreeBytes = 0;
				UInt64 TotalBytes = 0;
				UInt64 TotalFreeBytes = 0;
				if( GetDiskFreeSpaceEx( DirInfo.FullName, out FreeBytes, out TotalBytes, out TotalFreeBytes ) )
				{
					// Get the install size of the project the user has selected
					UInt64 ProjInstallSize = Program.Util.GetProjectInstallSize();

					if( ProjInstallSize == 0 )
					{
						// We got an invalid project size so we resort to a hard coded value
						ProjInstallSize = 4UL * 1024UL * 1024UL * 1024UL;
					}
					else
					{
						// Pad the value out by 15% just to be on the safe side
						ProjInstallSize = ( UInt64 )( ProjInstallSize * 1.15 );
					}

					if( TotalFreeBytes < ProjInstallSize )
					{
						return ( false );
					}
				}
			}
			catch
			{
				return ( false );
			}

			return ( true );
		}



		public string GetInstallLocation()
		{
			if( InstallLocationTextbox.Text.EndsWith( ":" ) )
			{
				InstallLocationTextbox.Text += "\\";
			}

			return ( InstallLocationTextbox.Text );
		}

		public string GetStartMenuLocation()
		{
			return ( StartMenuLocation );
		}

		public string GetSubscribeAddress()
		{
			string Email = EmailTextBox.Text.Trim();
			return ( Email );
		}

		public string GetProjectName()
		{
			return ProjectNameTextBox.Text.Trim();
		}

		private void ChooseInstallLocationClick( object sender, MouseEventArgs e )
		{
			ChooseInstallLocationBrowser.SelectedPath = InstallLocationTextbox.Text;
			DialogResult Result = ChooseInstallLocationBrowser.ShowDialog();
			if( Result == DialogResult.OK )
			{
				InstallLocationTextbox.Text = ChooseInstallLocationBrowser.SelectedPath;
			}
		}

		private void InstallButtonClick( object sender, EventArgs e )
		{
			if( !ValidateInstallLocation() )
			{
				GenericQuery Query = new GenericQuery( "GQCaptionInvalidInstall", "GQDescInvalidInstall", false, "GQCancel", true, "GQOK" );
				Query.ShowDialog();
			}
			else
			{
				if( Program.Util.bIsCustomProject )
				{
					string InstallLocation = GetProjectName();
					StartMenuLocation = Path.Combine( Program.Util.Manifest.FullName + "\\", InstallLocation );
				}

				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void CancelButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}

		private void EmailAddressTextChanged( object sender, EventArgs e )
		{
			bool bWellFormedAddress = Program.Util.ValidateEmailAddress( EmailTextBox.Text.Trim() );
			InvalidEmailLabel.Visible = !bWellFormedAddress;
			InstallButton.Enabled = bWellFormedAddress;
		}

		private void ProjectNameTextChanged( object sender, EventArgs e )
		{
			bool bWellFormedProjectName = Program.Util.ValidateProjectName( ProjectNameTextBox.Text.Trim() );
			InvalidProjectNameLabel.Visible = !bWellFormedProjectName;
			InstallButton.Enabled = bWellFormedProjectName;

			if( Program.Util.bIsCustomProject && bWellFormedProjectName && !bHasUserChangedInstallLocation )
			{
				// Based on user input, an invalid path could result even with a well formed project name.  We will catch those errors here and prevent update of the
				//  text box if we run into any problems.
				try
				{
					InstallLocationTextbox.Text = Path.Combine( "C:\\" + Program.Util.Manifest.RootName + "\\", ProjectNameTextBox.Text.Trim() );
				}
				catch( Exception )
				{ }
				// Restore the variable that would have been set by the textbox OnChanged event since the change wasn't from the user directly
				bHasUserChangedInstallLocation = false;
			}
		}

		private void PrivacyLinkClicked( object sender, LinkClickedEventArgs e )
		{
			Process.Start( "rundll32.exe", "url.dll,FileProtocolHandler " + e.LinkText );
		}

		private void InstallBackButton_Click( object sender, EventArgs e )
		{
			// Currently using the Retry dialog result for back button support.
			DialogResult = DialogResult.Retry;
			Close();
		}

		private void InstallOptions_Shown( object sender, EventArgs e )
		{
			if( !bIsGame )
			{
				SetProjectSpecificValues();
			}
		}

		private void InstallLocationTextbox_TextChanged( object sender, EventArgs e )
		{
			bHasUserChangedInstallLocation = true;
		}
	}
}
