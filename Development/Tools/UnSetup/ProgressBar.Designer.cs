/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class ProgressBar
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.GenericProgressBar = new System.Windows.Forms.ProgressBar();
			this.SubProgressBar = new System.Windows.Forms.ProgressBar();
			this.ProgressTitleLabel = new System.Windows.Forms.Label();
			this.ProgressLabel = new System.Windows.Forms.Label();
			this.ProgressHeaderLine = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// GenericProgressBar
			// 
			this.GenericProgressBar.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GenericProgressBar.Location = new System.Drawing.Point( 14, 185 );
			this.GenericProgressBar.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.GenericProgressBar.Name = "GenericProgressBar";
			this.GenericProgressBar.Size = new System.Drawing.Size( 766, 42 );
			this.GenericProgressBar.TabIndex = 0;
			// 
			// SubProgressBar
			// 
			this.SubProgressBar.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.SubProgressBar.Location = new System.Drawing.Point( 14, 149 );
			this.SubProgressBar.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.SubProgressBar.Name = "SubProgressBar";
			this.SubProgressBar.Size = new System.Drawing.Size( 766, 28 );
			this.SubProgressBar.TabIndex = 1;
			// 
			// ProgressTitleLabel
			// 
			this.ProgressTitleLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ProgressTitleLabel.BackColor = System.Drawing.Color.White;
			this.ProgressTitleLabel.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.ProgressTitleLabel.Location = new System.Drawing.Point( -3, 0 );
			this.ProgressTitleLabel.Name = "ProgressTitleLabel";
			this.ProgressTitleLabel.Size = new System.Drawing.Size( 800, 68 );
			this.ProgressTitleLabel.TabIndex = 10;
			this.ProgressTitleLabel.Text = "Title";
			this.ProgressTitleLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ProgressLabel
			// 
			this.ProgressLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ProgressLabel.Location = new System.Drawing.Point( 14, 100 );
			this.ProgressLabel.Name = "ProgressLabel";
			this.ProgressLabel.Size = new System.Drawing.Size( 766, 27 );
			this.ProgressLabel.TabIndex = 12;
			this.ProgressLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ProgressHeaderLine
			// 
			this.ProgressHeaderLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ProgressHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.ProgressHeaderLine.Location = new System.Drawing.Point( -3, 68 );
			this.ProgressHeaderLine.Name = "ProgressHeaderLine";
			this.ProgressHeaderLine.Size = new System.Drawing.Size( 800, 2 );
			this.ProgressHeaderLine.TabIndex = 13;
			// 
			// ProgressBar
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.BackColor = System.Drawing.SystemColors.Control;
			this.ClientSize = new System.Drawing.Size( 794, 242 );
			this.ControlBox = false;
			this.Controls.Add( this.ProgressHeaderLine );
			this.Controls.Add( this.ProgressLabel );
			this.Controls.Add( this.SubProgressBar );
			this.Controls.Add( this.GenericProgressBar );
			this.Controls.Add( this.ProgressTitleLabel );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ProgressBar";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Progress";
			this.Load += new System.EventHandler( this.OnLoad );
			this.ResumeLayout( false );

		}

		#endregion

		public System.Windows.Forms.ProgressBar GenericProgressBar;
		public System.Windows.Forms.ProgressBar SubProgressBar;
		public System.Windows.Forms.Label ProgressLabel;
		public System.Windows.Forms.Label ProgressTitleLabel;
		private System.Windows.Forms.Label ProgressHeaderLine;

	}
}