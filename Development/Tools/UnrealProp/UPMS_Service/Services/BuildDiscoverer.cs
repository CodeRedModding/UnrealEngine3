/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.IO;
using System.Data;
using System.Data.SqlClient;
using System.Configuration;

namespace UnrealProp
{
    // Working thread for discovering new builds in build repository
    public static class BuildDiscoverer
    {
        static UPMS_Service Owner = null;
        static Thread Thread = null;
        static Random Rnd = new Random();

        static public void Init( UPMS_Service InOwner )
        {
            Owner = InOwner;

            Thread = new Thread( new ThreadStart( DiscoveringProc ) );
            Thread.Start();
            Log.WriteLine( "UPMS BUILD DISCOVERER", Log.LogType.Important, "Initialised!" );
        }

        static public void Stop()
        {
			if( Thread != null )
			{
				Thread.Abort();
				Thread = null;
			}
        }

		// recursive finding specific folder in directory tree
		static void GetFolderListFromRepository( DirectoryInfo DirInfo, ref List<string> List, ref int SubLevel )
		{
			DirectoryInfo[] Dirs = DirInfo.GetDirectories();
			SubLevel--;

			if( SubLevel > 0 )
			{
				foreach( DirectoryInfo SubDir in Dirs )
				{
					GetFolderListFromRepository( SubDir, ref List, ref SubLevel );
				}
			}
			else
			{
				foreach( DirectoryInfo Dir in Dirs )
				{
					// Potentially, only add old builds
					if( Dir.LastWriteTimeUtc < DateTime.UtcNow )
					{
						List.Add( Dir.FullName );
					}
				}
			}

			SubLevel++;
		}

		// recursive finding specific files in directory tree
		static bool GetFileListFromRepository( DirectoryInfo DirInfo, List<string> FileNames, ref int SubLevel )
		{
			bool bTOCFound = false;

			SubLevel--;
			if( SubLevel > 0 )
			{
				DirectoryInfo[] Dirs = DirInfo.GetDirectories();
				foreach( DirectoryInfo SubDir in Dirs )
				{
					bTOCFound = GetFileListFromRepository( SubDir, FileNames, ref SubLevel );
					if( bTOCFound )
					{
						return ( true );
					}
				}
			}

			foreach( string FileName in FileNames )
			{
				FileInfo[] Files = DirInfo.GetFiles( FileName );
				if( Files.Length > 0 )
				{
					return ( true );
				}
			}

			SubLevel++;
			return ( bTOCFound );
		}

		static void RecursiveDeleteFolder( string Path )
		{
			DirectoryInfo DirInfo = new DirectoryInfo( Path );

			if( DirInfo.Exists )
			{
				foreach( FileInfo File in DirInfo.GetFiles() )
				{
					File.IsReadOnly = false;
					File.Delete();
				}

				foreach( DirectoryInfo Dir in DirInfo.GetDirectories() )
				{
					RecursiveDeleteFolder( Dir.FullName );
				}

				DirInfo.Delete();
			}
		}

		static void DeleteBuild( DataRow Row )
		{
#if !DEBUG
            try
            {
#endif
				Log.WriteLine( "UPMS BUILD CLEANER", Log.LogType.Info, "Started deleting build:" + Row["Path"].ToString().Trim() );

				// Analyzing files
				string Path = Row["Path"].ToString().Trim();
				
				// Get the ID of the build to delete
				long PlatformBuildID = Convert.ToInt64( Row["ID"] );

				// Delete the files and folders
				RecursiveDeleteFolder( Path );

				DataHelper.PlatformBuild_Delete( PlatformBuildID );

				Log.WriteLine( "UPMS BUILD CLEANER", Log.LogType.Info, "Finished deleting build: " + Row["Path"].ToString().Trim() );
#if !DEBUG
            }
            catch( Exception Ex )
            {
                Log.WriteLine( "UPMS BUILD CLEANER", Log.LogType.Error, "Unhandled exception while deleting build: " + Ex.ToString() );
            }
#endif
		}

		static void DeleteClientMachine( DataRow Row )
		{
#if !DEBUG
			try
			{
#endif
				Log.WriteLine( "UPMS MACHINE CLEANER", Log.LogType.Info, "Deleting client machine:" + Row["ClientMachineID"].ToString().Trim() );
				DataHelper.ClientMachine_Delete( ( int )Row["ClientMachineID"] );
#if !DEBUG
			}
			catch( Exception Ex )
			{
				Log.WriteLine( "UPMS MACHINE CLEANER", Log.LogType.Error, "Unhandled exception while deleting build: " + Ex.ToString() );
			}
#endif
		}

		static void AddBadBuild( string Error, string BuildFolder )
		{
			DataHelper.PlatformBuild_AddNew( "Bad", "PC", "", "", "", "buildmachine", "Bad Build", BuildFolder, DateTime.Now );

			Log.WriteLine( "UPMS BUILD DISCOVERER", Log.LogType.Error, Error + BuildFolder );
		}

		static string[] ValidBranchPrefixes = new string[]
		{
			"UnrealEngine3",
			"UE3",
			"UE4",
		};

        // Main thread to discover builds
        static void DiscoveringProc()
        {
            Thread.Sleep( 3000 );

            while( true )
            {
#if !DEBUG
                // Send out periodic status emails
				Owner.SendStatusEmail();
#endif

				// Build repository \ game name \ platform \ label  
                // e.g.
                // \\prop-01\Builds\Gear\PC\Gear_PC_[2007-12-04_02.00]
                // \\prop-01\Builds\UT\PC\UT_PC_[2007-10-21_00.11]_[SHIPPING_PC_GAME=1]
                // \\prop-01\Builds\UT\PS3\UT_PS3_[2007-11-14_20.12]_Fixed76801
#if !DEBUG
                try
                {
#endif
					foreach( string BuildPath in Owner.BuildRepositoryPaths )
					{
						Log.WriteLine( "UPMS BUILD DISCOVERER", Log.LogType.Info, "is looking for new builds in: " + BuildPath );

						// Get a list of potential builds from the current repository
						DirectoryInfo BuildPathInfo = new DirectoryInfo( BuildPath );
						List<string> FolderList = new List<string>();
						int FolderDepth = 3;
						GetFolderListFromRepository( BuildPathInfo, ref FolderList, ref FolderDepth );

						// Validate each potential build
						foreach( string PotentialBuild in FolderList )
						{
							DirectoryInfo PotentialBuildInfo = new DirectoryInfo( PotentialBuild );

							// Check for a valid branch prefix in the path
							DirectoryInfo[] PotentialBuildBranchInfo = null;
							foreach( string PotenialPrefix in ValidBranchPrefixes )
							{
								PotentialBuildBranchInfo = PotentialBuildInfo.GetDirectories( PotenialPrefix + "*" );
								if( PotentialBuildBranchInfo.Length == 1 )
								{
									break;
								}

								PotentialBuildBranchInfo = null;
							}

							if( PotentialBuildBranchInfo == null )
							{
								AddBadBuild( "Missing or multiple branch names!!! Deleting bad build: ", PotentialBuild );
								continue;
							}

							// Find the TOC file - always the last file written and required to be there
							List<string> FileNames = new List<string>() { "*TOC.txt", "ISSetup.dll" };
							int Depth = 2;
							if( !GetFileListFromRepository( PotentialBuildBranchInfo[0], FileNames, ref Depth ) )
							{
								// It's only a bad if it's taken more than a day to copy up the TOC file
								if( PotentialBuildBranchInfo[0].LastWriteTimeUtc.AddDays( 1 ) < DateTime.UtcNow )
								{
									AddBadBuild( "Missing *TOC.txt file!!! Deleting bad build: ", PotentialBuild );
								}
								continue;
							}

							// Set up build parameters
							string ProjectName = "";
							string Platform = "";
							string DefineA = "";
							string DefineB = "";
							string DefineC = "";
							string Email = "UnrealProp";

							// Get whether this is an official build
							bool OfficialBuild = false;
							if( PotentialBuild.IndexOf( "User\\" ) < 0 )
							{
								OfficialBuild = true;
								Email = "Build Machine";
							}

							// Get the unique encoded folder name
							string PublishFolder = PotentialBuild.Substring( PotentialBuild.LastIndexOf( '\\' ) + 1 );

							if( PublishFolder.Length < "X_Y_[YYYY-MM-DD_HH.MM]".Length )
							{
								AddBadBuild( "Invalid folder name!!! Deleting bad build that has a too small folder name: ", PotentialBuild );
								continue;
							}

							string[] Parts = PublishFolder.Split( '_' );
							if( Parts.Length < 3 )
							{
								AddBadBuild( "Invalid folder name!!! Deleting bad build with no game and platform info: ", PotentialBuild );
								continue;
							}

							// Gear
							ProjectName = Parts[0];
							// PC
							Platform = Parts[1];
							// TimeStamp
							DateTime TimeStamp = DateTime.Now;
							try
							{
								TimeStamp = DateTime.ParseExact( Parts[2] + "_" + Parts[3], "[yyyy-MM-dd_HH.mm]", null );
							}
							catch
							{
								AddBadBuild( "Invalid folder name!!! Deleting bad build with invalid timestamp: ", PotentialBuild );
								continue;
							}

							if( OfficialBuild )
							{
								if( Parts.Length == 5 )
								{
									DefineA = Parts[4].Trim( "[]".ToCharArray() ).ToUpper();
								}
								else if( Parts.Length == 6 )
								{
									DefineA = Parts[4].Trim( "[]".ToCharArray() ).ToUpper();
									DefineB = Parts[5].Trim( "[]".ToCharArray() ).ToUpper();
								}
								else if( Parts.Length >= 7 )
								{
									DefineA = Parts[4].Trim( "[]".ToCharArray() ).ToUpper();
									DefineB = Parts[5].Trim( "[]".ToCharArray() ).ToUpper();
									DefineC = Parts[6].Trim( "[]".ToCharArray() ).ToUpper();
								}
							}
							else
							{
								// Uploaded from UFE, has the format [User] on the end
								if( Parts.Length == 5 )
								{
									Email = Parts[4].Trim( "[]".ToCharArray() ).ToLower();

									// Capitalise
									string[] SplitName = Email.Split( ".".ToCharArray() );
									if( SplitName.Length == 2 )
									{
										Email = SplitName[0].Substring( 0, 1 ).ToUpper() + SplitName[0].Substring( 1 );
										Email += ".";
										Email += SplitName[1].Substring( 0, 1 ).ToUpper() + SplitName[1].Substring( 1 );
										Email += "@epicgames.com";
									}
								}
							}

							// looking for known platform in part[1]
							if( !DataHelper.Platform_IsValid( Platform ) )
							{
								AddBadBuild( "Unknown Platform!!! Deleting bad build with invalid platform:", PotentialBuild );
								continue;
							}

							// Shrink the publish folder down as we have a 40 character limit
							PublishFolder = PublishFolder.Replace( "_" + Platform + "_", "_" );

							int OpenIndex = PublishFolder.IndexOf( '[' );
							int CloseIndex = PublishFolder.IndexOf( ']' );
							if( OpenIndex < 0 || CloseIndex < 0 )
							{
								AddBadBuild( "Invalid folder name!!! Deleting bad build with no timestamp: ", PotentialBuild );
								continue;
							}

							// Chop out the year
							PublishFolder = PublishFolder.Substring( 0, OpenIndex + 1 ) + PublishFolder.Substring( OpenIndex + 6, PublishFolder.Length - OpenIndex - 6 );

							// Just truncate as a last resort
							if( PublishFolder.Length > 40 )
							{
								PublishFolder = PublishFolder.Substring( 0, 40 );
							}

							// Add this folder to the build repository database
							if( DataHelper.PlatformBuild_AddNew( ProjectName, Platform, DefineA, DefineB, DefineC, Email, PublishFolder, PotentialBuild, TimeStamp ) )
							{
								Log.WriteLine( "UPMS BUILD DISCOVERER", Log.LogType.Info, "found and registered new build in repository: " + PotentialBuild );
								Owner.NewBuildsTotal++;
							}
						}
					}

					// Search for deleted builds and delete them
					int StatusDeleted = ( int )( RemotableType.BuildStatus.Deleted );
					DataSet DS = DataHelper.GetDataSet( "SELECT * FROM [PlatformBuilds] WHERE ( StatusID = " + StatusDeleted.ToString() + " )" );
					foreach( DataRow Row in DS.Tables[0].Rows )
					{
						DeleteBuild( Row );
					}

					// Search for client machines that haven't had anything propped in 12 months
					DataSet ClientMachines = DataHelper.GetDataSet( "SELECT ClientMachineID FROM [Tasks], [ClientMachines] WHERE Tasks.ClientMachineID = ClientMachines.ID AND DATEDIFF( day, ClientMachines.Created, GETDATE() ) > 1 GROUP BY ClientMachineID" );
					foreach( DataRow Row in ClientMachines.Tables[0].Rows )
					{
						long Count = DataHelper.ExecuteScalar( "SELECT COUNT( * ) FROM [Tasks] WHERE ClientMachineID = " + ( int )Row["ClientMachineID"] + " AND DATEDIFF( month, ScheduleTime, GETDATE() ) <= 12" );
						if( Count == 0 )
						{
							DeleteClientMachine( Row );
							break;
						}
					}
					
                    // 55-65 sec interval
                    Thread.Sleep( Rnd.Next( 55000, 65000 ) ); 
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                    {
                        Log.WriteLine( "UPMS BUILD DISCOVERER", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                    }
                }
#endif
            }
        }
    }
}
