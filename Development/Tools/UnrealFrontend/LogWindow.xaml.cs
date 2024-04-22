/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System.Windows.Controls;
using UnrealControls;

namespace UnrealFrontend
{
	/// <summary>
	/// A wrapper for OutputWindowView
	/// </summary>
	public partial class LogWindow : UserControl
	{
		public LogWindow()
		{
			InitializeComponent();
			
			mOutputWindowView = new OutputWindowView();

			mOutputWindowView.AutoScroll = true;
			mOutputWindowView.BackColor = System.Drawing.SystemColors.Window;
			mOutputWindowView.Cursor = System.Windows.Forms.Cursors.IBeam;
			mOutputWindowView.Dock = System.Windows.Forms.DockStyle.Fill;
			mOutputWindowView.Document = new OutputWindowDocument();
			mOutputWindowView.Document.Text = "";
			mOutputWindowView.FindTextBackColor = System.Drawing.Color.Yellow;
			mOutputWindowView.FindTextForeColor = System.Drawing.Color.Black;
			mOutputWindowView.FindTextLineHighlight = System.Drawing.Color.FromArgb(((int)(((byte)(239)))), ((int)(((byte)(248)))), ((int)(((byte)(255)))));
			mOutputWindowView.Font = new System.Drawing.Font("Courier New", 9F);
			mOutputWindowView.ForeColor = System.Drawing.SystemColors.WindowText;
			mOutputWindowView.Location = new System.Drawing.Point(0, 0);
			mOutputWindowView.Name = "OutputWindowView_LogWindow";
			mOutputWindowView.Size = new System.Drawing.Size(1047, 1099);
			mOutputWindowView.TabIndex = 0;

			mWinformsHost.Child = mOutputWindowView;
		}

		private OutputWindowView mOutputWindowView;
		public OutputWindowDocument Document
		{
			get { return mOutputWindowView.Document; }
			set { mOutputWindowView.Document = value; }
		}

	}
}
