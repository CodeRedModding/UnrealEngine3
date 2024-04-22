/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace Pipes
{
	public class NamedPipe
	{
		[StructLayout( LayoutKind.Sequential )]
			public class Overlapped 
		{
		}

		[DllImport( "kernel32", SetLastError = true )]
		public static extern SafePipeHandle CreateNamedPipe(
			String	lpName,							// pipe name
			uint	dwOpenMode,							// pipe open mode
			uint	dwPipeMode,							// pipe-specific modes
			uint	nMaxInstances,						// maximum number of instances
			uint	nOutBufferSize,						// output buffer size
			uint	nInBufferSize,						// input buffer size
			uint	nDefaultTimeOut,						// time-out interval
			IntPtr	pipeSecurityDescriptor				// SD
			);

		[DllImport( "kernel32", SetLastError = true )]
		public static extern bool ConnectNamedPipe(
			SafePipeHandle hHandle,					// handle to named pipe
			Overlapped lpOverlapped					// overlapped structure
			);

		[DllImport( "kernel32", SetLastError = true )]
		public static extern bool DisconnectNamedPipe(
			SafePipeHandle hHandle
			);

		[DllImport( "kernel32", SetLastError = true )]
		public static extern bool ReadFile(
			SafePipeHandle hHandle,					// handle to file
			byte[] lpBuffer,							// data buffer
			uint nNumberOfBytesToRead,					// number of bytes to read
			byte[] lpNumberOfBytesRead,				// number of bytes read
			uint lpOverlapped							// overlapped buffer
			);

		public const int INVALID_HANDLE_VALUE = -1;
		public const uint PIPE_ACCESS_INBOUND = 0x00000001;
		public const uint PIPE_TYPE_BYTE = 0x00000000;
		public const uint PIPE_READMODE_BYTE = 0x00000000;

		private const uint BUFFER_SIZE = 1024;

		private SafePipeHandle PipeHandle;

		public NamedPipe()
		{
		}

		public void Disconnect()
		{
			DisconnectNamedPipe( PipeHandle );
		}

		public void Dispose()
		{
			Dispose( true );
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose( bool disposing )
		{
			if( disposing )
			{
				PipeHandle.Dispose();
			}
		}

		public bool Connect( Process ClientProcess )
		{
			try
			{
				string PipeName = "\\\\.\\pipe\\" + ClientProcess.Id + "cout";

				PipeHandle = CreateNamedPipe( PipeName, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, BUFFER_SIZE, BUFFER_SIZE, 1000, IntPtr.Zero );
				if( PipeHandle.IsInvalid )
				{
					return( false );
				}

				ConnectNamedPipe( PipeHandle, null );		
			}
			catch( System.Exception e )
			{
				System.Diagnostics.Debug.Write( e.Message );
				return( false );
			}

			return( true );
		}

		public string Read()
		{
			int	i, count;
			byte[] InData = new byte[BUFFER_SIZE + 1];
			byte[] NumBytes = new byte[4];
			string Output = "";

			// Read one line from the pipe
			ReadFile( PipeHandle, InData, BUFFER_SIZE, NumBytes, 0 );

			// Grab each unicode char as a pair of bytes
			count = NumBytes[0] + ( NumBytes[1] << 8 ) + ( NumBytes[2] << 16 ) + ( NumBytes[3] << 24 );
			for( i = 0; i < count; i += 2 )
			{
				int UnicodeChar = InData[i] + ( InData[i + 1] << 8 );
				Output += ( char )UnicodeChar;
			}

			return( Output );
		}
	}
}
