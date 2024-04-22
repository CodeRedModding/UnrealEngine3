/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	class ResponseFile
	{
		/** Creates a file from a list of strings; each string is placed on a line in the file. */
		public static string Create(List<string> Lines)
		{
			return Create(Path.GetTempFileName(), Lines);
		}

		/** Creates a file from a list of strings; each string is placed on a line in the file. */
		public static string Create(string TempFileName, List<string> Lines)
		{
			Directory.CreateDirectory(Path.GetDirectoryName(TempFileName));
			File.Delete(TempFileName);
			using (FileStream ResponseFileStream = new FileStream(TempFileName, FileMode.Create, FileAccess.Write))
			{
				using (StreamWriter StreamWriter = new StreamWriter(ResponseFileStream))
				{
					foreach (string Line in Lines)
					{
						StreamWriter.WriteLine(Line);
					}
				}
			}
			return TempFileName;
		}
	}
}
