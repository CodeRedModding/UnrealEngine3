// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Principal;
using System.ServiceModel;
using System.Web;
using System.Web.Mvc;

using CISStatus.Models;

namespace CISStatus.Controllers
{
	public enum JobState
	{
		Unneeded = 0,
		Pending,
		Optional,
		InProgress,
		InProgressOptional,
		Succeeded,
		Failed
	}

	public class BranchInfo
	{
		// The SQL FK ID
		public int ID = 0;

		// The name of the branch
		public string Name = "";

		public BranchInfo( int InID, string InName )
		{
			ID = InID;
			Name = InName;
		}
	}

	public class JobStateInfo
	{
		// The name of the CIS task
		public string TaskName = "";

		// The current state of the job
		public JobState StateOfJob = JobState.Unneeded;

		// The error for this task if it failed
		public string ErrorMessage = "";

		public JobStateInfo( string InTaskName, JobState InJobState, string InErrorMessage )
		{
			TaskName = InTaskName;
			StateOfJob = InJobState;
			ErrorMessage = InErrorMessage;
		}

		public string GetError()
		{
			return ErrorMessage;
		}
	}

	public class ChangelistInfo
	{
		// The SQL FK ID
		public int ID = 0;

		// The id of the change
		public int ChangeNumber = 0;

		// A bitwise representation of the state of the build
		public int BuildStatus = 0;

		// The state of each CIS job spawned for the changelist
		public List<JobStateInfo> JobStates = new List<JobStateInfo>();

		// The Perforce user that checked in this change
		public string User = "";
		// The Perforce description of this change
		public string Description = "";
		// The Perforce timestamp of the changelist
		public DateTime TimeStamp = DateTime.UtcNow;

		public ChangelistInfo( int InID, int InChangeNumber, int? InBuildStatus, string InUser, string InDescription, DateTime InTimeStamp )
		{
			ID = InID;
			ChangeNumber = InChangeNumber;

			User = InUser;
			Description = InDescription;
			TimeStamp = InTimeStamp.ToLocalTime();

			if( InBuildStatus.HasValue )
			{
				BuildStatus = ( int )InBuildStatus;
			}
		}

		public void SetJobStates( List<JobStateInfo> InJobStates )
		{
			JobStates = InJobStates;
		}

		public string GetP4WebLink()
		{
			return "http://p4-web/@md=d&cd=//depot/&c=0CK@/" + ChangeNumber.ToString() + "?ac=10";
		}

		public string GetBackground()
		{
			string Colour = "#ffffff";
			if( BuildStatus == 0 )
			{
				Colour = "#ffff00";
			}
			else if( BuildStatus != -1 )
			{
				Colour = "#ff0000";
			}

			return Colour;
		}

		public string GetBuildStatus()
		{
			if( BuildStatus < 0 )
			{
				return "-";
			}

			return Convert.ToString( BuildStatus, 2 );
		}

		public string GetRowBackground()
		{
			string Colour = "#ffffff";
			if( ( TimeStamp.AddHours( -2 ).DayOfYear & 1 ) == 1 )
			{
				Colour = "#f8f8f8";
			}
			return "background-color:" + Colour;
		}

		public string GetImage( JobState State )
		{
			string Image = "";
			switch( State )
			{
			case JobState.Unneeded:
				Image = "dash.png";
				break;
			case JobState.Pending:
				Image = "query.png";
				break;
			case JobState.Optional:
				Image = "query2.png";
				break;
			case JobState.InProgress:
				Image = "spinner.gif";
				break;
			case JobState.InProgressOptional:
				Image = "spinner2.gif";
				break;
			case JobState.Succeeded:
				Image = "tick.png";
				break;
			case JobState.Failed:
				Image = "cross.png";
				break;
			}

			return Image;
		}

		public string GetChangeNumber()
		{
			return ChangeNumber.ToString();
		}

		public string GetUser()
		{
			return User;
		}

		public string GetDescription()
		{
			return Description;
		}
	}

    public class CISStatusController : Controller
    {
		private BranchInfo FindBranch( GeneralViewModel Model, string BranchName )
		{
			foreach( BranchInfo Branch in Model.Branches )
			{
				if( BranchName == Branch.Name )
				{
					return Branch;
				}
			}

			return null;
		}

		private void SetCurrentBranchInfo( GeneralViewModel Model, string BranchName )
		{
			BranchConfigsDatabase Data = new BranchConfigsDatabase();

			Model.Branches = Data.GetCISBranches();

			Data.Dispose();

			Model.CurrentBranchInfo = FindBranch( Model, BranchName );

			// Set the default branch name if it is valid
			if( Model.CurrentBranchInfo == null )
			{
				Model.CurrentBranchInfo = FindBranch( Model, "UnrealEngine3" );
			}
		}

        public ActionResult Index( string BranchName = "UnrealEngine3" )
        {
			// Create a container for our view data
			GeneralViewModel Model = new GeneralViewModel();

			SetCurrentBranchInfo( Model, BranchName );

            return View( Model );
        }

		public ActionResult GetChangelists( string CurrentBranchName )
		{
			// Create a container for our view data
			GeneralViewModel Model = new GeneralViewModel();

			// Create a link to the database
			ChangelistsDatabase Data = new ChangelistsDatabase();

			SetCurrentBranchInfo( Model, CurrentBranchName );

			// Get the list of unique changes 
			Model.Changelists = Data.GetChanges( Model.CurrentBranchInfo.ID );

			// Make sure everything is refreshed every time
			Data.Dispose();

			if( Request.IsAjaxRequest() )
			{
				return PartialView( "ChangesTable", Model );
			}

			return View( "Index", Model );
		}
    }
}
