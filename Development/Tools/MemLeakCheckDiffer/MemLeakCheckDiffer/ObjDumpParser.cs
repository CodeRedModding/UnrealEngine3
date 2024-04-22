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
using System.IO;
using System.Diagnostics;

namespace MemLeakDiffer
{
    /*
			Log( "Obj List: %s", Cmd );
			Log( "Objects:" );
			Log( "" );

            if (something)
            {
			    Log( TEXT("%100s % 10s % 10s % 10s"), TEXT("Object"), TEXT("NumBytes"), TEXT("MaxBytes"), TEXT("ResKBytes") );
  			    for( INT ObjIndex=0; ObjIndex<Objects.Num(); ObjIndex++ )
			    {
				    Ar.Logf( "%100s % 10i % 10i % 10i", *ObjItem.Object->GetFullName(), ObjItem.Num, ObjItem.Max, ObjItem.Object->GetResourceSize() / 1024 );
			    }
            }
  
            Log(" %100s % 6s % 10s % 10s % 10s", "Class", "Count", "NumBytes", "MaxBytes", "ResBytes" );
    
			for( INT i=0; i<List.Num(); i++ )
			{
    			Log(" %100s % 6i % 10iK % 10iK % 10iK",
                   *List(i).Class->GetName(), List(i).Count, List(i).Num/1024, List(i).Max/1024, List(i).Res/1024 );
			}
			Log( "%i Objects (%.3fM / %.3fM / %.3fM)", Total.Count, (FLOAT)Total.Num/1024.0/1024.0, (FLOAT)Total.Max/1024.0/1024.0, (FLOAT)Total.Res/1024.0/1024.0 );

Log:                                                                                                 Class  Count   NumBytes   MaxBytes   ResBytes
Log:                                                                                         AccessControl      1          1K          1K          0K
Log:                                                                                   AIVisibilityManager      1          0K          0K          0K
Log:                                                                                AmbientOcclusionEffect      2          0K          0K          0K
Log:                                                                                   AmbientSoundNonLoop      6          3K          3K          0K
Log:                                                                                    AmbientSoundSimple     52         26K         26K          0K

 8   Log: 154251 Objects (72.826M / 73.369M / 360.413M)
     */

	/// <summary>
	/// Resource information for a single class
	/// </summary>
	class FClassResourceInfo
	{
		/// <summary>
		///	The name of the class
		/// </summary>
		public string ClassName;
		/// <summary>
		///	The number of instances of the class
		/// </summary>
		public int Count;
		/// <summary>
		///	The number of bytes taken by the class
		/// </summary>
		public int NumkBytes;
		/// <summary>
		///	The maximum number of bytes taken by the class
		/// </summary>
		public int MaxkBytes;
		/// <summary>
		///	The number of bytes the class resources take
		/// </summary>
		public int ReskBytes;
		/// <summary>
		///	The true number of bytes the class resources take
		/// </summary>
		public int TrueReskBytes;

		public FClassResourceInfo()
		{
		}

		public void Empty()
		{
			ClassName = "";
			Count = 0;
			NumkBytes = 0;
			MaxkBytes = 0;
			ReskBytes = 0;
			TrueReskBytes = 0;
		}
	}

	/// <summary>
	/// Resource information for a group of classes
	/// </summary>
	class FClassGroupResourceInfo : IComparable<FClassGroupResourceInfo>
	{
		/// <summary>
		///	The name of the group
		/// </summary>
		public string GroupName;
		/// <summary>
		/// The 'true' (unedited) name of the group
		/// </summary>
		public string TrueGroupName;
		/// <summary>
		/// The data for the classes in this group
		/// </summary>
		public List<FClassResourceInfo> ClassInfoData = new List<FClassResourceInfo>();
		/// <summary>
		///	The number of bytes taken by the classes in this group
		/// </summary>
		public int NumkBytes;
		/// <summary>
		///	The maximum number of bytes taken by the classes in this group
		/// </summary>
		public int MaxkBytes;
		/// <summary>
		///	The number of bytes the class resources in this group take
		/// </summary>
		public int ReskBytes;
		/// <summary>
		///	The true number of bytes the class resources in this group take
		/// </summary>
		public int TrueReskBytes;
	
		public FClassGroupResourceInfo()
		{
		}

		public void Empty()
		{
			GroupName = "";
			NumkBytes = 0;
			MaxkBytes = 0;
			ReskBytes = 0;
			TrueReskBytes = 0;
		}

		public void SetGroupName(string InGroupName)
		{
			TrueGroupName = InGroupName;
			if (InGroupName.StartsWith("Y_") || 
				InGroupName.StartsWith("Z_"))
			{
				GroupName = InGroupName.Substring(2);
			}
			else
			{
				GroupName = InGroupName;
			}
		}

		public void AddGroupData(FClassResourceInfo InClassInfo)
		{
			// See if it is already in here... this may not be necessary
			bool bFound = false;
			foreach (FClassResourceInfo CheckClass in ClassInfoData)
			{
				if (CheckClass.ClassName == InClassInfo.ClassName)
				{
					bFound = true;
					break;
				}
			}

			if (bFound == false)
			{
				ClassInfoData.Add(InClassInfo);
			}
			else
			{
				// should we throw a warning here?
				Console.WriteLine("Warning: Class " + InClassInfo.ClassName + " already found in group " + GroupName);
			}
		}

		/// <summary>
		/// Comparison 
		/// </summary>
		public int CompareTo(FClassGroupResourceInfo Other)
		{
			int ComparisonResult = GroupName.CompareTo(Other.GroupName);
			if (ComparisonResult == 0)
			{
				ComparisonResult = TrueGroupName.CompareTo(Other.TrueGroupName);
			}
			return ComparisonResult;
		}
	}

    /// <summary>
    /// Represents a report line for a single class
    /// </summary>
    class ObjectDumpLine : ProcessedLine
    {
        /// <summary>
        /// This boolean controls how the ApproxTotalSizeKB property functions.
        /// If true, it returns it's best guess, which uses information from a TrueResKB column if available.
        /// If false, it returns a filtered estimate based on the traditional ResKB numbers that should be
        /// comparable between both old and new memlk files.
        /// </summary>
        public static bool bUseTrueResKB = false;

        // Constructs a dump line from a set of strings
        public static ObjectDumpLine Create(string[] cells, ref FClassResourceInfo ClassInfo)
        {
            ObjectDumpLine result = new ObjectDumpLine();
            result.MyName = cells[0];
            result.Count = int.Parse(cells[1]);
            result.NumKB = int.Parse(cells[2]);
            result.MaxKB = int.Parse(cells[3]);
            result.ResKB = int.Parse(cells[4]);

            result.CleanUpResKB();

            if (cells.Length > 5)
            {
                result.TrueResKB = int.Parse(cells[5]);
            }
            else
            {
                result.TrueResKB = result.ResKB;
            }

			ClassInfo.ClassName = result.MyName;
			ClassInfo.Count = result.Count;
			ClassInfo.NumkBytes = result.NumKB;
			ClassInfo.MaxkBytes = result.MaxKB;
			ClassInfo.ReskBytes = result.ResKB;
			ClassInfo.TrueReskBytes = result.TrueResKB;

            return result;
        }

        protected string MyName = null;

        public override string Name { get { return MyName; } }
        public int Count = 0;
        public int NumKB = 0;
        public int MaxKB = 0;
        public int ResKB = 0;
        public int TrueResKB = 0;

        void CleanUpResKB()
        {
            int EffectiveResKB = ResKB;

            // Nasty heuristics to clean up old data

            // Did it include the archiver in the results?  (theoretically unsafe)
            if ((ResKB == NumKB) || (ResKB == MaxKB))
            {
                // This one was one that used the archiver again for ResKB
                EffectiveResKB = 0;
            }

            // Is it one that just counts references to other objects?
            if (ClassesWithBogusResources.Contains(Name))
            {
                EffectiveResKB = 0;
            }
            else if (ClassesWithDoubleCountedBytes.Contains(Name))
            {
                // This one has valid data, but also archive junk
                EffectiveResKB -= NumKB;
            }

            ResKB = EffectiveResKB;
        }

        public int ApproxTotalSizeKB
        {
            get
            {
                int EffectiveResKB = bUseTrueResKB ? TrueResKB : ResKB;

                return MaxKB + EffectiveResKB;
            }
        }

        /// <summary>
        /// Initializes the lists of classes that need special treatment when bUseTrueResKB = false or no TrueResKB numbers are available
        /// </summary>
        public static void Init()
        {
            ClassesWithBogusResources.Add("DecalMaterial");
            ClassesWithBogusResources.Add("Font");
            ClassesWithBogusResources.Add("Material");
            ClassesWithBogusResources.Add("Model");
            ClassesWithBogusResources.Add("SoundCue");
            ClassesWithBogusResources.Add("AnimSequence");
            ClassesWithBogusResources.Add("AnimSet");

            ClassesWithDoubleCountedBytes.Add("LightMapTexture2D");
            ClassesWithDoubleCountedBytes.Add("ShadowMapTexture2D");
            ClassesWithDoubleCountedBytes.Add("Texture2D");
            ClassesWithDoubleCountedBytes.Add("SoundNodeWave");
        }

        static HashSet<string> ClassesWithBogusResources = new HashSet<string>();
        static HashSet<string> ClassesWithDoubleCountedBytes = new HashSet<string>();
    }

    /// <summary>
    /// Report section for an object list portion of the report
    /// </summary>
    class ObjDumpReportSection : GridReportSection
    {
		public FClassResourceInfo ObjectsTotalInfo = new FClassResourceInfo();
		public List<FClassResourceInfo> ClassResourceInfoData = new List<FClassResourceInfo>();
		public List<FClassGroupResourceInfo> ClassGroupResourceInfoData = new List<FClassGroupResourceInfo>();

		public FClassGroupResourceInfo FindClassGroupInfo(string InGroupName, bool bCreateIfMissing)
		{
			FClassGroupResourceInfo ClassGroupInfo = null;
			foreach (FClassGroupResourceInfo CheckInfo in ClassGroupResourceInfoData)
			{
				if (CheckInfo.TrueGroupName == InGroupName)
				{
					ClassGroupInfo = CheckInfo;
					break;
				}
			}

			if ((ClassGroupInfo == null) && (bCreateIfMissing == true))
			{
				// Hasn't been added yet...
				FClassGroupResourceInfo NewInfo = new FClassGroupResourceInfo();
				NewInfo.SetGroupName(InGroupName);
				ClassGroupResourceInfoData.Add(NewInfo);
				ClassGroupInfo = NewInfo;
			}

			return ClassGroupInfo;
		}

        public override void Cook()
        {
			ObjectsTotalInfo.Empty();

			// Pre-add all groups so they will always be in the Group list view
			GroupedClassInfoTracker Tracker = MemDiffer.GetClassGroupingData();
			if (Tracker != null)
			{
				// Add all the class groups
				FClassGroupResourceInfo GroupResInfo;
				foreach (GroupInfo Group in Tracker.Groups)
				{
					GroupResInfo = FindClassGroupInfo(Group.GroupName, true);
				}
				// Add the UNGROUPED group as well
				GroupResInfo = FindClassGroupInfo("UNGROUPED", true);
			}

			foreach (GridRow gr in MyItems.Rows)
            {
				FClassResourceInfo ClassInfo = new FClassResourceInfo();
				ObjectDumpLine line = ObjectDumpLine.Create(gr.Items, ref ClassInfo);
                MyProcessedRows.Add(line.Name, line);
				ClassResourceInfoData.Add(ClassInfo);

				// Find it in the groups...
				if (Tracker != null)
				{
					GroupInfo Group = Tracker.GetClassGroup(ClassInfo.ClassName, false);
					FClassGroupResourceInfo ClassGroupInfo = null;
					if (Group != null)
					{
						ClassGroupInfo = FindClassGroupInfo(Group.GroupName, true);
					}
					else
					{
						// Didn't find it??
						ClassGroupInfo = FindClassGroupInfo("*** UNGROUPED ***", true);
					}

					if (ClassGroupInfo != null)
					{
						// Add this class...
						ClassGroupInfo.ClassInfoData.Add(ClassInfo);
						ClassGroupInfo.MaxkBytes += ClassInfo.MaxkBytes;
						ClassGroupInfo.ReskBytes += ClassInfo.ReskBytes;
					}
					else
					{
						Console.WriteLine("*** FAILED TO FIND CLASSGROUPINFO FOR " + ClassInfo.ClassName);
					}
				}
				else
				{
					Console.WriteLine("Failed to get class info tracker!");
				}

				ObjectsTotalInfo.Count += ClassInfo.Count;
				ObjectsTotalInfo.NumkBytes += ClassInfo.NumkBytes;
				ObjectsTotalInfo.MaxkBytes += ClassInfo.MaxkBytes;
				ObjectsTotalInfo.ReskBytes += ClassInfo.ReskBytes;
				ObjectsTotalInfo.TrueReskBytes += ClassInfo.TrueReskBytes;
			}

			ClassGroupResourceInfoData.Sort();
        }
    }

    /// <summary>
    /// Parser for an object list
    /// </summary>
    class ObjDumpParser : SmartParser
    {
        public ObjDumpParser()
        {
        }

        /// <summary>
        /// Assumes all cells beyond the first might be numbers and removes and K suffixes from them
        /// Trims all cells.
        /// </summary>
        /// <param name="items"></param>
        protected void CleanupCells(ref string[] Items)
        {
            for (int i = 0; i < Items.Length; ++i)
            {
                Items[i] = Items[i].Trim();

                if (i > 0)
                {
                    char[] KAtEnd = {'K'};
                    Items[i] = Items[i].TrimEnd(KAtEnd);
                }
            }
        }

        public override ReportSection Parse(string[] Items, ref int i)
        {
            ObjDumpReportSection Result = new ObjDumpReportSection();

            // Parse out the command that generated the object list
            string Heading;
            if (!ReadLine(Items, ref i, out Heading))
            {
                return Result;
            }
            Result.MyHeading = Heading.Substring("Obj List: ".Length);


            // Skip specific object reports
            if (Heading.Contains("class="))
            {
                string RowString;
                while (ReadLine(Items, ref i, out RowString))
                {
                }

                return Result;
            }

            // The next two lines are worthless
            if (!Ensure(Items, ref i, "Objects:"))
            {
                return Result;
            }
            if (!Ensure(Items, ref i, ""))
            {
                return Result;
            }

            // The next line will be the start of the Object or List lists
            //@TODO: Fully handle the Object list

            // Handle the rows from the printout
            bool bKeepRunning = true;
            while (bKeepRunning)
            {
                bKeepRunning = false;

                // Handle the header for the list
                string[] Delimiters = { ", ", " " };
                string ColumnString;
                if (!ReadLine(Items, ref i, out ColumnString))
                {
                    return Result;
                }

                string[] Columns = ColumnString.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);
                Result.MyItems.Headers = new GridRow(Columns);


                string RowString;
                while (ReadLine(Items, ref i, out RowString))
                {
                    if (RowString == "")
                    {
                        bKeepRunning = true;
                        break;
                    }

                    if ((RowString.IndexOf(" Objects (") >= 0))
                    {
                        // Last line, summary information
                    }
                    else
                    {
                        // Table row
                        string[] Cells = RowString.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);


                        //@TODO: Because of the object list, we split off the first item if we have too many
                        if (Cells.Length != Columns.Length)
                        {
                            Cells[0] = Cells[1];
                            Cells[1] = "1";

                            Debug.Assert(Cells.Length - 1 == Columns.Length);
                        }

                        CleanupCells(ref Cells);

                        Result.MyItems.Rows.Add(new GridRow(Cells));
                    }
                }
            }

            return Result;
        }
    }
}