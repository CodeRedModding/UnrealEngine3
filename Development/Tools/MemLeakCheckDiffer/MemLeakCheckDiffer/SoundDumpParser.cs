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
using EpicCommonUtilities;

namespace MemLeakDiffer
{
    class SoundDumpLine : ProcessedLine
    {
        public float SizeKb;
        public int NumChannels;
        public string SoundName;
        public int bAllocationInPermanentPool;
        public string SoundClass;

        public override string Name { get { return SoundName; } }

        public SoundDumpLine()
        {
        }

        public SoundDumpLine(string[] Cells)
        {
            SizeKb = float.Parse(Cells[0]);
            NumChannels = int.Parse(Cells[1].Substring(0, Cells[1].IndexOf(' ')));
            SoundName = Cells[2];
            bAllocationInPermanentPool = (Cells.Length > 3) ? int.Parse(Cells[3]) : 0;
            SoundClass = (Cells.Length > 4) ? Cells[4] : "<unknown>";
        }
    }

    class SoundDumpReportSection : GridReportSection
    {
        public override void Cook()
        {
            int UnnamedCounter = 0;
            foreach (GridRow Row in MyItems.Rows)
            {
                SoundDumpLine NewLine = new SoundDumpLine(Row.Items);

                if (NewLine.SoundName == "")
                {
                    // FinalRelease_DebugConsole builds don't have names available for sounds
                    NewLine.SoundName = String.Format("UnnamedSound_{0:0000}", UnnamedCounter);
                    UnnamedCounter++;                   
                }
                
                if (!MyProcessedRows.ContainsKey(NewLine.Name))
                {
                    MyProcessedRows.Add(NewLine.Name, NewLine);
                }
                else
                {
                    Console.WriteLine("Warning: 'Listing all sounds.' section contains more than one entry with the name '{0}'.", NewLine.Name);
                }
            }
        }
    }

    /*
Log: Listing all sounds.
Log: , Size Kb, NumChannels, SoundName, bAllocationInPermanentPool, SoundClass
Log: ,    20.00, 1 channel(s), Ambient_Loops.Computers_G2.computer_noise03, 0, None
 */
    /// <summary>
    /// Parser for the 'listing all sounds' section of a memlk file (exec command ListSounds)
    /// </summary>
    class SoundDumpParser : SmartParser
    {
        public SoundDumpParser()
        {
        }

        protected void CleanupCells(ref string[] Items)
        {
            for (int i = 0; i < Items.Length; ++i)
            {
                Items[i] = Items[i].Trim();
            }
        }

        public override ReportSection Parse(string[] Items, ref int i)
        {
            SoundDumpReportSection Result = new SoundDumpReportSection();

            // Parse out the command that generated the object list
            Result.MyHeading = "Listing all sounds.";
            string HeadingLine;
            if (!ReadLine(Items, ref i, out HeadingLine))
            {
                return Result;
            }
            HeadingLine = HeadingLine.Trim();
            if ((HeadingLine != Result.MyHeading) && (!HeadingLine.Contains("bAllocationInPermanentPool")))
            {
                return Result;
            }

            // Handle the header for the list
            string[] Delimiters = { "," };
            string ColumnString;
            if (!ReadLine(Items, ref i, out ColumnString))
            {
                return Result;
            }

            string[] Columns = ColumnString.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);
            Result.MyItems.Headers = new GridRow(Columns);

            // Handle the rows from the printout
            string RowString;
            while (ReadLine(Items, ref i, out RowString))
            {
                if (RowString == "")
                {
                    // really last line, empty
                }
                else if (RowString.IndexOf("resident sounds") >= 0)
                {
                    // Last line, summary information
                }
                else
                {
                    // Table row
                    string[] Cells = RowString.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);
                    CleanupCells(ref Cells);
                    Debug.Assert(Cells.Length == Columns.Length);

                    Result.MyItems.Rows.Add(new GridRow(Cells));
                }
            }

            return Result;
        }
    }


    /// <summary>
    /// The section representing the output of the exec command 'ListSoundClasses'
    /// </summary>
    class SoundClassesSection : KeyValuePairSectionDefaultMatching
    {
        public SoundClassesSection()
            : base("Listing all sound classes.")
        {
            MyHeading = "SoundClasses";
            MyBasePriority = 31;
        }

        protected string SanitizeName(string Name)
        {
            return "SoundClass_" + Name.Replace(',', '_');
        }

        protected override void ProcessLine(string InLine)
        {
            char[] Delimiters = { ' ', '\'' };

            string[] Parts = InLine.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);

            // A well formed line should look like:
            //   Class 'Stinger' has 20 resident sounds taking 545.25 kb
            if (Parts.Length == 9)
            {
                string ClassName = Parts[1];
                string ClassCountStr = Parts[3];
                string ClassSizeStr = Parts[7];

                string SafeName = SanitizeName(ClassName);

                //Samples.Add(SafeName + "_Count", int.Parse(ClassCountStr));
                Samples.Add(SafeName, new SampleRecord(float.Parse(ClassSizeStr), MyBasePriority, EStatType.Maximum));
            }
        }
    }

    /// <summary>
    /// The parser for the exec command ListSoundClasses
    /// </summary>
    class SoundClassesParser : SmartParser
    {
        public override ReportSection Parse(string[] Items, ref int i)
        {
            SoundClassesSection Result = new SoundClassesSection();
            Result.Parse(Items, ref i);
            return Result;
        }
    }
}
