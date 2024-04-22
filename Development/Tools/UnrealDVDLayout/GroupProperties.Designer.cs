/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnrealDVDLayout
{
    partial class GroupProperties
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
			this.GroupPropListView = new System.Windows.Forms.ListView();
			this.PathColumnHeader = new System.Windows.Forms.ColumnHeader();
			this.SizeColumnHeader = new System.Windows.Forms.ColumnHeader();
			this.RegularExpressionTextBox = new System.Windows.Forms.TextBox();
			this.GroupProp_RegExpLabel = new System.Windows.Forms.Label();
			this.GroupPropOKButton = new System.Windows.Forms.Button();
			this.GroupPropNameLabel = new System.Windows.Forms.Label();
			this.GroupPropSortTypeCombo = new System.Windows.Forms.ComboBox();
			this.GroupPropSortLabel = new System.Windows.Forms.Label();
			this.GroupPropCancelButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// GroupPropListView
			// 
			this.GroupPropListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.PathColumnHeader,
            this.SizeColumnHeader});
			this.GroupPropListView.Location = new System.Drawing.Point(422, 12);
			this.GroupPropListView.Name = "GroupPropListView";
			this.GroupPropListView.Size = new System.Drawing.Size(516, 544);
			this.GroupPropListView.TabIndex = 0;
			this.GroupPropListView.UseCompatibleStateImageBehavior = false;
			this.GroupPropListView.View = System.Windows.Forms.View.Details;
			// 
			// PathColumnHeader
			// 
			this.PathColumnHeader.Text = "Path";
			this.PathColumnHeader.Width = 348;
			// 
			// SizeColumnHeader
			// 
			this.SizeColumnHeader.Text = "Size (MB)";
			this.SizeColumnHeader.Width = 164;
			// 
			// RegularExpressionTextBox
			// 
			this.RegularExpressionTextBox.Location = new System.Drawing.Point(12, 67);
			this.RegularExpressionTextBox.Name = "RegularExpressionTextBox";
			this.RegularExpressionTextBox.Size = new System.Drawing.Size(400, 20);
			this.RegularExpressionTextBox.TabIndex = 2;
			// 
			// GroupProp_RegExpLabel
			// 
			this.GroupProp_RegExpLabel.AutoSize = true;
			this.GroupProp_RegExpLabel.Location = new System.Drawing.Point(12, 45);
			this.GroupProp_RegExpLabel.Name = "GroupProp_RegExpLabel";
			this.GroupProp_RegExpLabel.Size = new System.Drawing.Size(101, 13);
			this.GroupProp_RegExpLabel.TabIndex = 3;
			this.GroupProp_RegExpLabel.Text = "Regular Expression:";
			// 
			// GroupPropOKButton
			// 
			this.GroupPropOKButton.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.GroupPropOKButton.Location = new System.Drawing.Point(15, 533);
			this.GroupPropOKButton.Name = "GroupPropOKButton";
			this.GroupPropOKButton.Size = new System.Drawing.Size(75, 23);
			this.GroupPropOKButton.TabIndex = 4;
			this.GroupPropOKButton.Text = "OK";
			this.GroupPropOKButton.UseVisualStyleBackColor = true;
			// 
			// GroupPropNameLabel
			// 
			this.GroupPropNameLabel.AutoSize = true;
			this.GroupPropNameLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.GroupPropNameLabel.Location = new System.Drawing.Point(9, 12);
			this.GroupPropNameLabel.Name = "GroupPropNameLabel";
			this.GroupPropNameLabel.Size = new System.Drawing.Size(96, 15);
			this.GroupPropNameLabel.TabIndex = 5;
			this.GroupPropNameLabel.Text = "Group Name: ";
			// 
			// GroupPropSortTypeCombo
			// 
			this.GroupPropSortTypeCombo.FormattingEnabled = true;
			this.GroupPropSortTypeCombo.Location = new System.Drawing.Point(171, 107);
			this.GroupPropSortTypeCombo.Name = "GroupPropSortTypeCombo";
			this.GroupPropSortTypeCombo.Size = new System.Drawing.Size(241, 21);
			this.GroupPropSortTypeCombo.TabIndex = 6;
			this.GroupPropSortTypeCombo.SelectedIndexChanged += new System.EventHandler(this.GroupPropSortType_Changed);
			// 
			// GroupPropSortLabel
			// 
			this.GroupPropSortLabel.AutoSize = true;
			this.GroupPropSortLabel.Location = new System.Drawing.Point(12, 110);
			this.GroupPropSortLabel.Name = "GroupPropSortLabel";
			this.GroupPropSortLabel.Size = new System.Drawing.Size(56, 13);
			this.GroupPropSortLabel.TabIndex = 7;
			this.GroupPropSortLabel.Text = "Sort Type:";
			// 
			// GroupPropCancelButton
			// 
			this.GroupPropCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.GroupPropCancelButton.Location = new System.Drawing.Point(115, 533);
			this.GroupPropCancelButton.Name = "GroupPropCancelButton";
			this.GroupPropCancelButton.Size = new System.Drawing.Size(75, 23);
			this.GroupPropCancelButton.TabIndex = 8;
			this.GroupPropCancelButton.Text = "Cancel";
			this.GroupPropCancelButton.UseVisualStyleBackColor = true;
			// 
			// GroupProperties
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.GroupPropCancelButton;
			this.ClientSize = new System.Drawing.Size(950, 568);
			this.ControlBox = false;
			this.Controls.Add(this.GroupPropCancelButton);
			this.Controls.Add(this.GroupPropSortLabel);
			this.Controls.Add(this.GroupPropSortTypeCombo);
			this.Controls.Add(this.GroupPropNameLabel);
			this.Controls.Add(this.GroupPropOKButton);
			this.Controls.Add(this.GroupProp_RegExpLabel);
			this.Controls.Add(this.RegularExpressionTextBox);
			this.Controls.Add(this.GroupPropListView);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Fixed3D;
			this.Name = "GroupProperties";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "GroupProperties";
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListView GroupPropListView;
        private System.Windows.Forms.TextBox RegularExpressionTextBox;
        private System.Windows.Forms.Label GroupProp_RegExpLabel;
        private System.Windows.Forms.Button GroupPropOKButton;
        private System.Windows.Forms.Label GroupPropNameLabel;
        private System.Windows.Forms.ColumnHeader PathColumnHeader;
        private System.Windows.Forms.ColumnHeader SizeColumnHeader;
        private System.Windows.Forms.ComboBox GroupPropSortTypeCombo;
        private System.Windows.Forms.Label GroupPropSortLabel;
        private System.Windows.Forms.Button GroupPropCancelButton;
    }
}