/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Data.SqlClient;

namespace UnrealBuildTool
{
	class PerfDataBase
	{
		/**
		 * Sends the build summary to the performance database.
		 * 
		 * @param	Duration			Time spent in UnrealBuildTool thus far, excluding DB time
		 * @param	LinkTime			Time spent linking, -1 if not available
		 * @param	Target				Target that was built
		 * @param	bSuccess			Whether the build succeeded or not
		 * @param	NumActions			Total number of actions, includes ones not executed as produced items weren't outdated
		 * @param	NumOutdatedActions	Number of outdated actions
		 * @param	NumExecutedActions	Number of executed actions
		 * @param	ExecutorName		Name of executor executing actions, e.g. "NoActionsToExecute", "Local" or "XGE"
		 */
		public static void SendBuildSummary( double Duration, double LinkTime, UE3BuildTarget Target, bool bSuccess, int NumActions, int NumOutDatedActions, int NumExecutedActions, string ExecutorName )
		{
			try
			{
				// Only try to connect if DB name is not empty.
				if( BuildConfiguration.PerfDatabaseName.Length > 0 )
				{
					// Build query string using build in Windows Authentication and lowers the connection timeout to 3 seconds.
					string ConnectionString = String.Format("Data Source={0};Initial Catalog=UnrealBuildToolStats;Trusted_Connection=Yes;Connection Timeout=3;",BuildConfiguration.PerfDatabaseName);
					SqlConnection PerfDBConnection = new SqlConnection(ConnectionString);
					// Open connection to DB.
					PerfDBConnection.Open();

					// WARNING: DO NOT MODIFY THE BELOW WITHOUT UPDATING THE DB, BUMPING THE VERSION AND REGENERATE THE CREATE SCRIPTS

					// Create command string for stored procedure.
					string SqlCommandString = "EXEC dbo.AddRun_v3 ";
					SqlCommandString += String.Format(" @Duration='{0}',", Duration );
					SqlCommandString += String.Format(" @LinkTime='{0}',", LinkTime );
					SqlCommandString += String.Format(" @ConfigName='{0}',", Target == null ? "multiple" : Target.GetConfiguration().ToString() );
					SqlCommandString += String.Format(" @PlatformName='{0}',", Target == null ? "multiple" : Target.GetPlatform().ToString() );
					SqlCommandString += String.Format(" @GameName='{0}',", Target == null ? "multiple" : Target.GetGameName().Replace("Game","") );
					SqlCommandString += String.Format(" @MachineName='{0}',", System.Environment.MachineName );
					SqlCommandString += String.Format(" @UserName='{0}',", System.Environment.UserName );
					SqlCommandString += String.Format(" @ExecutorName='{0}',", ExecutorName );
					SqlCommandString += String.Format(" @bSuccess='{0}',", bSuccess );
					SqlCommandString += String.Format(" @bGeneratedDebugInfo='{0}',", Target == null ? !BuildConfiguration.bDisableDebugInfo : Target.IsCreatingDebugInfo() );
					SqlCommandString += String.Format(" @NumActions='{0}',", NumActions );
					SqlCommandString += String.Format(" @NumOutdatedActions='{0}',", NumOutDatedActions );
					SqlCommandString += String.Format(" @NumExecutedActions='{0}',", NumExecutedActions );
					SqlCommandString += String.Format(" @NumIncludedBytesPerUnityCPP='{0}',", BuildConfiguration.NumIncludedBytesPerUnityCPP );
					SqlCommandString += String.Format(" @MinFilesUsingPrecompiledHeader='{0}',", BuildConfiguration.MinFilesUsingPrecompiledHeader );
					SqlCommandString += String.Format(" @ProcessorCountMultiplier='{0}',", BuildConfiguration.ProcessorCountMultiplier );
					SqlCommandString += String.Format(" @bUseUnityBuild='{0}',", BuildConfiguration.bUseUnityBuild );
					SqlCommandString += String.Format(" @bStressTestUnity='{0}',", BuildConfiguration.bStressTestUnity );
					SqlCommandString += String.Format(" @bCheckSystemHeadersForModification='{0}',", BuildConfiguration.bCheckSystemHeadersForModification);
					SqlCommandString += String.Format(" @bCheckExternalHeadersForModification='{0}',", BuildConfiguration.bCheckExternalHeadersForModification);
					SqlCommandString += String.Format(" @bUsePDBFiles='{0}',", BuildConfiguration.bUsePDBFiles);
					SqlCommandString += String.Format(" @bUsePCHFiles='{0}',", BuildConfiguration.bUsePCHFiles);
					SqlCommandString += String.Format(" @bPrintDebugInfo='{0}',", BuildConfiguration.bPrintDebugInfo );
					SqlCommandString += String.Format(" @bLogDetailedActionStats='{0}',", BuildConfiguration.bLogDetailedActionStats);
					SqlCommandString += String.Format(" @bAllowXGE='{0}',", BuildConfiguration.bAllowXGE );
					SqlCommandString += String.Format(" @bShowXGEMonitor='{0}',", BuildConfiguration.bShowXGEMonitor );
					SqlCommandString += String.Format(" @bShouldDeleteAllOutdatedProducedItems='{0}',", BuildConfiguration.bShouldDeleteAllOutdatedProducedItems );
					SqlCommandString += String.Format(" @bUseIncrementalLinking='{0}',", BuildConfiguration.bUseIncrementalLinking );
					SqlCommandString += String.Format(" @bSupportEditAndContinue='{0}',", BuildConfiguration.bSupportEditAndContinue );
					SqlCommandString += String.Format(" @bUseIntelCompiler='{0}',", BuildConfiguration.bUseIntelCompiler);
					SqlCommandString += String.Format(" @bUseSNCCompiler='{0}'", true);
					
					// Execute stored procedure adding build summary information.
					SqlCommand SendSummaryCommand = new SqlCommand( SqlCommandString, PerfDBConnection);
					SendSummaryCommand.ExecuteNonQuery();

					// We're done, close the connection.
					PerfDBConnection.Close();
				}
			}
			// Catch exceptions here instead of at higher level as we don't care if DB connection fails.
			catch (Exception)
			{
				Console.WriteLine("Could not connect to database.");
			}
		}
	}
}
