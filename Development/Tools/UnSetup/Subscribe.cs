/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Net;

namespace UnSetup
{
	public partial class Utils
	{
		/** Simple sanity checking for a valid email */
		public bool ValidateEmailAddress( string Email )
		{
			if( Email.Length == 0 )
			{
				return ( true );
			}

			if( !Email.Contains( "@" ) )
			{
				return ( false );
			}

			if( !Email.Contains( "." ) )
			{
				return ( false );
			}

			if( Email.Contains( " " ) )
			{
				return ( false );
			}

			if( Email.LastIndexOf( '.' ) < Email.IndexOf( '@' ) )
			{
				return ( false );
			}

			if( Email.StartsWith( "@" ) )
			{
				return ( false );
			}

			if( Email.EndsWith( "." ) )
			{
				return ( false );
			}

			return ( true );
		}

		public bool SubscribeToMailingList( string Email )
		{
			// POST the email address to this web address.
			string SubscribeUrl = "http://udkprofiler.epicgames.com/subscribe/";

            bool bSubmitSuccessful = true;

			try
			{
				// set up the POST URL
				HttpWebRequest Request = WebRequest.Create( SubscribeUrl ) as HttpWebRequest;
				Request.ContentType = "text/plain";
				Request.Method = "POST";
				Request.Timeout = 5000;
				var PostBuffer = Encoding.Unicode.GetBytes( Email );
				Request.ContentLength = PostBuffer.Length;
				var PostStream = Request.GetRequestStream();
				PostStream.Write( PostBuffer, 0, PostBuffer.Length );
				PostStream.Close();

				using( HttpWebResponse Response = Request.GetResponse() as HttpWebResponse )
				{
					if( Response.StatusCode != HttpStatusCode.OK )
					{
						throw new ApplicationException( "Bad status code: " + Response.StatusCode );
					}
				}
			}
			catch( Exception Ex )
			{
				System.Diagnostics.Debug.WriteLine( string.Format( "Failed to post user's subscribe email {0} to {1}. Exception Details:\n{2}", Email, SubscribeUrl, Ex ) );
                bSubmitSuccessful = false;
			}

            return bSubmitSuccessful;
		}
	}
}
