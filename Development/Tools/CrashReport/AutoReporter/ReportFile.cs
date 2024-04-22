/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;


namespace AutoReporter
{

    /**
     * ReportFileData - a container representing the data that a crash dump contains and must be sent to the ReportService
     */
    class ReportFileData
    {
        public int Version;
        public string ComputerName;
        public string UserName;
        public string GameName;
        public string PlatformName;
        public string LanguageExt;
        public string TimeOfCrash;
        public string BuildVer;
        public string ChangelistVer;
        public string CommandLine;
        public string BaseDir;
        public string CallStack;
        public string EngineMode;

        /**
         * IsValid - checks if this is valid to be sent to ReportService
         * 
         * @return bool - true if successful
         */
        public bool IsValid(OutputLogFile logFile) 
        {
            if (Version != 0 && !ComputerName.Equals("") && !UserName.Equals("") && !GameName.Equals("") 
                && !LanguageExt.Equals("") && !TimeOfCrash.Equals("") && !BuildVer.Equals("") && !ChangelistVer.Equals("") 
                && !BaseDir.Equals("") && !CallStack.Equals("") && !EngineMode.Equals(""))
            {
                return true;
            }

            logFile.WriteLine("Errors detected reading report dump!");
            return false;
        }
    }

    class ReportFile
    {
        private int lastIndex;

        private static string currentDumpVersion = "3";

        private string GetNextString(string inputString, OutputLogFile logFile)
        {
            if (lastIndex == 0)
            {
                lastIndex = inputString.IndexOf("\0", 0);
            }

            string returnString = "";
            try
            {
                int nextIndex = inputString.IndexOf("\0", lastIndex + 1);
                returnString = inputString.Substring(lastIndex + 1, nextIndex - lastIndex - 1);
                lastIndex = nextIndex;
                logFile.WriteLine(returnString);
            }
            catch (Exception e)
            {
                logFile.WriteLine(e.Message);
                return "";
            }

            return returnString;
        }

        /**
         * ParseReportFile - extracts a ReportFileData from a crash dump file
         * 
         * @param filename - the crash dump
         * @param reportData - the container to fill
         * 
         * @return bool - true if successful
         */
        public bool ParseReportFile(string filename, ReportFileData reportData, OutputLogFile logFile)
        {
            logFile.WriteLine("");
            logFile.WriteLine("Parsing report dump " + filename);
            logFile.WriteLine("\n\n");
            string readText = "";
            try
            {
                readText = File.ReadAllText(filename, Encoding.Unicode);
            }
            catch (Exception e)
            {
                logFile.WriteLine(e.Message);
                logFile.WriteLine("Failed to read report dump!");
                return false;
            }

            int firstIndex = readText.IndexOf("\0");
            string VersionStr = readText.Substring(0, firstIndex);
            logFile.WriteLine("Version = " + VersionStr);

            if (VersionStr.Equals(currentDumpVersion))
            {
                logFile.WriteLine("Dump is current version: " + currentDumpVersion);
                reportData.Version = Int32.Parse(VersionStr);
                reportData.ComputerName = GetNextString(readText, logFile);
                reportData.UserName = GetNextString(readText, logFile);
                reportData.GameName = GetNextString(readText, logFile);
                reportData.PlatformName = GetNextString(readText, logFile);
                reportData.LanguageExt = GetNextString(readText, logFile);
                reportData.TimeOfCrash = GetNextString(readText, logFile);
                reportData.BuildVer = GetNextString(readText, logFile);
                reportData.ChangelistVer = GetNextString(readText, logFile);
                reportData.CommandLine = GetNextString(readText, logFile);
                reportData.BaseDir = GetNextString(readText, logFile);
                reportData.CallStack = GetNextString(readText, logFile);
                reportData.EngineMode = GetNextString(readText, logFile);
            }
            else if (VersionStr.Equals("2"))
            {
                logFile.WriteLine("Dump is old but supported. DumpVer=" + VersionStr + " CurVer=" + currentDumpVersion);
                reportData.Version = Int32.Parse(VersionStr);
                reportData.ComputerName = GetNextString(readText, logFile);
                reportData.UserName = GetNextString(readText, logFile);
                reportData.GameName = GetNextString(readText, logFile);
                reportData.LanguageExt = GetNextString(readText, logFile);
                reportData.TimeOfCrash = GetNextString(readText, logFile);
                reportData.BuildVer = GetNextString(readText, logFile);
                reportData.ChangelistVer = GetNextString(readText, logFile);
                reportData.CommandLine = GetNextString(readText, logFile);
                reportData.BaseDir = GetNextString(readText, logFile);
                reportData.CallStack = GetNextString(readText, logFile);
                reportData.EngineMode = GetNextString(readText, logFile);
            }
            else
            {
                logFile.WriteLine("Outdated dump version " + VersionStr + "! Current Version is " + currentDumpVersion);
                return false;
            }

            logFile.WriteLine("\n\n");

            return reportData.IsValid(logFile);
        }
    }
}
