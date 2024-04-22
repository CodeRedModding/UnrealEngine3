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
	/// Data container for serializing stats data to/from our XML files. These
	/// are the canonical instances of a given stats run's groups and individual
	/// stats. This class isn't used outside of serialization and initial processing
	/// </summary>
	public class StatFileDescription
	{
		/// <summary>
		/// The set of groups contained in the stat file
		/// </summary>
		[XmlArray]
		public Group[] Groups = new Group[0];
		/// <summary>
		/// The list used to manage recently added groups to the descriptions
		/// </summary>
		private ArrayList GroupList = new ArrayList();
		/// <summary>
		/// The set of stats that are contained in the stat file
		/// </summary>
		[XmlArray]
		public Stat[] Stats = new Stat[0];
		/// <summary>
		/// The list used to manage recently added stats to the descriptions
		/// </summary>
		private ArrayList StatList = new ArrayList();

		/// <summary>
		/// XML serialization requires a default ctor
		/// </summary>
		public StatFileDescription()
		{
		}

		/// <summary>
		/// Builds the hash tables for fast look ups of data
		/// </summary>
		/// <param name="GroupIdToGroup">Hash of group ids to group objects</param>
		/// <param name="StatIdToStat">Hash of stat ids to stat objects</param>
		public void FixupData(SortedList GroupIdToGroup,SortedList StatIdToStat)
		{
			// Go through each group and add the group to the fast lookup
			foreach (Group group in Groups)
			{
				GroupIdToGroup.Add(group.GroupId,group);
			}
			// Build the stat id to stat object mapping and set the group ref
			foreach (Stat stat in Stats)
			{
				StatIdToStat.Add(stat.StatId,stat);
				// Now do any per stat fix up
				stat.FixupData(GroupIdToGroup);
			}
		}

		/// <summary>
		/// Adds the new stat description to our descriptions list
		/// </summary>
		/// <param name="NewStat">The new stat description to add</param>
		public void AppendStat(Stat NewStat)
		{
			StatList.Add(NewStat);
		}

		/// <summary>
		/// Adds the new group description to our descriptions list
		/// </summary>
		/// <param name="NewGroup">The new group description to add</param>
		public void AppendGroup(Group NewGroup)
		{
			GroupList.Add(NewGroup);
		}

		/// <summary>
		/// Fixes up any stat description objects that were recently added
		/// </summary>
		/// <param name="GroupIdToGroup">Hash of group ids to group objects</param>
		/// <param name="StatIdToStat">Hash of stat ids to stat objects</param>
		private void FixupRecentStatDescriptions(SortedList GroupIdToGroup,
			SortedList StatIdToStat)
		{
			if (Stats.Length < StatList.Count)
			{
				// Go through each group and add the group to the fast lookup
				for (int Index = Stats.Length; Index < StatList.Count; Index++)
				{
					Stat stat = (Stat)StatList[Index];
					StatIdToStat.Add(stat.StatId,stat);
				}
				// Make a second pass through updating the parentage & groups as needed
				// Needs to be in a second pass so that all known stats are in the
				// hash
				for (int Index = Stats.Length; Index < StatList.Count; Index++)
				{
					Stat stat = (Stat)StatList[Index];
					// Now do any per stat fix up
					stat.FixupData(GroupIdToGroup);
				}
				// Rebuild the array from the recent list
				Stats = (Stat[])StatList.ToArray(typeof(Stat));
			}
		}

		/// <summary>
		/// Fixes up any group description objects that were recently added
		/// </summary>
		private void FixupRecentGroupDescriptions(SortedList GroupIdToGroup)
		{
			if (Groups.Length < GroupList.Count)
			{
				// Go through each group and add the group to the fast lookup
				for (int Index = Groups.Length; Index < GroupList.Count; Index++)
				{
					Group group = (Group)GroupList[Index];
					GroupIdToGroup.Add(group.GroupId,group);
				}
				// Rebuild the array from the recent list
				Groups = (Group[])GroupList.ToArray(typeof(Group));
			}
		}

		/// <summary>
		/// Used to fix up data on items that were added during the last set
		/// of packet processing
		/// </summary>
		/// <param name="GroupIdToGroup">Hash of group ids to group objects</param>
		/// <param name="StatIdToStat">Hash of stat ids to stat objects</param>
		public void FixupRecentItems(SortedList GroupIdToGroup,
			SortedList StatIdToStat)
		{
			// Update the group descriptions first since stats need them
			FixupRecentGroupDescriptions(GroupIdToGroup);
			// Update any stat description changes
			FixupRecentStatDescriptions(GroupIdToGroup,StatIdToStat);
		}
	}
}
