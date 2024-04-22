/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnrealDVDLayout
{
    partial class OptionsDialog
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
            this.Label_OptionsTitle = new System.Windows.Forms.Label();
            this.OptionsGrid = new System.Windows.Forms.PropertyGrid();
            this.SuspendLayout();
            // 
            // Label_OptionsTitle
            // 
            this.Label_OptionsTitle.AutoSize = true;
            this.Label_OptionsTitle.Location = new System.Drawing.Point( 12, 9 );
            this.Label_OptionsTitle.Name = "Label_OptionsTitle";
            this.Label_OptionsTitle.Size = new System.Drawing.Size( 128, 13 );
            this.Label_OptionsTitle.TabIndex = 1;
            this.Label_OptionsTitle.Text = "Set your preferred options";
            // 
            // OptionsGrid
            // 
            this.OptionsGrid.Location = new System.Drawing.Point( 15, 25 );
            this.OptionsGrid.Name = "OptionsGrid";
            this.OptionsGrid.Size = new System.Drawing.Size( 509, 682 );
            this.OptionsGrid.TabIndex = 2;
            // 
            // OptionsDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size( 536, 719 );
            this.Controls.Add( this.OptionsGrid );
            this.Controls.Add( this.Label_OptionsTitle );
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Name = "OptionsDialog";
            this.Text = "Options";
            this.ResumeLayout( false );
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label Label_OptionsTitle;
        private System.Windows.Forms.PropertyGrid OptionsGrid;
    }
}