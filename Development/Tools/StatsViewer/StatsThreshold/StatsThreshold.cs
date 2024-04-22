/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Xml;
using System.Xml.Serialization;
using Stats;

namespace StatsThreshold
{
	/// <summary>
	/// Loads a stats XML file and verifies that the specified stat never
	/// exceeds the specified threshold
	/// </summary>
	class StatsThreshold
	{
		/// <summary>
		/// Enum of the type of threshold comparison to perform
		/// </summary>
		public enum EThresholdType
		{
			TT_Average,
			TT_Threshold,
			TT_Frame
		};
		/// <summary>
		/// The stats data contained in the file
		/// </summary>
		private StatFile StatsFile;
		/// <summary>
		/// The time period to compare against. If 0, then the whole of the file
		/// </summary>
		private double TimePeriod;
		/// <summary>
		/// The name of the stat to compare data for
		/// </summary>
		private string StatName;
		/// <summary>
		/// The threshold value below which the stat must stay
		/// </summary>
		private double ThresholdValue;
		/// <summary>
		/// The type of threshold test we are performing
		/// </summary>
		private EThresholdType ThresholdType;

		/// <summary>
		/// Constructor for a threshold test. Copies values, reads the file
		/// </summary>
		/// <param name="FileName">The name of the XML file to open</param>
		/// <param name="TestType">The type of test to run</param>
		/// <param name="TestTimePeriod">The time period to evaluate over</param>
		/// <param name="StatToTest">The name of the stat to test</param>
		/// <param name="Threshold">The value above which is bad</param>
		public StatsThreshold(string FileName,string TestType,double TestTimePeriod,
			string StatToTest,double Threshold)
		{
			// Get the XML data stream to read from
			Stream XmlStream = new FileStream(FileName,FileMode.Open,FileAccess.Read,
				FileShare.None,256 * 1024,false);
			// Creates an instance of the XmlSerializer class so we can
			// read the object graph
			XmlSerializer ObjSer = new XmlSerializer(typeof(StatFile));
			// Create an object graph from the XML data
			StatsFile = (StatFile)ObjSer.Deserialize(XmlStream);
			// Close the file so we don't needlessly hold onto the handle
			XmlStream.Close();
			StatsFile.FixupData();
			// Figure out the test type
			if (String.Compare(TestType,"avg",true) == 0)
			{
				ThresholdType = EThresholdType.TT_Average;
			}
			else if (String.Compare(TestType,"threshold",true) == 0)
			{
				ThresholdType = EThresholdType.TT_Threshold;
			}
			else if (String.Compare(TestType,"frame",true) == 0)
			{
				ThresholdType = EThresholdType.TT_Frame;
				TimePeriod = 0.0;
			}
			else
			{
				throw new Exception("Incorrect test type specified. \"avg\" or \"threshold\" are the valid options");
			}
			// Copy the rest
			TimePeriod = TestTimePeriod;
			ThresholdValue = Threshold;
			StatName = StatToTest;
		}

		/// <summary>
		/// Performs the threshhold test
		/// </summary>
		/// <returns>The number of times the test failed</returns>
		public int Test()
		{
			int FailureCount = 0;
			// Find the canonical stat for the stat name
			Stat stat = StatsFile.GetStatFromName(StatName);
			// See if we are checking the overall or not
			if (TimePeriod == 0.0)
			{
				AggregateStatData AggData = StatsFile.GetAggregateData(stat.StatId);
				if (CompareAggregateData(AggData) < 0)
				{
					FailureCount++;
				}
			}
			// Check for per frame analysis
			else if (ThresholdType == EThresholdType.TT_Frame)
			{
				// Work through all frames, checking the stat for each one
				foreach (Frame CurrentFrame in StatsFile.Frames)
				{
					// Find the frame's set of data for this stat
					PerFrameStatData PerFrameData = CurrentFrame.GetPerFrameStat(stat.StatId);
					if (PerFrameData != null && PerFrameData.Stats.Count > 0)
					{
						// Compare against the per frame aggregate
						if (ComparePerFrameStat(PerFrameData) < 0)
						{
							FailureCount++;
						}
					}
				}
			}
			else
			{
				double NextPeriod = TimePeriod;
				// Create a new aggregate object to hold the aggregate data
				AggregateStatData AggData = new AggregateStatData();
				// Work through all frames, dividing them into timeperiod chunks
				foreach (Frame CurrentFrame in StatsFile.Frames)
				{
					if (CurrentFrame.ElapsedTime > NextPeriod)
					{
						NextPeriod += TimePeriod;
						// Compare the time data
						if (CompareAggregateData(AggData) < 0)
						{
							FailureCount++;
						}
						// Reset the aggregate data
						AggData = new AggregateStatData();
					}
					// Find the frame's set of data for this stat
					PerFrameStatData PerFrameData = CurrentFrame.GetPerFrameStat(stat.StatId);
					if (PerFrameData != null)
					{
						// Add each occurance to the aggregate
						foreach (Stat StatInstance in PerFrameData.Stats)
						{
							// Add it to our aggregate data
							AggData += StatInstance;
						}
					}
				}
				// Compare any remaining time/data
				if (CompareAggregateData(AggData) < 0)
				{
					FailureCount++;
				}
			}
			return FailureCount;
		}

		/// <summary>
		/// Compares an aggregate data object against our threshold values
		/// </summary>
		/// <param name="AggData">The data to compare</param>
		/// <returns>-1 if failed, 0 if passed</returns>
		public int CompareAggregateData(AggregateStatData AggData)
		{
			int Return = -1;
			if (ThresholdType == EThresholdType.TT_Average)
			{
				// If the average is below the threshold
				if (ThresholdValue > AggData.Average)
				{
					Return = 0;
				}
			}
			else
			{
				// Compare the maximum with the threshold
				if (ThresholdValue > AggData.Max)
				{
					Return = 0;
				}
			}
			return Return;
		}

		/// <summary>
		/// Compares the per frame stat data with our threshhold value
		/// </summary>
		/// <param name="PerFrameStat">The frame's value for the stat in aggregate</param>
		/// <returns>-1 if failed, 0 otherwise</returns>
		public int ComparePerFrameStat(PerFrameStatData PerFrameStat)
		{
			int Result = -1;
			// Figure out if it is a counter or not
			if (((Stat)PerFrameStat.Stats[0]).Type == Stat.StatType.STATTYPE_CycleCounter)
			{
				// Now compare time against the threshold
				if (ThresholdValue > PerFrameStat.TotalTime)
				{
					Result = 0;
				}
			}
			else
			{
				// Now compare total against the threshold
				if (ThresholdValue > PerFrameStat.Total)
				{
					Result = 0;
				}
			}
			return Result;
		}

		/// <summary>
		/// Creates the worker class based upon the args passed in
		/// </summary>
		[STAThread]
		static int Main(string[] Args)
		{
			// Default to some process error
			int ErrorCode = -1;
			try
			{
				// Validate the number of args
				if (Args.Length == 5)
				{
					Console.WriteLine("Creating threshold test class...");
					// Arg0 is file name
					// Arg1 is avg or threshold or frame
					// Arg2 is the time period to evaluate
					// Arg3 is the stat name to look at
					// Arg4 is the threshold value to compare
					StatsThreshold ThresholdTest = new StatsThreshold(Args[0],Args[1],
						Convert.ToDouble(Args[2]) * 1000.0,Args[3],Convert.ToDouble(Args[4]));
					// Run the test
					ErrorCode = ThresholdTest.Test();
					if (ErrorCode == 0)
					{
						Console.WriteLine("Threshold test succeeded");
					}
					else
					{
						Console.WriteLine("Threshold test failed {0} times",ErrorCode);
					}
				}
				else
				{
					Console.WriteLine("Usage:\r\n\tStatsThreshold \"File\" \"avg | threshold | frame\" \"TimePeriod (0 = all)\" \"StatName\" \"Threshold value\"");
					Console.WriteLine("Example:\r\n\tStatsThreshold StatsRun1.xml avg 0 FrameTime 33.3");
				}
			}
			catch (Exception e)
			{
				// We default to it being broken, so just fall through
				Console.WriteLine("Exception is:\r\n" + e.ToString());
			}
			return ErrorCode != 0 ? -1 : 0;
		}
	}
}
