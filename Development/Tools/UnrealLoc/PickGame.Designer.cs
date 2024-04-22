/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnrealLoc
{
    partial class PickGame
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
			this.Label_PickGame_Title = new System.Windows.Forms.Label();
			this.SuspendLayout();
			// 
			// Label_PickGame_Title
			// 
			this.Label_PickGame_Title.Anchor = System.Windows.Forms.AnchorStyles.Top;
			this.Label_PickGame_Title.Font = new System.Drawing.Font( "Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.Label_PickGame_Title.Location = new System.Drawing.Point( -86, 9 );
			this.Label_PickGame_Title.Name = "Label_PickGame_Title";
			this.Label_PickGame_Title.Size = new System.Drawing.Size( 752, 33 );
			this.Label_PickGame_Title.TabIndex = 0;
			this.Label_PickGame_Title.Text = "Pick a game to process";
			this.Label_PickGame_Title.TextAlign = System.Drawing.ContentAlignment.TopCenter;
			// 
			// PickGame
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 581, 221 );
			this.Controls.Add( this.Label_PickGame_Title );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Name = "PickGame";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "PickGame";
			this.ResumeLayout( false );

        }

        #endregion

        private System.Windows.Forms.Label Label_PickGame_Title;
    }
}