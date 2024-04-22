/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnSetup
{
	partial class GameDialog
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
			this.GamePropertyGrid = new System.Windows.Forms.PropertyGrid();
			this.GameSettingsOKButton = new System.Windows.Forms.Button();
			this.GameSettingsCancelButton = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// GamePropertyGrid
			// 
			this.GamePropertyGrid.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GamePropertyGrid.CommandsDisabledLinkColor = System.Drawing.Color.FromArgb( ( ( int )( ( ( byte )( 96 ) ) ) ), ( ( int )( ( ( byte )( 96 ) ) ) ), ( ( int )( ( ( byte )( 96 ) ) ) ) );
			this.GamePropertyGrid.Location = new System.Drawing.Point( 14, 13 );
			this.GamePropertyGrid.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.GamePropertyGrid.Name = "GamePropertyGrid";
			this.GamePropertyGrid.PropertySort = System.Windows.Forms.PropertySort.Alphabetical;
			this.GamePropertyGrid.Size = new System.Drawing.Size( 546, 282 );
			this.GamePropertyGrid.TabIndex = 0;
			// 
			// GameSettingsOKButton
			// 
			this.GameSettingsOKButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameSettingsOKButton.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.GameSettingsOKButton.Location = new System.Drawing.Point( 413, 303 );
			this.GameSettingsOKButton.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.GameSettingsOKButton.Name = "GameSettingsOKButton";
			this.GameSettingsOKButton.Size = new System.Drawing.Size( 147, 55 );
			this.GameSettingsOKButton.TabIndex = 3;
			this.GameSettingsOKButton.Text = "Package Game";
			this.GameSettingsOKButton.UseVisualStyleBackColor = true;
			this.GameSettingsOKButton.Click += new System.EventHandler( this.GameDialogOKButton );
			// 
			// GameSettingsCancelButton
			// 
			this.GameSettingsCancelButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.GameSettingsCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.GameSettingsCancelButton.Location = new System.Drawing.Point( 14, 330 );
			this.GameSettingsCancelButton.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.GameSettingsCancelButton.Name = "GameSettingsCancelButton";
			this.GameSettingsCancelButton.Size = new System.Drawing.Size( 87, 28 );
			this.GameSettingsCancelButton.TabIndex = 5;
			this.GameSettingsCancelButton.Text = "Cancel";
			this.GameSettingsCancelButton.UseVisualStyleBackColor = true;
			this.GameSettingsCancelButton.Click += new System.EventHandler( this.GameSettingsCancelClick );
			// 
			// GameDialog
			// 
			this.AcceptButton = this.GameSettingsOKButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.GameSettingsCancelButton;
			this.ClientSize = new System.Drawing.Size( 574, 373 );
			this.Controls.Add( this.GameSettingsCancelButton );
			this.Controls.Add( this.GameSettingsOKButton );
			this.Controls.Add( this.GamePropertyGrid );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MinimumSize = new System.Drawing.Size( 524, 400 );
			this.Name = "GameDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Game Settings";
			this.Load += new System.EventHandler( this.OnLoad );
			this.ResumeLayout( false );

		}

		#endregion

		private System.Windows.Forms.PropertyGrid GamePropertyGrid;
		private System.Windows.Forms.Button GameSettingsOKButton;
		private System.Windows.Forms.Button GameSettingsCancelButton;
	}
}