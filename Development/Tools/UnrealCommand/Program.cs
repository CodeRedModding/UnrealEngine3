/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Threading;
using System.Diagnostics;

namespace UnrealCommand
{
	partial class Program
	{
		static int ReturnCode = 0;
		static string MainCommand = "";

		/**
		 * Main control loop
		 */
		static int Main( string[] Arguments )
		{
			if( Arguments.Length == 0 )
			{
				Log( "Usage: UnrealCommand <Command> <GameName> <Configuration> [parameters]" );
				Log( "" );
				Log( "Commands:" );
				Log( "  ... PreprocessShader <GameName> <Configuration> <InputFile.glsl> <OutputFile.i> [-CleanWhitespace]" );
				Log( "  ... ProcessActionScript <WeakASFile> <SplitFile1> <SplitFile2>" );
				Log( "" );
				Log( "Examples:" );
				Log( "  UnrealCommand PreprocessShader SwordGame Shipping Input.glsl Output.i" );
				return ( 1 );
			}

			MainCommand = Arguments[0].ToLower();
			switch( MainCommand )
			{
				case "preprocessshader":
					PreprocessShader( Arguments );
					break;
				case "processactionscript":
					ProcessActionScript( Arguments );
					break;
				default:
					break;
			}

			return ( ReturnCode );
		}

		static private void Log(string Line)
		{
			Console.WriteLine(Line);
		}

		static void Error(string Line)
		{
			Console.ForegroundColor = ConsoleColor.Red;
			Log("UCO ERROR: " + Line);
			Console.ResetColor();
		}

		static private void Warning(string Line)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Log("UCO WARNING: " + Line);
			Console.ResetColor();
		}

		static int RunExecutableAndWait(string ExeName, string ArgumentList, out string StdOutResults)
		{
			// Create the process
			ProcessStartInfo PSI = new ProcessStartInfo(ExeName, ArgumentList);
			PSI.RedirectStandardOutput = true;
			PSI.UseShellExecute = false;
			PSI.CreateNoWindow = true;
			Process NewProcess = Process.Start(PSI);

			// Wait for the process to exit and grab it's output
			StdOutResults = NewProcess.StandardOutput.ReadToEnd();
			NewProcess.WaitForExit();
			return NewProcess.ExitCode;
		}
	}
}
