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
	public partial class RedistProgress : Form
	{
		public RedistProgress()
		{
			InitializeComponent();

			Text = Program.Util.GetPhrase( "RedistPleaseWait" );
			LabelPleaseWait.Text = Text;

			if( Program.Util.bStandAloneRedist )
			{
				Icon = UnSetup.Properties.Resources.UE3Redist;
				RedistAnimatedPictureBox.Image = UnSetup.Properties.Resources.UE3RedistWaiting;
				LabelPleaseWait.Image = global::UnSetup.Properties.Resources.UE3RedistBannerImage;
			}
			else
			{
				Icon = UnSetup.Properties.Resources.UDKIcon;
				RedistAnimatedPictureBox.Image = UnSetup.Properties.Resources.Waiting;
				LabelPleaseWait.Image = global::UnSetup.Properties.Resources.BannerImage;
			}
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}
	}
}
