// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;

using Builder.UnrealSync.Properties;

namespace Builder.UnrealSync
{
	/// <summary>
	/// The type of syncs exposed to the UI tailored to the user
	/// </summary>
	public enum EUserType
	{
		ContentCreator,
		Programmer,
	}

	/// <summary>
	/// The type of sync to be performed
	/// </summary>
	public enum ESyncType
	{
		Head,
		LatestGoodCIS,
		LatestBuild,
		LatestPromotedBuild,
		LatestQABuild,
		ArtistSyncGame,
		LaunchEditor,
	}

	/// <summary>
	/// The type of resolve to be applied to files that need resolution
	/// </summary>
	public enum EResolveType
	{
		None,
		Automatic,				// -am
		SafeAutomatic,			// -as
		AutoWithConflicts		// -af
	}

	public partial class UnrealSync2 : Form
	{
		public bool bTicking = false;
		public bool bRestart = false;
		public bool bAllGamesAreUpToDate = true;
		public bool bNewBuildAcknowledged = true;
		public P4 SCC = null;
		public SettableOptions Options = null;

		/** List of unique servers that the build system handles */
		public List<string> PerforceServers = null;

		/** List of clientspec/branch combinations available on this host */
		public List<BranchSpec> BranchSpecs = null;

		/** List of pending sync operations */
		public ReaderWriterQueue<BranchSpec> PendingSyncs = new ReaderWriterQueue<BranchSpec>();

		/** The default number of entries in the context menu */
		private int DefaultContextMenuItemCount = 0;

		/** A list of icons that make the notify tray animation */
		private Icon[] BusyAnimationIcons = new Icon[]
		{
			Icon.FromHandle( Properties.Resources.Sync_00.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_01.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_02.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_03.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_04.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_05.GetHicon() ),
		};

		/** A list of icons that make the notify tray animation */
		private Icon[] ErrorAnimationIcons = new Icon[]
 		{
 			Icon.FromHandle( Properties.Resources.Sync_00.GetHicon() ),
 			Icon.FromHandle( Properties.Resources.Sync_Error.GetHicon() ),
 		};

		private Icon[] NewBuildAnimationIcons = new Icon[]
		{
 			Icon.FromHandle( Properties.Resources.UpdateAvailable.GetHicon() ),
		};

		/** An event to control the sync animation of the notify tray icon */
		private ManualResetEvent SyncingEvent = new ManualResetEvent( false );
		private ManualResetEvent ErrorEvent = new ManualResetEvent( false );

		/** Last time the db was queried for a newly promoted build */
		private DateTime LastDataBasePollTime = DateTime.MinValue;

		/** Last time the db was queried for a newly promoted build */
		private DateTime LastBuildAckPollTime = DateTime.MinValue;

		/** Last time the log folder was checked for old logs */
		private DateTime LastCleanLogTime = DateTime.MinValue;

		/** Cached info about the most recent balloon popup */
		private string CachedBalloonTitle = "";
		private string CachedBalloonText = "";
		private ToolTipIcon CachedBalloonIcon = ToolTipIcon.None;

		/** Handle to LogViewer window; private to allow suppression of multiple windows */
		public BranchConfigure ConfigureBranches = null;
		public ArtistSyncConfigure ConfigureArtistSync = null;
		public LogViewer ViewLogs = null;

		public UnrealSync2()
		{
		}

		private string GetOptionsFileName()
		{
			string BaseDirectory = Application.StartupPath;
			if( ApplicationDeployment.IsNetworkDeployed )
			{
				BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
			}

			string FullPath = Path.Combine( BaseDirectory, "UnrealSync2.Settings.xml" );
			return ( FullPath );
		}

		private ToolStripMenuItem CreateSyncMenuItem( BranchSpec Branch, string Text, string SyncType )
		{
			ToolStripMenuItem MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			MenuItem.Name = SyncType;
			MenuItem.Size = new System.Drawing.Size( 152, 22 );
			MenuItem.Text = Text;
			MenuItem.Checked = false;
			MenuItem.Click += new System.EventHandler( MenuItemClick );
			MenuItem.Tag = ( object )Branch;

			return ( MenuItem );
		}

		private ToolStripMenuItem CreateSyncMenuItem( BranchSpec Branch )
		{
			ToolStripMenuItem MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			MenuItem.Name = Branch.SyncType.ToString();
			MenuItem.Size = new System.Drawing.Size( 152, 22 );
			MenuItem.Text = Branch.MRUString();
			MenuItem.Checked = false;
			MenuItem.Click += new System.EventHandler( MRUMenuItemClick );
			MenuItem.Tag = ( object )Branch;

			return ( MenuItem );
		}

		private ToolStripMenuItem CreateBranchMenuItem( BranchSpec Branch )
		{
			ToolStripMenuItem MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			MenuItem.Name = Branch.MRUString() + "ContextMenuItem";
			MenuItem.Size = new System.Drawing.Size( 152, 22 );
			MenuItem.Text = Branch.Name;
			MenuItem.Checked = false;

			return ( MenuItem );
		}

		private ToolStripMenuItem CreateClientSpecMenuItem( string Name )
		{
			ToolStripMenuItem ClientSpec = new System.Windows.Forms.ToolStripMenuItem();
			ClientSpec.Name = Name + "ContextMenuItem";
			ClientSpec.Size = new System.Drawing.Size( 152, 22 );
			ClientSpec.Text = Name;

			return ( ClientSpec );
		}

		private void AddBranches( ToolStripMenuItem ClientSpecMenuItem )
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				if( Branch.bDisplayInMenu && Branch.ClientSpec == ClientSpecMenuItem.Text )
				{
					Branch.MenuItem = CreateBranchMenuItem( Branch );

					// Clear out the old options
					Branch.MenuItem.DropDownItems.Clear();

					// Add in the new options depending on the user type
					switch( Options.SyncUserType )
					{
					case EUserType.Programmer:
						AddProgrammerSyncTypes( Branch );
						break;

					case EUserType.ContentCreator:
						AddContentCreatorSyncTypes( Branch );
						break;
					}

					if( Branch.MenuItem.DropDown.Items.Count > 0 )
					{
						ClientSpecMenuItem.DropDownItems.Add( Branch.MenuItem );
					}
				}
			}
		}

		public List<string> GetUniqueClientSpecs( bool bInclusive )
		{
			List<string> UniqueClientSpecs = new List<string>();

			foreach( BranchSpec Branch in BranchSpecs )
			{
				if( bInclusive || Branch.bDisplayInMenu )
				{
					if( !UniqueClientSpecs.Contains( Branch.ClientSpec ) )
					{
						UniqueClientSpecs.Add( Branch.ClientSpec );
					}
				}
			}

			return ( UniqueClientSpecs );
		}

		private void CreateContextMenu()
		{
			UnrealSyncContextMenu.SuspendLayout();

			// Clear out any existing branch options (above the top delimiter)
			int Index = 0;
			while( UnrealSyncContextMenu.Items.Count > DefaultContextMenuItemCount )
			{
				if( UnrealSyncContextMenu.Items[Index].GetType() != typeof( ToolStripSeparator ) )
				{
					UnrealSyncContextMenu.Items.RemoveAt( Index );
				}
				else
				{
					Index++;
				}
			}

			// Delete existing MRU entries
			while( Options.MRUSyncs.Count > 3 )
			{
				Options.MRUSyncs.RemoveAt( 0 );
			}

			// Insert the MRU entries inbetween the delimiters
			foreach( BranchSpec BranchInfo in Options.MRUSyncs )
			{
				ToolStripMenuItem MRUMenuItem = CreateSyncMenuItem( BranchInfo );
				UnrealSyncContextMenu.Items.Insert( 1, MRUMenuItem );
			}

			// Find and create unique clientspecs
			List<string> UniqueClientSpecs = GetUniqueClientSpecs( false );

			// Create a entry in the context menu for the clientspec
			foreach( string ClientSpec in UniqueClientSpecs )
			{
				ToolStripMenuItem ClientSpecMenuItem = CreateClientSpecMenuItem( ClientSpec );
				AddBranches( ClientSpecMenuItem );

				if( ClientSpecMenuItem.DropDownItems.Count > 0 )
				{
					UnrealSyncContextMenu.Items.Insert( 0, ClientSpecMenuItem );
				}
			}

			UnrealSyncContextMenu.ResumeLayout( true );
		}

		private void AddProgrammerSyncTypes( BranchSpec Branch )
		{
			Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync to head", ESyncType.Head.ToString() ) );

			if( Branch.LatestGoodCISChangelist > 0 )
			{
				Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest good CIS", ESyncType.LatestGoodCIS.ToString() ) );
			}

			Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest build", ESyncType.LatestBuild.ToString() ) );

			if( Branch.bIsMain )
			{
				Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest QA build", ESyncType.LatestQABuild.ToString() ) );
			}

			Branch.MenuItem.DropDown.Items.Add( "-" );
			Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Explore", "Explore" ) );
		}

		private void AddContentCreatorSyncTypes( BranchSpec Branch )
		{
			foreach( PromotableGame PromotableGame in Branch.PromotableGames )
			{
				if( PromotableGame.bDisplayInMenus )
				{
					ToolStripMenuItem Item = CreateSyncMenuItem( Branch, PromotableGame.GameName + " Artist Sync", ESyncType.ArtistSyncGame.ToString() );
					Branch.MenuItem.DropDown.Items.Add( Item );

					Item = CreateSyncMenuItem( Branch, PromotableGame.GameName + " Launch Editor", ESyncType.LaunchEditor.ToString() );
					Branch.MenuItem.DropDown.Items.Add( Item );
				}
			}
		}

		private void ShowBalloon()
		{
			if( CachedBalloonTitle.Length > 0 )
			{
				UnrealSync2NotifyIcon.BalloonTipTitle = CachedBalloonTitle;
				UnrealSync2NotifyIcon.BalloonTipText = CachedBalloonText;
				UnrealSync2NotifyIcon.BalloonTipIcon = CachedBalloonIcon;
				UnrealSync2NotifyIcon.ShowBalloonTip( ( int )( Options.BalloonDuration * 1000.0f ) );
			}
		}

		private void HandlePreSyncBat( string BranchRoot )
		{
			if( Options.PreSyncExecutable.Length > 0 )
			{
				string PreSyncExecutable = Options.PreSyncExecutable.Replace( "\"", "" );
				UnrealControls.ProcessHandler.SpawnProcessAndWait( PreSyncExecutable, BranchRoot, null, Options.PreSyncExecutableParameters );
			}
		}

		private void HandlePostSyncBat( string BranchRoot, int Version )
		{
			if( Options.AutoGenerateProjectFiles && Version >= 10 && Options.SyncUserType == EUserType.Programmer )
			{
				string Executable = Path.Combine( Environment.GetFolderPath( Environment.SpecialFolder.System ), "cmd.exe" );
				UnrealControls.ProcessHandler.SpawnProcessAndWait( Executable, BranchRoot, null, "/c GenerateProjectFiles.bat" );
			}
		}

		private void CleanFiles( BranchSpec Branch, List<string> FilePatterns )
		{
			foreach( string FilePattern in FilePatterns )
			{
				try
				{
					string FullPath = Path.Combine( Path.Combine( Branch.Root, Branch.Name ), FilePattern );
					string Folder = Path.GetDirectoryName( FullPath );
					string FileSpec = Path.GetFileName( FullPath );

					DirectoryInfo DirInfo = new DirectoryInfo( Folder );
					if( DirInfo.Exists )
					{
						FileInfo[] FileInfos = DirInfo.GetFiles( FileSpec );
						foreach( FileInfo Info in FileInfos )
						{
							Info.IsReadOnly = false;
							Info.Delete();
						}
					}
				}
				catch
				{
				}
			}
		}

		private void LaunchEditor( BranchSpec Branch )
		{
			try
			{
				if( Branch.Version >= 10 )
				{
					string GameExecutable = "Engine\\Binaries\\Win64\\UE4.exe";
					string GameBranchFolder = Path.Combine( Branch.Root, Branch.Name );
					Process.Start( Path.Combine( GameBranchFolder, GameExecutable ), Branch.GameName + " editor" );
				}
				else
				{
					string GameExecutable = "Binaries\\Win64\\" + Branch.GameName + "Game.exe";
					string GameBranchFolder = Path.Combine( Branch.Root, Branch.Name );
					Process.Start( Path.Combine( GameBranchFolder, GameExecutable ), "editor" );
				}
			}
			catch
			{
				MessageBox.Show( "Unable to launch editor!", "Critical Error!", MessageBoxButtons.OK, MessageBoxIcon.Error );
			}
		}

		private void HandleSync( BranchSpec Branch )
		{
			// Handle the sync operation
			switch( Branch.SyncType )
			{
			case ESyncType.Head:
				SCC.SyncRevision( Branch, "#head" );
				break;

			case ESyncType.LatestGoodCIS:
				if( Branch.LatestGoodCISChangelist > 0 )
				{
					SCC.SyncRevision( Branch, "@" + Branch.LatestGoodCISChangelist.ToString() );
				}
				break;

			case ESyncType.LatestBuild:
				if( Branch.LatestBuildLabel.Length > 0 )
				{
					SCC.SyncRevision( Branch, "@" + Branch.LatestBuildLabel );
				}
				break;

			case ESyncType.LatestQABuild:
				if( Branch.LatestQABuildLabel.Length > 0 )
				{
					SCC.SyncRevision( Branch, "@" + Branch.LatestQABuildLabel );
				}
				break;

			case ESyncType.ArtistSyncGame:
				string RulesFileName = Branch.GameName + "Game/Build/ArtistSyncRules.xml";
				SCC.SyncFile( Branch, RulesFileName );

				RulesFileName = Path.Combine( Branch.Root, Path.Combine( Branch.Name, RulesFileName ) );
				ArtistSyncRules Rules = UnrealControls.XmlHandler.ReadXml<ArtistSyncRules>( RulesFileName );
				if( Rules.PromotionLabel.Length > 0 )
				{
					CleanFiles( Branch, Rules.FilesToClean );
					SCC.SyncRevision( Branch, "@" + Rules.PromotionLabel, Rules.Rules );

					if( Options.LaunchEditor )
					{
						LaunchEditor( Branch );
					}
				}
				break;

			case ESyncType.LaunchEditor:
				LaunchEditor( Branch );
				break;

			default:
				break;
			}

			// Display the notify icon if requested
			if( Branch.bShowBalloon && Branch.SyncType != ESyncType.LaunchEditor )
			{
				DisplaySyncComplete();
			}
		}

		/** Consumer thread waiting for sync jobs */
		private void ManageSyncs()
		{
			while( bTicking )
			{
				if( PendingSyncs.Count > 0 )
				{
					SyncingEvent.Reset();

					BranchSpec BranchInfo = PendingSyncs.Dequeue();
					if( BranchInfo != null )
					{
						string BranchRoot = Path.Combine( BranchInfo.Root, BranchInfo.Name );

						HandlePreSyncBat( BranchRoot );
						HandleSync( BranchInfo );
						HandlePostSyncBat( BranchRoot, BranchInfo.Version );
					}

					SetOptions();
					UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );

					// Check to update the state of all the games
					CheckForUpToDatedness();
					// Update the overall icon if necessary
					UpdateOutOfDatedness();

					SyncingEvent.Set();

					if( SCC.SummaryIcon == ToolTipIcon.Error )
					{
						ErrorEvent.Reset();
					}
				}

				Thread.Sleep( 1000 );
			}
		}

		/** 
		 * Store a list of branches that we wish to display in the popup menu in the settings file
		 */
		private void SetOptions()
		{
			Options.Branches.Clear();

			foreach( BranchSpec Branch in BranchSpecs )
			{
				Options.Branches.Add( Branch );
			}
		}

		public void SetPromotableGameSyncTime( BranchSpec Branch, string GameName, bool bDisplay, DateTime SyncTime )
		{
			foreach( PromotableGame Game in Branch.PromotableGames )
			{
				if( Game.GameName == GameName )
				{
					Game.bDisplayInMenus = bDisplay;
					Game.SyncTime = SyncTime;
				}
			}
		}

		/**
		 * Apply the persistent data from the settings file to the active data
		 */
		private void ApplyOptions()
		{
			// Work out if any branches are displayed
			bool bBranchDisplayed = false;
			foreach( BranchSpec BranchInfo in Options.Branches )
			{
				bBranchDisplayed |= BranchInfo.bDisplayInMenu;
			}

			// Apply stored data to branch config
			foreach( BranchSpec BranchInfo in Options.Branches )
			{
				foreach( BranchSpec Branch in BranchSpecs )
				{
					if( Branch.Server == BranchInfo.Server && Branch.ClientSpec == BranchInfo.ClientSpec && Branch.Name == BranchInfo.Name )
					{
						Branch.bDisplayInMenu = BranchInfo.bDisplayInMenu;
						Branch.SyncType = BranchInfo.SyncType;
						Branch.SyncTime = BranchInfo.SyncTime;

						// If no branches displayed in the menu, enable the main branch by default
						if( !bBranchDisplayed )
						{
							Branch.bDisplayInMenu |= Branch.bIsMain;
						}

						// Set the artist sync times from the settings file
						foreach( PromotableGame Game in BranchInfo.PromotableGames )
						{
							SetPromotableGameSyncTime( Branch, Game.GameName, Game.bDisplayInMenus, Game.SyncTime );
						}
					}
				}
			}
		}

		/** 
		 * Look for valid ArtistSyncRules.xml files in Perforce for all valid games found on disk
		 */
		private void CachePromotableGames()
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				// Clear out the old list of games
				Branch.PromotableGames.Clear();

				// Reinterrogate Perforce for the latest set
				List<string> ValidGames = SCC.FindFolders( Branch, "*Game/Build/ArtistSyncRules.xml" );
				foreach( string ArtistSyncRuleName in ValidGames )
				{
					// Extract the game name
					string[] Game = ArtistSyncRuleName.Split( "/".ToCharArray() );
					string GameName = Game[0].Substring( 0, Game[0].Length - 4 );

					// Force sync the ArtistSyncRules file
					SCC.SyncFile( Branch, ArtistSyncRuleName );

					// If the above file does not exist, it has been masked out in the clientspec
					string RulesFileName = Path.Combine( Branch.Root, Branch.Name, GameName + "Game/Build/ArtistSyncRules.xml" );
					if( File.Exists( RulesFileName ) )
					{
						ArtistSyncRules Rules = UnrealControls.XmlHandler.ReadXml<ArtistSyncRules>( RulesFileName );
						if( Rules.PromotionLabel.Length > 0 )
						{
							PromotableGame NewGame = new PromotableGame( GameName, Rules.PromotionLabel );
							Branch.PromotableGames.Add( NewGame );
						}
					}
				}
			}
		}

		/** 
		 * Check for each game in all branches being synced to the correct artist sync
		 */
		private void CheckForUpToDatedness()
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				foreach( PromotableGame Game in Branch.PromotableGames )
				{
					Game.bUpToDate = SCC.GetFileUpToDate( Branch, Game );
				}
			}
		}

		/** 
		 * Set the flag to display the new build available icon
		 */
		private void UpdateOutOfDatedness()
		{
			bAllGamesAreUpToDate = true;

			if( Options.SyncUserType == EUserType.ContentCreator )
			{
				foreach( BranchSpec Branch in BranchSpecs )
				{
					foreach( PromotableGame Game in Branch.PromotableGames )
					{
						if( Game.bDisplayInMenus && !Game.bUpToDate )
						{
							bAllGamesAreUpToDate = false;
						}
					}
				}
			}
		}

		/// <summary>
		/// Populate the branches with extra info from the database via the web service
		/// </summary>
		public void PopulateBranchSpecs()
		{
			// Clear the cache as we want to refresh everything on startup
			WebService.ClearCache();

			// Populate every branch
			foreach( BranchSpec Branch in BranchSpecs )
			{
				WebService.PopulateBranch( Options.WebServiceURL, Branch );
			}
		}

		/// <summary>
		/// Main init function
		/// </summary>
		public bool Init( string[] Args )
		{
			Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( GetOptionsFileName() );

			// Init source control
			SCC = new P4( this );

			// Get a list of servers we build in from the database
			PerforceServers = WebService.GetPerforceServers( Options.WebServiceURL );

			// Remove duplicates and locate tickets for the servers
			List<PerforceInfo> PerforceServerInfos = SCC.ResolvePerforceServers( PerforceServers );

			// Get a list of available clientspecs from Perforce
			BranchSpecs = SCC.GetClientSpecs( PerforceServerInfos );

			// Grab all the branch information
			PopulateBranchSpecs();

			// Cache games with valid ArtistSyncRules.xml files for all the populated branches
			CachePromotableGames();

			// Grab all the branch information
			PopulateBranchSpecs();

			// Apply any remembered settings to the list of branches
			ApplyOptions();

			// Check for each game being correctly artist synced
			CheckForUpToDatedness();
			UpdateOutOfDatedness();

			// Init the forms
			InitializeComponent();

			// Create the context menu dynamically
			DefaultContextMenuItemCount = UnrealSyncContextMenu.Items.Count;

			// Add the available clientspecs and the available commands depending on the user's sync type
			CreateContextMenu();

			// Set the icon depending on whether theres a new build available
			if( !bAllGamesAreUpToDate )
			{
				UnrealSync2NotifyIcon.Icon = NewBuildAnimationIcons[0];
			}
			else
			{
				UnrealSync2NotifyIcon.Icon = BusyAnimationIcons[0];
			}

			// Set the configuration grid
			UnrealSyncConfigurationGrid.SelectedObject = Options;

			System.Version Version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
			DateTime CompileDateTime = DateTime.Parse( "01/01/2000" ).AddDays( Version.Build ).AddSeconds( Version.Revision * 2 );
			VersionText.Text = "Compiled: " + CompileDateTime.ToString( "d MMM yyyy HH:mm" );

			bTicking = true;

			// Start the thread that monitors sync to handle
			Thread ManageSyncThread = new Thread( ManageSyncs );
			ManageSyncThread.Start();

			// Start the thread that animates the cursor
			Thread BusyThread = new Thread( BusyThreadProc );
			BusyThread.Start();

			SyncingEvent.Set();
			ErrorEvent.Set();

			if( SCC.bReportInvalidClientSpec && BranchSpecs.Count == 0 )
			{
				DisplayBadClientSpec();
			}

			return ( true );
		}

		public void Destroy()
		{
			Dispose();

			SetOptions();
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );
		}

		public void Run()
		{
			/** Check to see if any syncs need spawning */
			CheckSyncSchedule();

			/** Check to see if any builds were promoted */
			CheckBuilds();

			/** Check to see if the build has been acknowledged, and redisplay the balloon if not */
			CheckBuildAckknowledged();

			/** Clean up logs older the a month */
			CleanLogs();

			/** Check to see if the p4tickets file has changed, and restart if so */
			SCC.CheckTickets();
		}

		/** 
		 * Check to see if any syncs need spawning 
		 */
		private void CheckSyncSchedule()
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				// Check for scheduled main branch syncs
				TimeSpan TimeCheck = DateTime.Now - Branch.SyncRandomisation - Branch.SyncTime;
				if( TimeCheck.TotalMinutes > 0 )
				{
					// Suppress the sync if it was missed by 10 minutes (to avoid a sync after leaving a machine off for a couple of days)
					if( TimeCheck.TotalMinutes < 10 )
					{
						BranchSpec BranchInfo = new BranchSpec( Branch );
						BranchInfo.bShowBalloon = false;
						PendingSyncs.Enqueue( BranchInfo );
					}

					while( DateTime.Now > Branch.SyncTime )
					{
						Branch.SyncTime = Branch.SyncTime.AddDays( 1.0 );
					}

					Branch.SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
				}

				// Check for scheduled artist syncs
				foreach( PromotableGame Game in Branch.PromotableGames )
				{
					TimeSpan GameCheck = DateTime.Now - Game.SyncRandomisation - Game.SyncTime;
					if( GameCheck.TotalMinutes > 0 )
					{
						// Suppress the sync if it was missed by more than 10 minutes (to avoid a sync after leaving a machine off for a couple of days)
						if( GameCheck.TotalMinutes < 10 )
						{
							BranchSpec BranchInfo = new BranchSpec( Branch );
							BranchInfo.GameName = Game.GameName;
							BranchInfo.SyncType = ESyncType.ArtistSyncGame;
							BranchInfo.bShowBalloon = false;
							PendingSyncs.Enqueue( BranchInfo );
						}

						while( DateTime.Now > Game.SyncTime )
						{
							Game.SyncTime = Game.SyncTime.AddDays( 1.0 );
						}

						Game.SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
					}
				}
			}
		}

		/** 
		 * Redisplay the balloon if the user hasn't acknowledged the build
		 */
		private void CheckBuildAckknowledged()
		{
			if( !bNewBuildAcknowledged && Options.SyncUserType == EUserType.ContentCreator && Options.RequireAcknowledgement )
			{
				if( LastBuildAckPollTime < DateTime.UtcNow )
				{
					ShowBalloon();

#if DEBUG
					LastBuildAckPollTime = DateTime.UtcNow.AddMinutes( 1 );
#else
					LastBuildAckPollTime = DateTime.UtcNow.AddMinutes( 5 );
#endif
				}
			}
		}

		/** 
		 * Pop up the notify balloon when a sync completes
		 */
		private void DisplaySyncComplete()
		{
			CachedBalloonTitle = SCC.SummaryTitle;
			CachedBalloonText = SCC.SummaryText;
			CachedBalloonIcon = SCC.SummaryIcon;

			ShowBalloon();
		}

		/** 
		 * Pop up the notify balloon if there is a new build
		 */
		private void DisplayNewBuild( string Title, string Content, string Label, bool bPlaySound )
		{
			CachedBalloonTitle = Title;
			CachedBalloonText = Content + Environment.NewLine + Label;
			CachedBalloonIcon = ToolTipIcon.Info;

			ShowBalloon();

			if( bPlaySound )
			{
				System.Media.SystemSounds.Exclamation.Play();
			}

			bNewBuildAcknowledged = false;
		}

		/** 
		 * Pop up the notify balloon for no branches found with a completely generic clientspec
		 */
		private void DisplayBadClientSpec()
		{
			CachedBalloonTitle = "No Branches Found!";
			CachedBalloonText = "Please map specific branches into your Perforce view." + Environment.NewLine
								+ "e.g. //depot/UE4/... //CLIENT/UE4/..." + Environment.NewLine + Environment.NewLine
								+ "EngineQA will be happy to assist!";
			CachedBalloonIcon = ToolTipIcon.Warning;

			ShowBalloon();

			System.Media.SystemSounds.Asterisk.Play();
		}

		/** 
		 * Check to see if any builds were promoted 
		 */
		private void CheckBuilds()
		{
			if( LastDataBasePollTime < DateTime.UtcNow )
			{
				List<string> PromotedBranchNames = new List<string>();
				List<string> PromotedBranchGameNames = new List<string>();
				List<string> PoppedBranchNames = new List<string>();

				foreach( BranchSpec Branch in BranchSpecs )
				{
					WebService.PopulateBranch( Options.WebServiceURL, Branch );

					// Check for new QA build
					if( Branch.bNewQABuild )
					{
						if( !PromotedBranchNames.Contains( Branch.Name ) )
						{
							if( Options.ShowPromotions )
							{
								DisplayNewBuild( "New QA build!", Branch.Name + " was promoted to build label", Branch.LatestQABuildLabel, Options.PlaySoundOnPromotion );
							}

							PromotedBranchNames.Add( Branch.Name );
						}

						Branch.bNewQABuild = false;
					}

					// Check for newly promoted builds
					foreach( PromotableGame Game in Branch.PromotableGames )
					{
						if( Game.bNewPromotion )
						{
							if( !PromotedBranchGameNames.Contains( Branch.Name + Game.GameName ) )
							{
								if( Options.ShowPromotions )
								{
									DisplayNewBuild( "New build promoted!", Game.GameName + " was promoted to build label", Game.PromotedLabel, Options.PlaySoundOnPromotion );
								}

								if( !PromotedBranchNames.Contains( Branch.Name ) )
								{
									// Issue a command to refresh the out of date state
									PendingSyncs.Enqueue( null );

									PromotedBranchNames.Add( Branch.Name );
								}

								PromotedBranchGameNames.Add( Branch.Name + Game.GameName );
							}

							Game.bNewPromotion = false;
						}
					}

					// Check for newly popped builds
					if( Branch.bNewBuild )
					{
						if( !PoppedBranchNames.Contains( Branch.Name ) )
						{
							if( Options.ShowBuildPops )
							{
								DisplayNewBuild( "New build!", Branch.Name + " has a new build", Branch.LatestBuildLabel, Options.PlaySoundOnBuildPop );
							}

							PoppedBranchNames.Add( Branch.Name );
						}

						Branch.bNewBuild = false;
					}
				}

				LastDataBasePollTime = DateTime.UtcNow.AddSeconds( Options.PollingInterval );
			}
		}

		/** 
		 * Clean up logs older the a month 
		 */
		private void CleanLogs()
		{
			// Check every 7 hours
			if( LastCleanLogTime < DateTime.UtcNow )
			{
				string BaseDirectory = Application.StartupPath;
				if( ApplicationDeployment.IsNetworkDeployed )
				{
					BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
				}

				DirectoryInfo DirInfo = new DirectoryInfo( BaseDirectory );
				foreach( FileInfo Info in DirInfo.GetFiles( "[*.txt" ) )
				{
					if( ( DateTime.UtcNow - Info.LastWriteTimeUtc ).TotalDays > 60 )
					{
						Info.IsReadOnly = false;
						Info.Delete();
					}
				}

				LastCleanLogTime = DateTime.UtcNow.AddHours( 7 );
			}
		}

		/** 
		 * Thread to handle animating the tray icon 
		 */
		private void BusyThreadProc( object Data )
		{
			while( bTicking )
			{
				int AnimationIndex = 0;
				while( bTicking && !SyncingEvent.WaitOne( 66 ) )
				{
					Icon[] Animation = BusyAnimationIcons;
					AnimationIndex++;
					if( AnimationIndex >= Animation.Length )
					{
						AnimationIndex = 0;
					}

					UnrealSync2NotifyIcon.Icon = Animation[AnimationIndex];
				}

				while( bTicking && !ErrorEvent.WaitOne( 132 ) )
				{
					Icon[] Animation = ErrorAnimationIcons;
					AnimationIndex++;
					if( AnimationIndex >= Animation.Length )
					{
						AnimationIndex = 0;
					}

					UnrealSync2NotifyIcon.Icon = Animation[AnimationIndex];
				}

				if( bTicking && bAllGamesAreUpToDate )
				{
					UnrealSync2NotifyIcon.Icon = BusyAnimationIcons[0];
				}

				if( bTicking && !bAllGamesAreUpToDate )
				{
					UnrealSync2NotifyIcon.Icon = NewBuildAnimationIcons[0];
				}

				Thread.Sleep( 100 );
			}
		}

		/** 
		 * Start the process of quitting the application 
		 */
		private void ExitMenuItemClick( object sender, EventArgs e )
		{
			bTicking = false;
		}

		/** 
		 * Update the most recently used list
		 */
		private void UpdateMRU( BranchSpec BranchInfo )
		{
			bool bMRUItemExists = false;
			foreach( BranchSpec ExistingBranchInfo in Options.MRUSyncs )
			{
				if( ExistingBranchInfo.MRUString() == BranchInfo.MRUString() )
				{
					bMRUItemExists = true;
				}
			}

			if( !bMRUItemExists )
			{
				Options.MRUSyncs.Add( BranchInfo );
			}
		}

		/** 
		 * Handle the clicking of an item on the popup menu
		 */
		private void MenuItemClick( object sender, EventArgs e )
		{
			ToolStripMenuItem MenuItem = ( ToolStripMenuItem )sender;
			BranchSpec Branch = ( BranchSpec )MenuItem.Tag;

			switch( MenuItem.Name )
			{
			case "Explore":
				string Folder = Path.Combine( Branch.Root, Branch.Name );
				if( Directory.Exists( Folder ) )
				{
					Process.Start( Folder );
				}
				break;

			default:
				ESyncType SyncType = ( ESyncType )Enum.Parse( typeof( ESyncType ), MenuItem.Name, false );

				// Set up the game to artist sync
				if( SyncType == ESyncType.ArtistSyncGame || SyncType == ESyncType.LaunchEditor )
				{
					string[] Params = MenuItem.Text.Split( " ".ToCharArray() );
					if( Params.Length > 0 )
					{
						Branch.GameName = Params[0];
					}
				}

				// Enqueue the job
				BranchSpec BranchInfo = new BranchSpec( Branch );
				BranchInfo.bShowBalloon = true;
				BranchInfo.SyncType = SyncType;
				PendingSyncs.Enqueue( BranchInfo );

				// Add this sync to the MRU if it isn't already there
				UpdateMRU( BranchInfo );

				// Update the context menu
				CreateContextMenu();
				break;
			}
		}

		/** 
		 * Handle clicking of one of the most recently used clicks
		 */
		private void MRUMenuItemClick( object sender, EventArgs e )
		{
			ToolStripMenuItem MenuItem = ( ToolStripMenuItem )sender;
			BranchSpec BranchInfo = ( BranchSpec )MenuItem.Tag;

			// Enqueue the job
			BranchInfo.bShowBalloon = true;
			PendingSyncs.Enqueue( BranchInfo );
		}

		/** 
		 * Bring up the branch configuration dialog 
		 */
		private void ContextMenuConfigureBranchesClicked( object sender, EventArgs e )
		{
			if( ConfigureBranches == null && ConfigureArtistSync == null )
			{
				if( Options.SyncUserType != EUserType.ContentCreator )
				{
					ConfigureBranches = new BranchConfigure( this );
					ConfigureBranches.Show();
				}
				else
				{
					ConfigureArtistSync = new ArtistSyncConfigure( this );
					ConfigureArtistSync.Show();
				}
			}
			else
			{
				if( ConfigureBranches != null )
				{
					ConfigureBranches.BranchConfigureOKButtonClicked( null, null );
					ConfigureBranches = null;
				}
				else if( ConfigureArtistSync != null )
				{
					ConfigureArtistSync.ArtistSyncConfigureOKButtonClicked( null, null );
					ConfigureArtistSync = null;
				}
			}
		}

		/** 
		 * Handle the preferences dialog 
		 */
		private void ContextMenuPreferencesClicked( object sender, EventArgs e )
		{
			Show();
		}

		/** 
		 * Launch P4Client 
		 */
		private void ContextMenuP4WinClicked( object sender, EventArgs e )
		{
			try
			{
				Process.Start( Options.PerforceGUIClient );
			}
			catch
			{
				MessageBox.Show( "Unable to launch Perforce client '" + Options.PerforceGUIClient + "'!", "Critical Error!", MessageBoxButtons.OK, MessageBoxIcon.Error );
			}
		}

		/** 
		 * Acknowledge the new build
		 */
		private void BalloonTipClicked( object sender, EventArgs e )
		{
			bNewBuildAcknowledged = true;
		}

		/** 
		 * Redisplay the last displayed sync info 
		 */
		private void TrayIconClicked( object sender, EventArgs e )
		{
			MouseEventArgs Event = ( MouseEventArgs )e;
			if( Event.Button == MouseButtons.Left )
			{
				ShowBalloon();
			}
		}

		public void ButtonOKClick( object sender, EventArgs e )
		{
			Hide();

			SetOptions();
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );

			CreateContextMenu();
			UpdateOutOfDatedness();
		}

		/** 
		 * Bring up the dialog that shows all the logs 
		 */
		private void ContextMenuShowLogsClicked( object sender, EventArgs e )
		{
			if( ViewLogs == null )
			{
				ViewLogs = new LogViewer( this );
				ViewLogs.Show();
			}
			else
			{
				ViewLogs.Close();
				ViewLogs = null;
			}

			ErrorEvent.Set();
		}

		public List<DataGridViewCellStyle> GetGridStyles( int Count )
		{
			// Create a cell style with a unique background per clientspec
			List<Color> Colours = new List<Color>() 
			{ 
				Color.Honeydew,
				Color.LavenderBlush, 
				Color.LightCyan,
				Color.LightYellow,
			};

			List<DataGridViewCellStyle> CellStyles = new List<DataGridViewCellStyle>();
			for( int Index = 0; Index < Count; Index++ )
			{
				DataGridViewCellStyle CellStyle = new DataGridViewCellStyle();
				CellStyle.BackColor = Colours[Index % Colours.Count];
				CellStyles.Add( CellStyle );
			}

			return ( CellStyles );
		}

		public string GetSyncTimeString( DateTime SyncTime )
		{
			string SyncTimeString = "Never";

			if( SyncTime != DateTime.MaxValue )
			{
				// Never sync before 3am or after noon
				if( SyncTime.Hour < 3 || SyncTime.Hour > 12 )
				{
					SyncTime = DateTime.MaxValue;
				}
				else
				{
					SyncTimeString = SyncTime.ToString( "h:mm tt" );
				}
			}

			return SyncTimeString;
		}

		public DateTime GetSyncTime( string Time )
		{
			DateTime SyncTime = DateTime.MaxValue;
			if( Time != "Never" )
			{
				if( DateTime.TryParse( Time, out SyncTime ) )
				{
					if( SyncTime < DateTime.Now )
					{
						SyncTime = SyncTime.AddDays( 1.0 );
					}
				}
			}

			return SyncTime;
		}
	}

	/** 
	 * A class containing the rules to complete an artist sync
	 */
	public class ArtistSyncRules
	{
		[XmlElement]
		public string PromotionLabel;
		[XmlArray]
		public List<string> FilesToClean;
		[XmlArray]
		public List<string> Rules;

		public ArtistSyncRules()
		{
			PromotionLabel = "";
			FilesToClean = new List<string>();
			Rules = new List<string>();
		}
	}

	/** 
	 * A class containing the settable options for a user
	 */
	public class SettableOptions
	{
		[XmlElement]
		public string DefaultPerforcePort = "p4-server:1666";

		[XmlElement]
		public List<BranchSpec> Branches = new List<BranchSpec>();

		[XmlElement]
		public List<BranchSpec> MRUSyncs = new List<BranchSpec>();

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "Automatically generate the project files for UE4 based branches." )]
		[XmlElement]
		public bool AutoGenerateProjectFiles { get; set; }

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "Set the type of syncs for your desired workflow." )]
		[XmlElement]
		public EUserType SyncUserType { get; set; }

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "Set whether to auto clobber (overwrite) files that are locally writable, but not checked out." )]
		[XmlElement]
		public bool AutoClobber { get; set; }

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "Whether to launch the editor after a manually triggered artist sync." )]
		[XmlElement]
		public bool LaunchEditor { get; set; }

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "Set the type of resolve to use." )]
		[XmlElement]
		public EResolveType ResolveType { get; set; }

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "An executable file to run before each sync operation. It is set to run in the root folder of the branch (e.g. \"d:\\depot\\UE4\")" )]
		[XmlElement]
		public string PreSyncExecutable { get; set; }

		[CategoryAttribute( "Syncing" )]
		[DescriptionAttribute( "The parameters to be passed to the above executable when it is run." )]
		[XmlElement]
		public string PreSyncExecutableParameters { get; set; }

		[CategoryAttribute( "Build Promotion" )]
		[DescriptionAttribute( "Shows the tip balloon when a new build is promoted." )]
		[XmlElement]
		public bool ShowPromotions { get; set; }

		[CategoryAttribute( "Build Promotion" )]
		[DescriptionAttribute( "Play a sound when a new build is promoted." )]
		[XmlElement]
		public bool PlaySoundOnPromotion { get; set; }

		[CategoryAttribute( "Build Popping" )]
		[DescriptionAttribute( "Shows the tip balloon when a new build is created." )]
		[XmlElement]
		public bool ShowBuildPops { get; set; }

		[CategoryAttribute( "Build Popping" )]
		[DescriptionAttribute( "Play a sound when a new build is created." )]
		[XmlElement]
		public bool PlaySoundOnBuildPop { get; set; }

		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "The Perforce GUI client (normally P4v.exe or P4win.exe)" )]
		[XmlElement]
		public string PerforceGUIClient { get; set; }

		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "Whether to repeatedly show the tip balloon until the balloon is clicked." )]
		[XmlElement]
		public bool RequireAcknowledgement { get; set; }

		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "Duration in seconds of the balloon showing a new build." )]
		[XmlElement]
		public float BalloonDuration
		{
			get
			{
				return LocalBalloonDuration;
			}
			set
			{
				if( value < 0.0f )
				{
					value = 0.0f;
				}
				else if( value > 30.0f )
				{
					value = 30.0f;
				}
				LocalBalloonDuration = value;
			}
		}
		private float LocalBalloonDuration = 10.0f;

		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "Polling interval in seconds (default is 120, with clamps at 60 and 300)." )]
		[XmlElement]
		public int PollingInterval
		{
			get
			{
				return ( LocalPollingInterval );
			}
			set
			{
				if( value < 60 )
				{
					value = 60;
				}
				else if( value > 300 )
				{
					value = 300;
				}
				LocalPollingInterval = value;
			}
		}
		private int LocalPollingInterval = 120;

		[CategoryAttribute( "Network" )]
		[DescriptionAttribute( "The domain to be used by default for the Perforce servers." )]
		[XmlElement]
		public string DefaultDomain { get; set; }

		[CategoryAttribute( "Network" )]
		[DescriptionAttribute( "URL of the web service." )]
		[XmlElement]
		public string WebServiceURL { get; set; }

		public SettableOptions()
		{
			AutoGenerateProjectFiles = false;
			AutoClobber = false;
			LaunchEditor = false;
			ResolveType = EResolveType.Automatic;
			PreSyncExecutable = "";
			PreSyncExecutableParameters = "";
			ShowPromotions = true;
			PlaySoundOnPromotion = true;
			ShowBuildPops = true;
			PlaySoundOnBuildPop = true;
			RequireAcknowledgement = true;
			PerforceGUIClient = "P4v.exe";
			WebServiceURL = "devweb-02.epicgames.net";
			DefaultDomain = "epicgames.net";
		}
	}

	/** 
	 * A class containing the information about a promotable game in a branch
	 */
	public class PromotableGame
	{
		[XmlAttribute]
		public string GameName;
		[XmlAttribute]
		public bool bDisplayInMenus;
		[XmlAttribute]
		public DateTime SyncTime;
		[XmlIgnore]
		public TimeSpan SyncRandomisation;
		[XmlIgnore]
		public string PromotedLabel;
		[XmlIgnore]
		public string PromotionLabel;
		[XmlIgnore]
		public bool bUpToDate;
		[XmlIgnore]
		public bool bNewPromotion;

		public PromotableGame()
		{
			GameName = "";
			bDisplayInMenus = true;
			SyncTime = DateTime.MaxValue;
			SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
			PromotedLabel = "";
			PromotionLabel = "";
			bUpToDate = false;
			bNewPromotion = false;
		}

		public PromotableGame( string InGameName, string InPromotionLabel )
		{
			GameName = InGameName;
			bDisplayInMenus = true;
			SyncTime = DateTime.MaxValue;
			SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
			PromotionLabel = InPromotionLabel;
			bUpToDate = false;
			bNewPromotion = false;
		}

		public PromotableGame( PromotableGame Game )
		{
			GameName = Game.GameName;
			bDisplayInMenus = Game.bDisplayInMenus;
			SyncTime = Game.SyncTime;
			SyncRandomisation = Game.SyncRandomisation;
			PromotionLabel = Game.PromotionLabel;
			bUpToDate = Game.bUpToDate;
			bNewPromotion = Game.bNewPromotion;
		}
	}
}
