/**
 * Defines the methods that all List components must define. Note that this interface is not implemented by the existing components, and does not need to be implemented, it is just a reference.
 */ 
 
/**************************************************************************

Filename    :   IList.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

interface gfx.interfaces.IList {
		
// Public Methods:
	/**
	 * The data model to be displayed in the component.  The DataProvider can be an Array or any object exposing the appropriate API.
	 */
	function get dataProvider():Object;
	function set dataProvider(value:Object):Void;
	
	/**
	 * Get the index of the item that is selected in a single-selection list.
	 */
	function get selectedIndex():Object;
	/**
	 * Set the index of the item that is selected in a single-selection list.
	 * @param value The new selected index.
	 */
	function set selectedIndex(value:Number):Void;
	
	/**
	 * Scrolls the list to the item at the specified index. If the index is not out of range, the scroll position does not change.
	 * @param index The index in the dataProvider to scroll to.
	 */
	function scrollToIndex(index:Number):Void;
}