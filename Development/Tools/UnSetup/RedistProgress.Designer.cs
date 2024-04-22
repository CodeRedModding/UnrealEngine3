/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class RedistProgress
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
			this.LabelPleaseWait = new System.Windows.Forms.Label();
			this.LabelDescription = new System.Windows.Forms.Label();
			this.LabelDetail = new System.Windows.Forms.Label();
			this.RedistAnimatedPictureBox = new System.Windows.Forms.PictureBox();
			this.RedistProgressHeaderLine = new System.Windows.Forms.Label();
			( ( System.ComponentModel.ISupportInitialize )( this.RedistAnimatedPictureBox ) ).BeginInit();
			this.SuspendLayout();
			// 
			// LabelPleaseWait
			// 
			this.LabelPleaseWait.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.LabelPleaseWait.BackColor = System.Drawing.Color.White;
			this.LabelPleaseWait.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.LabelPleaseWait.Location = new System.Drawing.Point( -3, 0 );
			this.LabelPleaseWait.Name = "LabelPleaseWait";
			this.LabelPleaseWait.Size = new System.Drawing.Size( 800, 68 );
			this.LabelPleaseWait.TabIndex = 0;
			this.LabelPleaseWait.Text = "Please Wait";
			this.LabelPleaseWait.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// LabelDescription
			// 
			this.LabelDescription.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.LabelDescription.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.LabelDescription.Location = new System.Drawing.Point( 14, 81 );
			this.LabelDescription.Name = "LabelDescription";
			this.LabelDescription.Size = new System.Drawing.Size( 770, 28 );
			this.LabelDescription.TabIndex = 2;
			this.LabelDescription.Text = "Description";
			this.LabelDescription.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// LabelDetail
			// 
			this.LabelDetail.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.LabelDetail.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.LabelDetail.Location = new System.Drawing.Point( 12, 121 );
			this.LabelDetail.Name = "LabelDetail";
			this.LabelDetail.Size = new System.Drawing.Size( 772, 76 );
			this.LabelDetail.TabIndex = 3;
			this.LabelDetail.Text = "Details";
			// 
			// RedistAnimatedPictureBox
			// 
			this.RedistAnimatedPictureBox.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.RedistAnimatedPictureBox.ErrorImage = null;
			this.RedistAnimatedPictureBox.InitialImage = null;
			this.RedistAnimatedPictureBox.Location = new System.Drawing.Point( 12, 200 );
			this.RedistAnimatedPictureBox.Name = "RedistAnimatedPictureBox";
			this.RedistAnimatedPictureBox.Size = new System.Drawing.Size( 772, 86 );
			this.RedistAnimatedPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
			this.RedistAnimatedPictureBox.TabIndex = 0;
			this.RedistAnimatedPictureBox.TabStop = false;
			// 
			// RedistProgressHeaderLine
			// 
			this.RedistProgressHeaderLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.RedistProgressHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.RedistProgressHeaderLine.Location = new System.Drawing.Point( -3, 68 );
			this.RedistProgressHeaderLine.Name = "RedistProgressHeaderLine";
			this.RedistProgressHeaderLine.Size = new System.Drawing.Size( 800, 2 );
			this.RedistProgressHeaderLine.TabIndex = 4;
			// 
			// RedistProgress
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 794, 298 );
			this.ControlBox = false;
			this.Controls.Add( this.RedistProgressHeaderLine );
			this.Controls.Add( this.RedistAnimatedPictureBox );
			this.Controls.Add( this.LabelDetail );
			this.Controls.Add( this.LabelDescription );
			this.Controls.Add( this.LabelPleaseWait );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnSetup.Properties.Resources.UE3Redist;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "RedistProgress";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UDK Redistributable Install Progress";
			this.Load += new System.EventHandler( this.OnLoad );
			( ( System.ComponentModel.ISupportInitialize )( this.RedistAnimatedPictureBox ) ).EndInit();
			this.ResumeLayout( false );

		}

		#endregion

		private System.Windows.Forms.Label LabelPleaseWait;
		public System.Windows.Forms.Label LabelDescription;
		public System.Windows.Forms.Label LabelDetail;
		private System.Windows.Forms.PictureBox RedistAnimatedPictureBox;
		private System.Windows.Forms.Label RedistProgressHeaderLine;
	}
}