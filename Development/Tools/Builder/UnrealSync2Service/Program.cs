// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceProcess;
using System.Text;

namespace Builder.UnrealSyncService
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		static void Main( string[] Args )
		{
			UnrealSyncServicer UnrealSync = new UnrealSyncServicer();
#if !DEBUG
			if( !Environment.UserInteractive )
			{
				// Launch the service as normal if we aren't in the debugger
				ServiceBase.Run( UnrealSync );
			}
			else
#endif
			{
				// Call OnStart, wait for a console key press, call OnStop
				UnrealSync.DebugRun( Args );
			}
		}
	}
}
