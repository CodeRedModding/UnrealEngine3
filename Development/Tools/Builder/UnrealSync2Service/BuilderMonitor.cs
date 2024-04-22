// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Xml.Serialization;

using Builder.UnrealSyncService.Models;

namespace Builder.UnrealSyncService
{
	public class BuilderMonitor
	{
		private Thread MonitorThread = null;
		private PerforceServers Servers = new PerforceServers();
		private Dictionary<string, BranchDefinition> BranchDefinitions = new Dictionary<string, BranchDefinition>();

		private static string ToXmlString<T>( T Data )
		{
			XmlSerializer Serialiser = new XmlSerializer( Data.GetType() );

			XmlSerializerNamespaces EmptyNameSpace = new XmlSerializerNamespaces();
			EmptyNameSpace.Add( "", "http://www.w3.org/2001/XMLSchema" );

			using( StringWriter Writer = new StringWriter() )
			{
				Serialiser.Serialize( Writer, Data, EmptyNameSpace );
				return Writer.ToString();
			}
		}

		/// <summary>
		/// Container for a game and its current promoted label
		/// </summary>
		public class GameLabelPair
		{
			public string GameName { get; set; }
			public string Label { get; set; }

			/// <summary>
			/// Default empty constructor for Xml serialisation
			/// </summary>
			public GameLabelPair()
			{
			}

			public GameLabelPair( string InGameName, string InLabel )
			{
				GameName = InGameName;
				Label = InLabel;
			}
		}

		/// <summary>
		/// A container for the available Perforce servers
		/// </summary>
		public class PerforceServers
		{
			[XmlAttribute]
			public List<string> ServerNames = new List<string>();

			/// <summary>
			/// Default empty constructor for Xml serialisation
			/// </summary>
			public PerforceServers()
			{
			}

			public PerforceServers( List<String> InServerNames )
			{
				ServerNames = InServerNames;
			}

			/// <summary>
			/// Convert the class into a serialisable Xml string
			/// </summary>
			public override string ToString()
			{
				return ToXmlString<PerforceServers>( this );
			}
		}

		/// <summary>
		/// A container for the branch info stored in the database
		/// </summary>
		public class BranchDefinition
		{
			[XmlAttribute]
			public string DepotName = "";
			[XmlAttribute]
			public int BranchConfigID = 0;
			[XmlAttribute]
			public int Version = 1;
			[XmlAttribute]
			public int LatestGoodCISChangelist = 0;
			[XmlAttribute]
			public int HeadChangelist = 0;
			[XmlAttribute]
			public string LatestBuildLabel = "";
			[XmlAttribute]
			public string LatestQABuildLabel = "";
			[XmlElement]
			public List<GameLabelPair> PromotableGameLabels = new List<GameLabelPair>();
			[XmlIgnore]
			public DateTime LastUpdated = DateTime.MinValue;

			/// <summary>
			/// Default empty constructor for Xml serialisation
			/// </summary>
			public BranchDefinition()
			{
			}

			public BranchDefinition( string InDepotName, int InBranchConfigID, int InVersion, int InLatestGoodCISChangelist, int InHeadChangelist )
			{
				DepotName = InDepotName;
				BranchConfigID = InBranchConfigID;
				Version = InVersion;
				LatestGoodCISChangelist = InLatestGoodCISChangelist;
				HeadChangelist = InHeadChangelist;
			}

			/// <summary>
			/// Convert the class into a serialisable Xml string
			/// </summary>
			public override string ToString()
			{
				return ToXmlString<BranchDefinition>( this );
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BuilderMonitor()
		{
			MonitorThread = new Thread( new ThreadStart( MonitorProc ) );
			MonitorThread.Start();
		}

		/// <summary>
		/// Abort the thread
		/// </summary>
		public void Release()
		{
			if( MonitorThread != null )
			{
				MonitorThread.Abort();
				MonitorThread = null;
			}
		}

		/// <summary>
		/// Return the list of Perforce servers in a thread safe manner
		/// </summary>
		public string GetPerforceServers()
		{
			string PerforceServersString = "";

			lock( Servers )
			{
				PerforceServersString = Servers.ToString();
			}

			return PerforceServersString;
		}

		/// <summary>
		/// Return an XML description of the branch
		/// </summary>
		public string GetBranch( string BranchName )
		{
			lock( BranchDefinitions )
			{
				BranchDefinition Branch = null;
				if( BranchDefinitions.TryGetValue( BranchName, out Branch ) )
				{
					return Branch.ToString();
				}
			}

			return "";
		}

		/// <summary>
		/// Main database thread. Periodically poll the database for information.
		/// </summary>
		private void MonitorProc()
		{
			while( true )
			{
#if !DEBUG
				try
				{
#endif
					// Create a link to the database
					Database Data = new Database();

					// Get the active Perforce servers
					Servers = new PerforceServers( Database.GetPerforceServers() );

					// Fill out all the details for each branch
					BranchDefinitions = Database.GetBranchDefinitions();

					// Small pause to give the system room to breathe
					Thread.Sleep( 15 * 1000 );
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
