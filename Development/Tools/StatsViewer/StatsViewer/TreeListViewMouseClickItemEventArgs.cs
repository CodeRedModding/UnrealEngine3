/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace StatsViewer
{
	/**
	 * This class contains information about an item that was clicked by the mouse.
	 */
	public class FTreeListViewMouseClickItemEventArgs : MouseEventArgs
	{
		FTreeListViewItem ItemClicked;

		/**
		 * Gets the item that was clicked.
		 */
		public FTreeListViewItem Item
		{
			get { return ItemClicked; }
		}

		/**
		 * Constructor.
		 * 
		 * @param	Item	The item that was clicked.
		 * @param	Button	The button that was pressed.
		 * @param	Clicks	The amount of clicks.
		 * @param	X		The X coordinate.
		 * @param	Y		The Y coordinate.
		 * @param	Delta	The number of ticks the mouse wheel was scrolled.
		 */
		public FTreeListViewMouseClickItemEventArgs(FTreeListViewItem Item, MouseButtons Button, int Clicks, int X, int Y, int Delta) 
			: base(Button, Clicks, X, Y, Delta)
		{
			this.ItemClicked = Item;
		}
	}
}
