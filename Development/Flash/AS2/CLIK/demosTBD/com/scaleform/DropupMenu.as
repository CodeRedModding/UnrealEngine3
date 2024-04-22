/**************************************************************************

Filename    :   DropupMenu.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

 /**
 * The DropdownMenu is a Button that shows the label of a selected item in a {@code dataProvider}, and displays a dropdown list when activated. The dropdown list can change the selected item in the {@code dataProvider}.
 
	<b>Events</b>
 	<ul><li>change: The {@code selectedIndex} of the {@code dataProvider} has changed</li></ul>

	<b>States</b>
	The DropdownMenu uses the states defined in its superClass, {@link gfx.controls.Button Button}.
 */

/*
	LM: We might want to automatically hide the scrollbar of the dropdown, in case its attached by reference.
	LM: We need to think about how to solve asynchronous gets on selectedItem - can we pass a token object or something?
		or perhaps we break with tradition and dispatch change for code initiated changes.
	LM: Change dropdown position to drop-up instead depending on component position on stage?
	LM: Add prompt?
	LM: Seems that the dropdown goes to the wrong index when opened. (+1?)
*/
 
import gfx.controls.DropdownMenu;
import gfx.controls.CoreList;
import gfx.data.DataProvider;
import gfx.managers.PopUpManager;
import gfx.ui.InputDetails;
import gfx.ui.NavigationCode;
import gfx.utils.Constraints;
import flash.geom.Point;

[InspectableList("disabled", "visible", "inspectableDropdown", "dropdownWidth")]
class com.scaleform.DropupMenu extends DropdownMenu {
	
	public function DropupMenu() { 
		super();
		dataProvider = [];
	}
	
	/**
	 * Open the dropdown list. The {@code selected} and {@code isOpen} properties of the DropdownMenu are set to {@code true} when open. Input will be passed to the dropdown when it is open before it is handled by the DropdownMenu.
	 */
	public function open():Void {
		if (_dropdown == null) {
			isOpen = false;
			return;
		}
		if (isOpen) { return; }
		isOpen = true;
		
		// Re-position
		PopUpManager.movePopUp(MovieClip(_dropdown), this, 0, -_dropdown.height);
		_dropdown.visible = true;
		/*if (_dropdown.scrollBar != null && MovieClip(_dropdown.scrollBar)._parent != _dropdown) {
			MovieClip(_dropdown.scrollBar)._visible = true;
		}*/
		if (_dropdownWidth != 0) {
			_dropdown.width = (_dropdownWidth == -1) ? __width : _dropdownWidth;
		}
		
		selected = true; // Select the Button state.
	}
	
	/** @exclude */
	public function toString():String {
		return "[Scaleform DropupMenu " + _name + "]";
	}
}