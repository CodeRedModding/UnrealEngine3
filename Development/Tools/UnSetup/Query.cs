/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class GenericQuery : Form
	{
		public GenericQuery()
		{
			InitializeComponent();
		}

		public GenericQuery( string Caption, string Description, bool EnableNegative, string Negative, bool EnableAffirmative, string Affirmative )
		{
			InitializeComponent();

			if( Program.Util.bStandAloneRedist )
			{
				Icon = UnSetup.Properties.Resources.UE3Redist;
				UDKLogoLabel.Image = UnSetup.Properties.Resources.UE3RedistImage;
			}
			else
			{
				Icon = UnSetup.Properties.Resources.UDKIcon;
				UDKLogoLabel.Image = UnSetup.Properties.Resources.UDKShieldImage;
			}

			Text = Program.Util.GetPhrase( Caption );
			QueryDescription.Clear();
			QueryDescription.SelectionAlignment = HorizontalAlignment.Center;

			QueryDescription.SelectedText = Program.Util.GetPhrase( Description );

			QueryCancelButton.Visible = EnableNegative;
			QueryOKButton.Visible = EnableAffirmative;

			if( EnableAffirmative )
			{
				QueryOKButton.Text = Program.Util.GetPhrase( Affirmative );
			}

			if( EnableNegative )
			{
				QueryCancelButton.Text = Program.Util.GetPhrase( Negative );
			}

			if( EnableAffirmative && !EnableNegative )
			{
				QueryOKButton.Location = QueryCancelButton.Location;
			}
		}

		public void OverrideDescriptionAlignment(HorizontalAlignment Alignment)
		{
			if (Alignment != QueryDescription.SelectionAlignment)
			{
				// Store off the existing text.
				string ExistingText = QueryDescription.Text;
				QueryDescription.Clear();
				// Set desired alignment.
				QueryDescription.SelectionAlignment = Alignment;
				QueryDescription.SelectedText = ExistingText;
			}
			
		}

		private void QueryCancelButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void QueryOKButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void QueryLinkClicked( object sender, LinkClickedEventArgs e )
		{
			Process.Start( "rundll32.exe", "url.dll,FileProtocolHandler " + e.LinkText );
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}
	}
}
