// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;

using CISStatus.Models;

namespace CISStatus.Controllers
{
	public class VerificationInfo
	{
		// The name of the build
		public int ID = 0;
		public string Description = "";

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

		public VerificationInfo( int InID, string InDescription, 
								int InAttempted, DateTime InAttemptedTime, int InGood, DateTime InGoodTime, int InFailed, DateTime InFailedTime,
								bool InSubTriggered, bool InSubSucceeded, bool InSubFailed )
		{
			ID = InID;
			Description = InDescription;
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

		private bool BuildIsBad()
		{
			if( LastGood < LastFailed )
			{
				return true;
			}

			if( LastGood == LastFailed )
			{
				return LastGoodTime < LastFailedTime;
			}

			return false;
		}

		public string GetBackground()
		{
			string Colour = "#ffffff";
			if( BuildIsBad() )
			{
				Colour = "#ff0000";
			}

			return Colour;
		}

		public string GetDescription()
		{
			return Description;
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
			if( BuildIsBad() )
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
			if( BuildIsBad() )
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
			if( BuildIsBad() )
			{
				return ( LastFailedTime - LastGoodTime ).TotalDays.ToString( "F0" );
			}
			else
			{
				return ( LastGoodTime - LastFailedTime ).TotalDays.ToString( "F0" );
			}
		}
	}

    public class VerificationStatusController : Controller
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

			return PartialView( "Index" );
		}

		public ActionResult Index()
		{
			// Create a container for our view data
			GeneralViewModel Model = new GeneralViewModel();

			// Create a link to the database	
			BuildStatusesDatabase Data = new BuildStatusesDatabase();

			// Get the list of unique changes 
			Model.Verifications = Data.GetVerifications( GetEmail() );

			// Make sure this gets cleaned up properly
			Data.Dispose();

			return View( Model );
		}
    }
}
