/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Ipc;
using System.Text;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;
using ConsoleInterface;
using UnrealConsoleRemoting;
using UnrealControls;

namespace UnrealConsole
{
	public partial class UnrealConsoleWindow : Form, IUnrealConsole
	{
		public bool Ticking = false;

		// This is used to marshal calls to Print() into the UI thread
		private delegate void PrintDelegate( string Text );
		private delegate void UIThreadCrashDelegate( string Callstack );
		private delegate bool OpenTargetDelegate( string Platform, string Target, bool bClearOutputWindow );
		private delegate ConsoleInterface.PlatformTarget FindTargetDelegate( string Platform, string Target );
		private bool mSendCommandToAll;

		/** 
		 * The settings structure that stores all user preferences
		 */
		static public UCSettings CurrentSettings = null;

		public class UCSettings
		{
			public Size UCWindowSize = new Size( 800, 600 );

			public bool AlwaysLog = false;
			public bool ShowAllTargetInfo = false;

			public TargetDumpTypeCollection TargetDumpTypes = new TargetDumpTypeCollection();
			public CrashReportFilter CrashFilter = CrashReportFilter.All;
			public AutoCompleteStringCollection CommandHistory = new AutoCompleteStringCollection();

			public void Clean()
			{
				// Filter \r\n from the history if it got polluted earlier
				for (int i = 0; i < CommandHistory.Count; ++i)
				{
					string History = CommandHistory[i];
					History = History.Replace("\n", "");
					History = History.Replace("\r", "");
					CommandHistory[i] = History;
				}
			}

            public UCSettings()
            {
            }
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		public UnrealConsoleWindow()
		{
			// Required for Windows Form Designer support
			InitializeComponent();
		}

		public bool Init( string InPlatform, string InTargetName )
		{
			// force load all of the platforms
			DLLInterface.LoadPlatforms( PlatformType.All );

			LoadSettings();

			if( InPlatform != null && InTargetName != null )
			{
				ConsoleInterface.Platform Plat = FindPlatform( InPlatform );
				if( Plat != null )
				{
					Plat.EnumerateAvailableTargets();
				}
			}

			// set the target
			SetTarget( InPlatform, InTargetName );

			RemoteUCObject.InternalUnrealConsole = this;

			Process CurProc = Process.GetCurrentProcess();
			IpcChannel Channel = new IpcChannel( CurProc.Id.ToString() );
			ChannelServices.RegisterChannel( Channel, false );

			RemotingConfiguration.RegisterWellKnownServiceType( typeof( RemoteUCObject ), CurProc.Id.ToString(), WellKnownObjectMode.SingleCall );

			Console.WriteLine( "Remoting server created!" );
			Console.WriteLine( "Remoting channel name: {0}", Channel.ChannelName );
			Console.WriteLine( "Remoting channel priority: {0}", Channel.ChannelPriority.ToString() );
			Console.WriteLine( "Channel URI's:" );

			// Show the URIs associated with the channel.
			System.Runtime.Remoting.Channels.ChannelDataStore ChannelData = ( System.Runtime.Remoting.Channels.ChannelDataStore )Channel.ChannelData;

			foreach( string URI in ChannelData.ChannelUris )
			{
				Console.WriteLine( URI );
			}

			Console.WriteLine( "Channel URL's:" );

			// Parse the channel's URI.
			string[] Urls = Channel.GetUrlsForUri( CurProc.Id.ToString() );
			if( Urls.Length > 0 )
			{
				string ObjectUrl = Urls[0];
				string ObjectUri;
				string ChannelUri = Channel.Parse( ObjectUrl, out ObjectUri );

				Console.WriteLine( "Object URI: {0}.", ObjectUri );
				Console.WriteLine( "Channel URI: {0}.", ChannelUri );
				Console.WriteLine( "Object URL: {0}.", ObjectUrl );
				Console.WriteLine( "+++++++++++++++++++++++++++++++++++++++" );
			}

			CurProc.Dispose();
			Show();

			return ( true );
		}

		public void Destroy()
		{
		}

		public void Run()
		{
#if false
			// Tick all the connected targets and acquire their state
			TickTargetStates();
#endif
		}

		/// <summary>
		/// Loads user settings.
		/// </summary>
		void LoadSettings()
		{
			CurrentSettings = UnrealControls.XmlHandler.ReadXml<UCSettings>( Path.Combine( Application.StartupPath, "UnrealConsole.Settings.xml" ) );
			CurrentSettings.Clean();

			Size = CurrentSettings.UCWindowSize;
			IDM_ALWAYS_LOG.Checked = CurrentSettings.AlwaysLog;
		}

		/// <summary>
		/// Saves user settings.
		/// </summary>
		void WriteSettings()
		{
			CurrentSettings.UCWindowSize = Size;
			CurrentSettings.AlwaysLog = IDM_ALWAYS_LOG.Checked;

			UnrealControls.XmlHandler.WriteXml<UCSettings>( CurrentSettings, Path.Combine( Application.StartupPath, "UnrealConsole.Settings.xml" ), "" );
		}

		/// <summary>
		/// Callback for when the form is closing.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		protected override void OnClosing(CancelEventArgs e)
		{
			base.OnClosing(e);

			WriteSettings();
			Ticking = false;
		}

		/// <summary>
		/// Callback for clicking the Exit menu option.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void OnExit(object sender, System.EventArgs e)
		{
			this.Close();
		}
#if false
		/// <summary>
		/// Get the state of all connected targets 
		/// </summary>
		private DateTime LastGetStateTime = DateTime.UtcNow;
		private void TickTargetStates()
		{
			if( DateTime.UtcNow > LastGetStateTime.AddSeconds( 2 ) )
			{
				foreach( ConsoleTargetTabPage TabPage in mMainTabControl.TabPages )
				{
					TargetState State = TabPage.Target.State;
					TabPage.Print( State.ToString() );
				}

				LastGetStateTime = DateTime.UtcNow;
			}
		}
#endif
		/// <summary>
		/// Finds a target with the specified name/IP on the specified platform.
		/// </summary>
		/// <param name="NewPlatformName">The platform the target belongs to.</param>
		/// <param name="NewTargetName">The name, debug channel IP address, or title IP address for the target to retrieve.</param>
		/// <returns>null if the target could not be found.</returns>
		static ConsoleInterface.PlatformTarget FindTarget( string NewPlatformName, string NewTargetName )
		{
			ConsoleInterface.PlatformTarget RetTarget = null;

			if( NewPlatformName != null && NewTargetName != null )
			{
				ConsoleInterface.Platform NewPlatform = FindPlatform( NewPlatformName );
				if( NewPlatform != null )
				{
					// Have targets been enumerated?
					if( NewPlatform.TargetEnumerationCount == 0 )
					{
						NewPlatform.EnumerateAvailableTargets();
					}

					// Get the address for this target
					IPAddress Addr;
					IPAddress.TryParse( NewTargetName, out Addr );

					if( Addr != null && BitConverter.ToUInt32( Addr.GetAddressBytes(), 0 ) == 0 )
					{
						Addr = null;
					}

					// Find a match in the target list
					foreach( ConsoleInterface.PlatformTarget CurTarget in NewPlatform.Targets )
					{
						if( Addr != null )
						{
							if( CurTarget.DebugIPAddress.Equals( Addr ) || CurTarget.IPAddress.Equals( Addr ) )
							{
								RetTarget = CurTarget;
								break;
							}
						}

						if( CurTarget.Name.Equals( NewTargetName, StringComparison.InvariantCultureIgnoreCase ) )
						{
							RetTarget = CurTarget;
							break;
						}
					}

					// Need to create a target that isn't connected yet, but valid enough for UnrealConsole
					if( RetTarget == null )
					{
						RetTarget = NewPlatform.ForceAddTarget( NewTargetName );
					}
				}
			}

			return RetTarget;
		}

		/// <summary>
		/// Finds the platform with the supplied name.
		/// </summary>
		/// <param name="NewPlatformName">The name of the platform to search for.</param>
		/// <returns>null if the platform with the specified name could not be found.</returns>
		private static ConsoleInterface.Platform FindPlatform( string NewPlatformName )
		{
			ConsoleInterface.Platform NewPlatform = null;

			foreach( ConsoleInterface.Platform CurPlatform in DLLInterface.Platforms )
			{
				if( CurPlatform.Name.Equals( NewPlatformName, StringComparison.InvariantCultureIgnoreCase ) )
				{
					NewPlatform = CurPlatform;
					break;
				}
			}
			return NewPlatform;
		}

		/// <summary>
		/// Sets the target, and if no target is given, then it will popup a dialog to choose one
		/// </summary>
		private void SetTarget( string NewPlatformName, string NewTargetName )
		{
			// Try to find the target (or create a null array)
			ConsoleInterface.PlatformTarget[] NewTargets = new PlatformTarget[] { FindTarget( NewPlatformName, NewTargetName ) };

			// If no platform or target passed in, bring up the request dialog
			if( NewPlatformName == null || NewTargetName == null )
			{
				using( NetworkConnectionDialog Dlg = new NetworkConnectionDialog() )
				{
					if( Dlg.ShowDialog() == DialogResult.OK )
					{
						NewTargets = Dlg.SelectedTargets;
					}
					else
					{
						return;
					}
				}
			}

			// If we have found some platforms, set up the tabs
			foreach( PlatformTarget CurTarg in NewTargets )
			{
				ConsoleTargetTabPage TargetTab = FindTargetTab( CurTarg );

				if( TargetTab != null )
				{
					mMainTabControl.TabPages.Remove( TargetTab );
				}

				if( CurTarg != null )
				{
					mMainTabControl.TabPages.Add( new ConsoleTargetTabPage( this, CurTarg, IDM_ALWAYS_LOG.Checked, mSendCommandToAll ) );
				}
			}

			reconnectTargetToolStripMenuItem.Enabled = mMainTabControl.TabPages.Count > 0;
		}

		/// <summary>
		/// Finds the tab that owns the specified target.
		/// </summary>
		/// <param name="PlatformName">The platform the target belongs to.</param>
		/// <param name="TargetName">The name, debug channel IP, or title IP of the requested target.</param>
		/// <returns>null if the tab that the specified target belongs to does not exist.</returns>
		ConsoleTargetTabPage FindTargetTab(string PlatformName, string TargetName)
		{
			ConsoleInterface.PlatformTarget Targ = FindTarget( PlatformName, TargetName );

			if( Targ != null )
			{
				return FindTargetTab( Targ );
			}

			return null;
		}

		/// <summary>
		/// Find the tab that owns the specified target.
		/// </summary>
		/// <param name="Target">The target who's tab is being searched for.</param>
		/// <returns>null if the tab could not be found.</returns>
		ConsoleTargetTabPage FindTargetTab(ConsoleInterface.PlatformTarget Target)
		{
			foreach( ConsoleTargetTabPage CurTab in mMainTabControl.TabPages )
			{
				if( CurTab.Target == Target )
				{
					return CurTab;
				}
			}

			return null;
		}

		/// <summary>
		/// Callback for connection to a target.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CONNECT_Click(object sender, EventArgs e)
		{
			// change targets with the popup dialog ("NullPlatform" to make sure the dialog pops up)
			SetTarget( null, null );
		}

		/// <summary>
		/// Callback for clearing the TTY buffer.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void OnClearWindow(object sender, System.EventArgs e)
		{
			ConsoleTargetTabPage CurTab = ( ConsoleTargetTabPage )mMainTabControl.SelectedTab;

			if( CurTab != null )
			{
				CurTab.ClearOutputWindow();
			}
		}

		/// <summary>
		/// Callback for rebooting the current target.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_REBOOT_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = ( ConsoleTargetTabPage )mMainTabControl.SelectedTab;

			if( CurTab != null )
			{
				CurTab.Target.Reboot();
			}
		}

		/// <summary>
		/// Callback for capturing a screen shot from the current target.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_SCREENCAPTURE_Click(object sender, EventArgs e)
		{
			CaptureScreenshot();
		}

		/// <summary>
		/// Capture a screenshot from the device, save it in a temporary location on disk, and open a preview of it.
		/// </summary>
		public void CaptureScreenshot()
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if (CurTab != null)
			{
                String PNGOutputPath = Path.Combine("..", "Binaries");

                if ((CurTab.Target.ParentPlatform.Type == PlatformType.PS3) || (CurTab.Target.ParentPlatform.Type == PlatformType.WiiU))
                {
                    PNGOutputPath = "";
                }
 
				PNGOutputPath = Path.Combine(PNGOutputPath, CurTab.Target.ParentPlatform.ToString());
				PNGOutputPath = Path.Combine(PNGOutputPath, "Screenshots");
				PNGOutputPath = Path.Combine(PNGOutputPath, Path.GetFileNameWithoutExtension(Path.GetTempFileName())+".bmp");

				// Ensure that the target directory exists.
				String OutputDir = Path.GetDirectoryName(PNGOutputPath);
				if (!Directory.Exists(OutputDir))
				{
					Directory.CreateDirectory(OutputDir);
				}

				// make DLL grab the screenshot
				if (!CurTab.Target.ScreenShot(PNGOutputPath))
				{
					MessageBox.Show("Failed to take a screenshot.");
					return;
				}

				Image Screenshot = null;
				try
				{
					Screenshot = Bitmap.FromFile(PNGOutputPath);
			
					ScreenShotForm NewFrm = new ScreenShotForm(this, string.Format("Screenshot from \'{0}\' taken at {3:h:mm:ss tt}: {1}x{2}", CurTab.Target.Name, Screenshot.Width, Screenshot.Height, DateTime.Now), Screenshot);

					// Normally this would be bad because we are never calling Dispose() on NewFrm but since
					// ScreenShotForm calls Dispose() in its OnClosed() for this very reason it's not an issue
					NewFrm.Show();
				}
				catch (Exception ex)
				{
					using (ExceptionBox Box = new ExceptionBox(ex))
					{
						Box.ShowDialog(this);
					}
				}
			}
		}

		/// <summary>
		/// Callback for saving the text buffer to disk.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_SAVE_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = ( ConsoleTargetTabPage )mMainTabControl.SelectedTab;

			if( CurTab != null )
			{
				// save the log window to a text file
				using( SaveFileDialog Dialog = new SaveFileDialog() )
				{
					Dialog.DefaultExt = "txt";
					Dialog.FileName = string.Format( "{0}_{1}.txt", CurTab.Target.Name, CurTab.Target.ParentPlatform.Name );
					Dialog.Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*";

					if( Dialog.ShowDialog( this ) == DialogResult.OK && Dialog.FileName.Length > 0 )
					{
						// write the string to a file
						try
						{
							CurTab.TTYText.SaveToFile( Dialog.FileName );
						}
						catch( Exception )
						{
							MessageBox.Show( this, string.Format( "Could not save \'{0}\'!", Dialog.FileName ), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
						}
					}
				}
			}
        }

		/// <summary>
		/// Callback for saving the text buffer of all tabs to disk.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_SAVEALL_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurSelectedTab = ( ConsoleTargetTabPage )mMainTabControl.SelectedTab;

			if( CurSelectedTab != null )
			{
				// save the log window to a text file
				using( SaveFileDialog Dialog = new SaveFileDialog() )
				{
					Dialog.DefaultExt = "txt";
					Dialog.FileName = "%TARGETNAME%_%PLATFORMNAME%.txt";
					Dialog.Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*";

					if( Dialog.ShowDialog( this ) == DialogResult.OK && Dialog.FileName.Length > 0 )
					{
						// write the string to a file

						foreach( ConsoleTargetTabPage CurTab in mMainTabControl.TabPages )
						{
							string CurFileName = Dialog.FileName.Replace( "%TARGETNAME%", CurTab.Target.Name ).Replace( "%PLATFORMNAME%", CurTab.Target.ParentPlatform.Type.ToString() );

							try
							{
								CurTab.TTYText.SaveToFile( CurFileName );
							}
							catch( Exception )
							{
								MessageBox.Show( this, string.Format( "Could not save \'{0}\'!", CurFileName ), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
							}
						}
					}
				}
			}
        }

		/// <summary>
		/// Callback for enabling logging.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
        private void IDM_ALWAYS_LOG_Click(object sender, EventArgs e)
        {
			foreach( ConsoleTargetTabPage CurTab in mMainTabControl.TabPages )
			{
				CurTab.LogOutput = IDM_ALWAYS_LOG.Checked;
			}
        }

		/// <summary>
		/// Callback for bringing up the Find dialog box.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void Menu_EditFind_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				CurTab.TTYText.EnterFindMode();
			}
		}

		/// <summary>
		/// Callback scrolls the cursor to the end of the TTY text buffer.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void Menu_ScrollToEnd_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				CurTab.TTYText.Focus();
				CurTab.TTYText.ScrollToEnd();
			}
		}

		/// <summary>
		/// Callback for Find Next.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void Menu_InvisFindNext_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				CurTab.TTYText.FindNext();
			}
		}

		/// <summary>
		/// Callback for Find Previous.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void Menu_InvisFindPrev_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				CurTab.TTYText.FindPrevious();
			}
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void MainTabControl_ControlRemoved(object sender, ControlEventArgs e)
		{
			ConsoleTargetTabPage Tab = e.Control as ConsoleTargetTabPage;

			if(Tab != null)
			{
				Tab.Dispose();
			}
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CLEAR_CMD_HISTORY_Click(object sender, EventArgs e)
		{
			UnrealConsoleWindow.CurrentSettings.CommandHistory.Clear();

			foreach(ConsoleTargetTabPage CurPage in mMainTabControl.TabPages)
			{
				CurPage.ClearCommandHistoryState();
			}
		}

		/// <summary>
		/// A version of <see cref="OpenTarget"/> that gets called on the UI thread.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <param name="bClearOutputWindow">True if the output for the specified target is to be cleared.</param>
		/// <returns>True if the target is located.</returns>
		private bool UIThreadOpenTarget(string Platform, string Target, bool bClearOutputWindow)
		{
			if( this.WindowState == FormWindowState.Minimized )
			{
				this.WindowState = FormWindowState.Normal;
			}

			this.Activate();

			PlatformTarget PlatTarget = FindTarget( Platform, Target );

			if( PlatTarget == null )
			{
				Debug.WriteLine( "Platform null" );
				return false;
			}

			ConsoleTargetTabPage Tab = FindTargetTab( PlatTarget );

			if( Tab == null )
			{
				Debug.WriteLine( "tab null" );
				mMainTabControl.TabPages.Add( new ConsoleTargetTabPage( this, PlatTarget, IDM_ALWAYS_LOG.Checked, mSendCommandToAll ) );
				reconnectTargetToolStripMenuItem.Enabled = mMainTabControl.TabPages.Count > 0;
			}
			else
			{
				Debug.WriteLine( "tab not null" );
				mMainTabControl.SelectedTab = Tab;

				if( bClearOutputWindow )
				{
					Tab.TTYText.Clear();
				}
			}

			return true;
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CONSOLE_DropDownOpening(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if (CurTab != null)
			{
				IDM_DELETE_PDB.Enabled = CurTab.Target.ParentPlatform.Type == ConsoleInterface.PlatformType.Xbox360;

				if (CurTab.Target.CrashDumpType == DumpType.Normal)
				{
					IDM_DUMP_NORMAL.Checked = true;
					IDM_DUMP_WITHFULLMEM.Checked = false;
				}
				else
				{
					IDM_DUMP_NORMAL.Checked = false;
					IDM_DUMP_WITHFULLMEM.Checked = true;
				}
			}
			reconnectTargetToolStripMenuItem.Enabled = CurTab != null;
		}

		/// <summary>
		/// Event handler.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_DELETE_PDB_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				string TargetName = CurTab.Target.Name;
				string[] PDBs = Directory.GetFiles(string.Format("Temp\\{0}", TargetName), "*.pdb", SearchOption.AllDirectories);
				int NumDeleted = 0;

				foreach(string CurPDB in PDBs)
				{
					try
					{
						File.Delete(CurPDB);
						++NumDeleted;
					}
					catch(Exception ex)
					{
						System.Diagnostics.Debug.WriteLine(ex.ToString());
					}
				}

				MessageBox.Show(this, "Deleted {0} pdb's.", TargetName, MessageBoxButtons.OK, MessageBoxIcon.Information);
			}
		}

		/// <summary>
		/// Global event handler for key down messages.
		/// </summary>
		/// <param name="e">Information about the event.</param>
		protected override void OnKeyDown(KeyEventArgs e)
		{
			if(e.KeyCode == Keys.ControlKey)
			{
				mSendCommandToAll = true;

				foreach(ConsoleTargetTabPage CurTab in mMainTabControl.TabPages)
				{
					CurTab.SendCommandsToAll = mSendCommandToAll;
				}
			}

			base.OnKeyDown(e);
		}

		/// <summary>
		/// Global event handler for key up messages.
		/// </summary>
		/// <param name="e">Information about the event.</param>
		protected override void OnKeyUp(KeyEventArgs e)
		{
			if(e.KeyCode == Keys.ControlKey)
			{
				mSendCommandToAll = false;

				foreach(ConsoleTargetTabPage CurTab in mMainTabControl.TabPages)
				{
					CurTab.SendCommandsToAll = mSendCommandToAll;
				}
			}

			base.OnKeyUp(e);
		}

		/// <summary>
		/// Event handler for when the window loses focus.
		/// </summary>
		/// <param name="e">Information about the event.</param>
		protected override void OnDeactivate(EventArgs e)
		{
			base.OnDeactivate(e);

			mSendCommandToAll = false;

			foreach(ConsoleTargetTabPage CurTab in mMainTabControl.TabPages)
			{
				CurTab.SendCommandsToAll = mSendCommandToAll;
			}
		}

		/// <summary>
		/// Event handler for clearing the text of every window.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CLEARALLWINDOWS_Click(object sender, EventArgs e)
		{
			foreach(ConsoleTargetTabPage CurTab in mMainTabControl.TabPages)
			{
				CurTab.TTYText.Clear();
			}
		}

		/// <summary>
		/// Event handler for updating the menu options for the crash report filters.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHREPORTFILTER_DropDownOpening(object sender, EventArgs e)
		{
			IDM_CRASHFILTER_DEBUG.Checked = (UnrealConsoleWindow.CurrentSettings.CrashFilter & CrashReportFilter.Debug) == CrashReportFilter.Debug;
			IDM_CRASHFILTER_RELEASE.Checked = ( UnrealConsoleWindow.CurrentSettings.CrashFilter & CrashReportFilter.Release ) == CrashReportFilter.Release;
			IDM_CRASHFILTER_SHIPPING.Checked = ( UnrealConsoleWindow.CurrentSettings.CrashFilter & CrashReportFilter.Shipping ) == CrashReportFilter.Shipping;
			IDM_CRASHFILTER_TEST.Checked = ( UnrealConsoleWindow.CurrentSettings.CrashFilter & CrashReportFilter.Test ) == CrashReportFilter.Test;
		}

		/// <summary>
		/// Event handler for selecting all crash report filters.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHFILTER_SELECTALL_Click(object sender, EventArgs e)
		{
			UnrealConsoleWindow.CurrentSettings.CrashFilter = CrashReportFilter.All;
			UpdateTargetFilterStates();
		}

		/// <summary>
		/// Event handler for deselecting all crash report filters.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHFILTER_DESELECTALL_Click(object sender, EventArgs e)
		{
			UnrealConsoleWindow.CurrentSettings.CrashFilter = CrashReportFilter.None;
			UpdateTargetFilterStates();
		}

		/// <summary>
		/// Event handler for toggling the debug crash report filtering state.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHFILTER_DEBUG_Click(object sender, EventArgs e)
		{
			if(IDM_CRASHFILTER_DEBUG.Checked)
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter &= ~CrashReportFilter.Debug;
			}
			else
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter |= CrashReportFilter.Debug;
			}

			UpdateTargetFilterStates();
		}

		/// <summary>
		/// Event handler for toggling the release crash report filtering state.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHFILTER_RELEASE_Click(object sender, EventArgs e)
		{
			if(IDM_CRASHFILTER_RELEASE.Checked)
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter &= ~CrashReportFilter.Release;
			}
			else
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter |= CrashReportFilter.Release;
			}

			UpdateTargetFilterStates();
		}

		/// <summary>
		/// Event handler for toggling the shipping crash report filtering state.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHFILTER_SHIPPING_Click(object sender, EventArgs e)
		{
			if( IDM_CRASHFILTER_SHIPPING.Checked )
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter &= ~CrashReportFilter.Shipping;
			}
			else
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter |= CrashReportFilter.Shipping;
			}

			UpdateTargetFilterStates();
		}

		/// <summary>
		/// Event handler for toggling the test crash report filtering state.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_CRASHFILTER_TEST_Click( object sender, EventArgs e )
		{
			if( IDM_CRASHFILTER_TEST.Checked )
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter &= ~CrashReportFilter.Test;
			}
			else
			{
				UnrealConsoleWindow.CurrentSettings.CrashFilter |= CrashReportFilter.Test;
			}

			UpdateTargetFilterStates();
		}

		/// <summary>
		/// Updates the filtering state for all tab pages.
		/// </summary>
		private void UpdateTargetFilterStates()
		{
			foreach(ConsoleTargetTabPage CurTab in mMainTabControl.TabPages)
			{
				CurTab.Target.CrashFilter = UnrealConsoleWindow.CurrentSettings.CrashFilter;
			}
		}

		/// <summary>
		/// Event handler for setting the dump type to Normal.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_DUMP_NORMAL_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				if(!IDM_DUMP_NORMAL.Checked)
				{
					CurTab.Target.CrashDumpType = DumpType.Normal;
					CurTab.SaveDumpType();
				}
			}
		}

		/// <summary>
		/// Event handler for setting the dump type to WithFullMemory.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_DUMP_WITHFULLMEM_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if(CurTab != null)
			{
				if(!IDM_DUMP_WITHFULLMEM.Checked)
				{
					CurTab.Target.CrashDumpType = DumpType.WithFullMemory;
					CurTab.SaveDumpType();
				}
			}
		}

		#region IUnrealConsole Members

		/// <summary>
		/// Opens a tab for the specified target.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <param name="bClearOutputWindow">True if the output for the specified target is to be cleared.</param>
		/// <returns>True if the target is located.</returns>
		public bool OpenTarget(string Platform, string Target, bool bClearOutputWindow)
		{
			// marshal to the UI thread
			return (bool)this.Invoke(new OpenTargetDelegate(this.UIThreadOpenTarget), Platform, Target, bClearOutputWindow);
		}

		/// <summary>
		/// Searches for the specified target.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <returns>True if the target is located.</returns>
		public bool HasTarget(string Platform, string Target)
		{
			return this.Invoke(new FindTargetDelegate(UnrealConsoleWindow.FindTarget), Platform, Target) != null;
		}

		#endregion

		/// <summary>
		/// Event handler for when the About menu item is clicked.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void IDM_HELP_ABOUT_Click(object sender, EventArgs e)
		{
			using(UnrealControls.UnrealAboutBox Dlg = new UnrealAboutBox(this.Icon, null))
			{
				Dlg.Text = "About UnrealConsole";
				Dlg.ShowDialog(this);
			}
		}

		private void reconnectTargetToolStripMenuItem_Click(object sender, EventArgs e)
		{
			ConsoleTargetTabPage CurTab = (ConsoleTargetTabPage)mMainTabControl.SelectedTab;

			if (CurTab != null)
			{
				CurTab.Target.Connect();
			}

		}
	}
}