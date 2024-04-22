// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.Linq;
using System.Linq;
using System.Web;

using CISStatus.Controllers;

namespace CISStatus.Models
{
	public class BranchConfigsDatabase
	{
		private BranchConfigsDataContext BranchConfigsData = null;

		public BranchConfigsDatabase()
		{
			BranchConfigsData = new BranchConfigsDataContext();
		}

		public void Dispose()
		{
			BranchConfigsData.Dispose();
		}

		public List<BranchInfo> GetCISBranches()
		{
			IQueryable<BranchInfo> Branches =
			(
				from BranchDetail in BranchConfigsData.BranchConfigs
				where BranchDetail.LastGoodOverall > 0
				select new BranchInfo( BranchDetail.ID, BranchDetail.Branch )
			);

			return Branches.ToList();
		}

		public List<BranchInfo> GetAllBranches()
		{
			IQueryable<BranchInfo> Branches =
			(
				from BranchDetail in BranchConfigsData.BranchConfigs
				where BranchDetail.DisplayOrder > 0
				orderby BranchDetail.DisplayOrder ascending
				select new BranchInfo( BranchDetail.ID, BranchDetail.Branch )
			);

			return Branches.ToList();
		}
	}

	public class ChangelistsDatabase
	{
		private ChangelistsDataContext ChangelistsData = null;

		public ChangelistsDatabase()
		{
			ChangelistsData = new ChangelistsDataContext();
		}

		public void Dispose()
		{
			ChangelistsData.Dispose();
		}

		public List<ChangelistInfo> GetChanges( int BranchConfigID )
		{
			// Get the list of changes for the past 36 hours in this branch
			IQueryable<ChangelistInfo> Changelists =
			(
				from ChangelistDetail in ChangelistsData.Changelists2s
				where ChangelistDetail.BranchConfigID == BranchConfigID 
				orderby ChangelistDetail.Changelist descending
				select new ChangelistInfo( ChangelistDetail.ID,
										ChangelistDetail.Changelist,
										ChangelistDetail.BuildStatus,
										ChangelistDetail.Submitter,
										ChangelistDetail.Description,
										ChangelistDetail.TimeStamp )
			).Take( 50 );

			// Fill in the CIS task entries
			List<ChangelistInfo> ListOfChanges = Changelists.ToList();

			foreach( ChangelistInfo Changelist in ListOfChanges )
			{
				IQueryable<JobStateInfo> JobStates =
				(
					from JobStateDetail in ChangelistsData.CISJobStates
					where JobStateDetail.ChangelistID == Changelist.ID
					orderby JobStateDetail.CISTask.Mask
					select new JobStateInfo( JobStateDetail.CISTask.Name, ( JobState )JobStateDetail.JobState, JobStateDetail.Error )
				);

				Changelist.SetJobStates( JobStates.ToList() );
			}

			return ListOfChanges;
		}
	}

	public class BuildStatusesDatabase
	{
		private CommandsDataContext BuildStatusesData = null;

		public BuildStatusesDatabase()
		{
			BuildStatusesData = new CommandsDataContext();
		}

		public void Dispose()
		{
			BuildStatusesData.Dispose();
		}

		public List<BuildInfo> GetBuildStatuses( int BranchConfigID, string Email )
		{
			IQueryable<BuildInfo> BuildStatuses =
			(
				from BuildStatusDetail in BuildStatusesData.Commands
				where BuildStatusDetail.BranchConfigID == BranchConfigID
				where BuildStatusDetail.PrimaryBuild == true
				where BuildStatusDetail.LastAttemptedDateTime.AddDays( 30 ) > DateTime.UtcNow
				where BuildStatusDetail.Access >= 1000 && BuildStatusDetail.Access < 100000
				let Subscriptions = (
						from SubscriptionDetail in BuildStatusesData.Subscriptions 
						where 
							SubscriptionDetail.CommandID == BuildStatusDetail.ID && 
							SubscriptionDetail.Email == Email
						select 
							SubscriptionDetail.Type
					)
				orderby BuildStatusDetail.Access ascending
				select new BuildInfo( BuildStatusDetail.ID, BuildStatusDetail.Description, BuildStatusDetail.Access,
										BuildStatusDetail.NextTrigger, BuildStatusDetail.Pending, BuildStatusDetail.Machine,
										BuildStatusDetail.LastAttemptedChangeList, BuildStatusDetail.LastAttemptedDateTime,
										BuildStatusDetail.LastGoodChangeList, BuildStatusDetail.LastGoodDateTime,
										BuildStatusDetail.LastFailedChangeList, BuildStatusDetail.LastFailedDateTime,
										Subscriptions.Contains( ( byte )'T' ),
										Subscriptions.Contains( ( byte )'S' ),
										Subscriptions.Contains( ( byte )'F' )
									)
			);

			return BuildStatuses.ToList();
		}

		public List<VerificationInfo> GetVerifications( string Email )
		{
			IQueryable<VerificationInfo> Verifications =
			(
				from VerificationDetail in BuildStatusesData.Commands
				where VerificationDetail.Game == "#VerificationLabel"
				let Subscriptions = (
						from SubscriptionDetail in BuildStatusesData.Subscriptions
						where
							SubscriptionDetail.CommandID == VerificationDetail.ID &&
							SubscriptionDetail.Email == Email
						select
							SubscriptionDetail.Type
					)
				orderby VerificationDetail.Access ascending
				select new VerificationInfo( VerificationDetail.ID, VerificationDetail.Description,
											VerificationDetail.LastAttemptedChangeList, VerificationDetail.LastAttemptedDateTime,
											VerificationDetail.LastGoodChangeList, VerificationDetail.LastGoodDateTime,
											VerificationDetail.LastFailedChangeList, VerificationDetail.LastFailedDateTime,
											Subscriptions.Contains( ( byte )'T' ),
											Subscriptions.Contains( ( byte )'S' ),
											Subscriptions.Contains( ( byte )'F' )
											)
			);

			return Verifications.ToList();
		}

		public void InsertSubscription( int CommandID, string Email, char SubscriptionType )
		{
			Subscription NewRecord = new Subscription();
			NewRecord.CommandID = CommandID;
			NewRecord.Email = Email;
			NewRecord.Type = ( byte )SubscriptionType;
			BuildStatusesData.Subscriptions.InsertOnSubmit( NewRecord );
			BuildStatusesData.SubmitChanges();
		}

		public void DeleteSubscription( int CommandID, string Email, char SubscriptionType )
		{
			IQueryable<Subscription> Subs = 
			( 
				from SubscriptionDetail in BuildStatusesData.Subscriptions
				where SubscriptionDetail.CommandID == CommandID && SubscriptionDetail.Email == Email && SubscriptionDetail.Type == ( byte )SubscriptionType
				select SubscriptionDetail 
			);

			foreach( Subscription Sub in Subs )
			{
				BuildStatusesData.Subscriptions.DeleteOnSubmit( Sub );
			}
			BuildStatusesData.SubmitChanges();
		}
	}
}