/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __CATEGORY_PROPERTY_NODE_H__
#define __CATEGORY_PROPERTY_NODE_H__

#include "PropertyNode.h"

/*-----------------------------------------------------------------------------
	WxCategoryPropertyControl
-----------------------------------------------------------------------------*/

/**
 * The header item for a category of items.
 */
class WxCategoryPropertyControl : public WxPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxCategoryPropertyControl);

	/**
	 * Virtual function to allow generation of display name
	 */
	virtual void InitBeforeCreate(void);

	/**
	 * Initialize this property window.  Must be the first function called after creating the window.
	 */
	virtual void Create(wxWindow* InParent);

	/** Does nothing to avoid flicker when we redraw the screen. */
	void OnEraseBackground( wxEraseEvent& Event )
	{
		// Intentionally do nothing.
	}

	void OnPaint( wxPaintEvent& In );
	void OnLeftClick( wxMouseEvent& In );
	void OnRightClick( wxMouseEvent& In );
	void OnMouseMove( wxMouseEvent& In );

	virtual FString GetPath() const;

	/**
	 * Accessor to get category name from the CategoryPropertyNode
	 */
	const FName& GetCategoryName() const;

	/**
	 * Gets only the last part of the category name (the subcategory)
	 *
	 * @return	Returns the subcategory name string
	 */
	const FString GetSubcategoryName() const;

	/**
	 * virtualized to allow custom naming
	 */
	virtual FString GetDisplayName (void) const;

	DECLARE_EVENT_TABLE();

protected:

	/** A human readable version of the property name. */
	FString DisplayName;
};

//-----------------------------------------------------------------------------
//FCategoryPropertyNode - Used for the highest level categories
//-----------------------------------------------------------------------------
class FCategoryPropertyNode : public FPropertyNode
{
public:
	FCategoryPropertyNode(void);
	virtual ~FCategoryPropertyNode(void);

	/**
	 * Overriden function to get the derived objet node
	 */
	virtual FCategoryPropertyNode* GetCategoryNode(void) { return this;}

	/**
	 * Accessor functions for getting a category name
	 */
	void SetCategoryName(const FName& InCategoryName) { CategoryName = InCategoryName; }
	const FName& GetCategoryName(void) const { return CategoryName; }

	/**
	 * Checks to see if this category is a 'sub-category'
	 *
	 * @return	True if this category node is a sub-category, otherwise false
	 */
	UBOOL IsSubcategory() const
	{
		return GetParentNode() != NULL && const_cast<FPropertyNode*>( GetParentNode() )->GetCategoryNode() != NULL;
	}


protected :
	/**
	 * Overriden function for special setup
	 */
	virtual void InitBeforeNodeFlags (void);
	/**
	 * Overriden function for Creating Child Nodes
	 */
	virtual void InitChildNodes(void);
	/**
	 * Overriden function for Creating the corresponding wxPropertyControl
	 */
	virtual void CreateInternalWindow(void);
	/**
	 * Function to allow different nodes to add themselves to focus or not
	 */
	virtual void AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& AddSelfToVisibleArray);

	/**
	 * Appends my path, including an array index (where appropriate)
	 */
	virtual void GetQualifedName( FString& PathPlusIndex, const UBOOL bWithArrayIndex ) const;

	/**Index of the first child property defined by script*/
	virtual INT GetPropertyScriptOrder(void) OVERRIDE;

	/**
	 * Stored category name for the window
	 */
	FName CategoryName;
};

#endif // __CATEGORY_PROPERTY_NODE_H__
