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
	 * This class represents a column in an FTreeListView.
	 */
	public class FTreeListViewColumnHeader
	{
		const int MIN_HEADER_WIDTH = 3;

		public object Tag;

		HorizontalAlignment TextAlignment;
		string TextString;
		FTreeListView Owner;
		int DisplayIndexInListView = -1;
		int ColumnHeaderWidth = 80;

		/**
		 * Gets the index within the parent list view or item.
		 */
		public int DisplayIndex
		{
			get { return DisplayIndexInListView; }
			internal set { DisplayIndexInListView = value; }
		}

		/**
		 * Gets/Sets the alignment of the text within the column header.
		 */
		public HorizontalAlignment TextAlign
		{
			get { return TextAlignment; }
			set { TextAlignment = value; OnModified(); }
		}

		/**
		 * Gets/Sets the text that is displayed on the column header.
		 */
		public string Text
		{
			get { return TextString; }
			set { TextString = value; OnModified(); }
		}

		/**
		 * Gets/Sets the width of the column header.
		 */
		public int Width
		{
			get { return ColumnHeaderWidth; }
			set
			{
				if(value < MIN_HEADER_WIDTH)
				{
					ColumnHeaderWidth = MIN_HEADER_WIDTH;
				}
				else
				{
					ColumnHeaderWidth = value;
				}
				
				OnModified();
			}
		}

		/**
		 * Gets the FTreeListView that owns the column.
		 */
		public FTreeListView ListView
		{
			get { return Owner; }
			internal set { Owner = value; }
		}

		/**
		 * Constructor.
		 */
		public FTreeListViewColumnHeader()
		{

		}

		/**
		 * Constructor.
		 * 
		 * @param	Text	The text to display on the column header.
		 */
		public FTreeListViewColumnHeader(string Text)
		{
			this.TextString = Text;
		}

		/**
		 * Event handler for when special keys are pressed.
		 * 
		 * @param	Text	The text to display on the column header.
		 * @param	Width	The width of the column header.
		 */
		public FTreeListViewColumnHeader(string Text, int Width)
		{
			this.TextString = Text;
			this.Width = Width;
		}

		private void OnModified()
		{
			if(Owner != null)
			{
				Owner.UpdateClientArea();
			}
		}
	}
}
