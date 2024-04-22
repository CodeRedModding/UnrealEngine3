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

namespace UnrealConsole
{
	/// <summary>
	/// A form for representing exceptions in an easily readable format.
	/// </summary>
	public partial class ExceptionBox : Form
	{
		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="ex">The exception to display.</param>
		public ExceptionBox(Exception ex)
		{
			InitializeComponent();

			this.Text = ex.GetType().FullName;
			this.txtDescription.Text = ex.ToString();
		}

		/// <summary>
		/// Closes the form.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void btnClose_Click(object sender, EventArgs e)
		{
			this.Close();
		}
	}
}