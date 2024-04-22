namespace P4PopulateDepot
{
	partial class NewWorkspaceDialog
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(NewWorkspaceDialog));
			this.FooterLine = new System.Windows.Forms.Label();
			this.WSNewOKButton = new System.Windows.Forms.Button();
			this.WSNewCancelButton = new System.Windows.Forms.Button();
			this.ConnectionWorkspaceBrowseButton = new System.Windows.Forms.Button();
			this.NWWorkspaceRootTextBox = new System.Windows.Forms.TextBox();
			this.label7 = new System.Windows.Forms.Label();
			this.label6 = new System.Windows.Forms.Label();
			this.NWWorkspaceNameTextBox = new System.Windows.Forms.TextBox();
			this.NWDescriptionTextBox = new System.Windows.Forms.RichTextBox();
			this.NWWorkspaceNameErrLabel = new System.Windows.Forms.Label();
			this.NWWorkspaceRootErrLabel = new System.Windows.Forms.Label();
			this.NWMainErrorDescription = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// FooterLine
			// 
			this.FooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.FooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.FooterLine.Location = new System.Drawing.Point(-3, 235);
			this.FooterLine.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.FooterLine.Name = "FooterLine";
			this.FooterLine.Size = new System.Drawing.Size(705, 2);
			this.FooterLine.TabIndex = 13;
			// 
			// WSNewOKButton
			// 
			this.WSNewOKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.WSNewOKButton.AutoSize = true;
			this.WSNewOKButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.WSNewOKButton.Location = new System.Drawing.Point(476, 246);
			this.WSNewOKButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
			this.WSNewOKButton.Name = "WSNewOKButton";
			this.WSNewOKButton.Size = new System.Drawing.Size(100, 32);
			this.WSNewOKButton.TabIndex = 11;
			this.WSNewOKButton.Tag = "";
			this.WSNewOKButton.Text = "OK";
			this.WSNewOKButton.UseVisualStyleBackColor = true;
			this.WSNewOKButton.Click += new System.EventHandler(this.WSNewOKButton_Click);
			// 
			// WSNewCancelButton
			// 
			this.WSNewCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.WSNewCancelButton.AutoSize = true;
			this.WSNewCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.WSNewCancelButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.WSNewCancelButton.Location = new System.Drawing.Point(582, 246);
			this.WSNewCancelButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
			this.WSNewCancelButton.Name = "WSNewCancelButton";
			this.WSNewCancelButton.Size = new System.Drawing.Size(100, 32);
			this.WSNewCancelButton.TabIndex = 12;
			this.WSNewCancelButton.Tag = "";
			this.WSNewCancelButton.Text = "Cancel";
			this.WSNewCancelButton.UseVisualStyleBackColor = true;
			// 
			// ConnectionWorkspaceBrowseButton
			// 
			this.ConnectionWorkspaceBrowseButton.Location = new System.Drawing.Point(551, 149);
			this.ConnectionWorkspaceBrowseButton.Name = "ConnectionWorkspaceBrowseButton";
			this.ConnectionWorkspaceBrowseButton.Size = new System.Drawing.Size(81, 23);
			this.ConnectionWorkspaceBrowseButton.TabIndex = 14;
			this.ConnectionWorkspaceBrowseButton.Text = "Browse...";
			this.ConnectionWorkspaceBrowseButton.UseVisualStyleBackColor = true;
			this.ConnectionWorkspaceBrowseButton.Click += new System.EventHandler(this.ConnectionWorkspaceBrowseButton_Click);
			// 
			// NWWorkspaceRootTextBox
			// 
			this.NWWorkspaceRootTextBox.Location = new System.Drawing.Point(161, 149);
			this.NWWorkspaceRootTextBox.Name = "NWWorkspaceRootTextBox";
			this.NWWorkspaceRootTextBox.ReadOnly = true;
			this.NWWorkspaceRootTextBox.Size = new System.Drawing.Size(384, 23);
			this.NWWorkspaceRootTextBox.TabIndex = 15;
			this.NWWorkspaceRootTextBox.TabStop = false;
			this.NWWorkspaceRootTextBox.TextChanged += new System.EventHandler(this.NWWorkspaceRootTextBox_TextChanged);
			// 
			// label7
			// 
			this.label7.AutoSize = true;
			this.label7.Location = new System.Drawing.Point(37, 152);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(101, 16);
			this.label7.TabIndex = 17;
			this.label7.Text = "Workspace Root";
			// 
			// label6
			// 
			this.label6.AutoSize = true;
			this.label6.Location = new System.Drawing.Point(37, 116);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(108, 16);
			this.label6.TabIndex = 16;
			this.label6.Text = "Workspace Name";
			// 
			// NWWorkspaceNameTextBox
			// 
			this.NWWorkspaceNameTextBox.Location = new System.Drawing.Point(161, 113);
			this.NWWorkspaceNameTextBox.Name = "NWWorkspaceNameTextBox";
			this.NWWorkspaceNameTextBox.Size = new System.Drawing.Size(384, 23);
			this.NWWorkspaceNameTextBox.TabIndex = 18;
			this.NWWorkspaceNameTextBox.TextChanged += new System.EventHandler(this.NWWorkspaceNameTextBox_TextChanged);
			// 
			// NWDescriptionTextBox
			// 
			this.NWDescriptionTextBox.BackColor = System.Drawing.SystemColors.Control;
			this.NWDescriptionTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
			this.NWDescriptionTextBox.Location = new System.Drawing.Point(14, 40);
			this.NWDescriptionTextBox.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
			this.NWDescriptionTextBox.Name = "NWDescriptionTextBox";
			this.NWDescriptionTextBox.ReadOnly = true;
			this.NWDescriptionTextBox.Size = new System.Drawing.Size(668, 65);
			this.NWDescriptionTextBox.TabIndex = 19;
			this.NWDescriptionTextBox.TabStop = false;
			this.NWDescriptionTextBox.Text = resources.GetString("NWDescriptionTextBox.Text");
			// 
			// NWWorkspaceNameErrLabel
			// 
			this.NWWorkspaceNameErrLabel.AutoSize = true;
			this.NWWorkspaceNameErrLabel.ForeColor = System.Drawing.Color.Firebrick;
			this.NWWorkspaceNameErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
			this.NWWorkspaceNameErrLabel.Location = new System.Drawing.Point(149, 116);
			this.NWWorkspaceNameErrLabel.Name = "NWWorkspaceNameErrLabel";
			this.NWWorkspaceNameErrLabel.Size = new System.Drawing.Size(12, 16);
			this.NWWorkspaceNameErrLabel.TabIndex = 20;
			this.NWWorkspaceNameErrLabel.Text = " ";
			// 
			// NWWorkspaceRootErrLabel
			// 
			this.NWWorkspaceRootErrLabel.AutoSize = true;
			this.NWWorkspaceRootErrLabel.ForeColor = System.Drawing.Color.Firebrick;
			this.NWWorkspaceRootErrLabel.Image = global::P4PopulateDepot.Properties.Resources.red_arrow_right;
			this.NWWorkspaceRootErrLabel.Location = new System.Drawing.Point(149, 152);
			this.NWWorkspaceRootErrLabel.Name = "NWWorkspaceRootErrLabel";
			this.NWWorkspaceRootErrLabel.Size = new System.Drawing.Size(12, 16);
			this.NWWorkspaceRootErrLabel.TabIndex = 21;
			this.NWWorkspaceRootErrLabel.Text = " ";
			// 
			// NWMainErrorDescription
			// 
			this.NWMainErrorDescription.ForeColor = System.Drawing.Color.Firebrick;
			this.NWMainErrorDescription.Location = new System.Drawing.Point(40, 201);
			this.NWMainErrorDescription.Name = "NWMainErrorDescription";
			this.NWMainErrorDescription.Size = new System.Drawing.Size(592, 18);
			this.NWMainErrorDescription.TabIndex = 22;
			this.NWMainErrorDescription.Text = "Error Text";
			// 
			// NewWorkspaceDialog
			// 
			this.AcceptButton = this.WSNewOKButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.WSNewCancelButton;
			this.ClientSize = new System.Drawing.Size(694, 292);
			this.Controls.Add(this.NWMainErrorDescription);
			this.Controls.Add(this.NWWorkspaceRootErrLabel);
			this.Controls.Add(this.NWWorkspaceNameErrLabel);
			this.Controls.Add(this.NWDescriptionTextBox);
			this.Controls.Add(this.NWWorkspaceNameTextBox);
			this.Controls.Add(this.label7);
			this.Controls.Add(this.label6);
			this.Controls.Add(this.NWWorkspaceRootTextBox);
			this.Controls.Add(this.ConnectionWorkspaceBrowseButton);
			this.Controls.Add(this.FooterLine);
			this.Controls.Add(this.WSNewOKButton);
			this.Controls.Add(this.WSNewCancelButton);
			this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
			this.Name = "NewWorkspaceDialog";
			this.Text = "New Workspace";
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label FooterLine;
		private System.Windows.Forms.Button WSNewOKButton;
		private System.Windows.Forms.Button WSNewCancelButton;
		private System.Windows.Forms.Button ConnectionWorkspaceBrowseButton;
		private System.Windows.Forms.TextBox NWWorkspaceRootTextBox;
		private System.Windows.Forms.Label label7;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.TextBox NWWorkspaceNameTextBox;
		private System.Windows.Forms.RichTextBox NWDescriptionTextBox;
		private System.Windows.Forms.Label NWWorkspaceNameErrLabel;
		private System.Windows.Forms.Label NWWorkspaceRootErrLabel;
		private System.Windows.Forms.Label NWMainErrorDescription;
	}
}