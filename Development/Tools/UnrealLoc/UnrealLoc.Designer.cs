/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnrealLoc
{
    partial class UnrealLoc
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
			this.components = new System.ComponentModel.Container();
			UnrealControls.OutputWindowDocument outputWindowDocument1 = new UnrealControls.OutputWindowDocument();
			this.Button_GenLocFiles = new System.Windows.Forms.Button();
			this.Button_ImportText = new System.Windows.Forms.Button();
			this.CheckBox_INT = new System.Windows.Forms.CheckBox();
			this.CheckBox_CHN = new System.Windows.Forms.CheckBox();
			this.CheckBox_KOR = new System.Windows.Forms.CheckBox();
			this.CheckBox_JPN = new System.Windows.Forms.CheckBox();
			this.CheckBox_PTB = new System.Windows.Forms.CheckBox();
			this.CheckBox_CZE = new System.Windows.Forms.CheckBox();
			this.CheckBox_HUN = new System.Windows.Forms.CheckBox();
			this.CheckBox_POL = new System.Windows.Forms.CheckBox();
			this.CheckBox_RUS = new System.Windows.Forms.CheckBox();
			this.CheckBox_ESM = new System.Windows.Forms.CheckBox();
			this.CheckBox_ESN = new System.Windows.Forms.CheckBox();
			this.CheckBox_DEU = new System.Windows.Forms.CheckBox();
			this.CheckBox_ITA = new System.Windows.Forms.CheckBox();
			this.CheckBox_FRA = new System.Windows.Forms.CheckBox();
			this.MainMenu = new System.Windows.Forms.MenuStrip();
			this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.QuitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.Button_GenDiffFiles = new System.Windows.Forms.Button();
			this.GenericFolderBrowser = new System.Windows.Forms.FolderBrowserDialog();
			this.Button_SaveComments = new System.Windows.Forms.Button();
			this.MainLogWindow = new UnrealControls.OutputWindowView();
			this.CheckBox_SLO = new System.Windows.Forms.CheckBox();
			this.CheckBox_XXX = new System.Windows.Forms.CheckBox();
			this.Button_ReloadChangedFiles = new System.Windows.Forms.Button();
			this.MainMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// Button_GenLocFiles
			// 
			this.Button_GenLocFiles.Location = new System.Drawing.Point( 15, 155 );
			this.Button_GenLocFiles.Name = "Button_GenLocFiles";
			this.Button_GenLocFiles.Size = new System.Drawing.Size( 200, 23 );
			this.Button_GenLocFiles.TabIndex = 4;
			this.Button_GenLocFiles.Text = "Update Game Localisation Data";
			this.Button_GenLocFiles.UseVisualStyleBackColor = true;
			this.Button_GenLocFiles.Click += new System.EventHandler( this.Button_GenLocFiles_Click );
			// 
			// Button_ImportText
			// 
			this.Button_ImportText.Location = new System.Drawing.Point( 15, 35 );
			this.Button_ImportText.Name = "Button_ImportText";
			this.Button_ImportText.Size = new System.Drawing.Size( 200, 23 );
			this.Button_ImportText.TabIndex = 5;
			this.Button_ImportText.Text = "Import Text";
			this.Button_ImportText.UseVisualStyleBackColor = true;
			this.Button_ImportText.Click += new System.EventHandler( this.Button_ImportText_Click );
			// 
			// CheckBox_INT
			// 
			this.CheckBox_INT.AutoSize = true;
			this.CheckBox_INT.Location = new System.Drawing.Point( 15, 206 );
			this.CheckBox_INT.Name = "CheckBox_INT";
			this.CheckBox_INT.Size = new System.Drawing.Size( 105, 17 );
			this.CheckBox_INT.TabIndex = 6;
			this.CheckBox_INT.Text = "INT (US English)";
			this.CheckBox_INT.UseVisualStyleBackColor = true;
			// 
			// CheckBox_CHN
			// 
			this.CheckBox_CHN.AutoSize = true;
			this.CheckBox_CHN.Location = new System.Drawing.Point( 15, 528 );
			this.CheckBox_CHN.Name = "CheckBox_CHN";
			this.CheckBox_CHN.Size = new System.Drawing.Size( 143, 17 );
			this.CheckBox_CHN.TabIndex = 7;
			this.CheckBox_CHN.Text = "CHN (Simplified Chinese)";
			this.CheckBox_CHN.UseVisualStyleBackColor = true;
			// 
			// CheckBox_KOR
			// 
			this.CheckBox_KOR.AutoSize = true;
			this.CheckBox_KOR.Location = new System.Drawing.Point( 15, 505 );
			this.CheckBox_KOR.Name = "CheckBox_KOR";
			this.CheckBox_KOR.Size = new System.Drawing.Size( 92, 17 );
			this.CheckBox_KOR.TabIndex = 8;
			this.CheckBox_KOR.Text = "KOR (Korean)";
			this.CheckBox_KOR.UseVisualStyleBackColor = true;
			// 
			// CheckBox_JPN
			// 
			this.CheckBox_JPN.AutoSize = true;
			this.CheckBox_JPN.Location = new System.Drawing.Point( 15, 482 );
			this.CheckBox_JPN.Name = "CheckBox_JPN";
			this.CheckBox_JPN.Size = new System.Drawing.Size( 101, 17 );
			this.CheckBox_JPN.TabIndex = 9;
			this.CheckBox_JPN.Text = "JPN (Japanese)";
			this.CheckBox_JPN.UseVisualStyleBackColor = true;
			// 
			// CheckBox_PTB
			// 
			this.CheckBox_PTB.AutoSize = true;
			this.CheckBox_PTB.Location = new System.Drawing.Point( 15, 344 );
			this.CheckBox_PTB.Name = "CheckBox_PTB";
			this.CheckBox_PTB.Size = new System.Drawing.Size( 152, 17 );
			this.CheckBox_PTB.TabIndex = 10;
			this.CheckBox_PTB.Text = "PTB (Brasilian Portuguese)";
			this.CheckBox_PTB.UseVisualStyleBackColor = true;
			// 
			// CheckBox_CZE
			// 
			this.CheckBox_CZE.AutoSize = true;
			this.CheckBox_CZE.Location = new System.Drawing.Point( 15, 436 );
			this.CheckBox_CZE.Name = "CheckBox_CZE";
			this.CheckBox_CZE.Size = new System.Drawing.Size( 86, 17 );
			this.CheckBox_CZE.TabIndex = 11;
			this.CheckBox_CZE.Text = "CZE (Czech)";
			this.CheckBox_CZE.UseVisualStyleBackColor = true;
			// 
			// CheckBox_HUN
			// 
			this.CheckBox_HUN.AutoSize = true;
			this.CheckBox_HUN.Location = new System.Drawing.Point( 15, 413 );
			this.CheckBox_HUN.Name = "CheckBox_HUN";
			this.CheckBox_HUN.Size = new System.Drawing.Size( 108, 17 );
			this.CheckBox_HUN.TabIndex = 12;
			this.CheckBox_HUN.Text = "HUN (Hungarian)";
			this.CheckBox_HUN.UseVisualStyleBackColor = true;
			// 
			// CheckBox_POL
			// 
			this.CheckBox_POL.AutoSize = true;
			this.CheckBox_POL.Location = new System.Drawing.Point( 15, 390 );
			this.CheckBox_POL.Name = "CheckBox_POL";
			this.CheckBox_POL.Size = new System.Drawing.Size( 84, 17 );
			this.CheckBox_POL.TabIndex = 13;
			this.CheckBox_POL.Text = "POL (Polish)";
			this.CheckBox_POL.UseVisualStyleBackColor = true;
			// 
			// CheckBox_RUS
			// 
			this.CheckBox_RUS.AutoSize = true;
			this.CheckBox_RUS.Location = new System.Drawing.Point( 15, 367 );
			this.CheckBox_RUS.Name = "CheckBox_RUS";
			this.CheckBox_RUS.Size = new System.Drawing.Size( 96, 17 );
			this.CheckBox_RUS.TabIndex = 14;
			this.CheckBox_RUS.Text = "RUS (Russian)";
			this.CheckBox_RUS.UseVisualStyleBackColor = true;
			// 
			// CheckBox_ESM
			// 
			this.CheckBox_ESM.AutoSize = true;
			this.CheckBox_ESM.Location = new System.Drawing.Point( 15, 321 );
			this.CheckBox_ESM.Name = "CheckBox_ESM";
			this.CheckBox_ESM.Size = new System.Drawing.Size( 169, 17 );
			this.CheckBox_ESM.TabIndex = 15;
			this.CheckBox_ESM.Text = "ESM (Latin American Spanish)";
			this.CheckBox_ESM.UseVisualStyleBackColor = true;
			// 
			// CheckBox_ESN
			// 
			this.CheckBox_ESN.AutoSize = true;
			this.CheckBox_ESN.Location = new System.Drawing.Point( 15, 298 );
			this.CheckBox_ESN.Name = "CheckBox_ESN";
			this.CheckBox_ESN.Size = new System.Drawing.Size( 144, 17 );
			this.CheckBox_ESN.TabIndex = 16;
			this.CheckBox_ESN.Text = "ESN (European Spanish)";
			this.CheckBox_ESN.UseVisualStyleBackColor = true;
			// 
			// CheckBox_DEU
			// 
			this.CheckBox_DEU.AutoSize = true;
			this.CheckBox_DEU.Location = new System.Drawing.Point( 15, 275 );
			this.CheckBox_DEU.Name = "CheckBox_DEU";
			this.CheckBox_DEU.Size = new System.Drawing.Size( 95, 17 );
			this.CheckBox_DEU.TabIndex = 17;
			this.CheckBox_DEU.Text = "DEU (German)";
			this.CheckBox_DEU.UseVisualStyleBackColor = true;
			// 
			// CheckBox_ITA
			// 
			this.CheckBox_ITA.AutoSize = true;
			this.CheckBox_ITA.Location = new System.Drawing.Point( 15, 252 );
			this.CheckBox_ITA.Name = "CheckBox_ITA";
			this.CheckBox_ITA.Size = new System.Drawing.Size( 80, 17 );
			this.CheckBox_ITA.TabIndex = 18;
			this.CheckBox_ITA.Text = "ITA (Italian)";
			this.CheckBox_ITA.UseVisualStyleBackColor = true;
			// 
			// CheckBox_FRA
			// 
			this.CheckBox_FRA.AutoSize = true;
			this.CheckBox_FRA.Location = new System.Drawing.Point( 15, 229 );
			this.CheckBox_FRA.Name = "CheckBox_FRA";
			this.CheckBox_FRA.Size = new System.Drawing.Size( 138, 17 );
			this.CheckBox_FRA.TabIndex = 19;
			this.CheckBox_FRA.Text = "FRA (European French)";
			this.CheckBox_FRA.UseVisualStyleBackColor = true;
			// 
			// MainMenu
			// 
			this.MainMenu.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.toolsToolStripMenuItem} );
			this.MainMenu.Location = new System.Drawing.Point( 0, 0 );
			this.MainMenu.Name = "MainMenu";
			this.MainMenu.Size = new System.Drawing.Size( 1313, 24 );
			this.MainMenu.TabIndex = 20;
			this.MainMenu.Text = "File";
			// 
			// fileToolStripMenuItem
			// 
			this.fileToolStripMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.QuitToolStripMenuItem} );
			this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
			this.fileToolStripMenuItem.Size = new System.Drawing.Size( 35, 20 );
			this.fileToolStripMenuItem.Text = "File";
			// 
			// QuitToolStripMenuItem
			// 
			this.QuitToolStripMenuItem.Name = "QuitToolStripMenuItem";
			this.QuitToolStripMenuItem.Size = new System.Drawing.Size( 94, 22 );
			this.QuitToolStripMenuItem.Text = "Quit";
			this.QuitToolStripMenuItem.Click += new System.EventHandler( this.QuitToolStripMenuItem_Click );
			// 
			// toolsToolStripMenuItem
			// 
			this.toolsToolStripMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.OptionsToolStripMenuItem} );
			this.toolsToolStripMenuItem.Name = "toolsToolStripMenuItem";
			this.toolsToolStripMenuItem.Size = new System.Drawing.Size( 44, 20 );
			this.toolsToolStripMenuItem.Text = "Tools";
			// 
			// OptionsToolStripMenuItem
			// 
			this.OptionsToolStripMenuItem.Name = "OptionsToolStripMenuItem";
			this.OptionsToolStripMenuItem.Size = new System.Drawing.Size( 111, 22 );
			this.OptionsToolStripMenuItem.Text = "Options";
			this.OptionsToolStripMenuItem.Click += new System.EventHandler( this.OptionsToolStripMenuItem_Click );
			// 
			// Button_GenDiffFiles
			// 
			this.Button_GenDiffFiles.Location = new System.Drawing.Point( 15, 95 );
			this.Button_GenDiffFiles.Name = "Button_GenDiffFiles";
			this.Button_GenDiffFiles.Size = new System.Drawing.Size( 200, 23 );
			this.Button_GenDiffFiles.TabIndex = 21;
			this.Button_GenDiffFiles.Text = "Generate Loc Diff Files";
			this.Button_GenDiffFiles.UseVisualStyleBackColor = true;
			this.Button_GenDiffFiles.Click += new System.EventHandler( this.Button_GenDiffFiles_Click );
			// 
			// GenericFolderBrowser
			// 
			this.GenericFolderBrowser.RootFolder = System.Environment.SpecialFolder.MyComputer;
			// 
			// Button_SaveComments
			// 
			this.Button_SaveComments.Location = new System.Drawing.Point( 15, 65 );
			this.Button_SaveComments.Name = "Button_SaveComments";
			this.Button_SaveComments.Size = new System.Drawing.Size( 200, 23 );
			this.Button_SaveComments.TabIndex = 22;
			this.Button_SaveComments.Text = "Save Errors and Warnings to File";
			this.Button_SaveComments.UseVisualStyleBackColor = true;
			this.Button_SaveComments.Click += new System.EventHandler( this.Button_SaveWarnings_Click );
			// 
			// MainLogWindow
			// 
			this.MainLogWindow.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.MainLogWindow.AutoScroll = true;
			this.MainLogWindow.BackColor = System.Drawing.SystemColors.Window;
			this.MainLogWindow.Cursor = System.Windows.Forms.Cursors.IBeam;
			outputWindowDocument1.Text = "";
			this.MainLogWindow.Document = outputWindowDocument1;
			this.MainLogWindow.FindTextBackColor = System.Drawing.Color.Yellow;
			this.MainLogWindow.FindTextForeColor = System.Drawing.Color.Black;
			this.MainLogWindow.FindTextLineHighlight = System.Drawing.Color.FromArgb( ( ( int )( ( ( byte )( 239 ) ) ) ), ( ( int )( ( ( byte )( 248 ) ) ) ), ( ( int )( ( ( byte )( 255 ) ) ) ) );
			this.MainLogWindow.Font = new System.Drawing.Font( "Courier New", 9F );
			this.MainLogWindow.ForeColor = System.Drawing.SystemColors.WindowText;
			this.MainLogWindow.Location = new System.Drawing.Point( 221, 35 );
			this.MainLogWindow.Name = "MainLogWindow";
			this.MainLogWindow.Size = new System.Drawing.Size( 1080, 706 );
			this.MainLogWindow.TabIndex = 23;
			// 
			// CheckBox_SLO
			// 
			this.CheckBox_SLO.AutoSize = true;
			this.CheckBox_SLO.Location = new System.Drawing.Point( 15, 459 );
			this.CheckBox_SLO.Name = "CheckBox_SLO";
			this.CheckBox_SLO.Size = new System.Drawing.Size( 103, 17 );
			this.CheckBox_SLO.TabIndex = 24;
			this.CheckBox_SLO.Text = "SLO (Slovakian)";
			this.CheckBox_SLO.UseVisualStyleBackColor = true;
			// 
			// CheckBox_XXX
			// 
			this.CheckBox_XXX.AutoSize = true;
			this.CheckBox_XXX.Location = new System.Drawing.Point( 15, 551 );
			this.CheckBox_XXX.Name = "CheckBox_XXX";
			this.CheckBox_XXX.Size = new System.Drawing.Size( 77, 17 );
			this.CheckBox_XXX.TabIndex = 25;
			this.CheckBox_XXX.Text = "XXX (Test)";
			this.CheckBox_XXX.UseVisualStyleBackColor = true;
			// 
			// Button_ReloadChangedFiles
			// 
			this.Button_ReloadChangedFiles.Location = new System.Drawing.Point( 15, 125 );
			this.Button_ReloadChangedFiles.Name = "Button_ReloadChangedFiles";
			this.Button_ReloadChangedFiles.Size = new System.Drawing.Size( 200, 23 );
			this.Button_ReloadChangedFiles.TabIndex = 26;
			this.Button_ReloadChangedFiles.Text = "Reload Changed Files";
			this.Button_ReloadChangedFiles.UseVisualStyleBackColor = true;
			this.Button_ReloadChangedFiles.Click += new System.EventHandler( this.ReloadChangedFilesClick );
			// 
			// UnrealLoc
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 1313, 753 );
			this.Controls.Add( this.Button_ReloadChangedFiles );
			this.Controls.Add( this.CheckBox_XXX );
			this.Controls.Add( this.CheckBox_SLO );
			this.Controls.Add( this.MainLogWindow );
			this.Controls.Add( this.Button_SaveComments );
			this.Controls.Add( this.Button_GenDiffFiles );
			this.Controls.Add( this.CheckBox_FRA );
			this.Controls.Add( this.CheckBox_ITA );
			this.Controls.Add( this.CheckBox_DEU );
			this.Controls.Add( this.CheckBox_ESN );
			this.Controls.Add( this.CheckBox_ESM );
			this.Controls.Add( this.CheckBox_RUS );
			this.Controls.Add( this.CheckBox_POL );
			this.Controls.Add( this.CheckBox_HUN );
			this.Controls.Add( this.CheckBox_CZE );
			this.Controls.Add( this.CheckBox_PTB );
			this.Controls.Add( this.CheckBox_JPN );
			this.Controls.Add( this.CheckBox_KOR );
			this.Controls.Add( this.CheckBox_CHN );
			this.Controls.Add( this.CheckBox_INT );
			this.Controls.Add( this.Button_ImportText );
			this.Controls.Add( this.Button_GenLocFiles );
			this.Controls.Add( this.MainMenu );
			this.MainMenuStrip = this.MainMenu;
			this.MinimumSize = new System.Drawing.Size( 300, 550 );
			this.Name = "UnrealLoc";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "UnrealLoc";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler( this.UnrealLoc_FormClosed );
			this.MainMenu.ResumeLayout( false );
			this.MainMenu.PerformLayout();
			this.ResumeLayout( false );
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button Button_GenLocFiles;
        private System.Windows.Forms.Button Button_ImportText;
        private System.Windows.Forms.CheckBox CheckBox_INT;
		private System.Windows.Forms.CheckBox CheckBox_CHN;
        private System.Windows.Forms.CheckBox CheckBox_KOR;
        private System.Windows.Forms.CheckBox CheckBox_JPN;
        private System.Windows.Forms.CheckBox CheckBox_PTB;
        private System.Windows.Forms.CheckBox CheckBox_CZE;
        private System.Windows.Forms.CheckBox CheckBox_HUN;
        private System.Windows.Forms.CheckBox CheckBox_POL;
        private System.Windows.Forms.CheckBox CheckBox_RUS;
        private System.Windows.Forms.CheckBox CheckBox_ESM;
        private System.Windows.Forms.CheckBox CheckBox_ESN;
        private System.Windows.Forms.CheckBox CheckBox_DEU;
        private System.Windows.Forms.CheckBox CheckBox_ITA;
        private System.Windows.Forms.CheckBox CheckBox_FRA;
        private System.Windows.Forms.MenuStrip MainMenu;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem QuitToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem toolsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem OptionsToolStripMenuItem;
        private System.Windows.Forms.Button Button_GenDiffFiles;
        private System.Windows.Forms.FolderBrowserDialog GenericFolderBrowser;
        private System.Windows.Forms.Button Button_SaveComments;
        private UnrealControls.OutputWindowView MainLogWindow;
		private System.Windows.Forms.CheckBox CheckBox_SLO;
		private System.Windows.Forms.CheckBox CheckBox_XXX;
		private System.Windows.Forms.Button Button_ReloadChangedFiles;
    }
}

