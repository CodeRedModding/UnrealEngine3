/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


using System.Drawing;
namespace UnrealFrontend
{
	public class UISettings
	{
		public UISettings()
		{
			WindowLeft = 10;
			WindowTop = 10;
			WindowWidth = 970;
			WindowHeight = 800;

			ProfileListWidth = 200;
			ProfilesSectionHeight = 560;
		}

		public void Validate()
		{
			Point WindowPosition = new Point((int)WindowLeft, (int)WindowTop);
			System.Drawing.Rectangle ScreenArea = System.Windows.Forms.Screen.GetWorkingArea(WindowPosition);

			if ( WindowLeft < ScreenArea.Left || WindowLeft >= ScreenArea.Right )
			{
				WindowLeft = ScreenArea.Left;
			}
			
			if (WindowTop < ScreenArea.Top || WindowTop >= ScreenArea.Bottom)
			{
				WindowTop = ScreenArea.Top;
			}
			

		}

		public double WindowLeft { get; set; }
		public double WindowTop { get; set; }
		public double WindowWidth { get; set; }
		public double WindowHeight { get; set; }

		public double ProfileListWidth { get; set; }
		public double ProfilesSectionHeight { get; set; }
	}
}
