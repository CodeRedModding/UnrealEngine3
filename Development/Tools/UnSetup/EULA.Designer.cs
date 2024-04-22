/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class EULA
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager( typeof( EULA ) );
			this.EULATextBox = new System.Windows.Forms.RichTextBox();
			this.ButtonAccept = new System.Windows.Forms.Button();
			this.ButtonReject = new System.Windows.Forms.Button();
			this.EULABannerLabel = new System.Windows.Forms.Label();
			this.EULALegaleseRichText = new System.Windows.Forms.RichTextBox();
			this.EULAFooterLine = new System.Windows.Forms.Label();
			this.EULAFooterLineLabel = new System.Windows.Forms.Label();
			this.EULAHeaderLine = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// EULATextBox
			// 
			this.EULATextBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.EULATextBox.BackColor = System.Drawing.Color.White;
			this.EULATextBox.Location = new System.Drawing.Point( 9, 83 );
			this.EULATextBox.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.EULATextBox.Name = "EULATextBox";
			this.EULATextBox.ReadOnly = true;
			this.EULATextBox.ScrollBars = System.Windows.Forms.RichTextBoxScrollBars.Vertical;
			this.EULATextBox.Size = new System.Drawing.Size( 770, 400 );
			this.EULATextBox.TabIndex = 0;
			this.EULATextBox.Text = "";
			this.EULATextBox.LinkClicked += new System.Windows.Forms.LinkClickedEventHandler( this.EULALinkClicked );
			// 
			// ButtonAccept
			// 
			this.ButtonAccept.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ButtonAccept.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.ButtonAccept.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.ButtonAccept.Location = new System.Drawing.Point( 573, 568 );
			this.ButtonAccept.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.ButtonAccept.Name = "ButtonAccept";
			this.ButtonAccept.Size = new System.Drawing.Size( 100, 32 );
			this.ButtonAccept.TabIndex = 1;
			this.ButtonAccept.Text = "I Accept";
			this.ButtonAccept.UseVisualStyleBackColor = true;
			this.ButtonAccept.Click += new System.EventHandler( this.ClickButtonAccept );
			// 
			// ButtonReject
			// 
			this.ButtonReject.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ButtonReject.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.ButtonReject.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.ButtonReject.Location = new System.Drawing.Point( 679, 568 );
			this.ButtonReject.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.ButtonReject.Name = "ButtonReject";
			this.ButtonReject.Size = new System.Drawing.Size( 100, 32 );
			this.ButtonReject.TabIndex = 2;
			this.ButtonReject.Text = "Reject";
			this.ButtonReject.UseVisualStyleBackColor = true;
			this.ButtonReject.Click += new System.EventHandler( this.ClickButtonReject );
			// 
			// EULABannerLabel
			// 
			this.EULABannerLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.EULABannerLabel.BackColor = System.Drawing.Color.White;
			this.EULABannerLabel.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.EULABannerLabel.Location = new System.Drawing.Point( -3, 0 );
			this.EULABannerLabel.Margin = new System.Windows.Forms.Padding( 0 );
			this.EULABannerLabel.Name = "EULABannerLabel";
			this.EULABannerLabel.Size = new System.Drawing.Size( 802, 68 );
			this.EULABannerLabel.TabIndex = 4;
			this.EULABannerLabel.Text = "Title";
			this.EULABannerLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// EULALegaleseRichText
			// 
			this.EULALegaleseRichText.BackColor = System.Drawing.SystemColors.Control;
			this.EULALegaleseRichText.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.EULALegaleseRichText.ForeColor = System.Drawing.SystemColors.ControlText;
			this.EULALegaleseRichText.Location = new System.Drawing.Point( 9, 500 );
			this.EULALegaleseRichText.Name = "EULALegaleseRichText";
			this.EULALegaleseRichText.ReadOnly = true;
			this.EULALegaleseRichText.Size = new System.Drawing.Size( 770, 58 );
			this.EULALegaleseRichText.TabIndex = 0;
			this.EULALegaleseRichText.TabStop = false;
			this.EULALegaleseRichText.Text = resources.GetString( "EULALegaleseRichText.Text" );
			// 
			// EULAFooterLine
			// 
			this.EULAFooterLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.EULAFooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.EULAFooterLine.Location = new System.Drawing.Point( 1, 559 );
			this.EULAFooterLine.Margin = new System.Windows.Forms.Padding( 3 );
			this.EULAFooterLine.Name = "EULAFooterLine";
			this.EULAFooterLine.Size = new System.Drawing.Size( 800, 2 );
			this.EULAFooterLine.TabIndex = 8;
			this.EULAFooterLine.Text = "label1";
			// 
			// EULAFooterLineLabel
			// 
			this.EULAFooterLineLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.EULAFooterLineLabel.AutoSize = true;
			this.EULAFooterLineLabel.Enabled = false;
			this.EULAFooterLineLabel.FlatStyle = System.Windows.Forms.FlatStyle.System;
			this.EULAFooterLineLabel.Location = new System.Drawing.Point( 12, 551 );
			this.EULAFooterLineLabel.Name = "EULAFooterLineLabel";
			this.EULAFooterLineLabel.Size = new System.Drawing.Size( 148, 16 );
			this.EULAFooterLineLabel.TabIndex = 9;
			this.EULAFooterLineLabel.Text = " UnSetup - UDK-2009-09";
			this.EULAFooterLineLabel.TextAlign = System.Drawing.ContentAlignment.TopCenter;
			// 
			// EULAHeaderLine
			// 
			this.EULAHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.EULAHeaderLine.Location = new System.Drawing.Point( -3, 68 );
			this.EULAHeaderLine.Name = "EULAHeaderLine";
			this.EULAHeaderLine.Size = new System.Drawing.Size( 800, 2 );
			this.EULAHeaderLine.TabIndex = 10;
			// 
			// EULA
			// 
			this.AcceptButton = this.ButtonAccept;
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.ButtonReject;
			this.ClientSize = new System.Drawing.Size( 794, 613 );
			this.Controls.Add( this.EULAFooterLineLabel );
			this.Controls.Add( this.EULAFooterLine );
			this.Controls.Add( this.EULALegaleseRichText );
			this.Controls.Add( this.EULABannerLabel );
			this.Controls.Add( this.ButtonReject );
			this.Controls.Add( this.ButtonAccept );
			this.Controls.Add( this.EULATextBox );
			this.Controls.Add( this.EULAHeaderLine );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.MinimumSize = new System.Drawing.Size( 699, 363 );
			this.Name = "EULA";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Title";
			this.TopMost = true;
			this.Load += new System.EventHandler( this.EULALoad );
			this.ResumeLayout( false );
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.RichTextBox EULATextBox;
		private System.Windows.Forms.Button ButtonAccept;
		private System.Windows.Forms.Button ButtonReject;
		private System.Windows.Forms.Label EULABannerLabel;
		private System.Windows.Forms.RichTextBox EULALegaleseRichText;
		private System.Windows.Forms.Label EULAFooterLine;
		private System.Windows.Forms.Label EULAFooterLineLabel;
		private System.Windows.Forms.Label EULAHeaderLine;
	}
}

