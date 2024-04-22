/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Diagnostics;
using System.Collections.Generic;
using System.Text;

namespace UnrealConsole
{
    class CrashReporter
    {
        public CrashReporter()
        {

        }

        /**
         * Finds Identifier in SearchTarget and returns the rest of the line after Identifier
         */
        private string FindLine(string Identifier, string SearchTarget)
        {
            int IdentifierStartIndex = SearchTarget.IndexOf(Identifier);
            string ResultString = "";
            if (IdentifierStartIndex >= 0)
            {
                int EndIndex = SearchTarget.IndexOf("\n", IdentifierStartIndex);
                if (EndIndex < 0)
                {
                    EndIndex = SearchTarget.Length;
                }
                int StartIndex = IdentifierStartIndex + Identifier.Length;
                ResultString = SearchTarget.Substring(StartIndex, EndIndex - StartIndex);
            }
            else
            {
                ResultString = "not found";
            }
            return ResultString;
        }

        /**
         * Finds the first non-empty line in SearchTarget, starting from StartIndex and going backwards
         */
        private string FindLastLine(int StartIndex, string SearchTarget)
        {
            int LineEndIndex = -1;
            int LastLineEndIndex = -1;
            int SearchStartIndex = StartIndex;
            int SizeofLineEnd = "\n".Length;
            int MinAcceptableLineLength = 4;
            
            do
            {
                LastLineEndIndex = LineEndIndex;
                // Find a line end token
                LineEndIndex = SearchTarget.LastIndexOf("\n", SearchStartIndex);
                // Update the position from where the search should start on the next iteration
                if (LineEndIndex <= SearchStartIndex && LineEndIndex - SizeofLineEnd >= 0)
                {
                    SearchStartIndex = LineEndIndex - SizeofLineEnd;
                }

            } while (
                // Continue searching as long as we are still finding lines
                LineEndIndex != -1 
                // and the most recently found line is too small to be acceptable
                && ((LastLineEndIndex - LineEndIndex < MinAcceptableLineLength) || LastLineEndIndex == -1));

            string ResultString = "";
            if (LineEndIndex != -1)
            {
                if (LastLineEndIndex != -1)
                {
                    // Extract the line, excluding the previous line end
                    ResultString = SearchTarget.Substring(LineEndIndex + SizeofLineEnd, LastLineEndIndex - LineEndIndex - SizeofLineEnd);
                }
                else
                {
                    ResultString = SearchTarget.Substring(LineEndIndex);
                }
            }
            return ResultString;
        }

        /**
         * Attempts to extract the assert message from the TTY output
         */
        private string ExtractAssert(string LogFileContents)
        {
            // Find token inserted by FLogWindow::HandleCrash that denotes the begining of the callstack
            int CallstackBeginIndex = LogFileContents.LastIndexOf("Detected a crash");
            if (CallstackBeginIndex == -1)
            {
                CallstackBeginIndex = LogFileContents.Length - 1;
            }

            // Look for known assert identifiers
            // "Assertion failed" from appFailAssertFunc()
            int AssertionBeginIndex = LogFileContents.LastIndexOf("Assertion failed", CallstackBeginIndex);
            if (AssertionBeginIndex == -1)
            {
                // "appError" from FOutputDeviceAnsiError::Serialize()
                AssertionBeginIndex = LogFileContents.LastIndexOf("appError", CallstackBeginIndex);
            }

            string AssertMessage = "";
            if (AssertionBeginIndex == -1)
            {
                // No assert identifier was found, get the last line of the log before the callstack
                AssertMessage = FindLastLine(CallstackBeginIndex, LogFileContents);
            }
            else
            {
                AssertMessage = LogFileContents.Substring(AssertionBeginIndex, CallstackBeginIndex - AssertionBeginIndex);
            }

            return AssertMessage;
        }

        /**
         * Removes some junk that the PS3 appends to the assert message
         * Does nothing on other platforms
         */
        private string FormatAssertMessage(string AssertMessage, string LogFileContents)
        {
            //find the first "lv2", cut off anything after that
            int PS3CrapStartIndex = AssertMessage.IndexOf("lv2");
            //find the "[PS3Callstack" garbage
            int PS3MoreCrapStartIndex = AssertMessage.IndexOf("[PS3Callstack");

            if (PS3MoreCrapStartIndex < PS3CrapStartIndex && PS3MoreCrapStartIndex != -1
                || PS3CrapStartIndex == -1)
            {
                PS3CrapStartIndex = PS3MoreCrapStartIndex;
            }

            string FormattedAssert;
            if (PS3CrapStartIndex >= 0)
            {
                FormattedAssert = AssertMessage.Substring(0, PS3CrapStartIndex - 0);
            }
            else
            {
                FormattedAssert = AssertMessage;
            }

            //assert message can't be empty, web site formatting depends on there being at least one line
            if (FormattedAssert.Length < 3)
            {
                int EndAssertIndex = LogFileContents.IndexOf(AssertMessage);
                if (EndAssertIndex > 0)
                {
                    //assume the assert is in the last couple of log lines before AssertMessage
                    int StartAssertIndex = 0;
                    int PreviousLineIndex = LogFileContents.LastIndexOf("\n", EndAssertIndex - 1);
                    if (PreviousLineIndex > 0)
                    {
                        StartAssertIndex = PreviousLineIndex;
                        PreviousLineIndex = LogFileContents.LastIndexOf("\n", PreviousLineIndex - 1);
                        if (PreviousLineIndex > 0)
                        {
                            StartAssertIndex = PreviousLineIndex;
                            PreviousLineIndex = LogFileContents.LastIndexOf("\n", PreviousLineIndex - 1);
                            if (PreviousLineIndex > 0)
                            {
                                StartAssertIndex = PreviousLineIndex;
                            }
                        }
                    }
                    FormattedAssert = LogFileContents.Substring(StartAssertIndex, EndAssertIndex - StartAssertIndex);
                }
                else
                {
                    FormattedAssert = "Couldn't find assert message";
                }
            }
            return FormattedAssert;
        }

		/// <summary>
		/// Formats the callstack to be consistent with VS Studio callstacks. This is necessary so the web site can format it correctly when viewing.
		/// </summary>
		/// <param name="CallStack">The callstack to format.</param>
		/// <returns>A formatted callstack.</returns>
        private string FormatCallStack(string CallStack)
        {
			string[] Lines = CallStack.Split(new string[] { "\r\n" }, StringSplitOptions.RemoveEmptyEntries);
			StringBuilder Bldr = new StringBuilder();

			foreach(string CurLine in Lines)
			{
				int BracketIndex = CurLine.IndexOf('[');

				if(BracketIndex != -1)
				{
					Bldr.Append(CurLine.Insert(BracketIndex + 1, "File="));
					Bldr.Append("\r\n");
				}
				else
				{
					Bldr.Append(CurLine);
					Bldr.Append("\r\n");
				}
			}

			return Bldr.ToString();
        }

        /**
         * Dumps out crash report information into temporary files and then starts up the autoreporter app and passes relevant 
         * filenames on the commandline.
         */
        public string SendCrashReport(string GameName, string TranslatedCallstack, string TTYOutput, string PlatformName, string MiniDumpLocation)
        {
            string ResultMessage = "Crash Report Successful!";

			if(MiniDumpLocation.Length == 0)
			{
				MiniDumpLocation = "noMiniDump";
			}

            try
            {
                //note: these are replicated in FOutputDeviceWindowsError::HandleError()
                //the dump format must be recognized by the autoreporter app, ReportFile::ParseReportFile()
                const string ReportDumpVersion = "3";
                const string ReportDumpFilename = "UE3AutoReportDump.txt";
                const string AutoReportExe = "AutoReporter.exe";
                const string LogFileName = "UnrealConsoleLogTransfer.txt";

                //use this string to identify the start of the current session
                int LogStartIndex = TTYOutput.LastIndexOf("Init: Version");
                //if we couldn't find the start of session identifier, just use the whole TTY output
                if (LogStartIndex < 0)
                {
                    LogStartIndex = 0;
                }
                
                //write out the log to a temporary file
                File.WriteAllText(LogFileName, TTYOutput);

                StringBuilder CrashReportDump = new StringBuilder(ReportDumpVersion + "\0");

				CrashReportDump.Append(System.Windows.Forms.SystemInformation.ComputerName);
				CrashReportDump.Append('\0');

				//make the name consistent with appUserName()
				CrashReportDump.Append(System.Windows.Forms.SystemInformation.UserName.Replace(".", ""));
				CrashReportDump.Append('\0');

                //skip Game name for now
				CrashReportDump.Append(GameName);
                CrashReportDump.Append('\0');

                //platform
				CrashReportDump.Append(PlatformName);
				CrashReportDump.Append('\0');

                //skip language for now
                CrashReportDump.Append("int\0");

                //build up a date string consistent with appSystemTimeString()
                //"2006.10.11-13.50.53"
				CrashReportDump.Append(DateTime.Now.ToString("yyyy.M.d-H.m.s"));
				CrashReportDump.Append('\0');

                //parse the engine version out of the TTY
                CrashReportDump.Append(FindLine("Version:", TTYOutput));
				CrashReportDump.Append('\0');

                //skip changelist version
                CrashReportDump.Append("0\0");

                //parse the commandline out of the log
				CrashReportDump.Append(FindLine("Command line:", TTYOutput));
				CrashReportDump.Append('\0');

                //skip base directory
                CrashReportDump.Append("n/a\0");

                //format the callstack consistent with VS Studio
				CrashReportDump.Append(FormatAssertMessage(ExtractAssert(TTYOutput), TTYOutput));
				CrashReportDump.Append(Environment.NewLine);
				CrashReportDump.Append(FormatCallStack(TranslatedCallstack));
				CrashReportDump.Append('\0');

                //assume we're in game mode
                CrashReportDump.Append("Game\0");

                //write out the temporary dump file with the accumulated information
                File.WriteAllText(ReportDumpFilename, CrashReportDump.ToString(), Encoding.Unicode);

                //send the temporary file names on the commandline, protect against spaces in the path names by surrounding with ""
                string AutoreportCommandline = "\"" + ReportDumpFilename + "\" \"" + LogFileName + "\" \"" + "noIniDumpYet" + "\" \"" + MiniDumpLocation + "\"";
                
                Process.Start(AutoReportExe, AutoreportCommandline);
            }
            catch (Exception e)
            {
                ResultMessage = "Couldn't send the crash report: " + e.Message;
            }
            return ResultMessage;
        }
    }
}
