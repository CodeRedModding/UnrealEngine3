/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class GenericQuery
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
			this.QueryOKButton = new System.Windows.Forms.Button();
			this.QueryCancelButton = new System.Windows.Forms.Button();
			this.UDKLogoLabel = new System.Windows.Forms.Label();
			this.QueryDescription = new System.Windows.Forms.RichTextBox();
			this.GenericQueryFooterLine = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// QueryOKButton
			// 
			this.QueryOKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.QueryOKButton.AutoSize = true;
			this.QueryOKButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.QueryOKButton.Location = new System.Drawing.Point(379, 169);
			this.QueryOKButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.QueryOKButton.Name = "QueryOKButton";
			this.QueryOKButton.Size = new System.Drawing.Size(100, 32);
			this.QueryOKButton.TabIndex = 4;
			this.QueryOKButton.Tag = "";
			this.QueryOKButton.Text = "OK";
			this.QueryOKButton.UseVisualStyleBackColor = true;
			this.QueryOKButton.Click += new System.EventHandler(this.QueryOKButtonClick);
			// 
			// QueryCancelButton
			// 
			this.QueryCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.QueryCancelButton.AutoSize = true;
			this.QueryCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.QueryCancelButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.QueryCancelButton.Location = new System.Drawing.Point(485, 169);
			this.QueryCancelButton.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.QueryCancelButton.Name = "QueryCancelButton";
			this.QueryCancelButton.Size = new System.Drawing.Size(100, 32);
			this.QueryCancelButton.TabIndex = 5;
			this.QueryCancelButton.Tag = "";
			this.QueryCancelButton.Text = "Cancel";
			this.QueryCancelButton.UseVisualStyleBackColor = true;
			this.QueryCancelButton.Click += new System.EventHandler(this.QueryCancelButtonClick);
			// 
			// UDKLogoLabel
			// 
			this.UDKLogoLabel.Location = new System.Drawing.Point(12, 9);
			this.UDKLogoLabel.Name = "UDKLogoLabel";
			this.UDKLogoLabel.Size = new System.Drawing.Size(128, 128);
			this.UDKLogoLabel.TabIndex = 7;
			this.UDKLogoLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// QueryDescription
			// 
			this.QueryDescription.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.QueryDescription.BackColor = System.Drawing.SystemColors.Control;
			this.QueryDescription.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.QueryDescription.Location = new System.Drawing.Point(146, 24);
			this.QueryDescription.Name = "QueryDescription";
			this.QueryDescription.ReadOnly = true;
			this.QueryDescription.ScrollBars = System.Windows.Forms.RichTextBoxScrollBars.Vertical;
			this.QueryDescription.Size = new System.Drawing.Size(439, 130);
			this.QueryDescription.TabIndex = 9;
			this.QueryDescription.TabStop = false;
			this.QueryDescription.Text = "Description";
			this.QueryDescription.LinkClicked += new System.Windows.Forms.LinkClickedEventHandler(this.QueryLinkClicked);
			// 
			// GenericQueryFooterLine
			// 
			this.GenericQueryFooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.GenericQueryFooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.GenericQueryFooterLine.Location = new System.Drawing.Point(-3, 160);
			this.GenericQueryFooterLine.Margin = new System.Windows.Forms.Padding(3);
			this.GenericQueryFooterLine.Name = "GenericQueryFooterLine";
			this.GenericQueryFooterLine.Size = new System.Drawing.Size(603, 2);
			this.GenericQueryFooterLine.TabIndex = 10;
			// 
			// GenericQuery
			// 
			this.AcceptButton = this.QueryOKButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.QueryCancelButton;
			this.ClientSize = new System.Drawing.Size(597, 214);
			this.Controls.Add(this.GenericQueryFooterLine);
			this.Controls.Add(this.UDKLogoLabel);
			this.Controls.Add(this.QueryDescription);
			this.Controls.Add(this.QueryOKButton);
			this.Controls.Add(this.QueryCancelButton);
			this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
			this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "GenericQuery";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Query Title";
			this.TopMost = true;
			this.Load += new System.EventHandler(this.OnLoad);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Button QueryOKButton;
		private System.Windows.Forms.Button QueryCancelButton;
		private System.Windows.Forms.Label UDKLogoLabel;
		private System.Windows.Forms.RichTextBox QueryDescription;
		private System.Windows.Forms.Label GenericQueryFooterLine;
	}
}