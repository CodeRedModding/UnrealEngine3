/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Xml;
using System.Xml.Serialization;
using System.Collections;

namespace Stats
{
	/// <summary>
	/// Data container for serializing stats data to/from our XML files. Each group
	/// is analagous to the groups in Unreal's stats. Used to organize sets of stats.
	/// </summary>
	public class Group : UIBaseElement
	{
		/// <summary>
		/// Contains a list of stats that are in this group. Can't be private because
		/// it causes problems with XML serialization & reflection
		/// </summary>
		[XmlIgnoreAttribute]
		public ArrayList OwnedStats = new ArrayList();
		/// <summary>
		/// The unique key for the group
		/// </summary>
		[XmlAttribute("ID")]
		public int GroupId;
		/// <summary>
		/// The display name for the stat group
		/// </summary>
		[XmlAttribute("N")]
		public string Name;

		
		/// <summary>
		/// XML serialization requires a default ctor
		/// </summary>
		public Group() :
			base(UIBaseElement.ElementType.GroupObject)
		{
		}

		/// <summary>
		/// Creates a group from a byte stream
		/// </summary>
		/// <param name="Data">The byte stream to build the group from</param>
		public Group(Byte[] Data) :
			base(UIBaseElement.ElementType.GroupObject)
		{
			int CurrentOffset = 2;
			// Packet looks like:
			// GD<GroupId><GroupNameLen><GroupName>
			GroupId = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
			Name = ByteStreamConverter.ToString(Data,ref CurrentOffset);
		}

		/// <summary>
		/// Adds a stat to this group
		/// </summary>
		/// <param name="NewStat">The new stat object to add to the list</param>
		public void AddStat(Stat NewStat)
		{
			OwnedStats.Add(NewStat);
		}
	}
}
