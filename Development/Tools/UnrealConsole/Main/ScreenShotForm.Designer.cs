/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
namespace UnrealConsole
{
	partial class ScreenShotForm
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
			if(disposing && (components != null))
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
			this.menuMain = new System.Windows.Forms.MenuStrip();
			this.menuItemFile = new System.Windows.Forms.ToolStripMenuItem();
			this.menuItemFile_SaveAs = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripSeparator();
			this.menuItemFile_Exit = new System.Windows.Forms.ToolStripMenuItem();
			this.screenshotToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.captureScreenshotToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.saveImageDlg = new System.Windows.Forms.SaveFileDialog();
			this.pictMain = new System.Windows.Forms.PictureBox();
			this.menuMain.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.pictMain)).BeginInit();
			this.SuspendLayout();
			// 
			// menuMain
			// 
			this.menuMain.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.menuMain.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.menuItemFile,
			this.screenshotToolStripMenuItem});
			this.menuMain.Location = new System.Drawing.Point(0, 0);
			this.menuMain.Name = "menuMain";
			this.menuMain.Padding = new System.Windows.Forms.Padding(7, 2, 0, 2);
			this.menuMain.Size = new System.Drawing.Size(673, 24);
			this.menuMain.TabIndex = 1;
			this.menuMain.Text = "menuStrip1";
			// 
			// menuItemFile
			// 
			this.menuItemFile.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.menuItemFile_SaveAs,
			this.toolStripMenuItem1,
			this.menuItemFile_Exit});
			this.menuItemFile.Name = "menuItemFile";
			this.menuItemFile.Size = new System.Drawing.Size(37, 20);
			this.menuItemFile.Text = "&File";
			// 
			// menuItemFile_SaveAs
			// 
			this.menuItemFile_SaveAs.Name = "menuItemFile_SaveAs";
			this.menuItemFile_SaveAs.Size = new System.Drawing.Size(123, 22);
			this.menuItemFile_SaveAs.Text = "&Save As...";
			this.menuItemFile_SaveAs.Click += new System.EventHandler(this.menuItemFile_SaveAs_Click);
			// 
			// toolStripMenuItem1
			// 
			this.toolStripMenuItem1.Name = "toolStripMenuItem1";
			this.toolStripMenuItem1.Size = new System.Drawing.Size(120, 6);
			// 
			// menuItemFile_Exit
			// 
			this.menuItemFile_Exit.Name = "menuItemFile_Exit";
			this.menuItemFile_Exit.Size = new System.Drawing.Size(123, 22);
			this.menuItemFile_Exit.Text = "E&xit";
			this.menuItemFile_Exit.Click += new System.EventHandler(this.menuItemFile_Exit_Click);
			// 
			// screenshotToolStripMenuItem
			// 
			this.screenshotToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.captureScreenshotToolStripMenuItem});
			this.screenshotToolStripMenuItem.Name = "screenshotToolStripMenuItem";
			this.screenshotToolStripMenuItem.Size = new System.Drawing.Size(62, 20);
			this.screenshotToolStripMenuItem.Text = "Console";
			// 
			// captureScreenshotToolStripMenuItem
			// 
			this.captureScreenshotToolStripMenuItem.Name = "captureScreenshotToolStripMenuItem";
			this.captureScreenshotToolStripMenuItem.ShortcutKeyDisplayString = "";
			this.captureScreenshotToolStripMenuItem.ShortcutKeys = System.Windows.Forms.Keys.F9;
			this.captureScreenshotToolStripMenuItem.Size = new System.Drawing.Size(173, 22);
			this.captureScreenshotToolStripMenuItem.Text = "Screen Capture";
			this.captureScreenshotToolStripMenuItem.Click += new System.EventHandler(this.captureScreenshotToolStripMenuItem_Click);
			// 
			// saveImageDlg
			// 
			this.saveImageDlg.DefaultExt = "png";
			this.saveImageDlg.Filter = "PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg|Bitmap (*.bmp)|*.bmp|GIF (*.gif)|*.gif";
			this.saveImageDlg.RestoreDirectory = true;
			this.saveImageDlg.SupportMultiDottedExtensions = true;
			this.saveImageDlg.Title = "Save Image As...";
			// 
			// pictMain
			// 
			this.pictMain.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
			this.pictMain.Dock = System.Windows.Forms.DockStyle.Top;
			this.pictMain.Location = new System.Drawing.Point(0, 24);
			this.pictMain.Margin = new System.Windows.Forms.Padding(5);
			this.pictMain.Name = "pictMain";
			this.pictMain.Size = new System.Drawing.Size(673, 0);
			this.pictMain.TabIndex = 2;
			this.pictMain.TabStop = false;
			// 
			// ScreenShotForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.AutoScroll = true;
			this.ClientSize = new System.Drawing.Size(673, 498);
			this.Controls.Add(this.pictMain);
			this.Controls.Add(this.menuMain);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.MainMenuStrip = this.menuMain;
			this.Name = "ScreenShotForm";
			this.Text = "ScreenShotForm";
			this.menuMain.ResumeLayout(false);
			this.menuMain.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.pictMain)).EndInit();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.MenuStrip menuMain;
		private System.Windows.Forms.ToolStripMenuItem menuItemFile;
		private System.Windows.Forms.ToolStripMenuItem menuItemFile_SaveAs;
		private System.Windows.Forms.ToolStripSeparator toolStripMenuItem1;
		private System.Windows.Forms.ToolStripMenuItem menuItemFile_Exit;
		private System.Windows.Forms.SaveFileDialog saveImageDlg;
		private System.Windows.Forms.PictureBox pictMain;
		private System.Windows.Forms.ToolStripMenuItem screenshotToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem captureScreenshotToolStripMenuItem;
	}
}