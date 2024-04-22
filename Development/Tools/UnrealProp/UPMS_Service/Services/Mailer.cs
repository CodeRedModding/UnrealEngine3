/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Mail;
using System.Text;

namespace UnrealProp
{
    public static class Mailer
    {
        public static void SendEmail( int TaskerUserNameID, int TaskeeUserNameID, string Subject, string Body, int Importance )
        {
            string MailServer = Properties.Settings.Default.MailServer;

            string To = "";
            string Cc = "";
			if( TaskerUserNameID > 0 && TaskeeUserNameID > 0 )
			{
				if( TaskerUserNameID == TaskeeUserNameID )
				{
					To = DataHelper.User_GetEmail( TaskerUserNameID );
				}
				else
				{
					To = DataHelper.User_GetEmail( TaskeeUserNameID );
					Cc = DataHelper.User_GetEmail( TaskerUserNameID );
				}
			}
			else
			{
				To = Properties.Settings.Default.UnrealPropManagerEmail;
			}

            try
            {
                SmtpClient Client = new SmtpClient( MailServer );
                if( Client != null )
                {
                    MailAddress AddrTo;

                    MailMessage Message = new MailMessage();

					Message.From = new MailAddress( Properties.Settings.Default.UnrealPropEmail );
                    Message.Priority = MailPriority.High;

                    if( To.Length > 0 )
                    {
                        string[] Addresses = To.Split( ';' );
                        foreach( string Address in Addresses )
                        {
                            if( Address.Length > 0 )
                            {
                                AddrTo = new MailAddress( Address );
                                Message.To.Add( AddrTo );
                            }
                        }
                    }

                    if( Cc.Length > 0 )
                    {
                        string[] Addresses = Cc.Split( ';' );
                        foreach( string Address in Addresses )
                        {
                            if( Address.Length > 0 )
                            {
                                AddrTo = new MailAddress( Address );
                                Message.CC.Add( AddrTo );
                            }
                        }
                    }

					AddrTo = new MailAddress( Properties.Settings.Default.UnrealPropAdminEmail );
                    Message.Bcc.Add( AddrTo );

                    Message.Subject = "[UNREALPROP] " + Subject;
                    Message.Body = Body;
                    Message.IsBodyHtml = false;

                    switch( Importance )
                    {
                        case 0:
                            Message.Priority = MailPriority.Low;
                            break;
                        case 1:
                        default:
                            Message.Priority = MailPriority.Normal;
                            break;
                        case 2:
                            Message.Priority = MailPriority.High;
                            break;
                    }

                    Client.Send( Message );
                }
            }
            catch( Exception Ex )
            {
                System.Diagnostics.Debug.WriteLine( Ex.Message );
            }
        }
    }
}
