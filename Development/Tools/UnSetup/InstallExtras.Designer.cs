/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class InstallExtras
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
            this.InstallExtrasTitleLabel = new System.Windows.Forms.Label();
            this.InstallExtrasHeaderLine = new System.Windows.Forms.Label();
            this.InstallExtrasFooterLine = new System.Windows.Forms.Label();
            this.InstallExtrasOKButton = new System.Windows.Forms.Button();
            this.InstallOptionsFooterLineLabel = new System.Windows.Forms.Label();
            this.InstallP4ServerLabel = new System.Windows.Forms.Label();
            this.P4ServerInfoTextbox = new System.Windows.Forms.RichTextBox();
            this.InstallLocationGroupBox = new System.Windows.Forms.GroupBox();
            this.InstallP4Server = new System.Windows.Forms.Button();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.InstallP4Client = new System.Windows.Forms.Button();
            this.InstallP4ClientLabel = new System.Windows.Forms.Label();
            this.P4ClientTextbox = new System.Windows.Forms.RichTextBox();
            this.p4DescriptionTextBox = new System.Windows.Forms.RichTextBox();
            this.InstallLocationGroupBox.SuspendLayout();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // InstallExtrasTitleLabel
            // 
            this.InstallExtrasTitleLabel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.InstallExtrasTitleLabel.BackColor = System.Drawing.Color.White;
            this.InstallExtrasTitleLabel.Font = new System.Drawing.Font("Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.InstallExtrasTitleLabel.Image = global::UnSetup.Properties.Resources.BannerImage;
            this.InstallExtrasTitleLabel.Location = new System.Drawing.Point(-3, 0);
            this.InstallExtrasTitleLabel.Name = "InstallExtrasTitleLabel";
            this.InstallExtrasTitleLabel.Size = new System.Drawing.Size(800, 68);
            this.InstallExtrasTitleLabel.TabIndex = 10;
            this.InstallExtrasTitleLabel.Text = "Title";
            this.InstallExtrasTitleLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // InstallExtrasHeaderLine
            // 
            this.InstallExtrasHeaderLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.InstallExtrasHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.InstallExtrasHeaderLine.Location = new System.Drawing.Point(-3, 68);
            this.InstallExtrasHeaderLine.Margin = new System.Windows.Forms.Padding(3);
            this.InstallExtrasHeaderLine.Name = "InstallExtrasHeaderLine";
            this.InstallExtrasHeaderLine.Size = new System.Drawing.Size(800, 2);
            this.InstallExtrasHeaderLine.TabIndex = 24;
            // 
            // InstallExtrasFooterLine
            // 
            this.InstallExtrasFooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.InstallExtrasFooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.InstallExtrasFooterLine.Location = new System.Drawing.Point(-3, 293);
            this.InstallExtrasFooterLine.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.InstallExtrasFooterLine.Name = "InstallExtrasFooterLine";
            this.InstallExtrasFooterLine.Size = new System.Drawing.Size(800, 2);
            this.InstallExtrasFooterLine.TabIndex = 25;
            // 
            // InstallExtrasOKButton
            // 
            this.InstallExtrasOKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.InstallExtrasOKButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.InstallExtrasOKButton.Location = new System.Drawing.Point(682, 303);
            this.InstallExtrasOKButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.InstallExtrasOKButton.Name = "InstallExtrasOKButton";
            this.InstallExtrasOKButton.Size = new System.Drawing.Size(100, 32);
            this.InstallExtrasOKButton.TabIndex = 0;
            this.InstallExtrasOKButton.Text = "Next";
            this.InstallExtrasOKButton.UseVisualStyleBackColor = true;
            this.InstallExtrasOKButton.Click += new System.EventHandler(this.InstallExtrasOKClicked);
            // 
            // InstallOptionsFooterLineLabel
            // 
            this.InstallOptionsFooterLineLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.InstallOptionsFooterLineLabel.AutoSize = true;
            this.InstallOptionsFooterLineLabel.Enabled = false;
            this.InstallOptionsFooterLineLabel.FlatStyle = System.Windows.Forms.FlatStyle.System;
            this.InstallOptionsFooterLineLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.InstallOptionsFooterLineLabel.Location = new System.Drawing.Point(12, 285);
            this.InstallOptionsFooterLineLabel.Name = "InstallOptionsFooterLineLabel";
            this.InstallOptionsFooterLineLabel.Size = new System.Drawing.Size(87, 16);
            this.InstallOptionsFooterLineLabel.TabIndex = 27;
            this.InstallOptionsFooterLineLabel.Text = " UDK-2009-09";
            // 
            // InstallP4ServerLabel
            // 
            this.InstallP4ServerLabel.AutoSize = true;
            this.InstallP4ServerLabel.Location = new System.Drawing.Point(19, 19);
            this.InstallP4ServerLabel.Name = "InstallP4ServerLabel";
            this.InstallP4ServerLabel.Size = new System.Drawing.Size(223, 16);
            this.InstallP4ServerLabel.TabIndex = 28;
            this.InstallP4ServerLabel.Text = Program.Util.GetPhrase("IEServerInstallDesc");
            // 
            // P4ServerInfoTextbox
            // 
            this.P4ServerInfoTextbox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.P4ServerInfoTextbox.BackColor = System.Drawing.SystemColors.Control;
            this.P4ServerInfoTextbox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.P4ServerInfoTextbox.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.P4ServerInfoTextbox.Location = new System.Drawing.Point(156, 52);
            this.P4ServerInfoTextbox.Name = "P4ServerInfoTextbox";
            this.P4ServerInfoTextbox.ReadOnly = true;
            this.P4ServerInfoTextbox.Size = new System.Drawing.Size(602, 22);
            this.P4ServerInfoTextbox.TabIndex = 30;
            this.P4ServerInfoTextbox.TabStop = false;
            this.P4ServerInfoTextbox.Text = Program.Util.GetPhrase("IEServerInstallDescLink");
            this.P4ServerInfoTextbox.LinkClicked += new System.Windows.Forms.LinkClickedEventHandler(this.P4ServerInfoTextbox_LinkClicked);
            // 
            // InstallLocationGroupBox
            // 
            this.InstallLocationGroupBox.Controls.Add(this.InstallP4Server);
            this.InstallLocationGroupBox.Controls.Add(this.InstallP4ServerLabel);
            this.InstallLocationGroupBox.Controls.Add(this.P4ServerInfoTextbox);
            this.InstallLocationGroupBox.Location = new System.Drawing.Point(15, 116);
            this.InstallLocationGroupBox.Name = "InstallLocationGroupBox";
            this.InstallLocationGroupBox.Size = new System.Drawing.Size(764, 80);
            this.InstallLocationGroupBox.TabIndex = 31;
            this.InstallLocationGroupBox.TabStop = false;
            // 
            // InstallP4Server
            // 
            this.InstallP4Server.Location = new System.Drawing.Point(45, 47);
            this.InstallP4Server.Name = "InstallP4Server";
            this.InstallP4Server.Size = new System.Drawing.Size(96, 23);
            this.InstallP4Server.TabIndex = 32;
            this.InstallP4Server.Text = "Install Server";
            this.InstallP4Server.UseVisualStyleBackColor = true;
            this.InstallP4Server.Click += new System.EventHandler(this.InstallP4Server_Click);
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.InstallP4Client);
            this.groupBox1.Controls.Add(this.InstallP4ClientLabel);
            this.groupBox1.Controls.Add(this.P4ClientTextbox);
            this.groupBox1.Location = new System.Drawing.Point(15, 202);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(764, 80);
            this.groupBox1.TabIndex = 33;
            this.groupBox1.TabStop = false;
            // 
            // InstallP4Client
            // 
            this.InstallP4Client.Location = new System.Drawing.Point(45, 47);
            this.InstallP4Client.Name = "InstallP4Client";
            this.InstallP4Client.Size = new System.Drawing.Size(102, 23);
            this.InstallP4Client.TabIndex = 32;
            this.InstallP4Client.Text = "Install Client";
            this.InstallP4Client.UseVisualStyleBackColor = true;
            this.InstallP4Client.Click += new System.EventHandler(this.InstallP4Client_Click);
            // 
            // InstallP4ClientLabel
            // 
            this.InstallP4ClientLabel.AutoSize = true;
            this.InstallP4ClientLabel.Location = new System.Drawing.Point(19, 19);
            this.InstallP4ClientLabel.Name = "InstallP4ClientLabel";
            this.InstallP4ClientLabel.Size = new System.Drawing.Size(259, 16);
            this.InstallP4ClientLabel.TabIndex = 28;
            this.InstallP4ClientLabel.Text = Program.Util.GetPhrase("IEClientInstallDesc");
            // 
            // P4ClientTextbox
            // 
            this.P4ClientTextbox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.P4ClientTextbox.BackColor = System.Drawing.SystemColors.Control;
            this.P4ClientTextbox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.P4ClientTextbox.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.P4ClientTextbox.Location = new System.Drawing.Point(156, 52);
            this.P4ClientTextbox.Name = "P4ClientTextbox";
            this.P4ClientTextbox.ReadOnly = true;
            this.P4ClientTextbox.Size = new System.Drawing.Size(602, 22);
            this.P4ClientTextbox.TabIndex = 30;
            this.P4ClientTextbox.TabStop = false;
            this.P4ClientTextbox.Text = Program.Util.GetPhrase("IEClientInstallDescLink");
            this.P4ClientTextbox.LinkClicked += new System.Windows.Forms.LinkClickedEventHandler(this.P4ClientTextbox_LinkClicked);
            // 
            // p4DescriptionTextBox
            // 
            this.p4DescriptionTextBox.BackColor = System.Drawing.SystemColors.Control;
            this.p4DescriptionTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.p4DescriptionTextBox.Location = new System.Drawing.Point(15, 76);
            this.p4DescriptionTextBox.Name = "p4DescriptionTextBox";
            this.p4DescriptionTextBox.ReadOnly = true;
            this.p4DescriptionTextBox.ScrollBars = System.Windows.Forms.RichTextBoxScrollBars.Vertical;
            this.p4DescriptionTextBox.Size = new System.Drawing.Size(767, 48);
            this.p4DescriptionTextBox.TabIndex = 34;
            this.p4DescriptionTextBox.TabStop = false;
            this.p4DescriptionTextBox.Text = "Description";
            this.p4DescriptionTextBox.LinkClicked += new System.Windows.Forms.LinkClickedEventHandler(this.p4DescriptionTextBox_LinkClicked);
            // 
            // InstallExtras
            // 
            this.AcceptButton = this.InstallExtrasOKButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(794, 348);
            this.Controls.Add(this.p4DescriptionTextBox);
            this.Controls.Add(this.groupBox1);
            this.Controls.Add(this.InstallLocationGroupBox);
            this.Controls.Add(this.InstallOptionsFooterLineLabel);
            this.Controls.Add(this.InstallExtrasOKButton);
            this.Controls.Add(this.InstallExtrasFooterLine);
            this.Controls.Add(this.InstallExtrasHeaderLine);
            this.Controls.Add(this.InstallExtrasTitleLabel);
            this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
            this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "InstallExtras";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Title";
            this.Load += new System.EventHandler(this.OnLoad);
            this.InstallLocationGroupBox.ResumeLayout(false);
            this.InstallLocationGroupBox.PerformLayout();
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label InstallExtrasTitleLabel;
		private System.Windows.Forms.Label InstallExtrasHeaderLine;
		private System.Windows.Forms.Label InstallExtrasFooterLine;
		private System.Windows.Forms.Button InstallExtrasOKButton;
		private System.Windows.Forms.Label InstallOptionsFooterLineLabel;
		private System.Windows.Forms.Label InstallP4ServerLabel;
		private System.Windows.Forms.RichTextBox P4ServerInfoTextbox;
		private System.Windows.Forms.GroupBox InstallLocationGroupBox;
		private System.Windows.Forms.Button InstallP4Server;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button InstallP4Client;
		private System.Windows.Forms.Label InstallP4ClientLabel;
		private System.Windows.Forms.RichTextBox P4ClientTextbox;
		private System.Windows.Forms.RichTextBox p4DescriptionTextBox;
	}
}