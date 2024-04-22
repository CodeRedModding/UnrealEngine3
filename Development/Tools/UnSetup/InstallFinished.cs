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
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class InstallFinished : Form
	{
		private string InstallFolder = "";
		private bool bIsGame;

		public InstallFinished( string DestFolder, bool IsGame, bool bShowBackButton )
		{
			bIsGame = IsGame;
			InstallFolder = DestFolder;

			InitializeComponent();

			if (!bShowBackButton)
			{
				InstallFinishedBackButton.Visible = false;
			}

			RadioButton[] RadioButtons = new RadioButton[]
			{
				RadioButton0,
				RadioButton1,
				RadioButton2,
			};

			Text = Program.Util.GetPhrase( "IFInstallFinished" );
			InstallFinishedTitleLabel.Text = Text;
			InstallFinishedOKButton.Text = Program.Util.GetPhrase( "IFFinished" );

			FinishedContentLabel.Text = Program.Util.GetPhrase( "IFInstallContentFinished" );

			PerforceImageLabel.Visible = false;
			if( IsGame )
			{
				FinishedContentLabel.Text += Program.Util.GetGameLongName();
				LaunchUDKCheckBox.Text = Program.Util.GetPhrase( "IFLaunch" ) + Program.Util.GetGameLongName();

				NetCodeRichTextBox.Clear();
				NetCodeRichTextBox.SelectionAlignment = HorizontalAlignment.Center;
				NetCodeRichTextBox.SelectedText = Program.Util.GetPhrase( "NetCodeWarn" );

				// Hide all radio buttons
				foreach( RadioButton Radio in RadioButtons )
				{
					Radio.Visible = false;
				}
			}
			else
			{
				FinishedContentLabel.Text += Program.Util.Manifest.FullName;

				UpdateNonGameElements();
			}

			InstallOptionsFooterLineLabel.Text = Program.Util.UnSetupVersionString;

			if( !Program.Util.bDependenciesSuccessful || Program.Util.bSkipDependencies )
			{
				FinishedContentLabel.Text = Program.Util.GetPhrase( "IFDependsFailed" );
				LaunchUDKCheckBox.Visible = false;
				LaunchUDKCheckBox.Checked = false;
			}
		}

		private void UpdateNonGameElements()
		{
			RadioButton[] RadioButtons = new RadioButton[]
			{
				RadioButton0,
				RadioButton1,
				RadioButton2,
			};

			int Index = 0;
			RadioButtons[Index].Checked = true;

			foreach (Utils.GameManifestOptions Game in Program.Util.Manifest.GameInfo)
			{
				RadioButtons[Index].Text = Program.Util.GetPhrase("IFLaunch") + Game.Name;
				RadioButtons[Index].Tag = Game;
                RadioButtons[Index].Visible = true;
				Index++;
			}

			if (Program.Util.GetP4ServerVersion() != string.Empty ||
				Program.Util.GetP4ClientVersion() != string.Empty)
			{
				Utils.GameManifestOptions P4Populate = new Utils.GameManifestOptions();
				P4Populate.Name = "P4PopulateDepot";
				P4Populate.AppToElevate = "P4PopulateDepot.exe";
				P4Populate.AppToCreateInis = string.Empty;
				P4Populate.AppCommandLine = string.Empty;

				RadioButtons[Index].Text = Program.Util.GetPhrase("IFPerforcePopulate");
				RadioButtons[Index].Tag = P4Populate;
                RadioButtons[Index].Visible = true;
				Index++;
			}

			RadioButtons[Index].Text = Program.Util.GetPhrase("R2D");
            RadioButtons[Index].Visible = true;
			Index++;

			while (Index < RadioButtons.Length)
			{
				RadioButtons[Index].Visible = false;
				Index++;
			}

			// Hide checkbox
			LaunchUDKCheckBox.Visible = false;

			NetCodeRichTextBox.Enabled = false;
		}

		public bool GetLaunchChecked()
		{
			return ( LaunchUDKCheckBox.Checked );
		}

		public Utils.GameManifestOptions GetLaunchType()
		{
			if( RadioButton0.Checked )
			{
				return ( ( Utils.GameManifestOptions )RadioButton0.Tag );
			}
			else if( RadioButton1.Checked )
			{
				return ( ( Utils.GameManifestOptions )RadioButton1.Tag );
			}
			else if( RadioButton2.Checked )
			{
				return ( ( Utils.GameManifestOptions )RadioButton2.Tag );
			}

			return ( null );
		}


		private void InstallFinishedOKClicked( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}

		private void InstallFinishedInstallExtrasClicked( object sender, EventArgs e )
		{
			try
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo( Path.Combine( InstallFolder, "Binaries\\InstallData\\Extras.hta" ) );
				StartInfo.WorkingDirectory = Path.Combine( InstallFolder, "Binaries\\InstallData" );
				Process.Start( StartInfo );
			}
			catch
			{
			}
		}

		private void InstallFinishedBackButton_Click(object sender, EventArgs e)
		{
			// Currently using the Retry dialog result for back button support.
			DialogResult = DialogResult.Retry;
			Close();
		}

		private void InstallFinished_Shown(object sender, EventArgs e)
		{
			if (!bIsGame)
			{
				UpdateNonGameElements();

				if (Program.Util.GetP4ServerVersion() != string.Empty ||
					Program.Util.GetP4ClientVersion() != string.Empty)
				{
					NetCodeRichTextBox.Enabled = true;
					NetCodeRichTextBox.Clear();
					NetCodeRichTextBox.SelectionAlignment = HorizontalAlignment.Left;
					NetCodeRichTextBox.SelectedText = Program.Util.GetPhrase("IFP4Description");
					PerforceImageLabel.Visible = true;
				}
				else
				{
					NetCodeRichTextBox.Enabled = false;
					NetCodeRichTextBox.Clear();
					PerforceImageLabel.Visible = false;
				}
			}
		}
	}
}
