// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using Ionic.Zip;

namespace CookerSync
{
	partial class CookerSyncApp
	{
		static public bool SaveZip( List<List<ConsoleInterface.TOCInfo>> TOCList, ConsoleInterface.TOCSettings BuildSettings )
		{
			string OldCWD = Environment.CurrentDirectory;
			Environment.CurrentDirectory = Path.Combine( Environment.CurrentDirectory, ".." );

			Log( Color.Green, "[ZIPPING STARTED]" );
			DateTime StartTime = DateTime.UtcNow;

			foreach( string ZipFileName in BuildSettings.ZipFiles )
			{
				string RootZipFileName = Path.ChangeExtension( ZipFileName, null );

				// Create empty zip
				ZipFile Zip = new ZipFile( RootZipFileName + ".zip" );
				Zip.CompressionLevel = Ionic.Zlib.CompressionLevel.Level9;
				Zip.BufferSize = 0x10000;
				Zip.UseZip64WhenSaving = Zip64Option.Always;

				// Copy all files into the zip
				foreach( List<ConsoleInterface.TOCInfo> TOC in TOCList )
				{
					// Copy each file from the table of contents into the zip
					foreach( ConsoleInterface.TOCInfo Entry in TOC )
					{
						string FullFileName = Entry.FileName;
						if( FullFileName.StartsWith( "..\\" ) )
						{
							FullFileName = FullFileName.Substring( 3 );
						}

						Log( Color.Black, "Adding/updating " + FullFileName );
						Zip.UpdateFile( FullFileName );
					}
				}

				// Save zip
				Log( Color.Black, "Saving zip: " + RootZipFileName + ".zip" );
				Zip.Save();
			}

			TimeSpan Duration = DateTime.UtcNow.Subtract( StartTime );
			Log( Color.Green, "Operation took " + Duration.Minutes.ToString() + ":" + Duration.Seconds.ToString( "D2" ) );
			Log( Color.Green, "[ZIPPING FINISHED]" );

			Environment.CurrentDirectory = OldCWD;
			return( true );
		}
	}
}