// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Controller
{
    public delegate void OnOutputDebugStringHandler( int pid, string text );

    class DebugMonitor
    {
        public static event OnOutputDebugStringHandler OnOutputDebugString;

        #region Win32 API Imports

        [StructLayout( LayoutKind.Sequential )]
        private struct SECURITY_DESCRIPTOR
        {
            public byte revision;
            public byte size;
            public short control;
            public IntPtr owner;
            public IntPtr group;
            public IntPtr sacl;
            public IntPtr dacl;
        }

        [StructLayout( LayoutKind.Sequential )]
        private struct SECURITY_ATTRIBUTES
        {
            public int nLength;
            public IntPtr lpSecurityDescriptor;
            public int bInheritHandle;
        }

        [Flags]
        private enum PageProtection : uint
        {
            NoAccess = 0x01,
            Readonly = 0x02,
            ReadWrite = 0x04,
            WriteCopy = 0x08,
            Execute = 0x10,
            ExecuteRead = 0x20,
            ExecuteReadWrite = 0x40,
            ExecuteWriteCopy = 0x80,
            Guard = 0x100,
            NoCache = 0x200,
            WriteCombine = 0x400,
        }


        private const int WAIT_OBJECT_0 = 0;
        private const uint INFINITE = 0xFFFFFFFF;
        private const int ERROR_ALREADY_EXISTS = 183;

        private const uint SECURITY_DESCRIPTOR_REVISION = 1;

        private const uint SECTION_MAP_READ = 0x0004;

        [DllImport( "kernel32.dll", SetLastError = true )]
        private static extern IntPtr MapViewOfFile( IntPtr hFileMappingObject, uint
            dwDesiredAccess, uint dwFileOffsetHigh, uint dwFileOffsetLow,
            uint dwNumberOfBytesToMap );

        [DllImport( "kernel32.dll", SetLastError = true )]
        private static extern bool UnmapViewOfFile( IntPtr lpBaseAddress );

        [DllImport( "advapi32.dll", SetLastError = true )]
        private static extern bool InitializeSecurityDescriptor( ref SECURITY_DESCRIPTOR sd, uint dwRevision );

        [DllImport( "advapi32.dll", SetLastError = true )]
        private static extern bool SetSecurityDescriptorDacl( ref SECURITY_DESCRIPTOR sd, bool daclPresent, IntPtr dacl, bool daclDefaulted );

        [DllImport( "kernel32.dll" )]
        private static extern IntPtr CreateEvent( ref SECURITY_ATTRIBUTES sa, bool bManualReset, bool bInitialState, string lpName );

        [DllImport( "kernel32.dll" )]
        private static extern bool PulseEvent( IntPtr hEvent );

        [DllImport( "kernel32.dll" )]
        private static extern bool SetEvent( IntPtr hEvent );

        [DllImport( "kernel32.dll", SetLastError = true )]
        private static extern IntPtr CreateFileMapping( IntPtr hFile,
            ref SECURITY_ATTRIBUTES lpFileMappingAttributes, PageProtection flProtect, uint dwMaximumSizeHigh,
            uint dwMaximumSizeLow, string lpName );

        [DllImport( "kernel32.dll", SetLastError = true )]
        private static extern bool CloseHandle( IntPtr hHandle );

        [DllImport( "kernel32", SetLastError = true, ExactSpelling = true )]
        private static extern Int32 WaitForSingleObject( IntPtr handle, uint milliseconds );

        #endregion

        private static IntPtr AckEvent = IntPtr.Zero;
        private static IntPtr ReadyEvent = IntPtr.Zero;
        private static IntPtr SharedFile = IntPtr.Zero;
        private static IntPtr SharedMem = IntPtr.Zero;
        private static Thread Capturer = null;
        private static object SyncRoot = new object();
        private static Mutex Mutex = null;

        public DebugMonitor()
        {
        }

        public static bool Start()
        {
            lock( SyncRoot )
            {
                if( Capturer != null )
                {
                    return( false );
                }

                bool CreatedNew = false;
                Mutex = new Mutex( false, typeof( DebugMonitor ).Namespace, out CreatedNew );

                SECURITY_DESCRIPTOR sd = new SECURITY_DESCRIPTOR();

                // Initialize the security descriptor.
                if( !InitializeSecurityDescriptor( ref sd, SECURITY_DESCRIPTOR_REVISION ) )
                {
                    // "Failed to initializes the security descriptor."
                    return ( false );
                }

                // Set information in a discretionary access control list
                if( !SetSecurityDescriptorDacl( ref sd, true, IntPtr.Zero, false ) )
                {
                    // "Failed to initializes the security descriptor"
                    return ( false );
                }

                SECURITY_ATTRIBUTES sa = new SECURITY_ATTRIBUTES();

                // Create the event for slot 'DBWIN_BUFFER_READY'
                AckEvent = CreateEvent( ref sa, false, false, "DBWIN_BUFFER_READY" );
                if( AckEvent == IntPtr.Zero )
                {
                    // "Failed to create event 'DBWIN_BUFFER_READY'"
                    return ( false );
                }

                // Create the event for slot 'DBWIN_DATA_READY'
                ReadyEvent = CreateEvent( ref sa, false, false, "DBWIN_DATA_READY" );
                if( ReadyEvent == IntPtr.Zero )
                {
                    // "Failed to create event 'DBWIN_DATA_READY'";
                    return ( false );
                }

                // Get a handle to the readable shared memory at slot 'DBWIN_BUFFER'.
                SharedFile = CreateFileMapping( new IntPtr( -1 ), ref sa, PageProtection.ReadWrite, 0, 4096, "DBWIN_BUFFER" );
                if( SharedFile == IntPtr.Zero )
                {
                    // "Failed to create a file mapping to slot 'DBWIN_BUFFER'"
                    return ( false );
                }

                // Create a view for this file mapping so we can access it
                SharedMem = MapViewOfFile( SharedFile, SECTION_MAP_READ, 0, 0, 512 );
                if( SharedMem == IntPtr.Zero )
                {
                    // "Failed to create a mapping view for slot 'DBWIN_BUFFER'"
                    return ( false );
                }

                // Start a new thread where we can capture the output
                // of OutputDebugString calls so we don't block here.
                Capturer = new Thread( new ThreadStart( Capture ) );
                Capturer.IsBackground = true;
                Capturer.Start();
                return ( true );
            }
        }

        private static void Capture()
        {
            try
            {
                // Everything after the first DWORD is our debugging text
                IntPtr pString = new IntPtr( SharedMem.ToInt32() + Marshal.SizeOf( typeof( int ) ) );

                while( true )
                {
                    SetEvent( AckEvent );

                    int ret = WaitForSingleObject( ReadyEvent, INFINITE );

                    // if we have no capture set it means that someone called 'Stop()' and is now waiting for us to exit this endless loop.
                    if( Capturer == null )
                    {
                        break;
                    }

                    if( ret == WAIT_OBJECT_0 )
                    {
                        // The first DWORD of the shared memory buffer contains the process ID of the client that sent the debug string.
                        FireOnOutputDebugString( Marshal.ReadInt32( SharedMem ), Marshal.PtrToStringAnsi( pString ) );
                    }
                }
            }
            catch
            {
                throw;
            }
            finally
            {
                Dispose();
            }
        }

        private static void FireOnOutputDebugString( int pid, string text )
        {
            // Raise event if we have any listeners
            if( OnOutputDebugString == null )
            {
                return;
            }

#if !DEBUG
            try
            {
#endif
                OnOutputDebugString( pid, text );
#if !DEBUG
            }
            catch( Exception )
            {
            }
#endif
        }

        private static void Dispose()
        {
            // Close AckEvent
            if( AckEvent != IntPtr.Zero )
            {
                CloseHandle( AckEvent );
                AckEvent = IntPtr.Zero;
            }

            // Close ReadyEvent
            if( ReadyEvent != IntPtr.Zero )
            {
                CloseHandle( ReadyEvent );
                ReadyEvent = IntPtr.Zero;
            }

            // Close SharedFile
            if( SharedFile != IntPtr.Zero )
            {
                CloseHandle( SharedFile );
                SharedFile = IntPtr.Zero;
            }

            // Unmap SharedMem
            if( SharedMem != IntPtr.Zero )
            {
                UnmapViewOfFile( SharedMem );
                SharedMem = IntPtr.Zero;
            }
        }

        public static void Stop()
        {
            lock( SyncRoot )
            {
                if( Capturer != null )
                {
                    Capturer = null;
                    PulseEvent( ReadyEvent );
                    while( AckEvent != IntPtr.Zero )
                    {
                    }
                }

                OnOutputDebugString = null;
            }
        }
    }
}
