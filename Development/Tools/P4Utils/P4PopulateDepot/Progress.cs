// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace P4PopulateDepot
{
	public partial class Progress : Form
	{
		public Progress()
		{
			InitializeComponent();

			//Text = Program.Util.GetPhrase("PROGRESSPleaseWait");
			//LabelHeading.Text = Text;

			Icon = P4PopulateDepot.Properties.Resources.UDKIcon;
			ProgressAnimatedPictureBox.Image = P4PopulateDepot.Properties.Resources.Waiting;
			LabelHeading.Image = P4PopulateDepot.Properties.Resources.Banner;
		}

		private void Progress_Load(object sender, EventArgs e)
		{
			Utils.CenterFormToPrimaryMonitor(this);
		}
	}
}
