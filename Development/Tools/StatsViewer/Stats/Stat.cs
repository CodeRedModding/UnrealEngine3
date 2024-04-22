/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Xml;
using System.Xml.Serialization;
using System.Collections;
using System.Text;

namespace Stats
{
	/// <summary>
	/// Data container for serializing stats data to/from our XML files. Each stat
	/// object contains either the canonical data for a stat, or the data for a
	/// snapshot of that stat for a given sample
	/// </summary>
	public class Stat : UIBaseElement
	{
		/// <summary>
		/// Unique key for this stat
		/// </summary>
		[XmlAttribute("ID")]
		public int StatId;
		/// <summary>
		/// Friendly name of this stat
		/// </summary>
		[XmlAttribute("N")]
		public string Name;
		/// <summary>
		/// The various types of stats that are written
		/// </summary>
		public enum StatType
		{
			STATTYPE_CycleCounter,
			STATTYPE_AccumulatorFLOAT,
			STATTYPE_AccumulatorDWORD,
			STATTYPE_CounterFLOAT,
			STATTYPE_CounterDWORD,
			STATTYPE_Error
		};
		/// <summary>
		/// The two well known stats
		/// </summary>
		public enum StatIds
		{
			STAT_Error,
			STAT_Root
		};
		/// <summary>
		/// The type of stat this is
		/// </summary>
		[XmlAttribute("ST")]
		public int ParsedType;
		/// <summary>
		/// Holds the enumerated version of the stats type
		/// </summary>
		[XmlIgnoreAttribute]
		public StatType Type;
		/// <summary>
		/// The id of the thread that this stat was captured on
		/// </summary>
		[XmlAttribute("TID")]
		public int ThreadId;
		/// <summary>
		/// The instance id of this stat
		/// </summary>
		[XmlAttribute("IID")]
		public int InstanceId;
		/// <summary>
		/// The instance id of this stat's parent
		/// </summary>
		[XmlAttribute("PID")]
		public int ParentInstanceId;
		/// <summary>
		/// Ref to the parent stat object
		/// </summary>
		[XmlIgnoreAttribute]
		public Stat ParentStat;
		/// <summary>
		/// This is the list of children stats in the hierarchy
		/// </summary>
		[XmlIgnoreAttribute]
		public ArrayList Children = new ArrayList();
		/// <summary>
		/// The id of the group this stat belongs to
		/// </summary>
		[XmlAttribute("GID")]
		public int GroupId;
		/// <summary>
		/// Ref to the group that owns this stat
		/// </summary>
		[XmlIgnoreAttribute]
		public Group OwningGroup;
		/// <summary>
		/// The number of times this stat was called in the frame it was captured in
		/// </summary>
		[XmlAttribute("PF")]
		public int CallsPerFrame;
		/// <summary>
		/// The value of the stat. Treat everything as doubles for simplicity sake
		/// </summary>
		[XmlAttribute("V")]
		public double Value;
		/// <summary>
		/// The value of this stat in corrected milliseconds
		/// </summary>
		[XmlIgnoreAttribute]
		public double ValueInMS;
		/// <summary>
		/// The id of the color to use when drawing
		/// </summary>
		[XmlIgnoreAttribute]
		public int ColorId;

        /// <summary>
        /// True if you want it to show the graph in the graph panel
        /// </summary>
        public bool bShowGraph;

        /// <summary>
        /// The metadata for this stat
        /// </summary>
        [XmlIgnoreAttribute]
        public StatMetadata Metadata;

		/// <summary>
		/// XML serialization requires a default ctor
		/// </summary>
		public Stat() : base(UIBaseElement.ElementType.StatsObject)
		{
            bShowGraph = true;
		}

		/// <summary>
		/// Creates a stat instance from a byte stream
		/// </summary>
		/// <param name="PacketType">String containing the 2 character packet type</param>
		/// <param name="Data">The data sent from the server for this stats instance</param>
		public Stat(string PacketType,Byte[] Data) :
			base(UIBaseElement.ElementType.StatsObject)
		{
			int CurrentOffset = 2;
			// Get the stat id
			StatId = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			// Route to the correct class/handler based upon packet type
			switch (PacketType)
			{
				// Ux == Update an existing stat
				// SD == Stat description
				case "SD":
				{
					SerializeStatDescription(Data,CurrentOffset);
					break;
				}
				// UC == Update a cycle counter
				case "UC":
				{
					SerializeCycleCounter(Data,CurrentOffset);
					break;
				}
				// UD == Update a dword counter
				case "UD":
				{
					SerializeDwordCounter(Data,CurrentOffset);
					break;
				}
				// UF == Update a float counter
				case "UF":
				{
					SerializeFloatCounter(Data,CurrentOffset);
					break;
				}
			}
            
            bShowGraph = true;
		}

		/// <summary>
		/// Populates the members of this object from the data in the byte stream
		/// </summary>
		/// <param name="Data">The byte stream representing this object</param>
		private void SerializeCycleCounter(Byte[] Data,int CurrentOffset)
		{
			// Packet looks like:
			// UC<StatId><InstanceId><ParentInstanceId><ThreadId><Value><CallsPerFrame>
			// Get our instance id from the stream
			InstanceId = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			// Get the parent instance id from the stream
			ParentInstanceId = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			// Now the thread id
			ThreadId = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			// Read the value
			Value = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			// And finally the number of calls per frame
			CallsPerFrame = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
		}

		/// <summary>
		/// Populates the members of this object from the data in the byte stream
		/// </summary>
		/// <param name="Data">The byte stream representing this object</param>
		/// <param name="CurrentOffset">The offset into the stream to start at</param>
		private void SerializeFloatCounter(Byte[] Data,int CurrentOffset)
		{
			// This packet looks like: UF<float>
			Value = ByteStreamConverter.ToDouble(Data,ref CurrentOffset);
		}

		/// <summary>
		/// Populates the members of this object from the data in the byte stream
		/// </summary>
		/// <param name="Data">The byte stream representing this object</param>
		/// <param name="CurrentOffset">The offset into the stream to start at</param>
		private void SerializeDwordCounter(Byte[] Data,int CurrentOffset)
		{
			// This packet looks like: UF<dword>
			Value = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
		}

		/// <summary>
		/// Populates the members of this object from the data in the byte stream
		/// </summary>
		/// <param name="Data">The byte stream representing this object</param>
		/// <param name="CurrentOffset">The offset into the stream to start at</param>
		private void SerializeStatDescription(Byte[] Data,int CurrentOffset)
		{
			// Packet looks like:
			// SD<StatId><StatNameLen><StatName><StatType><GroupId>
			Name = ByteStreamConverter.ToString(Data,ref CurrentOffset);
			ParsedType = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			Type = (StatType)ParsedType;
			GroupId = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
		}

		/// <summary>
		/// Updates the stat type field and group
		/// </summary>
		/// <param name="GroupIdToGroup">Hash of group ids to group objects</param>
		public void FixupData(SortedList GroupIdToGroup)
		{
			// Convert the integer type to the enum. Can't be done via serialization
			// since the XML data would need the text equivalent to handle properly
			Type = (StatType)ParsedType;
			// Find the group from the hash
			OwningGroup = (Group)GroupIdToGroup[GroupId]; 
			// Add ourselves to the group
			OwningGroup.AddStat(this);

            Metadata = StatMetadataManager.GetMetadata(OwningGroup.Name, Name);
		}

		/// <summary>
		/// Updates the parent stat
		/// </summary>
		/// <param name="CycleCounterInstanceHash">The mapping of instance id to instance</param>
		public void FixupParent(SortedList CycleCounterInstanceHash)
		{
			if (ParentInstanceId > 0)
			{
				// Get our parent from the hash
				ParentStat = (Stat)CycleCounterInstanceHash[ParentInstanceId];
				if (ParentStat != null)
				{
					// Add this stat to the parent's list
					ParentStat.Children.Add(this);
				}
			}
		}

		/// <summary>
		/// Converts the cycles to milliseconds
		/// </summary>
		/// <param name="SecondsPerCycle">Used to convert raw cycles into ms</param>
		/// <param name="StatIdToStat">Hash of stat ids to stat objects</param>
		public void FixupData(double SecondsPerCycle,SortedList StatIdToStat)
		{
			// Get our canonical stat info from the hash
			Stat CannonicalStat = (Stat)StatIdToStat[StatId];
			
			// Copy information from the canonical stat
			Type = CannonicalStat.Type;
			OwningGroup = CannonicalStat.OwningGroup;
			Name = CannonicalStat.Name;

			// Convert to milliseconds
			ValueInMS = Value * SecondsPerCycle * 1000.0;
		}

		/// <summary>
		/// Returns the child stat that was requested
		/// </summary>
		/// <param name="StatId">The stat id of child to find</param>
		/// <returns>The child stat if found, otherwise null</returns>
		public Stat GetChildStat(int StatId)
		{
			foreach (Stat stat in Children)
			{
				if (stat.StatId == StatId)
				{
					return stat;
				}
			}
			return null;
		}

		/// <summary>
		/// Returns the child stat that was requested
		/// </summary>
		/// <param name="StatId">The name of the child stat to find</param>
		/// <returns>The child stat if found, otherwise null</returns>
		public Stat GetChildStat(string Name)
		{
			foreach (Stat stat in Children)
			{
				if (String.Compare(stat.Name,Name,true) == 0)
				{
					return stat;
				}
			}
			return null;
		}
	}
}
