/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Drawing;
using System.Reflection;

namespace StatsViewer
{
	/// <summary>
	/// This class holds all of the color & pens that are used to draw the
	/// graphs
	/// </summary>
	public class GraphColors
	{
		/// <summary>
		/// The thickness of the pen to use for nromal lines
		/// </summary>
		private const float NormalThickness = 1.0F;
		/// <summary>
		/// The thickness of the pen to use for bold lines
		/// </summary>
		private const float BoldThickness = 3.0F;
		/// <summary>
		/// Holds the list of colored pens for drawing the graph data
		/// </summary>
		private ArrayList NormalPens = new ArrayList();
		/// <summary>
		/// Holds the list of colored pens for drawing the graph data, when an
		/// item in the view is selected (bold line)
		/// </summary>
		private ArrayList BoldPens = new ArrayList();

		/// <summary>
		/// Randomizes the array of normal pens
		/// </summary>
		public void ShuffleNormalPens()
		{
			Random rand = new Random();
			// Move through the array swapping items with random other items
			for (int Index = 0; Index < NormalPens.Count; Index++)
			{
				// Get a random index to swap with
				int RandIndex = rand.Next(0,NormalPens.Count);
				// Swap the objects contained at the random index and the current one
				Object temp = NormalPens[RandIndex];
				NormalPens[RandIndex] = NormalPens[Index];
				NormalPens[Index] = temp;
			}
		}

		/// <summary>
		/// Constructs the sets of colors, pens, and bold pens
		/// </summary>
		public GraphColors()
		{
			// Create a pen for each color that isn't a system color
			foreach (KnownColor kcolor in Enum.GetValues(typeof(KnownColor)))
			{
				// Skip whites and transparent
				if (kcolor != KnownColor.Transparent &&
					kcolor != KnownColor.White &&
					kcolor != KnownColor.WhiteSmoke)
				{
					Color color = Color.FromKnownColor(kcolor);
					// Check to see if the color is a system color or not
					// Skip super bright colors
					if (color.IsSystemColor == false && color.GetBrightness() < 0.8)
					{
						// Create the pens
						NormalPens.Add(new Pen(color,NormalThickness));
					}
				}
			}
			// Shuffle the pens so that we get some variety
			ShuffleNormalPens();
			// Create a pen for each color that isn't a system color
			foreach (Pen pen in NormalPens)
			{
				BoldPens.Add(new Pen(pen.Color,BoldThickness));
			}
		}

		/// <summary>
		/// Returns the pen for the specified id
		/// </summary>
		/// <param name="PenId">The pen the caller wants</param>
		/// <param name="bUseBold">Whether to return the bold version or not</param>
		/// <returns>The pen that matches the id</returns>
		public Pen GetPen(int PenId,bool bUseBold)
		{
			Pen pen;
			if (bUseBold == false)
			{
				pen = (Pen)NormalPens[PenId];
			}
			else
			{
				pen = (Pen)BoldPens[PenId];
			}
			return pen;
		}

		/// <summary>
		/// Returns the pen for the specified id
		/// </summary>
		/// <param name="PenId">The pen the caller wants</param>
		/// <returns>The pen that matches the id</returns>
		public Pen GetBoldPen(int PenId)
		{
			return (Pen)BoldPens[PenId];
		}

		/// <summary>
		/// Returns the color for the specified id
		/// </summary>
		/// <param name="PenId">The color the caller wants</param>
		/// <returns>The color that matches the id</returns>
		public Color GetColor(int ColorId)
		{
			Pen pen = (Pen)NormalPens[ColorId];
			return pen.Color;
		}

		/// <summary>
		/// Determines the next color id to use
		/// </summary>
		/// <param name="CurrentId">The last id used</param>
		/// <returns>The last id plus one with wrapping if larger than our pen count</returns>
		public int GetNextColorId(int CurrentId)
		{
			return (CurrentId + 1) % NormalPens.Count;
		}

		/// <summary>
		/// Sets the color at the specified color id by creating the normal
		/// and bold pens for that id
		/// </summary>
		/// <param name="ColorId">The id to replace the color for</param>
		/// <param name="NewColor">The color to replace the old color with</param>
		public void SetColorForId(int ColorId,Color NewColor)
		{
			// Dispose the old pens so they don't leak resources
			Pen DisposeMe = (Pen)NormalPens[ColorId];
			DisposeMe.Dispose();
			DisposeMe = (Pen)BoldPens[ColorId];
			DisposeMe.Dispose();
			// Now create the replacement pens
			NormalPens[ColorId] = new Pen(NewColor,NormalThickness);
			BoldPens[ColorId] = new Pen(NewColor,BoldThickness);
		}
	}
}
