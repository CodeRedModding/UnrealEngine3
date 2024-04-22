// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;

namespace Builder.UnrealSyncService
{
	public partial class UnrealSyncServicer : ServiceBase
	{
		public BuilderMonitor Monitor = null;
		public WebHandler Web = null;

		public UnrealSyncServicer()
		{
			InitializeComponent();
		}

		public void DebugRun( string[] Args )
		{
			OnStart( Args );
			Console.WriteLine( "Press enter to exit" );
			Console.Read();
			OnStop();
		}

		protected override void OnStart( string[] args )
		{
			// Start up the interrogation of the database tables for labels
			Monitor = new BuilderMonitor();

			// Start up the HttpListeners to respond to http requests
			Web = new WebHandler( Monitor );
		}

		protected override void OnStop()
		{
			//Web.Release();
			Monitor.Release();
		}
	}
}
