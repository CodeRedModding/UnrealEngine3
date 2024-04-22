// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Builder.UnrealSyncService;

namespace Builder.UnrealSyncService.Models
{
	public class Database
	{
		/// <summary>
		/// Get all the Perforce servers the current build system accesses
		/// </summary>
		public static List<string> GetPerforceServers()
		{
			using( BranchConfigsDataContext BranchData = new BranchConfigsDataContext() )
			{
				IQueryable<string> Branches =
				(
					from Branch in BranchData.BranchConfigs
					select Branch.Server
				).Distinct();

				return Branches.ToList();
			}
		}

		/// <summary>
		/// Extract all the information we care about in the database for the branches
		/// </summary>
		public static Dictionary<string, BuilderMonitor.BranchDefinition> GetBranchDefinitions()
		{
			List<BuilderMonitor.BranchDefinition> BranchDefinitions = new List<BuilderMonitor.BranchDefinition>();

			// Grab the info for all branches
			using( BranchConfigsDataContext BranchData = new BranchConfigsDataContext() )
			{
				BranchDefinitions =
				(
					from BranchDetail in BranchData.BranchConfigs
					select new BuilderMonitor.BranchDefinition( BranchDetail.Branch, 
																BranchDetail.ID, 
																BranchDetail.Version,
																BranchDetail.LastGoodOverall,
																BranchDetail.HeadChangelist )
				).ToList();
			}

			// Augment with data from the variables table
			using( VariablesDataContext VariableData = new VariablesDataContext() )
			{
				foreach( BuilderMonitor.BranchDefinition BranchDef in BranchDefinitions )
				{
					BranchDef.LatestBuildLabel = 
					(
						from VariableDetail in VariableData.Variables
						where VariableDetail.Variable1 == "LatestBuild"
						where VariableDetail.BranchConfigID == BranchDef.BranchConfigID
						select VariableDetail.Value
					).ToList().DefaultIfEmpty( "" ).First();

					BranchDef.LatestQABuildLabel =
					(
						from VariableDetail in VariableData.Variables
						where VariableDetail.Variable1 == "LatestApprovedQABuild"
						where VariableDetail.BranchConfigID == BranchDef.BranchConfigID
						select VariableDetail.Value
					).ToList().DefaultIfEmpty( "" ).First();
				}
			}

			// Augment even more with promoted labels
			using( CommandsDataContext CommandData = new CommandsDataContext() )
			{
				foreach( BuilderMonitor.BranchDefinition BranchDef in BranchDefinitions )
				{
					BranchDef.PromotableGameLabels =
					(
						from CommandDetail in CommandData.Commands
						where CommandDetail.Type == 3
						where CommandDetail.BranchConfigID == BranchDef.BranchConfigID
						select new BuilderMonitor.GameLabelPair( CommandDetail.Game, CommandDetail.LatestApprovedLabel )
					).ToList();
				}
			}

			// Return a dictionary of the results for quick lookup
			return BranchDefinitions.ToDictionary( x => x.DepotName, x => x );
		}
	}
}
