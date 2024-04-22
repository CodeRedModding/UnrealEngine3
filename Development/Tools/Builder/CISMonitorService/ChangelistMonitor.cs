// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

using Builder.CISMonitor.Data;

namespace Builder.CISMonitor
{
	public class ChangelistInfo
	{
		// The id of the change
		public int ChangeNumber = 0;

		// The overall state of the build for this changelist
		public int BuildStatus = 0;

		// The Perforce user that checked in this change
		public string User = "";
		// The Perforce description of this change
		public string Description = "";
		// The Perforce timestamp of the changelist
		public DateTime TimeStamp = DateTime.UtcNow;

		public ChangelistInfo( int InChangeNumber, int? InBuildStatus, string InUser, string InDescription, DateTime InTimeStamp )
		{
			ChangeNumber = InChangeNumber;

			if( InBuildStatus.HasValue )
			{
				BuildStatus = InBuildStatus.Value;
			}

			User = InUser;
			Description = InDescription;
			TimeStamp = InTimeStamp;
		}
	}

	public class ChangelistMonitor
	{
		private class BranchBuildState
		{
			public Dictionary<string, int> UserBuildStates = new Dictionary<string, int>();

			public BranchBuildState( Dictionary<string, int> NewUserBuildStates )
			{
				UserBuildStates = NewUserBuildStates;
			}
		}

		private Thread MonitorThread = null;
		private DateTime LastInterrogation = DateTime.Now;
		private List<string> CISBranches = new List<string>();
		private Dictionary<string, int> OverallBuildStates = new Dictionary<string, int>();
		private Dictionary<string, BranchBuildState> BranchBuildStates = new Dictionary<string, BranchBuildState>();

		public ChangelistMonitor()
		{
			MonitorThread = new Thread( new ThreadStart( MonitorProc ) );
			MonitorThread.Start();
		}

		public void Release()
		{
			if( MonitorThread != null )
			{
				MonitorThread.Abort();
				MonitorThread = null;
			}
		}

		public string GetInfo()
		{
			return LastInterrogation.ToLongTimeString();
		}

		public string GetCISBranches()
		{
			string Branches = "";

			lock( CISBranches )
			{
				foreach( string Branch in CISBranches )
				{
					Branches += Branch + "/";
				}
			}

			return Branches;
		}

		public string GetBuildState( string Branch, string User )
		{
			// Get the overall state of the build
			int OverallState = 0;
			int UserState = 0;

			lock( OverallBuildStates )
			{
				OverallBuildStates.TryGetValue( Branch, out OverallState );
			}

			// Get the state of the build according to the user
			lock( BranchBuildStates )
			{
				BranchBuildState BranchState = null;
				if( BranchBuildStates.TryGetValue( Branch, out BranchState ) )
				{
					if( !BranchState.UserBuildStates.TryGetValue( User.ToLower(), out UserState ) )
					{
						UserState = -1;
					}
				}
			}

			string Result = "";
			if( OverallState == 0 )
			{
				Result += "Unknown";
			}
			else if( OverallState != -1 )
			{
				Result += "Bad";
			}
			else
			{
				Result += "Good";
			}

			Result += "/";

			if( UserState == 0 )
			{
				Result += "Unknown";
			}
			else if( UserState != -1 )
			{
				Result += "Bad";
			}
			else
			{
				Result += "Good";
			}

			return Result;
		}

		private void EvaluateUserStates( string Branch, List<ChangelistInfo> Changes )
		{
			Dictionary<string, int> NewUserBuildStates = new Dictionary<string, int>();

			int OverallState = 0;
			int CurrentBuildState = 0;
			foreach( ChangelistInfo Change in Changes )
			{
				// Get first good or bad (but known) result
				if( Change.BuildStatus != 0 && OverallState == 0 )
				{
					OverallState = Change.BuildStatus;
					CurrentBuildState = Change.BuildStatus;
				}

				// If the build has ever been good, any changes before that (later in the list) are in the clear
				if( Change.BuildStatus == -1 )
				{
					CurrentBuildState = -1;
				}

				// Get the user's first unknown, good or bad result
				int UserState = -1;
				if( !NewUserBuildStates.TryGetValue( Change.User.ToLower(), out UserState ) )
				{
					NewUserBuildStates.Add( Change.User.ToLower(), CurrentBuildState );
				}
			}

			lock( OverallBuildStates )
			{
				OverallBuildStates[Branch] = OverallState;
			}

			lock( BranchBuildStates )
			{
				BranchBuildState BranchState = null;
				if( !BranchBuildStates.TryGetValue( Branch, out BranchState ) )
				{
					BranchState = new BranchBuildState( NewUserBuildStates );
					BranchBuildStates.Add( Branch, BranchState );
				}
				else
				{
					BranchState.UserBuildStates = NewUserBuildStates;
				}
			}
		}

		private void MonitorProc()
		{
			while( true )
			{
#if !DEBUG
				try
				{
#endif
					LastInterrogation = DateTime.Now;

					// Create a link to the database
					Database Data = new Database();

					List<string> Branches = Data.GetCISBranches();
					lock( CISBranches )
					{
						CISBranches = new List<string>( Branches );
					}

					foreach( string Branch in Branches )
					{
						// Get the list of unique changes 
						List<ChangelistInfo> Changes = Data.GetChanges( Branch );

						if( Changes.Count > 0 )
						{
							// Create a dictionary of the good, bad and unknown
							EvaluateUserStates( Branch, Changes );
						}
					}

					// Make sure everything is refreshed every time
					Data.Dispose();

					// Small pause to give the system room to breathe
					Thread.Sleep( 20 * 1000 );
#if !DEBUG
				}
				catch
				{
				}
#endif
			}
		}
	}
}
