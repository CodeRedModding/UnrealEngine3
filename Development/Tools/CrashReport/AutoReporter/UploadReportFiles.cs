/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Net;
using System.IO;

namespace AutoReporter
{
    class UploadReportFiles
    {

        string StripPath(string PathName)
        {
            int LastBackSlashIndex = PathName.LastIndexOf("\\");
            int LastForwardSlashIndex = PathName.LastIndexOf("/");

            if (LastBackSlashIndex > LastForwardSlashIndex && LastBackSlashIndex >= 0)
            {
                return PathName.Substring(LastBackSlashIndex + 1);
            }
            else if (LastForwardSlashIndex >= 0)
            {
                return PathName.Substring(LastForwardSlashIndex + 1);
            }
            return PathName;
        }

        /** 
         * UploadFiles - uploads the two files using HTTP POST
         * 
         * @param LogFilename - the local log file
         * @param IniFilename - the local ini file
         * @param uniqueID - the id of the crash report associated with these files. 
         * @return bool - true if successful
         */
        public bool UploadFiles(string LogFilename, string IniFilename, string MiniDumpFilename, int uniqueID, OutputLogFile logFile)
        {
            logFile.WriteLine("Uploading files " + LogFilename + ", " + IniFilename + " and " + MiniDumpFilename);
			Boolean bWasSuccessful = true;
            try 
            {
                WebClient client = new WebClient();

                //If debugging use local server
#if DEBUG
                string UploadReportURL = Properties.Settings.Default.UploadURL_Debug;
#else
				string UploadReportURL = Properties.Settings.Default.UploadURL;
#endif

                client.Headers.Add("NewFolderName", uniqueID.ToString());

                //copy the log before trying to upload it, since the engine may still have it open for writing
                //in which case a direct upload would fail
                string TempLogFilename = LogFilename + "_AutoReportTemp.log";
            	
                try
                {
                    File.Copy(LogFilename, TempLogFilename, true);
                }
                catch (Exception e)
                {
                    logFile.WriteLine(e.Message);
                    logFile.WriteLine("Couldn't copy log to " + TempLogFilename + ", continuing...");
                }

                client.Headers.Add("LogName", StripPath(TempLogFilename));

                byte[] responseArray;

                try
                {
                    responseArray = client.UploadFile(UploadReportURL, "POST", TempLogFilename);
                }
                catch (WebException webEx)
                {
                    logFile.WriteLine(webEx.Message);
                    logFile.WriteLine(webEx.InnerException.Message);
                    logFile.WriteLine("Couldn't upload log, continuing...");
                }

                try
                {
                    File.Delete(TempLogFilename);
                }
                catch (Exception)
                {
                }

				client.Headers.Remove( "SaveFileName" );
				client.Headers.Add( "SaveFileName", "UE3AutoReportIniDump.txt" );

				try
                {
                    responseArray = client.UploadFile(UploadReportURL, "POST", IniFilename);
                }
                catch (WebException webEx)
                {
                    logFile.WriteLine(webEx.Message);
                    logFile.WriteLine(webEx.InnerException.Message);
                    logFile.WriteLine("Couldn't upload ini, continuing...");
                	bWasSuccessful = false;
                }

				client.Headers.Remove( "SaveFileName" );
                client.Headers.Add("SaveFileName", "MiniDump.dmp");

                try
                {
                    responseArray = client.UploadFile(UploadReportURL, "POST", MiniDumpFilename);
                }
                catch (WebException webEx)
                {
                    logFile.WriteLine(webEx.Message);
                    logFile.WriteLine(webEx.InnerException.Message);
                    logFile.WriteLine("Couldn't upload mini dump, continuing...");
                	bWasSuccessful = false;
                }

            } catch (WebException webEx) {
                logFile.WriteLine(webEx.Message);
                logFile.WriteLine(webEx.InnerException.Message);
				bWasSuccessful = false;
            }

			return bWasSuccessful;
        }
    }
}
