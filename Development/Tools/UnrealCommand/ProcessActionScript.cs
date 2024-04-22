/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace UnrealCommand
{
	partial class Program
	{
		static void ProcessActionScript( string[] Arguments )
		{
			if( Arguments.Length != 4 )
			{
				Error( "Usage: UnrealCommand ProcessActionScript <WeakASFile> <SplitFile1> <SplitFile2>" );
				return;
			}

			string WeakASFile = Arguments[1];
			string SplitASFile1 = Arguments[2];
			string SplitASFile2 = Arguments[3];

			Log( "Splitting " + WeakASFile + " into " + SplitASFile1 + " and " + SplitASFile2 );

			FileInfo Info = new FileInfo( WeakASFile );
			if( !Info.Exists )
			{
				Error( "Source file does not exist (" + WeakASFile + ")" );
				return;
			}

			bool bSplit = false;
			StreamWriter Split1 = new StreamWriter( SplitASFile1 );
			StreamWriter Split2 = new StreamWriter( SplitASFile2 );

			using( StreamReader Reader = Info.OpenText() )
			{
				while( !Reader.EndOfStream )
				{
					string Line = Reader.ReadLine();

					// Remove all instances of [WEAK]
					Line = Line.Replace( "[Weak]", "" );

					if( Line == "#---SPLIT" )
					{
						bSplit = true;
					}
					else if( !bSplit )
					{
						Split1.WriteLine( Line );
					}
					else
					{
						Split2.WriteLine( Line );
					}
				}
			}

			Split1.Close();
			Split2.Close();
			Log( "Completed successfully" );
		}
	}
}
