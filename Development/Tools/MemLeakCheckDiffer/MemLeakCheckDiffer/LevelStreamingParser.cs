/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/*
Level Streaming:
 sp_azura_p
 SP_Azura_01 -  1.2 sec 		red loaded and visible
 SP_Azura_01_S -  0.8 sec 		red loaded and visible
 SP_Azura_07_S 		green Unloaded
 SP_Azura_07_BG -  0.1 sec 		red loaded and visible
 SP_Azura_08 		green Unloaded
 SP_Azura_08_S 		green Unloaded
 sp_tower_p -  1.9 sec 		purple (preloading)
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
	public enum ELoadedStatus
	{
		LOADSTATUS_Unknown,
		LOADSTATUS_Visible,					//TEXT( "red loaded and visible" );
		LOADSTATUS_MakingVisible,			//TEXT( "orange, in process of being made visible" );
		LOADSTATUS_Loaded,					//TEXT( "yellow loaded but not visible" );
		LOADSTATUS_UnloadedButStillAround,	//TEXT( "blue  (GC needs to occur to remove this)" );
		LOADSTATUS_Unloaded,				//TEXT( "green Unloaded" );
		LOADSTATUS_Preloading,				//TEXT( "purple (preloading)" );
	};

	public class FLoadedLevelInfo
	{
		/// <summary>
		///  The name of the level
		/// </summary>
		public string LevelName;
		/// <summary>
		/// The load time, if the status is loaded
		/// The percentage loaded if the status is loading
		/// </summary>
		public float LoadTime;
		/// <summary>
		/// The status of the level
		/// </summary>
		public ELoadedStatus Status;
		/// <summary>
		/// TRUE if the level is permanently loaded
		/// </summary>
		public bool bPermanent;

		public FLoadedLevelInfo()
		{
		}

		public string GetStatusString()
		{
			switch (Status)
			{
				case ELoadedStatus.LOADSTATUS_Visible:
					return "RED: loaded & visible";
				case ELoadedStatus.LOADSTATUS_MakingVisible:
					return "ORANGE: being made visible";
				case ELoadedStatus.LOADSTATUS_Loaded:
					return "YELLOW: loaded not visible";
				case ELoadedStatus.LOADSTATUS_UnloadedButStillAround:
					return "BLUE: GC needed";
				case ELoadedStatus.LOADSTATUS_Unloaded:
					return "GREEN: Unloaded";
				case ELoadedStatus.LOADSTATUS_Preloading:
					return "PURPLE: preloading";
				case ELoadedStatus.LOADSTATUS_Unknown:
				default:
					return "*** UNKNOWN ***";
			}
		}
	};

	/// <summary>
    /// Represents a report line for a single streaming level (name and loaded/loading/unloaded status)
    /// </summary>
    class StreamingLevelLine : ProcessedLine
    {
        string MyMapName;
        string MyStatus;

        public override string Name { get { return MyMapName; } }
        public string LoadedStatus { get { return MyStatus; } }

        public bool IsAtLeastPartiallyLoaded
        {
            get { return LoadedStatus != "Unloaded"; }
        }

        public StreamingLevelLine()
        {
        }

        public StreamingLevelLine(string[] Cells, ref FLoadedLevelInfo LoadedInfo)
        {
            MyMapName = Cells[0];
			LoadedInfo.LevelName = MyMapName;
			string MyColor = "";
            if (Cells.Length == 1)
            {
                MyStatus = "PERMANENT";
				LoadedInfo.bPermanent = true;
				MyColor = "red";
            }
            else if ((Cells.Length >= 6) && (Cells[1] == "-"))
            {
                // MapName - Time sec Color loaded [and visible]
                // MapName - Time sec Color (preloading)
                MyStatus = Cells[5];
				// Load time or percentage loaded
				LoadedInfo.LoadTime = (float)System.Convert.ToDouble(Cells[2]);
				MyColor = Cells[4];
            }
            else if (Cells.Length >= 3)
            {
                // MapName color Unloaded
                MyStatus = Cells[2];
				MyColor = Cells[1];
            }
            else
            {
                Console.WriteLine("Warning: Failed to parse status from StreamingLevelLine '{0}'", String.Join(" ", Cells));
                MyStatus = "<UNKNOWN>";
            }

			// Use the color to determine the status...
			if (MyColor.Length > 0)
			{
				if (MyColor.ToUpper() == "RED")
				{
					//TEXT( "red loaded and visible" );
					LoadedInfo.Status = ELoadedStatus.LOADSTATUS_Visible;
				} 
				else if (MyColor.ToUpper() == "ORANGE,")
				{
					//TEXT( "orange, in process of being made visible" );
					LoadedInfo.Status = ELoadedStatus.LOADSTATUS_MakingVisible;
				} 
				else if (MyColor.ToUpper() == "YELLOW")
				{
					//TEXT( "yellow loaded but not visible" );
					LoadedInfo.Status = ELoadedStatus.LOADSTATUS_Loaded;
				} 
				else if (MyColor.ToUpper() == "BLUE")
				{
					//TEXT( "blue  (GC needs to occur to remove this)" );
					LoadedInfo.Status = ELoadedStatus.LOADSTATUS_UnloadedButStillAround;
				} 
				else if (MyColor.ToUpper() == "GREEN")
				{
					//TEXT( "green Unloaded" );
					LoadedInfo.Status = ELoadedStatus.LOADSTATUS_Unloaded;
				} 
				else if (MyColor.ToUpper() == "PURPLE")
				{
					//TEXT( "purple (preloading)" );
					LoadedInfo.Status = ELoadedStatus.LOADSTATUS_Preloading;
				}
				else
				{
					// Unknown status color: Throw an exception...
					string ErrorMsg = "Unknown level streaming status color encountered: ";
					ErrorMsg += MyColor;
					throw new Exception(ErrorMsg);
				}
			}
        }
    }

    /// <summary>
    /// Represents a list of the currently loaded or loading streaming levels
    /// </summary>
    class LoadedLevelsSection : GridReportSection
    {
		public List<FLoadedLevelInfo> LoadedLevelsData = new List<FLoadedLevelInfo>();

        public override void Cook()
        {
            foreach (GridRow Row in MyItems.Rows)
            {
				FLoadedLevelInfo LoadedInfo = new FLoadedLevelInfo();
				StreamingLevelLine NewLine = new StreamingLevelLine(Row.Items, ref LoadedInfo);
                MyProcessedRows.Add(NewLine.Name, NewLine);
				LoadedLevelsData.Add(LoadedInfo);
            }
        }
    }

    /// <summary>
    /// Parser for the 'Level Streaming:' section of a mlc
    /// </summary>
    class LoadedLevelsParser : SmartParser
    {
        public override ReportSection Parse(string[] Items, ref int i)
        {
            // Create a new section for the parsed info
            LoadedLevelsSection Result = new LoadedLevelsSection();
            Result.MyHeading = "Level Streaming:";
            string[] Columns = {"MapName", "LoadStatus"};
            Result.MyItems.Headers = new GridRow(Columns);

            // Verify that the report header is in place
            Ensure(Items, ref i, "Level Streaming:");

            // Read all of the lines detailing streaming status
            char[] Delimiters = { ' ', '\t' };
            string RowString;
            while (ReadLine(Items, ref i, out RowString))
            {
                if (RowString != "")
                {
                    // Pull off the arrow that gets added if LevelPlayerIsInName == MapName
                    string PossiblePrefix = "->  ";
                    if (RowString.StartsWith(PossiblePrefix))
                    {
                        RowString = RowString.Substring(PossiblePrefix.Length);
                    }

                    // Split it into cells
                    string[] Cells = RowString.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);
                    Result.MyItems.Rows.Add(new GridRow(Cells));
                }
            }

            return Result;
        }
    }
}
