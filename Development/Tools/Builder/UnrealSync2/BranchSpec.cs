// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Windows.Forms;
using System.Xml.Serialization;

namespace Builder.UnrealSync
{
	public class BranchSpec
	{
		// Persistent info
		[XmlAttribute]
		public string Server = "";
		[XmlAttribute]
		public string UserName = "";
		[XmlAttribute]
		public string ClientSpec = "";
		[XmlAttribute]
		public string Name = "";
		[XmlAttribute]
		public string DepotName = "";
		[XmlAttribute]
		public string Root = "";
		[XmlAttribute]
		public ESyncType SyncType = ESyncType.Head;
		[XmlAttribute]
		public DateTime SyncTime = DateTime.MaxValue;
		[XmlAttribute]
		public string GameName = "";
		[XmlElement]
		public List<PromotableGame> PromotableGames;
		[XmlAttribute]
		public bool bDisplayInMenu = true;
		[XmlAttribute]
		public bool bShowBalloon = true;

		// Info used at run time
		[XmlIgnore]
		public int Version = 1;
		[XmlIgnore]
		public int BranchConfigID = 0;
		[XmlIgnore]
		public TimeSpan SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
		[XmlIgnore]
		public bool bIsMain = false;
		[XmlIgnore]
		public ESyncType ImmediateSyncType = ESyncType.Head;
		[XmlIgnore]
		public int HeadChangelist = -1;
		[XmlIgnore]
		public int LatestGoodCISChangelist = -1;
		[XmlIgnore]
		public string LatestBuildLabel = "";
		[XmlIgnore]
		public string LatestQABuildLabel = "";
		[XmlIgnore]
		public ToolStripMenuItem MenuItem = null;
		[XmlIgnore]
		public bool bNewBuild = false;
		[XmlIgnore]
		public bool bNewQABuild = false;

		/// <summary>
		/// Empty constructor (Xml serialisation requirement)
		/// </summary>
		public BranchSpec()
		{
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		public BranchSpec( BranchSpec Other )
		{
			Server = Other.Server;
			UserName = Other.UserName;
			ClientSpec = Other.ClientSpec;
			Name = Other.Name;
			DepotName = Other.DepotName;
			Root = Other.Root;
			SyncType = Other.SyncType;
			SyncTime = Other.SyncTime;
			GameName = Other.GameName;

			PromotableGames = new List<PromotableGame>();
			foreach( PromotableGame Game in Other.PromotableGames )
			{
				PromotableGames.Add( new PromotableGame( Game ) );
			}

			bDisplayInMenu = Other.bDisplayInMenu;
			bShowBalloon = Other.bShowBalloon;

			Version = Other.Version;
			BranchConfigID = Other.BranchConfigID;
			SyncRandomisation = Other.SyncRandomisation;
			bIsMain = Other.bIsMain;
			ImmediateSyncType = Other.ImmediateSyncType;
			HeadChangelist = Other.HeadChangelist;
			LatestGoodCISChangelist = Other.LatestGoodCISChangelist;
			LatestBuildLabel = Other.LatestBuildLabel;
			LatestQABuildLabel = Other.LatestQABuildLabel;
			MenuItem = Other.MenuItem;
			bNewBuild = Other.bNewBuild;
			bNewQABuild = Other.bNewQABuild;
		}

		public BranchSpec( string InServer, string InUserName, string InClientSpec, string InName, string InDepotName, string InRoot )
		{
			Server = InServer;
			UserName = InUserName;
			ClientSpec = InClientSpec;
			Name = InName;
			DepotName = InDepotName;
			Root = InRoot;
			PromotableGames = new List<PromotableGame>();

			bIsMain = ( DepotName.ToLower() == "unrealengine3" ) || ( DepotName.ToLower() == "ue4" );
		}

		public string MRUString()
		{
			string Abbreviated = ClientSpec;
			Abbreviated += "/";
			Abbreviated += Name.Replace( "UnrealEngine3", "UE3" );
			Abbreviated += "/";
			Abbreviated += SyncType.ToString();

			if( SyncType == ESyncType.ArtistSyncGame || SyncType == ESyncType.LaunchEditor )
			{
				Abbreviated += "/";
				Abbreviated += GameName;
			}

			return Abbreviated;
		}

		public string DialogString()
		{
			return ( Server.Replace( ':', '-' ) + "_" + ClientSpec + "_" + Name );
		}
	}
}