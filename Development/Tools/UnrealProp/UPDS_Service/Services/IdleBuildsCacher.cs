/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using RemotableType;

namespace UnrealProp
{
    // Working thread for caching newest builds during idle
    public static class IdleBuildsCacher
    {
        static Thread Thread;
        static object SyncObj = new Object();

        static public void Init()
        {
            Thread = new Thread( new ThreadStart( IdleProc ) );
            Thread.Start();
        }

        static public void Release()
        {
            if( Thread != null )
            {
                Thread.Abort();
                Thread = null;
            }
        }

        static void CacheBuilds()
        {
            // Get a list of all the builds
            PlatformBuilds Builds = UPDS_Service.IUPMS.PlatformBuild_GetListForProjectPlatformAndStatus( ( short )-1, ( short )-1, ( short )BuildStatus.Ready );

            int BuildsToCache = 3;

            // Precache the newest builds
            foreach( PlatformBuilds.PlatformBuildsRow Row in Builds.Tables[0].Rows )
            {
                string RepositoryPath = UPDS_Service.IUPMS.PlatformBuild_GetRepositoryPath( Row.ID );
                PlatformBuildFiles FilesToCache = UPDS_Service.IUPMS.PlatformBuild_GetFiles( Row.ID );
                CacheSystem.PrecacheBuild( FilesToCache, RepositoryPath, true );

                Log.WriteLine( "UPDS IDLE BUILDS CACHER", Log.LogType.Info, "Precache build: " + Row.Platform.Trim() + " / " + Row.Project.Trim() + " / " + Row.Title );

                BuildsToCache--;
                if( BuildsToCache == 0 )
                {
                    break;
                }
            }
        }

        // thread proc
        static void IdleProc()
        {
            Thread.Sleep( 60 * 1000 );

            while( true )
            {
#if !DEBUG
                try
                {
#endif
					int NumberOfFilesToCache = CacheSystem.GetNumberFilesToForceCache();
					Log.WriteLine( "UPDS IDLE BUILDS CACHER", Log.LogType.Info, "Tick. (" + NumberOfFilesToCache.ToString() + " files to cache, and " + TaskExecutor.TaskCount.ToString() + " tasks)" );

                    // Check for no builds to force cache
					if( NumberOfFilesToCache == 0 )
                    {
						if( TaskExecutor.TaskCount == 0 )
						{
							// Find any orphaned files and delete them
							CacheSystem.DeleteOrphans();
						}

                        // 5 minute gap between cache attempts
                        Thread.Sleep( 5 * 60 * 1000 );

						if( TaskExecutor.TaskCount == 0 )
						{
							// Delete oldest files if the cache is greater than the requested size
							CacheSystem.DeleteOldFiles();
						}

                        // 5 minute gap between cache attempts
                        Thread.Sleep( 5 * 60 * 1000 );

						if( TaskExecutor.TaskCount == 0 )
						{
							// Idly cache the newest builds
							CacheBuilds();
						}
                    }

					// 5 minute gap between cache attempts
					Thread.Sleep( 5 * 60 * 1000 );
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
					{
						Log.WriteLine( "UPDS IDLE BUILD CACHER", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
					}
				}
#endif
            }
        }
    }
}
