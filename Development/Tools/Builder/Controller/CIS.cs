// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

using Controller.Models;

namespace Controller
{
	public partial class Main : Form
	{
		public enum JobState
		{
			Unneeded = 0,
			Pending,
			Optional,
			InProgress,
			InProgressOptional,
			Succeeded,
			Failed
		}

		public class CISTaskInfo
		{
			public int ID;
			public int BranchConfigID;
			public string TestType;
			public int LastAttempted;
			public int LastGood;
			public int LastFail;

			public bool GameSpecific;
			public bool PlatformSpecific;
			public bool Remote;
			public bool CompileAllowed;
			public string[] Folders;
			public string[] DependentPlatforms;
			public string[] AlternateFolders;

			public bool CompileTaskRequired;
			public bool JobSpawned;

			public CISTaskInfo( CISTask InCISTask )
			{
				ID = InCISTask.ID;
				BranchConfigID = InCISTask.BranchConfigID;
				TestType = InCISTask.Name;
				LastAttempted = InCISTask.LastAttempted;
				LastGood = InCISTask.LastGood;
				LastFail = InCISTask.LastFail;

				GameSpecific = InCISTask.GameSpecific;
				PlatformSpecific = InCISTask.PlatformSpecific;
				Remote = InCISTask.Remote;
				CompileAllowed = InCISTask.CompileAllowed;
				Folders = InCISTask.Folders.Split( ";".ToCharArray() );
				DependentPlatforms = InCISTask.DependentPlatforms.Split( ";".ToCharArray() );
				AlternateFolders = InCISTask.AlternateFolders.Split( ";".ToCharArray() );

				CompileTaskRequired = false;
				JobSpawned = false;

				ValidateFolder();
			}

			public string GetTestType()
			{
				return TestType;
			}

			public void CompileRequired()
			{
				CompileTaskRequired = true;
			}

			public bool CheckScript( BuildState Builder, string FileName )
			{
				if( GameSpecific )
				{
					if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/" + TestType ) )
					{
						CompileRequired();
						return true;
					}
				}

				return false;
			}

			public bool IsToolsJob( string FileName, string FolderToCheck )
			{
				string SubFolder = FileName.Substring( FolderToCheck.Length );
				if( !SubFolder.Contains( "Tools/" ) )
				{
					CompileRequired();
					return false;
				}

				return true;
			}

			// If the folders don't exist, suppress this CIS task
			public void ValidateFolder()
			{
				if( LastAttempted > 0 && Folders != null )
				{
					foreach( string Folder in Folders )
					{
						if( !Directory.Exists( Path.Combine( Environment.CurrentDirectory, Folder ) ) )
						{
							LastAttempted = -1;
						}
					}
				}
			}

			public bool JobsToSpawn()
			{
				if( LastAttempted > 0 )
				{
					if( CompileTaskRequired && CompileAllowed )
					{
						return true;
					}
				}

				return false;
			}

			public bool SpawnJobs( Main Parent, BuildState Builder, BuildState.ChangeList Changelist, int NewChangeID, long JobSpawnTime )
			{
				if( LastAttempted > 0 )
				{
					if( CompileTaskRequired && CompileAllowed )
					{
						// Add in job state for this job
						int CISJobStateID = BuilderLinq.AddJobState( NewChangeID, ID, Main.JobState.Pending );

						// Add in the job
						string Game = GameSpecific ? TestType : "All";
						string Platform = PlatformSpecific ? TestType : "Win32";

						string CommandConfiguration = Builder.BranchDef.Version >= 10 ? "Development" : "Release";
						BuilderLinq.AddJob(   "CIS Code Builder (" + TestType + ")",	// Name
											"Jobs/CISCodeBuilder" + TestType,	// Command
											Platform,							// Platform
											Game,								// Game
											CommandConfiguration,				// BuildConfig
											"",									// ScriptConfiguration
											"",									// Language
											"",									// Define
											"",									// Parameter
											Remote,								// Whether to use the remote clientspec
											Builder.BranchDef.ID,				// BranchIndex
											ID,
											CISJobStateID,						// ID of the Job State for this job
											Changelist.Number.ToString(),		// Dependency
											false,								// PrimaryBuild
											JobSpawnTime );						// SpawnTime

						CompileTaskRequired = false;
						JobSpawned = true;
					}
					else
					{
						BuilderLinq.AddJobState( NewChangeID, ID, Main.JobState.Unneeded );
					}
				}

				return JobSpawned;
			}
		}

		private void ProcessP4Changes()
		{
#if false
			// Builder.BranchDef.Branch = "UnrealEngine3";
			Builder.BranchDef.Branch = "UnrealEngine3";
			Builder.BranchDef.Version = 1;
			Builder.BranchDef.ID = 1;
#endif
			Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CISProcessP4Changes ), false );

			// Get the range of changes and determine if there's any work to do
			int CISLastChangelistProcessed = BuilderLinq.GetIntFromBranch( Builder.BranchDef.ID, "LastAttemptedOverall" );

			// If there was an error of some kind, just break out of this command
			if( CISLastChangelistProcessed <= 0 )
			{
				return;
			}

			// Define a null range of changes that doesn't update the DB as the default
			int CISFirstChangelistToProcess = CISLastChangelistProcessed + 1;
			int CISLastChangelistToProcess = CISLastChangelistProcessed;
			bool UpdateLatestProcessedChangelist = false;

			// Commonly used value
			int LatestChangelist = SCC.GetLatestChangelist( Builder, "//" + Builder.BranchDef.CurrentClient.ClientName + "/" + Builder.BranchDef.Branch + "/..." );

			// First, see whether we are building a single, non-DB-updating changelist
			if( Builder.GetCurrentCommandLine().ToLower().StartsWith( "changelist" ) )
			{
				// Determine if the changelist is specified or simply #head
				string ChangelistToBuildFrom = Builder.GetCurrentCommandLine().Substring( "changelist".Length ).Trim();
				if( ChangelistToBuildFrom.ToLower().CompareTo( "head" ) == 0 )
				{
					// Set these values equal to one another to ensure a build
					CISLastChangelistToProcess = LatestChangelist;
					CISFirstChangelistToProcess = CISLastChangelistToProcess;
				}
				else
				{
					// Set these values equal to one another to ensure a build
					CISFirstChangelistToProcess = Builder.SafeStringToInt( ChangelistToBuildFrom );
					CISLastChangelistToProcess = CISFirstChangelistToProcess;
				}

				Builder.Write( "Building only changelist " + CISFirstChangelistToProcess.ToString() + " and not updating CIS Monitor values" );
			}
			else
			{
				// By default, process every single changelist and update the DB accordingly
				CISLastChangelistToProcess = LatestChangelist;
				CISFirstChangelistToProcess = CISLastChangelistProcessed + 1;
				UpdateLatestProcessedChangelist = true;

				Builder.Write( "Building all out-of-date changelists" );
			}

			// Process any changelists as defined by a valid range set above
			if( CISLastChangelistToProcess >= CISFirstChangelistToProcess )
			{
				SpawnCISTasks( CISFirstChangelistToProcess, CISLastChangelistToProcess, UpdateLatestProcessedChangelist );
			}
			else
			{
				Builder.Write( "No new changes have been checked in!" );
			}

			// Done
			Builder.CloseLog();
		}

		private void UpdateMonitorValues()
		{
#if false
			// Builder.BranchDef.Branch = "UnrealEngine3";
			Builder.BranchDef.Branch = "UE4";
			Builder.BranchDef.Version = 10;
			Builder.BranchDef.ID = 184;
#endif
			Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CISUpdateMonitorValues ), false );

			// Compute a minimum of the overall good builds, and a maximum of the overall failed builds
			int LastGood_Overall = Int32.MaxValue;
			int LastFailed_Overall = 0;

			bool BuildIsGood_Overall = true;
			bool SuccessChanged_Overall = false;
			int SuccessChanger_Overall = 0;

			bool BuildIsGood = false;
			bool SuccessChanged = false;
			int SuccessChanger = 0;

			// Interrogate the database for CIS tasks associated with this branch
			Dictionary<string, CISTaskInfo> CISTasks = BuilderLinq.GetCISTasksForBranch( Builder.BranchDef.ID );

			foreach( CISTaskInfo Task in CISTasks.Values )
			{
				BuildIsGood = UpdateLastGoodAndFailedValues( Task, out SuccessChanged, out SuccessChanger );

				// LastGood and/or LastFailed are -1 if CIS is disabled
				if( Task.LastGood >= 0 && Task.LastFail >= 0 )
				{
					if( BuildIsGood && SuccessChanged )
					{
						SuccessChanged_Overall = true;
						SuccessChanger_Overall = ( SuccessChanger_Overall > SuccessChanger ) ? SuccessChanger_Overall : SuccessChanger;
					}

					BuildIsGood_Overall &= BuildIsGood;
					LastGood_Overall = ( Task.LastGood < LastGood_Overall ) ? Task.LastGood : LastGood_Overall;
					LastFailed_Overall = ( Task.LastFail > LastFailed_Overall ) ? Task.LastFail : LastFailed_Overall;
				}
			}

			// Update the summary variables in the DB
			Log( "Overall LastGood   = " + LastGood_Overall, Color.Green );
			Log( "Overall LastFailed = " + LastFailed_Overall, Color.Green );

			if( LastGood_Overall > 0 && LastFailed_Overall > 0 )
			{
				int OldLastGood_Overall = BuilderLinq.GetIntFromBranch( Builder.BranchDef.ID, "LastGoodOverall" );
				int OldLastFail_Overall = BuilderLinq.GetIntFromBranch( Builder.BranchDef.ID, "LastFailOverall" );

				if( BuildIsGood_Overall && SuccessChanged_Overall )
				{
					Mailer.SendCISMail( Builder, "CIS (" + Builder.BranchDef.Branch + ")", OldLastGood_Overall.ToString(), SuccessChanger_Overall.ToString() );
				}

				BuilderLinq.UpdateBranchConfigInt( Builder.BranchDef.ID, "LastGoodOverall", LastGood_Overall );
				BuilderLinq.UpdateBranchConfigInt( Builder.BranchDef.ID, "LastFailOverall", LastFailed_Overall );

				// Update the overall build state
				int LastFullyKnownChange = BuilderLinq.UpdateBuildState( Builder.BranchDef.ID );

				if( LastFullyKnownChange > 0 )
				{
					// Update the LastFullyKnown change
					BuilderLinq.UpdateBranchConfigInt( Builder.BranchDef.ID, "LastFullyKnown", LastFullyKnownChange );

					Log( "Updated LastFullyKnownChange to " + LastFullyKnownChange.ToString() + " for branch ID + " + Builder.BranchDef.ID.ToString(), Color.Green );
				}
			}
			else
			{
				Log( "CIS disabled; database not updated", Color.Green );
			}

			// Done
			Builder.CloseLog();
		}

		private void CompileAllGames( Dictionary<string, Main.CISTaskInfo> CISTasks )
		{
			foreach( CISTaskInfo Task in CISTasks.Values )
			{
				if( Task.GameSpecific )
				{
					Task.CompileRequired();
				}
			}
		}

		private void CompileAllPlatforms( Dictionary<string, Main.CISTaskInfo> CISTasks )
		{
			foreach( CISTaskInfo Task in CISTasks.Values )
			{
				if( Task.PlatformSpecific )
				{
					Task.CompileRequired();
				}
			}
		}

		private void CompileAll( Dictionary<string, Main.CISTaskInfo> CISTasks )
		{
			foreach( CISTaskInfo Task in CISTasks.Values )
			{
				Task.CompileRequired();
			}
		}

		private void SpawnCISTasks( int CISFirstChangelistToProcess, int CISLastChangelistToProcess, bool UpdateLatestProcessedChangelist )
		{
			Builder.Write( "Processing changelists " + CISFirstChangelistToProcess.ToString() + " through " + CISLastChangelistToProcess.ToString() );

			// For each changelist in the range, get the description and determine what types of builds are needed (this call deposits the changes in the ChangeLists instance variable of the Builder)
			SCC.GetChangesInRange( Builder, "//" + Builder.BranchDef.CurrentClient.ClientName + "/" + Builder.BranchDef.Branch + "/...", CISFirstChangelistToProcess.ToString(), CISLastChangelistToProcess.ToString() );

			if( Builder.ChangeLists.Count > 0 )
			{
				// Extract data common to all CIS tasks for this changelist
				long JobSpawnTime = DateTime.UtcNow.Ticks;

				// Interrogate the database for CIS tasks associated with this branch
				Dictionary<string, CISTaskInfo> CISTasks = BuilderLinq.GetCISTasksForBranch( Builder.BranchDef.ID );

				// Then interrogate the (properly ordered) changelists for which builds to spawn
				Builder.ChangeLists.Reverse();
				foreach( BuildState.ChangeList CL in Builder.ChangeLists )
				{
					Builder.Write( "    Processing changelist " + CL.Number.ToString() );

					// Make sure there are no stale JobSpawned entries
					foreach( CISTaskInfo Task in CISTasks.Values )
					{
						Task.JobSpawned = false;
					}

					bool bJobsSpawned = false;
					bool bAddToolsJob = false;

					if( Builder.BranchDef.Version >= 10 )
					{
						foreach( string FileName in CL.Files )
						{
							string LowerFileName = FileName.ToLower();

							if( LowerFileName.EndsWith( ".upk" ) )
							{
								// Don't do anything for packages
								continue;
							}
							else if( LowerFileName.EndsWith( ".umap" ) )
							{
								// Don't do anything for maps
								continue;
							}
							else if( LowerFileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch.ToLower() + "/engine/source/" )
									|| LowerFileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch.ToLower() + "/emptygame/source/" )
									|| LowerFileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch.ToLower() + "/examplegame/source/" )
									|| LowerFileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch.ToLower() + "/fortnitegame/source/" )
									|| LowerFileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch.ToLower() + "/pearlgame/source/" )
									|| LowerFileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch.ToLower() + "/udkgame/source/" ) )
							{
								CompileAll( CISTasks );
							}
						}
					}
					else
					{
						// Evaluate the changelist for jobs, trying to early-out in a few ways
						foreach( string FileName in CL.Files )
						{
							bool bFileHandled = false;
							Builder.Write( "        Processing File " + FileName );

							if( FileName.EndsWith( ".build" ) || FileName.EndsWith( ".brick" ) )
							{
								CompileAll( CISTasks );
								bFileHandled = true;
							}

							if( FileName.EndsWith( ".uc" ) || FileName.EndsWith( ".uci" ) )
							{
								// In any case, kick off the job that checks for script files in projects
								bAddToolsJob = true;
								bFileHandled = true;

								// Check if this script is game specific
								bool ScriptRequired = false;
								foreach( CISTaskInfo Task in CISTasks.Values )
								{
									ScriptRequired |= Task.CheckScript( Builder, FileName );
								}

								// If the script doesn't specifically reference a game, spawn script compilation for all games
								if( !ScriptRequired )
								{
									CompileAllGames( CISTasks );
								}
							}

							if( !bFileHandled && FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/" ) &&
									 ( FileName.EndsWith( ".vcxproj" ) || FileName.EndsWith( ".vcxproj.filters" ) ) )
							{
								// The tools CIS task does the vcxproj validation
								bAddToolsJob = true;
								bFileHandled = true;
							}

							if( !bFileHandled && FileName.EndsWith( ".ini" ) )
							{
								// Look further
								if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Engine/Config/Base" ) )
								{
									CompileAllGames( CISTasks );
									bFileHandled = true;
								}
								else
								{
									foreach( CISTaskInfo Task in CISTasks.Values )
									{
										if( Task.GameSpecific )
										{
											if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/" + Task.TestType + "Game/Config/Default" ) )
											{
												Task.CompileRequired();
												bFileHandled = true;
											}
										}
									}
								}

								// Ignore platform specific inis for now
							}

							if( !bFileHandled && ( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/" ) 
								|| FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Tools/" ) ) )
							{
								// Windows folder is a special case
								if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/Windows/" ) )
								{
									if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/Windows/WindowsTools" ) )
									{
										bAddToolsJob = true;
									}
									else
									{
										CompileAllGames( CISTasks );
									}

									bFileHandled = true;
								}

								// Tools folder is a special case
								if( !bFileHandled )
								{
									if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Tools/" ) )
									{
										if( !FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Tools/UnrealSwarm/Agent/" ) )
										{
											bAddToolsJob = true;
										}
										else
										{
											CompileAllGames( CISTasks );
										}

										bFileHandled = true;
									}
								}

								if( !bFileHandled )
								{
									foreach( CISTaskInfo Task in CISTasks.Values )
									{
										// Check to see if game specific code is updated
										if( Task.GameSpecific )
										{
											string FolderToCheck =  "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/" + Task.TestType;
											if( FileName.StartsWith( FolderToCheck ) )
											{
												Task.CompileRequired();

												// Add in all the platforms this game compiles for
												foreach( string DependentPlatform in Task.DependentPlatforms )
												{
													try
													{
														if( CISTasks[DependentPlatform] != null )
														{
															CISTasks[DependentPlatform].CompileRequired();
														}
													}
													catch( Exception Ex )
													{
														SendWarningMail( "Dependent Platform: " + DependentPlatform, Ex.Message, false );
													}
												}

												bFileHandled = true;
											}
										}

										// Check to see if any platform specific code is updated
										if( Task.PlatformSpecific )
										{
											string FolderToCheck = "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/" + Task.TestType + "/";
											if( FileName.StartsWith( FolderToCheck ) )
											{
												bAddToolsJob |= Task.IsToolsJob( FileName, FolderToCheck );
												bFileHandled = true;
											}

											foreach( string AlternateFolder in Task.AlternateFolders )
											{
												FolderToCheck = "    ... //depot/" + Builder.BranchDef.Branch + "/Development/Src/" + AlternateFolder + "/";
												if( FileName.StartsWith( FolderToCheck ) )
												{
													bAddToolsJob |= Task.IsToolsJob( FileName, FolderToCheck );
													bFileHandled = true;
												}
											}
										}
									}
								}

								if( !bFileHandled )
								{
									// Something in Development/Src that we can't categorise - so build everything!
									CompileAll( CISTasks );
									bFileHandled = true;
								}
							}

							if( !bFileHandled )
							{
								if( FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Engine" )
									|| FileName.StartsWith( "    ... //depot/" + Builder.BranchDef.Branch + "/Development/External" ) )
								{
									// Build everything!
									CompileAll( CISTasks );
								}

								bFileHandled = true;
							}
						}

						if( bAddToolsJob && CISTasks.ContainsKey( "Tools" ) )
						{
							CISTasks["Tools"].CompileRequired();
						}
					}

					// Check to see if any jobs need to be spawned
					foreach( CISTaskInfo Task in CISTasks.Values )
					{
						if( Task.JobsToSpawn() )
						{
							bJobsSpawned = true;
							break;
						}
					}

					// Spawn needed jobs
					if( bJobsSpawned )
					{
						// Create the container changelist
						int NewID = BuilderLinq.InsertChangelist( Builder.BranchDef, CL );

						foreach( CISTaskInfo Task in CISTasks.Values )
						{
							if( Task.SpawnJobs( this, Builder, CL, NewID, JobSpawnTime ) )
							{
								if( UpdateLatestProcessedChangelist )
								{
									Task.LastAttempted = CL.Number;
								}
							}
						}
					}
				}

				// Update LastAttempted in the database
				foreach( CISTaskInfo Task in CISTasks.Values )
				{
					BuilderLinq.UpdateCISTask( Task );
				}
			}
			else
			{
				Builder.Write( "None of the new changes require CIS jobs!" );
			}

			if( UpdateLatestProcessedChangelist )
			{
				// Update the variables table with the last processed CL
				BuilderLinq.UpdateBranchConfigInt( Builder.BranchDef.ID, "LastAttemptedOverall", CISLastChangelistToProcess );
			}
		}

		public bool UpdateLastGoodAndFailedValues( CISTaskInfo Task, out bool SuccessChanged, out int SuccessChanger )
		{
			// Check to see if CIS being run for this type
			int OldLastAttemptedChangelist = Task.LastAttempted;
			if( OldLastAttemptedChangelist < 0 )
			{
				// CIS not being run for this type
				Task.LastGood = -1;
				Task.LastFail = -1;
				SuccessChanged = false;
				SuccessChanger = -1;
				return true;
			}

			// Construct the query we'll use to get the changes of interest
			int OldLastChangelistProcessed = BuilderLinq.GetIntFromBranch( Task.BranchConfigID, "LastAttemptedOverall" );
	
			int LastFullyKnownChangelist = BuilderLinq.GetIntFromBranch( Task.BranchConfigID, "LastFullyKnown" );
			int OldLastGoodChangelist = Task.LastGood;
			int OldLastFailedChangelist = Task.LastFail;
			bool OldBuildIsGood = OldLastGoodChangelist > OldLastFailedChangelist;

			// Seed with the old/current/sane values, assuming we'll update them below, if needed
			SuccessChanged = false;
			SuccessChanger = -1;

			// Display the current state of the build before checking the database for updates
			Log( "Original " + Task.TestType + " CIS Monitor values", Color.Green );
			Log( "    Old LastGood        = " + OldLastGoodChangelist.ToString(), Color.Green );
			Log( "    Old LastFailed      = " + OldLastFailedChangelist.ToString(), Color.Green );
			Log( "", Color.Green );

			List<Main.CISJobsInfo> CISJobs = BuilderLinq.GetActiveCISJobs( LastFullyKnownChangelist, Task.BranchConfigID, Task.TestType );

			bool JobsInFlight = false;
			bool BuildIsGood = OldBuildIsGood;
			bool MarkRemainingJobsOptional = false;
			bool FoundCompletedJob = false;

			// Evaluate the results and update the DB
			int CurrentResultIndex;
			for( CurrentResultIndex = 0; CurrentResultIndex < CISJobs.Count && !FoundCompletedJob; CurrentResultIndex++ )
			{
				Main.CISJobsInfo CurrentResult = CISJobs[CurrentResultIndex];

				// If we hit an incomplete job, note that we have some relevant jobs in flight and move along to the next job
				if( !CurrentResult.Complete )
				{
					JobsInFlight = true;
					if( CurrentResult.Active )
					{
						BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.InProgress, "In Progress" );
					}

					continue;
				}
				// Once we find a job that is complete,
				else
				{
					// If the job is successful,
					if( CurrentResult.Success )
					{
						// And if there are no other jobs in flight after this one,
						if( !JobsInFlight )
						{
							// Then promote the success state up to the latest one processed
							Task.LastGood = OldLastChangelistProcessed;
						}
						else
						{
							// Otherwise, mark this as the last good changelist and move on
							Task.LastGood = CurrentResult.Changelist;
						}

						// All remaining older jobs are optional now
						MarkRemainingJobsOptional = true;
						BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.Succeeded, "OK" );
					}
					// If the job failed,
					else
					{
						// And if there are no other job in flight after this one,
						if( !JobsInFlight )
						{
							// Then promote the failure state up to the latest one processed
							Task.LastFail = OldLastChangelistProcessed;
						}
						else
						{
							// Otherwise, mark this as the last failing changelist and move on
							Task.LastFail = CurrentResult.Changelist;
						}

						BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.Failed, "" );
					}

					// Update the state of the build variables
					BuildIsGood = CurrentResult.Success;
					SuccessChanged = BuildIsGood != OldBuildIsGood;
					SuccessChanger = CurrentResult.Changelist;

					// We're all done!
					FoundCompletedJob = true;
				}
			}

			// Make sure to update the state of the remaining jobs
			for( ; CurrentResultIndex < CISJobs.Count; CurrentResultIndex++ )
			{
				Main.CISJobsInfo CurrentResult = CISJobs[CurrentResultIndex];

				// If the job has been started,
				if( CurrentResult.Active )
				{
					// And it's been completed,
					if( CurrentResult.Complete )
					{
						// And it succeeded
						if( CurrentResult.Success )
						{
							// Update with success
							if( Task.LastGood < CurrentResult.Changelist )
							{
								Task.LastGood = CurrentResult.Changelist;
							}
							MarkRemainingJobsOptional = true;
							BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.Succeeded, "OK" );
						}
						else
						{
							// Otherwise, update with failure
							if( Task.LastFail < CurrentResult.Changelist )
							{
								Task.LastFail = CurrentResult.Changelist;
							}
							BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.Failed, "" );
						}
					}
					else if( MarkRemainingJobsOptional )
					{
						// It's active but incomplete and the results are optional - update as such since we won't revisit
						BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.InProgressOptional, "In Progress (Optional)" );
					}
					else
					{
						// Otherwise, update as still in progress
						BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.InProgress, "In Progress (Not optional)" );
					}
				}
				else if( MarkRemainingJobsOptional )
				{
					// If the job hasn't been started yet and we can mark it as optional
					BuilderLinq.MarkJobsOptional( CurrentResult.ID );

					BuilderLinq.UpdateChangelist( CurrentResult.CISJobStateID, Main.JobState.Optional, "Optional" );
				}
			}

			// If CIS was disabled, fix up any variables that may have been lost
			if( Task.LastGood < 0 )
			{
				Task.LastGood = Task.LastFail - 1;
			}

			if( Task.LastFail < 0 )
			{
				Task.LastFail = Task.LastGood - 1;
			}

			Log( "Updated " + Task.TestType + " CIS Monitor values", Color.Green );
			Log( "    New LastGood        = " + Task.LastGood.ToString(), Color.Green );
			Log( "    New LastFailed      = " + Task.LastFail.ToString(), Color.Green );
			Log( "    New Changer         = " + SuccessChanger.ToString(), Color.Green );
			Log( "    Build state         = " + BuildIsGood.ToString(), Color.Green );
			Log( "    Build state changed = " + SuccessChanged.ToString(), Color.Green );
			Log( "", Color.Green );

			// Post the updated results
			BuilderLinq.UpdateCISTask( Task );

			// Update any InProgressOptional results
			BuilderLinq.CleanupInProgressOptional( Task.BranchConfigID, Task.TestType );

			return BuildIsGood;
		}
	}
}