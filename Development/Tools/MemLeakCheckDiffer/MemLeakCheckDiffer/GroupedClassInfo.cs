/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using EpicCommonUtilities;
using System.Globalization;
using System.IO;

namespace MemLeakDiffer
{
	/// <summary>
	/// The units of the per-file samples
	/// </summary>
	public enum ESampleUnit
	{
		Seconds,
		Kilobytes,
	};

	/// <summary>
	/// Holds information about a reporting group:
	///    GroupName
	///    List of individual class names that should be aggregated into this group
	/// </summary>
	public class BaseGroupInfo : IComparable<BaseGroupInfo>
	{
		/// <summary>
		/// The type of the entry in each sample
		/// </summary>
		public ESampleUnit SampleUnitType = ESampleUnit.Kilobytes;

		/// <summary>
		/// Sort-key when sorting groups, where smaller values will appear nearer to the top of the report
		/// If two groups have the same priority, then they will be alpha-sorted instead
		/// </summary>
		public int PriorityIndex = 10;

        /// <summary>
        /// Is this an object list trend?
        /// </summary>
        public bool bIsObjListData = false;

		/// <summary>
		/// True if this group can be filtered by a size cutoff
		/// </summary>
		public bool bCanBeFilteredBySize = true;

		/// <summary>
		/// True if this group should always be excluded from the global summaries
		/// </summary>
		public bool bIsNotInterestingForSummary = false;

        /// <summary>
        /// True if this group should always be excluded from the standard report
        /// </summary>
        public bool bIsNotInterestingForStandardReport = false;

		/// <summary>
		/// Name of the group
		/// </summary>
		public string GroupName = null;

		/// <summary>
		/// The summary stat that best encapsulates this group and should be used in the overview report
		/// </summary>
		public EStatType OverviewStatType = EStatType.Maximum;

		/// <summary>
		/// Comparison by GroupName and PriorityIndex, used to alpha-sort groups
		/// </summary>
		public int CompareTo(BaseGroupInfo OtherGroup)
		{
			int DeltaPriority = PriorityIndex - OtherGroup.PriorityIndex;
			if (DeltaPriority == 0)
			{
				return GroupName.CompareTo(OtherGroup.GroupName);
			}
			else
			{
				return DeltaPriority;
			}
		}
	}

	public class GroupSummary : BaseGroupInfo
	{
		public StatisticalFloat Distribution = new StatisticalFloat();

		/// <summary>
		/// Creates a summary for a group
		/// </summary>
		public GroupSummary(GroupInfo Source)
		{
			CopyFrom(Source);

			foreach (double SampleValue in Source.SamplePerFile)
			{
				Distribution.AddSample(SampleValue);
			}
		}

		public GroupSummary(GroupSummary Source)
		{
			CopyFrom(Source);

			Distribution = Source.Distribution.Clone();
		}

		protected void CopyFrom(BaseGroupInfo Source)
		{
			PriorityIndex = Source.PriorityIndex;
			bCanBeFilteredBySize = Source.bCanBeFilteredBySize;
            bIsObjListData = Source.bIsObjListData;
			bIsNotInterestingForSummary = Source.bIsNotInterestingForSummary;
            bIsNotInterestingForStandardReport = Source.bIsNotInterestingForStandardReport;
			GroupName = Source.GroupName;
			OverviewStatType = Source.OverviewStatType;
			SampleUnitType = Source.SampleUnitType;
		}

		/// <summary>
		/// Merge the samples from another summary of the same group into this distribution
		/// </summary>
		/// <param name="Source"></param>
		public void MergeSamples(GroupSummary Source)
		{
			Debug.Assert(Source.GroupName == GroupName);
			Distribution.AddSamples(Source.Distribution);
		}

		public bool CanPassSizeFilter(double Threshold, bool GreaterThanThreshold)
		{
            if ((Distribution.Max == Distribution.Min) && ((int)Distribution.Max == MemStatsReportSection.INVALID_SIZE))
            {
                return false;
            }

			if (!bCanBeFilteredBySize)
			{
				return true;
			}
			else
			{
				double Value = Distribution.GetByType(OverviewStatType);
				return GreaterThanThreshold ? (Value >= Threshold) : (Value <= Threshold);
			}
		}

		public bool PassesInterestingForSummaryFilter(double Threshold, bool GreaterThanThreshold)
		{
            if ((Distribution.Max == Distribution.Min) && ((int)Distribution.Max == MemStatsReportSection.INVALID_SIZE))
            {
                return false;
            }
            
            if (bIsNotInterestingForSummary)
			{
				return false;
			}
			else
			{
				return CanPassSizeFilter(Threshold, GreaterThanThreshold);
			}
		}
	}

	/// <summary>
	/// Holds information about a reporting group, adding
	/// Summed sizes for the group in each of N .memlk files
	/// </summary>
	public class GroupInfo : BaseGroupInfo, IComparable<GroupInfo>
	{
		/// <summary>
		/// True if this size shouldn't be considered as part of the class sums (e.g., if it's bogus, or itself a meta-size)
		/// </summary>
		public bool bIgnoreSizes = false;

		/// <summary>
		/// List of samples for each file in this report
		/// </summary>
		public double[] SamplePerFile = null;

		/// <summary>
		/// List of class names that will contribute to this file
		/// </summary>
		public List<string> SourceClassNames = new List<string>();

		/// <summary>
		/// Constructs a group with just one class name
		/// </summary>
		public GroupInfo(string SourceClassName, int NumFiles)
		{
			GroupName = SourceClassName;
			SourceClassNames.Add(GroupName);
			ChangeFileCount(NumFiles);
		}

		/// <summary>
		/// Converts a particular sample to a string
		/// </summary>
		public string SampleToString(int FileIndex, double SizeScaler)
		{
			double SampleValue = SamplePerFile[FileIndex];

			switch (SampleUnitType)
			{
				case ESampleUnit.Kilobytes:
					return String.Format("{0:0.0}", SampleValue * SizeScaler);
				case ESampleUnit.Seconds:
					return String.Format("{0:0.0}", SampleValue);
				default:
					return SampleValue.ToString();
			}
		}

		/// <summary>
		/// Constructs an empty group, protected because the group should get some content
		/// </summary>
		/// <param name="NumFiles"></param>
		protected GroupInfo(int NumFiles)
		{
			ChangeFileCount(NumFiles);
		}

		/// <summary>
		/// Comparison by GroupName and PriorityIndex, used to alpha-sort groups
		/// </summary>
		public int CompareTo(GroupInfo OtherGroup)
		{
			return base.CompareTo(OtherGroup);
		}

		/// <summary>
		/// Creates an empty group with a given name
		/// </summary>
		public static GroupInfo CreateEmptyGroup(string InGroupName, int NumFiles)
		{
			GroupInfo Result = new GroupInfo(NumFiles);
			Result.GroupName = InGroupName;
			return Result;
		}

		/// <summary>
		/// Changes the number of files for which this group will store data.
		/// Warning: Destroys the size data
		/// </summary>
		public void ChangeFileCount(int NewFileCount)
		{
			SamplePerFile = new double[NewFileCount];
			for (int i = 0; i < NewFileCount; ++i)
			{
				SamplePerFile[i] = 0;
			}
		}

		/// <summary>
		/// Records a sample for a given file
		/// </summary>
		public void SetSamplePoint(int FileIndex, double SampleValue)
		{
			SamplePerFile[FileIndex] = SampleValue;
		}
	}
	
	/// <summary>
	/// This class holds grouped class information when building a report
	/// </summary>
	public class GroupedClassInfoTracker
	{
		// This map converts a class name into a group index (>= 0) or a 'filter this out' sentinel of -1
		// If the name isn't found at all, it's assumed to be an ungrouped stat that should still be considered
		// These ones will be added to the map
		protected Dictionary<string, int> ClassNameToGroupMap = new Dictionary<string, int>();
		public List<GroupInfo> Groups = new List<GroupInfo>();
		protected int NumFiles;
		public int SystemMemBar;
		public Color SystemMemBarColor;

		public GroupedClassInfoTracker(int InNumFiles)
		{
			NumFiles = InNumFiles;
		}

		public GroupInfo FindGroupByName(string Name)
		{
			foreach (GroupInfo Group in Groups)
			{
				if (Group.GroupName == Name)
				{
					return Group;
				}
			}

			return null;
		}

		/// <summary>
		/// Parses a file of the format:
		///   [Group] GroupName
		///   ClassName_1
		///   ClassName_2
		///   [Group] AnotherGroupName
		///   ClassName_3
		///   etc...
		/// </summary>
		public void ParseGroupFile(string GroupFilePath)
		{
            string[] Lines = new string[0];
            try
            {
                Lines = File.ReadAllLines(GroupFilePath);
            }
            catch (Exception)
            {
                Lines = new string[0];
            } 

            string GroupPrefix = "[Group] ";
			string SystemMemBarPrefix = "[SystemMemBarKB] ";
			string SystemMemBarColorPrefix = "[SystemMemBarColor] ";

			SystemMemBarColor = Color.Tomato;

			// Read all of the groups from the file
			GroupInfo ActiveGroup = null;
			foreach (string OriginalLine in Lines)
			{
				string Line = OriginalLine.Trim();
				if (Line.StartsWith(GroupPrefix))
				{
					// Add the previous group to the report
					if (ActiveGroup != null)
					{
						AddGroup(ActiveGroup);
					}

					// Parse out the name of the new group
					string GroupNamePlusTags = Line.Substring(GroupPrefix.Length);

					char[] Delimiters = { '?' };
					string[] GroupNameParts = GroupNamePlusTags.Split(Delimiters);
					string GroupName = GroupNameParts[0];

					ActiveGroup = GroupInfo.CreateEmptyGroup(GroupName, NumFiles);
                    ActiveGroup.bIsObjListData = true;

					foreach (string Part in GroupNameParts)
					{
						if (Part.Equals("Important", StringComparison.InvariantCultureIgnoreCase))
						{
							ActiveGroup.PriorityIndex = 0;
						}
						else if (Part.Equals("IgnoreSize", StringComparison.InvariantCultureIgnoreCase))
						{
							ActiveGroup.bIgnoreSizes = true;
						}
						else if (Part.Equals("NoSummary", StringComparison.InvariantCultureIgnoreCase))
						{
							ActiveGroup.bIsNotInterestingForSummary = true;
						}
						else if (Part.StartsWith("Priority", StringComparison.InvariantCultureIgnoreCase))
						{
							string Priority = "Priority";
							string ValueString = Part.Substring(Priority.Length + 1);

							int Value;
							if (int.TryParse(ValueString, out Value))
							{
								ActiveGroup.PriorityIndex = Value;
							}
						}
					}
				}
				else if (Line.StartsWith(SystemMemBarPrefix))
				{
					// Parse out the system memory bar
					string SystemMemoryBarString = Line.Substring(SystemMemBarPrefix.Length);
					int Value;
					if (int.TryParse(SystemMemoryBarString, out Value))
					{
						SystemMemBar = Value;
					}
				}
				else if (Line.StartsWith(SystemMemBarColorPrefix))
				{
					// Assumes the format is R,G,B w/ each component in the range [0..255]
					string SystemMemoryBarColorString = Line.Substring(SystemMemBarColorPrefix.Length);
					char[] Delimiters = { ',', ' ' };
					string[] SystemMemoryBarColorParts = SystemMemoryBarColorString.Split(Delimiters);
					// All 3 colors must be defined...
					if (SystemMemoryBarColorParts.Length >= 3)
					{
						int[] Value = { 0, 0, 0 };
						int CurrIndex = 0;
						foreach (string Part in SystemMemoryBarColorParts)
						{
							if (Part.Length > 0)
							{
								Value[CurrIndex++] = Convert.ToInt32(Part);
							}
						}
						SystemMemBarColor = Color.FromArgb(255, Value[0], Value[1], Value[2]);
					}
				}
				else if (Line.StartsWith("#"))
				{
					// Comment
				}
				else if (Line != "")
				{
					// This line is a class name for the active group
					if (ActiveGroup != null)
					{
						ActiveGroup.SourceClassNames.Add(Line);
					}
				}
			}

			if (ActiveGroup != null)
			{
				AddGroup(ActiveGroup);
			}
		}

		/// <summary>
		/// Adds a group to the report data collector
		/// </summary>
		/// <param name="NewGroup"></param>
		public int AddGroup(GroupInfo NewGroup)
		{
			int Index = Groups.Count;
			Groups.Add(NewGroup);

			foreach (string SourceName in NewGroup.SourceClassNames)
			{
				if (!ClassNameToGroupMap.ContainsKey(SourceName))
				{
					ClassNameToGroupMap.Add(SourceName, Index);
				}
			}

			return Index;
		}

		// Returns -1 if the class should be filtered and a non-zero index into ClassInfo otherwise
		public int GetIndexForClassName(string ClassName, bool bCreateIfMissing)
		{
			int Index;
			if (!ClassNameToGroupMap.TryGetValue(ClassName, out Index))
			{
				if (bCreateIfMissing)
				{
					GroupInfo NewGroup = new GroupInfo(ClassName, NumFiles);
					Index = AddGroup(NewGroup);
                    NewGroup.bIsObjListData = true;
                }
				else
				{
					Index = -1;
				}
			}
			return Index;
		}

		// Get the group for the given class
		public GroupInfo GetClassGroup(string ClassName, bool bCreateIfMissing)
		{
			int Index;
			if (ClassNameToGroupMap.TryGetValue(ClassName, out Index))
			{
				return Groups[Index];
			}
			return null;
		}

		// Register an observation
		public void RegisterSize(string ClassName, int FileIndex, double SizeKB)
		{
			int Index = GetIndexForClassName(ClassName, true);
			if (Index >= 0)
			{
				Debug.Assert(Groups[Index].SampleUnitType == ESampleUnit.Kilobytes);
				Groups[Index].SamplePerFile[FileIndex] += SizeKB;
			}
		}

		/// <summary>
		/// Creates a meta-group (no backing class names)
		/// </summary>
		public GroupInfo CreateMetaGroup(string InGroupName)
		{
			GroupInfo Metagroup = GroupInfo.CreateEmptyGroup(InGroupName, NumFiles);
			Metagroup.bIgnoreSizes = true;
			Metagroup.bCanBeFilteredBySize = false;
			Metagroup.SourceClassNames.Add("$" + InGroupName);
			AddGroup(Metagroup);
			return Metagroup;
		}
	}
}