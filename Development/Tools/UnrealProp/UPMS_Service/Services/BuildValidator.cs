using System;
using System.Collections.Generic;
using System.Data;
using System.IO;
using System.Text;
using System.Threading;
using RemotableType;
using RemotableType.PlatformBuildFilesTableAdapters;

namespace UnrealProp
{
	public static class BuildValidator
	{
		static Random Rnd = new Random( 5000 );
		static Thread Thread;

		static public void Init()
		{
			Thread = new Thread( new ThreadStart( ValidatingProc ) );
			Thread.Start();
			Log.WriteLine( "UPMS BUILD VALIDATOR", Log.LogType.Important, "Initialised!" );
		}

		static public void Release()
		{
			if( Thread != null )
			{
				Thread.Abort();
				Thread = null;
			}
		}

		// Check for a build being mentioned only once in the database
		static bool CheckUniqueBuild( DataRow Row )
		{
			bool Unique = true;

			using( DataSet Build = DataHelper.GetDataSet( "SELECT * FROM [PlatformBuilds] WHERE ( Path = '" + Row["Path"] + "' )" ) )
			{
				if( Build.Tables[0].Rows.Count > 1 )
				{
					Unique = false;

					DataRow BuildRow = Build.Tables[0].Rows[1];
					long PlatformBuildID = Convert.ToInt64( BuildRow["ID"] );
					DataHelper.PlatformBuild_Delete( PlatformBuildID );
				}
			}

			return ( Unique );
		}

		// Ensure the build path exists, and delete from db if it doesn't
		static bool CheckNonExistent( DataRow Row )
		{
			bool Valid = true;

			string Path = ( string )Row["Path"];
			DirectoryInfo DirInfo = new DirectoryInfo( Path );
			if( !DirInfo.Exists )
			{
				Valid = false;

				long PlatformBuildID = Convert.ToInt64( Row["ID"] );
				DataHelper.PlatformBuild_Delete( PlatformBuildID );
			}

			return ( Valid );
		}

        // Main thread to validate builds
		static void ValidatingProc()
		{
			Thread.Sleep( 1000 );

			int StatusReady = ( int )( RemotableType.BuildStatus.Ready );
			int StatusArchived = ( int )( RemotableType.BuildStatus.Archived );

			while( true )
			{
#if !DEBUG
				try
				{
#endif
					// Search for duplicated builds
					using( DataSet AllBuilds = DataHelper.GetDataSet( "SELECT TOP( 1 ) * FROM [PlatformBuilds] " +
																		"WHERE ( ( StatusID = " + StatusReady.ToString() + " ) OR ( StatusID = " + StatusArchived.ToString() + " ) ) " +
																		"AND ( Special = 0 ) ORDER BY NEWID()" ) )
					{
						if( AllBuilds.Tables[0].Rows.Count > 0 )
						{
							DataRow Row = AllBuilds.Tables[0].Rows[0];
							if( CheckUniqueBuild( Row ) )
							{
								CheckNonExistent( Row );
							}
						}
					}

					// 50-70 sec interval
					Thread.Sleep( Rnd.Next( 50000, 70000 ) );

					// Search for builds that have been hidden for more than 3 days
					DataHelper.ExecuteNonQuery( "UPDATE [PlatformBuilds] SET StatusID = 6 WHERE ( StatusID = 5 ) AND ( DATEDIFF( day, DiscoveryTime, GETDATE() ) > 3 ) AND ( Special = 0 )" );

					// 50-70 sec interval
					Thread.Sleep( Rnd.Next( 50000, 70000 ) );
#if !DEBUG
				}
				catch( Exception Ex )
				{
					if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
					{
						Log.WriteLine( "UPMS BUILD VALIDATOR", Log.LogType.Error, "Unhandled exception while interrogating database: " + Ex.ToString() );
					}
				}
#endif
			}
		}
	}
}
