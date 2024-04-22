using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using System.IO;
using System.IO.Compression;

using System.Xml;
using System.Diagnostics;

namespace AndroidExpansionPackager
{
    class TOCEntry
    {
        public string name;
        public Int64 offset;
        public Int32 size;
        public Int64 fixupPosition;
        public byte[] data;
    }

    class Program
    {
        // String placed at start of .OBB file to identify it
        const String magicValue = "UE3AndroidOBB";

        static void WriteLine(string Msg, params object[] Parms)
        {
            string Final = string.Format(Msg, Parms);

            System.Diagnostics.Debug.WriteLine(Final);
            Console.WriteLine(Final);
        }

        static void ShowUsage()
        {
            WriteLine("Builds Android .OBB file");
            WriteLine("");
            WriteLine("AndroidExpansionPackager <Name>");
            WriteLine("");
            WriteLine("  Name           : Name of the game to be packaged");
            WriteLine("");
        }

        /// <summary>
        /// Main entry point
        /// </summary>
        /// <param name="Args">Command line arguments</param>
        /// <returns></returns>
        static int Main(string[] Args)
        {
            string GameName = "";
            string Filter = "";

            if (Args.Length == 0)
            {
                ShowUsage();
                return 1;
            }

            if (Args[0].StartsWith("-"))
            {
                ShowUsage();
                return 1;
            }

            // if any params were specified, parse them
            if (Args.Length > 0)
            {
                GameName = Args[0];

                for (int ArgIndex = 1; ArgIndex < Args.Length; ArgIndex++)
                {
                    if ((String.Compare(Args[ArgIndex], "-appendfilter", true) == 0))
                    {
                        // make sure there is another param after this one
                        if (Args.Length > ArgIndex + 1)
                        {
                            // the next param is the base directory
                            Filter = Args[ArgIndex + 1];

                            // skip over it
                            ArgIndex++;
                        }
                        else
                        {
                            WriteLine("Error: No filter specified (use -h to see usage).");
                            return 1;
                        }
                    }
                    else if (String.Compare(Args[ArgIndex], "-h", true) == 0)
                    {
                        ShowUsage();
                    }
                }
            }

            // Set path to Binaries (1 above /Android)
            Directory.SetCurrentDirectory(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location));
            Directory.SetCurrentDirectory("..");

            // Find the manifest file
            string ManifestPath = "..\\" + GameName + "\\Build\\Android\\java\\AndroidManifest" + Filter + ".xml";
            if (!File.Exists(Path.GetFullPath(ManifestPath)))
            {
                ManifestPath = "..\\Development\\Src\\Android\\java\\AndroidManifest" + Filter + ".xml";
            }

            if (!File.Exists(ManifestPath))
            {

                WriteLine("Could not find AndroidManifest" + Filter + ".xml!");
                return 1;
            }

            WriteLine("Packaging expansion file.");

            // Parse manifest to get package name and 
            XmlDocument XmlDoc = new XmlDocument();
            XmlDoc.Load(ManifestPath);

            // Get Element
            XmlNode ManifestNode = XmlDoc["manifest"];
            // Grab version number and package name
            string VersionNumber = ManifestNode.Attributes["android:versionCode"].Value;
            string PackageName = ManifestNode.Attributes["package"].Value;

            // Build OBB name
            string OBBName = "main." + VersionNumber + "." + PackageName + Filter + ".obb";

            string TOCPath = "..\\" + GameName + "\\AndroidTOC.txt";
            if (!File.Exists(TOCPath))
            {
                WriteLine("Could not find AndroidTOC.txt!");
                return 1;
            }

            TextReader reader = new StreamReader(TOCPath);

            List<TOCEntry> TOCList = new List<TOCEntry>();

            String entry = reader.ReadLine();
            while (entry != null)
            {
                String[] fields = entry.Split(' ');

                // a valid TOC entry should have 4 parts
                if (fields.Length == 4)
                {
                    TOCEntry newTOCEntry = new TOCEntry();
                    newTOCEntry.name = fields[2];

                    TOCList.Add(newTOCEntry);
                }
                entry = reader.ReadLine();
            }

            Directory.SetCurrentDirectory("..\\" + GameName);

            // actually generate compressed data from files
            foreach (TOCEntry currentEntry in TOCList)
            {
                using (MemoryStream memStream = new MemoryStream())
                {               
                    FileInfo fileInfo = new FileInfo(currentEntry.name);
                    FileStream inFile = fileInfo.OpenRead();

                    // save data size
                    currentEntry.size = (Int32)inFile.Length;

                    // copy data to entry
                    currentEntry.data = new byte[currentEntry.size];
                    inFile.Read(currentEntry.data, 0, (int)inFile.Length);

                   // WriteLine("{0} written to expansion file.", Path.GetFileName(inFile.Name).ToString()); 
                }
            }

            // Make sure assets directory exists
            if (!Directory.Exists("..\\Binaries\\Android\\assets"))
            {
                Directory.CreateDirectory("..\\Binaries\\Android\\assets");
            }

            // Create wad file
            FileStream OBBFile = File.Create("..\\Binaries\\Android\\assets\\" + OBBName);

            // Write identifier
            OBBFile.Write(Encoding.ASCII.GetBytes(magicValue), 0, (int)magicValue.Length);

            // Write number of entires in TOC
            OBBFile.Write(BitConverter.GetBytes((UInt32)TOCList.Count), 0, 4);

            // Build TOC
            foreach (TOCEntry currentEntry in TOCList)
            {
                // record name length
                OBBFile.Write(BitConverter.GetBytes((UInt32)(currentEntry.name.Length + 1)), 0, 4);

                OBBFile.Write(Encoding.ASCII.GetBytes(currentEntry.name), 0, (int)currentEntry.name.Length);
                OBBFile.WriteByte(0); // null terminate

                currentEntry.fixupPosition = OBBFile.Position;

                // leave space for offset and size
                byte[] dummyBytes = new byte[12];
                OBBFile.Write(dummyBytes, 0, 12);
            }

            // Build wad
            foreach (TOCEntry currentEntry in TOCList)
            {
                currentEntry.offset = (Int64)OBBFile.Position;
                OBBFile.Write(currentEntry.data, 0, (int) currentEntry.size);
            }

            // Go back and fix-up positions
            foreach (TOCEntry currentEntry in TOCList)
            {
                OBBFile.Position = currentEntry.fixupPosition;
                OBBFile.Write(BitConverter.GetBytes(currentEntry.offset), 0, 8);
                OBBFile.Write(BitConverter.GetBytes(currentEntry.size), 0, 4);
            }

            // Close file
            OBBFile.Close();

            WriteLine("{0} written to Binaries/Android/assets", OBBName);

            return 0;
        }
    }
}
