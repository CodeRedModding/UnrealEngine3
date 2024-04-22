/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;

namespace Stats
{
	/// <summary>
	/// This class provides information about the type of object held in a UI
	/// element. This is used for items added to tree/list views.
	/// </summary>
	public class UIBaseElement
	{
		/// <summary>
		/// Enum for the types of objects we hold in the tree
		/// </summary>
		public enum ElementType
		{
			UnknownObject,
			GroupObject,
			StatsObject
		};
		/// <summary>
		/// Holds the type of object for querying by UI elements
		/// </summary>
		private ElementType ElemType;
		/// <summary>
		/// Read only access to the UI type
		/// </summary>
		public ElementType UIType
		{
			get
			{
				return ElemType;
			}
		}

		/// <summary>
		/// Sets the type of object for querying in the tree/list views
		/// </summary>
		/// <param name="InType">The type of object we are</param>
		public UIBaseElement(ElementType InType)
		{
			ElemType = InType;
		}
	}
}
