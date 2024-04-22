namespace UnSetup
{
	partial class UninstallProgress
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
			this.UninstallProgressTitle = new System.Windows.Forms.Label();
			this.UninstallAnimatedPictureBox = new System.Windows.Forms.PictureBox();
			this.UninstallProgressHeaderLine = new System.Windows.Forms.Label();
			( ( System.ComponentModel.ISupportInitialize )( this.UninstallAnimatedPictureBox ) ).BeginInit();
			this.SuspendLayout();
			// 
			// UninstallProgressTitle
			// 
			this.UninstallProgressTitle.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UninstallProgressTitle.BackColor = System.Drawing.Color.White;
			this.UninstallProgressTitle.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.UninstallProgressTitle.Image = global::UnSetup.Properties.Resources.BannerImage;
			this.UninstallProgressTitle.Location = new System.Drawing.Point( -3, 0 );
			this.UninstallProgressTitle.Name = "UninstallProgressTitle";
			this.UninstallProgressTitle.Size = new System.Drawing.Size( 800, 68 );
			this.UninstallProgressTitle.TabIndex = 1;
			this.UninstallProgressTitle.Text = "Uninstalling";
			this.UninstallProgressTitle.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// UninstallAnimatedPictureBox
			// 
			this.UninstallAnimatedPictureBox.ErrorImage = null;
			this.UninstallAnimatedPictureBox.Image = global::UnSetup.Properties.Resources.Waiting;
			this.UninstallAnimatedPictureBox.InitialImage = null;
			this.UninstallAnimatedPictureBox.Location = new System.Drawing.Point( 12, 84 );
			this.UninstallAnimatedPictureBox.Name = "UninstallAnimatedPictureBox";
			this.UninstallAnimatedPictureBox.Size = new System.Drawing.Size( 770, 64 );
			this.UninstallAnimatedPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
			this.UninstallAnimatedPictureBox.TabIndex = 4;
			this.UninstallAnimatedPictureBox.TabStop = false;
			// 
			// UninstallProgressHeaderLine
			// 
			this.UninstallProgressHeaderLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.UninstallProgressHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.UninstallProgressHeaderLine.Location = new System.Drawing.Point( -3, 68 );
			this.UninstallProgressHeaderLine.Name = "UninstallProgressHeaderLine";
			this.UninstallProgressHeaderLine.Size = new System.Drawing.Size( 800, 2 );
			this.UninstallProgressHeaderLine.TabIndex = 5;
			// 
			// UninstallProgress
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 794, 161 );
			this.ControlBox = false;
			this.Controls.Add( this.UninstallProgressHeaderLine );
			this.Controls.Add( this.UninstallAnimatedPictureBox );
			this.Controls.Add( this.UninstallProgressTitle );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "UninstallProgress";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UDK Uninstall Progress";
			this.Load += new System.EventHandler( this.OnLoad );
			( ( System.ComponentModel.ISupportInitialize )( this.UninstallAnimatedPictureBox ) ).EndInit();
			this.ResumeLayout( false );

		}

		#endregion

		private System.Windows.Forms.Label UninstallProgressTitle;
		private System.Windows.Forms.PictureBox UninstallAnimatedPictureBox;
		private System.Windows.Forms.Label UninstallProgressHeaderLine;
	}
}