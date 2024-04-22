/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;

namespace Stats
{
	/// <summary>
	/// Holds the aggregate information for a specific stat across all
	/// frames that have been captured
	/// </summary>
	public class AggregateStatData
	{
		/// <summary>
		/// This is the number of times this stat was collected
		/// </summary>
		private int Count;
		/// <summary>
		/// Holds the total number of times this stats was tracked
		/// </summary>
		private int TotalCalls;
		/// <summary>
		/// This is the sum of all stat instances for this stat
		/// </summary>
		private double Total;
		/// <summary>
		/// This is the average for all stat instances
		/// </summary>
		private double CalcedAverage;
		/// <summary>
		/// This is the average per call for this stat
		/// </summary>
		private double CalcedPerCallAverage;
		/// <summary>
		/// Holds the min value of all instances for this stat
		/// </summary>
		private double MinValue = double.MaxValue;
		/// <summary>
		/// Holds the max value of all instances for this stat
		/// </summary>
		private double MaxValue = double.MinValue;

		/// <summary>
		/// Default ctor
		/// </summary>
		public AggregateStatData()
		{
		}

		/// <summary>
		/// The calculated average of all instances of this stat
		/// </summary>
		public double Average
		{
			get
			{
				return CalcedAverage;
			}
		}

		/// <summary>
		/// The calculated average per call of all instances of this stat
		/// </summary>
		public double AveragePerCall
		{
			get
			{
				return CalcedPerCallAverage;
			}
		}

		/// <summary>
		/// Returns the min value for this set of aggregated data
		/// </summary>
		public double Min
		{
			get
			{
				return MinValue;
			}
		}

		/// <summary>
		/// Returns the max value for this set of aggregated data
		/// </summary>
		public double Max
		{
			get
			{
				return MaxValue;
			}
		}

		/// <summary>
		/// Returns the number of stats that make up this aggregate
		/// </summary>
		public int NumStats
		{
			get
			{
				return Count;
			}
		}

		/// <summary>
		/// Adds a stat to our aggregated data
		/// </summary>
		/// <param name="AggData">The object that is tracking overall data information</param>
		/// <param name="StatToAdd">The stat instance beind added</param>
		/// <returns>The aggregate data</returns>
		public static AggregateStatData operator +(AggregateStatData AggData,Stat StatToAdd)
		{
			double Value = 0.0;
			if (StatToAdd != null)
			{
				// Determine whether to we are reading a cycle counter or not
				if (StatToAdd.Type == Stat.StatType.STATTYPE_CycleCounter)
				{
					Value = Math.Max(0,StatToAdd.ValueInMS);
				}
				else
				{
					Value = StatToAdd.Value;
				}
				// Add to our total
				AggData.Total += Value;
				AggData.Count++;
				// Determine the new average
				AggData.CalcedAverage = AggData.Total / AggData.Count;
				// Now update the per call data
				AggData.TotalCalls += StatToAdd.CallsPerFrame;
				if (AggData.TotalCalls > 0)
				{
					AggData.CalcedPerCallAverage = AggData.Total / AggData.TotalCalls;
				}
				else
				{
					AggData.CalcedPerCallAverage = AggData.CalcedAverage;
				}
				// Check for a new min
				if (AggData.MinValue > Value)
				{
					AggData.MinValue = Value;
				}
				// Check for a new max
				if (AggData.MaxValue < Value)
				{
					AggData.MaxValue = Value;
				}
			}
			return AggData;
		}

		/// <summary>
		/// Removes a stat to our aggregated data, used for windowed averages
		/// </summary>
		/// <param name="AggData">The object that is tracking overall data information</param>
		/// <param name="StatToRemove">The stat instance being removed</param>
		/// <returns>The aggregate data</returns>
		public static AggregateStatData operator -(AggregateStatData AggData,Stat StatToRemove)
		{
			double Value = 0.0;
			// Determine whether to we are reading a cycle counter or not
			if (StatToRemove.Type == Stat.StatType.STATTYPE_CycleCounter)
			{
				Value = Math.Max(0,StatToRemove.ValueInMS);
			}
			else
			{
				Value = StatToRemove.Value;
			}
			// Subtract from our total
			AggData.Total -= Value;
			AggData.Count--;
			// Determine the new average
			AggData.CalcedAverage = AggData.Total / AggData.Count;
			// Now update the per call data
			AggData.TotalCalls -= StatToRemove.CallsPerFrame;
			if (AggData.TotalCalls > 0)
			{
				AggData.CalcedPerCallAverage = AggData.Total / AggData.TotalCalls;
			}
			else
			{
				AggData.CalcedPerCallAverage = AggData.CalcedAverage;
			}
			//@todo -- recalc the min/max
			return AggData;
		}
	}
}
