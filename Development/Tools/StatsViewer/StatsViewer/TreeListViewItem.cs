/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;

namespace StatsViewer
{
    /**
     * This class represents a cell in a row (either the row header, which owns the other cells; or an individual child cell)
     */
    public class FTreeListViewCell
    {
        public object Tag;

        protected Color BackgroundColor = SystemColors.Window;
        protected Color ForegroundColor = SystemColors.WindowText;
        protected string TextString = "";

        /**
         * Gets/Sets the text that is displayed by the item.
         */
        public string Text
        {
            get { return TextString; }
            set { TextString = value; OnModified(); }
        }

        /**
         * Gets/Sets the background color of the item.
         */
        public Color BackColor
        {
            get { return BackgroundColor; }
            set { BackgroundColor = value; OnModified(); }
        }

        /**
         * Gets/Sets the foreground color of the item.
         */
        public Color ForeColor
        {
            get { return ForegroundColor; }
            set { ForegroundColor = value; OnModified(); }
        }

        /**
         * Event handler for when the item is modified.
        */
        protected virtual void OnModified()
        {
        }

        /**
         * Custom drawing method.  Returns true if implemented, otherwise the default string rendering will occur.
         */
        public virtual bool Draw(Graphics Gfx, FTreeListViewColumnHeader Column, ref RectangleF Rect, Color BackgroundColor, Color ForeGroundColor, bool bSelected)
        {
            return false;
        }
    }

	/**
	 * This class represents a row within a FTreeListView.
	 */
    public class FTreeListViewItem : FTreeListViewCell
	{
		#region Internal Classes
		/**
		 * This represents the value that is displayed in a column within a row.
		 */
        public class FTreeListViewSubItem : FTreeListViewCell
		{
            protected FTreeListViewItem Owner;
            
            /**
             * Sets owner of the item.
             */
			internal FTreeListViewItem OwningItem
			{
				set { Owner = value; }
			}

            /**
             * Event handler for when the item is modified.
            */
            protected override void OnModified()
            {
                if (Owner != null)
                {
                    Owner.OnModified();
                }
            }
            
            /**
             * Constructor.
             * 
             * @param	Owner	The owner of the item.
             */
			public FTreeListViewSubItem(FTreeListViewItem Owner)
			{
				this.Owner = Owner;
			}

			/**
			 * Constructor.
			 * 
			 * @param	Owner	The owner of the item.
			 * @param	Text	The text to be displayed by the item.
			 */
			public FTreeListViewSubItem(FTreeListViewItem Owner, string Text)
			{
				this.Owner = Owner;
				this.Text = Text;
			}
		}

		/**
		 * This class holds a collection of FTreeListViewSubItem's.
		 */
		public class FTreeListViewSubItemCollection : IList<FTreeListViewSubItem>
		{
			List<FTreeListViewSubItem> SubItems;
			FTreeListViewItem Owner;

			/**
			 * Constructor.
			 * 
			 * @param	Owner	The item that owns the collection.
			 */
			public FTreeListViewSubItemCollection(FTreeListViewItem Owner)
			{
				this.Owner = Owner;

				if(Owner.ListView != null)
				{
					SubItems = new List<FTreeListViewSubItem>(Owner.ListView.Columns.Count);
				}
				else
				{
					SubItems = new List<FTreeListViewSubItem>();
				}
			}

			/**
			 * Adds a new FTreeListViewSubItem to the collection with the specified text.
			 * 
			 * @param	Text	The text the new item will display.
			 */
			public void Add(string Text)
			{
				this.Add(new FTreeListViewSubItem(Owner, Text));
			}

			#region IList<FTreeListViewSubItem> Members
			/**
			 * Returns the index of an item or -1 if it doesn't exist.
			 * 
			 * @param	Item	The item to search for.
			 */
			public int IndexOf(FTreeListViewSubItem Item)
			{
				return SubItems.IndexOf(Item);
			}

			/**
			 * Inserts a new item at the specified index.
			 * 
			 * @param	Index	The index to insert the item at.
			 * @param	Item	The item to insert.
			 */
			public void Insert(int index, FTreeListViewSubItem Item)
			{
				Item.OwningItem = Owner;
				SubItems.Insert(index, Item);
				Owner.OnModified();
			}

			/**
			 * Removes the item at the specified index.
			 * 
			 * @param	Index	The index to remove the item at.
			 */
			public void RemoveAt(int Index)
			{
				FTreeListViewSubItem Item = SubItems[Index];
				Item.OwningItem = null;
				SubItems.RemoveAt(Index);
				Owner.OnModified();
			}

			/**
			 * Indexer overloading the [] operator.
			 * 
			 * @param	Index	The index of the item to Get/Set.
			 */
			public FTreeListViewSubItem this[int Index]
			{
				get
				{
					return SubItems[Index];
				}
				set
				{
					SubItems[Index].OwningItem = null;
					SubItems[Index] = value;
					value.OwningItem = Owner;
					Owner.OnModified();
				}
			}

			#endregion

			#region ICollection<FTreeListViewSubItem> Members
			/**
			 * Adds an item to the end of the collection.
			 * 
			 * @param	Item	The item to be added.
			 */
			public void Add(FTreeListViewSubItem Item)
			{
				SubItems.Add(Item);
				Item.OwningItem = Owner;
				Owner.OnModified();
			}

			/**
			 * Removes all items from the collection.
			 */
			public void Clear()
			{
				foreach(FTreeListViewSubItem Item in SubItems)
				{
					Item.OwningItem = null;
				}
				SubItems.Clear();
				Owner.OnModified();
			}

			/**
			 * Returns true if the collection contains the specified item.
			 * 
			 * @param	Item	The item to check for containment.
			 */
			public bool Contains(FTreeListViewSubItem Item)
			{
				return SubItems.Contains(Item);
			}

			/**
			 * Copies the collection to the specified array.
			 * 
			 * @param	Ar			The array to copy the collection into.
			 * @param	ArrayIndex	The index into the array to start copying at.
			 */
			public void CopyTo(FTreeListViewSubItem[] Array, int ArrayIndex)
			{
				SubItems.CopyTo(Array, ArrayIndex);
			}

			/**
			 * Gets the number of items in the collection.
			 */
			public int Count
			{
				get { return SubItems.Count; }
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
			public bool Remove(FTreeListViewSubItem Item)
			{
				bool bSuccess = SubItems.Remove(Item);

				if(bSuccess)
				{
					Item.OwningItem = null;
					Owner.OnModified();
				}

				return bSuccess;
			}

			#endregion

			#region IEnumerable<FTreeListViewSubItem> Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			public IEnumerator<FTreeListViewSubItem> GetEnumerator()
			{
				return SubItems.GetEnumerator();
			}

			#endregion

			#region IEnumerable Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
			{
				return SubItems.GetEnumerator();
			}

			#endregion
		}

		/**
		 * This class holds a collection of FTreeListViewItem's.
		 */
		public class FTreeListViewItemNodeCollection : IList<FTreeListViewItem>
		{
			List<FTreeListViewItem> Nodes = new List<FTreeListViewItem>();
			FTreeListViewItem Owner;

			/**
			 * Constructor.
			 * 
			 * @param	Owner	The owner of the collection.
			 */
			public FTreeListViewItemNodeCollection(FTreeListViewItem Owner)
			{
				this.Owner = Owner;
			}

			/**
			 * Initializes the relevant fields of an item that has just been added to the collection.
			 * 
			 * @param	Item	The new item.
			 * @param	Index	The index the item was inserted at.
			 */
			void InitializeNewItem(FTreeListViewItem Item, int Index)
			{
				Item.Parent = Owner;
				Item.Owner = Owner.ListView;
				Item.Index = Index;
			}

			/**
			 * Clears the relevant fields of an item that has been removed from the collection.
			 * 
			 * @param	Item	The removed item.
			 */
			void ClearRemovedItem(FTreeListViewItem Item)
			{
				Item.Parent = null;
				Item.Owner = null;
				Item.Index = -1;
			}

			#region IList<FTreeListViewSubItem> Members
			/**
			 * Returns the index of an item or -1 if it doesn't exist.
			 * 
			 * @param	Item	The item to search for.
			 */
			public int IndexOf(FTreeListViewItem Item)
			{
				return Nodes.IndexOf(Item);
			}

			/**
			 * Inserts a new item at the specified index.
			 * 
			 * @param	Index	The index to insert the item at.
			 * @param	Item	The item to insert.
			 */
			public void Insert(int Index, FTreeListViewItem Item)
			{
				InitializeNewItem(Item, Index);
				Nodes.Insert(Index, Item);
				for(int i = Index + 1; i < Nodes.Count; ++i)
				{
					Nodes[i].Index = i;
				}

				Owner.OnModified();
			}

			/**
			 * Removes the item at the specified index.
			 * 
			 * @param	Index	The index to remove the item at.
			 */
			public void RemoveAt(int Index)
			{
				FTreeListViewItem Item = Nodes[Index];
				ClearRemovedItem(Item);
				Nodes.RemoveAt(Index);

				for(int i = Index; i < Nodes.Count; ++i)
				{
					Nodes[i].Index = i;
				}

				Owner.OnModified();
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
					return Nodes[Index];
				}
				set
				{
					ClearRemovedItem(Nodes[Index]);
					Nodes[Index] = value;
					InitializeNewItem(value, Index);
					Owner.OnModified();
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
				Nodes.Add(Item);
				InitializeNewItem(Item,  Nodes.Count - 1);
				Owner.OnModified();
			}

			/**
			 * Removes all items from the collection.
			 */
			public void Clear()
			{
				foreach(FTreeListViewItem Item in Nodes)
				{
					ClearRemovedItem(Item);
				}
				Nodes.Clear();
				Owner.OnModified();
			}

			/**
			 * Returns true if the collection contains the specified item.
			 * 
			 * @param	Item	The item to check for containment.
			 */
			public bool Contains(FTreeListViewItem Item)
			{
				return Nodes.Contains(Item);
			}

			/**
			 * Copies the collection to the specified array.
			 * 
			 * @param	Ar			The array to copy the collection into.
			 * @param	ArrayIndex	The index into the array to start copying at.
			 */
			public void CopyTo(FTreeListViewItem[] Array, int ArrayIndex)
			{
				Nodes.CopyTo(Array, ArrayIndex);
			}

			/**
			 * Gets the number of items in the collection.
			 */
			public int Count
			{
				get { return Nodes.Count; }
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
				bool bSuccess = Nodes.Remove(Item);

				if(bSuccess)
				{
					ClearRemovedItem(Item);
					Owner.OnModified();
				}

				return bSuccess;
			}

			#endregion

			#region IEnumerable<FTreeListViewItem> Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			public IEnumerator<FTreeListViewItem> GetEnumerator()
			{
				return Nodes.GetEnumerator();
			}

			#endregion

			#region IEnumerable Members
			/**
			 * Returns an enumerator for iterating over the collection.
			 */
			System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
			{
				return Nodes.GetEnumerator();
			}

			#endregion
		}
		#endregion

		string ToolTipTextString = "";
		FTreeListView Owner;
		FTreeListViewItem ParentNode;
		FTreeListViewSubItemCollection SubItemCollection;
		FTreeListViewItemNodeCollection NodeCollection;
		int IndexInListView = -1;
		bool bUseStyleForSubItems = true;
		bool bExpanded = false;


		/**
		 * Gets/Sets the text that is displayed by the item's tooltip.
		 */
		public string ToolTipText
		{
			get { return ToolTipTextString; }
			set { ToolTipTextString = value; }
		}

		/**
		 * Gets/Sets the index of the item in its parent item or the ListView.
		 */
		public int Index
		{
			get { return IndexInListView; }
			internal set { IndexInListView = value; }
		}

		/**
		 * Gets/Sets whether or not the item style is forced for all sub-items.
		 */
		public bool UseItemStyleForSubItems
		{
			get { return bUseStyleForSubItems; }
			set { bUseStyleForSubItems = value; OnModified(); }
		}

		/**
		 * Gets/Sets FTreeListView that owns the item.
		 */
		public FTreeListView ListView
		{
			get { return Owner; }
			internal set { Owner = value; }
		}

		/**
		 * Gets/Sets whether or not the item's sub-nodes are being displayed.
		 */
		public bool Expanded
		{
			get { return bExpanded; }
			set
			{
				if(this.NodeCollection.Count > 0)
				{
					if(value != bExpanded)
					{
						bExpanded = value;
						OnModified();
					}
				}
			}
		}

		/**
		 * Gets the collection of sub-items.
		 */
		public FTreeListViewSubItemCollection SubItems
		{
			get { return SubItemCollection; }
		}

		/**
		 * Gets the collection of sub-nodes.
		 */
		public FTreeListViewItemNodeCollection Nodes
		{
			get { return NodeCollection; }
		}

		/**
		 * Gets the item's parent.
		 */
		internal FTreeListViewItem Parent
		{
			get { return ParentNode; }
			set { ParentNode = value; OnParentChanged(); }
		}

		/**
		 * Constructor.
		 */
		public FTreeListViewItem()
		{
			SubItemCollection = new FTreeListViewSubItemCollection(this);
			NodeCollection = new FTreeListViewItemNodeCollection(this);
		}

        /**
         * Event handler for when the item is modified.
         */
        protected override void OnModified()
        {
            if (this.Owner != null)
            {
                Owner.UpdateClientArea();
            }
        }
        
        /**
         * Constructor.
         * 
         * @param	Text	The text that the item will display.
         */
		public FTreeListViewItem(string Text)
		{
			SubItemCollection = new FTreeListViewSubItemCollection(this);
			NodeCollection = new FTreeListViewItemNodeCollection(this);
			this.Text = Text;
		}

		/**
		 * Constructor.
		 * 
		 * @param	Items	The text that the item and its sub-item's will display.
		 */
		public FTreeListViewItem(string[] Items)
		{
			if(Items == null || Items.Length == 0)
			{
				throw new ArgumentException("Items");
			}

			NodeCollection = new FTreeListViewItemNodeCollection(this);
			SubItemCollection = new FTreeListViewSubItemCollection(this);
			this.TextString = Items[0];

			for(int i = 1; i < Items.Length; ++i)
			{
				this.SubItemCollection.Add(new FTreeListViewSubItem(this, Items[i]));
			}
		}

		protected virtual void OnParentChanged()
		{
		}
	}
}
