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
	public partial class ProgressBar : Form
	{
		public ProgressBar()
		{
			InitializeComponent();
		}

		private void OnLoad(object sender, EventArgs e)
		{
			Utils.CenterFormToPrimaryMonitor( this );
		}
	}
}
