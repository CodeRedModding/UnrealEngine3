/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Net.NetworkInformation;
using System.Text;
using System.Runtime.InteropServices;
using System.ComponentModel;

namespace UnrealProp
{
    public class IPCTarget
    {
    }

    class PCFileManager
    {
        public string ErrorString = "";

        public PCFileManager()
        {
        }

        public IPCTarget ConnectToTarget( string TargetName )
        {
            // Ping the target to make sure it is available
            IPCTarget Target = null;
            try
            {
                Ping PingSender = new Ping();
                PingReply Reply = PingSender.Send( TargetName );
                if( Reply.Status == IPStatus.Success )
                {
                    Target = new IPCTarget();
                }                
            }
            catch
            {
            }

            return ( Target );
        }

        public bool DisconnectFromTarget( IPCTarget Target )
        {
            return( true );
        }

        public bool SendFile( IPCTarget Target, string Source, string Destination )
        {
            return ( Utils.CopyFile( Source, Destination, ref ErrorString ) >= 0 );
        }

        public bool CreateDirectory( IPCTarget Target, string DirectoryPath )
        {
            try
            {
                Directory.CreateDirectory( DirectoryPath );
            }
            catch( Exception Ex )
            {
                ErrorString = "CreateDirectory exception: " + Ex.ToString();
                return ( false );
            }

            return ( true );
        }

        public bool DeleteDirectory( IPCTarget Target, string DirectoryPath )
        {
            try
            {
                if( Directory.Exists( DirectoryPath ) )
                {
                    Directory.Delete( DirectoryPath, true );
                }
            }
            catch( Exception Ex )
            {
                ErrorString = "DeleteDirectory exception: " + Ex.ToString();
                return ( false );
            }

            return ( true );
        }

        public bool RebootTarget( IPCTarget Target )
        {
            return( true );
        }

        public bool RebootAndLaunchTarget( IPCTarget Target, string BaseFolder, string Game, string Config, string CommandLine )
        {
            return ( true );
        }
    }
}

