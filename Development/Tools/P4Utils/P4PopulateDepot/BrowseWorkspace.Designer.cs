namespace P4PopulateDepot
{
	partial class BrowseWorkspaceDialog
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BrowseWorkspaceDialog));
            this.WSBrowseDescriptionTextBox = new System.Windows.Forms.RichTextBox();
            this.FooterLine = new System.Windows.Forms.Label();
            this.WSBrowseOKButton = new System.Windows.Forms.Button();
            this.WSBrowseCancelButton = new System.Windows.Forms.Button();
            this.ShowAllCheckbox = new System.Windows.Forms.CheckBox();
            this.WSBrowseListViewAlwaysSel = new P4PopulateDepot.ListViewAlwaysSelected();
            this.SuspendLayout();
            // 
            // WSBrowseDescriptionTextBox
            // 
            this.WSBrowseDescriptionTextBox.BackColor = System.Drawing.SystemColors.Control;
            this.WSBrowseDescriptionTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.WSBrowseDescriptionTextBox.Location = new System.Drawing.Point(14, 20);
            this.WSBrowseDescriptionTextBox.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.WSBrowseDescriptionTextBox.Name = "WSBrowseDescriptionTextBox";
            this.WSBrowseDescriptionTextBox.ReadOnly = true;
            this.WSBrowseDescriptionTextBox.Size = new System.Drawing.Size(668, 23);
            this.WSBrowseDescriptionTextBox.TabIndex = 17;
            this.WSBrowseDescriptionTextBox.TabStop = false;
            this.WSBrowseDescriptionTextBox.Text = "Select an available workspace below. ";
            // 
            // FooterLine
            // 
            this.FooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.FooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.FooterLine.Location = new System.Drawing.Point(-6, 238);
            this.FooterLine.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.FooterLine.Name = "FooterLine";
            this.FooterLine.Size = new System.Drawing.Size(705, 2);
            this.FooterLine.TabIndex = 20;
            // 
            // WSBrowseOKButton
            // 
            this.WSBrowseOKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.WSBrowseOKButton.AutoSize = true;
            this.WSBrowseOKButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.WSBrowseOKButton.Location = new System.Drawing.Point(476, 246);
            this.WSBrowseOKButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.WSBrowseOKButton.Name = "WSBrowseOKButton";
            this.WSBrowseOKButton.Size = new System.Drawing.Size(100, 32);
            this.WSBrowseOKButton.TabIndex = 2;
            this.WSBrowseOKButton.Tag = "";
            this.WSBrowseOKButton.Text = "OK";
            this.WSBrowseOKButton.UseVisualStyleBackColor = true;
            this.WSBrowseOKButton.Click += new System.EventHandler(this.WSBrowseOKButton_Click);
            // 
            // WSBrowseCancelButton
            // 
            this.WSBrowseCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.WSBrowseCancelButton.AutoSize = true;
            this.WSBrowseCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.WSBrowseCancelButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.WSBrowseCancelButton.Location = new System.Drawing.Point(582, 246);
            this.WSBrowseCancelButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.WSBrowseCancelButton.Name = "WSBrowseCancelButton";
            this.WSBrowseCancelButton.Size = new System.Drawing.Size(100, 32);
            this.WSBrowseCancelButton.TabIndex = 3;
            this.WSBrowseCancelButton.Tag = "";
            this.WSBrowseCancelButton.Text = "Cancel";
            this.WSBrowseCancelButton.UseVisualStyleBackColor = true;
            // 
            // ShowAllCheckbox
            // 
            this.ShowAllCheckbox.AutoSize = true;
            this.ShowAllCheckbox.Location = new System.Drawing.Point(593, 19);
            this.ShowAllCheckbox.Name = "ShowAllCheckbox";
            this.ShowAllCheckbox.Size = new System.Drawing.Size(77, 20);
            this.ShowAllCheckbox.TabIndex = 21;
            this.ShowAllCheckbox.Text = "Show All";
            this.ShowAllCheckbox.UseVisualStyleBackColor = true;
            this.ShowAllCheckbox.CheckedChanged += new System.EventHandler(this.ShowAllCheckbox_CheckedChanged);
            // 
            // WSBrowseListViewAlwaysSel
            // 
            this.WSBrowseListViewAlwaysSel.FullRowSelect = true;
            this.WSBrowseListViewAlwaysSel.GridLines = true;
            this.WSBrowseListViewAlwaysSel.HideSelection = false;
            this.WSBrowseListViewAlwaysSel.Location = new System.Drawing.Point(14, 49);
            this.WSBrowseListViewAlwaysSel.MultiSelect = false;
            this.WSBrowseListViewAlwaysSel.Name = "WSBrowseListViewAlwaysSel";
            this.WSBrowseListViewAlwaysSel.Size = new System.Drawing.Size(668, 179);
            this.WSBrowseListViewAlwaysSel.TabIndex = 1;
            this.WSBrowseListViewAlwaysSel.UseCompatibleStateImageBehavior = false;
            this.WSBrowseListViewAlwaysSel.View = System.Windows.Forms.View.Details;
            this.WSBrowseListViewAlwaysSel.SelectedIndexChanged += new System.EventHandler(this.WSBrowseListViewAlwaysSel_SelectedIndexChanged);
            this.WSBrowseListViewAlwaysSel.DoubleClick += new System.EventHandler(this.WSBrowseListViewAlwaysSel_DoubleClick);
            // 
            // BrowseWorkspaceDialog
            // 
            this.AcceptButton = this.WSBrowseOKButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.WSBrowseCancelButton;
            this.ClientSize = new System.Drawing.Size(694, 292);
            this.Controls.Add(this.ShowAllCheckbox);
            this.Controls.Add(this.WSBrowseListViewAlwaysSel);
            this.Controls.Add(this.FooterLine);
            this.Controls.Add(this.WSBrowseOKButton);
            this.Controls.Add(this.WSBrowseCancelButton);
            this.Controls.Add(this.WSBrowseDescriptionTextBox);
            this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.Name = "BrowseWorkspaceDialog";
            this.Text = "Browse Workspace";
            this.ResumeLayout(false);
            this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.RichTextBox WSBrowseDescriptionTextBox;
		private System.Windows.Forms.Label FooterLine;
		private System.Windows.Forms.Button WSBrowseOKButton;
		private System.Windows.Forms.Button WSBrowseCancelButton;
		private ListViewAlwaysSelected WSBrowseListViewAlwaysSel;
		private System.Windows.Forms.CheckBox ShowAllCheckbox;
	}
}