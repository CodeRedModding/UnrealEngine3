// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Deployment.Application;
using System.Threading;
using System.Drawing;
using System.Linq;
using System.Text;

namespace Controller.Models
{
	public partial class BuilderLinq
	{
		/// <summary>
		/// Reference to main controller object for utility functions
		/// </summary>
		static public Main Parent = null;

		/// <summary>
		/// Generic retrying of Linq operations via delegates
		/// </summary>
		private delegate int LinqOperation( object DataContext, string Parameter );

		/// <summary>
		/// Delegate for inserting build log rows
		/// </summary>
		static private int InsertBuildLog( object DataContext, string Parameter )
		{
			BuildLogDataContext BuildLogData = ( BuildLogDataContext )DataContext;
			BuildLogData.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Delegate for broadcasting the machine info
		/// </summary>
		static private int GenericBuilderSubmit( object DataContext, string Parameter )
		{
			BuildersDataContext BuildersData = ( BuildersDataContext )DataContext;
			BuildersData.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Delegate for generic buildlog update queries
		/// </summary>
		static private int GenericBuildLogUpdate( object DataContext, string Parameter )
		{
			BuildLogDataContext BuildLogData = ( BuildLogDataContext )DataContext;
			BuildLogData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for unbroadcasting the machine info
		/// </summary>
		static private int GenericBuilderUpdate( object DataContext, string Parameter )
		{
			BuildersDataContext BuildersData = ( BuildersDataContext )DataContext;
			BuildersData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for submitting changes to the commands table
		/// </summary>
		static private int SubmitCommandsChanges( object DataContext, string Parameter )
		{
			CommandsDataContext CommandsData = ( CommandsDataContext )DataContext;
			CommandsData.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Delegate for generic command update queries
		/// </summary>
		static private int GenericCommandUpdate( object DataContext, string Parameter )
		{
			CommandsDataContext CommandsData = ( CommandsDataContext )DataContext;
			CommandsData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for generic command update queries
		/// </summary>
		static private int GenericBranchConfigUpdate( object DataContext, string Parameter )
		{
			BranchConfigDataContext BranchConfigData = ( BranchConfigDataContext )DataContext;
			BranchConfigData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for updating the build state
		/// </summary>
		static private int GenericChangelistUpdate( object DataContext, string Parameter )
		{
			Changelist2DataContext Changelist2Data = ( Changelist2DataContext )DataContext;
			Changelist2Data.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Delegate for generic job update queries
		/// </summary>
		static private int GenericJobUpdate( object DataContext, string Parameter )
		{
			JobsDataContext JobsData = ( JobsDataContext )DataContext;
			JobsData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for adding job rows
		/// </summary>
		static private int SubmitJobChanges( object DataContext, string Parameter )
		{
			JobsDataContext JobsData = ( JobsDataContext )DataContext;
			JobsData.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Delegate for adding job states
		/// </summary>
		static private int AddJobState( object DataContext, string Parameter )
		{
			CISTaskDataContext CISJobStatesData = ( CISTaskDataContext )DataContext;
			CISJobStatesData.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Delegate for updating changelists
		/// </summary>
		static private int UpdateCISTask( object DataContext, string Parameter )
		{
			CISTaskDataContext CISJobStatesData = ( CISTaskDataContext )DataContext;
			CISJobStatesData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for updating variables
		/// </summary>
		static private int GenericVariableUpdate( object DataContext, string Parameter )
		{
			VariablesDataContext VariablesData = ( VariablesDataContext )DataContext;
			VariablesData.ExecuteCommand( Parameter );
			return 0;
		}

		/// <summary>
		/// Delegate for inserting build log rows
		/// </summary>
		static private int InsertVerioning( object DataContext, string Parameter )
		{
			VersioningDataContext VersioningData = ( VersioningDataContext )DataContext;
			VersioningData.SubmitChanges();
			return 0;
		}

		/// <summary>
		/// Retry a Linq query in an exception checked environment
		/// </summary>
		/// <param name="Operation">The delegate for the operation to run</param>
		/// <param name="DataContext">The data context to run the query on</param>
		/// <param name="Parameter">The string of the query</param>
		/// <returns>The return value of the query</returns>
		/// <remarks>The query is retried up to 4 times, sending a warning email with each retry. If it fails all 4 times, it sends an error email.</remarks>
		static private int RetryLinqOperation( LinqOperation Operation, object DataContext, string Parameter )
		{
			int RetryCount = 0;
			int Result = 0;

			while( true )
			{
				try
				{
					Result = Operation( DataContext, Parameter );
					return Result;
				}
				catch( Exception Ex )
				{
					string Message = DataContext.ToString() + Environment.NewLine + Environment.NewLine;
					if( Parameter != null )
					{
						Message += Parameter + Environment.NewLine + Environment.NewLine;
					}
					Message += Ex.ToString();
		
					RetryCount++;
					if( RetryCount >= 4 )
					{
						Parent.SendErrorMail( "Linq fail after " + RetryCount + " retries", Message );
						return -1;
					}	

					Parent.SendWarningMail( "Linq operation failed; attempting retry #" + RetryCount.ToString(), Message, false );

					Random Rand = new Random();
					Thread.Sleep( 50 + Rand.Next( 100 ) );
				}
			}
		}

		/// <summary>
		/// Create a new build log row and insert into the database
		/// </summary>
		static public int CreateBuildLog( Main.CommandInfo CommandDetails, Main.BranchDefinition BranchDef, string MachineName )
		{
			try
			{
				using( BuildLogDataContext BuildLogData = new BuildLogDataContext() )
				{
					BuildLog NewBuildLog = new BuildLog();

					NewBuildLog.Command = CommandDetails.Script;
					NewBuildLog.CommandID = CommandDetails.CommandID;
					NewBuildLog.JobID = CommandDetails.JobID;
					NewBuildLog.Machine = MachineName;
					NewBuildLog.ChangeList = 0;
					NewBuildLog.BuildStarted = CommandDetails.BuildStarted;
					NewBuildLog.BuildEnded = NewBuildLog.BuildStarted;
					NewBuildLog.BuildLabel = null;
					NewBuildLog.DetailedLogPath = "";
					NewBuildLog.BranchConfigID = ( short )BranchDef.ID;

					BuildLogData.BuildLogs.InsertOnSubmit( NewBuildLog );
					RetryLinqOperation( new LinqOperation( InsertBuildLog ), BuildLogData, "CreateBuildLog" );

					return NewBuildLog.ID;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "CreateBuildLog", Ex.Message );
			}

			return -1;
		}

		/// <summary>
		/// Set the changelist for this build log row
		/// </summary>
		static public void SetBuildLogChangelist( int BuildLogID, int Changelist )
		{
			try
			{
				using( BuildLogDataContext BuildLogData = new BuildLogDataContext() )
				{
					string UpdateQuery = "UPDATE BuildLog SET ChangeList = " + Changelist.ToString() + " WHERE ( ID = " + BuildLogID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericBuildLogUpdate ), BuildLogData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetBuildLogChangelist", Ex.Message );
			}
		}

		/// <summary>
		/// Set the end time for this build log
		/// </summary>
		static public void LogBuildEnded( int BuildLogID )
		{
			try
			{
				using( BuildLogDataContext BuildLogData = new BuildLogDataContext() )
				{
					string UpdateQuery = "UPDATE BuildLog SET BuildEnded = '" + DateTime.UtcNow.ToString( Main.US_TIME_DATE_STRING_FORMAT ) + "' WHERE ( ID = " + BuildLogID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericBuildLogUpdate ), BuildLogData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "LogBuildEnded", Ex.Message );
			}
		}

		/// <summary>
		/// Set a string in the BuildLog row
		/// </summary>
		static public void SetBuildLogString( int BuildLogID, string Field, string Value )
		{
			try
			{
				using( BuildLogDataContext BuildLogData = new BuildLogDataContext() )
				{
					if( Value != "null" )
					{
						Value = "'" + Value.Replace( "'", "" ) + "'";
					}

					string UpdateQuery = "UPDATE BuildLog SET " + Field + " = " + Value + " WHERE ( ID = " + BuildLogID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericBuildLogUpdate ), BuildLogData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetBuildLogString", Ex.Message );
			}
		}

		/// <summary>
		/// Broadcast that the machine is available to participate in the build farm
		/// </summary>
		static public int BroadcastMachine( Main Parent )
		{
			try
			{
				using( BuildersDataContext BuildersData = new BuildersDataContext() )
				{
					Builder NewBuilder = new Builder();

					NewBuilder.Machine = Parent.MachineName;
					NewBuilder.Version = Parent.CompileDateTime;
					NewBuilder.Deployed = ApplicationDeployment.IsNetworkDeployed;
					NewBuilder.State = "Connected";
					NewBuilder.StartTime = DateTime.UtcNow;

					NewBuilder.DirectXSDKVersion = Parent.DXVersion;
					NewBuilder.AndroidSDKVersion = Parent.AndroidSDKVersion;
					NewBuilder.iPhoneSDKVersion = Parent.iPhoneSDKVersion;
					NewBuilder.MacSDKVersion = Parent.MacSDKVersion;
					NewBuilder.NGPSDKVersion = Parent.NGPSDKVersion;
					NewBuilder.PS3SDKVersion = Parent.PS3SDKVersion;
					NewBuilder.WiiUSDKVersion = Parent.WiiUSDKVersion;
					NewBuilder.Xbox360SDKVersion = Parent.Xbox360SDKVersion;
					NewBuilder.FlashSDKVersion = Parent.FlashSDKVersion;

					NewBuilder.CPUString = Parent.ProcessorString;
					NewBuilder.ProcessorCount = Parent.NumProcessors;
					NewBuilder.Memory = Parent.PhysicalMemory;
					NewBuilder.DriveCSize = Parent.DriveCSize;
					NewBuilder.DriveCFree = Parent.DriveCFree;
					NewBuilder.DriveDSize = Parent.DriveDSize;
					NewBuilder.DriveDFree = Parent.DriveDFree;

					BuildersData.Builders.InsertOnSubmit( NewBuilder );
					RetryLinqOperation( new LinqOperation( GenericBuilderSubmit ), BuildersData, "BroadcastMachine" );

					return NewBuilder.ID;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "BroadcastMachine", Ex.Message );
			}

			return -1;
		}

		/// <summary>
		/// Mark any orphaned builders as zombies
		/// </summary>
		static public void CleanupOrphanedBuilders( string MachineName )
		{
			try
			{
				using( BuildersDataContext BuildersData = new BuildersDataContext() )
				{
					string UpdateQuery = "UPDATE Builders SET State = 'Zombied' WHERE ( Machine = '" + MachineName + "' AND State != 'Dead' AND State != 'Zombied' )";
					RetryLinqOperation( new LinqOperation( GenericBuilderUpdate ), BuildersData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "CleanupOrphanedBuilders", Ex.Message );
			}
		}

		/// <summary>
		/// Mark the machine as no longer available for tasks
		/// </summary>
		static public void UnbroadcastMachine( Main Parent )
		{
			try
			{
				using( BuildersDataContext BuildersData = new BuildersDataContext() )
				{
					string UpdateQuery = "UPDATE Builders SET State = 'Dead', EndTime = '" + DateTime.UtcNow.ToString( Main.US_TIME_DATE_STRING_FORMAT ) + "' ";
					UpdateQuery += "WHERE ( Machine = '" + Parent.MachineName + "' AND State != 'Dead' AND State != 'Zombied' )";
					RetryLinqOperation( new LinqOperation( GenericBuilderUpdate ), BuildersData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UnbroadcastMachine", Ex.Message );
			}
		}

		/// <summary>
		/// Set the current state of a builder
		/// </summary>
		static public void SetCurrentStatus( int BuilderID, string Line )
		{
			try
			{
				using( BuildersDataContext BuildersData = new BuildersDataContext() )
				{
					if( Line != "null" )
					{
						Line = "'" + Line.Replace( "'", "" ) + "'";
					}

					string UpdateQuery = "UPDATE Builders SET CurrentStatus = " + Line + " WHERE ( ID = " + BuilderID + " )";
					RetryLinqOperation( new LinqOperation( GenericBuilderUpdate ), BuildersData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetCurrentStatus", Ex.Message );
			}
		}

		/// <summary>
		/// Set the current state of a builder
		/// </summary>
		static public void SetBuildState( int BuilderID, string State )
		{
			try
			{
				using( BuildersDataContext BuildersData = new BuildersDataContext() )
				{
					string UpdateQuery = "UPDATE Builders SET State = '" + State + "' WHERE ( ID = " + BuilderID + " )";
					RetryLinqOperation( new LinqOperation( GenericBuilderUpdate ), BuildersData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetBuildState", Ex.Message );
			}
		}

		/// <summary>
		/// Ping the database showing this controller is still ticking
		/// </summary>
		static public void PingDatabase( int BuilderID )
		{
			try
			{
				using( BuildersDataContext BuildersData = new BuildersDataContext() )
				{
					string UpdateQuery = "UPDATE Builders SET CurrentTime = '" + DateTime.UtcNow.ToString( Main.US_TIME_DATE_STRING_FORMAT ) + "' WHERE ( ID = " + BuilderID + " )";
					RetryLinqOperation( new LinqOperation( GenericBuilderUpdate ), BuildersData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "PingDatabase", Ex.Message );
			}
		}

		/// <summary>
		/// Grab the publishing conch if it is available
		/// </summary>
		static public bool AvailableBandwidth( int CommandID )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					// Grab the rows we are interested in - the current conch holders and our current build
					IQueryable<Command> Commands =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.ConchHolder != null || CommandDetail.ID == CommandID
						select CommandDetail
					);

					// Delete any timed out conch holders, and work out if anyone else is
					bool bCurrentHolder = false;
					foreach( Command ConchHolder in Commands )
					{
						if( ConchHolder.ConchHolder.HasValue )
						{
							if( ConchHolder.ConchHolder.Value.AddHours( -1 ) > DateTime.UtcNow || ConchHolder.ID == CommandID )
							{
								ConchHolder.ConchHolder = null;
							}
							else
							{
								bCurrentHolder = true;
							}
						}
					}

					// If no one has the conch, grab it
					if( !bCurrentHolder )
					{
						foreach( Command ConchHolder in Commands )
						{
							if( ConchHolder.ID == CommandID )
							{
								ConchHolder.ConchHolder = DateTime.UtcNow;
							}
						}
					}

					// Apply any changes
					RetryLinqOperation( new LinqOperation( SubmitCommandsChanges ), CommandsData, "AvailableBandwidth" );

					return !bCurrentHolder;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "AvailableBandwidth", Ex.Message );
			}

			return false;
		}

		/// <summary>
		/// Get the details of the given command
		/// </summary>
		static public Main.CommandInfo GetCommandInfo( int CommandID )
		{
			Main.CommandInfo CommandDetails = null;

			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					CommandDetails =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.ID == CommandID
						select new Main.CommandInfo( CommandDetail.ID,
												  CommandDetail.Command1,
												  CommandDetail.Description,
												  CommandDetail.BranchConfigID,
												  CommandDetail.Game,
												  CommandDetail.Platform,
												  CommandDetail.Config,
												  CommandDetail.Language,
												  CommandDetail.Parameter,
												  CommandDetail.Remote,
												  CommandDetail.Operator,
												  CommandDetail.LatestApprovedLabel,
												  CommandDetail.LastAttemptedChangeList,
												  CommandDetail.LastGoodChangeList,
												  CommandDetail.LastFailedChangeList,
												  CommandDetail.PrimaryBuild
												)
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetCommandInfo", Ex.Message );
				CommandDetails = null;
			}

			return CommandDetails;
		}

		/// <summary>
		/// Get the details of the given job
		/// </summary>
		static public Main.CommandInfo GetJobInfo( int JobID )
		{
			Main.CommandInfo JobDetails = null;

			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					JobDetails =
					(
						from JobDetail in JobsData.Jobs
						where JobDetail.ID == JobID
						select new Main.CommandInfo( JobDetail.ID,
												  JobDetail.Command,
												  JobDetail.Name,
												  JobDetail.BranchConfigID,
												  JobDetail.CISTaskID,
												  JobDetail.CISJobStateID,
												  JobDetail.Game,
												  JobDetail.Platform,
												  JobDetail.Config,
												  JobDetail.ScriptConfig,
												  JobDetail.Language,
												  JobDetail.Define,
												  JobDetail.Parameter,
												  JobDetail.Remote,
												  JobDetail.Label,
												  JobDetail.PrimaryBuild
												)
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetJobInfo", Ex.Message );
				JobDetails = null;
			}

			return JobDetails;
		}

		/// <summary>
		/// Check to see if the blocking build has completed
		/// </summary>
		static public bool BlockingBuildDone( int BlockingBuildID, DateTime BuildStarted )
		{
			bool bBuildDone = false;
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					bBuildDone =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.ID == BlockingBuildID
						where CommandDetail.BuildLogID != 0 || CommandDetail.LastAttemptedDateTime >= BuildStarted
						select CommandDetail.ID
					).Any();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "BlockingBuildDone", Ex.Message );
				bBuildDone = false;
			}

			return bBuildDone;
		}

		/// <summary>
		/// Get the start time of the blocking build
		/// </summary>
		static public DateTime? GetBlockingBuildStartTime( int BlockingBuildID )
		{
			DateTime? BlockingBuildStartTime = DateTime.MaxValue;

			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					BlockingBuildStartTime =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.ID == BlockingBuildID
						join BuildLogDetail in CommandsData.BuildLogAlts on CommandDetail.BuildLogID equals BuildLogDetail.ID
						select BuildLogDetail.BuildStarted
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetBlockingBuildStartTime", Ex.Message );
				BlockingBuildStartTime = DateTime.MaxValue;
			}

			return BlockingBuildStartTime;
		}

		/// <summary>
		/// Get the build that spawned this one
		/// </summary>
		static public int GetParentBuild( string Dependency )
		{
			int BlockingBuildID = 0;
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					BlockingBuildID =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.Description == Dependency
						select CommandDetail.ID
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetParentBuild", Ex.Message );
				BlockingBuildID = 0;
			}

			return BlockingBuildID;
		}

		/// <summary>
		/// Poll for any build being killed
		/// </summary>
		static public int PollForKillBuild( string MachineName )
		{
			int KilledBuild = 0;
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					KilledBuild =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.Machine == MachineName && CommandDetail.Killing
						select CommandDetail.ID
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "PollForKillBuild", Ex.Message );
				KilledBuild = 0;
			}

			return KilledBuild;
		}

		/// <summary>
		/// Get the last good changelist of a command
		/// </summary>
		static public int GetLastGoodChangelist( int CommandID )
		{
			int LastGoodChange = 0;
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					LastGoodChange =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.ID == CommandID
						select CommandDetail.LastGoodChangeList
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetLastGoodChangelist", Ex.Message );
				LastGoodChange = 0;
			}

			return LastGoodChange;
		}

		/// <summary>
		/// Check to see if the given machine is reserved for any commands
		/// </summary>
		static public int GetMachineLock( string MachineName )
		{
			int MachineLockID = 0;
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					MachineLockID =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.MachineLock.ToLower().Contains( MachineName.ToLower() )
						select CommandDetail.ID
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetMachineLock", Ex.Message );
				MachineLockID = 0;
			}

			return MachineLockID;
		}

		/// <summary>
		/// Update a field in the commands table
		/// </summary>
		static public void SetCommandString( int CommandID, string Field, string Value )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					if( Value != "null" )
					{
						Value = "'" + Value.Replace( "'", "" ) + "'";
					}

					string UpdateQuery = "UPDATE Commands SET " + Field + " = " + Value + " WHERE ( ID = " + CommandID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericCommandUpdate ), CommandsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetCommandString", Ex.Message );
			}
		}

		/// <summary>
		/// Remove any stale data from the command
		/// </summary>
		static public void CleanupCommand( int CommandID, string MachineName )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					string UpdateQuery = "UPDATE Commands SET Machine = '', Killing = 0, Killer = '', BuildLogID = null, ConchHolder = null WHERE ( ID = " + CommandID.ToString() + " OR Machine = '" + MachineName + "' )";
					RetryLinqOperation( new LinqOperation( GenericCommandUpdate ), CommandsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "CleanupCommand", Ex.Message );
			}
		}

		/// <summary>
		/// Set the active information for a command
		/// </summary>
		static public void SetCommandActive( int CommandID, int BuildLogID, string MachineName )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					string UpdateQuery = "UPDATE Commands SET BuildLogID = " + BuildLogID.ToString() + ", Machine = '" + MachineName + "' WHERE ( ID = " + CommandID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericCommandUpdate ), CommandsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetCommandActive", Ex.Message );
			}
		}

		/// <summary>
		/// Set the changelist of the last attempted build for the command
		/// </summary>
		static public void SetLastAttemptedBuild( int CommandID, int ChangeList )
		{
			try
			{
				if( CommandID != 0 && ChangeList != 0 )
				{
					using( CommandsDataContext CommandsData = new CommandsDataContext() )
					{
						string UpdateQuery = "UPDATE Commands SET LastAttemptedDateTime = '" + DateTime.UtcNow.ToString( Main.US_TIME_DATE_STRING_FORMAT ) + "', ";
						UpdateQuery += "LastAttemptedChangeList = " + ChangeList.ToString() + " WHERE ( ID = " + CommandID.ToString() + " )";

						RetryLinqOperation( new LinqOperation( GenericCommandUpdate ), CommandsData, UpdateQuery );
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetLastAttemptedBuild", Ex.Message );
			}
		}

		/// <summary>
		/// Set the changelist of the last failed build for the command
		/// </summary>
		static public void SetLastFailedBuild( int CommandID, int ChangeList )
		{
			try
			{
				if( CommandID != 0 && ChangeList != 0 )
				{
					using( CommandsDataContext CommandsData = new CommandsDataContext() )
					{
						string UpdateQuery = "UPDATE Commands SET LastFailedDateTime = '" + DateTime.UtcNow.ToString( Main.US_TIME_DATE_STRING_FORMAT ) + "', ";
						UpdateQuery += "LastFailedChangeList = " + ChangeList.ToString() + " WHERE ( ID = " + CommandID.ToString() + " )";

						RetryLinqOperation( new LinqOperation( GenericCommandUpdate ), CommandsData, UpdateQuery );
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetLastFailedBuild", Ex.Message );
			}
		}

		/// <summary>
		/// Set the changelist of the last good build for the command
		/// </summary>
		static public void SetLastGoodBuild( int CommandID, int ChangeList )
		{
			try
			{
				if( CommandID != 0 && ChangeList != 0 )
				{
					using( CommandsDataContext CommandsData = new CommandsDataContext() )
					{
						string UpdateQuery = "UPDATE Commands SET LastGoodDateTime = '" + DateTime.UtcNow.ToString( Main.US_TIME_DATE_STRING_FORMAT ) + "', ";
						UpdateQuery += "LastGoodChangeList = " + ChangeList.ToString() + " WHERE ( ID = " + CommandID.ToString() + " )";

						RetryLinqOperation( new LinqOperation( GenericCommandUpdate ), CommandsData, UpdateQuery );
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetLastGoodBuild", Ex.Message );
			}
		}

		/// <summary>
		/// Get a generic string from the commands table
		/// </summary>
		static public string GetCommandString( int CommandID, string Field )
		{
			string Result = "";

			try
			{
				string Query = "SELECT " + Field + " FROM Commands WHERE ( ID = " + CommandID + " )";
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					IEnumerable<string> Results = ( IEnumerable<string> )CommandsData.ExecuteQuery( typeof( string ), Query );
					Result = Results.FirstOrDefault();
					if( Result == null )
					{
						Result = "";
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetCommandString", Ex.Message );
				Result = "";
			}

			return Result;
		}

		/// <summary>
		/// Trigger a build with a specific name
		/// </summary>
		static public void Trigger( int CurrentCommandID, string BuildDescription, bool bForceAutoTimerUser )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					Command NewCommand =
					(
						from CommandDetail in CommandsData.Commands
						where CommandDetail.Description == BuildDescription
						select CommandDetail
					).FirstOrDefault();

					if( NewCommand != null )
					{
						if( NewCommand.Machine != null && NewCommand.Machine.Length > 0 )
						{
							Parent.Log( " ... suppressing retrigger of '" + BuildDescription + "'", Color.Magenta );
							Parent.SendAlreadyInProgressMail( NewCommand.Operator, BuildDescription );
						}
						else
						{
							string Operator = "AutoTimer";
							if( !bForceAutoTimerUser )
							{
								Operator = GetCommandString( CurrentCommandID, "Operator" );
							}

							NewCommand.Pending = true;
							NewCommand.Operator = Operator;

							RetryLinqOperation( new LinqOperation( SubmitCommandsChanges ), CommandsData, "Trigger" );
						}
					}
					else
					{
						Parent.SendErrorMail( "Build '" + BuildDescription + "' does not exist!", "" );
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "Trigger", Ex.Message );
			}
		}

		/// <summary>
		/// Get the list of branches from the database
		/// </summary>
		static public List<Main.BranchDefinition> GetBranches()
		{
			try
			{
				using( BranchConfigDataContext BranchConfigData = new BranchConfigDataContext() )
				{
					IQueryable<Main.BranchDefinition> Branches =
					(
						from Branch in BranchConfigData.BranchConfigs
						orderby Branch.ID ascending
						select new Main.BranchDefinition( Branch.ID,
														Branch.Version,
														Branch.Branch,
														Branch.Server,
														Branch.P4User,
														Branch.P4Password,
														Branch.P4SyncUser,
														Branch.P4SyncPassword,
														Branch.P4ClientSpec,
														Branch.BranchAdmin,
														Branch.DirectXVersion,
														Branch.AndroidSDKVersion,
														Branch.iPhoneSDKVersion,
														Branch.MacSDKVersion,
														Branch.NGPSDKVersion,
														Branch.PS3SDKVersion,
														Branch.WiiUSDKVersion,
														Branch.Xbox360SDKVersion,
														Branch.FlashSDKVersion,
														Branch.PFXSubjectName,
														Branch.IPhone_DeveloperSigningIdentity,
														Branch.IPhone_DistributionSigningIdentity )
					);

					return Branches.ToList();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetBranches", Ex.Message );
			}

			return new List<Main.BranchDefinition>();
		}

		/// <summary>
		/// Update a generic value in the branch config table
		/// </summary>
		static public void UpdateBranchConfigInt( int BranchConfigID, string Field, int Changelist )
		{
			try
			{
				using( BranchConfigDataContext BranchConfigData = new BranchConfigDataContext() )
				{
					string UpdateQuery = "Update BranchConfig SET " + Field + " = " + Changelist.ToString() + " WHERE ( ID = " + BranchConfigID + " )";
					RetryLinqOperation( new LinqOperation( GenericBranchConfigUpdate ), BranchConfigData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UpdateBranchConfigInt", Ex.Message );
			}
		}

		/// <summary>
		/// Retrieve a generic int value from the branch config table
		/// </summary>
		static public int GetIntFromBranch( int BranchConfigID, string Command )
		{
			int Result = -1;

			try
			{
				string Query = "SELECT " + Command + " FROM BranchConfig WHERE ( ID = " + BranchConfigID + " )";
				using( BranchConfigDataContext BranchConfigData = new BranchConfigDataContext() )
				{
					IEnumerable<int> Results = ( IEnumerable<int> )BranchConfigData.ExecuteQuery( typeof( int ), Query );
					Result = Results.FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetIntFromBranch", Ex.Message );
				Result = -1;
			}

			return Result;
		}

		/// <summary>
		/// Update the overall build state
		/// </summary>
		static public int UpdateBuildState( int BranchConfigID )
		{
			int LastFullyKnown = -1;

			try
			{
				using( Changelist2DataContext Changelists2Data = new Changelist2DataContext() )
				{
					// Get all the changelists more recent than LastFullyKnown
					IQueryable<Changelists2> Changelists =
					(
						from Changelist2Detail in Changelists2Data.Changelists2s
						where Changelist2Detail.BranchConfigID == BranchConfigID
						join BranchConfigDetail in Changelists2Data.BranchConfigAlts on Changelist2Detail.BranchConfigID equals BranchConfigDetail.ID
						where Changelist2Detail.Changelist >= BranchConfigDetail.LastFullyKnown
						orderby Changelist2Detail.Changelist ascending
						select Changelist2Detail
					);

					bool bFirst = true;
					bool bChanges = false;
					int BuildStatus = -1;
					foreach( Changelists2 Change in Changelists )
					{
						if( bFirst )
						{
							if( Change.BuildStatus.HasValue )
							{
								BuildStatus = Change.BuildStatus.Value;
								bFirst = false;
							}
						}
						else
						{
							IQueryable<Main.CISJobStateInfo> JobStates =
							(
								from CISJobStateDetail in Changelists2Data.CISJobStateAlts
								where CISJobStateDetail.ChangelistID == Change.ID
								join CISTaskDetail in Changelists2Data.CISTaskAlts on CISJobStateDetail.CISTaskID equals CISTaskDetail.ID
								select new Main.CISJobStateInfo( CISJobStateDetail.JobState, CISTaskDetail.Mask )
							);

							int Completed = -1;
							int OverallMask = 0;
							foreach( Main.CISJobStateInfo ActualJobState in JobStates )
							{
								OverallMask |= ActualJobState.Mask;

								switch( ( JobState )ActualJobState.CISJobState )
								{
								case JobState.Unneeded:
								case JobState.InProgressOptional:
									break;

								case JobState.Pending:
								case JobState.InProgress:
									Completed &= ~ActualJobState.Mask;
									break;

								case JobState.Optional:
								case JobState.Succeeded:
									BuildStatus |= ActualJobState.Mask;
									break;

								case JobState.Failed:
									BuildStatus &= ~ActualJobState.Mask;
									break;
								}
							}

							BuildStatus |= ~OverallMask;

							if( Completed == -1 )
							{
								Change.BuildStatus = BuildStatus;
								LastFullyKnown = Change.Changelist;
								bChanges = true;
							}
							else
							{
								break;
							}
						}
					}

					if( bChanges )
					{
						RetryLinqOperation( new LinqOperation( GenericChangelistUpdate ), Changelists2Data, "UpdateBuildState" );
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UpdateBuildState", Ex.Message );
				LastFullyKnown = -1;
			}

			return LastFullyKnown;
		}

		/// <summary>
		/// Insert a new changelist
		/// </summary>
		static public int InsertChangelist( Main.BranchDefinition BranchDef, BuildState.ChangeList Change )
		{
			try
			{
				using( Changelist2DataContext Changelists2Data = new Changelist2DataContext() )
				{
					Changelists2 NewChange = new Changelists2();

					NewChange.BranchConfigID = BranchDef.ID;
					NewChange.Changelist = Change.Number;
					NewChange.Submitter = Change.User;
					NewChange.TimeStamp = new DateTime( 1970, 1, 1 ).AddSeconds( Change.Time );
					NewChange.Description = Change.Description;

					Changelists2Data.Changelists2s.InsertOnSubmit( NewChange );
					RetryLinqOperation( new LinqOperation( GenericChangelistUpdate ), Changelists2Data, "InsertChangelist" );

					return NewChange.ID;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "InsertChangelist", Ex.Message );
			}

			return -1;
		}

		/// <summary>
		/// Get a generic string from the commands table
		/// </summary>
		static public string GetJobString( int JobID, string Field )
		{
			string Result = "";
			try
			{
				string Query = "SELECT " + Field + " FROM Jobs WHERE ( ID = " + JobID + " )";
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					IEnumerable<string> Results = ( IEnumerable<string> )JobsData.ExecuteQuery( typeof( string ), Query );
					Result = Results.FirstOrDefault();
					if( Result == null )
					{
						Result = "";
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetJobString", Ex.Message );
				Result = "";
			}

			return Result;
		}

		/// <summary>
		/// Find any jobs marked as InProgressOptional that have completed, and update their state
		/// </summary>
		static private void SelectInProgressOptionalJobs( Changelist2DataContext Changelist2Data, int BranchConfigID, string JobName, bool bSucceeded, JobState NewJobState )
		{
			bool bHasChanges = false;

			IQueryable<CISJobStateAlt> CISJobStates =
			(
				from Changelist2Detail in Changelist2Data.Changelists2s
				where Changelist2Detail.BranchConfigID == BranchConfigID
				join CISJobStateDetail in Changelist2Data.CISJobStateAlts on Changelist2Detail.ID equals CISJobStateDetail.ChangelistID
				where CISJobStateDetail.JobState == ( byte )JobState.InProgressOptional
				join CISTaskDetail in Changelist2Data.CISTaskAlts on CISJobStateDetail.CISTaskID equals CISTaskDetail.ID
				where CISTaskDetail.Name == JobName
				join JobDetail in Changelist2Data.JobAlts on Changelist2Detail.Changelist.ToString() equals JobDetail.Label
				where JobDetail.Complete && JobDetail.Succeeded == bSucceeded
				select CISJobStateDetail
			);

			foreach( CISJobStateAlt CISJobStateInfo in CISJobStates )
			{
				CISJobStateInfo.JobState = ( byte )NewJobState;
				bHasChanges = true;
			}

			// Apply any changes back to the database
			if( bHasChanges )
			{
				Changelist2Data.SubmitChanges();
			}
		}

		/// <summary>
		/// Cleanup any jobs that were in progress, but have now finished, but have been passed by LastFullyKnown processing
		/// </summary>
		static public void CleanupInProgressOptional( int BranchConfigID, string JobName )
		{
			try
			{
				// Get all the changelists with InProgressOptional jobs for this job type
				using( Changelist2DataContext Changelist2Data = new Changelist2DataContext() )
				{
					SelectInProgressOptionalJobs( Changelist2Data, BranchConfigID, JobName, true, JobState.Succeeded );
					SelectInProgressOptionalJobs( Changelist2Data, BranchConfigID, JobName, false, JobState.Failed );
				}
			}
			catch
			{
				// We can silently fail on a deadlock as there is no iterative state with the above queries
			}
		}

		/// <summary>
		/// Get a list of emails subscribed to a given build
		/// </summary>
		static public List<string> GetSubscribers( int CommandID, byte SubscriptionType )
		{
			try
			{
				using( SubscriptionsDataContext SubscriptionsData = new SubscriptionsDataContext() )
				{
					IQueryable<string> Subscribers =
					(
						from SubscriptionsDetail in SubscriptionsData.Subscriptions
						where SubscriptionsDetail.CommandID == CommandID
						where SubscriptionsDetail.Type == SubscriptionType
						select SubscriptionsDetail.Email
					);

					return Subscribers.ToList();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetSubscribers", Ex.Message );
			}

			return new List<string>();
		}

		/// <summary>
		/// Find any jobs that failed, but did not fail the build
		/// </summary>
		static public List<Main.SuppressedJobsInfo> FindSuppressedJobs( long JobSpawnTime )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					IQueryable<Main.SuppressedJobsInfo> SuppressedJobs =
					(
						from JobDetail in JobsData.Jobs
						where JobDetail.Suppressed && JobDetail.SpawnTime == JobSpawnTime
						select new Main.SuppressedJobsInfo( JobDetail.Platform, JobDetail.Game, JobDetail.Config )
					);

					return SuppressedJobs.ToList();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "FindSuppressedJobs", Ex.Message );
			}

			return new List<Main.SuppressedJobsInfo>();
		}

		/// <summary>
		/// Find the currently active CIS jobs
		/// </summary>
		static public List<Main.CISJobsInfo> GetActiveCISJobs( int LastFullyKnown, int BranchConfigID, string JobNameSuffix )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					IQueryable<Main.CISJobsInfo> CISJobs =
					(
						from JobDetail in JobsData.Jobs
						where !JobDetail.PrimaryBuild && !JobDetail.Killing
						where JobDetail.BranchConfigID == BranchConfigID
						where JobDetail.Name == "CIS Code Builder (" + JobNameSuffix + ")"
						where Convert.ToInt32( JobDetail.Label ) > LastFullyKnown
						orderby JobDetail.Label descending
						select new Main.CISJobsInfo( JobDetail.ID, JobDetail.CISJobStateID, JobDetail.Label, JobDetail.Active, JobDetail.Complete, JobDetail.Succeeded )
					);

					return CISJobs.ToList();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetActiveCISJobs", Ex.Message ); 
			}

			return new List<Main.CISJobsInfo>();
		}

		/// <summary>
		/// Find the number of completed jobs
		/// </summary>
		static public int GetCompletedJobsCount( long JobSpawnTime )
		{
			int Count = 0;
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					Count =
					(
						from JobDetail in JobsData.Jobs
						where JobDetail.Complete && JobDetail.SpawnTime == JobSpawnTime
						select JobDetail.ID
					).Count();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetCompletedJobsCount", Ex.Message );
				Count = 0;
			}

			return Count;
		}

		/// <summary>
		/// Find the number of succeeded jobs
		/// </summary>
		static public int GetSucceededJobsCount( long JobSpawnTime )
		{
			int Count = 0;

			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					Count =
					(
						from JobDetail in JobsData.Jobs
						where JobDetail.Succeeded && JobDetail.SpawnTime == JobSpawnTime
						select JobDetail.ID
					).Count();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetSucceededJobsCount", Ex.Message );
				Count = 0;
			}

			return Count;
		}

		/// <summary>
		/// Kill any jobs associate with a given label
		/// </summary>
		static public void KillAssociatedJobs( string LabelName )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string UpdateQuery = "UPDATE Jobs SET Killing = 1 WHERE ( Label = '" + LabelName + "' )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "KillAssociatedJobs", Ex.Message );
			}
		}

		/// <summary>
		/// Kill any jobs associate with a given spawn time
		/// </summary>
		static public void KillAssociatedJobs( long JobSpawnTime )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string UpdateQuery = "UPDATE Jobs SET Killing = 1 WHERE ( SpawnTime = " + JobSpawnTime.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "KillAssociatedJobs", Ex.Message );
			}
		}

		/// <summary>
		/// Mark all uncompleted orphaned jobs as complete and unsuccessful
		/// </summary>
		static public void CleanupJob( string MachineName )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string UpdateQuery = "UPDATE Jobs SET Complete = 1, Succeeded = 0 WHERE ( Machine = '" + MachineName + "' AND Complete = 0 )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "CleanupJob", Ex.Message );
			}
		}

		/// <summary>
		/// Mark a specific job as complete
		/// </summary>
		static public void MarkJobComplete( int JobID )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string UpdateQuery = "UPDATE Jobs SET Complete = 1 WHERE ( ID = " + JobID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "MarkJobComplete", Ex.Message );
			}
		}

		/// <summary>
		/// Mark a job succeeded
		/// </summary>
		static public void MarkJobSucceeded( int JobID, bool bHasSuppressedErrors )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string Suppressed = bHasSuppressedErrors ? "1" : "0";
					string UpdateQuery = "UPDATE Jobs SET Succeeded = 1, Suppressed = " + Suppressed + " WHERE ( ID = " + JobID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "MarkJobSucceeded", Ex.Message );
			}
		}

		/// <summary>
		/// Mark a job as not required for the overall build state
		/// </summary>
		static public void MarkJobsOptional( int JobID )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string UpdateQuery = "UPDATE Jobs SET Optional = 1 WHERE ( ID = " + JobID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "MarkJobsOptional", Ex.Message );
			}
		}

		/// <summary>
		/// Set the active information for a job
		/// </summary>
		static public void SetJobActive( int JobID, int BuildLogID, string MachineName )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					string UpdateQuery = "UPDATE Jobs SET BuildLogID = " + BuildLogID.ToString() + ", Machine = '" + MachineName + "' WHERE ( ID = " + JobID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericJobUpdate ), JobsData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "SetJobActive", Ex.Message );
			}
		}

		/// <summary>
		/// Add a new unique job
		/// </summary>
		static public void AddJob( string Name, string Command, string Platform, string Game, string BuildConfig,
								string ScriptConfiguration, string Language, string Define, string Parameter, bool Remote, int BranchConfigID, int CISTaskID, int JobStateID,
								string Dependency, bool PrimaryBuild, long JobSpawnTime )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					Job NewRecord = new Job();

					NewRecord.Name = Name;
					NewRecord.Command = Command;
					NewRecord.Platform = Platform;
					NewRecord.Game = Game;
					NewRecord.Config = BuildConfig;
					NewRecord.ScriptConfig = ScriptConfiguration;
					NewRecord.Language = Language;
					NewRecord.Define = Define;
					NewRecord.Parameter = Parameter;
					NewRecord.Remote = Remote;
					NewRecord.BranchConfigID = BranchConfigID;
					NewRecord.CISTaskID = CISTaskID;
					NewRecord.CISJobStateID = JobStateID;
					NewRecord.Label = Dependency;
					NewRecord.Machine = "";
					NewRecord.BuildLogID = 0;
					NewRecord.PrimaryBuild = PrimaryBuild;
					NewRecord.Active = false;
					NewRecord.Complete = false;
					NewRecord.Succeeded = false;
					NewRecord.Optional = false;
					NewRecord.Killing = false;
					NewRecord.Suppressed = false;
					NewRecord.SpawnTime = JobSpawnTime;

					JobsData.Jobs.InsertOnSubmit( NewRecord );
					RetryLinqOperation( new LinqOperation( SubmitJobChanges ), JobsData, "AddJob" );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "AddJob", Ex.Message );
			}
		}

		/// <summary>
		/// Check to see if any jobs are being killed
		/// </summary>
		static public int PollForKillJob( string MachineName )
		{
			int KilledJob = 0;

			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					KilledJob =
					(
						from JobDetail in JobsData.Jobs
						where JobDetail.Machine == MachineName && JobDetail.Killing && !JobDetail.Complete
						select JobDetail.ID
					).FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "PollForKillJob", Ex.Message );
				KilledJob = 0;
			}

			return KilledJob;
		}

		/// <summary>
		/// Add a new job state row for CIS
		/// </summary>
		static public int AddJobState( int ChangelistID, int CISTaskID, Main.JobState InitialState )
		{
			try
			{
				using( CISTaskDataContext CISJobStatesData = new CISTaskDataContext() )
				{
					CISJobState NewRecord = new CISJobState();

					NewRecord.ChangelistID = ChangelistID;
					NewRecord.CISTaskID = CISTaskID;
					NewRecord.JobState = ( byte )InitialState;
					NewRecord.Error = InitialState.ToString();

					CISJobStatesData.CISJobStates.InsertOnSubmit( NewRecord );
					RetryLinqOperation( new LinqOperation( AddJobState ), CISJobStatesData, "AddJobState" );

					return NewRecord.ID;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "AddJobState", Ex.Message );
			}

			return -1;
		}

		/// <summary>
		/// Update the job state for a given job in a given changelist
		/// </summary>
		static public void UpdateChangelist( int CISJobStateID, Main.JobState State, string Error )
		{
			try
			{
				if( CISJobStateID > 0 )
				{
					using( CISTaskDataContext CISTaskData = new CISTaskDataContext() )
					{
						string JobState = ( ( int )State ).ToString();
						string UpdateQuery;
						if( Error.Length > 0 )
						{
							Error = "'" + Error.Replace( "'", "" ) + "'";
							UpdateQuery = "UPDATE CISJobStates SET JobState = " + JobState + ", Error = " + Error + " WHERE ( ID = " + CISJobStateID.ToString() + " )";
						}
						else
						{
							UpdateQuery = "UPDATE CISJobStates SET JobState = " + JobState + " WHERE ( ID = " + CISJobStateID.ToString() + " )";
						}

						RetryLinqOperation( new LinqOperation( UpdateCISTask ), CISTaskData, UpdateQuery );
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UpdateChangelist", Ex.Message );
			}
		}

		/// <summary>
		/// Set the LastAttemped, LastGood, and LastFail fields for CISTasks
		/// </summary>
		static public void UpdateCISTask( Main.CISTaskInfo Task )
		{
			try
			{
				using( CISTaskDataContext CISTaskData = new CISTaskDataContext() )
				{
					string UpdateQuery = "UPDATE CISTasks SET LastAttempted = " + Task.LastAttempted + ", LastGood = " + Task.LastGood + ", LastFail = " + Task.LastFail + " WHERE ( ID = " + Task.ID + " )";
					RetryLinqOperation( new LinqOperation( UpdateCISTask ), CISTaskData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UpdateCISTask", Ex.Message );
			}
		}

		/// <summary>
		/// Get a single row from the CISTask table
		/// </summary>
		static public Main.CISTaskInfo GetCISTaskInfo( int CISTaskID )
		{
			try
			{
				using( CISTaskDataContext CISTaskData = new CISTaskDataContext() )
				{
					Main.CISTaskInfo CISTask =
					(
						from CISTaskDetail in CISTaskData.CISTasks
						where CISTaskDetail.ID == CISTaskID
						select new Main.CISTaskInfo( CISTaskDetail )
					).FirstOrDefault();

					return CISTask;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetCISTaskInfo", Ex.Message );
			}

			return null;
		}

		/// <summary>
		/// Get the list of potential CIS tasks for a given branch
		/// </summary>
		static public Dictionary<string, Main.CISTaskInfo> GetCISTasksForBranch( int BranchConfigID )
		{
			try
			{
				using( CISTaskDataContext CISTaskData = new CISTaskDataContext() )
				{
					IEnumerable<Main.CISTaskInfo> CISTasks =
					(
						from CISTaskDetail in CISTaskData.CISTasks
						where CISTaskDetail.BranchConfigID == BranchConfigID
						select new Main.CISTaskInfo( CISTaskDetail )
					);

					return CISTasks.ToDictionary( x => x.TestType, x => x );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetCISTasksForBranch", Ex.Message );
			}

			return new Dictionary<string, Main.CISTaskInfo>();
		}

		/// <summary>
		/// See if there are any builds that need to be triggered
		/// </summary>
		static public int PollForBuild( Main Parent, bool bPrimaryBuildsOnly )
		{
			int ID;

			// Check for machine locked timed or triggered build
			ID = CheckForBuild( Parent, false, bPrimaryBuildsOnly );
			if( ID >= 0 )
			{
				// Check for success
				if( ID != 0 )
				{
					return ID;
				}

				// If this machine locked to a build then don't allow it to grab normal ones
				if( Parent.MachineLock != 0 )
				{
					return 0;
				}

				// Poll for a timed or triggered build to be grabbed by anyone
				ID = CheckForBuild( Parent, true, bPrimaryBuildsOnly );
			}

			return ID;
		}

		/// <summary>
		/// Retrieve a variable for a given branch - typically, 'LatestBuild' or 'LatestApprovedBuild'
		/// </summary>
		static public string GetVariable( int BranchConfigID, string Var )
		{
			try
			{
				using( VariablesDataContext VariablesData = new VariablesDataContext() )
				{
					string Value =
					(
						from VariablesDetail in VariablesData.Variables
						where VariablesDetail.BranchConfigID == BranchConfigID && VariablesDetail.Variable1 == Var
						select VariablesDetail.Value
					).FirstOrDefault();

					return Value;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GetVariable", Ex.Message );
			}

			return "";
		}

		/// <summary>
		/// Update a variable for a given branch
		/// </summary>
		static public void UpdateVariable( int BranchConfigID, string Variable, string Value )
		{
			try
			{
				using( VariablesDataContext VariablesData = new VariablesDataContext() )
				{
					string UpdateQuery = "UPDATE Variables SET Value = '" + Value + "' WHERE ( Variable = '" + Variable + "' AND BranchConfigID = " + BranchConfigID.ToString() + " )";
					RetryLinqOperation( new LinqOperation( GenericVariableUpdate ), VariablesData, UpdateQuery );
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UpdateVariable", Ex.Message );
			}
		}

		/// <summary>
		/// Update a variable for a given branch
		/// </summary>
		static public int UpdateVersioning( string LabelName, int EngineVersion, int Changelist )
		{
			try
			{
				using( VersioningDataContext VersioningData = new VersioningDataContext() )
				{
					Versioning NewVersioning = new Versioning();

					NewVersioning.Label = LabelName;
					NewVersioning.EngineVersion = EngineVersion;
					NewVersioning.Changelist = Changelist;

					VersioningData.Versionings.InsertOnSubmit( NewVersioning );
					RetryLinqOperation( new LinqOperation( InsertVerioning ), VersioningData, "CreateVersioning" );

					return NewVersioning.ID;
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "UpdateVersioning", Ex.Message );
			}

			return -1;
		}

		/// <summary>
		/// Select the correct client depending on whether the task is remote or not
		/// </summary>
		static private void SelectClient( bool RemoteTask, Main.BranchDefinition BranchDef )
		{
			BranchDef.CurrentClient = null;
			foreach( Main.ClientInfo Client in BranchDef.Clients )
			{
				if( RemoteTask && Client.bIsRemoteClient )
				{
					BranchDef.CurrentClient = Client;
					break;
				}

				if( !RemoteTask && !Client.bIsRemoteClient )
				{
					BranchDef.CurrentClient = Client;
					break;
				}
			}
		}

		/// <summary>
		/// Check to see if the platform SDK is correct
		/// </summary>
		static private bool CheckPlatformSDK( string Platform, string PlatformArg, string LocalSDK, string BranchSDK )
		{
			// Branch does not care about this platform - all good
			if( BranchSDK.ToLower() == "none" )
			{
				return true;
			}

			// If SDKs match, then we're good anyway
			if( LocalSDK.ToLower() == BranchSDK.ToLower() )
			{
				return true;
			}

			// If we're WiiU, and building for the WiiU, and the SDKs don't match (above) - that's no good
			if( Platform.ToLower() == PlatformArg.ToLower() )
			{
				return false;
			}

			// If no platform argument, presume building for multiple platforms, which requires all SDKs to match
			if( PlatformArg.ToLower().Length == 0 )
			{
				return false;
			}

			// PC targets compile in SDK specific elements (for the editor), which requires all SDKs to match (include pcserver and pcconsole variants)
			if( PlatformArg.ToLower().StartsWith( "win32" ) || PlatformArg.ToLower().StartsWith( "win64" ) || PlatformArg.ToLower().StartsWith( "pc" ) )
			{
				return false;
			}

			return true;
		}

		static private bool CheckPlatformSDKs( Main Parent, Main.BranchDefinition BranchDef, Command Triggered )
		{
			// Make sure the NGP SDK is valid
			if( !CheckPlatformSDK( "NGP", Triggered.Platform, Parent.NGPSDKVersion, BranchDef.NGPSDKVersion ) )
			{
				return false;
			}

			// Make sure the PS3 SDK is valid
			if( !CheckPlatformSDK( "PS3", Triggered.Platform, Parent.PS3SDKVersion, BranchDef.PS3SDKVersion ) )
			{
				return false;
			}	

			// Make sure the XDK is valid
			if( !CheckPlatformSDK( "Xbox360", Triggered.Platform, Parent.Xbox360SDKVersion, BranchDef.Xbox360SDKVersion ) )
			{
				return false;
			}
#if false
			// Make sure the Android SDK is valid
			if( !CheckPlatformSDK( "Android", Triggered.Platform, Parent.AndroidSDKVersion, BranchDef.AndroidSDKVersion ) )
			{
				return false;
			}
	
			// Make sure the iPhone SDK is valid
			if( !CheckPlatformSDK( "iPhone", Triggered.Platform, Parent.iPhoneSDKVersion, BranchDef.iPhoneSDKVersion ) )
			{
				return false;
			}

			// Make sure the Mac SDK is valid
			if( !CheckPlatformSDK( "Mac", Triggered.Platform, Parent.MacSDKVersion, BranchDef.MacSDKVersion ) )
			{
				return false;
			}

			// Make sure the WiiU SDK is valid
			if( !CheckPlatformSDK( "WiiU", Triggered.Platform, Parent.WiiUSDKVersion, BranchDef.WiiUSDKVersion ) )
			{
				return false;
			}

			// Make sure the Flash SDK is valid
			if( !CheckPlatformSDK( "Flash", Triggered.Platform, Parent.FlashSDKVersion, BranchDef.FlashSDKVersion ) )
			{
				return false;
			}
#endif
			return true;
		}

		/// <summary>
		/// Check for a timed or triggered build
		/// </summary>
		static public int CheckForBuild( Main Parent, bool bAnyMachine, bool bPrimaryBuild )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					// Basic query to get a potential trigger
					IEnumerable<Command> CheckBuild = (
						from CommandDetail in CommandsData.Commands
						where ( CommandDetail.NextTrigger != null && DateTime.Now > CommandDetail.NextTrigger ) || CommandDetail.Pending
						where CommandDetail.PrimaryBuild == bPrimaryBuild
						where ( CommandDetail.MachineLock == "None" && bAnyMachine ) || ( CommandDetail.MachineLock.ToLower() == Parent.MachineName.ToLower() )
						select CommandDetail
					);

					foreach( Command Triggered in CheckBuild )
					{
						// If we have a potential match, filter against the local data
						if( Triggered == null )
						{
							continue;
						}

						// Check to make sure we have the branch
						Main.BranchDefinition BranchDef = Parent.FindBranchDefinition( Triggered.BranchConfigID );
						if( BranchDef == null )
						{
							continue;
						}

						// Select the correct client
						SelectClient( Triggered.Remote, BranchDef );
						if( BranchDef.CurrentClient == null )
						{
							continue;
						}

						// Check to make sure we have all the relevant platform SDKs installed
						if( !CheckPlatformSDKs( Parent, BranchDef, Triggered ) )
						{
							continue;
						}

						// If this build is promotable
						if( Triggered.IsPromotable )
						{
							// ... make sure there are no other promotable builds from the same branch already in progress
							int PromotableBuildCount =
							(
								from CommandDetail in CommandsData.Commands
								where CommandDetail.Machine.Length > 0 && CommandDetail.IsPromotable && CommandDetail.BranchConfigID == Triggered.BranchConfigID
								select CommandDetail.ID
							).Count();

							if( PromotableBuildCount > 0 )
							{
								continue;
							}
						}

						// Update the time for the next trigger
						if( Triggered.NextTriggerDelay.HasValue && Triggered.NextTrigger.HasValue && !Triggered.Pending )
						{
							if( Triggered.NextTriggerDelay.Value <= 30 )
							{
								// If this is a frequent update, don't run multiple times to catch up
								Triggered.NextTrigger = DateTime.Now.AddMinutes( Triggered.NextTriggerDelay.Value );
							}
							else
							{
								// If this is a less frequent update, make sure the build triggers at the same time every day, or on the hour
								Triggered.NextTrigger = Triggered.NextTrigger.Value.AddMinutes( Triggered.NextTriggerDelay.Value );
							}

							Triggered.Operator = "AutoTimer";
						}

						// Always clear as we can always manually trigger a build that is normally timed
						Triggered.Pending = false;

						try
						{
							CommandsData.SubmitChanges();
							return Triggered.ID;
						}
						catch
						{
							// Another machine has already grabbed the task; return no task to run
						}
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "CheckForBuild", Ex.Message );
			}

			return 0;
		}

		static private bool CheckPlatformSDKs( Main Parent, Main.BranchDefinition BranchDef, Job Triggered )
		{
			// Make sure the NGP SDK is valid
			if( !CheckPlatformSDK( "NGP", Triggered.Platform, Parent.NGPSDKVersion, BranchDef.NGPSDKVersion ) )
			{
				return false;
			}

			// Make sure the PS3 SDK is valid
			if( !CheckPlatformSDK( "PS3", Triggered.Platform, Parent.PS3SDKVersion, BranchDef.PS3SDKVersion ) )
			{
				return false;
			}

			// Make sure the XDK is valid
			if( !CheckPlatformSDK( "Xbox360", Triggered.Platform, Parent.Xbox360SDKVersion, BranchDef.Xbox360SDKVersion ) )
			{
				return false;
			}
#if false
			// Make sure the Android SDK is valid
			if( !CheckPlatformSDK( "Android", Triggered.Platform, Parent.AndroidSDKVersion, BranchDef.AndroidSDKVersion ) )
			{
				return false;
			}

			// Make sure the iPhone SDK is valid
			if( !CheckPlatformSDK( "iPhone", Triggered.Platform, Parent.iPhoneSDKVersion, BranchDef.iPhoneSDKVersion ) )
			{
				return false;
			}

			// Make sure the Mac SDK is valid
			if( !CheckPlatformSDK( "Mac", Triggered.Platform, Parent.MacSDKVersion, BranchDef.MacSDKVersion ) )
			{
				return false;
			}

			// Make sure the WiiU SDK is valid
			if( !CheckPlatformSDK( "WiiU", Triggered.Platform, Parent.WiiUSDKVersion, BranchDef.WiiUSDKVersion ) )
			{
				return false;
			}

			// Make sure the Flash SDK is valid
			if( !CheckPlatformSDK( "Flash", Triggered.Platform, Parent.FlashSDKVersion, BranchDef.FlashSDKVersion ) )
			{
				return false;
			}
#endif
			return true;
		}

		/// <summary>
		/// Check for any available jobs
		/// </summary>
		static public int CheckForJob( Main Parent, bool bPrimaryBuild )
		{
			try
			{
				using( JobsDataContext JobsData = new JobsDataContext() )
				{
					// Basic query to get job candidates
					IEnumerable<Job> CheckJob =
					(
						from JobDetail in JobsData.Jobs
						where !JobDetail.Active && !JobDetail.Complete && !JobDetail.Killing && !JobDetail.Optional
						where JobDetail.PrimaryBuild == bPrimaryBuild
						orderby JobDetail.Label descending
						select JobDetail
					);

					foreach( Job Triggered in CheckJob )
					{
						if( Triggered == null )
						{
							continue;
						}

						// Check to make sure we have the branch
						Main.BranchDefinition BranchDef = Parent.FindBranchDefinition( Triggered.BranchConfigID );
						if( BranchDef == null )
						{
							continue;
						}

						// Select the correct client
						SelectClient( Triggered.Remote, BranchDef );
						if( BranchDef.CurrentClient == null )
						{
							continue;
						}

						// Check to make sure we have all the relevant platform SDKs installed
						if( !CheckPlatformSDKs( Parent, BranchDef, Triggered ) )
						{
							continue;
						}

						// Generate an update to remove the chance of race conditions
						string UpdateQuery = "UPDATE Jobs SET Active = 1 OUTPUT INSERTED.ID WHERE ( Active = 0 AND ID = " + Triggered.ID + " )";
						try
						{
							IEnumerable<int> Results = ( IEnumerable<int> )JobsData.ExecuteQuery( typeof( int ), UpdateQuery );
							return Results.FirstOrDefault();
						}
						catch
						{
							// Another machine has already grabbed the task; return no task to run
						}
					}
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "CheckForJob", Ex.Message );
			}

			return 0;
		}

		/// <summary>
		/// Write a packet of performance data to the database
		/// </summary>
		static public void WritePerformanceData( string MachineName, string BranchName, string KeyName, long Value, int Changelist )
		{
			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					CommandsData.CreatePerformanceData2( KeyName, MachineName, BranchName, Value, Changelist, DateTime.UtcNow );
				}
			}
			catch
			{
				// It's OK to miss the occasional performance data sample
			}
		}

		/// <summary>
		/// Call a generic named stored procedure
		/// </summary>
		static public T GenericStoredProcedure<T>( string StoredProcedure ) where T : new()
		{
			T Result = new T();

			try
			{
				using( CommandsDataContext CommandsData = new CommandsDataContext() )
				{
					IEnumerable<T> Results = ( IEnumerable<T> )CommandsData.ExecuteQuery( typeof( T ), StoredProcedure );
					Result = Results.FirstOrDefault();
				}
			}
			catch( Exception Ex )
			{
				Parent.SendErrorMail( "GenericStoredProcedure : " + StoredProcedure, Ex.Message );
			}

			return Result;
		}
	}
}

