/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using System.ComponentModel;
using XDevkit;

namespace UnrealProp
{
    class X360FileManager
    {
        public string ErrorString = "";

        private IXboxManager Manager;

        public X360FileManager()
        {
            Manager = new XboxManagerClass();
        }

        public IXboxConsole ConnectToTarget( string TargetName )
        {
            IXboxConsole Target = null;
            try
            {
                Target = Manager.OpenConsole( TargetName );
                TargetName = Target.Name;
            }
            catch
            {
                Target = null;
            }

            return ( Target );
        }

        public bool DisconnectFromTarget( IXboxConsole Target )
        {
            return( true );
        }

        public bool SendFile( IXboxConsole Target, string Source, string Destination )
        {
            try
            {
                Target.SendFile( Source, "xe:\\" + Destination );
            }
            catch( Exception Ex )
            {
                ErrorString = "SendFile exception: " + Ex.ToString();
                return ( false );
            }

            return( true );
        }

        private void CreateFolder( IXboxConsole Target, string Folder, string SubFolder )
        {
            IXboxFiles Files = Target.DirectoryFiles( Folder );
            foreach( IXboxFile File in Files )
            {
                if( File.IsDirectory && File.Name.ToLower() == SubFolder.ToLower() )
                {
                    return;
                }
            }

            Target.MakeDirectory( SubFolder );
        }

        public bool CreateDirectory( IXboxConsole Target, string DirectoryPath )
        {
            try
            {
                string[] Folders = DirectoryPath.Split( Path.DirectorySeparatorChar );
                string CurrentFolder = "xe:" + Path.DirectorySeparatorChar;
                foreach( string Folder in Folders )
                {
                    string CurrentSubFolder = CurrentFolder + Folder;
                    CreateFolder( Target, CurrentFolder, CurrentSubFolder );
                    CurrentFolder = CurrentSubFolder + Path.DirectorySeparatorChar;
                }
            }
            catch( Exception Ex )
            {
                ErrorString = "CreateDirectory exception: " + Ex.ToString();
                return ( false );
            }

            return ( true );
        }

        private void DeleteFolder( IXboxConsole Target, string Folder )
        {
            IXboxFiles Files = Target.DirectoryFiles( Folder );
            foreach( IXboxFile File in Files )
            {
                if( File.IsDirectory )
                {
                    DeleteFolder( Target, File.Name );
                    Target.RemoveDirectory( File.Name );
                }
                else
                {
                    Target.DeleteFile( File.Name );
                }
            }
        }

        public bool DeleteDirectory( IXboxConsole Target, string DirectoryPath )
        {
            try
            {
                IXboxFiles Files = Target.DirectoryFiles( "xe:\\" );
                foreach( IXboxFile File in Files )
                {
                    if( File.IsDirectory && File.Name.ToLower() == "xe:\\" + DirectoryPath.ToLower() )
                    {
                        DeleteFolder( Target, File.Name );
                        Target.RemoveDirectory( File.Name );
                    }
                }

            }
            catch( Exception Ex )
            {
                ErrorString = "DeleteDirectory exception: " + Ex.ToString();
                return ( false );
            }

            return ( true );
        }

        public ulong AvailableSpace( IXboxConsole Target )
        {
            ulong SpaceAvailable = 0;
            ulong TotalSpace = 0;
            ulong TotalFree = 0;
            try
            {
                Target.GetDiskFreeSpace( 'E', out SpaceAvailable, out TotalSpace, out TotalFree );
            }
            catch( Exception Ex )
            {
                ErrorString = "GetDiskFreeSpace exception: " + Ex.ToString();
            }

            return ( SpaceAvailable );
        }

        public bool RebootTarget( IXboxConsole Target )
        {
            try
            {
                Target.Reboot( null, null, "", XboxRebootFlags.Cold );
            }
            catch( Exception Ex )
            {
                ErrorString = "RebootTarget exception: " + Ex.ToString();
                return ( false );
            }

            return( true );
        }

        public bool RebootAndLaunchTarget( IXboxConsole Target, string BaseFolder, string Game, string Config, string CommandLine )
        {
            try
            {
		        string ExeName = "xe:\\" + BaseFolder + "\\" + Game + "Game-Xe" + Config + ".xex";
                string FolderName = "xe:\\" + BaseFolder;

                Target.Reboot( ExeName, FolderName, CommandLine, XboxRebootFlags.Title );
            }
            catch( Exception Ex )
            {
                ErrorString = "RebootAndLaunchTarget exception: " + Ex.ToString();
                return ( false );
            }

            return ( true );
        }
    }
}

