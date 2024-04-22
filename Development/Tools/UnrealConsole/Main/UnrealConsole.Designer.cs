/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System.Windows.Forms;

namespace UnrealConsole
{
	partial class UnrealConsoleWindow
	{
		#region Windows Form Designer generated code

		private ToolStripMenuItem IDM_EXIT;
		private ToolStripMenuItem IDM_CLEARWINDOW;
		private ToolStripMenuItem IDM_SAVE;
		private ToolStripMenuItem FileMenu;
		private ToolStripMenuItem EditMenu;
		private MenuStrip MainMenu;
		private ToolStripSeparator FileSeparator1;

		private ToolStripMenuItem IDM_CONNECT;
		private ToolStripMenuItem IDM_CONSOLE;
		private ToolStripMenuItem IDM_REBOOT;
		private ToolStripMenuItem IDM_SCREENCAPTURE;
		private ToolStripMenuItem Menu_EditFind;
		private ToolStripMenuItem Menu_ScrollToEnd;
		private ToolStripMenuItem Mene_InvisFindNext;
		private ToolStripMenuItem Menu_InvisFindPrev;
		private ToolStripMenuItem IDM_ALWAYS_LOG;
		private ToolStripSeparator toolStripSeparator1;
		private ToolStripSeparator toolStripSeparator2;
		private ToolStripMenuItem IDM_CLEAR_CMD_HISTORY;
		private ToolStripMenuItem IDM_DELETE_PDB;

		private UnrealControls.DynamicTabControl mMainTabControl;
		private ToolStripMenuItem IDM_CLEARALLWINDOWS;
		private ToolStripMenuItem IDM_CRASHREPORTFILTER;
		private ToolStripMenuItem IDM_CRASHFILTER_SELECTALL;
		private ToolStripMenuItem IDM_CRASHFILTER_DESELECTALL;
		private ToolStripSeparator toolStripMenuItem1;
		private ToolStripMenuItem IDM_CRASHFILTER_DEBUG;
		private ToolStripMenuItem IDM_CRASHFILTER_RELEASE;
		private ToolStripMenuItem IDM_CRASHFILTER_SHIPPING;
		private ToolStripMenuItem IDM_SAVEALL;
		private ToolStripMenuItem IDM_DUMPTYPE;
		private ToolStripMenuItem IDM_DUMP_NORMAL;
		private ToolStripMenuItem IDM_DUMP_WITHFULLMEM;
		private ToolStripMenuItem IDM_HELP;
		private ToolStripMenuItem IDM_HELP_ABOUT;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				foreach( ConsoleInterface.Platform CurPlatform in ConsoleInterface.DLLInterface.Platforms )
				{
					CurPlatform.Dispose();
				}
			}

			base.Dispose( disposing );
		}

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UnrealConsoleWindow));
			this.MainMenu = new System.Windows.Forms.MenuStrip();
			this.FileMenu = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_SAVE = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_SAVEALL = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_ALWAYS_LOG = new System.Windows.Forms.ToolStripMenuItem();
			this.FileSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.IDM_EXIT = new System.Windows.Forms.ToolStripMenuItem();
			this.EditMenu = new System.Windows.Forms.ToolStripMenuItem();
			this.Menu_EditFind = new System.Windows.Forms.ToolStripMenuItem();
			this.Menu_ScrollToEnd = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
			this.IDM_CLEARWINDOW = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CLEARALLWINDOWS = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CLEAR_CMD_HISTORY = new System.Windows.Forms.ToolStripMenuItem();
			this.Mene_InvisFindNext = new System.Windows.Forms.ToolStripMenuItem();
			this.Menu_InvisFindPrev = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CONSOLE = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CONNECT = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.IDM_REBOOT = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_SCREENCAPTURE = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_DELETE_PDB = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_DUMPTYPE = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_DUMP_NORMAL = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_DUMP_WITHFULLMEM = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CRASHREPORTFILTER = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CRASHFILTER_SELECTALL = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CRASHFILTER_DESELECTALL = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripSeparator();
			this.IDM_CRASHFILTER_DEBUG = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CRASHFILTER_RELEASE = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CRASHFILTER_SHIPPING = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_CRASHFILTER_TEST = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_HELP = new System.Windows.Forms.ToolStripMenuItem();
			this.IDM_HELP_ABOUT = new System.Windows.Forms.ToolStripMenuItem();
			this.mMainTabControl = new UnrealControls.DynamicTabControl();
			this.reconnectTargetToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.MainMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// MainMenu
			// 
			this.MainMenu.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.MainMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FileMenu,
            this.EditMenu,
            this.IDM_CONSOLE,
            this.IDM_CRASHREPORTFILTER,
            this.IDM_HELP});
			this.MainMenu.Location = new System.Drawing.Point(0, 0);
			this.MainMenu.Name = "MainMenu";
			this.MainMenu.Size = new System.Drawing.Size(811, 24);
			this.MainMenu.TabIndex = 0;
			// 
			// FileMenu
			// 
			this.FileMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.IDM_SAVE,
            this.IDM_SAVEALL,
            this.IDM_ALWAYS_LOG,
            this.FileSeparator1,
            this.IDM_EXIT});
			this.FileMenu.Name = "FileMenu";
			this.FileMenu.Size = new System.Drawing.Size(37, 20);
			this.FileMenu.Text = "&File";
			// 
			// IDM_SAVE
			// 
			this.IDM_SAVE.Name = "IDM_SAVE";
			this.IDM_SAVE.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.S)));
			this.IDM_SAVE.Size = new System.Drawing.Size(224, 22);
			this.IDM_SAVE.Text = "&Save Log...";
			this.IDM_SAVE.Click += new System.EventHandler(this.IDM_SAVE_Click);
			// 
			// IDM_SAVEALL
			// 
			this.IDM_SAVEALL.Name = "IDM_SAVEALL";
			this.IDM_SAVEALL.ShortcutKeys = ((System.Windows.Forms.Keys)(((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.Shift) 
            | System.Windows.Forms.Keys.S)));
			this.IDM_SAVEALL.Size = new System.Drawing.Size(224, 22);
			this.IDM_SAVEALL.Text = "Save All Logs...";
			this.IDM_SAVEALL.Click += new System.EventHandler(this.IDM_SAVEALL_Click);
			// 
			// IDM_ALWAYS_LOG
			// 
			this.IDM_ALWAYS_LOG.Name = "IDM_ALWAYS_LOG";
			this.IDM_ALWAYS_LOG.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.L)));
			this.IDM_ALWAYS_LOG.Size = new System.Drawing.Size(224, 22);
			this.IDM_ALWAYS_LOG.Text = "&Always Log...";
			this.IDM_ALWAYS_LOG.Click += new System.EventHandler(this.IDM_ALWAYS_LOG_Click);
			// 
			// FileSeparator1
			// 
			this.FileSeparator1.Name = "FileSeparator1";
			this.FileSeparator1.Size = new System.Drawing.Size(221, 6);
			// 
			// IDM_EXIT
			// 
			this.IDM_EXIT.Name = "IDM_EXIT";
			this.IDM_EXIT.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Alt | System.Windows.Forms.Keys.F4)));
			this.IDM_EXIT.Size = new System.Drawing.Size(224, 22);
			this.IDM_EXIT.Text = "E&xit";
			this.IDM_EXIT.Click += new System.EventHandler(this.OnExit);
			// 
			// EditMenu
			// 
			this.EditMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.Menu_EditFind,
            this.Menu_ScrollToEnd,
            this.toolStripSeparator2,
            this.IDM_CLEARWINDOW,
            this.IDM_CLEARALLWINDOWS,
            this.IDM_CLEAR_CMD_HISTORY,
            this.Mene_InvisFindNext,
            this.Menu_InvisFindPrev});
			this.EditMenu.Name = "EditMenu";
			this.EditMenu.Size = new System.Drawing.Size(39, 20);
			this.EditMenu.Text = "&Edit";
			// 
			// Menu_EditFind
			// 
			this.Menu_EditFind.Name = "Menu_EditFind";
			this.Menu_EditFind.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.F)));
			this.Menu_EditFind.Size = new System.Drawing.Size(227, 22);
			this.Menu_EditFind.Text = "&Find";
			this.Menu_EditFind.Click += new System.EventHandler(this.Menu_EditFind_Click);
			// 
			// Menu_ScrollToEnd
			// 
			this.Menu_ScrollToEnd.Name = "Menu_ScrollToEnd";
			this.Menu_ScrollToEnd.Size = new System.Drawing.Size(227, 22);
			this.Menu_ScrollToEnd.Text = "Scroll to End";
			this.Menu_ScrollToEnd.Click += new System.EventHandler(this.Menu_ScrollToEnd_Click);
			// 
			// toolStripSeparator2
			// 
			this.toolStripSeparator2.Name = "toolStripSeparator2";
			this.toolStripSeparator2.Size = new System.Drawing.Size(224, 6);
			// 
			// IDM_CLEARWINDOW
			// 
			this.IDM_CLEARWINDOW.Name = "IDM_CLEARWINDOW";
			this.IDM_CLEARWINDOW.ShortcutKeys = System.Windows.Forms.Keys.F11;
			this.IDM_CLEARWINDOW.Size = new System.Drawing.Size(227, 22);
			this.IDM_CLEARWINDOW.Text = "&Clear window";
			this.IDM_CLEARWINDOW.Click += new System.EventHandler(this.OnClearWindow);
			// 
			// IDM_CLEARALLWINDOWS
			// 
			this.IDM_CLEARALLWINDOWS.Name = "IDM_CLEARALLWINDOWS";
			this.IDM_CLEARALLWINDOWS.ShortcutKeys = System.Windows.Forms.Keys.F12;
			this.IDM_CLEARALLWINDOWS.Size = new System.Drawing.Size(227, 22);
			this.IDM_CLEARALLWINDOWS.Text = "Clear All Windows";
			this.IDM_CLEARALLWINDOWS.Click += new System.EventHandler(this.IDM_CLEARALLWINDOWS_Click);
			// 
			// IDM_CLEAR_CMD_HISTORY
			// 
			this.IDM_CLEAR_CMD_HISTORY.Name = "IDM_CLEAR_CMD_HISTORY";
			this.IDM_CLEAR_CMD_HISTORY.ShortcutKeys = System.Windows.Forms.Keys.F10;
			this.IDM_CLEAR_CMD_HISTORY.Size = new System.Drawing.Size(227, 22);
			this.IDM_CLEAR_CMD_HISTORY.Text = "Clear Command History";
			this.IDM_CLEAR_CMD_HISTORY.Click += new System.EventHandler(this.IDM_CLEAR_CMD_HISTORY_Click);
			// 
			// Mene_InvisFindNext
			// 
			this.Mene_InvisFindNext.Name = "Mene_InvisFindNext";
			this.Mene_InvisFindNext.ShortcutKeys = System.Windows.Forms.Keys.F3;
			this.Mene_InvisFindNext.Size = new System.Drawing.Size(227, 22);
			this.Mene_InvisFindNext.Text = "InvisFindNext";
			this.Mene_InvisFindNext.Visible = false;
			this.Mene_InvisFindNext.Click += new System.EventHandler(this.Menu_InvisFindNext_Click);
			// 
			// Menu_InvisFindPrev
			// 
			this.Menu_InvisFindPrev.Name = "Menu_InvisFindPrev";
			this.Menu_InvisFindPrev.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Shift | System.Windows.Forms.Keys.F3)));
			this.Menu_InvisFindPrev.Size = new System.Drawing.Size(227, 22);
			this.Menu_InvisFindPrev.Text = "InvisFindPrev";
			this.Menu_InvisFindPrev.Visible = false;
			this.Menu_InvisFindPrev.Click += new System.EventHandler(this.Menu_InvisFindPrev_Click);
			// 
			// IDM_CONSOLE
			// 
			this.IDM_CONSOLE.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.IDM_CONNECT,
            this.toolStripSeparator1,
            this.IDM_REBOOT,
            this.IDM_SCREENCAPTURE,
            this.IDM_DELETE_PDB,
            this.IDM_DUMPTYPE,
            this.reconnectTargetToolStripMenuItem});
			this.IDM_CONSOLE.Name = "IDM_CONSOLE";
			this.IDM_CONSOLE.Size = new System.Drawing.Size(62, 20);
			this.IDM_CONSOLE.Text = "Console";
			this.IDM_CONSOLE.DropDownOpening += new System.EventHandler(this.IDM_CONSOLE_DropDownOpening);
			// 
			// IDM_CONNECT
			// 
			this.IDM_CONNECT.Name = "IDM_CONNECT";
			this.IDM_CONNECT.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.O)));
			this.IDM_CONNECT.Size = new System.Drawing.Size(208, 22);
			this.IDM_CONNECT.Text = "Connect...";
			this.IDM_CONNECT.Click += new System.EventHandler(this.IDM_CONNECT_Click);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(205, 6);
			// 
			// IDM_REBOOT
			// 
			this.IDM_REBOOT.Name = "IDM_REBOOT";
			this.IDM_REBOOT.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.R)));
			this.IDM_REBOOT.Size = new System.Drawing.Size(208, 22);
			this.IDM_REBOOT.Text = "Reboot Target";
			this.IDM_REBOOT.Click += new System.EventHandler(this.IDM_REBOOT_Click);
			// 
			// IDM_SCREENCAPTURE
			// 
			this.IDM_SCREENCAPTURE.Name = "IDM_SCREENCAPTURE";
			this.IDM_SCREENCAPTURE.ShortcutKeys = System.Windows.Forms.Keys.F9;
			this.IDM_SCREENCAPTURE.Size = new System.Drawing.Size(208, 22);
			this.IDM_SCREENCAPTURE.Text = "Screen Capture";
			this.IDM_SCREENCAPTURE.Click += new System.EventHandler(this.IDM_SCREENCAPTURE_Click);
			// 
			// IDM_DELETE_PDB
			// 
			this.IDM_DELETE_PDB.Name = "IDM_DELETE_PDB";
			this.IDM_DELETE_PDB.Size = new System.Drawing.Size(208, 22);
			this.IDM_DELETE_PDB.Text = "Delete PDB\'s";
			this.IDM_DELETE_PDB.Click += new System.EventHandler(this.IDM_DELETE_PDB_Click);
			// 
			// IDM_DUMPTYPE
			// 
			this.IDM_DUMPTYPE.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.IDM_DUMP_NORMAL,
            this.IDM_DUMP_WITHFULLMEM});
			this.IDM_DUMPTYPE.Name = "IDM_DUMPTYPE";
			this.IDM_DUMPTYPE.Size = new System.Drawing.Size(208, 22);
			this.IDM_DUMPTYPE.Text = "Dump Type";
			// 
			// IDM_DUMP_NORMAL
			// 
			this.IDM_DUMP_NORMAL.Name = "IDM_DUMP_NORMAL";
			this.IDM_DUMP_NORMAL.Size = new System.Drawing.Size(169, 22);
			this.IDM_DUMP_NORMAL.Text = "Normal";
			this.IDM_DUMP_NORMAL.Click += new System.EventHandler(this.IDM_DUMP_NORMAL_Click);
			// 
			// IDM_DUMP_WITHFULLMEM
			// 
			this.IDM_DUMP_WITHFULLMEM.Name = "IDM_DUMP_WITHFULLMEM";
			this.IDM_DUMP_WITHFULLMEM.Size = new System.Drawing.Size(169, 22);
			this.IDM_DUMP_WITHFULLMEM.Text = "With Full Memory";
			this.IDM_DUMP_WITHFULLMEM.Click += new System.EventHandler(this.IDM_DUMP_WITHFULLMEM_Click);
			// 
			// IDM_CRASHREPORTFILTER
			// 
			this.IDM_CRASHREPORTFILTER.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.IDM_CRASHFILTER_SELECTALL,
            this.IDM_CRASHFILTER_DESELECTALL,
            this.toolStripMenuItem1,
            this.IDM_CRASHFILTER_DEBUG,
            this.IDM_CRASHFILTER_RELEASE,
            this.IDM_CRASHFILTER_SHIPPING,
            this.IDM_CRASHFILTER_TEST});
			this.IDM_CRASHREPORTFILTER.Name = "IDM_CRASHREPORTFILTER";
			this.IDM_CRASHREPORTFILTER.Size = new System.Drawing.Size(116, 20);
			this.IDM_CRASHREPORTFILTER.Text = "Crash Report Filter";
			this.IDM_CRASHREPORTFILTER.DropDownOpening += new System.EventHandler(this.IDM_CRASHREPORTFILTER_DropDownOpening);
			// 
			// IDM_CRASHFILTER_SELECTALL
			// 
			this.IDM_CRASHFILTER_SELECTALL.Name = "IDM_CRASHFILTER_SELECTALL";
			this.IDM_CRASHFILTER_SELECTALL.Size = new System.Drawing.Size(135, 22);
			this.IDM_CRASHFILTER_SELECTALL.Text = "Select All";
			this.IDM_CRASHFILTER_SELECTALL.Click += new System.EventHandler(this.IDM_CRASHFILTER_SELECTALL_Click);
			// 
			// IDM_CRASHFILTER_DESELECTALL
			// 
			this.IDM_CRASHFILTER_DESELECTALL.Name = "IDM_CRASHFILTER_DESELECTALL";
			this.IDM_CRASHFILTER_DESELECTALL.Size = new System.Drawing.Size(135, 22);
			this.IDM_CRASHFILTER_DESELECTALL.Text = "Deselect All";
			this.IDM_CRASHFILTER_DESELECTALL.Click += new System.EventHandler(this.IDM_CRASHFILTER_DESELECTALL_Click);
			// 
			// toolStripMenuItem1
			// 
			this.toolStripMenuItem1.Name = "toolStripMenuItem1";
			this.toolStripMenuItem1.Size = new System.Drawing.Size(132, 6);
			// 
			// IDM_CRASHFILTER_DEBUG
			// 
			this.IDM_CRASHFILTER_DEBUG.Name = "IDM_CRASHFILTER_DEBUG";
			this.IDM_CRASHFILTER_DEBUG.Size = new System.Drawing.Size(135, 22);
			this.IDM_CRASHFILTER_DEBUG.Text = "Debug";
			this.IDM_CRASHFILTER_DEBUG.Click += new System.EventHandler(this.IDM_CRASHFILTER_DEBUG_Click);
			// 
			// IDM_CRASHFILTER_RELEASE
			// 
			this.IDM_CRASHFILTER_RELEASE.Name = "IDM_CRASHFILTER_RELEASE";
			this.IDM_CRASHFILTER_RELEASE.Size = new System.Drawing.Size(135, 22);
			this.IDM_CRASHFILTER_RELEASE.Text = "Release";
			this.IDM_CRASHFILTER_RELEASE.Click += new System.EventHandler(this.IDM_CRASHFILTER_RELEASE_Click);
			// 
			// IDM_CRASHFILTER_SHIPPING
			// 
			this.IDM_CRASHFILTER_SHIPPING.Name = "IDM_CRASHFILTER_SHIPPING";
			this.IDM_CRASHFILTER_SHIPPING.Size = new System.Drawing.Size(135, 22);
			this.IDM_CRASHFILTER_SHIPPING.Text = "Shipping";
			this.IDM_CRASHFILTER_SHIPPING.Click += new System.EventHandler(this.IDM_CRASHFILTER_SHIPPING_Click);
			// 
			// IDM_CRASHFILTER_TEST
			// 
			this.IDM_CRASHFILTER_TEST.Name = "IDM_CRASHFILTER_TEST";
			this.IDM_CRASHFILTER_TEST.Size = new System.Drawing.Size(135, 22);
			this.IDM_CRASHFILTER_TEST.Text = "Test";
			this.IDM_CRASHFILTER_TEST.Click += new System.EventHandler(this.IDM_CRASHFILTER_TEST_Click);
			// 
			// IDM_HELP
			// 
			this.IDM_HELP.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.IDM_HELP_ABOUT});
			this.IDM_HELP.Name = "IDM_HELP";
			this.IDM_HELP.Size = new System.Drawing.Size(44, 20);
			this.IDM_HELP.Text = "&Help";
			// 
			// IDM_HELP_ABOUT
			// 
			this.IDM_HELP_ABOUT.Name = "IDM_HELP_ABOUT";
			this.IDM_HELP_ABOUT.Size = new System.Drawing.Size(107, 22);
			this.IDM_HELP_ABOUT.Text = "&About";
			this.IDM_HELP_ABOUT.Click += new System.EventHandler(this.IDM_HELP_ABOUT_Click);
			// 
			// mMainTabControl
			// 
			this.mMainTabControl.Dock = System.Windows.Forms.DockStyle.Fill;
			this.mMainTabControl.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.mMainTabControl.Location = new System.Drawing.Point(0, 24);
			this.mMainTabControl.Name = "mMainTabControl";
			this.mMainTabControl.SelectedIndex = 0;
			this.mMainTabControl.SelectedTab = null;
			this.mMainTabControl.Size = new System.Drawing.Size(811, 452);
			this.mMainTabControl.TabIndex = 0;
			this.mMainTabControl.ControlRemoved += new System.Windows.Forms.ControlEventHandler(this.MainTabControl_ControlRemoved);
			// 
			// reconnectTargetToolStripMenuItem
			// 
			this.reconnectTargetToolStripMenuItem.Name = "reconnectTargetToolStripMenuItem";
			this.reconnectTargetToolStripMenuItem.ShortcutKeys = ((System.Windows.Forms.Keys)((System.Windows.Forms.Keys.Control | System.Windows.Forms.Keys.T)));
			this.reconnectTargetToolStripMenuItem.Size = new System.Drawing.Size(208, 22);
			this.reconnectTargetToolStripMenuItem.Text = "Reconnect Target";
			this.reconnectTargetToolStripMenuItem.Click += new System.EventHandler(this.reconnectTargetToolStripMenuItem_Click);
			// 
			// UnrealConsoleWindow
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(6, 16);
			this.ClientSize = new System.Drawing.Size(811, 476);
			this.Controls.Add(this.mMainTabControl);
			this.Controls.Add(this.MainMenu);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.KeyPreview = true;
			this.MainMenuStrip = this.MainMenu;
			this.Name = "UnrealConsoleWindow";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Unreal Console";
			this.MainMenu.ResumeLayout(false);
			this.MainMenu.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}
		#endregion

		private ToolStripMenuItem IDM_CRASHFILTER_TEST;
		private ToolStripMenuItem reconnectTargetToolStripMenuItem;
	}
}
