// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Xml.Serialization;

using Builder.UnrealSyncService;

namespace Builder.UnrealSync
{
	public class WebService
	{
		private static Dictionary<string, BuilderMonitor.BranchDefinition> BranchDefinitions = new Dictionary<string, BuilderMonitor.BranchDefinition>();

		private static T FromXmlString<T>( string XmlString )
		{
			XmlSerializer Serialiser = new XmlSerializer( typeof( T ) );
			using( StringReader Reader = new StringReader( XmlString ) )
			{
				return ( T )Serialiser.Deserialize( Reader );
			}
		}

		private static string GetResponseString( HttpWebResponse Response )
		{
			string ResponseString = "";

			if( Response.ContentLength < 2048 )
			{
				// Process the response
				Stream ResponseStream = Response.GetResponseStream();
				byte[] RawResponse = new byte[Response.ContentLength];
				ResponseStream.Read( RawResponse, 0, ( int )Response.ContentLength );
				ResponseStream.Close();

				// Convert response into a string
				ResponseString = Encoding.UTF8.GetString( RawResponse );
			}

			return ResponseString;
		}

		/// <summary>
		/// Get a list of Perforce servers used by the build system via the web service
		/// </summary>
		public static List<string> GetPerforceServers( string WebServiceURL )
		{
			BuilderMonitor.PerforceServers Servers = new BuilderMonitor.PerforceServers();
			try
			{
				// Send off the request - e.g. http://devweb-02:2827/UnrealSync/GetPerforceServers
				string RequestString = "http://" + WebServiceURL + ":2827/UnrealSync/GetPerforceServers";
				HttpWebRequest Request = ( HttpWebRequest )WebRequest.Create( RequestString );
				HttpWebResponse Response = ( HttpWebResponse )Request.GetResponse();

				string ResponseString = GetResponseString( Response );
				if( ResponseString.Length > 0 )
				{
					// Convert response into a list of branches
					Servers = FromXmlString<BuilderMonitor.PerforceServers>( ResponseString );
				}
			}
			catch
			{
			}

			return Servers.ServerNames;
		}

		/// <summary>
		/// Update the branch with info from the database via the webservice
		/// </summary>
		private static BuilderMonitor.BranchDefinition UpdateBranch( string WebServiceURL, string DepotName )
		{
			BuilderMonitor.BranchDefinition BranchInfo = null;
			try
			{
				string RequestString = "http://" + WebServiceURL + ":2827/UnrealSync/GetBranch/" + DepotName;
				HttpWebRequest Request = ( HttpWebRequest )WebRequest.Create( RequestString );
				HttpWebResponse Response = ( HttpWebResponse )Request.GetResponse();

				string ResponseString = GetResponseString( Response );

				if( ResponseString.Length > 0 )
				{
					BranchInfo = FromXmlString<BuilderMonitor.BranchDefinition>( ResponseString );
					BranchInfo.LastUpdated = DateTime.UtcNow;
				}

				BranchDefinitions[BranchInfo.DepotName] = BranchInfo;
			}
			catch
			{
			}

			return BranchInfo;
		}

		/// <summary>
		/// Clear out all cached data to ensure a full refresh
		/// </summary>
		public static void ClearCache()
		{
			BranchDefinitions = new Dictionary<string, BuilderMonitor.BranchDefinition>();
		}

		public static void PopulateBranch( string WebServiceURL, BranchSpec Branch )
		{
			// See if we have a cached copy
			BuilderMonitor.BranchDefinition BranchInfo;
			if( !BranchDefinitions.TryGetValue( Branch.DepotName, out BranchInfo ) )
			{
				BranchInfo = UpdateBranch( WebServiceURL, Branch.DepotName );
				if( BranchInfo == null )
				{
					return;
				}
			}

			// Early out if the branch is not being built by the build system
			if( BranchInfo == null )
			{
				return;
			}

			// See if we've updated really recently
			if( ( DateTime.UtcNow - BranchInfo.LastUpdated ).TotalSeconds > 10 )
			{
				BranchInfo = UpdateBranch( WebServiceURL, Branch.DepotName );
				if( BranchInfo == null )
				{
					return;
				}
			}

			// Copy out the pertinent information
			Branch.BranchConfigID = BranchInfo.BranchConfigID;
			Branch.Version = BranchInfo.Version;
			Branch.LatestGoodCISChangelist = BranchInfo.LatestGoodCISChangelist;
			Branch.HeadChangelist = BranchInfo.HeadChangelist;

			Branch.bNewBuild = ( Branch.LatestBuildLabel != BranchInfo.LatestBuildLabel );
			Branch.LatestBuildLabel = BranchInfo.LatestBuildLabel;

			Branch.bNewQABuild = ( Branch.LatestQABuildLabel != BranchInfo.LatestQABuildLabel );
			Branch.LatestQABuildLabel = BranchInfo.LatestQABuildLabel;

			foreach( PromotableGame Game in Branch.PromotableGames )
			{
				foreach( BuilderMonitor.GameLabelPair GameLabelPair in BranchInfo.PromotableGameLabels )
				{
					if( Game.GameName == GameLabelPair.GameName )
					{
						Game.bNewPromotion = ( Game.PromotedLabel != GameLabelPair.Label );
						Game.PromotedLabel = GameLabelPair.Label;
						break;
					}
				}
			}
		}
	}
}
