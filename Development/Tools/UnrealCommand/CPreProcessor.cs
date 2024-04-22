/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.IO;
using System.Collections.Generic;


namespace UnrealCommand
{
	partial class Program
	{
		static private void PreprocessShader(string[] args)
		{
			if (args.Length < 5)
			{
				Log("Usage: UnrealCommand PreprocessShader <GameName> <Configuration> <InputFile.glsl> <OutputFile.i> [-CleanWhitespace]");
				Log("		-CleanWhitespace		Deletes all empty lines as well as leading and trailing whitespace from output shader");
				Log("");
				return;
			}

			// Check to see if the user passed an argument telling us to strip whitespace
			bool bShouldCleanWhitespace =
				args.Length >= 6 &&
				args[5].Equals("-CleanWhitespace", StringComparison.InvariantCultureIgnoreCase);

			string InputFilename = args[3];
			string OutputFilename = args[4];


			try
			{
				string StdOutResults;
//				int ExitCode = RunExecutableAndWait("cl.exe", String.Format("{0} /EP /u /X /nologo", InputFilename), out StdOutResults);
                int ExitCode = RunExecutableAndWait("Redist\\MCPP\\bin\\mcpp.exe", String.Format("-@std -P {0} -Q", InputFilename), out StdOutResults);

				DeleteFile(OutputFilename);

				// Did preprocessing succeed?
				if (ExitCode == 0)
				{
					char Newline = '\n';
					char CR = '\r';
					char[] Delimiters = { Newline, CR };
					string[] Lines = StdOutResults.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);

					List<String> GoodLines = new List<String>();
					char[] WhiteSpace = { ' ', '\t' };
					foreach (var AString in Lines)
					{
						if (AString.Trim().Length > 0)
						{
							if (bShouldCleanWhitespace)
							{
								GoodLines.Add(AString.Trim());
							}
							else
							{
								GoodLines.Add(AString);
							}
						}
					}

					string NoEmptyLines = String.Join("\r\n", GoodLines.ToArray(), 0, GoodLines.Count);

					File.WriteAllText(OutputFilename, NoEmptyLines);
					Log("Successfully preprocessed shader: " + InputFilename);
				}
				else
				{
					Error("Failed to preprocess shader (" + InputFilename + "), mcpp.exe failed with return code: " + ExitCode.ToString());
					ReturnCode = ExitCode;
				}
			}
			catch (Exception ex)
			{
				Error("Failed to preprocess shader (" + InputFilename + "), due to error: " + ex.Message);
				ReturnCode = 400;
			}
		}
	}
}