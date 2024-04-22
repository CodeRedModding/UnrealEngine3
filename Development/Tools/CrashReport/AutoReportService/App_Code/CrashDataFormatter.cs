using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace CrashDataFormattingRoutines
{

    public class CrashDataFormater
    {
    }

    public class CallStackEntry
    {
        string mFileName = "";
        string mFilePath = "";
        string mFunctionName = "";
        int? mLineNumber;

        public string FileName
        {
            get { return mFileName; }
        }

        public string FilePath
        {
            get { return mFilePath; }
        }

        public string FunctionName
        {
            get { return mFunctionName; }
        }

        public int? LineNumber
        {
            get { return mLineNumber; }
        }

        public CallStackEntry()
        {
        }

        public CallStackEntry(string FileName, string FilePath, string FuncName, int? LineNumber)
        {
            mFileName = FileName;
            mFilePath = FilePath;
            mFunctionName = FuncName;
            mLineNumber = LineNumber;
        }

        public string GetTrimmedFunctionName(int MaxLength)
        {
            
            var line = FunctionName;
            if (MaxLength > 0)
            {
                var length = line.Length;
                if (length > MaxLength) length = MaxLength;
                line = line.Substring(0, length);
            }
            return line;

        }

        public string GetTrimmedFunctionName()
        {
            int MaxLength = 40;
            var line = FunctionName;
            if (MaxLength > 0)
            {
                var length = line.Length;
                if (length > MaxLength) length = MaxLength;
                line = line.Substring(0, length);
            }
            return line;

        }
    }

    public class CallStackContainer
    {
        private static readonly Regex mRegex = new Regex(@"([^(]*[(][^)]*[)])([^\[]*([\[][^\]]*[\]]))*", RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled);

        private string mErrorMsg;
        private string mUnformattedCallStack;
        private IList<CallStackEntry> mEntries = new List<CallStackEntry>();
        private String[] Lines;

        public bool DisplayUnformattedCallStack;
        public bool DisplayFunctionNames;
        public bool DisplayFileNames;
        public bool DisplayFilePathNames;

        public string ErrorMessage
        {
            get { return mErrorMsg; }
        }

        public CallStackContainer(
            string CallStack,
            int FunctionParseCount,
            bool InDisplayFunctions,
            bool InDisplayFileNames)
        {
            ParseCallStack(CallStack, FunctionParseCount, InDisplayFunctions, InDisplayFileNames);
        }

        public IList<CallStackEntry> GetEntries()
        {
            return mEntries;
        }

        public CallStackEntry GetEntry(int position)
        {


            return mEntries.ElementAt(position);
        }

        public String[] GetLines()
        {
            return Lines;
        }

        private void ParseCallStack(
            string CallStack,
            int FunctionParseCount,
            bool InDisplayFunctions,
            bool InDisplayFileNames)
        {
            DisplayUnformattedCallStack = false;
            DisplayFunctionNames = InDisplayFunctions;
            DisplayFileNames = InDisplayFileNames;
            DisplayFilePathNames = false;

            if (CallStack == null)
            {
                //do nothing
            }
            else
            {
                mUnformattedCallStack = CallStack;

                Lines = CallStack.Split(new char[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);

                StringBuilder ErrorBldr = new StringBuilder();

                foreach (string CurLine in Lines)
                {
                    if (mEntries.Count >= FunctionParseCount)
                    {
                        break;
                    }

                    Match CurMatch = mRegex.Match(CurLine);

                    if (CurMatch.Success)
                    {
                        string FuncName = CurMatch.Groups[1].Value;
                        string FileName = "";
                        string FilePath = "";
                        int? LineNumber = null;

                        Group ExtraInfo = CurMatch.Groups[CurMatch.Groups.Count - 1];

                        if (ExtraInfo.Success)
                        {
                            const string FILE_START = "[File=";
                            //const string PATH_START = "[in ";

                            foreach (Capture CurCapture in ExtraInfo.Captures)
                            {
                                if (CurCapture.Value.StartsWith(FILE_START, StringComparison.OrdinalIgnoreCase))
                                {
                                    // -1 cuts off closing ]
                                    string Segment = CurCapture.Value.Substring(FILE_START.Length, CurCapture.Length - FILE_START.Length - 1);
                                    int LineNumSeparator = Segment.LastIndexOf(':');
                                    string LineNumStr = "";

                                    if (LineNumSeparator != -1)
                                    {
                                        LineNumStr = Segment.Substring(LineNumSeparator + 1);
                                        Segment = Segment.Substring(0, LineNumSeparator);
                                    }

                                    // if the path is corrupt we don't want the exception to fall through
                                    try
                                    {
                                        FileName = Path.GetFileName(Segment);
                                    }
                                    catch (Exception)
                                    {
                                        // if it fails just assign the file name to the segment
                                        FileName = Segment;
                                    }

                                    // if the path is corrupt we don't want the exception to fall through
                                    try
                                    {
                                        FilePath = Path.GetDirectoryName(Segment);
                                    }
                                    catch (Exception)
                                    {
                                    }

                                    int TempLineNum;

                                    if (int.TryParse(LineNumStr, out TempLineNum))
                                    {
                                        LineNumber = TempLineNum;
                                    }
                                }
                                //else if(CurCapture.Value.StartsWith(PATH_START, StringComparison.OrdinalIgnoreCase))
                                //{
                                //}
                            }
                        }

                        mEntries.Add(new CallStackEntry(FileName, FilePath, FuncName, LineNumber));
                    }
                    else
                    {
                        ErrorBldr.Append(CurLine);
                        ErrorBldr.Append('\n');
                    }
                }

                mErrorMsg = ErrorBldr.ToString();
            }
        }

        public string GetFormattedCallStack()
        {
            if (DisplayUnformattedCallStack)
            {
                return mUnformattedCallStack;
            }

            StringBuilder formattedCallStack = new StringBuilder(mErrorMsg);
            formattedCallStack.Append("<br>");

            foreach (CallStackEntry currentDesc in mEntries)
            {
                if (DisplayFunctionNames)
                {
                    formattedCallStack.AppendFormat("<b><font color=\"#151B8D\">{0}</font></b>", currentDesc.FunctionName);
                }

                if (DisplayFileNames && DisplayFilePathNames && currentDesc.FileName.Length > 0 && currentDesc.FilePath.Length > 0)
                {
                    formattedCallStack.Append(" --- ");

                    if (currentDesc.LineNumber.HasValue)
                    {
                        formattedCallStack.AppendFormat("<b>{0} line={1}</b>", Path.Combine(currentDesc.FilePath, currentDesc.FileName), currentDesc.LineNumber.Value.ToString());
                    }
                    else
                    {
                        formattedCallStack.AppendFormat("<b>{0}</b>", Path.Combine(currentDesc.FilePath, currentDesc.FileName));
                    }
                }
                else if (DisplayFilePathNames && currentDesc.FilePath.Length > 0)
                {
                    formattedCallStack.Append(" --- ");
                    formattedCallStack.AppendFormat("<b>{0}</b>", currentDesc.FilePath);
                }
                else if (DisplayFileNames && currentDesc.FileName.Length > 0)
                {
                    formattedCallStack.Append(" --- ");

                    if (currentDesc.LineNumber.HasValue)
                    {
                        formattedCallStack.AppendFormat("<b>{0} line={1}</b>", currentDesc.FileName, currentDesc.LineNumber.Value.ToString());
                    }
                    else
                    {
                        formattedCallStack.AppendFormat("<b>{0}</b>", currentDesc.FileName);
                    }
                }

                formattedCallStack.Append("<br>");
            }

            return formattedCallStack.ToString();
        }
    }
}
