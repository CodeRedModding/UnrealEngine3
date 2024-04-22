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
	public partial class EULA : Form
	{
		public EULA()
		{
			InitializeComponent();

			Text = Program.Util.GetPhrase( "EULATitle" );
			EULABannerLabel.Text = Text;

			EULALegaleseRichText.Clear();
			if( Program.Util.bStandAloneRedist )
			{
				Icon = UnSetup.Properties.Resources.UE3Redist;
				EULABannerLabel.Image = UnSetup.Properties.Resources.UE3RedistBannerImage;
				EULALegaleseRichText.AppendText( Program.Util.GetPhrase( "UE3RedistEULALegalese" ) );
			}
			else
			{
				Icon = UnSetup.Properties.Resources.UDKIcon;
				EULABannerLabel.Image = UnSetup.Properties.Resources.BannerImage;
				EULALegaleseRichText.AppendText( Program.Util.GetPhrase( "EULALegalese" ) );
			}

			EULAFooterLineLabel.Text = Program.Util.UnSetupVersionString;

			ButtonAccept.Text = Program.Util.GetPhrase( "ButAccept" );
			ButtonReject.Text = Program.Util.GetPhrase( "ButReject" );
		}

		private void ClickButtonReject( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void ClickButtonAccept( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void EULALoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );

			string LocEULA = Program.Util.GetSafeLocFileName( "InstallData\\EULA", "rtf" );

			FileInfo Info = new FileInfo( LocEULA );
			if( Info.Exists )
			{
				EULATextBox.LoadFile( LocEULA );
			}
			else
			{
				GenericQuery Query = new GenericQuery( "GQCaptionNoEULA", "GQDescNoEULA", false, "GQCancel", true, "GQOK" );
				Query.ShowDialog();
				DialogResult = DialogResult.Cancel;
				Close();
			}
		}

		private void EULALinkClicked( object sender, LinkClickedEventArgs e )
		{
			Process.Start( "rundll32.exe", "url.dll,FileProtocolHandler " + e.LinkText );
		}
	}
}
