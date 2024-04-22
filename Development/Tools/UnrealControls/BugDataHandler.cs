using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Xml;
using System.Diagnostics;

namespace UnrealControls
{
    public static class BugDataHandler
    {
        private static string mProcessToLaunch = string.Empty;
        private static string mFileName = string.Empty;
        private static Dictionary<string, string> mBugFieldsAndData;

        /// <summary>
        /// Entry point for the Bug Handler. Takes the string of bug data submitted to the console
        /// from the product and calls supporting functions to parse and output the data
        /// </summary>
        /// <param name="Bug_Data">Bug data string containing all bug information to be exported
        /// to external bug submission tool. Field data Entries are separated by '|' and fields names
        /// and their data are separated via ':'</param>
        /// <returns></returns>
        public static string HandleBug(string BugData)
        {
            string ret_val = string.Empty;

            mBugFieldsAndData = new Dictionary<string, string>();

            ParseBugDataString(BugData);

            ret_val = OutputBugDataFile();

            if (File.Exists(mProcessToLaunch))
            {
                int error_code = RunExternalBugSubmitter();
                if(error_code != 0)
                {
                    ret_val = "ERROR: External program returned error code: " + error_code;
                }
            }
            
            return ret_val;
        }

        /// <summary>
        /// Writes an XML based file out to the current UnrealConsole working directory containing all
        /// bug information for consumption of external bug submission program
        /// </summary>
        /// <returns>File name of written file</returns>
        private static string OutputBugDataFile()
        {
            string bug_out_filename = "BUGDATA_" + new Random().Next() + ".ue3bug";
            while(File.Exists(bug_out_filename))
            {
                //If the file exists roll through some filenames
                bug_out_filename = "BUGDATA_" + new Random().Next() + ".ue3bug";
            }
            XmlDocument bug_out = new XmlDocument();
            bug_out.LoadXml("<BUG></BUG>");

            foreach (KeyValuePair<string, string> bug_field in mBugFieldsAndData)
            {
                bug_out["BUG"].AppendChild(bug_out.CreateNode(XmlNodeType.Element, bug_field.Key, null));
                bug_out["BUG"][bug_field.Key].InnerText = bug_field.Value;
            }

            bug_out.Save(bug_out_filename);

            return bug_out_filename;
        }


        /// <summary>
        /// If bug data string contains a "PROGRAM" entry this method will run that program 
        /// once the bug file has been written
        /// </summary>
        /// <returns>Returns exit code of called program</returns>
        private static int RunExternalBugSubmitter()
        {
            int ret_val = 0;

            Process bug_submitter = new Process();
            
            bug_submitter.StartInfo.FileName = mProcessToLaunch;
            bug_submitter.Start();
            bug_submitter.WaitForExit();

            ret_val = bug_submitter.ExitCode;

            return ret_val;
        }

        /// <summary>
        /// Parses the bug data string and adds the data into a dictionary for exporting.
        /// </summary>
        /// <param name="Bug_Data"></param>
        private static void ParseBugDataString(string Bug_Data)
        {    
            string field = string.Empty;
            string data = string.Empty;
            string[] split_instance;

            foreach (string split_string in Bug_Data.Split('|'))
            {
                if(split_string.Replace(" ", "").Length == 0)
                {
                    //Assume an empty data entry and continue
                    continue;
                }
                split_instance = split_string.Split(':');
                if (split_instance.Length != 2)
                {
                    //Assume end of line or fouled data point continue
                    continue;
                }
                field = split_instance[0];
                data = split_instance[1];

                if (field.Contains("PROGRAM"))
                {
                    mProcessToLaunch = data;
                }
                else
                {
                    if (!mBugFieldsAndData.ContainsKey(field))
                    {
                        mBugFieldsAndData.Add(field, data);
                    }
                    else
                    {
                        mBugFieldsAndData[field] += System.Environment.NewLine + data;
                    }
                }
            }
        }
    }
}
