/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ServiceProcess;
using System.Text;

namespace UnrealProp
{
    static class Program
    {
        static void Main( string[] Args )
        {
            UPMS_Service UPMS = new UPMS_Service();

            if( !Environment.UserInteractive )
            {
                // Launch the service as normal if we aren't in the debugger
                ServiceBase.Run( UPMS );
            }
            else
            {
                // Call OnStart, wait for a console keypress, call OnStop
                UPMS.DebugRun( Args );
            }
        }
    }
}