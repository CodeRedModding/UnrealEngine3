// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;

using CISStatus.Models;

namespace CISStatus.Controllers
{
	public class BuildInfo
	{
		// The name of the build
		public int ID;
		public string Description = "";
		public int Access = -1;
		public DateTime NextTrigger = DateTime.MinValue;
		public bool Pending = false;
		public string Machine = "";

		// The changelists relating to the state of the verification build
		public int LastAttempted = -1;
		public DateTime LastAttemptedTime = DateTime.MinValue;
		public int LastGood = -1;
		public DateTime LastGoodTime = DateTime.MinValue;
		public int LastFailed = -1;
		public DateTime LastFailedTime = DateTime.MinValue;

		public bool SubscribeTriggered = false;
		public bool SubscribeSucceeded = false;
		public bool SubscribeFailed = false;

		public BuildInfo( int InID, string InDescription, int InAccess, 
						DateTime? InNextTrigger, bool InPending, string InMachine, 
						int InAttempted, DateTime InAttemptedTime, 
						int InGood, DateTime InGoodTime, 
						int InFailed, DateTime InFailedTime,
						bool InSubTriggered, bool InSubSucceeded, bool InSubFailed )
		{
			ID = InID;
			Description = InDescription;
			Access = InAccess;
			if( InNextTrigger.HasValue )
			{
				NextTrigger = InNextTrigger.Value;
			}
			Pending = InPending;
			Machine = InMachine;
			LastAttempted = InAttempted;
			LastAttemptedTime = InAttemptedTime;
			LastGood = InGood;
			LastGoodTime = InGoodTime;
			LastFailed = InFailed;
			LastFailedTime = InFailedTime;

			SubscribeTriggered = InSubTriggered;
			SubscribeSucceeded = InSubSucceeded;
			SubscribeFailed = InSubFailed;
		}

		public string GetCommandID( string Prefix )
		{
			return Prefix + ID.ToString();
		}

		public string GetBackground()
		{
			string Colour = "#ffffff";
			if( LastGood != LastAttempted )
			{
				Colour = "#ff0000";
			}

			return Colour;
		}

		public string GetSubscribeTriggered()
		{
			if( SubscribeTriggered )
			{
				return "Checked=\"yes\"";
			}
			return "";
		}

		public string GetSubscribeSucceeded()
		{
			if( SubscribeSucceeded )
			{
				return "Checked=\"yes\"";
			}
			return "";
		}

		public string GetSubscribeFailed()
		{
			if( SubscribeFailed )
			{
				return "Checked=\"yes\"";
			}
			return "";
		}

		public string GetDescription()
		{
			return Description;
		}

		public string GetAccess()
		{
			return Access.ToString();
		}

		public string GetAttempted()
		{
			return LastAttempted.ToString();
		}

		public string GetGood()
		{
			return LastGood.ToString();
		}

		public string GetFailed()
		{
			return LastFailed.ToString();
		}

		public string GetStateColor()
		{
			if( LastGood != LastAttempted )
			{
				return "red";
			}
			else
			{
				return "#33CC33";
			}
		}

		public string GetImage()
		{
			if( Pending )
			{
				return "query.png";
			}

			if( Machine.Length > 0 )
			{
				return "spinner.gif";
			}

			if( LastGood != LastAttempted )
			{
				return "cross.png";
			}
			else
			{
				return "tick.png";
			}
		}

		public string GetStateTime()
		{
			if( LastFailed == 0 )
			{
				return "Forever";
			}
			else if( LastGood < LastFailed )
			{
				return ( LastFailedTime - LastGoodTime ).TotalDays.ToString( "F0" );
			}
			else
			{
				return ( LastGoodTime - LastFailedTime ).TotalDays.ToString( "F0" );
			}
		}

		public string GetTimeToNextBuild()
		{
			// Check for special cases
			if( Pending )
			{
				return "Pending";
			}

			if( Machine.Length > 0 )
			{
				return "In Progress";
			}

			if( NextTrigger == DateTime.MinValue )
			{
				return "-";
			}

			TimeSpan Delay = NextTrigger.ToUniversalTime() - DateTime.UtcNow;

			// Check for error
			if( Delay.TotalSeconds < 0 )
			{
				return "-";
			}

			// Return the approximate time
			if( Delay.TotalDays > 2.0 )
			{
				int Days = ( int )Delay.TotalDays;
				return Days.ToString() + " day(s)";
			}

			if( Delay.TotalHours > 2.0 )
			{
				int Hours = ( int )Delay.TotalHours;
				return Hours.ToString() + " hour(s)";
			}

			int Minutes = ( int )Delay.TotalMinutes;
			return Minutes.ToString() + " min(s)";
		}
	}

	public class BuildStatusController : Controller
	{
		private string GetEmail()
		{
			string[] UserName = User.Identity.Name.Split( "\\".ToCharArray() );
			return UserName[1] + "@" + UserName[0].ToLower() + ".com";
		}

		public ActionResult CheckBoxClicked( string Name, string Checked )
		{
			int CommandID = 0;
			if( Int32.TryParse( Name.Substring( 1 ), out CommandID ) )
			{
				string Email = GetEmail();

				BuildStatusesDatabase Data = new BuildStatusesDatabase();
				if( Checked == "true" )
				{
					Data.InsertSubscription( CommandID, Email, Name[0] );
				}
				else
				{
					Data.DeleteSubscription( CommandID, Email, Name[0] );
				}
				Data.Dispose();
			}

			return PartialView( "Status" );
		}

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

			Model.Branches = Data.GetAllBranches();

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

			Model.Title = "Build Status - Work in Progress!";

			SetCurrentBranchInfo( Model, BranchName );

			return View( Model );
		}

		public ActionResult GetBuildStatuses( string CurrentBranchName )
		{
			// Create a container for our view data
			GeneralViewModel Model = new GeneralViewModel();

			// Create a link to the database	
			BuildStatusesDatabase Data = new BuildStatusesDatabase();

			SetCurrentBranchInfo( Model, CurrentBranchName );

			Model.BuildStatuses = Data.GetBuildStatuses( Model.CurrentBranchInfo.ID, GetEmail() );

			// Make sure this gets cleaned up properly
			Data.Dispose();

			if( Request.IsAjaxRequest() )
			{
				return PartialView( "BuildsTable", Model );
			}

			return View( "Index", Model );
		}
	}
}
