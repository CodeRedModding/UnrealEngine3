/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Xml;
using System.Xml.Serialization;
using System.Collections;
using System.Collections.Generic;

namespace Stats
{
	/// <summary>
	/// Data container for serializing stats data to/from our XML files. This is
	/// the full data tree for processing a set of stats
	/// </summary>
	public class StatFile
	{
		/// <summary>
		/// This is a fast look up using the group id as the key to find the object
		/// </summary>
		private SortedList GroupIdToGroup = new SortedList();
		/// <summary>
		/// This is a fast look up using the stat id as the key to find the object
		/// </summary>
		private SortedList StatIdToStat = new SortedList();
		/// <summary>
		/// Holds the overall aggregate data map for all stats
		/// </summary>
		private SortedList StatIdToAggData = new SortedList();
		/// <summary>
		/// Used to convert raw cycle counters into milliseconds
		/// </summary>
		[XmlAttribute]
		public double SecondsPerCycle;
		/// <summary>
		/// Holds the set of descriptions. List of all known groups and stats
		/// </summary>
		public StatFileDescription Descriptions = new StatFileDescription();
		/// <summary>
		/// This is the set of all frames that were captured during the session
		/// </summary>
		[XmlArray("Frames")]
		public Frame[] Frames = new Frame[0];
		/// <summary>
		/// Holds the list of items that were added via network packets
		/// </summary>
		private ArrayList FrameList = new ArrayList();


		/// <summary>
		/// Warning messages generated from the data repair process
		/// </summary>
		public List<string> RepairWarningMessages = new List<string>();


		/// <summary>
		/// XML serialization requires a default ctor
		/// </summary>
		public StatFile()
		{
		}


		/// <summary>
		/// Fixes up the stats data for a given frame
		/// </summary>
		/// <param name="frame">The frame to fix up</param>
		/// <param name="LastFrameTime">The elapsed frame time</param>
		private void FixupFrame( Frame frame, ref double LastFrameTime )
		{
			// Fixup each stat and build aggregate data
			foreach( Stat stat in frame.Stats )
			{
				// Move from platform specific cycles to milliseconds
				stat.FixupData( SecondsPerCycle, StatIdToStat );

				// Get the aggregate object that we are updating
				AggregateStatData AggData = (AggregateStatData)StatIdToAggData[ stat.StatId ];

				// Update the aggregate data with this stat instance
				AggData += stat;
			}

			// Get the id of the frame time stat
			int FrameTimeId = GetStatFromName( "FrameTime" ).StatId;

			// Place the stats in a fast search structure
			LastFrameTime = frame.FixupData( LastFrameTime, FrameTimeId );


			// Repair data problems
			foreach( Stat CurStat in frame.Stats )
			{
				RecursivelyFixBadInclusiveTimes( CurStat, frame.FrameNumber, ref RepairWarningMessages );
			}
		}


		/// <summary>
		/// Recursively repairs timing data for bad inclusive times
		/// </summary>
		/// <param name="InStat">Root stat value to repair</param>
		private void RecursivelyFixBadInclusiveTimes( Stat InStat, int InFrameNumber, ref List<string> RepairWarningMessages )
		{
			// Fix up all of the children first, so that we have proper child inclusive times to look at
			foreach( Stat ChildStat in InStat.Children )
			{
				RecursivelyFixBadInclusiveTimes( ChildStat, InFrameNumber, ref RepairWarningMessages );
			}

			if( InStat.Type == Stat.StatType.STATTYPE_CycleCounter )
			{
				// Sum up total time of child calls
				double TotalChildValue = 0.0;
				double TotalChildTimeInMS = 0.0;
				foreach( Stat ChildStat in InStat.Children )
				{
					if( ChildStat.Type == Stat.StatType.STATTYPE_CycleCounter )
					{
						TotalChildValue += ChildStat.Value;
						TotalChildTimeInMS += ChildStat.ValueInMS;
					}									   
				}


				{
					// Check for stat reporting errors
					if( InStat.Value < TotalChildValue )
					{
						// OK for some reason the child times sum up to more than their parent's total
						// inclusive time.  This should never really happen!

						// @todo: In these cases we can assume our Value is bogus and we no longer have an
						//    accurate picture of this stat's Exclusive time!

#if ALLOW_FRAME_WARNINGS
						RepairWarningMessages.Add( "Frame " + InFrameNumber.ToString() + ": " + InStat.Name + " with inclusive value of " + InStat.Value.ToString( "F1" ) + " but children have total value of " + TotalChildValue.ToString( "F1" ) );
#endif

						InStat.Value = TotalChildValue;
					}

					// Check for stat reporting errors
					if( InStat.ValueInMS < TotalChildTimeInMS )
					{
						// OK for some reason the child times sum up to more than their parent's total
						// inclusive time.  This should never really happen!

						// @todo: In these cases we can assume our Value is bogus and we no longer have an
						//    accurate picture of this stat's Exclusive time!

#if ALLOW_FRAME_WARNINGS
						RepairWarningMessages.Add( "Frame " + InFrameNumber.ToString() + ": " + InStat.Name + " with inclusive time of " + InStat.ValueInMS.ToString( "F1" ) + " but children have total time of " + TotalChildTimeInMS.ToString( "F1" ) );
#endif

						InStat.ValueInMS = TotalChildTimeInMS;
					}
				}
			}
		}


		/// <summary>
		/// Verifies that there is an aggregate object for each stat type
		/// </summary>
		private void CreateAggregates()
		{
			foreach (Stat stat in Descriptions.Stats)
			{
				// Check for an aggregate data object for this stat
				if (StatIdToAggData.ContainsKey(stat.StatId) == false)
				{
					// Create a new aggregated data class
					StatIdToAggData.Add(stat.StatId,new AggregateStatData());
				}
			}
		}

		/// <summary>
		/// Has the descriptions class fix up the data and build our hash maps
		/// </summary>
		public void FixupData()
		{
			double LastFrameTime = 0.0;

			// Fixes up any cross object pointer references
			Descriptions.FixupData(GroupIdToGroup,StatIdToStat);

			// Create an aggregate object for every stat description
			CreateAggregates();

			// For each frame, go through the stats data building aggregate data
			foreach (Frame frame in Frames)
			{
				FixupFrame(frame,ref LastFrameTime);
			}
		}

		/// <summary>
		/// Finds the stat associated with the given name
		/// </summary>
		/// <param name="Name">The name of the stat to search for</param>
		/// <returns>The stat object matching this name</returns>
		public Stat GetStatFromName(string Name)
		{
			// Search through the stats for one matching this name
			foreach (Stat stat in Descriptions.Stats)
			{
				// If it matches the name and is in our hash
				if (String.Compare(stat.Name,Name,true) == 0 &&
					StatIdToStat[stat.StatId] != null)
				{
					return stat;
				}
			}
			return null;
		}

		/// <summary>
		/// Uses the hash to find the stat object for this id
		/// </summary>
		/// <param name="StatId">The id of the stat to find the object for</param>
		/// <returns>The stat object for this id or null if not valid</returns>
		public Stat GetStat(int StatId)
		{
			return (Stat)StatIdToStat[StatId];
		}

		/// <summary>
		/// Uses the hash to find the group object for this id
		/// </summary>
		/// <param name="StatId">The id of the group to find the object for</param>
		/// <returns>The group object for this id or null if not valid</returns>
		public Group GetGroup(int GroupId)
		{
			return (Group)GroupIdToGroup[GroupId];
		}

		/// <summary>
		/// Looks up the object that holds the aggregate data for this stat
		/// </summary>
		/// <param name="StatId"></param>
		/// <returns></returns>
		public AggregateStatData GetAggregateData(int StatId)
		{
			return (AggregateStatData)StatIdToAggData[StatId];
		}

		/// <summary>
		/// Updates the SecondPerCycle value from packet data
		/// </summary>
		/// <param name="Data">The data to read the value from</param>
		public void UpdateConversionFactor(Byte[] Data)
		{
			int Offset = 2;
			SecondsPerCycle = ByteStreamConverter.ToDouble(Data,ref Offset);
		}

		/// <summary>
		/// Adds the new frame to the "needing fixup" list
		/// </summary>
		/// <param name="NewFrame"></param>
		public void AppendFrame(Frame NewFrame)
		{
			FrameList.Add(NewFrame);
		}

		/// <summary>
		/// Adds the new stat to the most recently added frame
		/// </summary>
		/// <param name="NewStat">The new stat to add</param>
		public void AppendStat(Stat NewStat)
		{
			if (FrameList.Count > 0)
			{
				// Find the frame that we are adding to
				Frame frame = (Frame)FrameList[FrameList.Count - 1];
				frame.AppendStat(NewStat);
			}
			else
			{
				Console.WriteLine("ERROR: Adding stat before a frame");
			}
		}

		/// <summary>
		/// Adds the new stat description to our descriptions list
		/// </summary>
		/// <param name="NewStat">The new stat description to add</param>
		public void AppendStatDescription(Stat NewStat)
		{
			Descriptions.AppendStat(NewStat);
		}

		/// <summary>
		/// Adds the new group description to our descriptions list
		/// </summary>
		/// <param name="NewGroup">The new group description to add</param>
		public void AppendGroupDescription(Group NewGroup)
		{
			Descriptions.AppendGroup(NewGroup);
		}

		/// <summary>
		/// Fixes up the stats data for a given frame
		/// </summary>
		/// <param name="frame">The frame to fix up</param>
		/// <param name="LastFrameTime">The elapsed frame time</param>
		private void FixupRecentFrame(Frame frame,ref double LastFrameTime)
		{
			// Only update if they need to be
			if (frame.Stats.Length < frame.StatList.Count)
			{
				// Fixup each stat and build aggregate data
				for (int Index = frame.Stats.Length; Index < frame.StatList.Count; Index++)
				{
					Stat stat = (Stat)frame.StatList[Index];

					// Move from platform specific cycles to milliseconds
					stat.FixupData(SecondsPerCycle,StatIdToStat);

					// Get the aggregate object that we are updating
					AggregateStatData AggData = (AggregateStatData)StatIdToAggData[stat.StatId];

					// Update the aggregate data with this stat instance
					AggData += stat;
				}
			}

			// Get the id of the frame time stat
			int FrameTimeId = GetStatFromName("FrameTime").StatId;

			// Update the recently added items in the frame
			LastFrameTime = frame.FixupRecentData(LastFrameTime,FrameTimeId);

			// Repair data problems
			foreach( Stat CurStat in frame.Stats )
			{
				RecursivelyFixBadInclusiveTimes( CurStat, frame.FrameNumber, ref RepairWarningMessages );
			}
		}

		/// <summary>
		/// Fixes up the stats data for any frames that were recently added
		/// </summary>
		private void FixupRecentFrames()
		{
			double LastFrameTime = 0.0;

			// Update the last frame time with the last frame's elapsed time
			if (Frames.Length > 0)
			{
				LastFrameTime = Frames[Frames.Length - 1].ElapsedTime;
			}

			// Only update the items that aren't already in the Frames array
			for (int Index = Frames.Length; Index < FrameList.Count; Index++)
			{
				Frame frame = (Frame)FrameList[Index];

				// Fixup the frame and all of its stats
				FixupRecentFrame(frame,ref LastFrameTime);
			}
		}

		/// <summary>
		/// Used to fix up data on items that were added during the last set
		/// of packet processing
		/// </summary>
		public void FixupRecentItems()
		{
			// Update the group/stat descriptions
			Descriptions.FixupRecentItems(GroupIdToGroup,StatIdToStat);

			// Update the aggregate objects as needed
			CreateAggregates();

			// No work to do if they match or the frame list is empty
			if (Frames.Length < FrameList.Count)
			{
				// Now update any frame changes
				FixupRecentFrames();

				// Now recreate the Frames array from the FrameList
				Frames = (Frame[])FrameList.ToArray(typeof(Frame));
			}
		}
	}
}
