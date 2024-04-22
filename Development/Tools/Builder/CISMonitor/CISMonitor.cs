// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Reflection;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;

namespace CISMonitor
{
	public partial class CISMonitor : Form
	{
		public enum SubmitState
		{
			CISUnknown,
			CISBadFailed,
			CISBadPending,
			CISBadSuccessful,
			CISGoodFailed,
			CISGoodPending,
			CISGoodSuccessful
		};

		public bool Ticking = false;
		public bool Restart = false;
		public SettableOptions Options = null;

		private string User = "";
		private bool bFirstTime = true;
		private System.Drawing.Icon GoodGoodIcon = null;
		private System.Drawing.Icon GoodPendingIcon = null;
		private System.Drawing.Icon GoodBadIcon = null;
		private System.Drawing.Icon BadGoodIcon = null;
		private System.Drawing.Icon BadPendingIcon = null;
		private System.Drawing.Icon BadBadIcon = null;
		private DateTime LastCheckTime = DateTime.MinValue;
		private bool OverallBuildsAreGood = false;
		private SubmitState OverallState = SubmitState.CISUnknown;
		private bool BalloonTickActive = false;
		private bool InstantUpdate = true;

		public CISMonitor()
		{
		}

		private string GetOptionsFileName()
		{
			string BaseDirectory = Application.StartupPath;
			if( ApplicationDeployment.IsNetworkDeployed )
			{
				BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
			}

			return ( Path.Combine( BaseDirectory, "CISMonitor.Options.xml" ) );
		}

		public void Init( string[] args )
		{
			Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( GetOptionsFileName() );

			// Init and precache the UI elements
			InitializeComponent();
			ConfigurationGrid.SelectedObject = Options;

			System.Version Version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
			DateTime CompileDateTime = DateTime.Parse( "01/01/2000" ).AddDays( Version.Build ).AddSeconds( Version.Revision * 2 );
			VersionText.Text = "Compiled: " + CompileDateTime.ToString( "d MMM yyyy HH:mm" ) + " (.NET)";

			User = Environment.UserName;

			GoodGoodIcon = Resources.green_green;
			GoodPendingIcon = Resources.green_yellow;
			GoodBadIcon = Resources.green_red;
			BadGoodIcon = Resources.red_green;
			BadPendingIcon = Resources.red_yellow;
			BadBadIcon = Resources.red_red;

			Ticking = true;

			// Work out the potential branches we are interested in
			PopulateBranchConfig();
		}

		public void Destroy()
		{
			Dispose();

			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );
		}

		private string[] GetBranches()
		{
			string[] Branches = new string[1] { "UnrealEngine3" };
			try
			{
				// Send off the request - e.g. http://devweb-02:2827/CISMonitor/Branches
				string RequestString = "http://" + Properties.Settings.Default.CISMonitorServiceHost + ":2827/CISMonitor/GetBranches";
				HttpWebRequest Request = ( HttpWebRequest )WebRequest.Create( RequestString );
				HttpWebResponse Response = ( HttpWebResponse )Request.GetResponse();

				if( Response.ContentLength < 2048 )
				{
					// Process the response
					Stream ResponseStream = Response.GetResponseStream();
					byte[] RawResponse = new byte[Response.ContentLength];
					ResponseStream.Read( RawResponse, 0, ( int )Response.ContentLength );
					ResponseStream.Close();

					// Convert response into a list of branches
					string ResponseString = Encoding.UTF8.GetString( RawResponse );
					Branches = ResponseString.Split( "/".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
				}
			}
			catch
			{
			}

			return Branches;
		}

		private string GetUserState( string Branch, string User )
		{
			string Result = "Unknown/Unknown";
			try
			{
				// Send off the request - e.g. http://devweb-02:2827/CISMonitor/Branches
				string RequestString = "http://" + Properties.Settings.Default.CISMonitorServiceHost + ":2827/CISMonitor/GetUserState/" + Branch + "/" + User;
				HttpWebRequest Request = ( HttpWebRequest )WebRequest.Create( RequestString );
				HttpWebResponse Response = ( HttpWebResponse )Request.GetResponse();

				// Process the response
				Stream ResponseStream = Response.GetResponseStream();
				byte[] RawResponse = new byte[Response.ContentLength];
				ResponseStream.Read( RawResponse, 0, ( int )Response.ContentLength );
				ResponseStream.Close();

				// Convert response into the result
				Result = Encoding.ASCII.GetString( RawResponse );
			}
			catch
			{
			}

			return Result;
		}

		public void Run()
		{
			// Poll periodically
#if DEBUG
			TimeSpan Interval = new TimeSpan( 0, 0, 10 );
#else
			int Randomiser = new Random().Next( 45 );
			TimeSpan Interval = new TimeSpan( 0, 0, Options.PollingInterval + Randomiser );
#endif
			if( InstantUpdate )
			{
				// Limit the max update to 1 per second even when requesting instant updates
				if( ( DateTime.UtcNow - LastCheckTime ).TotalSeconds < 1.0 )
				{
					return;
				}
			}
			else if( DateTime.UtcNow < LastCheckTime + Interval )
			{
				return;
			}

			InstantUpdate = false;
			LastCheckTime = DateTime.UtcNow;

			// Update the status
			try
			{
				foreach( BranchConfig Branch in Options.BranchesToMonitor )
				{
					// Only interested in branches we need to monitor
					if( Branch.Monitor )
					{
						string CISState = GetUserState( Branch.Name, User );
						switch( CISState.ToLower() )
						{
						case "good/good":
							Branch.CurrentState = SubmitState.CISGoodSuccessful;
							break;

						case "good/unknown":
							Branch.CurrentState = SubmitState.CISGoodPending;
							break;

						case "good/bad":
							Branch.CurrentState = SubmitState.CISGoodFailed;
							break;

						case "bad/good":
							Branch.CurrentState = SubmitState.CISBadSuccessful;
							break;

						case "bad/unknown":
							Branch.CurrentState = SubmitState.CISBadPending;
							break;

						case "bad/bad":
							Branch.CurrentState = SubmitState.CISBadFailed;
							break;

						default:
						case "unknown/good":
						case "unknown/unknown":
						case "unknown/bad":
							Branch.CurrentState = SubmitState.CISUnknown;
							break;
						}
					}
				}

				// Find the overall state of the build
				SubmitState OldOverallState = OverallState;
				OverallState = SubmitState.CISGoodSuccessful;

				foreach( BranchConfig Branch in Options.BranchesToMonitor )
				{
					if( Branch.Monitor )
					{
						if( Branch.CurrentState < OverallState )
						{
							OverallState = Branch.CurrentState;
						}
					}
				}

				OverallBuildsAreGood = ( OverallState != SubmitState.CISBadSuccessful )
										&& ( OverallState != SubmitState.CISBadPending )
										&& ( OverallState != SubmitState.CISBadFailed );

				// Update the UI
				if( OverallState != OldOverallState || bFirstTime )
				{
					UpdateTrayIcon( false );
					bFirstTime = false;
				}
			}
			catch( Exception Ex )
			{
				Debug.WriteLineIf( Debugger.IsAttached, Ex.Message );
			}
		}

		public void PopulateBranchConfig()
		{
#if false
			Options.BranchesToMonitor = new List<BranchConfig>();
			Options.BranchesToMonitor.Add( new BranchConfig( "UnrealEngine3", true ) );
#else
			bool NewSettings = false;
			if( Options.BranchesToMonitor == null )
			{
				Options.BranchesToMonitor = new List<BranchConfig>();
				NewSettings = true;
			}

			// Get the branches that we could potentially be monitoring
			string[] Branches = GetBranches();

			// If the branch doesn't exist, add it with default values
			foreach( string Branch in Branches )
			{
				bool AlreadyMonitoring = false;
				foreach( BranchConfig BranchInfo in Options.BranchesToMonitor )
				{
					if( Branch.ToLower() == BranchInfo.Name.ToLower() )
					{
						AlreadyMonitoring = true;
						BranchInfo.HasCIS = true;
						break;
					}
				}

				if( !AlreadyMonitoring )
				{
					// Default monitoring the main branch to on, and other branches to off
					bool DefaultMonitor = false;
					if( NewSettings && Branch == "UnrealEngine3" )
					{
						DefaultMonitor = true;
					}

					Options.BranchesToMonitor.Add( new BranchConfig( Branch, DefaultMonitor ) );
				}
			}

			// Clear out any stale branches
			bool BranchesToDelete = true;
			while( BranchesToDelete )
			{
				BranchesToDelete = false;
				foreach( BranchConfig BranchInfo in Options.BranchesToMonitor )
				{
					if( !BranchInfo.HasCIS )
					{
						Options.BranchesToMonitor.Remove( BranchInfo );
						BranchesToDelete = true;
						break;
					}
				}
			}

			// Associate branches with tool strip menu items
			Queue<ToolStripMenuItem> MenuItemBranches = new Queue<ToolStripMenuItem>();
			MenuItemBranches.Enqueue( ToolStripMenuItemBuildStatus1 );
			MenuItemBranches.Enqueue( ToolStripMenuItemBuildStatus2 );
			MenuItemBranches.Enqueue( ToolStripMenuItemBuildStatus3 );
			MenuItemBranches.Enqueue( ToolStripMenuItemBuildStatus4 );
			MenuItemBranches.Enqueue( ToolStripMenuItemBuildStatus5 );

			foreach( BranchConfig Branch in Options.BranchesToMonitor )
			{
				if( Branch.Monitor )
				{
					ToolStripMenuItem MenuItem = MenuItemBranches.Dequeue();
					MenuItem.Visible = true;
					MenuItem.Text = "Build Status (" + Branch.Name + ")";
					MenuItem.Tag = Branch.Name;
				}
			}

			foreach( ToolStripMenuItem MenuItem in MenuItemBranches )
			{
				MenuItem.Visible = false;
			}
#endif
		}

		private void UpdateTrayIcon( bool Force )
		{
			bool ShowBalloon = true;

			switch( OverallState )
			{
			case SubmitState.CISBadSuccessful:
				BuildState.Icon = BadGoodIcon;
				BuildState.BalloonTipTitle = "CIS has detected the build is BAD!";
				BuildState.BalloonTipText = "You have no pending changelists";
				BuildState.BalloonTipIcon = ToolTipIcon.Info;
				ShowBalloon = Options.ShowSuccessBalloon;
				break;

			case SubmitState.CISGoodSuccessful:
				BuildState.Icon = GoodGoodIcon;
				BuildState.BalloonTipTitle = "CIS has verified the build is GOOD!";
				BuildState.BalloonTipText = "You have no pending changelists";
				BuildState.BalloonTipIcon = ToolTipIcon.Info;
				ShowBalloon = Options.ShowSuccessBalloon;
				break;

			case SubmitState.CISBadPending:
			case SubmitState.CISGoodPending:
				BuildState.Icon = OverallBuildsAreGood ? GoodPendingIcon : BadPendingIcon;
				BuildState.BalloonTipTitle = "CIS is processing your changelists...";
				BuildState.BalloonTipText = "You have changelists waiting to be verified.";
				BuildState.BalloonTipIcon = ToolTipIcon.Info;
				ShowBalloon = Options.ShowPendingBalloon;
				break;

			case SubmitState.CISBadFailed:
			case SubmitState.CISGoodFailed:
				BuildState.Icon = OverallBuildsAreGood ? GoodBadIcon : BadBadIcon;
				BuildState.BalloonTipTitle = "CIS is failing, and it could be due to your changelists";
				BuildState.BalloonTipText = "You have changelists that are part of a failing build.";
				BuildState.BalloonTipIcon = ToolTipIcon.Error;

				if( Options.PlaySound )
				{
					System.Media.SystemSounds.Exclamation.Play();
				}
				break;

			case SubmitState.CISUnknown:
				BuildState.Icon = GoodPendingIcon;
				BuildState.BalloonTipTitle = "Unable to contact CIS Monitor Service...";
				BuildState.BalloonTipText = "Please check network status.";
				break;

			}

			if( Force || ShowBalloon )
			{
				BuildState.ShowBalloonTip( 1000 );
			}
		}

		private void ShowChangelists( object sender, MouseEventArgs e )
		{
			if( !BalloonTickActive && e.Button == MouseButtons.Left )
			{
				UpdateTrayIcon( true );
			}
		}

		private void BalloonTipShown( object sender, EventArgs e )
		{
			BalloonTickActive = true;
		}

		private void BalloonTipClosed( object sender, EventArgs e )
		{
			BalloonTickActive = false;
		}

		private void BalloonTipClicked( object sender, EventArgs e )
		{
			BalloonTickActive = false;
			if( !OverallBuildsAreGood )
			{
				Process.Start( "http://Builder/CIS/Status/" );
			}
		}

		private void ExitMenuItemClicked( object sender, EventArgs e )
		{
			Ticking = false;
		}

		private void MenuItemStatusClick( object sender, EventArgs e )
		{
			UpdateTrayIcon( true );
		}

		private void MenuItemConfigureClick( object sender, EventArgs e )
		{
			Show();
		}

		private void ButtonOKClick( object sender, EventArgs e )
		{
			Hide();
			PopulateBranchConfig();

			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );
		}

		private void MenuItemBuildStatusClick( object sender, EventArgs e )
		{
			ToolStripMenuItem MenuItem = ( ToolStripMenuItem )sender;
			string BranchName = ( string )MenuItem.Tag;

			Process.Start( "http://Builder/CIS/Branch/" + BranchName );
		}

		private void BuildStateMouseOver( object sender, MouseEventArgs e )
		{
			InstantUpdate = true;
		}
	}

	public class BranchConfig
	{
		[XmlIgnore]
		public bool HasCIS = false;
		[XmlIgnore]
		public bool BuildIsGood = false;
		[XmlIgnore]
		public CISMonitor.SubmitState CurrentState = CISMonitor.SubmitState.CISUnknown;
		[XmlIgnore]
		public int NumPendingChangelists = 0;

		[CategoryAttribute( "Configuration" )]
		[ReadOnlyAttribute( true )]
		public string Name { get; set; }

		[CategoryAttribute( "Configuration" )]
		public bool Monitor { get; set; }

		public BranchConfig()
		{
			Name = "";
			Monitor = false;
		}

		public BranchConfig( string InName, bool InMonitor )
		{
			Name = InName;
			Monitor = InMonitor;
			HasCIS = true;
		}
	}

	public class SettableOptions
	{
		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "Shows the tip balloon when your changelists pass CIS." )]
		[XmlElement]
		public bool ShowSuccessBalloon { get; set; }

		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "Shows the tip balloon when unverified changelists are detected." )]
		[XmlElement]
		public bool ShowPendingBalloon { get; set; }

		[CategoryAttribute( "Preferences" )]
		[DescriptionAttribute( "Play a sound when the build starts failing." )]
		[XmlElement]
		public bool PlaySound { get; set; }

		[CategoryAttribute( "Branches" )]
		[DescriptionAttribute( "List of branches to monitor for CIS results." )]
		[XmlElement]
		public List<BranchConfig> BranchesToMonitor { get; set; }

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
				if( value < 30 )
				{
					value = 30;
				}
				else if( value > 300 )
				{
					value = 300;
				}
				LocalPollingInterval = value;
			}
		}
		[XmlAttribute]
		private int LocalPollingInterval = 60;
	}
}
