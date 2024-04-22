/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.IO;
using System.Management;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Tcp;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using RemotableType;

namespace UnrealProp
{
    public partial class UPDS_Service : ServiceBase
    {
        // For consistent formatting to the US standard (05/29/2008 11:33:52)
        public const string US_TIME_DATE_STRING_FORMAT = "MM/dd/yyyy HH:mm:ss";

        public static IUPMS_Interface IUPMS = null;
        public static string MachineName = "";

		private static DateTime StartTime = DateTime.Now;
		private static DateTime LastStatusTime = DateTime.MinValue;

        public UPDS_Service()
        {
            InitializeComponent();
        }

        static void AppExceptionHandler( object sender, UnhandledExceptionEventArgs args )
        {
            Exception E = ( Exception )args.ExceptionObject;
            Log.WriteLine( "Application exception", Log.LogType.Error, E.ToString() );
        }

        public void DebugRun( string[] Args )
        {
            OnStart( Args );
            Console.WriteLine( "Press enter to exit" );
            Console.Read();
            OnStop();
        }

        private void GetMachineName()
        {
            ManagementObjectSearcher Searcher = new ManagementObjectSearcher( "Select * from Win32_ComputerSystem" );
            ManagementObjectCollection Collection = Searcher.Get();

            foreach( ManagementObject Object in Collection )
            {
                Object Value;

                Value = Object.GetPropertyValue( "Name" );
                if( Value != null )
                {
                    MachineName = Value.ToString();
                }
                break;
            }
        }

		public static void SendStatusEmail()
		{
			string Message;

			if( DateTime.Now - LastStatusTime > new TimeSpan( 3, 0, 0 ) )
			{
				TimeSpan Span = DateTime.Now - StartTime;
				Message = "Uptime: " + Span.Days.ToString() + " days and " + Span.Hours + " hours." + Environment.NewLine + Environment.NewLine;
				Message += "Local cache limit (GB):" + Properties.Settings.Default.CacheSizeGB + Environment.NewLine;
				float CacheSize = ( float )( CacheSystem.GetCacheSize() / ( 1024.0 * 1024.0 * 1024.0 ) );
				Message += "Local cache size (GB): " + CacheSize.ToString( "F2" ) + Environment.NewLine + Environment.NewLine;
				Message += "Cheers" + Environment.NewLine + MachineName + Environment.NewLine;

				IUPMS.Utils_SendEmail( -1, -1, "UPDS Status", Message, 1 );

				LastStatusTime = DateTime.Now;
			}
		}

		private static Assembly CurrentDomain_AssemblyResolve( Object sender, ResolveEventArgs args )
		{
			// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
			string[] AssemblyInfo = args.Name.Split( ",".ToCharArray() );
			string AssemblyName = AssemblyInfo[0];

			if( AssemblyName.ToLower() == "xdevkit" )
			{
				string XDKPath = Environment.GetEnvironmentVariable( "XEDK" );
				if( !string.IsNullOrEmpty( XDKPath ) )
				{
					if( Environment.Is64BitProcess )
					{
						AssemblyName = Path.Combine( XDKPath, "bin\\x64\\", AssemblyName + ".dll" );
					}
					else
					{
						AssemblyName = Path.Combine( XDKPath, "bin\\win32\\", AssemblyName + ".dll" );
					}

					Debug.WriteLineIf( Debugger.IsAttached, "Loading assembly: " + AssemblyName );

					Assembly AssemblyInstance = Assembly.LoadFile( AssemblyName );
					return ( AssemblyInstance );
				}
			}

			return ( null );
		}

        protected override void OnStart( string[] Args )
        {
			AppDomain.CurrentDomain.AssemblyResolve += new ResolveEventHandler( CurrentDomain_AssemblyResolve );

            // Configure the remoting service
            ChannelServices.RegisterChannel( new TcpClientChannel(), false );
#if false
			IUPMS = ( IUPMS_Interface )Activator.GetObject( typeof( IUPMS_Interface ), "tcp://localhost:9090/UPMS" );
#else
            IUPMS = ( IUPMS_Interface )Activator.GetObject( typeof( IUPMS_Interface ), Properties.Settings.Default.RemotableHost );
#endif
            if( IUPMS == null )
            {
                Log.WriteLine( "UPDS", Log.LogType.Error, "No UPMS found - exiting!" );
                return;
            }

            CacheSystem.Init();
            IdleBuildsCacher.Init();
			TaskPoller.Init();

            GetMachineName();

            IUPMS.DistributionServer_Register( MachineName );

            // Register application exception handler
            AppDomain.CurrentDomain.UnhandledException += new UnhandledExceptionEventHandler( AppExceptionHandler );

            Log.WriteLine( "UPDS", Log.LogType.Important, "Service has been successfully started!" );
        }

        protected override void OnStop()
        {
            IUPMS.DistributionServer_Unregister( MachineName );

            TaskExecutor.StopAllTasks();
            IdleBuildsCacher.Release();

            Log.WriteLine( "UPDS", Log.LogType.Important, "Service has been stopped!" );
        }
    }

    public class Utils
    {
        static public long CopyFile( string SourceFileName, string DestFileName, ref string Error )
        {
            try
            {
                byte[] ReadBytes;
                long BufferSize = 10 * 1024 * 1024;
                FileStream SourceFileStream = null;
                FileStream DestFileStream = null;
                FileInfo SourceFileInfo = null;

                try
                {
                    FileInfo DestFile = new FileInfo( DestFileName );
                    if( DestFile.Exists )
                    {
                        DestFile.IsReadOnly = false;
                        DestFile.Delete();
                    }

                    DestFileStream = DestFile.Create();
                }
                catch( Exception Ex )
                {
                    Error = "Failed to create destination file: " + DestFileName + " with exception " + Ex.ToString();
                    return ( -1 );
                }

                try
                {
                    SourceFileStream = File.OpenRead( SourceFileName );
                }
                catch( Exception Ex )
                {
                    DestFileStream.Close();
                    Error = "Failed to open source file: " + SourceFileName + " with exception " + Ex.ToString();
                    return ( -1 );
                }

                try
                {
                    SourceFileInfo = new FileInfo( SourceFileStream.Name );
                }
                catch( Exception Ex )
                {
                    SourceFileStream.Close();
                    DestFileStream.Close();
                    Error = "Failed to get FileInfo on source file: " + SourceFileName + " with exception " + Ex.ToString();
                    return ( -1 );
                }

                long FileLength = SourceFileInfo.Length;
                long OriginalFileLength = FileLength;
                if( FileLength < BufferSize )
                {
                    BufferSize = FileLength;
                }

                if( BufferSize > 0 )
                {
                    ReadBytes = new byte[BufferSize];

                    while( FileLength > 0 )
                    {
                        if( FileLength < BufferSize )
                        {
                            BufferSize = FileLength;
                        }

                        try
                        {
                            SourceFileStream.Read( ReadBytes, 0, ( int )BufferSize );
                        }
                        catch( Exception Ex )
                        {
                            Error = "Stream read on: " + SourceFileName + " failed with exception " + Ex.ToString();
                            OriginalFileLength = -1;
                            break;
                        }

                        try
                        {
                            DestFileStream.Write( ReadBytes, 0, ( int )BufferSize );
                        }
                        catch( Exception Ex )
                        {
                            Error = "Stream write on: " + DestFileName + " failed with exception " + Ex.ToString();
                            OriginalFileLength = -1;
                            break;
                        }

                        FileLength -= BufferSize;
                    }
                }

                SourceFileStream.Close();
                DestFileStream.Close();

                return ( OriginalFileLength );
            }
            catch( Exception Ex )
            {
                Error = "Unhandled exception: " + Ex.ToString();
            }

            return ( -1 );
        }
    }

    public class Log
    {
        public enum LogType
        {
            Debug = 0,
			Info = 1,
            Important = 2,
            Error = 3,
        }

        private static TextWriter LogFile = null;

        public static void WriteLine( string Label, LogType Type, string Message )
        {
			// Suppress debug messages
			if( Type < LogType.Info )
			{
				return;
			}

            string Format = DateTime.Now.ToString( UPDS_Service.US_TIME_DATE_STRING_FORMAT ) + " [" + Label + "] ";
            if( Type == LogType.Error )
            {
                Format += "ERROR!!! ";
            }
            string RawString = Format + Message;

            Trace.WriteLine( RawString );
            Console.WriteLine( RawString );

            if( LogFile == null )
            {
                string LogFilePath = "UPDS_" + DateTime.Now.ToString( "yyyy_MM_dd-HH_mm_ss" ) + ".txt";
				LogFile = TextWriter.Synchronized( new StreamWriter( LogFilePath, true, Encoding.ASCII ) );
            }

            LogFile.WriteLine( RawString );

            if( Type == LogType.Error )
            {
                // Suppress the disconnected warning
                if( RawString.Contains( "HRESULT: 0x82DA0101" ) )
                {
                    return;
                }

                UPDS_Service.IUPMS.Utils_SendEmail( -1, -1, "UPDS '" + UPDS_Service.MachineName + "' reported error!", RawString, 2 );
                LogFile.Flush();
                // Sleep 30 seconds after any error
                Thread.Sleep( 30 * 1000 );
            }
        }
    }
}

