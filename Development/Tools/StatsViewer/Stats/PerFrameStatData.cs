/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;

namespace Stats
{
	/// <summary>
	/// Holds the per frame total for a given stat
	/// </summary>
	public class PerFrameStatData
	{
		/// <summary>
		/// Holds the tolal value for the stat across a single frame
		/// </summary>
		private double TotalValue;

		/// <summary>
		/// Holds the total number of calls for the stat for a single frame
		/// </summary>
		private int TotalCallsValue;

		/// <summary>
		/// Holds the total time used for a given stat across a single frame
		/// This will be zero for non-cycle counter stats
		/// </summary>
		private double TotalTimeInMS;

		/// <summary>
		/// The list of stats that make up this data
		/// </summary>
		private ArrayList StatsData = new ArrayList();

		/// <summary>
		/// Default ctor
		/// </summary>
		public PerFrameStatData()
		{
		}

		/// <summary>
		/// Accessor to the total for this stat
		/// </summary>
		public double Total
		{
			get
			{
				return TotalValue;
			}
		}

		/// <summary>
		/// Accessor to the total calls for this stat
		/// </summary>
		public int TotalCalls
		{
			get
			{
				return TotalCallsValue;
			}
		}

		/// <summary>
		/// Accessor to the time in millis var
		/// </summary>
		public double TotalTime
		{
			get
			{
				return TotalTimeInMS;
			}
		}

		/// <summary>
		/// Adds a stat instance to our the single frame's data
		/// </summary>
		/// <param name="AggData">The object that is tracking overall data information</param>
		/// <param name="StatToAdd">The stat instance beind added</param>
		/// <returns>The per frame data</returns>
		public static PerFrameStatData operator +(PerFrameStatData PerFrameData,Stat StatToAdd)
		{
			if (StatToAdd != null)
			{
				// Update the total for all stats
				PerFrameData.TotalValue += StatToAdd.Value;

				// Update total calls
				PerFrameData.TotalCallsValue += StatToAdd.CallsPerFrame;

				// Only update the time value for cycle counters
				if (StatToAdd.Type == Stat.StatType.STATTYPE_CycleCounter)
				{
					PerFrameData.TotalTimeInMS += Math.Max(0, StatToAdd.ValueInMS );
				}
				PerFrameData.StatsData.Add(StatToAdd);
			}
			return PerFrameData;
		}

		/// <summary>
		/// Accessor to the stats that make up this frame's data
		/// </summary>
		public ArrayList Stats
		{
			get
			{
				return StatsData;
			}
		}
	}
}
