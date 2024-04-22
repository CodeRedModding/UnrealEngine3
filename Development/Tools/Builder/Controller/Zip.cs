// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Ionic.Zip;

namespace Controller
{
	public partial class SandboxedAction
	{
		private void CreateZip()
		{
			if( Builder.CurrentZip == null )
			{
				Builder.CurrentZip = new ZipFile();
				Builder.CurrentZip.CompressionLevel = Ionic.Zlib.CompressionLevel.None;
				Builder.CurrentZip.UseZip64WhenSaving = Zip64Option.Always;
				Builder.Write( " ...  ZipFile created" );
			}
		}

		private MODES ZipAddItem( COMMANDS Command )
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( Command );
				Builder.OpenLog( LogFileName, false );

				// Create the zip if it doesn't exist
				CreateZip();

				string[] Params = Builder.SplitCommandline();
				if( Params.Length != 1 )
				{
					Builder.Write( "Error: incorrect number of parameters for " + Command.ToString() );
					State = Command;
				}
				else
				{
					switch( Command )
					{
					case COMMANDS.ZipAddImage:
						string ImageName = Path.Combine( Params[0], Builder.GetFolderName() + "." + Builder.ImageMode );
						Builder.Write( "Adding to Zip: " + ImageName );
						Builder.CurrentZip.AddFile( ImageName, "" );
						break;

					case COMMANDS.ZipAddFile:
						Builder.Write( "Adding to Zip: " + Builder.GetCurrentCommandLine() );
						Builder.CurrentZip.AddFile( Params[0], "" );
						break;
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = Command;
				Builder.Write( "Error: exception while adding item to zip" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES ZipAddFile()
		{
			return ZipAddItem( COMMANDS.ZipAddFile );
		}

		public MODES ZipAddImage()
		{
			return ZipAddItem( COMMANDS.ZipAddImage );
		}

		public MODES ZipSave()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ZipSave );
				Builder.OpenLog( LogFileName, false );

				string[] Params = Builder.SplitCommandline();
				if( Params.Length != 1 )
				{
					Builder.Write( "Error: incorrect number of parameters for ZipSave" );
					State = COMMANDS.ZipSave;
				}
				else
				{
					string ArchiveName = Path.Combine( Params[0], Builder.GetFolderName() + ".zip" );
					Builder.Write( "Saving Zip: " + ArchiveName );
					Builder.CurrentZip.Save( ArchiveName );
				}

				// Now we've saved the zip, we're done with it
				Builder.Write( "Disposing of Zip" );
				Builder.CurrentZip.Dispose();
				Builder.CurrentZip = null;

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.ZipSave;
				Builder.Write( "Error: exception while saving zip" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}
	}
}