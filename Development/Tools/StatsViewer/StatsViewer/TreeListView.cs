/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using System.Windows.Forms.VisualStyles;

namespace StatsViewer
{
	/**
	 * This class implements a ListView with items that can contain nodes like a TreeView.
	 */
	public class FTreeListView : Control
	{
		#region Internal Classes
		/**
		 * This class contains a collection of column headers.
		 */
		public class FColumnHeaderCollection : IList<FTreeListViewColumnHeader>
		{
			FTreeListView Owner;
			List<FTreeListViewColumnHeader> Columns = new List<FTreeListViewColumnHeader>();

			/**
			 * Constructor.
			 * 
			 * @param	Owner	The owner of the collection.
			 */
			public FColumnHeaderCollection(FTreeListView Owner)
			{
				if(Owner == null)
				{
					throw new ArgumentNullException("Owner");
				}

				this.Owner = Owner;
			}

			#region IList<FTreeListViewColumnHeader> Members

			/**
			 * Returns the index of an item or -1 if it doesn't exist.
			 * 
			 * @param	Item	The item to search for.
			 */
			public int IndexOf(FTreeListViewColumnHeader Item)
			{
				return Columns.IndexOf(Item);
			}

			/**
			 * Inserts a new item at the specified index.
			 * 
			 * @param	Index	The index to insert the item at.
			 * @param	Item	The item to insert.
			 */
			public void Insert(int Index, FTreeListViewColumnHeader Item)
			{
				Columns.Insert(Index, Item);
				Item.ListView = Owner;
				Item.DisplayIndex = Index;

				for(int i = Index + 1; i < Columns.Count; ++i)
				{
					Columns[i].DisplayIndex += 1;
				}

				Owner.UpdateClientArea();
			}

			/**
			 * Removes the item at the specified index.
			 * 
			 * @param	Index	The index to remove the item at.
			 */
			public void RemoveAt(int Index)
			{
				FTreeListViewColumnHeader Header = Columns[Index];
				Columns.RemoveAt(Index);
				Header.DisplayIndex = -1;
				Header.ListView = null;

				for(int i = Index; i < Columns.Count; ++i)
				{
					Columns[i].DisplayIndex -= 1;
				}
				
				Owner.UpdateClientArea();
			}

			/**
			 * Indexer overloading the [] operator.
			 * 
			 * @param	Index	The index of the item to Get/Set.
			 */
			public FTreeListViewColumnHeader this[int Index]
			{
				get
				{
					return Columns[Index];
				}
				set
				{
					if(Columns[Index] != value)
					{
						Columns[Index].DisplayIndex = -1;
						Columns[Index].ListView = null;
						Columns[Index] = value;
						value.ListView = Owner;
						value.DisplayIndex = Index;
						Owner.UpdateClientArea();
					}
				}
			}

			#endregion

			#region ICollection<FTreeListViewColumnHeader> Members
			/**
			 * Adds an item to the end of the collection.
			 * 
			 * @param	Item	The item to be added.
			 */
			public void Add(FTreeListViewColumnHeader Item)
			{
				Columns.Add(Item);
				Item.ListView = this.Owner;
				Item.DisplayIndex = Columns.Count - 1;

				Owner.UpdateClientArea();
			}

			/**
			 * Removes all items from the collection.
			 */
			public void Clear()
			{
				foreach(FTreeListViewColumnHeader Header in Columns)
				{
					Header.DisplayIndex = -1;
					Header.ListView = null;
				}
				Columns.Clear();
				Owner.UpdateClientArea();
			}

			/**
			 * Returns true if the collection contains the specified item.
			 * 
			 * @param	Item	The item to check for containment.
			 */
			public bool Contains(FTreeListViewColumnHeader Item)
			{
				return Columns.Contains(Item);
			}

			/**
			 * Copies the collection to the specified array.
			 * 
			 * @param	Ar			The array to copy the collection into.
			 * @param	ArrayIndex	The index into the array to start copying at.
			 */
			public void CopyTo(FTreeListViewColumnHeader[] Ar, int ArrayIndex)
			{
				Columns.CopyTo(Ar, ArrayIndex);
			}

			/**
			 * Gets the number of items in the collection.
			 */
			public int Count
			{
				get { return Columns.Count; }
			}

			/**
			 * Returns false.
			 */
			public bool IsReadOnly
			{
				get { return false; }
			}

			/**
			 * Removes the specified item from the collection if it exists.
			 * 
			 * @param	Item	The item to remove.
			 */
			public bool Remove(FTreeListViewColumnHeader Item)
			{
				int Index = IndexOf(Item);

				if(Index != -1)
				{
					RemoveAt(Index);
				}

				return Index != -1;
			}

			#endregion

			#region IEnumerable<FTreeListViewColumnHeader> Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			public IEnumerator<FTreeListViewColumnHeader> GetEnumerator()
			{
				return Columns.GetEnumerator();
			}

			#endregion

			#region IEnumerable Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
			{
				return Columns.GetEnumerator();
			}

			#endregion
		}

		/**
		 * This class contains a collection of FTreeListViewItem's.
		 */
		public class FTreeListViewItemCollection : IList<FTreeListViewItem>
		{
			FTreeListView Owner;
			List<FTreeListViewItem> Items = new List<FTreeListViewItem>();

			/**
			 * Constructor.
			 * 
			 * @param	Owner	The owner of the collection.
			 */
			public FTreeListViewItemCollection(FTreeListView Owner)
			{
				this.Owner = Owner;
			}

			#region IList<FTreeListViewItem> Members
			/**
			 * Returns the index of an item or -1 if it doesn't exist.
			 * 
			 * @param	Item	The item to search for.
			 */
			public int IndexOf(FTreeListViewItem Item)
			{
				return Items.IndexOf(Item);
			}

			/**
			 * Inserts a new item at the specified index.
			 * 
			 * @param	Index	The index to insert the item at.
			 * @param	Item	The item to insert.
			 */
			public void Insert(int Index, FTreeListViewItem Item)
			{
				Items.Insert(Index, Item);
				Item.ListView = Owner;
				Item.Index = Index;

				for(int i = Index + 1; i < Items.Count; ++i)
				{
					Items[i].Index += 1;
				}
				
				Owner.UpdateClientArea();
			}

			/**
			 * Removes the item at the specified index.
			 * 
			 * @param	Index	The index to remove the item at.
			 */
			public void RemoveAt(int Index)
			{
				FTreeListViewItem Item = Items[Index];
				Items.RemoveAt(Index);
				Item.ListView = null;
				Item.Index = -1;

				for(int i = Index; i < Items.Count; ++i)
				{
					Items[i].Index -= 1;
				}

				Owner.UpdateClientArea();
			}

			/**
			 * Indexer overloading the [] operator.
			 * 
			 * @param	Index	The index of the item to Get/Set.
			 */
			public FTreeListViewItem this[int Index]
			{
				get
				{
					return Items[Index];
				}
				set
				{
					FTreeListViewItem Item = Items[Index];
					Item.ListView = null;
					Item.Index = -1;
					Items[Index] = value;
					value.ListView = Owner;
					value.Index = Index;
					Owner.UpdateClientArea();
				}
			}

			#endregion

			#region ICollection<FTreeListViewItem> Members
			/**
			 * Adds an item to the end of the collection.
			 * 
			 * @param	Item	The item to be added.
			 */
			public void Add(FTreeListViewItem Item)
			{
				Item.ListView = Owner;
				Items.Add(Item);
				Item.Index = Items.Count - 1;
				Owner.UpdateClientArea();
			}

			/**
			 * Removes all items from the collection.
			 */
			public void Clear()
			{
				foreach(FTreeListViewItem Item in Items)
				{
					Item.ListView = null;
					Item.Index = -1;
				}

				Items.Clear();
				Owner.UpdateClientArea();
			}

			/**
			 * Returns true if the collection contains the specified item.
			 * 
			 * @param	Item	The item to check for containment.
			 */
			public bool Contains(FTreeListViewItem Item)
			{
				return Items.Contains(Item);
			}

			/**
			 * Copies the collection to the specified array.
			 * 
			 * @param	Ar			The array to copy the collection into.
			 * @param	ArrayIndex	The index into the array to start copying at.
			 */
			public void CopyTo(FTreeListViewItem[] Ar, int ArrayIndex)
			{
				Items.CopyTo(Ar, ArrayIndex);
			}

			/**
			 * Gets the number of items in the collection.
			 */
			public int Count
			{
				get { return Items.Count; }
			}

			/**
			 * Returns false.
			 */
			public bool IsReadOnly
			{
				get { return false; }
			}

			/**
			 * Removes the specified item from the collection if it exists.
			 * 
			 * @param	Item	The item to remove.
			 */
			public bool Remove(FTreeListViewItem Item)
			{
				int Index = IndexOf(Item);

				if(Index != -1)
				{
					RemoveAt(Index);
				}

				return Index != -1;
			}

			#endregion

			#region IEnumerable<FTreeListViewItem> Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			public IEnumerator<FTreeListViewItem> GetEnumerator()
			{
				return Items.GetEnumerator();
			}

			#endregion

			#region IEnumerable Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
			{
				return Items.GetEnumerator();
			}

			#endregion
		}

		/**
		 * The state of a column header.
		 */
		public enum EHeaderColumnBackgroundState
		{
			Normal = 0,
			Hot,
			Pressed
		}

		/**
		 * Interface for rendering items that can have visual styles.
		 */
		interface ITreeListViewRenderer
		{
			/**
			 * Draws the [+]/[-] expansion glyph's.
			 * 
			 * @param	Gfx		The GDI+ device.
			 * @param	bClosed	True if the glyph is closed ([+]).
			 * @param	Rect	The rectangle to draw the glyph in.
			 */
			void DrawExpansionGlyph(Graphics Gfx, bool bClosed, Rectangle Rect);

			/**
			 * Draws the background of a column header.
			 * 
			 * @param	Gfx		The GDI+ device.
			 * @param	State	The state of the header.
			 * @param	Rect	The rectangle to draw the glyph in.
			 */
			void DrawHeaderColumnBackground(Graphics Gfx, EHeaderColumnBackgroundState State, Rectangle Rect);
		}

		/**
		 * Visual style renderer for win9x and win2k2/3
		 */
		public class FTreeListViewRendererWin95 : ITreeListViewRenderer
		{

			#region ITreeListViewRenderer Members
			/**
			 * Draws the [+]/[-] expansion glyph's.
			 * 
			 * @param	Gfx		The GDI+ device.
			 * @param	bClosed	True if the glyph is closed ([+]).
			 * @param	Rect	The rectangle to draw the glyph in.
			 */
			public void DrawExpansionGlyph(Graphics Gfx, bool bClosed, Rectangle Rect)
			{
				Rectangle GlyphRect = new Rectangle(Rect.X + (Rect.Width - 8) / 2, Rect.Y + (Rect.Height - 8) / 2 - 1, 8, 8);

				Gfx.DrawRectangle(SystemPens.ControlDark, GlyphRect);
				Gfx.DrawLine(SystemPens.ControlText, GlyphRect.X + 2, GlyphRect.Y + 4, GlyphRect.X + 6, GlyphRect.Y + 4);
				
				if(bClosed)
				{
					Gfx.DrawLine(SystemPens.ControlText, GlyphRect.X + 4, GlyphRect.Y + 2, GlyphRect.X + 4, GlyphRect.Y + 6);
				}
			}

			/**
			 * Draws the background of a column header.
			 * 
			 * @param	Gfx		The GDI+ device.
			 * @param	State	The state of the header.
			 * @param	Rect	The rectangle to draw the glyph in.
			 */
			public void DrawHeaderColumnBackground(Graphics Gfx, EHeaderColumnBackgroundState State, Rectangle Rect)
			{
				Gfx.FillRectangle(SystemBrushes.Control, Rect);

				switch(State)
				{
					case EHeaderColumnBackgroundState.Hot:
					case EHeaderColumnBackgroundState.Normal:
						{
							ControlPaint.DrawBorder3D(Gfx, Rect, Border3DStyle.Raised);
							break;
						}
					case EHeaderColumnBackgroundState.Pressed:
						{
							ControlPaint.DrawBorder3D(Gfx, Rect, Border3DStyle.Flat);
							break;
						}
				}
			}

			#endregion
		}

		/**
		 * Visual style renderer for WinXP witn visual styles enabled and it should work on vista as well.
		 */
		public class FTreeListViewRendererXP : ITreeListViewRenderer
		{
			static readonly VisualStyleRenderer TreeNodeExpandedGlyphRenderer = new VisualStyleRenderer(VisualStyleElement.TreeView.Glyph.Opened);
			static readonly VisualStyleRenderer TreeNodeClosedGlyphRenderer = new VisualStyleRenderer(VisualStyleElement.TreeView.Glyph.Closed);
			static readonly VisualStyleRenderer HeaderMainRenderer = new VisualStyleRenderer(VisualStyleElement.Header.Item.Normal);
			static readonly VisualStyleRenderer HeaderHotRenderer = new VisualStyleRenderer(VisualStyleElement.Header.Item.Hot);
			static readonly VisualStyleRenderer HeaderPressedRenderer = new VisualStyleRenderer(VisualStyleElement.Header.Item.Pressed);

			#region ITreeListViewRenderer Members
			/**
			 * Draws the [+]/[-] expansion glyph's.
			 * 
			 * @param	Gfx		The GDI+ device.
			 * @param	bClosed	True if the glyph is closed ([+]).
			 * @param	Rect	The rectangle to draw the glyph in.
			 */
			public void DrawExpansionGlyph(Graphics Gfx, bool bClosed, Rectangle Rect)
			{
				if(bClosed)
				{
					TreeNodeClosedGlyphRenderer.DrawBackground(Gfx, Rect);
				}
				else
				{
					TreeNodeExpandedGlyphRenderer.DrawBackground(Gfx, Rect);
				}
			}

			/**
			 * Draws the background of a column header.
			 * 
			 * @param	Gfx		The GDI+ device.
			 * @param	State	The state of the header.
			 * @param	Rect	The rectangle to draw the glyph in.
			 */
			public void DrawHeaderColumnBackground(Graphics Gfx, EHeaderColumnBackgroundState State, Rectangle Rect)
			{
				switch(State)
				{
					case EHeaderColumnBackgroundState.Normal:
						{
							HeaderMainRenderer.DrawBackground(Gfx, Rect);
							break;
						}
					case EHeaderColumnBackgroundState.Hot:
						{
							HeaderHotRenderer.DrawBackground(Gfx, Rect);
							break;
						}
					case EHeaderColumnBackgroundState.Pressed:
						{
							HeaderPressedRenderer.DrawBackground(Gfx, Rect);
							break;
						}
				}
			}

			#endregion
		}
		#endregion

		static readonly int HEADER_HEIGHT;
		const int HEADER_PADLEFT = 5;
		const int ENTRY_PADRIGHT = 3;
		const int ENTRY_PADLEFT = 3;
		const int MOVEABLE_COL_WIDTH = 10; //NOTE: should be even numbers
		const int ROW_HEIGHT_PAD = 1;
		const int TREE_EXPANSION_GLYPH_WIDTH = 10;
		const int TREE_EXPANSION_GLYPH_PAD = 3;
		const int EXPANDED_LINE_X = 2;
		const int EXPANDED_LINE_PAD_RIGHT = 3;
		const int CHILD_INDENT_AMOUNT = 16;

		static readonly StringFormat HeaderStringFormat = new StringFormat(StringFormat.GenericTypographic);
		static readonly Pen SelectedItemBorderPen = new Pen(Color.Goldenrod);
		static readonly Pen ExpandedItemLinePen = new Pen(SystemColors.ControlDark);
		static readonly ITreeListViewRenderer Renderer;

		FColumnHeaderCollection ColumnCollection;
		FTreeListViewItemCollection ItemCollection;
		SolidBrush TextBrush = new SolidBrush(Color.Black);
		HScrollBar HorizontalScrollBar = new HScrollBar();
		VScrollBar VerticalScrollBar = new VScrollBar();
		FTreeListViewItem SelectedRow;
		SolidBrush RowBrush = new SolidBrush(Color.White);
		EventHandler<EventArgs> ItemSelectionChangedEvent;
		EventHandler<FTreeListViewMouseClickItemEventArgs> ItemMouseClickEvent;
		EventHandler<FTreeListViewMouseClickItemEventArgs> ItemMouseDoubleClickEvent;

		bool bUpdating = false;
		bool bMouseIsDown = false;
		int ResizableColumn = -1;
		int HotColumn = -1;
		int MouseDownColumn = -1;
		int LineHeight = 0;
		//int SelectedRowIndex = -1;
		Point LastMousePosition;

		/**
		 * This event is triggered when a new item has been selected.
		 */
		public event EventHandler<EventArgs> ItemSelectionChanged
		{
			add { ItemSelectionChangedEvent += value; }
			remove { ItemSelectionChangedEvent -= value; }
		}

		/**
		 * This event is triggered when an item has been clicked by the mouse.
		 */
		public event EventHandler<FTreeListViewMouseClickItemEventArgs> ItemMouseClick
		{
			add { ItemMouseClickEvent += value; }
			remove { ItemMouseClickEvent -= value; }
		}

		/**
		 * This event is triggered when an item has been double clicked by the mouse.
		 */
		public event EventHandler<FTreeListViewMouseClickItemEventArgs> ItemMouseDoubleClick
		{
			add { ItemMouseDoubleClickEvent += value; }
			remove { ItemMouseDoubleClickEvent -= value; }
		}

		/**
		 * Gets/Sets the foreground color.
		 */
		public override Color ForeColor
		{
			get
			{
				return base.ForeColor;
			}
			set
			{
				base.ForeColor = value;
				TextBrush.Dispose();
				TextBrush = new SolidBrush(value);
			}
		}

		/**
		 * Gets the collection of columns.
		 */
		public FColumnHeaderCollection Columns
		{
			get { return ColumnCollection; }
		}

		/**
		 * Gets the collection of items.
		 */
		public FTreeListViewItemCollection Items
		{
			get { return ItemCollection; }
		}


		/**
		 * Returns the row that's currently selected
		 */
		public FTreeListViewItem SelectedNode
		{
			get
			{
				return SelectedRow;
			}

			set
			{
				if( value != SelectedRow )
				{
					SelectedRow = value;
					Invalidate();
				}
			}
		}


		/**
		 * Gets/Sets the font.
		 */
		public override Font Font
		{
			get
			{
				return base.Font;
			}
			set
			{
				base.Font = value;
				this.LineHeight = this.FontHeight + ROW_HEIGHT_PAD;
			}
		}

		/**
		 * Static Constructor.
		 */
		static FTreeListView()
		{
			SelectedItemBorderPen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dot;
			HeaderStringFormat.FormatFlags |= StringFormatFlags.NoWrap;
			HeaderStringFormat.Trimming = StringTrimming.EllipsisCharacter;
			ExpandedItemLinePen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dot;

			if(VisualStyleInformation.IsSupportedByOS && VisualStyleInformation.IsEnabledByUser)
			{
				HEADER_HEIGHT = 22;
				Renderer = new FTreeListViewRendererXP();
			}
			else
			{
				HEADER_HEIGHT = 18;
				Renderer = new FTreeListViewRendererWin95();
			}
		}

		/**
		 * Constructor.
		 */
		public FTreeListView()
		{
			this.LineHeight = this.FontHeight + ROW_HEIGHT_PAD;

			ColumnCollection = new FColumnHeaderCollection(this);
			ItemCollection = new FTreeListViewItemCollection(this);

			this.Font = new Font("Microsoft Sans Serif", 8.25f);
			SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.Selectable | ControlStyles.ResizeRedraw |
				ControlStyles.StandardClick | ControlStyles.StandardDoubleClick | ControlStyles.UserMouse, true);

			this.BackColor = SystemColors.Window;
			this.Width = 128;
			this.Height = 128;

			HorizontalScrollBar.Dock = DockStyle.Bottom;
			HorizontalScrollBar.Visible = false;
			HorizontalScrollBar.SmallChange = 1;
			HorizontalScrollBar.ValueChanged += new EventHandler(HorizontalScrollBar_ValueChanged);

			VerticalScrollBar.Dock = DockStyle.Right;
			VerticalScrollBar.Visible = false;
			VerticalScrollBar.SmallChange = 1;
			VerticalScrollBar.LargeChange = 1;
			VerticalScrollBar.ValueChanged += new EventHandler(VerticalScrollBar_ValueChanged);

			this.Controls.Add(HorizontalScrollBar);
			this.Controls.Add(VerticalScrollBar);
		}

		/**
		 * Event handler for when the horizontal scroll bar has been moved.
		 */
		void HorizontalScrollBar_ValueChanged(object sender, EventArgs e)
		{
			if(HorizontalScrollBar.Visible && !bUpdating)
			{
				Invalidate();
			}
		}

		/**
		 * Event handler for when the vertical scroll bar has been moved.
		 */
		void VerticalScrollBar_ValueChanged(object sender, EventArgs e)
		{
			if(VerticalScrollBar.Visible && !bUpdating)
			{
				Invalidate();
			}
		}

		/**
		 * Event hander for the WM_PAINT message.
		 * 
		 * @param	e	The event arguments.
		 */
		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			e.Graphics.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAlias;

			DrawColumnHeaders(e.Graphics);

			if(this.Columns.Count > 0)
			{
				DrawRows(e.Graphics);
			}
		}

		/**
		 * Draws the visible rows.
		 * 
		 * @param	Gfx		The GDI+ graphics device.
		 */
		private void DrawRows(Graphics Gfx)
		{
			float XOff = HorizontalScrollBar.Visible ? -(float)HorizontalScrollBar.Value : 0f;
			float YOff = HEADER_HEIGHT;
			int StartIndex = VerticalScrollBar.Visible ? VerticalScrollBar.Value : 0;
			int Index = 0;
			int ClientHeight = GetClientHeight();

			const int ParentIndex = 0;
			bool bLastItemWasSibling = true;

			foreach(FTreeListViewItem Item in ItemCollection)
			{
				// If the last item was a sibling (on the same indent level as us), then we don't need to
				// draw a big vertical line, so we just pass 0 for the 'distance to parent'
				float ChildXOff = 0.0f;
				if( Index >= StartIndex )
				{
					int ParentDist = bLastItemWasSibling ? 0 : ( Index - ParentIndex );
					DrawVerticalConnectionLine( Gfx, Item, XOff, ChildXOff, YOff, ParentDist );
				}


				if( YOff < ClientHeight )
				{
					if( Index >= StartIndex )
					{
						DrawRow( Gfx, Item, XOff, ChildXOff, YOff, ( Item == SelectedRow ) );
					}
				}

				bLastItemWasSibling = true;

				if( Index >= StartIndex )
				{
					YOff += this.LineHeight;
				}

				++Index;

				if( Item.Expanded )
				{
					if( YOff < ClientHeight )
					{
						float SubChildXOff = CHILD_INDENT_AMOUNT;
						DrawNodeRows( Gfx, Item, StartIndex, ClientHeight, XOff, SubChildXOff, ref YOff, ref Index );
					}

					if( Item.Nodes.Count > 0 )
					{
						bLastItemWasSibling = false;
					}
				}
			}
		}

		/**
		 * Draws all of the visible rows within an items nodes.
		 * 
		 * @param	Gfx				The GDI+ graphics device.
		 * @param	Item			The parent item.
		 * @param	StartIndex		The index of the first visible row.
		 * @param	ClientHeight	The height of the drawing area.
		 * @param	XOff			The X offset of the row.
		 * @param	YOff			The Y offset of the row.
		 * @param	Index			The row index of the parent item.
		 */
		private void DrawNodeRows(Graphics Gfx, FTreeListViewItem Item, int StartIndex, int ClientHeight, float XOff, float ChildXOff, ref float YOff, ref int Index)
		{
			int ParentIndex = Index;
			bool bLastItemWasSibling = true;

			foreach(FTreeListViewItem Node in Item.Nodes)
			{
				// If the last item was a sibling (on the same indent level as us), then we don't need to
				// draw a big vertical line, so we just pass 0 for the 'distance to parent'
				if( Index >= StartIndex )
				{
					int ParentDist = bLastItemWasSibling ? 0 : ( Index - ParentIndex );
					DrawVerticalConnectionLine( Gfx, Node, XOff, ChildXOff, YOff, ParentDist );
				}

				if( YOff < ClientHeight )
				{
					if( Index >= StartIndex )
					{
						DrawRow( Gfx, Node, XOff, ChildXOff, YOff, ( Node == SelectedRow ) );
					}
				}

				bLastItemWasSibling = true;

				if( Index >= StartIndex )
				{
					YOff += this.LineHeight;
				}

				++Index;

				if(Node.Expanded)
				{
					if( YOff < ClientHeight )
					{
						float NewChildXOff = ChildXOff + CHILD_INDENT_AMOUNT;
						DrawNodeRows( Gfx, Node, StartIndex, ClientHeight, XOff, NewChildXOff, ref YOff, ref Index );
					}

					if( Node.Nodes.Count > 0 )
					{
						bLastItemWasSibling = false;
					}
				}
			}
		}


		/**
		 * Draws a row
		 * 
		 * @param	Gfx		The GDI+ graphics device.
		 * @param	Item	The row to draw.
		 * @param	XOff	The X offset of the row.
		 * @param	YOff	The Y offset of the row.
		 */
		private void DrawRow(Graphics Gfx, FTreeListViewItem Item, float XOff, float ChildXOff, float YOff, bool bSelected )
		{
			Rectangle BGRect = new Rectangle( ENTRY_PADLEFT + ( HorizontalScrollBar.Visible ? -HorizontalScrollBar.Value : 0 ), (int)YOff, GetTotalHeaderWidth() - ENTRY_PADLEFT, this.LineHeight );

			if( bSelected )
			{
				Gfx.FillRectangle( SystemBrushes.Highlight, BGRect );
			}

			RectangleF IndentedRect = new RectangleF( XOff + ChildXOff, YOff, ColumnCollection[ 0 ].Width, this.LineHeight );
			float IndentedTextXOffset = IndentedRect.X + ENTRY_PADLEFT * 2 + TREE_EXPANSION_GLYPH_WIDTH;
			RectangleF IndentedGlyphOffsetRect = new RectangleF( IndentedTextXOffset, IndentedRect.Y, IndentedRect.Width - IndentedTextXOffset, IndentedRect.Height );

			{
				DrawRowEntry( Gfx, Item, ColumnCollection[ 0 ], ref IndentedGlyphOffsetRect, Item.BackColor, Item.ForeColor, bSelected );
			}


			int LineX = (int)XOff + (int)ChildXOff + EXPANDED_LINE_X - 8;
			Rectangle GlyphRect = new Rectangle( (int)IndentedRect.X + ENTRY_PADLEFT + TREE_EXPANSION_GLYPH_PAD, (int)IndentedRect.Y, TREE_EXPANSION_GLYPH_WIDTH, (int)IndentedRect.Height );
			int HalfLineHeight = this.LineHeight / 2;
			Point GlyphCenter = new Point( GlyphRect.X + GlyphRect.Width / 2, HalfLineHeight + (int)YOff );

			// Make make sure GlyphCenter is actually on the glyph center!
			if( ( HalfLineHeight & 1 ) == 1 )
			{
				GlyphCenter.Y -= 1;
			}

			RectangleF Rect = new RectangleF( XOff + ColumnCollection[ 0 ].Width, YOff, 0, this.LineHeight );

			int NumIterations = Math.Min(ColumnCollection.Count - 1, Item.SubItems.Count);
			for(int i = 0; i < NumIterations; ++i)
			{
				FTreeListViewColumnHeader Column = ColumnCollection[i + 1];
				Rect.Width = Column.Width;

				if(Item.UseItemStyleForSubItems)
				{
					DrawRowEntry(Gfx, Item.SubItems[i], Column, ref Rect, Item.BackColor, Item.ForeColor, bSelected);
				}
				else
				{
					DrawRowEntry(Gfx, Item.SubItems[i], Column, ref Rect, Item.SubItems[i].BackColor, Item.SubItems[i].ForeColor, bSelected);
				}
				
				Rect.X += Rect.Width;
			}



			// Set clipping rectangle to avoid lines/glyphs drawing outside of the first column's area
			Rectangle ClipRect = new Rectangle( 0, (int)YOff, ColumnCollection[ 0 ].Width, this.LineHeight );
			Gfx.SetClip( ClipRect );


			if( Item.Parent != null )
			{
				if(Item.Nodes.Count > 0)
				{
					Gfx.DrawLine(ExpandedItemLinePen, GlyphCenter, new Point(LineX, GlyphCenter.Y));
				}
				else
				{
					GlyphCenter.X = (int)IndentedGlyphOffsetRect.X + (ENTRY_PADLEFT - EXPANDED_LINE_PAD_RIGHT);
					Gfx.DrawLine(ExpandedItemLinePen, GlyphCenter, new Point(LineX, GlyphCenter.Y));
				}
			}

			if(Item.Nodes.Count > 0)
			{
				if(Item.Expanded)
				{
					//TreeNodeExpandedGlyphRenderer.DrawBackground(Gfx, GlyphRect);
					Renderer.DrawExpansionGlyph(Gfx, false, GlyphRect);
				}
				else
				{
					//TreeNodeClosedGlyphRenderer.DrawBackground(Gfx, GlyphRect);
					Renderer.DrawExpansionGlyph(Gfx, true, GlyphRect);
				}

			}

			// Restore clipping rect
			Gfx.ResetClip();

			
			if( bSelected )
			{
				//account for border's drawing on the outside by subtracting 1 pixel
				BGRect.Width -= 1;
				BGRect.Height -= 1;
				Gfx.DrawRectangle( SelectedItemBorderPen, BGRect );
			}
		}


		/// <summary>
		/// Draws only the vertical line that connects a child item to its parent
		/// </summary>
		private void DrawVerticalConnectionLine( Graphics Gfx, FTreeListViewItem Item, float XOff, float ChildXOff, float YOff, int InParentDist )
		{
			if( Item.Parent != null )
			{
				if( YOff >= HEADER_HEIGHT )
				{
					int LineX = (int)XOff + (int)ChildXOff + EXPANDED_LINE_X - CHILD_INDENT_AMOUNT / 2;
					int HalfLineHeight = this.LineHeight / 2;

					int ParentDistInPixels = InParentDist * LineHeight;
					int TopY = Math.Max( HEADER_HEIGHT, (int)YOff - HalfLineHeight - ParentDistInPixels );
					int BottomY = HalfLineHeight + (int)YOff;

					if( TopY < BottomY )
					{
						// Set clipping rectangle to avoid lines/glyphs drawing outside of the first column's area
						Rectangle ClipRect = new Rectangle( 0, TopY, ColumnCollection[ 0 ].Width, BottomY );
						Gfx.SetClip( ClipRect );

						// Draw vertical line from parent
						Gfx.DrawLine( ExpandedItemLinePen, new Point( LineX, TopY ), new Point( LineX, BottomY ) );

						// Restore clipping rect
						Gfx.ResetClip();
					}
				}
			}
		}

		
		/**
		 * Draws a row entry
		 * 
		 * @param	Gfx		The GDI+ graphics device.
		 * @param	Text	The text to draw.
		 * @param	Column	The column the entry belongs to.
		 * @param	Rect	The bounds of the row.
		 */
		private void DrawRowEntry(Graphics Gfx, FTreeListViewCell Item, FTreeListViewColumnHeader Column, ref RectangleF Rect, Color BackgroundColor, Color ForeGroundColor, bool bSelected)
		{
			RectangleF StringRect = Rect;
			StringRect.X += ENTRY_PADLEFT;
			StringRect.Width -= ENTRY_PADLEFT;

            if (Item.Draw(Gfx, Column, ref StringRect, BackColor, ForeGroundColor, bSelected))
            {
                return;
            }
            else
            {
                string Text = Item.Text;

                //TextFormatFlags Flags = TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.NoClipping | TextFormatFlags.TextBoxControl;
                switch (Column.TextAlign)
                {
                    case HorizontalAlignment.Left:
                        {
                            HeaderStringFormat.Alignment = StringAlignment.Near;
                            //Flags |= TextFormatFlags.Left;
                            break;
                        }
                    case HorizontalAlignment.Center:
                        {
                            HeaderStringFormat.Alignment = StringAlignment.Center;
                            //Flags |= TextFormatFlags.HorizontalCenter;
                            break;
                        }
                    case HorizontalAlignment.Right:
                        {
                            HeaderStringFormat.Alignment = StringAlignment.Far;
                            StringRect.Width -= ENTRY_PADRIGHT;
                            //Flags |= TextFormatFlags.Right;
                            break;
                        }
                }

                HeaderStringFormat.LineAlignment = StringAlignment.Center;

                if (bSelected)
                {
                    Gfx.DrawString(Text, this.Font, SystemBrushes.HighlightText, StringRect, HeaderStringFormat);
                }
                else
                {
                    RowBrush.Color = BackgroundColor;
                    Gfx.FillRectangle(RowBrush, Rect);
                    //TextRenderer.DrawText(Gfx, Text, this.Font, new Rectangle((int)Rect.X, (int)Rect.Y, (int)Rect.Width, (int)Rect.Height), ForeGroundColor, BackgroundColor, Flags);
                    RowBrush.Color = ForeGroundColor;
                    Gfx.DrawString(Text, this.Font, RowBrush, StringRect, HeaderStringFormat);
                }
            }
		}

		/**
		 * Draws a column header.
		 * 
		 * @param	Gfx		The GDI+ graphics device.
		 */
		void DrawColumnHeaders(Graphics Gfx)
		{
			if(this.ColumnCollection.Count > 0)
			{
				Rectangle Rect = new Rectangle(HorizontalScrollBar.Visible ? -HorizontalScrollBar.Value : 0, 0, 0, HEADER_HEIGHT);
				for(int Index = 0; Index < ColumnCollection.Count; ++Index)
				{
					if(Rect.X >= this.GetClientWidth())
					{
						break;
					}

					FTreeListViewColumnHeader Header = ColumnCollection[Index];

					Rect.Width = Header.Width;
					if(bMouseIsDown)
					{
						if(Index == MouseDownColumn && Index == HotColumn && ResizableColumn == -1)
						{
							//HeaderPressedRenderer.DrawBackground(Gfx, Rect);
							Renderer.DrawHeaderColumnBackground(Gfx, EHeaderColumnBackgroundState.Pressed, Rect);
						}
						else if(Index == HotColumn || Index == ResizableColumn)
						{
							//HeaderHotRenderer.DrawBackground(Gfx, Rect);
							Renderer.DrawHeaderColumnBackground(Gfx, EHeaderColumnBackgroundState.Hot, Rect);
						}
						else
						{
							//HeaderMainRenderer.DrawBackground(Gfx, Rect);
							Renderer.DrawHeaderColumnBackground(Gfx, EHeaderColumnBackgroundState.Normal, Rect);
						}
					}
					else if(Index == HotColumn || Index == ResizableColumn)
					{
						//HeaderHotRenderer.DrawBackground(Gfx, Rect);
						Renderer.DrawHeaderColumnBackground(Gfx, EHeaderColumnBackgroundState.Hot, Rect);
					}
					else
					{
						//HeaderMainRenderer.DrawBackground(Gfx, Rect);
						Renderer.DrawHeaderColumnBackground(Gfx, EHeaderColumnBackgroundState.Normal, Rect);
					}

					switch(Header.TextAlign)
					{
						case HorizontalAlignment.Left:
							{
								HeaderStringFormat.Alignment = StringAlignment.Near;
								break;
							}
						case HorizontalAlignment.Center:
							{
								HeaderStringFormat.Alignment = StringAlignment.Center;
								break;
							}
						case HorizontalAlignment.Right:
							{
								HeaderStringFormat.Alignment = StringAlignment.Far;
								break;
							}
					}

					HeaderStringFormat.LineAlignment = StringAlignment.Center;
					
					RectangleF RectF = RectangleToRectangleF(ref Rect);
					RectF.X += HEADER_PADLEFT;
					RectF.Width = RectF.Width - (HEADER_PADLEFT + ENTRY_PADRIGHT);

					Gfx.DrawString(Header.Text, this.Font, TextBrush, RectF, HeaderStringFormat);

					Rect.X += Header.Width;
				}

				if(Rect.X < this.GetClientWidth())
				{
					Rect.Width = this.Width - Rect.X + 2;
					//HeaderMainRenderer.DrawBackground(Gfx, Rect);
					Renderer.DrawHeaderColumnBackground(Gfx, EHeaderColumnBackgroundState.Normal, Rect);
				}
			}
			else
			{
				Gfx.FillRectangle(SystemBrushes.Control, 0, 0, this.GetClientWidth(), HEADER_HEIGHT);
			}
		}

		/**
		 * Converts a Renctangle to RectangleF.
		 * 
		 * @param	Rect	The rectangle to convert.
		 */
		public static RectangleF RectangleToRectangleF(ref Rectangle Rect)
		{
			return new RectangleF((float)Rect.X, (float)Rect.Y, (float)Rect.Width, (float)Rect.Height);
		}

		/**
		 * Prevents updates from causing a redraw until EndUpdate() is called.
		 */
		public void BeginUpdate()
		{
			this.bUpdating = true;
		}

		/**
		 * Updates and redraws the control.
		 */
		public void EndUpdate()
		{
			this.bUpdating = false;
			UpdateScrollBars();
			Invalidate(); //I assume you did some updating between the update function pair
		}

		/**
		 * Locates the column that contains the point if any.
		 * 
		 * @param	Pt		The point to derive the column index from.
		 */
		public int GetColumnIndexFromPoint(Point Pt)
		{
			Rectangle HeaderRect = new Rectangle(HorizontalScrollBar.Visible ? -HorizontalScrollBar.Value : 0, 0, 0, HEADER_HEIGHT);
			bool bFound = false;
			int Index = 0;

			for(; Index < ColumnCollection.Count; HeaderRect.X += ColumnCollection[Index].Width, ++Index)
			{
				HeaderRect.Width = ColumnCollection[Index].Width;
				if(HeaderRect.Contains(Pt))
				{
					bFound = true;
					break;
				}
			}

			if(!bFound)
			{
				Index = -1;
			}

			return Index;
		}

		/**
		 * Returns the index of the column that contains the point within it's resizing region.
		 * 
		 * @param	Pt		The point to check the column headers against.
		 */
		public int GetMoveableColumnIndexFromPoint(Point Pt)
		{
			//minus 1 accounts for the | at the end of a column header and makes it look better
			Rectangle HeaderRect = new Rectangle(-MOVEABLE_COL_WIDTH / 2 - 1, 0, MOVEABLE_COL_WIDTH, HEADER_HEIGHT);

			if(HorizontalScrollBar.Visible)
			{
				HeaderRect.X -= HorizontalScrollBar.Value;
			}

			bool bFound = false;
			int Index = 0;

			for(; Index < ColumnCollection.Count; ++Index)
			{
				HeaderRect.X += ColumnCollection[Index].Width;
				if(HeaderRect.Contains(Pt))
				{
					bFound = true;
					break;
				}
			}

			if(!bFound)
			{
				Index = -1;
			}

			return Index;
		}

		/**
		 * Event handler for when the mouse is moved.
		 * 
		 * @param	Args		The event's arguments.
		 */
		protected override void OnMouseMove(MouseEventArgs Args)
		{
			base.OnMouseMove(Args);

			if(bMouseIsDown && ResizableColumn != -1)
			{
				ColumnCollection[ResizableColumn].Width += Args.X - LastMousePosition.X;
				UpdateScrollBars();
			}

			LastMousePosition = Args.Location;

			UpdateColumnsFromMousePosition(Args.Location);
		}

		/**
		 * Updates how the column headers are drawn based on the location of the mouse.
		 * 
		 * @param	Location	The location of the mouse within the control's client area.
		 */
		void UpdateColumnsFromMousePosition(Point Location)
		{
			Rectangle HeaderRect = new Rectangle(0, 0, this.Width, HEADER_HEIGHT);

			if(ColumnCollection.Count > 0 && HeaderRect.Contains(Location))
			{
				int Index = GetMoveableColumnIndexFromPoint(Location);

				if(Index != -1)
				{
					HotColumn = -1;
					if(Index != ResizableColumn)
					{
						ResizableColumn = Index;
						this.Cursor = Cursors.VSplit;
						Invalidate();
					}
				}
				else
				{
					if(ResizableColumn != -1)
					{
						this.Cursor = DefaultCursor;
						ResizableColumn = -1;
						Invalidate();
					}

					Index = GetColumnIndexFromPoint(Location);

					if(HotColumn != Index)
					{
						HotColumn = Index;

						Invalidate();
					}
				}
			}
			else
			{
				if(HotColumn != -1 || (ResizableColumn != -1 && !bMouseIsDown))
				{
					ResizableColumn = -1;
					HotColumn = -1;
					this.Cursor = DefaultCursor;
					Invalidate();
				}
			}
		}

		/**
		 * Event handler for when a mouse button is pressed.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnMouseDown(MouseEventArgs Args)
		{
			base.OnMouseDown(Args);

			bMouseIsDown = true;

			if(ColumnCollection.Count > 0)
			{
				ResizableColumn = GetMoveableColumnIndexFromPoint(Args.Location);

				if(ResizableColumn != -1)
				{
					this.Cursor = Cursors.VSplit;
				}
				else
				{
					this.Cursor = DefaultCursor;

					MouseDownColumn = GetColumnIndexFromPoint(Args.Location);

					if(MouseDownColumn != -1)
					{
						Invalidate();
					}
					else
					{
						bool bClickedExpansion;
						FTreeListViewItem SelectedItem = GetItemAt(Args.X, Args.Y, out bClickedExpansion);

						if(bClickedExpansion)
						{
							if(SelectedItem != SelectedRow)
							{
								SelectedRow = SelectedItem;
								OnItemSelectionChanged(new EventArgs());
							}

							SelectedItem.Expanded = !SelectedItem.Expanded; //calls Invalidate() for us
						}
						else if(SelectedItem != SelectedRow)
						{
							SelectedRow = SelectedItem;
							Invalidate();
							OnItemSelectionChanged(new EventArgs());
						}
					}
				}
			}
			else
			{
				MouseDownColumn = -1;
				ResizableColumn = -1;
			}
		}

		/**
		 * Event handler for when a mouse button is released.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnMouseUp(MouseEventArgs Args)
		{
			base.OnMouseUp(Args);

			bMouseIsDown = false;
			if(MouseDownColumn != -1)
			{
				MouseDownColumn = -1;
				Invalidate();
			}

			UpdateColumnsFromMousePosition(Args.Location);

			bool bClickedExpansion;
			FTreeListViewItem SelectedItem = GetItemAt(Args.X, Args.Y, out bClickedExpansion);

			if(SelectedItem != null && SelectedItem == SelectedRow)
			{
				OnItemMouseClick(new FTreeListViewMouseClickItemEventArgs(SelectedItem, Args.Button, Args.Clicks, Args.X, Args.Y, Args.Delta));
			}
		}

		/**
		 * Event handler for when a mouse leaves the control.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnMouseLeave(EventArgs Args)
		{
			base.OnMouseLeave(Args);

			this.Cursor = DefaultCursor;

			if(HotColumn != -1 || ResizableColumn != -1)
			{
				ResizableColumn = -1;
				HotColumn = -1;
				Invalidate();
			}
		}

		/**
		 * Event handler for when a mouse button is double clicked.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnMouseDoubleClick(MouseEventArgs Args)
		{
			base.OnMouseDoubleClick(Args);

			bool bClickedExpansion;
			FTreeListViewItem SelectedItem = GetItemAt(Args.X, Args.Y, out bClickedExpansion);

			if(SelectedItem != null && !bClickedExpansion)
			{
				OnItemMouseDoubleClick( new FTreeListViewMouseClickItemEventArgs(SelectedItem, Args.Button, Args.Clicks, Args.X, Args.Y, Args.Delta));
			}
		}

		/**
		 * Event handler for when a mouse wheel is scrolled.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnMouseWheel(MouseEventArgs Args)
		{
			base.OnMouseWheel(Args);

			if(this.VerticalScrollBar.Visible)
			{
				int Ticks = -Args.Delta / 40;

				if(Ticks < 0)
				{
					VerticalScrollBar.Value = Math.Max(0, VerticalScrollBar.Value + Ticks);
				}
				else
				{
					int Max = VerticalScrollBar.Maximum - VerticalScrollBar.LargeChange + 1;
					int NewValue = VerticalScrollBar.Value + Ticks;

					if(NewValue > Max)
					{
						NewValue = Max;
					}

					VerticalScrollBar.Value = NewValue;
				}
			}
		}

		/**
		 * Updates the drawable (client) area of the control.
		 */
		internal void UpdateClientArea()
		{
			if(!this.bUpdating)
			{
				UpdateScrollBars();
				Invalidate();
			}
		}

		/**
		 * Returns the number of viewable rows.
		 */
		int GetViewableRowCount()
		{
			return (this.GetClientHeight() - HEADER_HEIGHT) / (this.LineHeight);
		}

		/**
		 * Returns the item that contains the specified point or null.
		 * 
		 * @param	X					The X coordinate.
		 * @param	Y					The Y coordinate.
		 * @param	bHitExpansionGlyph	This is set to true if the point is also contained within the expansion glyph.
		 */
		public FTreeListViewItem GetItemAt(int X, int Y, out bool bHitExpansionGlyph)
		{
			FTreeListViewItem ReturnItem = null;
			bHitExpansionGlyph = false;

			if(X >= 0 && X < this.GetTotalHeaderWidth() - (HorizontalScrollBar.Visible ? HorizontalScrollBar.Value : 0))
			{
				int ClientHeight = GetClientHeight();
				if(Y >= HEADER_HEIGHT && Y <= ClientHeight)
				{
					Rectangle Rect = new Rectangle(0, HEADER_HEIGHT, GetClientWidth(), this.LineHeight);
					int ViewableRows = GetViewableRowCount();

					for(int RowOffset = 0; RowOffset < ViewableRows; ++RowOffset)
					{
						if(Rect.Contains(X, Y))
						{
							if(VerticalScrollBar.Visible)
							{
								RowOffset += VerticalScrollBar.Value;
							}

							int FoundChildDepth = 0;
							ReturnItem = GetViewableRow( RowOffset, ref FoundChildDepth );


							if(ReturnItem != null)
							{
								//NOTE: We only have to test the left and right bound because since we already know it's within the row rectangle we know the Y value is good to go
								int LeftGlyphBound =
									(HorizontalScrollBar.Visible ? -HorizontalScrollBar.Value : 0) +
									ENTRY_PADLEFT + TREE_EXPANSION_GLYPH_PAD +
									CHILD_INDENT_AMOUNT * FoundChildDepth;
								int RightGlyphBound = LeftGlyphBound + TREE_EXPANSION_GLYPH_WIDTH;

								if(X >= LeftGlyphBound && X <= RightGlyphBound)
								{
									bHitExpansionGlyph = true;
								}
							}
							
							break;
						}

						Rect.Y += Rect.Height;
					}
				}
			}

			return ReturnItem;
		}

		/**
		 * Gets the sub-node within the client area at the specified index.
		 * 
		 * @param	RowsToDraw		The current drawable row index.
		 * @param	bFoundSubNode	This is set to true if a sub node is the drawable node at the specified index.
		 * @param	RowItem			This is set to the drawable sub node if it is found.
		 * @param	Index			The drawable row index being searched for.
		 * @param	Item			The parent item.
		 */
		void GetViewableSubNodes(ref int RowsToDraw, ref bool bFoundSubNode, ref FTreeListViewItem RowItem, int Index, FTreeListViewItem Item, int ParentDepth, ref int FoundChildDepth )
		{
			int MyDepth = ParentDepth + 1;

			foreach(FTreeListViewItem Node in Item.Nodes)
			{
				if(bFoundSubNode)
				{
					break;
				}

				if(RowsToDraw == Index)
				{
					bFoundSubNode = true;
					RowItem = Node;
					FoundChildDepth = MyDepth;
					break;
				}

				++RowsToDraw;

				if(Node.Expanded && Node.Nodes.Count > 0)
				{
					GetViewableSubNodes( ref RowsToDraw, ref bFoundSubNode, ref RowItem, Index, Node, MyDepth, ref FoundChildDepth );
				}
			}
		}

		/**
		 * Gets the row within the client area at the specified index.
		 * 
		 * @param	Index	The index of the row being searched for.
		 * @param	FoundChildDepth	[in/out] The depth of the item in the tree (0 = root level)
		 */
		FTreeListViewItem GetViewableRow(int Index, ref int FoundChildDepth )
		{
			FoundChildDepth = 0;

			FTreeListViewItem RowItem = null;
			int RowsToDraw = 0;
			bool bFoundSubNode = false;

			foreach(FTreeListViewItem Item in ItemCollection)
			{
				if(bFoundSubNode)
				{
					break;
				}

				if(RowsToDraw == Index)
				{
					RowItem = Item;
					FoundChildDepth = 0;
					break;
				}

				++RowsToDraw;

				if(Item.Expanded && Item.Nodes.Count > 0)
				{
					int MyDepth = 1;

					foreach( FTreeListViewItem Node in Item.Nodes )
					{
						if( bFoundSubNode )
						{
							break;
						}

						if(RowsToDraw == Index)
						{
							RowItem = Node;
							bFoundSubNode = true;
							FoundChildDepth = MyDepth;
							break;
						}

						++RowsToDraw;

						if(Node.Expanded && Node.Nodes.Count > 0)
						{
							GetViewableSubNodes(ref RowsToDraw, ref bFoundSubNode, ref RowItem, Index, Node, MyDepth, ref FoundChildDepth );
						}
					}
				}
			}

			return RowItem;
		}

		/**
		 * Returns the height of the client area.
		 */
		int GetClientHeight()
		{
			if(this.HorizontalScrollBar.Visible)
			{
				return this.Height - this.HorizontalScrollBar.Height;
			}

			return this.Height;
		}

		/**
		 * Returns the width of the client area.
		 */
		int GetClientWidth()
		{
			if(this.VerticalScrollBar.Visible)
			{
				return this.Width - this.VerticalScrollBar.Width;
			}

			return this.Width;
		}

		/**
		 * Returns the sum of all header column widths.
		 */
		int GetTotalHeaderWidth()
		{
			int TotalHeaderWidth = 0;
			foreach(FTreeListViewColumnHeader Header in ColumnCollection)
			{
				TotalHeaderWidth += Header.Width;
			}

			return TotalHeaderWidth;
		}

		/**
		 * Gets the number of viewable sub nodes that need to be drawn.
		 * 
		 * @param	RowsToDraw	The total number of rows that need to be draw is incremented by every viewable row.
		 * @param	Item		The parent item.
		 */
		void CountSubNodesToDraw(ref int RowsToDraw, FTreeListViewItem Item)
		{
			if(Item.Expanded)
			{
				foreach(FTreeListViewItem Node in Item.Nodes)
				{
					++RowsToDraw;
					CountSubNodesToDraw(ref RowsToDraw, Node);
				}
			}
		}

		/**
		 * Gets the total number viewable rows.
		 */
		int GetRowsToDrawCount()
		{
			int RowsToDraw = 0;

			foreach(FTreeListViewItem Item in ItemCollection)
			{
				++RowsToDraw;

				if(Item.Expanded)
				{
					foreach(FTreeListViewItem Node in Item.Nodes)
					{
						++RowsToDraw;
						CountSubNodesToDraw(ref RowsToDraw, Node);
					}
				}
			}

			return RowsToDraw;
		}

		/**
		 * Updates the scroll bars size and status.
		 */
		void UpdateScrollBars()
		{
			int TotalHeaderWidth = GetTotalHeaderWidth();

			if(TotalHeaderWidth > this.GetClientWidth() && !this.HorizontalScrollBar.Visible)
			{
				this.HorizontalScrollBar.Visible = true;
				this.HorizontalScrollBar.Minimum = 0;
				this.HorizontalScrollBar.Maximum = TotalHeaderWidth;
				this.HorizontalScrollBar.Value = 0;
				this.HorizontalScrollBar.LargeChange = GetClientWidth();
			}
			else if(TotalHeaderWidth > this.GetClientWidth())
			{
				this.HorizontalScrollBar.Maximum = TotalHeaderWidth;
				this.HorizontalScrollBar.LargeChange = System.Math.Max(0, GetClientWidth());
			}
			else
			{
				this.HorizontalScrollBar.Visible = false;
			}

			int ViewableRows = GetViewableRowCount();
			int RowsToDraw = GetRowsToDrawCount();
			if(ViewableRows < RowsToDraw && !this.VerticalScrollBar.Visible)
			{
				this.VerticalScrollBar.Visible = true;
				this.VerticalScrollBar.Minimum = 0;
				this.VerticalScrollBar.Maximum = System.Math.Max(0, RowsToDraw - 1);
				this.VerticalScrollBar.Value = 0;
				this.VerticalScrollBar.LargeChange = System.Math.Max(0, ViewableRows);
			}
			else if(ViewableRows < RowsToDraw)
			{
				this.VerticalScrollBar.Maximum = System.Math.Max(0, RowsToDraw - 1);
				this.VerticalScrollBar.LargeChange = System.Math.Max(0, ViewableRows);

				int Value = this.VerticalScrollBar.Maximum - ViewableRows;
				if(this.VerticalScrollBar.Value > Value)
				{
					this.VerticalScrollBar.Value = Value;
				}
			}
			else if(ViewableRows >= RowsToDraw)
			{
				this.VerticalScrollBar.Visible = false;
			}
		}

		/**
		 * Event handler for when the control is resized.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnResize(EventArgs Args)
		{
			base.OnResize(Args);
			UpdateScrollBars();
		}

		/**
		 * Event handler for when the item selected is changed.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected void OnItemSelectionChanged(EventArgs Args)
		{
			if(ItemSelectionChangedEvent != null)
			{
				ItemSelectionChangedEvent(this, Args);
			}
		}

		/**
		 * Event handler for when an item has been clicked by the mouse.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected void OnItemMouseClick(FTreeListViewMouseClickItemEventArgs Args)
		{
			if(ItemMouseClickEvent != null)
			{
				ItemMouseClickEvent(this, Args);
			}
		}

		/**
		 * Event handler for when an item has been double clicked by the mouse.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected void OnItemMouseDoubleClick(FTreeListViewMouseClickItemEventArgs Args)
		{
			if(ItemMouseDoubleClickEvent != null)
			{
				ItemMouseDoubleClickEvent(this, Args);
			}
		}

		/**
		 * Event handler for when a key has been pressed.
		 * 
		 * @param	Args	The event arguments.
		 */
		protected override void OnKeyDown(KeyEventArgs Args)
		{
			base.OnKeyDown(Args);

			if(SelectedRow != null)
			{
				switch(Args.KeyCode)
				{
					case Keys.Up:
						{
							FTreeListViewItem PrevRow = GetPreviousRow(SelectedRow);

							if(PrevRow != null)
							{
								SelectedRow = PrevRow;
								this.OnItemSelectionChanged(new EventArgs());

								if(!ScrollRowIntoView(GetDrawableIndexFromRow(SelectedRow)))
								{
									Invalidate();
								}
							}
							break;
						}
					case Keys.Down:
						{
							FTreeListViewItem NextRow = GetNextRow(SelectedRow);

							if(NextRow != null)
							{
								SelectedRow = NextRow;
								this.OnItemSelectionChanged(new EventArgs());

								if(!ScrollRowIntoView(GetDrawableIndexFromRow(SelectedRow)))
								{
									Invalidate();
								}
							}
							break;
						}
					case Keys.Right:
						{
							if(SelectedRow.Nodes.Count > 0 && !SelectedRow.Expanded)
							{
								SelectedRow.Expanded = true; //calls Invalidate() for us
							}
							break;
						}
					case Keys.Left:
						{
							if(SelectedRow.Nodes.Count > 0 && SelectedRow.Expanded)
							{
								SelectedRow.Expanded = false; //calls Invalidate() for us
							}
							break;
						}
				}
			}
			else
			{
				if(ItemCollection.Count > 0 && Args.KeyCode == Keys.Down)
				{
					SelectedRow = ItemCollection[0];
					OnItemSelectionChanged(new EventArgs());
					Invalidate();
				}
			}
		}

		/**
		 * Event handler for when special keys are pressed.
		 * 
		 * @param	Msg		The windows message.
		 * @param	KeyData	Information about the key event.
		 */
		protected override bool ProcessCmdKey(ref Message Msg, Keys KeyData)
		{
			if(KeyData == Keys.Down || KeyData == Keys.Up || KeyData == Keys.Right || KeyData == Keys.Left)
			{
				OnKeyDown(new KeyEventArgs(KeyData));
			}
			return base.ProcessCmdKey(ref Msg, KeyData);
		}

		/**
		 * Returns the next visible row after the supplied row or null.
		 * 
		 * @param	Row		The row to start searching after.
		 */
		FTreeListViewItem GetNextRow(FTreeListViewItem Row)
		{
			if(Row == null)
			{
				return null;
			}

			FTreeListViewItem NextItem = null;

			if(Row.Parent == null)
			{
				if(Row.Nodes.Count > 0 && Row.Expanded)
				{
					NextItem = GetNextSubRow(Row);
				}
				else if(Row.Index < ItemCollection.Count - 1)
				{
					NextItem = ItemCollection[Row.Index + 1];
				}
			}
			else
			{
				if(Row.Nodes.Count > 0 && Row.Expanded)
				{
					NextItem = Row.Nodes[0];
				}
				else
				{
					NextItem = GetNextRowFromParent(Row.Parent, Row.Index);
				}
			}

			return NextItem;
		}

		/**
		 * Walks up the sub-node hierarchy to retrieve the next visible row or null.
		 * 
		 * @param	Parent	The parent node.
		 * @param	Index	The current sub-node's index.
		 */
		FTreeListViewItem GetNextRowFromParent(FTreeListViewItem Parent, int Index)
		{
			FTreeListViewItem NextItem = null;

			if(Parent != null)
			{
				if(Index >= 0 && Index < Parent.Nodes.Count - 1)
				{
					NextItem = Parent.Nodes[Index + 1];
				}
				else
				{
					NextItem = GetNextRowFromParent(Parent.Parent, Parent.Index);
				}
			}
			else if(Index < ItemCollection.Count - 1)
			{
				NextItem = ItemCollection[Index + 1];
			}

			return NextItem;
		}

		/**
		 * Walks down the sub-node hierarchy to retrieve the next visible row or null.
		 * 
		 * @param	Row		The parent row.
		 */
		FTreeListViewItem GetNextSubRow(FTreeListViewItem Row)
		{
			FTreeListViewItem NextRow = null;

			if(Row.Nodes[0].Nodes.Count > 0 && Row.Nodes[0].Expanded)
			{
				NextRow = GetNextSubRow(Row.Nodes[0]);
			}
			else
			{
				NextRow = Row.Nodes[0];
			}

			return NextRow;
		}

		/**
		 * Returns the previous visible row before the supplied row or null.
		 * 
		 * @param	Msg		The windows message.
		 */
		FTreeListViewItem GetPreviousRow(FTreeListViewItem Row)
		{
			if(Row == null || (Row.Parent == null && Row.Index == 0))
			{
				return null;
			}

			FTreeListViewItem PrevItem = null;

			if(Row.Parent == null)
			{
				PrevItem = ItemCollection[Row.Index - 1];

				if(PrevItem.Nodes.Count > 0 && PrevItem.Expanded)
				{
					PrevItem = GetPrevSubRow(PrevItem);
				}
			}
			else
			{
				if(Row.Index > 0)
				{
					PrevItem = GetPrevSubRow(Row.Parent.Nodes[Row.Index - 1]);
				}
				else
				{
					PrevItem = Row.Parent;
				}
			}

			return PrevItem;
		}

		/**
		 * Walks down the sub-node hierarchy to get the previous visible row or null.
		 * 
		 * @param	Item	The parent item.
		 */
		private FTreeListViewItem GetPrevSubRow(FTreeListViewItem Item)
		{
			FTreeListViewItem PrevItem = Item;

			if(Item.Nodes.Count > 0 && Item.Expanded)
			{
				PrevItem = GetPrevSubRow(Item.Nodes[Item.Nodes.Count - 1]);
			}

			return PrevItem;
		}

		/**
		 * Releases all managed and native resources.
		 */
		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			this.VerticalScrollBar.Dispose();
			this.HorizontalScrollBar.Dispose();
			this.RowBrush.Dispose();
		}

		/**
		 * Returns the drawing index of the specified row or -1.
		 * 
		 * @param	Row		The row to look for.
		 */
		private int GetDrawableIndexFromRow(FTreeListViewItem Row)
		{
			int Index = 0;
			bool bFound = false;

			foreach(FTreeListViewItem Item in ItemCollection)
			{
				if(bFound || Item == Row)
				{
					bFound = true;
					break;
				}

				++Index;

				if(Item.Expanded && Item.Nodes.Count > 0)
				{
					GetDrawableIndexFromRowSubNode(Row, Item, ref Index, ref bFound);
				}
			}

			if(!bFound)
			{
				Index = -1;
			}

			return Index;
		}

		/**
		 * Returns the drawing index of the specified row within or -1.
		 * 
		 * @param	Row					The row to look for.
		 * @param	Item				The parent item.
		 * @param	Index				The index counter.
		 * @param	bFoundInSubNode		This is set to true if the row is found.
		 */
		private void GetDrawableIndexFromRowSubNode(FTreeListViewItem Row, FTreeListViewItem Item, ref int Index, ref bool bFoundInSubNode)
		{
			foreach(FTreeListViewItem Node in Item.Nodes)
			{
				if(bFoundInSubNode || Node == Row)
				{
					bFoundInSubNode = true;
					break;
				}

				++Index;

				if(Node.Expanded && Node.Nodes.Count > 0)
				{
					GetDrawableIndexFromRowSubNode(Row, Node, ref Index, ref bFoundInSubNode);
				}
			}
		}

		/**
		 * Returns true if the row at DrawIndex was scrolled into view.
		 * 
		 * @param	DrawIndex	The draw index of the row to be scrolled into view.
		 */
		bool ScrollRowIntoView(int DrawIndex)
		{
			if(DrawIndex < 0)
			{
				return false;
			}

			int Height = GetViewableRowCount();
			bool bScrolled = false;

			if(VerticalScrollBar.Visible)
			{
				if(DrawIndex < VerticalScrollBar.Value)
				{
					VerticalScrollBar.Value = DrawIndex;
					bScrolled = true;
				}
				else if(DrawIndex >= VerticalScrollBar.Value + Height)
				{
					DrawIndex = (VerticalScrollBar.Value + Height) - DrawIndex - 1;
					DrawIndex = -DrawIndex;

					bScrolled = true;
					VerticalScrollBar.Value = Math.Min(VerticalScrollBar.Value + DrawIndex, VerticalScrollBar.Maximum - VerticalScrollBar.LargeChange + 1);
				}
			}

			return bScrolled;
		}
	}
}
