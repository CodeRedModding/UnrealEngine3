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
using System.Threading;

namespace UnrealConsole
{
	/// <summary>
	/// A form for telling the user targets are being enumerated.
	/// </summary>
	public partial class EnumeratingTargetsForm : Form
	{
		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="Event">The event that tells the form all targets have been enumerated.</param>
		public EnumeratingTargetsForm()
		{
			InitializeComponent();
			Show();
		}
	}
}