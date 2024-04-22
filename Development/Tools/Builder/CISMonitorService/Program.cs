// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceProcess;
using System.Text;

namespace Builder.CISMonitor
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		static void Main( string[] Args )
		{
			CISMonitorService CISMonitor = new CISMonitorService();
#if !DEBUG	
			if( !Environment.UserInteractive )
			{
				// Launch the service as normal if we aren't in the debugger
				ServiceBase.Run( CISMonitor );
			}
			else
#endif
			{
				// Call OnStart, wait for a console key press, call OnStop
				CISMonitor.DebugRun( Args );
			}
		}
	}
}
