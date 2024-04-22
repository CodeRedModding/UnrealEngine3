//=============================================================================
// EditorLinkSelectionInterface
//
// Implement this interface to allow this object to perform special handling when 'linkselected' or 'unlinkselected'
// is called from the editor (for example, if the user has two objects selected and calls linkselected and you want the objects
// to bind themselves together in some fashion)
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
interface EditorLinkSelectionInterface 
	native;

cpptext
{
	virtual void LinkSelection(USelection* SelectedObjects){}
	virtual void UnLinkSelection(USelection* SelectedObjects){}
}