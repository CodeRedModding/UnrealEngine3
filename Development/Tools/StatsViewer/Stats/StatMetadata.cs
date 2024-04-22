/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Xml;
using System.Xml.Serialization;
using System.Collections;
using System.Text;
using System.Collections.Generic;

namespace Stats
{
    /// <summary>
    /// Class containing metadata about a statistic (unit type, display scale, etc...)
    /// </summary>
    public class StatMetadata
    {
        public enum Unit
        {
            Cycles,
            Count,
            Seconds,
            Bytes
        }

        public static StatMetadata GetDefaultMetadata()
        {
            StatMetadata Result = new StatMetadata();
            Result.Units = Unit.Seconds; 
            Result.Scale = 1.0;
            return Result;
        }

        public Unit Units;
        public double Scale;
        public string Suffix = "";
    }

    /// <summary>
    /// Class that holds information about a suffix (e.g., KB, MB, ms, us, Hz, etc...)
    /// The scale is the value to multiply a number in it's natural units to be in the suffix units
    /// e.g., if Suffix=KB, then Scale=1/1024 as natural units are bytes
    /// </summary>
    class SuffixInfo
    {
        public StatMetadata.Unit BaseUnit;
        public double Scale;

        public SuffixInfo(StatMetadata.Unit InBaseUnit, double InScale)
        {
            BaseUnit = InBaseUnit;
            Scale = InScale;
        }
    }

    /// <summary>
    /// This class has a list of groups and stats to view together at once
    /// </summary>
    public class StatViewingPreset
    {
        public string Name;
        public string Description;

        protected List<string> InclusionList = new List<string>();
        protected List<string> ExclusionList = new List<string>();
        protected List<string> InclusionGroups = new List<string>();

        /// <summary>
        /// Sets up this preset based on the values in an XML node
        /// </summary>
        /// <param name="ParentNode"></param>
        public void ReadFromXml(XmlNode ParentNode)
        {
            Name = ParentNode.Attributes["name"].Value;
            Description = (ParentNode.Attributes["description"] != null) ? (ParentNode.Attributes["description"].Value) : "";

            char[] Comma = { ',' };
            foreach (XmlNode Node in ParentNode.ChildNodes)
            {
                string[] Entries = Node.InnerText.Split(Comma);
                if (Node.Name == "add")
                {
                    InclusionList.AddRange(Entries);
                }
                else if (Node.Name == "sub")
                {
                    ExclusionList.AddRange(Entries);
                }
                else if (Node.Name == "add_group")
                {
                    InclusionGroups.AddRange(Entries);
                }
            }
        }

        /// <summary>
        /// Writes out a single list as a node if it has any elements
        /// </summary>
        protected void WriteList(XmlDocument Doc, XmlNode ParentNode, List<string> Values, string ListName)
        {
            if (Values.Count > 0)
            {
                string ValueString = String.Join(",", Values.ToArray());

                XmlNode Node = Doc.CreateNode(XmlNodeType.Element, ListName, "");
                Node.InnerText = ValueString;

                ParentNode.AppendChild(Node);
            }
        }

        /// <summary>
        /// Writes this preset into an XML document
        /// </summary>
        public void SaveToXml(XmlDocument Doc, XmlNode Root)
        {
            XmlNode Node = Doc.CreateNode(XmlNodeType.Element, "preset", "");

            // Write out the name
            XmlAttribute NameAttr = Doc.CreateAttribute("name");
            NameAttr.Value = Name;
            Node.Attributes.Append(NameAttr);

            // Write out the description
            if (Description != "")
            {
                XmlAttribute DescAttr = Doc.CreateAttribute("description");
                DescAttr.Value = Description;
                Node.Attributes.Append(DescAttr);
            }

            // Write out the lists
            WriteList(Doc, Node, InclusionList, "add");
            WriteList(Doc, Node, InclusionGroups, "add_group");
            WriteList(Doc, Node, ExclusionList, "sub");

            // And add the preset node to the parent
            Root.AppendChild(Node);
        }
        

        /// <summary>
        /// Returns true if the specific stat passes the filtering criteria for this preset
        /// </summary>
        public bool PassesFilter(string Name, string Group)
        {
            return (InclusionList.Contains(Name) || InclusionGroups.Contains(Group)) && !ExclusionList.Contains(Name);
        }
    }

    class MetadataGroup
    {
        public string GroupName;
        public StatMetadata DefaultMetadata;
        public Dictionary<string, StatMetadata> Metadata = new Dictionary<string, StatMetadata>();
    }

    /// <summary>
    /// The metadata manager keeps track of metadata for stats, and has methods to load it from a configuration
    /// file.
    /// It also manages view presets.
    /// </summary>
    public class StatMetadataManager
    {
        /// <summary>
        /// Mapping from group name to metadata collection
        /// </summary>
        static Dictionary<string, MetadataGroup> MetadataGroups = new Dictionary<string, MetadataGroup>();

        /// <summary>
        /// List of suffixes
        /// </summary>
        static Dictionary<string, SuffixInfo> Suffixes = new Dictionary<string,SuffixInfo>();

        /// <summary>
        /// List of viewing presets
        /// </summary>
        static List<StatViewingPreset> Presets = new List<StatViewingPreset>();

        /// <summary>
        /// Is the preset list dirty (does it need to be saved at program exit)?
        /// </summary>
        static bool bPresetsDirty = false;

        /// <summary>
        /// Returns the list of presets.  Should only be used to iterate over them, never to modify them!
        /// </summary>
        public static List<StatViewingPreset> GetPresets()
        {
            return Presets;
        }

        /// <summary>
        /// Add a preset to the preset list, or replaces an existing one.
        /// In either case, it notes that the preset list is dirty and should be saved
        /// </summary>
        public static void AddPreset(StatViewingPreset NewPreset)
        {
            bPresetsDirty = true;
            for (int i = 0; i < Presets.Count; ++i)
            {
                if (Presets[i].Name == NewPreset.Name)
                {
                    Presets[i] = NewPreset;
                    return;
                }
            }
            Presets.Add(NewPreset);
        }

        /// <summary>
        /// Deletes a preset from the preset list and notes that the preset list is dirty and should be saved
        /// </summary>
        public static void RemovePreset(StatViewingPreset PresetToRemove)
        {
            Presets.Remove(PresetToRemove);
            bPresetsDirty = true;
        }

        /// <summary>
        /// Initialize the stat metadata system
        /// </summary>
        public static void Initialize()
        {
            Suffixes.Add("BYTES", new SuffixInfo(StatMetadata.Unit.Bytes, 1.0));
            Suffixes.Add("KB", new SuffixInfo(StatMetadata.Unit.Bytes, 1.0 / 1024.0));
            Suffixes.Add("MB", new SuffixInfo(StatMetadata.Unit.Bytes, 1.0 / (1024.0 * 1024.0)));
            Suffixes.Add("S", new SuffixInfo(StatMetadata.Unit.Seconds, 1.0));
            Suffixes.Add("SECONDS", new SuffixInfo(StatMetadata.Unit.Seconds, 1.0));
            Suffixes.Add("MILISECONDS", new SuffixInfo(StatMetadata.Unit.Seconds, 1e3));
            Suffixes.Add("MS", new SuffixInfo(StatMetadata.Unit.Seconds, 1e3));
            Suffixes.Add("US", new SuffixInfo(StatMetadata.Unit.Seconds, 1e6));
            Suffixes.Add("HERTZ", new SuffixInfo(StatMetadata.Unit.Cycles, 1.0));
            Suffixes.Add("HZ", new SuffixInfo(StatMetadata.Unit.Cycles, 1.0));
            Suffixes.Add("KHZ", new SuffixInfo(StatMetadata.Unit.Cycles, 1e3));
            Suffixes.Add("CYCLES", new SuffixInfo(StatMetadata.Unit.Cycles, 1.0));
            Suffixes.Add("COUNT", new SuffixInfo(StatMetadata.Unit.Count, 1.0));
        }

        /// <summary>
        /// Parses the Group, Name, Units, and Scale attributes of a node, creates a StatMetadata object from them and sticks it into Destination
        /// </summary>
        protected static void ParseMetadataNode(XmlNode Node)
        {
            StatMetadata Result = new StatMetadata();

            // Parse the group name
            string GroupName = Node.Attributes["group"].Value;

            MetadataGroup Group;
            if (!MetadataGroups.TryGetValue(GroupName, out Group))
            {
                Group = new MetadataGroup();
                Group.GroupName = GroupName;
                MetadataGroups.Add(GroupName, Group);
            }

            // Parse the name attribute.  If absent, this is a default entry for the entire group
            string Name = "";
            XmlAttribute NameAttr = Node.Attributes["name"];
            if (NameAttr != null)
            {
                Name = NameAttr.Value;
                Group.Metadata.Add(Name, Result);
            }
            else
            {
                if (Group.DefaultMetadata != null)
                {
                    Console.WriteLine("Warning: The group '{0}' has more than one default metadata entry.", GroupName);
                }
                Group.DefaultMetadata = Result;
            }

            // Parse the units
            string UnitString = Node.Attributes["units"].Value;
            UnitString = UnitString.ToUpperInvariant();

            SuffixInfo UnitInfo;
            if (!Suffixes.TryGetValue(UnitString, out UnitInfo))
            {
                Console.WriteLine("Warning: Failed to parse units ('{0}') for stat metadata '{1}'.", UnitString, Name);

                StatMetadata Default = StatMetadata.GetDefaultMetadata();
                UnitInfo = new SuffixInfo(Default.Units, Default.Scale);
            }

            Result.Units = UnitInfo.BaseUnit;
            Result.Scale = 1.0 / UnitInfo.Scale;

            // Parse the scale / suffix
            string SuffixStringOriginalCase = (Node.Attributes["scale"] != null) ? Node.Attributes["scale"].Value : "1.0";
            string ScaleString = SuffixStringOriginalCase.ToUpperInvariant();

            double SuffixScale;
            if (double.TryParse(ScaleString, out SuffixScale))
            {
                Result.Scale *= SuffixScale;
            }
            else
            {
                SuffixInfo ScaleInfo;
                if (Suffixes.TryGetValue(ScaleString, out ScaleInfo))
                {
                    if (ScaleInfo.BaseUnit == UnitInfo.BaseUnit)
                    {
                        Result.Scale *= ScaleInfo.Scale;
                        if (Result.Scale != 1.0)
                        {
                            Result.Suffix = SuffixStringOriginalCase;
                        }
                    }
                    else
                    {
                        Console.WriteLine("Warning: Unit mismatch between scale ('{0}') and unit type ('{1}') for for stat metadata '{2}'.", ScaleString, UnitString, Name);
                    }
                }
                else
                {
                    Console.WriteLine("Warning: Failed to parse scale ('{0}') for stat metadata '{1}'.", ScaleString, Name);
                }
            }
        }

        /// <summary>
        /// Parses all of the entries in a settings node
        /// </summary>
        /// <param name="Filename"></param>
        protected static void ParseSettingsNode(XmlNode ParentNode)
        {
            foreach (XmlNode Node in ParentNode.ChildNodes)
            {
                if (Node.Name == "metadata")
                {
                    ParseMetadataNode(Node);
                }
                else if (Node.Name == "preset")
                {
                    StatViewingPreset Preset = new StatViewingPreset();
                    Preset.ReadFromXml(Node);
                    
                    Presets.Add(Preset);
                }
                else if (Node.NodeType != XmlNodeType.Comment)
                {
                    Console.WriteLine("Warning: Unknown tag '{0}' in settings section.", Node.Name);
                }
            }
        }

        /// <summary>
        /// Loads a settings or preset file from disk
        /// </summary>
        public static void LoadSettings(string Filename)
        {
            try
            {
                // Open and read the settings file
                XmlDocument Doc = new XmlDocument();
                Doc.Load(Filename);
                XmlNodeList Nodes = Doc.GetElementsByTagName("settings");

                // Parse the settings
                foreach (XmlNode Node in Nodes)
                {
                    ParseSettingsNode(Node);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Failed to open or parse settings file '{0}'.  Error: '{1}'", Filename, ex.Message);
            }
        }

        /// <summary>
        /// Retrieves the metadata for a given stat, or the default metadata if none exists.
        /// </summary>
        public static StatMetadata GetMetadata(string GroupName, string StatName)
        {
            MetadataGroup GroupInfo;
            if (MetadataGroups.TryGetValue(GroupName, out GroupInfo))
            {
                StatMetadata Result;
                if (GroupInfo.Metadata.TryGetValue(StatName, out Result))
                {
                    return Result;
                }
                else if (GroupInfo.DefaultMetadata != null)
                {
                    return GroupInfo.DefaultMetadata;
                }
            }

            return StatMetadata.GetDefaultMetadata();
        }

        /// <summary>
        /// Saves the presets to disk if they have changed or the force parameter is set
        /// </summary>
        /// <param name="Filename"></param>
        public static void SavePresets(string Filename, bool bForceSave)
        {
            if (bPresetsDirty || bForceSave)
            {
                try
                {
                    XmlDocument Doc = new XmlDocument();

                    XmlNode Root = Doc.CreateNode(XmlNodeType.Element, "settings", "");
                    Doc.AppendChild(Root);

                    foreach (StatViewingPreset Preset in Presets)
                    {
                        Preset.SaveToXml(Doc, Root);
                    }
                    
                    Doc.Save(Filename);
                    bPresetsDirty = false;
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Failed to save presets file '{0}'.  Error: '{1}'", Filename, ex.Message);
                }
            }
        }
    }
}