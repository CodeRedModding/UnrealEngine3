// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using CISMonitorService.Data;

namespace Builder.CISMonitor.Data
{
	public class Database
	{
		private ChangelistsDataContext ChangelistsData = null;

		public Database()
		{
			ChangelistsData = new ChangelistsDataContext();
		}

		public void Dispose()
		{
			ChangelistsData.Dispose();
		}

		public List<ChangelistInfo> GetChanges( string BranchName )
		{
			IQueryable<ChangelistInfo> Changelists =
			(
				from ChangelistDetail in ChangelistsData.Changelists2s
				join BranchConfigDetail in ChangelistsData.BranchConfigs on ChangelistDetail.BranchConfigID equals BranchConfigDetail.ID
				where BranchConfigDetail.Branch == BranchName && ChangelistDetail.TimeStamp.AddHours( 72 ) > DateTime.Now
				orderby ChangelistDetail.Changelist descending
				select new ChangelistInfo( ChangelistDetail.Changelist,
										ChangelistDetail.BuildStatus,
										ChangelistDetail.Submitter.ToLower(),
										ChangelistDetail.Description,
										ChangelistDetail.TimeStamp )
			);

			return Changelists.ToList();
		}

		public List<string> GetCISBranches()
		{
			IQueryable<string> Branches =
			(
				from BranchConfigDetail in ChangelistsData.BranchConfigs
				where BranchConfigDetail.LastGoodOverall > 0
				select BranchConfigDetail.Branch
			);

			return Branches.ToList();
		}
	}
}
