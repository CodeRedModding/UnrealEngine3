/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace Clean
{
	class Program
	{
		// Deletes the passed in folder and all its subfolders.
		static void DeleteFolder( string RootFolder )
		{
			if( Directory.Exists( RootFolder ) )
			{
				try
				{
                    Console.WriteLine( "Deleting folder: " + RootFolder );

					// Manually delete all files in folder first. Directory.Delete can do this but we want to
					// be as thorough as possible in the case of exceptions.
					foreach( string FileName in Directory.GetFiles( RootFolder ) )
					{
						try
						{
							File.Delete( FileName );
						}
						catch (System.Exception Ex)
						{
							Console.WriteLine( " ... Unable to delete file '" + FileName + "' - " + Ex.Message );
						}						
					}

					// Manually recurse to handle exceptions and be thorough in deletion in presence of exceptions.
					foreach( string FolderName in Directory.GetDirectories( RootFolder ) )
					{
						DeleteFolder( FolderName );
					}
					
					// Last but not least, try to delete folder.
					Directory.Delete( RootFolder, true );
				}
				catch( System.Exception Ex )
				{
					// Disregard if a folder couldn't be deleted.
                    Console.WriteLine( " ... Unable to delete folder '" + RootFolder + "' - " + Ex.Message );
				}
			}
		}

		// Deletes target related files based on the passed in base path
		static void DeleteTargetFiles( string BasePath )
		{
			try
			{
				if( Directory.Exists( Path.GetDirectoryName( BasePath ) ) )
				{
					Console.WriteLine( "Deleting generated binaries from " + BasePath );
					File.Delete( BasePath + ".exe" );
					File.Delete( BasePath + ".pdb" );
					File.Delete( BasePath + ".exp" );
					File.Delete( BasePath + ".lib" );
					File.Delete( BasePath + ".xdb" );
					File.Delete( BasePath + ".xex" );
					File.Delete( BasePath + ".elf" );
					File.Delete( BasePath + ".self" );
					File.Delete( BasePath + ".xelf" );
					File.Delete( BasePath + ".stub" );
					File.Delete( BasePath + ".apk" );
					File.Delete( BasePath + ".so" );
					File.Delete( BasePath + ".zip" );
					File.Delete( BasePath + ".ipa" );
					File.Delete( BasePath + ".rpx" );
					File.Delete( BasePath + ".swf" );
					File.Delete( BasePath + "." );
				}
			}
			catch( System.Exception Ex )
			{
				// Disregard if a file couldn't be deleted.
				Console.WriteLine( " ... Unable to delete target files ..." + Ex.Message );
			}
		}

		static void CleanFiles( string GameName, string PlatformName, string ConfigName, string OutputName )
		{
			// Folder relative to Development/Src
			string IntermediateFolder = Path.Combine( "..\\Intermediate", GameName);
			IntermediateFolder = Path.Combine(IntermediateFolder, PlatformName);
			IntermediateFolder = Path.Combine(IntermediateFolder, ConfigName);
			DeleteFolder( IntermediateFolder );
			DeleteTargetFiles( Path.Combine( Path.GetDirectoryName( OutputName ), Path.GetFileNameWithoutExtension( OutputName ) ) );
		}

		static void CleanAllFiles()
		{
			// Folder relative to location of clean.exe
			Console.WriteLine( "Deleting temporary folder, but not produced target items." );
			DeleteFolder( "..\\..\\Intermediate" );
		}

		static void Main( string[] Arguments )
		{
			Mutex GlobalMutex = new Mutex(false, "UnrealEngine3_CleanMutex");
			if ( GlobalMutex.WaitOne(60 * 1000) )
			{
				if( Arguments.Length != 4 )
				{
					CleanAllFiles();
				}
				else
				{
					CleanFiles( Arguments[0], Arguments[1], Arguments[2], Arguments[3] );
				}
			}
			else
			{
				StringBuilder Builder = new StringBuilder();
				foreach (string Value in Arguments)
				{
					Builder.Append(Value);
					Builder.Append(' ');
				}
				Console.WriteLine("Timed out trying to clean: " + Builder.ToString());
			}
			GlobalMutex.ReleaseMutex();
		}
	}
}
