/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Windows.Threading;
using System.Drawing;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.IO;
using UnrealControls;
using System.Text.RegularExpressions;

namespace UnrealConsole
{
	/// <summary>
	/// A tab page that represents a console target.
	/// </summary>
	public class ConsoleTargetTabPage : UnrealControls.DynamicTabPage
	{
		class DocLineUserData : ICloneable
		{
			public int Filter;

			public DocLineUserData() { }
			public DocLineUserData(int NewFilter) { Filter = NewFilter; }

			#region ICloneable Members

			public object Clone()
			{
				return new DocLineUserData(Filter);
			}

			#endregion
		}

		/// <summary>
		/// This class exists to lock a scope of scode from reentry.
		/// </summary>
		class ScopeLock
		{
			public bool Locked;
		}

		/// <summary>
		/// This structure allows you to conveniently use a ScopeLock within a using statement.
		/// </summary>
		struct ScopeLockGuard : IDisposable
		{
			public ScopeLock mScopeLock;

			public ScopeLockGuard(ScopeLock Lock)
			{
				mScopeLock = Lock;
				mScopeLock.Locked = true;
			}

			#region IDisposable Members

			public void Dispose()
			{
				mScopeLock.Locked = false;
			}

			#endregion
		}

		/// <summary>
		/// Delegate for marshaling append text calls into the UI thread.
		/// </summary>
		/// <param name="Txt">The text to be appended.</param>
		delegate void AppendTextDelegate(string Txt);

		/// <summary>
		/// Delegate for marshaling crashes to the UI thread.
		/// </summary>
		/// /// <param name="GameName">The name of the game that crashed.</param>
		/// <param name="CallStack">The call stack.</param>
		/// <param name="MiniDumpLocation">The path to the mini-dump file.</param>
		delegate void CrashHandlerDelegate(string GameName, string CallStack, string MiniDumpLocation);

		private static readonly string LOG_DIR = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Unreal Console Logs\\");
		private static Regex mFilterRegex = new Regex(@"[^\s]*:", RegexOptions.Singleline | RegexOptions.Compiled | RegexOptions.CultureInvariant | RegexOptions.IgnoreCase);

		private UnrealConsoleWindow MainWindow = null;

		private TextBox mCommand;
		private Button mBtnSend;
		private ConsoleInterface.PlatformTarget mTarget;
		private System.Windows.Threading.DispatcherTimer mOutputTimer;
		private StringBuilder mOutputBuffer = new StringBuilder();
		private int mCmdIndex = -1;
		private int mLastCrashReport;
		private ContextMenuStrip mTTYCtxMenu;
		private System.ComponentModel.IContainer components;
		private ToolStripMenuItem mSelectAllToolStripMenuItem;
		private ToolStripMenuItem mCopyToolStripMenuItem;
		private bool mSendCommandToAll;
		private UnrealControls.OutputWindowView mOutputWindow;
		private volatile bool mLogOutput;
		private ToolStrip mToolStrip_Filter;
		private ToolStripDropDownButton mToolStripButton_Filter;
		private UnrealControls.OutputWindowDocument mDocument = new UnrealControls.OutputWindowDocument();
		private ListView mListView_Filters = new ListView();
		private ToolStripDropDown mDropDown_Filters = new ToolStripDropDown();
		private ToolStripControlHost mHost_Filters;
		private ToolStripLabel mLabel_Filters;
		private ScopeLock mFilterCheckedScopeLock = new ScopeLock();

		/// <summary>
		/// Gets the target that belongs to the tab page.
		/// </summary>
		public ConsoleInterface.PlatformTarget Target
		{
			get { return mTarget; }
		}

		/// <summary>
		/// Gets the TTY output text box associated with the target.
		/// </summary>
		public UnrealControls.OutputWindowView TTYText
		{
			get { return mOutputWindow; }
		}

		/// <summary>
		/// Gets/Sets whether or not TTY output should be logged to disk.
		/// </summary>
		public bool LogOutput
		{
			get { return mLogOutput; }
			set
			{
				mLogOutput = value;

				if(value)
				{
					LogText(string.Format("\r\n*********************************** Starting new log session at {0} ***********************************\r\n\r\n", DateTime.Now.ToString("H:m:s")));
				}
			}
		}

		/// <summary>
		/// Gets the log file's name on disk.
		/// </summary>
		public string LogFilename
		{
			get { return string.Format("{0}_{1}_{2}.txt", mTarget.Name, mTarget.ParentPlatform.Name, DateTime.Today.ToString("M-dd-yyyy")); }
		}

		/// <summary>
		/// Gets/Sets whether commands are sent to all targets.
		/// </summary>
		public bool SendCommandsToAll
		{
			get { return mSendCommandToAll; }
			set
			{
				mSendCommandToAll = value;

				if(value)
				{
					mBtnSend.Text = "Send All";
				}
				else
				{
					mBtnSend.Text = "Send";
				}
			}
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="Target">The target to be associated with the tab page.</param>
		/// <param name="bLogOutput">True if the tab page is to log all TTY output to disk.</param>
		/// <param name="SendCommandsToAll">True if commands are to be sent to all targets.</param>
		public ConsoleTargetTabPage( UnrealConsoleWindow InMainWindow, ConsoleInterface.PlatformTarget Target, bool bLogOutput, bool SendCommandsToAll )
			: base( string.Format( "{0} <{1}>", Target.Name, Target.ParentPlatform.Name ) )
		{
			InitializeComponent();

			MainWindow = InMainWindow;

			mListView_Filters.MaximumSize = mListView_Filters.Size = new Size( 200, 200 );
			mListView_Filters.Columns.Add( "Name", 200 );
			mListView_Filters.MultiSelect = false;
			mListView_Filters.View = View.Details;
			mListView_Filters.HeaderStyle = ColumnHeaderStyle.None;
			mListView_Filters.BorderStyle = BorderStyle.None;
			mListView_Filters.CheckBoxes = true;
			mListView_Filters.HideSelection = true;
			mListView_Filters.Items.Add( "*" ).Checked = true;
			mListView_Filters.ItemChecked += new ItemCheckedEventHandler( mListView_Filters_ItemChecked );

			mHost_Filters = new ToolStripControlHost( mListView_Filters );
			mDropDown_Filters.Items.Add( mHost_Filters );
			mDropDown_Filters.DefaultDropDownDirection = ToolStripDropDownDirection.Left;
			mDropDown_Filters.AutoClose = true;
			mToolStripButton_Filter.DropDown = mDropDown_Filters;

			mTarget = Target;
			mSendCommandToAll = SendCommandsToAll;

			// use the property so the time stamp is logged
			this.LogOutput = bLogOutput;

			mTarget.SetTTYCallback( new ConsoleInterface.TTYOutputDelegate( this.OnTTYEventCallback ) );
			mTarget.SetCrashCallback( new ConsoleInterface.CrashCallbackDelegate( this.OnCrash ) );
			mTarget.CrashFilter = UnrealConsoleWindow.CurrentSettings.CrashFilter;

			if( !mTarget.Connect() )
			{
				mDocument.AppendLine( Color.Red, "Could not connect to target!" );
			}

			ConsoleInterface.DumpType DumpType;
			if( UnrealConsoleWindow.CurrentSettings.TargetDumpTypes.TryGetValue( mTarget.TargetManagerName, out DumpType ) )
			{
				mTarget.CrashDumpType = DumpType;
			}

			mCommand.AutoCompleteCustomSource = UnrealConsoleWindow.CurrentSettings.CommandHistory;
			mCommand.Focus();

			// use a dispatch timer so it happens in the UI thread, because if a tools DLL takes a
			// long time to Heartbeat (say it shows a dialog to get an executable for callstack
			// parsing), then a normal Timer will keep heartbeating on other threads. We can't 
			// guarantee that Heartbeat in a DLL is re-entrant, which normal Timers need
			mOutputTimer = new System.Windows.Threading.DispatcherTimer();
			mOutputTimer.Tick += new EventHandler(OnOutputTimer);
			mOutputTimer.Interval = new TimeSpan(0, 0, 0, 0, 100);
			mOutputTimer.Start();

			mDocument.LineAdded += new EventHandler<OutputWindowDocumentLineAddedEventArgs>( mDocument_LineAdded );

			mOutputWindow.AutoScroll = true;
			mOutputWindow.Document = mDocument;
			mOutputWindow.KeyPress += new KeyPressEventHandler( mOutputWindow_KeyPress );
		}

		/// <summary>
		/// Clears all of the text for the target.
		/// </summary>
		public void ClearOutputWindow()
		{
			mOutputWindow.Clear();

			// The view could be pointing to a filtered document
			if(mOutputWindow.Document != mDocument)
			{
				mDocument.Clear();
			}
		}

		/// <summary>
		/// This delegate is for creating filtered documents.
		/// </summary>
		/// <param name="Line">The line of text that is being checked for filtering.</param>
		/// <param name="Data">User supplied data that may control filtering.</param>
		/// <returns>True if the line of text is to be filtered out of the document.</returns>
		public bool DocumentFilter(OutputWindowDocument.ReadOnlyDocumentLine Line, object Data)
		{
			ListView.CheckedListViewItemCollection Filters = (ListView.CheckedListViewItemCollection)Data;
			DocLineUserData LineUserData = (DocLineUserData)Line.UserData;

			if(LineUserData != null)
			{
				foreach(ListViewItem CurItem in Filters)
				{
					DocLineUserData ItemUserData = (DocLineUserData)CurItem.Tag;

					if(ItemUserData.Filter == LineUserData.Filter)
					{
						return false;
					}
				}
			}

			return true;
		}

		/// <summary>
		/// Event handler for when a filter has been checked.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mListView_Filters_ItemChecked(object sender, ItemCheckedEventArgs e)
		{
			// use a scope lock because we check/uncheck other checkboxes in this function and it causes reentry
			if(!mFilterCheckedScopeLock.Locked)
			{
				using(ScopeLockGuard ScopeLock = new ScopeLockGuard(mFilterCheckedScopeLock))
				{
					if(e.Item.Index == 0 && e.Item.Checked)
					{
						foreach(ListViewItem CurItem in mListView_Filters.Items)
						{
							if(CurItem == e.Item)
							{
								continue;
							}

							CurItem.Checked = false;
						}
					}
					else if(e.Item.Index != 0 && e.Item.Checked && mListView_Filters.Items[0].Checked)
					{
						mListView_Filters.Items[0].Checked = false;
					}

					ListView.CheckedListViewItemCollection CheckedItems = mListView_Filters.CheckedItems;

					if(CheckedItems.Count == 0)
					{
						mOutputWindow.Document = null;
						mLabel_Filters.Text = "";
					}
					else
					{
						if(mListView_Filters.Items[0].Checked)
						{
							mOutputWindow.Document = mDocument;
							mLabel_Filters.Text = mListView_Filters.Items[0].Text;
						}
						else
						{
							mOutputWindow.Document = mDocument.CreateFilteredDocument(DocumentFilter, CheckedItems);

							StringBuilder NewFilterLabel = new StringBuilder();
							foreach(ListViewItem CurItem in CheckedItems)
							{
								NewFilterLabel.Append(CurItem.Text);
								NewFilterLabel.Append(", ");
							}

							// remove the trailing ", "
							NewFilterLabel.Length = NewFilterLabel.Length - 2;

							mLabel_Filters.Text = NewFilterLabel.ToString();
						}

						// This will make sure that the Find text box is appropriately updated
						if(mOutputWindow.IsInFindMode)
						{
							mOutputWindow.FindNext();
						}

						mOutputWindow.ScrollToEnd();
					}
				}
			}
		}

		/// <summary>
		/// Event handler for when a full line of text has been added to the window document.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mDocument_LineAdded(object sender, OutputWindowDocumentLineAddedEventArgs e)
		{
			string LineTxt = e.Line.ToString();
			Match FilterMatch = mFilterRegex.Match(LineTxt);

			if(FilterMatch.Success)
			{
				bool bFound = false;
				int HashCode = FilterMatch.Value.ToLowerInvariant().GetHashCode();
				OutputWindowDocument.ReadOnlyDocumentLine DocumentLine = e.Line;

				DocumentLine.UserData = new DocLineUserData(HashCode);

				foreach(ListViewItem CurItem in mListView_Filters.Items)
				{
					DocLineUserData ItemUserData = (DocLineUserData)CurItem.Tag;

					if(ItemUserData != null && HashCode == ItemUserData.Filter)
					{
						bFound = true;

						// if we're currently using a filtered document and this is a valid line then add it to the filtered document
						if(mOutputWindow.Document != mDocument && CurItem.Checked && mOutputWindow.Document != null)
						{
							mOutputWindow.Document.AppendText(null, LineTxt);
						}

						break;
					}
				}

				if(!bFound)
				{
					ListViewItem NewItem = new ListViewItem(FilterMatch.Value);
					NewItem.Checked = false;
					NewItem.Tag = new DocLineUserData(HashCode);

					mListView_Filters.Items.Add(NewItem);
				}
			}
		}

		/// <summary>
		/// Event handler for when the user starts typing in the output window.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mOutputWindow_KeyPress(object sender, KeyPressEventArgs e)
		{
			if(char.IsLetterOrDigit(e.KeyChar) || char.IsPunctuation(e.KeyChar) || char.IsSymbol(e.KeyChar) || char.IsSeparator(e.KeyChar))
			{
				mCommand.AppendText("" + e.KeyChar);
				mCommand.Focus();
			}
		}

		/// <summary>
		/// Logs a message to disk.
		/// </summary>
		/// <param name="Message">The message to be appended to the log file.</param>
		private void LogText(string Message)
		{
			if(Message.Length > 0)
			{
				try
				{
					if(!Directory.Exists(LOG_DIR))
					{
						Directory.CreateDirectory(LOG_DIR);
					}

					File.AppendAllText(Path.Combine(LOG_DIR, LogFilename), Message);
				}
				catch(Exception ex)
				{
					System.Diagnostics.Debug.WriteLine(ex.ToString());
				}
			}
		}

		/// <summary>
		/// Timer callback for batch appending TTY output.
		/// </summary>
		/// <param name="State">State passed to the callback.</param>
		private void OnOutputTimer(object sender, EventArgs e)
		{
            if (IsHandleCreated)
            {
                string Buf = string.Empty;

                lock (mOutputBuffer)
                {
                    if (mOutputBuffer.Length > 0)
                    {
                        Buf = mOutputBuffer.ToString();

						if (Buf.EndsWith("\r") || Buf.EndsWith("\n"))
						{
							mOutputBuffer.Length = 0;
						}
                    }
                }

                if (Buf.Length > 0 && mOutputBuffer.Length == 0)
                {
                    Print(Buf);

                    if (mLogOutput)
                    {
                        LogText(Buf);
                    }
                }
            }
			
			// let the target managing code have some time
			if( MainWindow.Ticking )
			{
				mTarget.Heartbeat();
			}
		}

		/// <summary>
		/// This function handles a TTY console ouput event.
		/// </summary>
		/// <param name="Txt">A pointer to a wide character string</param>
		unsafe void OnTTYEventCallback(IntPtr Txt)
		{
			char* TxtPtr = (char*)Txt.ToPointer();

			if(*TxtPtr != '\0')
			{
				lock(mOutputBuffer)
				{
					mOutputBuffer.Append(new string(TxtPtr));
				}
			}
		}

		/// <summary>
		/// Appends TTY output text.
		/// </summary>
		/// <param name="Str">The text to be appended.</param>
		private void AppendText(string Str)
		{
			if(Str == null || Str.Length == 0)
			{
				return;
			}

			System.Diagnostics.Debug.WriteLine(Str);

			DynamicTabControl Owner = (DynamicTabControl)this.Parent;

			if(Owner != null && Owner.SelectedTab != this)
			{
				this.TabForegroundColor = Brushes.Red;
			}

			mDocument.AppendText(null, Str);
		}

		/// <summary>
		/// Marshals a print command into the UI thread.
		/// </summary>
		/// <param name="Str">The message to print.</param>
		public void Print(string Str)
		{
            if (Str.Length > 0)
            {
                BeginInvoke(new AppendTextDelegate(this.AppendText), Str);
            }
		}

		/// <summary>
		/// Handles crashes.
		/// </summary>
		/// <param name="Data">A pointer to the callstack.</param>
		unsafe void OnCrash(IntPtr GameNamePtr, IntPtr CallStackPtr, IntPtr MiniDumpLocationPtr)
		{
			string GameName = new string((char*)GameNamePtr.ToPointer());
			string CallStack = new string((char*)CallStackPtr.ToPointer());
			string MiniDumpLoc = new string((char*)MiniDumpLocationPtr.ToPointer());

			if(CallStack.Length > 0 || MiniDumpLoc.Length > 0)
			{
				BeginInvoke(new CrashHandlerDelegate(UIThreadOnCrash), GameName, CallStack, MiniDumpLoc);
			}
		}

		/// <summary>
		/// The crash handler that runs on the UI thread.
		/// </summary>
		/// <param name="CallStack">The crash callstack.</param>
		void UIThreadOnCrash(string GameName, string CallStack, string MiniDumpLocation)
		{
			if(File.Exists("EpicInternal.txt") && Environment.TickCount - mLastCrashReport >= 10000)
			{
				CrashReporter Reporter = new CrashReporter();
				//fire up the autoreporter
				string CrashResult = Reporter.SendCrashReport(GameName, CallStack, TTYText.Text, mTarget.ParentPlatform.Name, MiniDumpLocation);

				AppendText(Environment.NewLine + CallStack + Environment.NewLine);
				AppendText(Environment.NewLine + CrashResult + Environment.NewLine);

				mLastCrashReport = Environment.TickCount;
			}
			else
			{
				AppendText(Environment.NewLine + CallStack);
			}
		}

		#region Windows Forms generated code

		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ConsoleTargetTabPage));
			this.mBtnSend = new System.Windows.Forms.Button();
			this.mTTYCtxMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.mSelectAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.mCopyToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.mOutputWindow = new UnrealControls.OutputWindowView();
			this.mToolStrip_Filter = new System.Windows.Forms.ToolStrip();
			this.mToolStripButton_Filter = new System.Windows.Forms.ToolStripDropDownButton();
			this.mLabel_Filters = new System.Windows.Forms.ToolStripLabel();
			this.mCommand = new System.Windows.Forms.TextBox();
			this.mTTYCtxMenu.SuspendLayout();
			this.mToolStrip_Filter.SuspendLayout();
			this.SuspendLayout();
			// 
			// mBtnSend
			// 
			this.mBtnSend.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.mBtnSend.FlatStyle = System.Windows.Forms.FlatStyle.System;
			this.mBtnSend.Location = new System.Drawing.Point(362, 306);
			this.mBtnSend.Name = "mBtnSend";
			this.mBtnSend.Size = new System.Drawing.Size(75, 23);
			this.mBtnSend.TabIndex = 1;
			this.mBtnSend.Text = "&Send";
			this.mBtnSend.UseVisualStyleBackColor = true;
			this.mBtnSend.Click += new System.EventHandler(this.mBtnSend_Click);
			// 
			// mTTYCtxMenu
			// 
			this.mTTYCtxMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.mSelectAllToolStripMenuItem,
            this.mCopyToolStripMenuItem});
			this.mTTYCtxMenu.Name = "mTTYCtxMenu";
			this.mTTYCtxMenu.Size = new System.Drawing.Size(118, 48);
			// 
			// mSelectAllToolStripMenuItem
			// 
			this.mSelectAllToolStripMenuItem.Name = "mSelectAllToolStripMenuItem";
			this.mSelectAllToolStripMenuItem.Size = new System.Drawing.Size(117, 22);
			this.mSelectAllToolStripMenuItem.Text = "Select All";
			this.mSelectAllToolStripMenuItem.Click += new System.EventHandler(this.mSelectAllToolStripMenuItem_Click);
			// 
			// mCopyToolStripMenuItem
			// 
			this.mCopyToolStripMenuItem.Name = "mCopyToolStripMenuItem";
			this.mCopyToolStripMenuItem.Size = new System.Drawing.Size(117, 22);
			this.mCopyToolStripMenuItem.Text = "Copy";
			this.mCopyToolStripMenuItem.Click += new System.EventHandler(this.mCopyToolStripMenuItem_Click);
			// 
			// mOutputWindow
			// 
			this.mOutputWindow.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
						| System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mOutputWindow.BackColor = System.Drawing.SystemColors.Window;
			this.mOutputWindow.Cursor = System.Windows.Forms.Cursors.Arrow;
			this.mOutputWindow.Document = null;
			this.mOutputWindow.Font = new System.Drawing.Font("Courier New", 9F);
			this.mOutputWindow.Location = new System.Drawing.Point(0, 24);
			this.mOutputWindow.Name = "mOutputWindow";
			this.mOutputWindow.Size = new System.Drawing.Size(440, 278);
			this.mOutputWindow.TabIndex = 2;
			// 
			// mToolStrip_Filter
			// 
			this.mToolStrip_Filter.BackColor = System.Drawing.SystemColors.Window;
			this.mToolStrip_Filter.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.mToolStrip_Filter.GripStyle = System.Windows.Forms.ToolStripGripStyle.Hidden;
			this.mToolStrip_Filter.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.mToolStripButton_Filter,
            this.mLabel_Filters});
			this.mToolStrip_Filter.LayoutStyle = System.Windows.Forms.ToolStripLayoutStyle.HorizontalStackWithOverflow;
			this.mToolStrip_Filter.Location = new System.Drawing.Point(0, 0);
			this.mToolStrip_Filter.Name = "mToolStrip_Filter";
			this.mToolStrip_Filter.RenderMode = System.Windows.Forms.ToolStripRenderMode.System;
			this.mToolStrip_Filter.RightToLeft = System.Windows.Forms.RightToLeft.No;
			this.mToolStrip_Filter.Size = new System.Drawing.Size(440, 25);
			this.mToolStrip_Filter.TabIndex = 3;
			this.mToolStrip_Filter.Text = "toolStrip1";
			// 
			// mToolStripButton_Filter
			// 
			this.mToolStripButton_Filter.BackColor = System.Drawing.SystemColors.Window;
			this.mToolStripButton_Filter.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.mToolStripButton_Filter.Image = ((System.Drawing.Image)(resources.GetObject("mToolStripButton_Filter.Image")));
			this.mToolStripButton_Filter.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.mToolStripButton_Filter.Name = "mToolStripButton_Filter";
			this.mToolStripButton_Filter.Size = new System.Drawing.Size(46, 22);
			this.mToolStripButton_Filter.Text = "Filter";
			// 
			// mLabel_Filters
			// 
			this.mLabel_Filters.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.mLabel_Filters.Name = "mLabel_Filters";
			this.mLabel_Filters.Size = new System.Drawing.Size(12, 15);
			this.mLabel_Filters.Text = "*";
			// 
			// mCommand
			// 
			this.mCommand.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.mCommand.AutoCompleteMode = System.Windows.Forms.AutoCompleteMode.SuggestAppend;
			this.mCommand.AutoCompleteSource = System.Windows.Forms.AutoCompleteSource.CustomSource;
			this.mCommand.Location = new System.Drawing.Point(3, 308);
			this.mCommand.Name = "mCommand";
			this.mCommand.Size = new System.Drawing.Size(353, 23);
			this.mCommand.TabIndex = 0;
			this.mCommand.WordWrap = false;
			this.mCommand.KeyDown += new System.Windows.Forms.KeyEventHandler(this.mCommand_KeyDown);
			// 
			// ConsoleTargetTabPage
			// 
			this.Controls.Add(this.mCommand);
			this.Controls.Add(this.mOutputWindow);
			this.Controls.Add(this.mToolStrip_Filter);
			this.Controls.Add(this.mBtnSend);
			this.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.Name = "ConsoleTargetTabPage";
			this.Size = new System.Drawing.Size(440, 332);
			this.mTTYCtxMenu.ResumeLayout(false);
			this.mToolStrip_Filter.ResumeLayout(false);
			this.mToolStrip_Filter.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mTTYText_KeyPress(object sender, KeyPressEventArgs e)
		{
			e.Handled = true;
			mCommand.Text += e.KeyChar;
			mCommand.Focus();
			mCommand.SelectionStart = mCommand.TextLength;
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mCommand_KeyDown(object sender, KeyEventArgs e)
		{
			AutoCompleteStringCollection CmdHistory = UnrealConsoleWindow.CurrentSettings.CommandHistory;

			switch(e.KeyCode)
			{
				case Keys.Up:
					{
						e.Handled = true;

						if(CmdHistory != null && CmdHistory.Count > 0)
						{
							++mCmdIndex;

							if(mCmdIndex >= CmdHistory.Count)
							{
								mCmdIndex = 0;
							}

							LoadCurrentCommand();
						}

						break;
					}
				case Keys.Down:
					{
						e.Handled = true;

						if(CmdHistory != null)
						{
							--mCmdIndex;

							if(mCmdIndex < 0)
							{
								mCmdIndex = CmdHistory.Count - 1;
							}

							LoadCurrentCommand();

							
						}

						break;
					}
				case Keys.Enter:
					{
						e.Handled = true;
						ExecuteCommand();

						break;
					}
			}
		}

		/// <summary>
		/// Executes a command on the tab page target.
		/// </summary>
		private void ExecuteCommand()
		{
			AutoCompleteStringCollection CmdHistory = UnrealConsoleWindow.CurrentSettings.CommandHistory;

            string CmdNoNewline = mCommand.Text;
			string Cmd = CmdNoNewline + Environment.NewLine;
			mCommand.Clear();

            if (CmdNoNewline.Length > 0)
			{
				if(CmdHistory == null)
				{
					CmdHistory = new AutoCompleteStringCollection();
					mCommand.AutoCompleteCustomSource = CmdHistory;
					UnrealConsoleWindow.CurrentSettings.CommandHistory = CmdHistory;
				}

                if (!CmdHistory.Contains(CmdNoNewline))
				{
					CmdHistory.Insert(0, CmdNoNewline);
				}
				else
				{
                    CmdHistory.Remove(CmdNoNewline);
                    CmdHistory.Insert(0, CmdNoNewline);
				}

				DynamicTabControl Owner = this.Parent as DynamicTabControl;

				if(Owner != null)
				{
					foreach(ConsoleTargetTabPage CurPage in Owner.TabPages)
					{
						CurPage.ClearCommandHistoryState();
					}
				}

				if(mSendCommandToAll && Owner != null)
				{
					foreach(ConsoleTargetTabPage CurPage in Owner.TabPages)
					{
						CurPage.Target.SendCommand(Cmd);
					}
				}
				else
				{
					mTarget.SendCommand(Cmd);
				}
			}
		}

		/// <summary>
		/// Clears the command history state for the tab page.
		/// </summary>
		public void ClearCommandHistoryState()
		{
			mCmdIndex = -1;
		}

		/// <summary>
		/// Loads the currently selected command in the command history.
		/// </summary>
		private void LoadCurrentCommand()
		{
			AutoCompleteStringCollection CmdHistory = UnrealConsoleWindow.CurrentSettings.CommandHistory;
			if(CmdHistory != null)
			{
				if(mCmdIndex >= 0 && mCmdIndex < CmdHistory.Count)
				{
                    string History = CmdHistory[mCmdIndex];
                    mCommand.Text = History;
                    mCommand.SelectAll();
				}
			}
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="e">Information about the event.</param>
		protected override void OnTabSelected(EventArgs e)
		{
			base.OnTabSelected(e);
			mCommand.Focus();

			this.TabForegroundColor = SystemBrushes.WindowText;
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mBtnSend_Click(object sender, EventArgs e)
		{
			ExecuteCommand();
            mCommand.Focus();
		}

		public void Cleanup()
		{
			mTarget.SetTTYCallback(null);
			mOutputTimer.Stop();
		}

		/// <summary>
		/// Cleans up resources when the control is being destroyed.
		/// </summary>
		/// <param name="disposing">True if disposing managed resources.</param>
		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);
		}

		/// <summary>
		/// Event handler for selecting all TTY output text.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mSelectAllToolStripMenuItem_Click(object sender, EventArgs e)
		{
			mOutputWindow.SelectAll();
		}

		/// <summary>
		/// Event handler for copying selected TTY output text to the clipboard.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void mCopyToolStripMenuItem_Click(object sender, EventArgs e)
		{
			mOutputWindow.CopySelectedText();
		}

		/// <summary>
		/// Saves the dump type to the user settings.
		/// </summary>
		public void SaveDumpType()
		{
			UnrealConsoleWindow.CurrentSettings.TargetDumpTypes.SetValue(mTarget.TargetManagerName, mTarget.CrashDumpType);
		}
	}
}
