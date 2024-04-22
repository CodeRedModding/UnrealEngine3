/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __OBJECT_PROPERTY_NODE_H__
#define __OBJECT_PROPERTY_NODE_H__

#include "PropertyNode.h"

/*-----------------------------------------------------------------------------
	WxObjectsPropertyControl
-----------------------------------------------------------------------------*/

/**
 * This holds all the child controls and anything related to
 * editing the properties of a collection of UObjects.
 */
class WxObjectsPropertyControl : public WxPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxObjectsPropertyControl);

	//////////////////////////////////////////////////////////////////////////
	// Constructors
	/**
	 * Initialize this property window.  Must be the first function called after creating the window.
	 */
	virtual void Create(wxWindow* InParent);

	//////////////////////////////////////////////////////////////////////////
	// Object binding

	/**
	 * Returns TRUE if the specified object is in this node's object list.
	 */
	//UBOOL HandlesObject(UObject* InObject) const;

	//////////////////////////////////////////////////////////////////////////
	// Misc accessors
	virtual FString GetPath() const;

	/**
	 * Handles paint events.
	 */
	void OnPaint( wxPaintEvent& In );

	/** Does nothing to avoid flicker when we redraw the screen. */
	void OnEraseBackground( wxEraseEvent& Event );

	/**
	 * Objects are never displayed like this, but needed to satisfy interface reqs
	 */
	virtual FString GetDisplayName (void) const { return FString(); }


private:

	DECLARE_EVENT_TABLE();
};

//-----------------------------------------------------------------------------
//FObjectPropertyNode - Used for the root and various sub-nodes
//-----------------------------------------------------------------------------
class FObjectPropertyNode : public FPropertyNode
{
public:
	FObjectPropertyNode(void);
	virtual ~FObjectPropertyNode(void);

	/**
	 * Overriden function to get the derived objet node
	 */
	virtual FObjectPropertyNode* GetObjectNode(void) { return this;}

	/**
	 * Returns the UObject at index "n" of the Objects Array
	 * @param InIndex - index to read out of the array
	 */
	UObject* GetObject (const INT InIndex);
	/** Returns the number of objects for which properties are currently being edited. */
	INT GetNumObjects() const	{ return Objects.Num(); }

	/**
	 * Adds a new object to the list.
	 */
	void AddObject( UObject* InObject );
	/**
	 * Removes an object from the list.
	 */
	void RemoveObject(UObject* InObject);
	/**
	 * Removes all objects from the list.
	 */
	void RemoveAllObjects();
	// Called when the object list is finalized, Finalize() finishes the property window setup.
	void Finalize(void);

	/** @return		The base-est baseclass for objects in this list. */
	UClass*			GetObjectBaseClass()       { return BaseClass; }
	/** @return		The base-est baseclass for objects in this list. */
	const UClass*	GetObjectBaseClass() const { return BaseClass; }

	//////////////////////////////////////////////////////////////////////////
	/** @return		The property stored at this node, to be passed to Pre/PostEditChange. */
	virtual UProperty*		GetStoredProperty()		{ return StoredProperty; }

	//////////////////////////////////////////////////////////////////////////
	// Object iteration
	typedef TArray<UObject*>::TIterator TObjectIterator;
	typedef TArray<UObject*>::TConstIterator TObjectConstIterator;

	TObjectIterator			ObjectIterator()			{ return TObjectIterator( Objects ); }
	TObjectConstIterator	ObjectConstIterator() const	{ return TObjectConstIterator( Objects ); }

	// The bArrayPropertiesCanDifferInSize flag is an override for array properties which want to display
	// e.g. the "Clear" and "Empty" buttons, even though the array properties may differ in the number of elements.
	virtual UBOOL GetReadAddress(FPropertyNode* InNode,
		UBOOL InRequiresSingleSelection,
		TArray<BYTE*>& OutAddresses,
		UBOOL bComparePropertyContents = TRUE,
		UBOOL bObjectForceCompare = FALSE,
		UBOOL bArrayPropertiesCanDifferInSize = FALSE);

	/**
	 * fills in the OutAddresses array with the addresses of all of the available objects.
	 * @param InItem		The property to get objects from.
	 * @param OutAddresses	Storage array for all of the objects' addresses.
	 */
	virtual UBOOL GetReadAddress(FPropertyNode* InNode,
		TArray<BYTE*>& OutAddresses);
	/**
	 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
	 *
	 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
	 */
	virtual BYTE* GetValueBaseAddress( BYTE* Base );

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
	virtual void AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& OutVisibleArray);
	/**
	 * Appends my path, including an array index (where appropriate)
	 */
	virtual void GetQualifedName( FString& PathPlusIndex, const UBOOL bWithArrayIndex ) const;

	/**
	 * Looks at the Objects array and creates the best base class.  Called by
	 * Finalize(); that is, when the list of selected objects is being finalized.
	 */
	void SetBestBaseClass();



	/** The list of objects we are editing properties for. */
	TArray<UObject*>		Objects;

	/** The lowest level base class for objects in this list. */
	UClass*					BaseClass;

	/**
	 * The property passed to Pre/PostEditChange calls.  Separated from WxPropertyControl::Property
	 * because WxObjectsPropertyControl is structural only, and Property should be NULL so this node
	 * doesn't eg return values for GetPropertyText(), etc.
	 */
	UProperty*				StoredProperty;
private:
};

#endif // __OBJECT_PROPERTY_NODE_H__
