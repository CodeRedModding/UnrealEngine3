/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ServiceProcess;
using System.Text;
using System.IO;

namespace UnrealProp
{
    static class Program
    {
        static void Main( string[] Args )
        {
            UPDS_Service UPDS = new UPDS_Service();

            if( !Environment.UserInteractive )
            {
                // Launch the service as normal if we aren't in the debugger
                ServiceBase.Run( UPDS );
            }
            else
            {
                // Call OnStart, wait for a console keypress, call OnStop
                UPDS.DebugRun( Args );
            }
        }
    }
}