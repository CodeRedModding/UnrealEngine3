/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class UninstallOptions : Form
	{
		public UninstallOptions( bool bIsGame )
		{
			InitializeComponent();

			if( !bIsGame )
			{
				Text = Program.Util.GetPhrase( "UIOOptionsGame" ) + Program.Util.Manifest.FullName;
				UninstallOptionsTitleLabel.Text = Text;
			}
			else
			{
				Text = Program.Util.GetPhrase( "UIOOptionsGame" ) + Program.Util.GetGameLongName();
				UninstallOptionsTitleLabel.Text = Text;
			}

			InstallLocationLabel.Text = Program.Util.GetPhrase( "UIOLocation" ) + Program.Util.PackageInstallLocation;
			UnInstallButton.Text = Program.Util.GetPhrase( "ButUninstall" );
			UnInstallCancelButton.Text = Program.Util.GetPhrase( "GQCancel" );

			UninstallUDKRadio.Text = Program.Util.GetPhrase( "UIODeleteOnly" );
			UninstallAllRadio.Text = Program.Util.GetPhrase( "UIODeleteAll" );

			int Left = ( 800 - UninstallUDKRadio.Width ) / 2;
			UninstallUDKRadio.Location = new System.Drawing.Point( Left, UninstallUDKRadio.Location.Y );
			UninstallAllRadio.Location = new System.Drawing.Point( Left, UninstallAllRadio.Location.Y );

			UninstallOptionsFooterLineLabel.Text = Program.Util.UnSetupVersionString;
		}

		public bool GetDeleteAll()
		{
			return ( UninstallAllRadio.Checked );
		}

		private void UninstallButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void UninstallCancelButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}
	}
}
