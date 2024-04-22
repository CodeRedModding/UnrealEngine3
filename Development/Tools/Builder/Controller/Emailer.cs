// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Net;
using System.Net.Mail;
using System.Text;

using Controller.Models;

namespace Controller
{
    public class Emailer
    {
        string MailServer = Properties.Settings.Default.MailServer;

        Main Parent;
        P4 SCC;

        public Emailer( Main InParent, P4 InP4 )
        {
            Parent = InParent;
            SCC = InP4;
            Parent.Log( "Emailer created successfully", Color.DarkGreen );
        }

		private List<MailAddress> SplitDistro( string Emails )
		{
			List<MailAddress> MailAddresses = new List<MailAddress>();
			string[] Addresses = Emails.Split( ';' );
			foreach( string Address in Addresses )
			{
				if( Address.Trim().Length > 0 )
				{
					MailAddress AddrTo = null;
					try
					{
						AddrTo = new MailAddress( Address.Trim() );
					}
					catch
					{
					}

					if( AddrTo != null )
					{
						MailAddresses.Add( AddrTo );
					}
				}
			}

			return( MailAddresses );
		}

        private void SendMail( string To, string Cc, string Subject, string Body, bool CCEngineQA, bool CCIT, bool bReplyToCC, MailPriority Priority )
        {
#if !DEBUG
            try
            {
                SmtpClient Client = new SmtpClient( MailServer );
                if( Client != null )
                {
					MailAddress AddrTo = null;
					MailMessage Message = new MailMessage();

					Message.From = new MailAddress( Properties.Settings.Default.BuilderEmail );
					Message.Priority = Priority;

                    if( To.Length > 0 )
                    {
						List<MailAddress> ToAddresses = SplitDistro( To );
						foreach( MailAddress Address in ToAddresses )
                    	{
                    		Message.To.Add(Address);
                    	}
                    }

					// Add in any carbon copy addresses
					if( Cc.Length > 0 )
					{
						List<MailAddress> CcAddresses = SplitDistro( Cc );
						foreach( MailAddress Address in CcAddresses )
						{
							Message.CC.Add( Address );
						}

						if( bReplyToCC && CcAddresses.Count > 0 )
						{
							Message.ReplyToList.Add( CcAddresses[0] );
						}
					}

					if( CCEngineQA )
                    {
						AddrTo = new MailAddress( "EngineQA@epicgames.com" );
                        Message.CC.Add( AddrTo );
					
						Message.ReplyToList.Add( AddrTo );
					}

					if( CCIT )
					{
						AddrTo = new MailAddress( "IT@epicgames.com" );
						Message.CC.Add( AddrTo );
					}

					// Quietly notify everyone that listens to build notifications
                    AddrTo = new MailAddress( Properties.Settings.Default.BuilderAdminEmail );
                    Message.Bcc.Add( AddrTo );

					Message.Subject = Subject;
                    Message.Body = Body;
                    Message.IsBodyHtml = false;

                    Client.Send( Message );

                    Parent.Log( "Email sent to: " + To, Color.Orange );
                }
            }
            catch( Exception e )
            {
                Parent.Log( "Failed to send email to: " + To, Color.Orange );
                Parent.Log( "'" + e.Message + "'", Color.Orange );
            }
#endif
		}

        private string BuildTime( BuildState Builder )
        {
            StringBuilder Result = new StringBuilder();

			Result.Append( "Build type: '" + Builder.CommandDetails.Description + "'" + Environment.NewLine );
			Result.Append( "    Began at " + Builder.CommandDetails.BuildStarted.ToLocalTime() + Environment.NewLine );
			Result.Append( "    Ended at " + DateTime.Now + Environment.NewLine );
			Result.Append( "    Elapsed Time was " );

			TimeSpan Duration = DateTime.UtcNow - Builder.CommandDetails.BuildStarted;
            if( Duration.Hours > 0 )
            {
                Result.Append( Duration.Hours.ToString() + " hour(s) " );
            }
            if( Duration.Hours > 0 || Duration.Minutes > 0 )
            {
                Result.Append( Duration.Minutes.ToString() + " minute(s) " );
            }
            Result.Append( Duration.Seconds.ToString() + " second(s)" + Environment.NewLine );

            return( Result.ToString() );
        }

		private string GetReportStatus( BuildState Builder )
		{
			string ReportStatus = "";
			List<string> StatusReport = Builder.GetStatusReport();
			if( StatusReport.Count > 0 )
			{
				ReportStatus += Environment.NewLine;
				foreach( string Status in StatusReport )
				{
					ReportStatus += Status + Environment.NewLine;
				}
			}

			return ( ReportStatus );
		}

		private string ConstructToList( BuildState Builder )
		{
			string To = "";
			if( Builder.CommandDetails.Operator.Length > 0 && Builder.CommandDetails.Operator != "AutoTimer" && Builder.CommandDetails.Operator != "LocalUser" )
			{
				To = Builder.CommandDetails.Operator + "@epicgames.com";
			}

			return To;
		}

        private string ConstructEmailAddressList( List<string> Recipients )
        {
			string Cc = "";
			if( Recipients != null )
			{
				foreach( string Recipient in Recipients )
				{
					Cc += ";" + Recipient;
				}
			}

			return ( Cc );
        }

        private string AddKiller( string Killer, int CommandID, string To )
        {
            if( Killer.Length > 0 && Killer != "LocalUser" )
            {
                if( To.Length > 0 )
                {
                    To += ";";
                }
                To += Killer + "@epicgames.com";
            }

            return ( To );
        }

        private string Signature()
        {
            return ( Environment.NewLine + "Cheers" + Environment.NewLine + Parent.MachineName + Environment.NewLine );
        }

		private string ConstructBaseSubject( string Subject, BuildState Builder, bool ShowGameAndPlatform, string Action )
		{
			// [BUILDER][BRANCH][GAME][PLATFORM][ACTION] Command
			if( Builder != null )
			{
				if( Builder.BranchDef.Branch.Length > "UnrealEngine3-".Length )
				{
					Subject += "[" + Builder.BranchDef.Branch.Substring( "UnrealEngine3-".Length ).ToUpper() + "]";
				}

				if( ShowGameAndPlatform && Builder.LabelInfo != null )
				{
					if( Builder.LabelInfo.Game.Length > 0 )
					{
						Subject += "[" + Builder.LabelInfo.Game.ToUpper() + "]";
					}
					if( Builder.LabelInfo.Platform.Length > 0 )
					{
						Subject += "[" + Builder.LabelInfo.Platform.ToUpper() + "]";
					}
				}
			}

			if( Action.Length > 0 )
			{
				Subject += "[" + Action.ToUpper() + "]";
			}

			return ( Subject );
		}

		private string ConstructSubject( string Subject, BuildState Builder, bool ShowGameAndPlatform, string Action, string Additional )
		{
			Subject = ConstructBaseSubject( Subject, Builder, ShowGameAndPlatform, Action );

			if( Builder != null )
			{
				Subject += " " + Builder.CommandDetails.Description;
			}

			if( Additional.Length > 0 )
			{
				Subject += " (" + Additional + ")";
			}

			return ( Subject );
		}

        public void SendTriggeredMail( BuildState Builder )
        {
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( BuilderLinq.GetSubscribers( Builder.CommandDetails.CommandID, ( byte )'T' ) );

			if( To.Length > 0 )
			{
				string Subject = ConstructSubject( "[BUILDER]", Builder, false, "Triggered", "" );
				StringBuilder Body = new StringBuilder();

				Body.Append( "Build type: '" + Builder.CommandDetails.Description + "'" + Environment.NewLine );
				Body.Append( Signature() );

				SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Low );
			}
        }

        public void SendKilledMail( BuildState Builder, int CommandID, int BuildLogID, string Killer )
        {
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( BuilderLinq.GetSubscribers( Builder.CommandDetails.CommandID, ( byte )'F' ) );
			To = AddKiller( Killer, CommandID, To );

			string Subject = ConstructSubject( "[BUILDER]", Builder, false, "Killed", "" );
            StringBuilder Body = new StringBuilder();

			Body.Append( BuildTime( Builder ) );
			Body.Append( "    Working from changelist " + Builder.LabelInfo.Changelist + Environment.NewLine );
			Body.Append( "    Started by " + Builder.CommandDetails.Operator + Environment.NewLine );
            Body.Append( "    Killed by " + Killer + Environment.NewLine );
            Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.High );
        }

		public string PrintChangesInRange( BuildState Builder, string DepotPath, string StartingRevision, string EndingRevision, out List<string> UsersToEmail )
		{
			StringBuilder Output = new StringBuilder();
			UsersToEmail = new List<string>();

			SCC.GetChangesInRange( Builder, DepotPath, StartingRevision, EndingRevision );
			foreach( BuildState.ChangeList CL in Builder.ChangeLists )
			{
				DateTime Time = new DateTime( 1970, 1, 1 );
				Time += new TimeSpan( ( long )CL.Time * 10000 * 1000 );
				Output.Append( Environment.NewLine + "Changelist " + CL.Number.ToString() + " submitted by " + CL.User + " on " + Time.ToLocalTime().ToString() + Environment.NewLine );
				Output.Append( Environment.NewLine + CL.Description + Environment.NewLine );
				Output.Append( "Affected files..." + Environment.NewLine + Environment.NewLine );
				foreach( string CLFile in CL.Files )
				{
					Output.Append( CLFile + Environment.NewLine );
				}
				Output.Append( Environment.NewLine );
				Output.Append( "-------------------------------------------------------------------------------" + Environment.NewLine );

				// Compose the user's email address
				string FullName, EmailAddress;
				if( SCC.GetUserInformation( Builder, CL.User, out FullName, out EmailAddress ) )
				{
					if( !UsersToEmail.Contains( EmailAddress ) )
					{
						UsersToEmail.Add( EmailAddress );
					}
				}
			}

			return Output.ToString();
		}

		private string HandleCISMessage( BuildState Builder, string FailureMessage, ref StringBuilder Body )
		{
			// Determine if it's possible this change actually broke the build
			string Additional = "";

			Additional = " FAILED WITH CHANGELIST " + Builder.LabelInfo.Changelist.ToString() + "!";

			if( Builder.CommandDetails.CISTaskID > 0 )
			{
				Main.CISTaskInfo CISTask = BuilderLinq.GetCISTaskInfo( Builder.CommandDetails.CISTaskID );
				if( CISTask != null )
				{
					if( CISTask.LastGood > Builder.LabelInfo.Changelist )
					{
						Body.Append( "A CIS Failure has been detected but THE BUILD HAS ALREADY BEEN FIXED." + Environment.NewLine );
						Body.Append( "                   Your change is: " + Builder.LabelInfo.Changelist.ToString() + Environment.NewLine );
						Body.Append( "    The last known good change is: " + CISTask.LastGood.ToString() + Environment.NewLine );
						Body.Append( "    The last known fail change is: " + CISTask.LastFail.ToString() + Environment.NewLine );
						Body.Append( Environment.NewLine );
						Body.Append( "Although the build has been fixed, please verify that the failure reported below has actually been addressed properly." + Environment.NewLine );
						Body.Append( Environment.NewLine );
					}
					else
					{
						Body.Append( "A CIS Failure has been detected and it's possible that it's a result of your changes." + Environment.NewLine );
						Body.Append( "                   Your change is: " + Builder.LabelInfo.Changelist.ToString() + Environment.NewLine );
						Body.Append( "    The last known good change is: " + CISTask.LastGood.ToString() + Environment.NewLine );
						Body.Append( "    The last known fail change is: " + CISTask.LastFail.ToString() + Environment.NewLine );
						Body.Append( Environment.NewLine );
						Body.Append( "It is also possible that a change between the last good changelist and yours is actually responsible. PLEASE VERIFY ASAP." + Environment.NewLine );
						Body.Append( Environment.NewLine );
                        Body.Append("Build status page: http://cisstatus.epicgames.net/");
					}
				}
			}

			Body.Append( Environment.NewLine );
			Body.Append( "-------------------------------------------------------------------------------" + Environment.NewLine );
			Body.Append( Environment.NewLine );

			return Additional;
		}

        public void SendFailedMail( BuildState Builder, int CommandID, int BuildLogID, string FailureMessage, string LogFileName )
        {
            // It's a job, so tag the failure message to the active label (if it's not a CIS job)
			if( CommandID == 0 && Builder.CommandDetails.bIsPrimaryBuild )
            {
                string JobStatus = Environment.NewLine + "Job failed on " + Parent.MachineName + ":" + Environment.NewLine;
                JobStatus += "Detailed log copied to: " + Properties.Settings.Default.FailedLogLocation.Replace( '/', '\\' ) + "\\" + Builder.BranchDef.Branch + "\\" + LogFileName + Environment.NewLine + Environment.NewLine;
                JobStatus += FailureMessage;
                JobStatus += Environment.NewLine;

                SCC.TagMessage( Builder, JobStatus );
                return;
            }

            // It's a normal build script - send a mail as usual
			string To = "";
			string Cc = "";
			string Subject = "";

            StringBuilder Body = new StringBuilder();

			if( CommandID != 0 || Builder.CommandDetails.bIsPrimaryBuild )
			{
				// Report the full info for a build failing
				To = ConstructToList( Builder );
				List<string> Subscribers = BuilderLinq.GetSubscribers( CommandID, ( byte )'F' );
				if( Builder.OfficialBuild )
				{
					Subscribers.Insert( 0, Properties.Settings.Default.BuilderFailureEmail );
				}

				Cc = ConstructEmailAddressList( Subscribers );

				Subject = ConstructSubject( "[BUILDER]", Builder, true, "Failed", "" );
			
				Body.Append( BuildTime( Builder ) );

				// Only report these for primary builds
				Body.Append( "    Current failing changelist " + Builder.LabelInfo.Changelist + Environment.NewLine );
				Body.Append( "    Last successful changelist " + Builder.CommandDetails.LastGoodBuild + Environment.NewLine );
			}
			else
			{
				// If this is a CIS job, issue the standard warning
				To = ConstructToList( Builder );
				List<string> Subscribers = new List<string>() { Properties.Settings.Default.BuilderFailureEmail };
				Cc = ConstructEmailAddressList( Subscribers );

				Subject = ConstructBaseSubject( "[CIS]", Builder, true, "Failed" );

				Subject += HandleCISMessage( Builder, FailureMessage, ref Body );

				Body.Append( BuildTime( Builder ) );

				Body.Append( "-------------------------------------------------------------------------------" + Environment.NewLine );

				List<string> UserEmail;
				Body.Append( PrintChangesInRange( Builder, "...", Builder.LabelInfo.Changelist.ToString(), Builder.LabelInfo.Changelist.ToString(), out UserEmail ) );
				To += ";" + ConstructEmailAddressList( UserEmail );
			}

			string LogFile = Properties.Settings.Default.FailedLogLocation.Replace( '/', '\\' ) + "\\" + Builder.BranchDef.Branch + "\\" + LogFileName;
			Body.Append( Environment.NewLine + "Detailed log copied to: " + LogFile + Environment.NewLine + Environment.NewLine );

			Body.Append( Environment.NewLine + FailureMessage + Environment.NewLine + Environment.NewLine );
			Body.Append( "-------------------------------------------------------------------------------" + Environment.NewLine );

			Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.High );
        }

        // Sends mail stating the version of the build that was just created
        public void SendSucceededMail( BuildState Builder, int CommandID, int BuildLogID, string FinalStatus )
        {
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( BuilderLinq.GetSubscribers( Builder.CommandDetails.CommandID, ( byte )'S' ) );

            string Label = Builder.LabelInfo.GetLabelName();
			string Subject = ConstructSubject( "[BUILDER]", Builder, false, "Succeeded", Label );
			StringBuilder Body = new StringBuilder();

            Body.Append( BuildTime( Builder ) );
			Body.Append( "    Current successful changelist " + Builder.LabelInfo.Changelist + Environment.NewLine );
			Body.Append( "         First failing changelist " + Builder.CommandDetails.LastFailedBuild + Environment.NewLine );

            if( Builder.NewLabelCreated )
            {
                if( Label.Length > 0 )
                {
                    Body.Append( Environment.NewLine + "Build is labeled '" + Label + "'" + Environment.NewLine + Environment.NewLine );
                }
            }

			Body.Append( GetReportStatus( Builder ) );

            Body.Append( Environment.NewLine + FinalStatus + Environment.NewLine );
            Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Normal );
        }

        // Sends mail stating the version of the build has started passing again
        public void SendNewSuccessMail( BuildState Builder, int CommandID, int BuildLogID, string FinalStatus )
        {
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( null );

			int Changelist = Builder.LabelInfo.Changelist;
			string Label = Builder.LabelInfo.GetLabelName();
			string Subject = ConstructSubject( "[BUILDER]", Builder, false, "Fixed", Changelist.ToString() );

			StringBuilder Body = new StringBuilder();

            Body.Append( BuildTime( Builder ) );
			Body.Append( "    Current successful changelist " + Builder.LabelInfo.Changelist + Environment.NewLine );
			Body.Append( "         First failing changelist " + Builder.CommandDetails.LastFailedBuild + Environment.NewLine );

            if( Builder.NewLabelCreated )
            {
                if( Label.Length > 0 )
                {
                    Body.Append( Environment.NewLine + "Build is labeled '" + Label + "'" + Environment.NewLine + Environment.NewLine );
                }
            }

            Body.Append( Environment.NewLine + FinalStatus + Environment.NewLine );
            Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Normal );
        }

		// Sends mail stating the build has failed, but we are suppressing the error
		public void SendSuppressedMail( BuildState Builder, int CommandID, int BuildLogID, string Status )
		{
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( null );

			int Changelist = Builder.LabelInfo.Changelist;
			string Label = Builder.LabelInfo.GetLabelName();
			string Subject = ConstructSubject( "[BUILDER]", Builder, true, "Suppressed", Changelist.ToString() );
			StringBuilder Body = new StringBuilder();

			if( Builder.NewLabelCreated )
			{
				if( Label.Length > 0 )
				{
					Body.Append( Environment.NewLine + "Build is labeled '" + Label + "'" + Environment.NewLine + Environment.NewLine );
				}
			}

			Body.Append( Environment.NewLine + Status + Environment.NewLine );
			Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Normal );
		}

		// Sends mail stating the version of the build has started passing again
		public void SendCISMail( BuildState Builder, string BuildName, string StartingRevision, string EndingRevision )
		{
			string Subject = ConstructBaseSubject( "[CIS]", Builder, false, "Fixed" );
			Subject += " Passed with changelist " + EndingRevision.ToString();

			StringBuilder Body = new StringBuilder();

			Body.Append( "-------------------------------------------------------------------------------" + Environment.NewLine );

			Body.Append( Environment.NewLine );
			Body.Append( BuildTime( Builder ) );
			Body.Append( "    First changelist of this range " + StartingRevision + Environment.NewLine );
			Body.Append( "     Last changelist of this range " + EndingRevision + Environment.NewLine );
			Body.Append( Environment.NewLine );

			Body.Append( "-------------------------------------------------------------------------------" + Environment.NewLine );

			List<string> UserEmail;
			Body.Append( PrintChangesInRange( Builder, "...", StartingRevision, EndingRevision, out UserEmail ) );
			string To = ConstructEmailAddressList( UserEmail );
			string Cc = "";

			Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Normal );
		}
		
		// Sends mail stating the version of the build that was used to create the data
        public void SendPromotedMail( BuildState Builder, int CommandID )
        {
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( BuilderLinq.GetSubscribers( Builder.CommandDetails.CommandID, ( byte )'S' ) );

			string Subject = ConstructSubject( "[BUILDER]", Builder, true, "Promoted", Builder.LabelInfo.GetLabelName() );
			StringBuilder Body = new StringBuilder();

            string Label = Builder.LabelInfo.GetLabelName();
            if( Label.Length > 0 )
            {
                Body.Append( Environment.NewLine + "The build labeled '" + Label + "' was promoted (details below)" + Environment.NewLine + Environment.NewLine );
            }

            Body.Append( Environment.NewLine + Builder.GetChanges( "" ) + Environment.NewLine );
            Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Normal );
        }

        // Sends mail stating the changes on a per user basis
        public void SendUserChanges( BuildState Builder, string User )
        {
            string QAChanges = Builder.GetChanges( User );

            // If there are no changes, don't send an email
            if( QAChanges.Length == 0 )
            {
                return;
            }

			string FullName = "";
			string EmailAddress = "";
			if( SCC.GetUserInformation( Builder, User, out FullName, out EmailAddress ) )
			{
				string Subject = "[QA] 'QA Build Changes' (" + Builder.LabelInfo.GetLabelName() + ")";
				StringBuilder Body = new StringBuilder();

				foreach( string Line in Builder.BUNs )
				{
					Body.Append( Line + Environment.NewLine );
				}

				Body.Append( Environment.NewLine );

				string Label = Builder.LabelInfo.GetLabelName();
				if( Label.Length > 0 )
				{
					Body.Append( Environment.NewLine + "The build labeled '" + Label + "' is the QA build (details below)" + Environment.NewLine + Environment.NewLine );
				}

				Body.Append( Environment.NewLine + QAChanges + Environment.NewLine );
				Body.Append( Signature() );

				SendMail( EmailAddress, "BUNs@epicgames.com", Subject, Body.ToString(), false, false, true, MailPriority.Normal );
			}
        }

        // Send a mail stating a cook has been copied to the network
        public void SendPublishedMail( BuildState Builder, int CommandID, int BuildLogID )
        {
			string To = ConstructToList( Builder );
			string Cc = ConstructEmailAddressList( BuilderLinq.GetSubscribers( Builder.CommandDetails.CommandID, ( byte )'S' ) );

			string Subject = ConstructSubject( "[BUILDER]", Builder, true, "Published", Builder.GetFolderName() );
			StringBuilder Body = new StringBuilder();

            Body.Append( BuildTime( Builder ) );

            List<string> Dests = Builder.GetPublishDestinations();
            if( Dests.Count > 0 )
            {
                Body.Append( Environment.NewLine + "Build was published to -" + Environment.NewLine );
                foreach( string Dest in Dests )
                {
					string WindowsFriendlyDest = Dest.Replace( "/", "\\" );
					Body.Append( "\t\"" + WindowsFriendlyDest + " \"" + Environment.NewLine );
                }
            }

			Body.Append( GetReportStatus( Builder ) );

            string Label = Builder.SyncedLabel;
            if( Label.Length > 0 )
            {
                Body.Append( Environment.NewLine + "The build labeled '" + Label + "' was used to cook the data (details below)" + Environment.NewLine + Environment.NewLine );
            }

            Body.Append( Environment.NewLine + Builder.GetChanges( "" ) + Environment.NewLine );
            Body.Append( Signature() );

			SendMail( To, Cc, Subject, Body.ToString(), false, false, false, MailPriority.Normal );
        }

        public void SendAlreadyInProgressMail( string Operator, string BuildType )
        {
            // Don't send in progress emails for CIS or Sync type builds
            if( BuildType.StartsWith( "CIS" ) || BuildType.StartsWith( "Sync" ) )
            {
                return;
            }

            string To = Operator + "@epicgames.com";
			string Subject = ConstructSubject( "[BUILDER]", null, false, "InProgress", BuildType );
			StringBuilder Body = new StringBuilder();

            string FinalStatus = "Build '" + BuildType + "' not retriggered because it is already building.";

            Body.Append( Environment.NewLine + FinalStatus + Environment.NewLine );
            Body.Append( Signature() );

			SendMail( To, "", Subject, Body.ToString(), false, false, false, MailPriority.Normal );
        }

		private void SendInfoMail( string InfoType, string Title, string Content, bool CCIT, MailPriority Priority )
		{
			string Subject = "[BUILDER] " + InfoType + " (" + Title + ")";

			StringBuilder Body = new StringBuilder();
			Body.Append( Content + Environment.NewLine );
			Body.Append( Signature() );

			SendMail( "", "", Subject, Body.ToString(), false, false, false, Priority );
		}

        public void SendWarningMail( string Title, string Warnings, bool CCIT )
        {
			SendInfoMail( "Warning!", Title, Warnings, CCIT, MailPriority.Normal );
        }

		public void SendErrorMail( string Title, string Errors, bool CCIT )
		{
			SendInfoMail( "Error!", Title, Errors, CCIT, MailPriority.High );
		}

        public void SendGlitchMail( string Line )
        {
            string To = "john.scott@epicgames.com";

            string Subject = "[BUILDER] File Copy Error! (disk full?)";
            StringBuilder Body = new StringBuilder();

			Body.Append( "Executing:" + Line );

			Body.Append( Environment.NewLine +  Signature() );

			SendMail( To, "", Subject, Body.ToString(), true, true, false, MailPriority.High );
        }
    }
}
