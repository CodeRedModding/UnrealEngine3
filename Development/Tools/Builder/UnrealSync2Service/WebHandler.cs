// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;

namespace Builder.UnrealSyncService
{
	public class WebHandler
	{
		private const int MaxSimultaneousListeners = 5;
		private BuilderMonitor Monitor = null;
		private HttpListener ServiceHttpListener = null;

		public WebHandler( BuilderMonitor InMonitor )
		{
			Monitor = InMonitor;

			ServiceHttpListener = new HttpListener();
			ServiceHttpListener.Prefixes.Add( "http://*:2827/UnrealSync/" );
			ServiceHttpListener.Start();
			for( int i = 0; i < MaxSimultaneousListeners; ++i )
			{
				ServiceHttpListener.BeginGetContext( AsyncHandleHttpRequest, null );
			}
		}

		public void Release()
		{
			ServiceHttpListener.Stop();
			ServiceHttpListener.Abort();
		}

		private void AsyncHandleHttpRequest( IAsyncResult Result )
		{
#if !DEBUG
			try
			{
#endif
			HttpListenerContext Context = ServiceHttpListener.EndGetContext( Result );
			ServiceHttpListener.BeginGetContext( AsyncHandleHttpRequest, null );
			HttpListenerRequest Request = Context.Request;

			using( HttpListenerResponse Response = Context.Response )
			{
				// Extract the URL parameters
				string[] UrlElements = Request.RawUrl.Split( "/".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );

				// http://*:2827/UnrealSync/Info
				// http://*:2827/UnrealSync/GetBranch/BranchName
				string ResponseString = "";
				switch( UrlElements[1].ToLower() )
				{
				case "getperforceservers":
					ResponseString = Monitor.GetPerforceServers();
					break;
				case "getbranch":
					ResponseString = Monitor.GetBranch( UrlElements[2] );
					break;
				default:
					ResponseString = "Incorrect parameters - http://*:2827/UnrealSync/GetPerforceServers or http://*:2827/UnrealSync/GetBranch/BranchName";
					break;
				}

				Response.SendChunked = true;
				Response.ContentType = "text/xml";

				byte[] Buffer = System.Text.Encoding.UTF8.GetBytes( ResponseString );
				Response.ContentLength64 = Buffer.Length;
				Response.OutputStream.Write( Buffer, 0, Buffer.Length );

				Response.StatusCode = ( int )HttpStatusCode.OK;
			}
#if !DEBUG
			}
			catch
			{
			}
#endif
		}
	}
}
