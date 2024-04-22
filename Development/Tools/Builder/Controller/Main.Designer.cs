// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace Controller
{
    partial class Main
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
			this.components = new System.ComponentModel.Container();
			this.MainLogWindow = new UnrealControls.OutputWindowView();
			this.SuspendLayout();
			// 
			// MainLogWindow
			// 
			this.MainLogWindow.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.MainLogWindow.AutoScroll = true;
			this.MainLogWindow.BackColor = System.Drawing.SystemColors.Window;
			this.MainLogWindow.Cursor = System.Windows.Forms.Cursors.Arrow;
			this.MainLogWindow.Document = null;
			this.MainLogWindow.FindTextBackColor = System.Drawing.Color.Yellow;
			this.MainLogWindow.FindTextForeColor = System.Drawing.Color.Black;
			this.MainLogWindow.FindTextLineHighlight = System.Drawing.Color.FromArgb( ( ( int )( ( ( byte )( 239 ) ) ) ), ( ( int )( ( ( byte )( 248 ) ) ) ), ( ( int )( ( ( byte )( 255 ) ) ) ) );
			this.MainLogWindow.Font = new System.Drawing.Font( "Courier New", 9F );
			this.MainLogWindow.ForeColor = System.Drawing.SystemColors.WindowText;
			this.MainLogWindow.Location = new System.Drawing.Point( 2, 3 );
			this.MainLogWindow.Name = "MainLogWindow";
			this.MainLogWindow.Size = new System.Drawing.Size( 1225, 442 );
			this.MainLogWindow.TabIndex = 3;
			// 
			// Main
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 1227, 444 );
			this.Controls.Add( this.MainLogWindow );
			this.Font = new System.Drawing.Font( "Consolas", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.MinimumSize = new System.Drawing.Size( 277, 225 );
			this.Name = "Main";
			this.Text = "Build Controller";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler( this.Main_FormClosed );
			this.ResumeLayout( false );

        }

        #endregion

		private UnrealControls.OutputWindowView MainLogWindow;



	}
}

