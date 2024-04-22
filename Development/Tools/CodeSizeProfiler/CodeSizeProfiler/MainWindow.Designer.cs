// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace CodeSizeProfiler
{
    partial class MainWindow
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
			this.FunctionTreeView = new System.Windows.Forms.TreeView();
			this.OpenButton = new System.Windows.Forms.Button();
			this.RequiresUndecorationCheckbox = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// FunctionTreeView
			// 
			this.FunctionTreeView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
						| System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.FunctionTreeView.Font = new System.Drawing.Font("Tahoma", 8F);
			this.FunctionTreeView.Location = new System.Drawing.Point(1, 39);
			this.FunctionTreeView.Name = "FunctionTreeView";
			this.FunctionTreeView.Size = new System.Drawing.Size(1188, 821);
			this.FunctionTreeView.TabIndex = 0;
			// 
			// OpenButton
			// 
			this.OpenButton.Location = new System.Drawing.Point(1, 10);
			this.OpenButton.Name = "OpenButton";
			this.OpenButton.Size = new System.Drawing.Size(190, 23);
			this.OpenButton.TabIndex = 1;
			this.OpenButton.Text = "Open MAP file";
			this.OpenButton.UseVisualStyleBackColor = true;
			this.OpenButton.Click += new System.EventHandler(this.OpenButton_Click);
			// 
			// RequiresUndecorationCheckbox
			// 
			this.RequiresUndecorationCheckbox.AutoSize = true;
			this.RequiresUndecorationCheckbox.Location = new System.Drawing.Point(197, 10);
			this.RequiresUndecorationCheckbox.Name = "RequiresUndecorationCheckbox";
			this.RequiresUndecorationCheckbox.Size = new System.Drawing.Size(133, 17);
			this.RequiresUndecorationCheckbox.TabIndex = 2;
			this.RequiresUndecorationCheckbox.Text = "Requires undecoration";
			this.RequiresUndecorationCheckbox.UseVisualStyleBackColor = true;
			// 
			// MainWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1190, 861);
			this.Controls.Add(this.RequiresUndecorationCheckbox);
			this.Controls.Add(this.OpenButton);
			this.Controls.Add(this.FunctionTreeView);
			this.Name = "MainWindow";
			this.Text = "CodeSizeProfiler";
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TreeView FunctionTreeView;
        private System.Windows.Forms.Button OpenButton;
		private System.Windows.Forms.CheckBox RequiresUndecorationCheckbox;
    }
}

