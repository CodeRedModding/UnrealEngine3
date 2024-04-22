namespace P4PopulateDepot
{
    partial class BrowseUserDialog
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BrowseUserDialog));
            this.BUOKButton = new System.Windows.Forms.Button();
            this.BUCancelButton = new System.Windows.Forms.Button();
            this.BUBrowseDescriptionTextBox = new System.Windows.Forms.RichTextBox();
            this.FooterLine = new System.Windows.Forms.Label();
            this.BUListViewAlwaysSel = new P4PopulateDepot.ListViewAlwaysSelected();
            this.SuspendLayout();
            // 
            // BUOKButton
            // 
            this.BUOKButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.BUOKButton.AutoSize = true;
            this.BUOKButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.BUOKButton.Location = new System.Drawing.Point(560, 248);
            this.BUOKButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.BUOKButton.Name = "BUOKButton";
            this.BUOKButton.Size = new System.Drawing.Size(100, 32);
            this.BUOKButton.TabIndex = 19;
            this.BUOKButton.Tag = "";
            this.BUOKButton.Text = "OK";
            this.BUOKButton.UseVisualStyleBackColor = true;
            this.BUOKButton.Click += new System.EventHandler(this.BUOKButton_Click);
            // 
            // BUCancelButton
            // 
            this.BUCancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.BUCancelButton.AutoSize = true;
            this.BUCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.BUCancelButton.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.BUCancelButton.Location = new System.Drawing.Point(666, 248);
            this.BUCancelButton.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.BUCancelButton.Name = "BUCancelButton";
            this.BUCancelButton.Size = new System.Drawing.Size(100, 32);
            this.BUCancelButton.TabIndex = 20;
            this.BUCancelButton.Tag = "";
            this.BUCancelButton.Text = "Cancel";
            this.BUCancelButton.UseVisualStyleBackColor = true;
            // 
            // BUBrowseDescriptionTextBox
            // 
            this.BUBrowseDescriptionTextBox.BackColor = System.Drawing.SystemColors.Control;
            this.BUBrowseDescriptionTextBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.BUBrowseDescriptionTextBox.Location = new System.Drawing.Point(14, 20);
            this.BUBrowseDescriptionTextBox.Margin = new System.Windows.Forms.Padding(3, 5, 3, 5);
            this.BUBrowseDescriptionTextBox.Name = "BUBrowseDescriptionTextBox";
            this.BUBrowseDescriptionTextBox.ReadOnly = true;
            this.BUBrowseDescriptionTextBox.Size = new System.Drawing.Size(668, 23);
            this.BUBrowseDescriptionTextBox.TabIndex = 21;
            this.BUBrowseDescriptionTextBox.TabStop = false;
            this.BUBrowseDescriptionTextBox.Text = "Select an available workspace below. ";
            // 
            // FooterLine
            // 
            this.FooterLine.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.FooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.FooterLine.Location = new System.Drawing.Point(-6, 238);
            this.FooterLine.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.FooterLine.Name = "FooterLine";
            this.FooterLine.Size = new System.Drawing.Size(795, 2);
            this.FooterLine.TabIndex = 22;
            // 
            // BUListViewAlwaysSel
            // 
            this.BUListViewAlwaysSel.FullRowSelect = true;
            this.BUListViewAlwaysSel.GridLines = true;
            this.BUListViewAlwaysSel.HideSelection = false;
            this.BUListViewAlwaysSel.Location = new System.Drawing.Point(14, 49);
            this.BUListViewAlwaysSel.MultiSelect = false;
            this.BUListViewAlwaysSel.Name = "BUListViewAlwaysSel";
            this.BUListViewAlwaysSel.Size = new System.Drawing.Size(748, 179);
            this.BUListViewAlwaysSel.TabIndex = 18;
            this.BUListViewAlwaysSel.UseCompatibleStateImageBehavior = false;
            this.BUListViewAlwaysSel.View = System.Windows.Forms.View.Details;
            this.BUListViewAlwaysSel.SelectedIndexChanged += new System.EventHandler(this.BUListViewAlwaysSel_SelectedIndexChanged);
            this.BUListViewAlwaysSel.DoubleClick += new System.EventHandler(this.BUListViewAlwaysSel_DoubleClick);
            // 
            // BrowseUserDialog
            // 
            this.AcceptButton = this.BUOKButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.BUCancelButton;
            this.ClientSize = new System.Drawing.Size(774, 292);
            this.Controls.Add(this.FooterLine);
            this.Controls.Add(this.BUListViewAlwaysSel);
            this.Controls.Add(this.BUOKButton);
            this.Controls.Add(this.BUCancelButton);
            this.Controls.Add(this.BUBrowseDescriptionTextBox);
            this.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.Name = "BrowseUserDialog";
            this.Text = "Browse Users";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private ListViewAlwaysSelected BUListViewAlwaysSel;
        private System.Windows.Forms.Button BUOKButton;
        private System.Windows.Forms.Button BUCancelButton;
        private System.Windows.Forms.RichTextBox BUBrowseDescriptionTextBox;
        private System.Windows.Forms.Label FooterLine;
    }
}