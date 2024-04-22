/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SequenceObjectHelper extends Object
	native;

cpptext
{
	/**
	 * Called when the user double clicks on a sequence object, can be used to display object specific
	 * property dialogs.
	 *
	 * @param InEditor	Pointer to the editor that initiated the callback.
	 * @param InObject	Pointer to the object that was clicked on.
	 */
	virtual void  OnDoubleClick( const class WxKismet* InEditor, USequenceObject* InObject ) const { }

	/**
	 * Called when the user right clicks on a sequence object, should show a object specific context menu.
	 *
	 * @param InEditor	Pointer to the editor that initiated the callback.
	 */
	virtual void  ShowContextMenu( class WxKismet* InEditor ) const { }

	/**
	 * Called when the Kismet editor wants the object to add itself to a tree control.
	 *
	 * @param InEditor		Pointer to the editor that initiated the callback.
	 * @param InTreeCtrl	Pointer to the tree control we will be adding an item to.
	 * @param ParentItem	The parent of the item that will be added.
	 * @return				The tree item we created.
	 */
	virtual wxTreeItemId AddToTreeControl(  class WxKismet* InEditor, class wxTreeCtrl* InTreeCtrl, class wxTreeItemId& ParentItem ) const 
	{
		return wxTreeItemId();
	}
}
