/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Xml;
using System.Xml.Serialization;
using Stats;

namespace StatsComparison
{
	/// <summary>
	/// Loads the two specified XML files and compares the specified stat to
	/// determine if it is within an acceptable range. If the second file differs
	/// by too much, an error is returned
	/// </summary>
	class StatsComparison
	{
		/// <summary>
		/// The stats data contained in the first file
		/// </summary>
		private StatFile FirstFile;
		/// <summary>
		/// The stats data contained in the second file
		/// </summary>
		private StatFile SecondFile;
		/// <summary>
		/// The name of the stat to compare data for
		/// </summary>
		private string StatName;
		/// <summary>
		/// The maximum amount of difference allowed before returning an error
		/// </summary>
		private double PercentDiffAllowed;

		/// <summary>
		/// Reads the command line options and creates comparison class
		/// </summary>
		[STAThread]
		static int Main(string[] Args)
		{
			// Default to some process error
			int ErrorCode = -1;
			try
			{
				// Validate the number of args
				if (Args.Length == 4)
				{
					Console.WriteLine("Creating comparison class...");
					// Arg0 is the first file name
					// Arg1 is the second file name
					// Arg2 is the stat to compare
					// Arg3 is the % of difference before returning an error
					StatsComparison Comparer = new StatsComparison(Args[0],
						Args[1],Args[2],Convert.ToDouble(Args[3]) / 100.0);
					// Returns 0 if the values are within tolerance or not
					ErrorCode = Comparer.Compare();
					if (ErrorCode == 0)
					{
						Console.WriteLine("Comparison succeeded");
					}
					else
					{
						Console.WriteLine("Comparison failed");
					}
				}
				else
				{
					Console.WriteLine("Usage:\r\n\tStatsComparison \"FirstFile\" \"SecondFile\" \"StatName\" \"% difference allowed\"");
					Console.WriteLine("Example:\r\n\tStatsComparison Run1.xml Run2.xml FrameTime 4");
				}
			}
			catch (Exception e)
			{
				// We default to it being broken, so just fall through
				Console.WriteLine("Exception is:\r\n" + e.ToString());
			}
			return ErrorCode;
		}

		/// <summary>
		/// Converts a XML file into our StatFile object
		/// </summary>
		/// <param name="FileName">The XML file to open</param>
		/// <returns>The file that was opened</returns>
		StatFile ReadFile(string FileName)
		{
			// Get the XML data stream to read from
			Stream XmlStream = new FileStream(FileName,FileMode.Open,FileAccess.Read,
				FileShare.None,256 * 1024,false);
			// Creates an instance of the XmlSerializer class so we can
			// read the object graph
			XmlSerializer ObjSer = new XmlSerializer(typeof(StatFile));
			// Create an object graph from the XML data
			StatFile NewFile = (StatFile)ObjSer.Deserialize(XmlStream);
			// Close the file so we don't needlessly hold onto the handle
			XmlStream.Close();
			return NewFile;
		}

		/// <summary>
		/// Copies the values and reads the file data
		/// </summary>
		/// <param name="FirstFileName">The first file to compare</param>
		/// <param name="SecondFileName">The second file to compare</param>
		/// <param name="InStatName">the name of the stat to compare</param>
		/// <param name="PercentDiff">The amount of difference that causes a failure</param>
		StatsComparison(string FirstFileName,string SecondFileName,
			string InStatName,double PercentDiff)
		{
			// Copy the settings
			StatName = InStatName;
			PercentDiffAllowed = PercentDiff;
			// Now open the files
			FirstFile = ReadFile(FirstFileName);
			FirstFile.FixupData();
			SecondFile = ReadFile(SecondFileName);
			SecondFile.FixupData();
		}

		/// <summary>
		/// Compares the stat values from the two files and determines if they
		/// are within the specified tolerance
		/// </summary>
		/// <returns>0 if the stats are ok, 1 if the test fails</returns>
		int Compare()
		{
			double Diff = 0.0;
			// Get the stat ids individually in case they were changed between runs
			Stat stat1 = FirstFile.GetStatFromName(StatName);
			Stat stat2 = SecondFile.GetStatFromName(StatName);
			if (stat1 != null && stat2 != null)
			{
				int StatId1 = stat1.StatId;
				int StatId2 = stat2.StatId;
				// Now get the aggregate data for these
				AggregateStatData FirstFileData = FirstFile.GetAggregateData(StatId1);
				AggregateStatData SecondFileData = SecondFile.GetAggregateData(StatId2);
				// Calculate the differences
				Diff = 1.0 - (FirstFileData.Average / SecondFileData.Average);
			}
			else
			{
				// Log and throw an exception
				if (stat1 == null)
				{
					Console.WriteLine("Failed to find the specified stat {0} in the first file",StatName);
				}
				if (stat2 == null)
				{
					Console.WriteLine("Failed to find the specified stat {0} in the second file",StatName);
				}
				throw new Exception("Invalid stat specified");
			}
			// Is it within the threshold?
			return Diff >= Math.Abs(PercentDiffAllowed) ? 1 : 0;
		}
	}
}
