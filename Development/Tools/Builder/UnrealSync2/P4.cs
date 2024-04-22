// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Deployment.Application;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;
using System.Windows.Forms;
using P4API;

namespace Builder.UnrealSync
{
	// A container to help resolve P4 servers
	public class PerforceInfo
	{
		public string FriendlyName = "";
		public string ServerName = "";
		public string UserName = "";
		public string Ticket = "";
		public IPAddress IP = null;

		// Store off the basic info and get the IP
		public PerforceInfo( string InFriendlyName, string InServerName, string InServerIP )
		{
			FriendlyName = InFriendlyName;
			ServerName = InServerName;

			string[] CleanServerIP = InServerIP.Split( ":".ToCharArray() );
			if( CleanServerIP.Length > 0 )
			{
				IP = IPAddress.Parse( CleanServerIP[0] );
			}
		}

		// Returns true if all the data is valid
		public bool IsValid()
		{
			if( UserName.Length == 0 )
			{
				return false;
			}

			if( Ticket.Length == 0 )
			{
				return false;
			}

			if( IP == null )
			{
				return false;
			}

			return true;
		}
	}

	public class P4
	{
		public string SummaryTitle = "";
		public string SummaryText = "";
		public ToolTipIcon SummaryIcon = ToolTipIcon.None;

		private int ErrorCount = 0;
		private int WarningCount = 0;
		private int MessageCount = 0;
		private int FilesSynced = 0;
		private TimeSpan TotalDuration = TimeSpan.MinValue;

		// The file timestamp of the p4 tickets file
		private DateTime TicketsTimeStamp = DateTime.MinValue;
		private DateTime LastTicketCheckTime = DateTime.MinValue;

		private UnrealSync2 Parent = null;
		private P4Connection IP4Net = null;
		private TextWriter Log = null;

		public bool bReportInvalidClientSpec = false;

		public P4( UnrealSync2 InParent )
		{
			Parent = InParent;
			IP4Net = new P4Connection();
		}

		// Sync a single file in a branch
		public void SyncFile( BranchSpec Branch, string FileName )
		{
			try
			{
				IP4Net.Port = Branch.Server;
				IP4Net.User = Branch.UserName;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
				string Parameters = "//depot/" + Branch.DepotName + "/" + FileName;
                // so here we do -f because we MUST have the .xml file which controls all of the syncing
				IP4Net.Run( "sync", "-f", Parameters );
			}
			catch
			{
			}

			IP4Net.Disconnect();
		}

		public bool GetFileUpToDate( BranchSpec Branch, PromotableGame Game )
		{
			bool bUpToDate = false;

			string BuildProperties = "Binaries/build.properties";
			if( Branch.Version >= 10 )
			{
				BuildProperties = "Engine/Build/build.properties";
			}

			try
			{
				string BranchFileName = Path.Combine( Branch.Root, Branch.Name, BuildProperties );

				IP4Net.Port = Branch.Server;
				IP4Net.User = Branch.UserName;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
				P4RecordSet Results = IP4Net.Run( "fstat", BranchFileName + "@" + Game.PromotionLabel );

				foreach( P4Record Record in Results )
				{
					if( Record["haveRev"] == Record["headRev"] )
					{
						bUpToDate = true;
					}
				}
			}
			catch
			{
			}

			IP4Net.Disconnect();

			return bUpToDate;
		}

		public List<string> FindFolders( BranchSpec Branch, string FileSpec )
		{
			List<string> Paths = new List<string>();

			P4RecordSet Results = null;
			try
			{
				IP4Net.Port = Branch.Server;
				IP4Net.User = Branch.UserName;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.ExceptionOnBothErrorsAndWarnings;
				string Parameters = "//depot/" + Branch.DepotName + "/" + FileSpec;
				Results = IP4Net.Run( "fstat", Parameters );

				foreach( P4Record Record in Results )
				{
					if( Record["headAction"].ToLower() != "delete" )
					{
						Paths.Add( Record["depotFile"].Substring( ( "//depot/" + Branch.DepotName + "/" ).Length ) );
					}
				}
			}
			catch
			{
			}

			IP4Net.Disconnect();

			return Paths;
		}

		private bool InterrogateServers( string PerforceServer, List<PerforceInfo> Servers )
		{
			bool bFoundServer = false;
			try
			{
				Debug.WriteLine( " ...... interrogating " + PerforceServer );
				IP4Net.Port = PerforceServer;
				IP4Net.ExceptionLevel = P4ExceptionLevels.ExceptionOnBothErrorsAndWarnings;
				P4RecordSet Results = IP4Net.Run( "info" );
				foreach( P4Record Record in Results.Records )
				{
					Debug.WriteLine( " ......... adding " + PerforceServer );
					Servers.Add( new PerforceInfo( PerforceServer, Record["serverAddress"], Record["serverLicense-ip"] ) );
					bFoundServer = true;
				}
			}
			catch
			{
				Debug.WriteLine( " ......... ignoring " + PerforceServer );
			}

			IP4Net.Disconnect();
			return bFoundServer;
		}

		// Add the real server names - some may be uncontactable
		private void FindPotentialServers( List<string> PerforceServers, List<PerforceInfo> Servers )
		{
			Debug.WriteLine( " ... interrogating " + PerforceServers.Count + " Perforce servers" );
			foreach( string PerforceServer in PerforceServers )
			{
				// Any blank server is just stubbed out
				if( PerforceServer.Length > 0 )
				{
					if( !InterrogateServers( PerforceServer, Servers ) )
					{
						string[] ServerComponents = PerforceServer.Split( ":".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
						if( ServerComponents.Length == 2 )
						{
							string NewPerforceServer = ServerComponents[0] + "." + Parent.Options.DefaultDomain + ":" + ServerComponents[1];
							InterrogateServers( NewPerforceServer, Servers );
						}
					}
				}
			}
		}

		// Check to see if the p4tickets file has been updated, and restart if so
		public void CheckTickets()
		{
			if( LastTicketCheckTime < DateTime.UtcNow )
			{
				try
				{
					string TicketPath = Path.Combine( Environment.GetEnvironmentVariable( "USERPROFILE" ), "p4tickets.txt" );
					if( TicketsTimeStamp < File.GetLastWriteTimeUtc( TicketPath ) )
					{
						Parent.bTicking = false;
						Parent.bRestart = true;
					}
				}
				catch
				{
				}
			
				LastTicketCheckTime = DateTime.UtcNow.AddSeconds( 20 );
			}
		}

		// Populate the Perforce infos with ticket info
		private void SetTicketInfo( List<PerforceInfo> Servers )
		{
			Debug.WriteLine( " ... setting ticket info for " + Servers.Count + " Perforce servers" );
			try
			{
				string TicketPath = Path.Combine( Environment.GetEnvironmentVariable( "USERPROFILE" ), "p4tickets.txt" );
				TicketsTimeStamp = File.GetLastWriteTimeUtc( TicketPath );

				StreamReader Reader = new StreamReader( TicketPath );
				while( !Reader.EndOfStream )
				{
					string Line = Reader.ReadLine();
					// 10.1.20.100:1666=john.scott:82AA62BFD985516D75E1B2518AFC1F25
					string[] Elements = Line.Split( ":=".ToCharArray() );
					if( Elements.Length == 4 )
					{
						IPAddress IP = IPAddress.Parse( Elements[0] );
						string UserName = Elements[2].ToLower();

						Debug.WriteLine( " ...... found ticket for " + Elements[2] + " with IP " + Elements[0] );
						if( UserName != "buildmachine" && !UserName.StartsWith( "ue3_" ) && !UserName.StartsWith( "ue4_" ) )
						{
							foreach( PerforceInfo Server in Servers )
							{
								if( Server.IP.Equals( IP ) )
								{
									Server.UserName = Elements[2];
									Server.Ticket = Elements[3];
									Debug.WriteLine( " ......... added ticket" );
								}
							}
						}
					}
				}
			}
			catch
			{
			}
		}

		// Cull out the servers we can't connect to directly, use the friendly name instead
		private void FindInvalidProxies( List<PerforceInfo> Servers )
		{
			Debug.WriteLine( " ... finding invalid proxies for " + Servers.Count + " Perforce servers" );
			foreach( PerforceInfo Server in Servers )
			{
				try
				{
					IP4Net.Port = Server.ServerName;
					IP4Net.ExceptionLevel = P4ExceptionLevels.ExceptionOnBothErrorsAndWarnings;
					P4RecordSet Results = IP4Net.Run( "info" );

					// Add the real server name if we can contact it
					Server.FriendlyName = Server.ServerName;
				}
				catch
				{
				}

				IP4Net.Disconnect();
			}
		}

		// Resolve the perforce servers to remove duplicates (proxies and regular)
		public List<PerforceInfo> ResolvePerforceServers( List<string> PerforceServers )
		{
			Debug.WriteLine( "Finding Perforce servers ..." );

			// Info to validate server info
			List<PerforceInfo> Servers = new List<PerforceInfo>();

			// Add the real server names - some may be uncontactable
			FindPotentialServers( PerforceServers, Servers );

			// Populate the server info with ticket information
			SetTicketInfo( Servers );

			// Cull out the servers we can't connect to directly, use the friendly name instead
			FindInvalidProxies( Servers );

			// Create a unique list of servers
			List<PerforceInfo> ResolvedServers = new List<PerforceInfo>();
			foreach( PerforceInfo PerforceServerInfo in Servers )
			{
				bool bAlreadyFound = false;
				foreach( PerforceInfo ResolvedPerforceServerInfo in ResolvedServers )
				{
					if( PerforceServerInfo.FriendlyName == ResolvedPerforceServerInfo.FriendlyName )
					{
						bAlreadyFound = true;
					}
				}

				if( !bAlreadyFound )
				{
					Debug.WriteLine( " ... checking server: " + PerforceServerInfo.FriendlyName );
					if( PerforceServerInfo.IsValid() )
					{
						Debug.WriteLine( " ...... adding: " + PerforceServerInfo.FriendlyName );
						ResolvedServers.Add( PerforceServerInfo );
					}
				}
			}

			Debug.WriteLine( " ... found " + ResolvedServers.Count + " Perforce servers" );
			return ResolvedServers;
		}

		List<string> UE3BranchFiles = new List<string>()
		{
			"Binaries/build.properties",
			"Engine/Config/BaseEngine.ini"
		};

		List<string> UE4BranchFiles = new List<string>()
		{
			"Engine/Build/build.properties",
			"Engine/Config/BaseEngine.ini"
		};

		private bool ValidateBranch( string BranchName, List<string> BranchFiles )
		{
			// Make sure it includes all files
			if( !BranchName.EndsWith( "/..." ) )
			{
				return false;
			}

			string BranchRoot = BranchName.Substring( 0, BranchName.Length - "...".Length );

			if( BranchRoot == "//depot/" )
			{
				bReportInvalidClientSpec = true;
				return false;
			}

			bool bFileExists = false;
			foreach( string BranchFile in BranchFiles )
			{
				try
				{
					string BranchFileName = BranchRoot + BranchFile;
					P4RecordSet Records = IP4Net.Run( "fstat", BranchFileName );
					bFileExists = ( Records.Records.Length > 0 );
				}
				catch
				{
					bFileExists = false;
				}
			}

			return bFileExists;
		}

		private void AnalyseClientSpecs( PerforceInfo P4Server, P4Record ClientSpec, ref List<BranchSpec> BranchSpecs )
		{
			// Grab the details of the clientspec
			string ClientSpecName = ClientSpec["client"];
			IP4Net.Client = ClientSpecName;
			P4RecordSet ClientSpecDetails = IP4Net.Run( "client", "-o", ClientSpecName );

			// Make a list of unique branch references
			if( ClientSpecDetails.Records.Length > 0 && ClientSpecDetails.Records[0].ArrayFields["View"] != null )
			{
				List<string> Branches = new List<string>();
				foreach( string View in ClientSpecDetails.Records[0].ArrayFields["View"] )
				{
					string PotentialBranch = View.ToLower();

					if( PotentialBranch.StartsWith( "-" ) )
					{
						continue;
					}

					string DepotFileSpec = PotentialBranch.Substring( 0, PotentialBranch.IndexOf( " " ) );
					if( !ValidateBranch( DepotFileSpec, UE3BranchFiles ) && !ValidateBranch( DepotFileSpec, UE4BranchFiles ) )
					{
						Debug.WriteLine( " ...... ignoring branch: " + PotentialBranch );
						continue;
					}

					// Extract the depot name of the branch
					int StringIndex = PotentialBranch.IndexOf( " " );
					if( StringIndex < 0 )
					{
						continue;
					}
					string BranchDepotName = View.Substring( 0, StringIndex );

					BranchDepotName = BranchDepotName.TrimStart( "/".ToCharArray() );

					StringIndex = BranchDepotName.IndexOf( '/' );
					if( StringIndex < 0 )
					{
						continue;
					}
					BranchDepotName = BranchDepotName.Substring( StringIndex + 1 );

					StringIndex = BranchDepotName.LastIndexOf( '/' );
					if( StringIndex < 0 )
					{
						continue;
					}
					BranchDepotName = BranchDepotName.Substring( 0, StringIndex );

					// "Partners/Adobe/UnrealEngine3-Flash"
					// UnrealEngine3"

					// Get the client name of the branch
					int ClientSpecIndex = PotentialBranch.IndexOf( ClientSpecName.ToLower() );
					if( ClientSpecIndex < 0 )
					{
						continue;
					}
					PotentialBranch = View.Substring( ClientSpecIndex + ClientSpecName.Length + 1 );

					StringIndex = PotentialBranch.LastIndexOf( '/' );
					if( StringIndex < 0 )
					{
						continue;
					}
					string BranchName = PotentialBranch.Substring( 0, StringIndex );

					// If we haven't already, add it to the list
					if( !Branches.Contains( BranchName ) )
					{
						string BranchRoot = ClientSpec["Root"];

						BranchSpec NewBranch = new BranchSpec( P4Server.FriendlyName, P4Server.UserName, ClientSpecName, BranchName, BranchDepotName, BranchRoot );
						BranchSpecs.Add( NewBranch );

						Branches.Add( BranchName );
						Debug.WriteLine( " ...... added branch: " + BranchDepotName );
					}
				}
			}
		}

		public List<BranchSpec> GetClientSpecs( List<PerforceInfo> PerforceServerInfos )
		{
			List<BranchSpec> BranchSpecs = new List<BranchSpec>();

			Debug.WriteLine( "Analysing servers ..." );
			foreach( PerforceInfo P4Server in PerforceServerInfos )
			{
				Debug.WriteLine( " ... analysing clientspecs for: " + P4Server.FriendlyName + "/" + P4Server.ServerName );

				// Get a list of clientspecs associated with this user and server
				IP4Net.Port = P4Server.FriendlyName;
				IP4Net.User = P4Server.UserName;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
				P4RecordSet ClientSpecs = IP4Net.Run( "clients", "-u", IP4Net.User );

				// Find out which ones are on this machine
				foreach( P4Record ClientSpec in ClientSpecs.Records )
				{
					// Check to see if we're on the correct host
					if( ClientSpec["Host"].ToLower() == Environment.MachineName.ToLower() )
					{
						AnalyseClientSpecs( P4Server, ClientSpec, ref BranchSpecs );
					}
					else if( ClientSpec["Host"].Length == 0 )
					{
						// ... or if we have a blank host
						string BranchRoot = ClientSpec["Root"];
						if( Directory.Exists( BranchRoot ) )
						{
							AnalyseClientSpecs( P4Server, ClientSpec, ref BranchSpecs );
						}
					}
					else
					{
						Debug.WriteLine( " ...... ignoring clientspec (wrong host): " + ClientSpec["client"] );
					}
				}

				IP4Net.Disconnect();
			}

			Debug.WriteLine( " ... completed analysing servers" );
			return BranchSpecs;
		}

		private void OpenLog( BranchSpec Branch )
		{
			DateTime LocalTime = DateTime.Now;

			string TimeStamp = LocalTime.Year + "-"
						+ LocalTime.Month.ToString( "00" ) + "-"
						+ LocalTime.Day.ToString( "00" ) + "_"
						+ LocalTime.Hour.ToString( "00" ) + "."
						+ LocalTime.Minute.ToString( "00" ) + "."
						+ LocalTime.Second.ToString( "00" );

			string BaseDirectory = Application.StartupPath;
			if( ApplicationDeployment.IsNetworkDeployed )
			{
				BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
			}

			string LogFileName = "[" + TimeStamp + "]_" + Branch.DepotName + ".txt";
			string LogPath = Path.Combine( BaseDirectory, LogFileName );

			Log = TextWriter.Synchronized( new StreamWriter( LogPath, true, Encoding.Unicode ) );

			WriteToLog( "User: " + Branch.UserName );
			WriteToLog( "ClientSpec: " + Branch.ClientSpec + " (" + Branch.Root + ")" );
			WriteToLog( "" );
			WriteToLog( "AutoClobber: " + Parent.Options.AutoClobber.ToString() );
			WriteToLog( "ResolveType: " + Parent.Options.ResolveType.ToString() );
			WriteToLog( "" );

			ErrorCount = 0;
			WarningCount = 0;
			MessageCount = 0;
			FilesSynced = 0;
			TotalDuration = new TimeSpan( 0 );
		}

		private void AnalyseResults()
		{
			if( ErrorCount > 0 )
			{
				SummaryTitle = "Sync failed with " + ErrorCount.ToString() + " errors";
				SummaryIcon = ToolTipIcon.Error;
			}
			else
			{
				SummaryTitle = "Sync succeeded";
				SummaryIcon = ToolTipIcon.Info;
			}

			SummaryText = FilesSynced.ToString() + " files synced taking " + TotalDuration.TotalSeconds.ToString( "F0" ) + " seconds.";
		}

		private void CloseLog()
		{
            if( Log != null )
            {
				WriteToLog( "Total errors: " + ErrorCount.ToString() );
				WriteToLog( "Total warnings: " + WarningCount.ToString() );
				WriteToLog( "Total messages: " + MessageCount.ToString() );
				WriteToLog( "Total files synced: " + FilesSynced.ToString() );
				WriteToLog( "Total duration: " + TotalDuration.TotalSeconds.ToString( "F0" ) + " seconds" );
				WriteToLog( "End of sync" );
				
				Log.Close();
				Log = null;
            }
        }

		private void WriteToLog( string Line )
		{
			if( Log != null )
			{
				Log.WriteLine( DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Line );
			}
		}

		private void WriteToLog( P4RecordSet Results )
		{
			if( Log != null )
			{
				WriteToLog( Results.Errors.Length.ToString() + " errors found" );
				foreach( string Error in Results.Errors )
				{
					string ErrorMessage = Error.Replace( "\r", "" );
					ErrorMessage = ErrorMessage.Replace( "\n", " " );
					WriteToLog( "ERROR: " + ErrorMessage );
				}

				WriteToLog( Results.Warnings.Length.ToString() + " warnings found" );
				foreach( string Warning in Results.Warnings )
				{
					string WarningMessage = Warning.Replace( "\r", "" );
					WarningMessage = WarningMessage.Replace( "\n", " " );
					WriteToLog( " ... " + WarningMessage );
				}

				WriteToLog( Results.Messages.Length.ToString() + " messages found" );
				foreach( string Message in Results.Messages )
				{
					WriteToLog( " ... " + Message.Replace( Environment.NewLine, "" ) );
				}

				WriteToLog( Results.Records.Length.ToString() + " records returned" );
				foreach( P4Record Record in Results )
				{
					WriteToLog( " ... " + Record["depotFile"] + "#" + Record["rev"] + " " + Record["action"] );
				}

				WriteToLog( "" );
			}
		}

		private void CollateResults( P4RecordSet Results, TimeSpan Duration )
		{
			ErrorCount += Results.Errors.Length;
			WarningCount += Results.Warnings.Length;
			MessageCount += Results.Messages.Length;
			FilesSynced += Results.Records.Length;

			TotalDuration += Duration;
		}

		/** Dump the info about the revision to the log file */
		private void DumpRevisionInfo( BranchSpec Branch, string Revision )
		{
			int SpecificChangelist = -1;
			if( Int32.TryParse( Revision.TrimStart( "@".ToCharArray() ), out SpecificChangelist ) )
			{
				WriteToLog( " ... syncing to changelist " + SpecificChangelist.ToString() );
			}
			else if( Revision.ToLower() == "#head" )
			{
				P4RecordSet Results = IP4Net.Run( "changes", "-s", "submitted", "-m1", "//depot/" + Branch.DepotName + "/..." );
				if( Results != null && Results.Records.Length > 0 )
				{
					string ChangeListString = Results.Records[0].Fields["change"];
					WriteToLog( " ... syncing to #head (changelist " + ChangeListString + ")" );
				}
			}
			else
			{
				P4RecordSet Results = IP4Net.Run( "label", "-o", Revision.TrimStart( "@".ToCharArray() ) );
				if( Results != null && Results.Records.Length > 0 )
				{
					WriteToLog( " ... syncing to label " + Revision );
					WriteToLog( "" );

					string[] DescriptionLines = Results.Records[0].Fields["Description"].Split( "\n\r".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					foreach( string Line in DescriptionLines )
					{
						WriteToLog( Line );
					}
				}
			}

			WriteToLog( "" );
		}

		/** Check to see if any writable files need to be clobbered */
		private void CheckClobber( P4RecordSet Results, string Revision )
		{
			if( Parent.Options.AutoClobber )
			{
				foreach( string Error in Results.Errors )
				{
					if( Error.StartsWith( "Can't clobber writable file " ) )
					{
						string FileName = Error.Substring( "Can't clobber writable file ".Length );
						foreach( P4Record Record in Results )
						{
							if( FileName == Record["clientFile"] )
							{
								FileName = Record["depotFile"];
								break;
							}
						}

						P4RecordSet ForceSyncResults = IP4Net.Run( "sync", "-f", FileName + Revision );
						WriteToLog( "Command: p4 sync -f " + FileName + Revision );
						WriteToLog( ForceSyncResults );

						if( ForceSyncResults.Errors.Length == 0 )
						{
							ErrorCount--;
						}
					}
				}
			}
		}

		/** Apply the desired resolve paradigm */
		private void Resolve( BranchSpec Branch )
		{
			string ResolveParameter = "";
			switch( Parent.Options.ResolveType )
			{
			case EResolveType.None:
				break;

			case EResolveType.Automatic:
				ResolveParameter = "-am";
				break;

			case EResolveType.SafeAutomatic:
				ResolveParameter = "-as";
				break;

			case EResolveType.AutoWithConflicts:
				ResolveParameter = "-af";
				break;
			}

			if( ResolveParameter.Length > 0 )
			{
				DateTime StartTime = DateTime.UtcNow;
				P4RecordSet Results = IP4Net.Run( "resolve", ResolveParameter, "//depot/" + Branch.DepotName + "/..." );
				TimeSpan Duration = DateTime.UtcNow - StartTime;
				CollateResults( Results, Duration );

				WriteToLog( "Command: p4 resolve " + ResolveParameter + " //depot/" + Branch.DepotName + "/... (" + Duration.TotalSeconds.ToString( "F0" ) + " seconds)" );
				WriteToLog( Results );
			}
		}

		/** Sync an entire branch to a single specified revision */
		public void SyncRevision( BranchSpec Branch, string Revision )
		{
			try
			{
				SummaryIcon = ToolTipIcon.Info;
				OpenLog( Branch );

				DateTime StartTime = DateTime.UtcNow;

				IP4Net.Port = Branch.Server;
				IP4Net.User = Branch.UserName;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;

				DumpRevisionInfo( Branch, Revision );

				string Parameters = "//depot/" + Branch.DepotName + "/..." + Revision;
				P4RecordSet Results = IP4Net.Run( "sync", Parameters );

				TimeSpan Duration = DateTime.UtcNow - StartTime;
				CollateResults( Results, Duration );

				WriteToLog( "Command: p4 sync " + Parameters + " (" + Duration.TotalSeconds.ToString( "F0" ) + " seconds)" );
				WriteToLog( Results );

				CheckClobber( Results, Revision );
				Resolve( Branch );

				IP4Net.Disconnect();
				CloseLog();

				AnalyseResults();
			}
			catch( Exception Ex )
			{
				WriteToLog( "ERROR: Exception During Sync!" );
				WriteToLog( Ex.Message );
				IP4Net.Disconnect();
				CloseLog();

				SummaryTitle = "Exception During Sync!";
				SummaryText = Ex.Message;
				SummaryIcon = ToolTipIcon.Error;
			}
		}

		/** Sync a mixture of files to various revisions e.g. and ArtistSync */
		public void SyncRevision( BranchSpec Branch, string Revision, List<string> SyncCommands )
		{
			try
			{
				SummaryIcon = ToolTipIcon.Info;
				OpenLog( Branch );

				IP4Net.Port = Branch.Server;
				IP4Net.User = Branch.UserName;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;

				DumpRevisionInfo( Branch, Revision );

				foreach( string SyncCommand in SyncCommands )
				{
					DateTime StartTime = DateTime.UtcNow;

					string Parameters = "//depot/" + Branch.DepotName + SyncCommand.Replace( "%LABEL_TO_SYNC_TO%", Revision );
					P4RecordSet Results = IP4Net.Run( "sync", Parameters );

					TimeSpan Duration = DateTime.UtcNow - StartTime;
					CollateResults( Results, Duration );

					WriteToLog( "Command: p4 sync " + Parameters + " (" + Duration.TotalSeconds.ToString( "F0" ) + " seconds)" );
					WriteToLog( Results );

					CheckClobber( Results, Revision );
				}

				Resolve( Branch );

				IP4Net.Disconnect();
				CloseLog();

				AnalyseResults();
			}
			catch( Exception Ex )
			{
				WriteToLog( "ERROR: Exception During Sync!" );
				WriteToLog( Ex.Message );
				IP4Net.Disconnect();
				CloseLog();

				SummaryTitle = "Exception During Sync!";
				SummaryText = Ex.Message;
				SummaryIcon = ToolTipIcon.Error;
			}
		}
	}
}
