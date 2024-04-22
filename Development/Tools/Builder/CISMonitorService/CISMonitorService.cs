// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Web.Services;

namespace Builder.CISMonitor
{
	public partial class CISMonitorService : ServiceBase
	{
		public ChangelistMonitor Monitor = null;
		public WebHandler Web = null;

		public CISMonitorService()
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
			// Start up the interrogation of the database tables for CIS data
			Monitor = new ChangelistMonitor();

			// Start up the HttpListeners to respond to http requests
			Web = new WebHandler( Monitor );
		}

		protected override void OnStop()
		{
			Web.Release();
			Monitor.Release();
		}
	}
}
