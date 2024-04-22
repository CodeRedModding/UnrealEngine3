/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class UninstallOptions
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
			this.UninstallOptionsTitleLabel = new System.Windows.Forms.Label();
			this.UnInstallButton = new System.Windows.Forms.Button();
			this.UnInstallCancelButton = new System.Windows.Forms.Button();
			this.UninstallUDKRadio = new System.Windows.Forms.RadioButton();
			this.UninstallAllRadio = new System.Windows.Forms.RadioButton();
			this.InstallLocationLabel = new System.Windows.Forms.Label();
			this.UninstallOptionsHeaderLine = new System.Windows.Forms.Label();
			this.UninstallOptionsFooterLine = new System.Windows.Forms.Label();
			this.UninstallOptionsFooterLineLabel = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// UninstallOptionsTitleLabel
			// 
			this.UninstallOptionsTitleLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UninstallOptionsTitleLabel.BackColor = System.Drawing.Color.White;
			this.UninstallOptionsTitleLabel.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.UninstallOptionsTitleLabel.Image = global::UnSetup.Properties.Resources.BannerImage;
			this.UninstallOptionsTitleLabel.Location = new System.Drawing.Point( -3, 0 );
			this.UninstallOptionsTitleLabel.Name = "UninstallOptionsTitleLabel";
			this.UninstallOptionsTitleLabel.Size = new System.Drawing.Size( 800, 68 );
			this.UninstallOptionsTitleLabel.TabIndex = 10;
			this.UninstallOptionsTitleLabel.Text = "Uninstall Options for the Unreal Development Kit";
			this.UninstallOptionsTitleLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// UnInstallButton
			// 
			this.UnInstallButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UnInstallButton.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.UnInstallButton.Location = new System.Drawing.Point( 576, 233 );
			this.UnInstallButton.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.UnInstallButton.Name = "UnInstallButton";
			this.UnInstallButton.Size = new System.Drawing.Size( 100, 32 );
			this.UnInstallButton.TabIndex = 0;
			this.UnInstallButton.Text = "Uninstall";
			this.UnInstallButton.UseVisualStyleBackColor = true;
			this.UnInstallButton.Click += new System.EventHandler( this.UninstallButtonClick );
			// 
			// UnInstallCancelButton
			// 
			this.UnInstallCancelButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.UnInstallCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.UnInstallCancelButton.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.UnInstallCancelButton.Location = new System.Drawing.Point( 682, 233 );
			this.UnInstallCancelButton.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.UnInstallCancelButton.Name = "UnInstallCancelButton";
			this.UnInstallCancelButton.Size = new System.Drawing.Size( 100, 32 );
			this.UnInstallCancelButton.TabIndex = 1;
			this.UnInstallCancelButton.Text = "Cancel";
			this.UnInstallCancelButton.UseVisualStyleBackColor = true;
			this.UnInstallCancelButton.Click += new System.EventHandler( this.UninstallCancelButtonClick );
			// 
			// UninstallUDKRadio
			// 
			this.UninstallUDKRadio.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UninstallUDKRadio.AutoSize = true;
			this.UninstallUDKRadio.Checked = true;
			this.UninstallUDKRadio.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.UninstallUDKRadio.Location = new System.Drawing.Point( 15, 134 );
			this.UninstallUDKRadio.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.UninstallUDKRadio.Name = "UninstallUDKRadio";
			this.UninstallUDKRadio.Size = new System.Drawing.Size( 128, 20 );
			this.UninstallUDKRadio.TabIndex = 2;
			this.UninstallUDKRadio.TabStop = true;
			this.UninstallUDKRadio.Text = "Uninstall UDK files";
			this.UninstallUDKRadio.UseVisualStyleBackColor = true;
			// 
			// UninstallAllRadio
			// 
			this.UninstallAllRadio.AutoSize = true;
			this.UninstallAllRadio.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.UninstallAllRadio.Location = new System.Drawing.Point( 15, 162 );
			this.UninstallAllRadio.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.UninstallAllRadio.Name = "UninstallAllRadio";
			this.UninstallAllRadio.Size = new System.Drawing.Size( 118, 20 );
			this.UninstallAllRadio.TabIndex = 3;
			this.UninstallAllRadio.TabStop = true;
			this.UninstallAllRadio.Text = "Uninstall all files";
			this.UninstallAllRadio.UseVisualStyleBackColor = true;
			// 
			// InstallLocationLabel
			// 
			this.InstallLocationLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.InstallLocationLabel.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.InstallLocationLabel.Location = new System.Drawing.Point( 12, 82 );
			this.InstallLocationLabel.Name = "InstallLocationLabel";
			this.InstallLocationLabel.Size = new System.Drawing.Size( 770, 42 );
			this.InstallLocationLabel.TabIndex = 19;
			this.InstallLocationLabel.Text = "Location: C:\\UDK\\UDK-2009-08";
			this.InstallLocationLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// UninstallOptionsHeaderLine
			// 
			this.UninstallOptionsHeaderLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UninstallOptionsHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.UninstallOptionsHeaderLine.Location = new System.Drawing.Point( -3, 68 );
			this.UninstallOptionsHeaderLine.Name = "UninstallOptionsHeaderLine";
			this.UninstallOptionsHeaderLine.Size = new System.Drawing.Size( 800, 2 );
			this.UninstallOptionsHeaderLine.TabIndex = 20;
			// 
			// UninstallOptionsFooterLine
			// 
			this.UninstallOptionsFooterLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UninstallOptionsFooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.UninstallOptionsFooterLine.Location = new System.Drawing.Point( -3, 224 );
			this.UninstallOptionsFooterLine.Margin = new System.Windows.Forms.Padding( 3 );
			this.UninstallOptionsFooterLine.Name = "UninstallOptionsFooterLine";
			this.UninstallOptionsFooterLine.Size = new System.Drawing.Size( 800, 2 );
			this.UninstallOptionsFooterLine.TabIndex = 21;
			// 
			// UninstallOptionsFooterLineLabel
			// 
			this.UninstallOptionsFooterLineLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.UninstallOptionsFooterLineLabel.AutoSize = true;
			this.UninstallOptionsFooterLineLabel.Enabled = false;
			this.UninstallOptionsFooterLineLabel.FlatStyle = System.Windows.Forms.FlatStyle.System;
			this.UninstallOptionsFooterLineLabel.Location = new System.Drawing.Point( 12, 216 );
			this.UninstallOptionsFooterLineLabel.Name = "UninstallOptionsFooterLineLabel";
			this.UninstallOptionsFooterLineLabel.Size = new System.Drawing.Size( 148, 16 );
			this.UninstallOptionsFooterLineLabel.TabIndex = 22;
			this.UninstallOptionsFooterLineLabel.Text = " UnSetup - UDK-2009-09";
			// 
			// UninstallOptions
			// 
			this.AcceptButton = this.UnInstallButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.UnInstallCancelButton;
			this.ClientSize = new System.Drawing.Size( 794, 278 );
			this.Controls.Add( this.UninstallOptionsFooterLineLabel );
			this.Controls.Add( this.UninstallOptionsFooterLine );
			this.Controls.Add( this.UninstallOptionsHeaderLine );
			this.Controls.Add( this.InstallLocationLabel );
			this.Controls.Add( this.UninstallAllRadio );
			this.Controls.Add( this.UninstallUDKRadio );
			this.Controls.Add( this.UnInstallCancelButton );
			this.Controls.Add( this.UnInstallButton );
			this.Controls.Add( this.UninstallOptionsTitleLabel );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "UninstallOptions";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UDK Uninstall Options";
			this.Load += new System.EventHandler( this.OnLoad );
			this.ResumeLayout( false );
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label UninstallOptionsTitleLabel;
		private System.Windows.Forms.Button UnInstallButton;
		private System.Windows.Forms.Button UnInstallCancelButton;
		private System.Windows.Forms.RadioButton UninstallUDKRadio;
		private System.Windows.Forms.RadioButton UninstallAllRadio;
		private System.Windows.Forms.Label InstallLocationLabel;
		private System.Windows.Forms.Label UninstallOptionsHeaderLine;
		private System.Windows.Forms.Label UninstallOptionsFooterLine;
		private System.Windows.Forms.Label UninstallOptionsFooterLineLabel;
	}
}