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
	/// Data container for serializing stats data to/from our XML files. Each
	/// frame object contains zero or more stats entries.
	/// </summary>
	public class Frame
	{
		/// <summary>
		/// The frame number this frame object represents
		/// </summary>
		[XmlAttribute("N")]
		public int FrameNumber;
		/// <summary>
		/// The set of stats written out for this frame
		/// </summary>
		[XmlArray]
		public Stat[] Stats = new Stat[0];
		/// <summary>
		/// Represents the elapsed time from the first frame until now
		/// (running total across all frames)
		/// </summary>
		private double TimeInMS;
		/// <summary>
		/// Holds the list of recently added items for update tracking
		/// </summary>
		[XmlIgnoreAttribute]
		public ArrayList StatList = new ArrayList();
		/// <summary>
		/// Holds the list of per frame stats (the aggregate of all stat instances
		/// for a given id)
		/// </summary>
		private SortedList PerFrameStats = new SortedList();
		/// <summary>
		/// Used to find stats quickly using the instance id
		/// </summary>
		private SortedList CycleCounterInstanceHash = new SortedList();

		/// <summary>
		/// View location vector for this frame
		/// </summary>
		public float ViewLocationX = 0.0f;
		public float ViewLocationY = 0.0f;
		public float ViewLocationZ = 0.0f;

		/// <summary>
		/// View rotation (rotator) for this frame
		/// </summary>
		public int ViewRotationYaw = 0;
		public int ViewRotationPitch = 0;
		public int ViewRotationRoll = 0;

		/// <summary>
		/// XML serialization requires a default ctor
		/// </summary>
		public Frame()
		{
		}

		/// <summary>
		/// Creates a frame object from a byte stream
		/// </summary>
		/// <param name="Data">The byte stream to build the group from</param>
		public Frame(Byte[] Data)
		{
			int CurrentOffset = 2;
			// Packet looks like:
			// NF<FrameNum>
			FrameNumber = ByteStreamConverter.ToInt(Data,ref CurrentOffset);
		}

		/// <summary>
		/// Updates the per frame data for a given stat
		/// </summary>
		/// <param name="stat">The stat that needs to be appended</param>
		private void UpdatePerFrameData(Stat stat)
		{
			// See if we have an instance for this stat and add one if
			// we don't have one present
			PerFrameStatData PerFrameData = (PerFrameStatData)PerFrameStats[stat.StatId];
			if (PerFrameData == null)
			{
				PerFrameData = new PerFrameStatData();
				PerFrameStats.Add(stat.StatId,PerFrameData);
			}
			// Add to the per frame total for this stat
			PerFrameData += stat;
		}

		/// <summary>
		/// Puts the stats data into a fast look up structure
		/// </summary>
		/// <param name="PreviousFrameTime">The last frame's time</param>
		/// <param name="FrameTimeId">The id of the stat to add to our frame time</param>
		public double FixupData(double PreviousFrameTime,int FrameTimeId)
		{
			Stat FrameTimeStat = null;
			// Add each stat's unique instance data
			foreach (Stat stat in Stats)
			{
				// Grab the frame time counter if this is it
				if (FrameTimeStat == null && stat.StatId == FrameTimeId)
				{
					// Found the FrameTime stat
					FrameTimeStat = stat;
				}
				// Don't add non-cycle counters or ones that were never used
				if (stat.InstanceId > 0 &&
					CycleCounterInstanceHash.Contains(stat.InstanceId) == false)
				{
					CycleCounterInstanceHash.Add(stat.InstanceId,stat);
				}
			}
			// Update the per frame data and the hierarchy
			foreach (Stat stat in Stats)
			{
				// Fix up the stat's parentage
				stat.FixupParent(CycleCounterInstanceHash);
				UpdatePerFrameData(stat);
			}
			// Update elapsed time based upon the previous frame
			TimeInMS = PreviousFrameTime;
			// Add our time to the last the time so the next frame is correct
			return FrameTimeStat != null ? TimeInMS + FrameTimeStat.ValueInMS : 0.0;
		}

		/// <summary>
		/// Finds the per frame stat data (total) by id within the scope of this frame
		/// </summary>
		/// <param name="StatId">The id of the stat to fetch</param>
		/// <returns>The per frame data that corresponds to this id</returns>
		public PerFrameStatData GetPerFrameStat(int StatId)
		{
			return (PerFrameStatData)PerFrameStats[StatId];
		}
		/// <summary>
		/// Read only property for getting this frame's elapsed time
		/// </summary>
		public double ElapsedTime
		{
			get
			{
				return TimeInMS;
			}
		}

		/// <summary>
		/// Puts the stats data into a fast look up structure. Updates the
		/// elapsed frame time
		/// </summary>
		/// <param name="PreviousFrameTime">The last frame's time</param>
		/// <param name="FrameTimeId">The id of the stat to add to our frame time</param>
		public double FixupRecentData(double PreviousFrameTime,int FrameTimeId)
		{
			// Only update if they need to be
			if (Stats.Length < StatList.Count)
			{
				// Add each stat to the instance hash
				for (int Index = Stats.Length; Index < StatList.Count; Index++)
				{
					Stat stat = (Stat)StatList[Index];
					// Don't add non-cycle counters or ones that were never used
					if (stat.InstanceId > 0 &&
						CycleCounterInstanceHash.Contains(stat.InstanceId) == false)
					{
						CycleCounterInstanceHash.Add(stat.InstanceId,stat);
					}
				}
				for (int Index = Stats.Length; Index < StatList.Count; Index++)
				{
					Stat stat = (Stat)StatList[Index];
					// Fix up the stat's parentage
					stat.FixupParent(CycleCounterInstanceHash);
					// Update per frame stats
					UpdatePerFrameData(stat);
				}
				// Rebuild the Stats array from the StatList
				Stats = (Stat[])StatList.ToArray(typeof(Stat));
			}
			// Update elapsed time based upon the previous frame
			TimeInMS = PreviousFrameTime;
			// Find the FrameTime stat
			Stat FrameTimeStat = GetFrameTimeStat();
			// Add our time to the last the time so the next frame is correct
			return FrameTimeStat != null ? TimeInMS + FrameTimeStat.ValueInMS : 0.0;
		}

		/// <summary>
		/// Adds a new stat to the recently added list
		/// </summary>
		/// <param name="NewStat">The new stat to add</param>
		public void AppendStat(Stat NewStat)
		{
			StatList.Add(NewStat);
		}

		/// <summary>
		/// Finds the frame time stat for this frame
		/// </summary>
		/// <returns>The frame time stat object</returns>
		public Stat GetFrameTimeStat()
		{
            return GetStatByName("FrameTime");
		}

        /// <summary>
        /// Finds a specific named stat for this frame
        /// </summary>
        /// <param name="Name">The name to search for</param>
        /// <returns>The stat in this frame if found (null otherwise)</returns>
        public Stat GetStatByName(string Name)
        {
            foreach (Stat stat in Stats)
            {
                if (String.Compare(stat.Name, Name, true) == 0)
                {
                    return stat;
                }
            }
            return null;
        }

		/// <summary>
		/// Finds a stat by instance id within the scope of this file
		/// </summary>
		/// <param name="StatInstanceId">The instance id of the stat to fetch</param>
		/// <returns>The stat instance that corresponds to this id</returns>
		public Stat GetStatInstance(int StatInstanceId)
		{
			return (Stat)CycleCounterInstanceHash[StatInstanceId];
		}
	}
}
