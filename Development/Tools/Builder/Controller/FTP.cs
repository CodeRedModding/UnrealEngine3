// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Threading;

namespace Controller
{
	public partial class SandboxedAction
	{
		private static int GlobalFTPRetries = 0;

		private bool ThreadShouldRun()
		{
			if( !Parent.Ticking )
			{
				Builder.Write( "FTP ERROR: Exceeded 100 retries while FTPing - giving up" );
				return false;
			}

			if( Parent.CommandID == 0 && Parent.JobID == 0 )
			{
				return false;
			}

			if( GlobalFTPRetries > 20 )
			{
				return false;
			}

			return true;
		}

		private Stream GetWebRequestStream( string RootName, FileInfo Info )
		{
			int RetryCount = 0;
			Stream RequestStream = null;
			while( ThreadShouldRun() && RetryCount < 2 )
			{
				string DestinationName = Info.FullName.Substring( RootName.Length ).Replace( '\\', '/' );
				Uri DestURI = new Uri( "ftp://" + Builder.FTPServer + DestinationName );
                
                try
				{
					FtpWebRequest Request = ( FtpWebRequest )FtpWebRequest.Create( DestURI );

					// Set the credentials required to connect to the FTP server
					Request.Credentials = new NetworkCredential( Builder.FTPUserName, Builder.FTPPassword );

					// Automatically close the connection on completion
					Request.KeepAlive = true;

					// Set to upload a file
					Request.Method = WebRequestMethods.Ftp.UploadFile;

					// Send a binary file
					Request.UseBinary = true;

					// Amount of data to upload
					Request.ContentLength = Info.Length;

					RequestStream = Request.GetRequestStream();
				}
				catch( Exception Ex )
				{
					Parent.SendWarningMail( "FTP Exception getting web request stream; retry #" + RetryCount, Ex.ToString() + "\n\n\nDestURI: " + DestURI.ToString(), true );
					RetryCount++;
					GlobalFTPRetries++;
					Thread.Sleep( 500 );
				}
			}

			return RequestStream;
		}

		private void CreateFolder( string RootName, DirectoryInfo DirInfo )
		{
			int RetryCount = 0;
			while( ThreadShouldRun() && RetryCount < 2 )
			{
				string DestinationName = DirInfo.FullName.Substring( RootName.Length ).Replace( '\\', '/' );
				Uri DestURI = new Uri( "ftp://" + Builder.FTPServer + DestinationName );
				try
				{
					FtpWebRequest Request = ( FtpWebRequest )FtpWebRequest.Create( DestURI );

					// Set the credentials required to connect to the FTP server
					Request.Credentials = new NetworkCredential( Builder.FTPUserName, Builder.FTPPassword );

					// Automatically close the connection on completion
					Request.KeepAlive = true;

					// Set to upload a file
					Request.Method = WebRequestMethods.Ftp.MakeDirectory;
					WebResponse Response = Request.GetResponse();
				}
				catch( Exception Ex )
				{
					Parent.SendWarningMail( "FTP Exception creating folder; retry #" + RetryCount, Ex.ToString() + "\n\n\nDestURI: " + DestURI.ToString(), true );
					RetryCount++;
					GlobalFTPRetries++;
					Thread.Sleep( 500 );
				}
			}
		}

		private long UploadFile( string RootName, FileInfo Info, long Current, long Total )
		{
			int RetryCount = 0;
			string DestinationName = Info.FullName.Substring( RootName.Length );
			Parent.Log( "Uploading: " + DestinationName, Color.DarkGreen );

			while( ThreadShouldRun() && RetryCount < 2 )
			{
				try
				{
					using( FileStream Source = Info.OpenRead() )
					{
						using( Stream Destination = GetWebRequestStream( RootName, Info ) )
						{
							int CurrentPercent = ( int )( ( Current * 100.0 ) / Total );

							int MaxBufferLength = 65536;
							byte[] Buffer = new byte[MaxBufferLength];

							int BufferLength = Source.Read( Buffer, 0, MaxBufferLength );
							while( ThreadShouldRun() && BufferLength > 0 )
							{
								Destination.Write( Buffer, 0, BufferLength );
								Current += BufferLength;

								BufferLength = Source.Read( Buffer, 0, MaxBufferLength );

								int NewPercent = ( int )( ( Current * 100.0 ) / Total );
								if( NewPercent != CurrentPercent )
								{
									CurrentPercent = NewPercent;
									Parent.Log( "[STATUS] Uploading " + CurrentPercent.ToString() + "% complete", Color.Magenta );
								}
							}
						}
					}
				}
				catch( Exception Ex )
				{
					Parent.SendWarningMail( "FTP Exception uploading file; retry #" + RetryCount, Ex.ToString(), true );
					RetryCount++;
					GlobalFTPRetries++;
					Thread.Sleep( 500 );
				}
			}

			Parent.WritePerformanceData( Parent.MachineName, "Controller", "BytesFTPed", Info.Length, 0 );

			return Current;
		}

		private long UploadFolder( string RootName, DirectoryInfo DirInfo, long Current, long Total )
		{
			// Make the remote folder if it doesn't exist
			CreateFolder( RootName, DirInfo );

			// Recurse over all folders
			foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
			{
				if( ThreadShouldRun() )
				{
					Current = UploadFolder( RootName, SubDirInfo, Current, Total );
				}
			}

			// Upload all files
			foreach( FileInfo Info in DirInfo.GetFiles() )
			{
				if( ThreadShouldRun() )
				{
					Current = UploadFile( RootName, Info, Current, Total );
				}
			}

			return Current;
		}

		private void FTPFileThreadProc( object SourceFileName )
		{
			try
			{
				string FullPath = Path.GetFullPath( ( string )SourceFileName );
				FileInfo Info = new FileInfo( FullPath );
				if( !Info.Exists )
				{
					Builder.Write( "FTP ERROR: Unable to find file to send: " + FullPath );
					return;
				}

				Parent.Log( "[STATUS] Starting to upload " + Info.Name, Color.Magenta );

				UploadFile( Path.GetDirectoryName( Info.FullName ), Info, 0, Info.Length );
			}
			catch( Exception Ex )
			{
				Parent.Log( "FTP ERROR: Exception while FTPing: " + Ex.ToString(), Color.Red );
				Builder.Write( "FTP ERROR: Exception while FTPing: " + Ex.ToString() );
			}
		}

		private void FTPFolderThreadProc( object SourceFolderName )
		{
			try
			{
				string FullPath = Path.GetFullPath( ( string )SourceFolderName );
				DirectoryInfo Info = new DirectoryInfo( FullPath );
				if( !Info.Exists )
				{
					Builder.Write( "FTP ERROR: Unable to find folder to send: " + FullPath );
					return;
				}

				Parent.Log( "[STATUS] Starting to upload " + Info.Name, Color.Magenta );

				long Total = Parent.GetDirectorySize( FullPath );
				string RootName = Path.GetDirectoryName( Info.FullName );
				UploadFolder( RootName, Info, 0, Total );
			}
			catch( Exception Ex )
			{
				Parent.Log( "FTP ERROR: Exception while FTPing: " + Ex.ToString(), Color.Red );
				Builder.Write( "FTP ERROR: Exception while FTPing: " + Ex.ToString() );
			}
		}

		private void FTPSend( string FileName, COMMANDS Command )
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( Command );
				Builder.OpenLog( LogFileName, false );

				if( Builder.FTPServer.Length == 0 || Builder.FTPUserName.Length == 0 || Builder.FTPPassword.Length == 0 )
				{
					Builder.Write( "Error: FTP server and credentials are not set!" );
					State = Command;
				}
				else
				{
					GlobalFTPRetries = 0;

					switch( Command )
					{
					case COMMANDS.FTPSendFile:
					case COMMANDS.FTPSendImage:
						Builder.ManageFTPThread = new Thread( FTPFileThreadProc );
						Builder.ManageFTPThread.Start( FileName );
						break;

					case COMMANDS.FTPSendFolder:
						Builder.ManageFTPThread = new Thread( FTPFolderThreadProc );
						Builder.ManageFTPThread.Start( FileName );
						break;
					}
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = Command;
				Builder.Write( "Error: exception while starting to FTP" );
				Builder.CloseLog();
			}
		}

		public MODES FTPSendFile()
		{
			string[] Params = Builder.SplitCommandline();
			if( Params.Length != 1 )
			{
				Builder.Write( "Error: incorrect number of parameters for FTPSendFile" );
				State = COMMANDS.FTPSendFile;
			}
			else
			{
				FTPSend( Params[0], COMMANDS.FTPSendFile );
			}

			return MODES.WaitForFTP;
		}

		public MODES FTPSendFolder()
		{
			string[] Params = Builder.SplitCommandline();
			if( Params.Length != 1 )
			{
				Builder.Write( "Error: incorrect number of parameters for FTPSendFolder" );
				State = COMMANDS.FTPSendFolder;
			}
			else
			{
				string SourceFolder = Path.Combine( Params[0], Builder.GetFolderName() );
				FTPSend( SourceFolder, COMMANDS.FTPSendFolder );
			}

			return MODES.WaitForFTP;
		}

		public MODES FTPSendImage()
		{
			string[] Params = Builder.SplitCommandline();
			if( Params.Length != 1 )
			{
				Builder.Write( "Error: incorrect number of parameters for FTPSendImage" );
				State = COMMANDS.FTPSendImage;
			}
			else
			{
				string SourceFile = Path.Combine( Params[0], Builder.GetImageName() + "." + Builder.ImageMode );
				FTPSend( SourceFile, COMMANDS.FTPSendImage );
			}

			return MODES.WaitForFTP;
		}

		public MODES CreateMGSTrigger()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CreateMGSTrigger ), false );

				string[] Params = Builder.SplitCommandline();
				if( Params.Length != 1 )
				{
					Builder.Write( "Error: incorrect number of parameters for CreateMGSTrigger" );
					State = COMMANDS.CreateMGSTrigger;
				}
				else
				{
					string SourceFolder = Path.Combine( Params[0], Builder.GetFolderName() );
					FileStream Trigger = File.Create( Path.Combine( SourceFolder, "MGSTrigger.txt" ) );
					Trigger.Close();
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.CreateMGSTrigger;
				Builder.Write( "Error: exception creating MGS trigger file." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES CreateFakeTOC()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CreateFakeTOC ), false );

				string[] Params = Builder.SplitCommandline();
				if( Params.Length != 1 )
				{
					Builder.Write( "Error: incorrect number of parameters for CreateFakeTOC" );
					State = COMMANDS.CreateFakeTOC;
				}
				else
				{
					string SourceFolder = Path.Combine( Params[0], Builder.GetFolderName(), Builder.BranchDef.Branch, Builder.LabelInfo.Game + "Game" );
					FileStream Trigger = File.Create( Path.Combine( SourceFolder, "FakeTOC.txt" ) );
					Trigger.Close();
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.CreateFakeTOC;
				Builder.Write( "Error: exception creating fake TOC file." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}
	}
}
