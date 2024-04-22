// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Controller
{
    class LogParser
    {
        private Main Parent = null;
        private BuildState Builder = null;
        private StreamReader Log = null;
        private string LastProject;
        private string FinalError;
        private bool FoundAnyError = false;
        private bool FoundError = false;
		private bool SuppressChecking = false;

        public LogParser( Main InParent, BuildState InBuilder )
        {
            Parent = InParent;
            Builder = InBuilder;

            try
            {
                Log = new StreamReader( Builder.LogFileName );
			}
            catch
            {
            }
        }

		static public string ExplainError( string Error )
		{
			string Explanation = "";
			if( Error.Contains( "System.IO.IOException: Data error" ) )
			{
				Explanation = "This means the destination disk is full or corrupt";
			}

			return( Explanation );
		}

		public string Parse( bool ReportEntireLog, bool CheckCookingSuccess, bool CheckCookerSyncSuccess, ref COMMANDS ErrorLevel )
        {
            string Line;
            int LinesToGrab = 0;

            if( Log == null )
            {
                return ( "Failed to open log: '" + Builder.LogFileName + "'" );
            }

            // Read in the entire log file
            Line = Log.ReadLine();
            while( Line != null )
            {
                // Check for a project status line
                if( Line.IndexOf( "------" ) >= 0 || Line.IndexOf( "Entering directory" ) >= 0 )
                {
                    LastProject = Line;
                    FoundError = false;
                }
				// Don't add any more errors as they are already counted
				else if( Line.IndexOf( "Warning/Error Summary" ) >= 0 )
				{
					SuppressChecking = true;
				}
                // Grab any extra lines if we have been told to
				else if( !SuppressChecking && LinesToGrab > 0 )
                {
                    FinalError += Line + Environment.NewLine;
                    LinesToGrab--;
                }
				else if( !SuppressChecking && Line.IndexOf( "M: make" ) >= 0 )
                {
                    FinalError += Line + Environment.NewLine;
                }
				else if( CheckCookingSuccess && ErrorLevel == COMMANDS.None &&
                         ( Line.IndexOf( "Success - 0 error(s)" ) >= 0 ) )
                {
					ErrorLevel = COMMANDS.CookingSuccess;
                }
				else if( CheckCookerSyncSuccess && ErrorLevel == COMMANDS.None &&
                         ( Line.IndexOf( "[SYNCING WITH FOLDERS FINISHED]" ) >= 0 ) )
                {
					ErrorLevel = COMMANDS.CookerSyncSuccess;
                }
				else if( Builder.ErrorMode == BuildState.ErrorModeType.IgnoreErrors && Line.IndexOf( "DBGHELP: " ) >= 0 )
				{
					// We've crashed - check for errors and fail properly
					Builder.ErrorMode = BuildState.ErrorModeType.SuppressErrors;
					SuppressChecking = false;
					ErrorLevel = COMMANDS.CriticalError;

					// Seek back to beginning of log file 
					Log.BaseStream.Seek( 0, SeekOrigin.Begin );
				}
				// A specific check for a specific error with a specific command...
				else if( (Line.IndexOf( "Error deleting file" ) >= 0) &&
						 (Line.IndexOf( "WorkerLog.txt' (GetLastError: 32)" ) >= 0) )
				{
					Parent.SendWarningMail( "Deleting a shader compile worker file", Line, false );
				}
				else if( Line.IndexOf( "GetLastE-r-r-o-r" ) >= 0 )
                {
					Parent.SendWarningMail( "MoveError", Line, false );
                }
				else if( Line.IndexOf( "[DEPRECATED] " ) >= 0 )
				{
					Parent.SendWarningMail( "Deprecated command", Line, false );
				}
				// Check for errors
				else if( !SuppressChecking &&
						 ( Line.IndexOf( "ERROR: " ) >= 0
						 || Line.IndexOf( " : error" ) >= 0
						 || Line.IndexOf( ":Error:" ) >= 0
						 || Line.IndexOf( " Error:" ) >= 0
						 || Line.IndexOf( "]Error:" ) >= 0
						 || Line.IndexOf( ": error:" ) >= 0
						 || Line.IndexOf( ": error " ) >= 0
						 || Line.IndexOf( "cl : Command line error" ) >= 0
						 || Line.IndexOf( "1 error generated." ) >= 0
						 || Line.IndexOf( "SYMSTORE ERROR:" ) >= 0
						 || Line.IndexOf( "PROCESS ERROR:" ) >= 0
						 || Line.IndexOf( "FATAL ERROR:" ) >= 0
						 || Line.IndexOf( "UBT ERROR:" ) >= 0
						 || Line.IndexOf( "IPP ERROR:" ) >= 0
						 || Line.IndexOf( "FTP ERROR:" ) >= 0
						 || Line.IndexOf( "PSP ERROR:" ) >= 0
						 || Line.IndexOf( "XBP ERROR:" ) >= 0
						 || Line.IndexOf( "UCO ERROR:" ) >= 0
						 || Line.IndexOf( "SYNC ERROR:" ) >= 0
						 || Line.IndexOf( "MAKEISO ERROR:" ) >= 0
						 || Line.IndexOf( "ERROR 3" ) >= 0
						 || Line.IndexOf( "VALIDATION ERROR:" ) >= 0
						 || Line.IndexOf( "[BEROR]" ) >= 0
						 || Line.IndexOf( "Script error on line" ) >= 0
						 || Line.IndexOf( "Utility finished with exit code: -1" ) >= 0
						 || Line.IndexOf( ": fatal error" ) >= 0
						 || Line.IndexOf( " ... failed." ) >= 0
						 || Line.IndexOf( "] Error" ) >= 0
						 || Line.IndexOf( "** BUILD FAILED **" ) >= 0
						 || Line.IndexOf( "is not recognized as an internal or external command" ) >= 0
						 || Line.IndexOf( "Error Domain=NSCocoaErrorDomain Code=513" ) >= 0
						 || Line.IndexOf( "Could not open solution: " ) >= 0
						 || Line.IndexOf( "Parameter format not correct" ) >= 0
						 || Line.IndexOf( "internal compiler error" ) >= 0
						 || Line.IndexOf( "Another build is already started on this computer." ) >= 0
						 || Line.IndexOf( "Failed to initialize Build System:" ) >= 0
						 || Line.IndexOf( "Sorry but the link was not completed because memory was exhausted." ) >= 0
						 || Line.IndexOf( "simply rerunning the compiler might fix this problem" ) >= 0
						 || Line.IndexOf( "No connection could be made because the target machine actively refused" ) >= 0
						 || Line.IndexOf( ": unexpected error with pch, try rebuilding the pch" ) >= 0
						 || Line.IndexOf( "Internal Linker Exception:" ) >= 0
						 || Line.IndexOf( ": warning LNK4019: corrupt string table" ) >= 0
						 || Line.IndexOf( "Proxy could not update its cache" ) >= 0
						 || Line.IndexOf( "error MS" ) >= 0
						 || Line.IndexOf( "Error! Inject " ) >= 0
						 || Line.IndexOf( "Error! Failed " ) >= 0
						 || Line.IndexOf( "ANDROID: Error" ) >= 0
						 || Line.IndexOf( "You have not agreed to the Xcode license agreements" ) >= 0
						 || Line.IndexOf( "FATAL ERROR" ) >= 0
						 || Line.IndexOf( "cannot execute binary file" ) >= 0
						 || Line.IndexOf( "Invalid solution configuration" ) >= 0
						 || Line.IndexOf( "java.io.FileNotFoundException: C:\\cygwin\\tmp" ) >= 0
						 || Line.IndexOf( "java.lang.OutOfMemoryError:" ) >= 0
						 || Line.IndexOf( "is from a previous version of this application and must be converted in order to build" ) >= 0
						 || Line.IndexOf( "invalid name for SPA section" ) >= 0
						 || Line.IndexOf( ": Invalid file name, " ) >= 0
						 || Line.IndexOf( "System.UnauthorizedAccessException" ) >= 0
						 || Line.IndexOf( "System.NullReferenceException:" ) >= 0
						 || Line.IndexOf( "System.IO.IOException: Data error" ) >= 0
						 || Line.IndexOf( "System.IO.PathTooLongException: The specified path, file name, or both are too long" ) >= 0
						 || Line.IndexOf( "System.IO.FileNotFoundException: Could not find file" ) >= 0
						 || Line.IndexOf( "System.IO.DirectoryNotFoundException:" ) >= 0
						 || Line.IndexOf( "System.IO.IOException: An internal error occurred." ) >= 0
						 || Line.IndexOf( "System.ArgumentException: An item with the same key has already been added." ) >= 0
						 || Line.IndexOf( "System.ArgumentException: Illegal characters in path." ) >= 0
						 || Line.IndexOf( "The specified PFX file do not exist. Aborting" ) >= 0
						 || Line.IndexOf( "ZDP SDK tool error:" ) >= 0
						 || Line.IndexOf( "Error parsing command file on line" ) >= 0
						 || Line.IndexOf( ": Failed to start local process for action:" ) >= 0
						 || Line.IndexOf( "binary is not found. Aborting" ) >= 0
						 || Line.IndexOf( "Input file not found: " ) >= 0
						 || Line.IndexOf( "An exception occurred during merging:" ) >= 0
						 || Line.IndexOf( "Install the 'Microsoft Windows SDK for Windows 7 and .NET Framework 3.5 SP1'" ) >= 0
						 || Line.IndexOf( "is less than package's new version 0x" ) >= 0
						 || Line.IndexOf( "Build failed. Please ensure that all updated executables have the correct version." ) >= 0
						 || Line.IndexOf( "Assert( Assertion Failed: Looks like steam didn't shutdown cleanly, scheduling immediate update check )" ) >= 0
						 || Line.IndexOf( "current engine version is older than version the package was originally saved with" ) >= 0 ) )
				{
					if( !FoundError )
					{
						FinalError += LastProject + Environment.NewLine;
					}
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
				}
				else if( !SuppressChecking && Line.IndexOf( "P4ERROR: " ) >= 0 )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 2;
				}
				// Check for script compile errors
				else if( !SuppressChecking &&
						 ( Line.IndexOf( "warning treated as error" ) >= 0
						 || Line.IndexOf( "warnings being treated as errors" ) >= 0 ) )
				{
					if( !FoundError )
					{
						FinalError += LastProject + Environment.NewLine;
					}
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 6;
				}
				// Check for script compile errors
				else if( !SuppressChecking && 
						 ( Line.IndexOf( " : Error," ) >= 0 ) )
				{
					if( !FoundError )
					{
						FinalError += LastProject + Environment.NewLine;
					}
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
				}
				// Check for UBT errors
				else if( ( Line.IndexOf( "UnrealBuildTool.BuildException:" ) >= 0 )
						 || Line.IndexOf( "UnrealBuildTool error:" ) >= 0 )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 10;
				}
				// Check for app crashing
				else if( ( Line.IndexOf( "=== Critical error: ===" ) >= 0 )
						|| ( Line.IndexOf( "Critical: appError" ) >= 0 ) )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					SuppressChecking = false;
					ErrorLevel = COMMANDS.CriticalError;

					// Grab start of callstack
					LinesToGrab = 10;
				}
				// Check for app errors
				else if( ( Line.IndexOf( ": Failure -" ) >= 0
							|| Line.IndexOf( "exceeds maximum length" ) >= 0
							|| Line.IndexOf( "appError" ) >= 0 ) )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 2;
				}
				// Check for app errors
				else if( ( Line.IndexOf( "The following files were specified on the command line:" ) >= 0
						 || Line.IndexOf( "Error executing" ) >= 0 ) )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 4;
				}
				// Check for CookerSync fails
				else if( ( Line.IndexOf( ": Exception was" ) >= 0
						 || Line.IndexOf( "=> NETWORK WRITE ERROR: There is not enough space on the disk." ) >= 0
						 || Line.IndexOf( "==> " ) >= 0 )
						 || Line.IndexOf( ":  error: " ) >= 0 )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 1;
				}
				// Check for P4 sync errors
				else if( ( Line.IndexOf( "can't edit exclusive file already opened" ) >= 0 ) )
				{
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
					LinesToGrab = 1;

					ErrorLevel = COMMANDS.SCC_Checkout;
				}
				else if( ( ErrorLevel == COMMANDS.CheckForUCInVCProjFiles ) &&
						 ( ( Line.IndexOf( ".vcproj is missing" ) >= 0 ) ||
						   ( Line.IndexOf( ".vcproj contains a reference to" ) >= 0) ) )
				{
					if( !FoundError )
					{
						FinalError += LastProject + Environment.NewLine;
					}
					FinalError += Line + Environment.NewLine;
					FoundError = true;
					FoundAnyError = true;
				}
				else if( Line.Contains( "[REPORT]" ) )
				{
					int Index = Line.IndexOf( "[REPORT]" );
					if( Index >= 0 )
					{
						Builder.AddToSuccessStatus( Line.Substring( Index ) );
					}
				}
				else if( Line.Contains( "[WARNING]" ) )
				{
					int Index = Line.IndexOf( "[WARNING]" );
					if( Index >= 0 )
					{
						Builder.AddToSuccessStatus( Line.Substring( Index ) );
					}
				}
				else if( Line.Contains( "is not conformed, skipping localization for SoundNodeWave" ) )
				{
					int PackageIndex = Line.IndexOf( "Package " );
					int PackageNameIndex = Line.IndexOf( ' ', PackageIndex + "Package ".Length + 1 );
					if( PackageIndex >= 0 && PackageNameIndex >= 0 )
					{
						string Message = Line.Substring( PackageIndex, PackageNameIndex - PackageIndex );
						Builder.AddUniqueToSuccessStatus( Message + " is not conformed!" );
					}
				}
				else if( Line.Contains( "[PERFCOUNTER]" ) )
				{
					int Index = Line.IndexOf( "[PERFCOUNTER]" );
					if( Index >= 0 )
					{
						Parent.AddPerfData( Line.Substring( Index + "[PERFCOUNTER]".Length ) );
					}
				}
				else if( Line.Contains( "Operation synced" ) )
				{
					// Operation synced 0.00 GB
					// Track amount of data published here
				}
				else if( ReportEntireLog )
				{
					FoundAnyError = true;
					if( Line.Length > 0 )
					{
						FinalError += Line + Environment.NewLine;
					}
				}
				// Finally, if we encounter a warning, just grab the line for tracking
				// and continue quietly along. If the warning is treated as an error,
				// it'll be caught later on (this is especially important for PS3).
				else if( !SuppressChecking && Line.IndexOf( "): warning " ) >= 0 )
				{
					// Add some context indicators
					FinalError += "..." + Environment.NewLine;
					FinalError += Line + Environment.NewLine;
					// Read a few lines for additional context
					Line = Log.ReadLine();
					if( Line != null )
					{
						FinalError += Line + Environment.NewLine;
						Line = Log.ReadLine();
						if( Line != null )
						{
							FinalError += Line + Environment.NewLine;
						}
					}
					FinalError += "..." + Environment.NewLine;
				}

                Line = Log.ReadLine();
            }

			Log.Close();

            if( FoundAnyError )
            {
				return ( FinalError );
            }

            return ( "Succeeded" );
        }
    }
}
