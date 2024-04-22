/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PROPERTY_NODE_H__
#define __PROPERTY_NODE_H__

#include "Bitmaps.h"
#include "LevelUtils.h"

#define PROP_CategoryHeight		24
#define PROP_CategorySpacer		8
#define PROP_SubcategoryHeight	16
#define PROP_SubcategorySpacer	4
#define PROP_GenerateHeight		-1
#define	PROP_DefaultItemHeight	20
#define PROP_Indent				16
#define PROP_TextLineSpacing	8

#define PROP_ARROW_Width				16
#define PROP_OFFSET_None				-1
#define PROP_Default_Favorite_Index		-1

//-----------------------------------------------------------------------------
//forward declarations
class FPropertyNode;
class FObjectPropertyNode;
class FCategoryPropertyNode;
class FItemPropertyNode;

class WxObjectsPropertyControl;
class WxCategoryPropertyControl;
class WxItemPropertyControl;


class WxPropertyWindow;
class WxPropertyWindowFrame;
class UPropertyDrawProxy;
class UPropertyInputProxy;




//-----------------------------------------------------------------------------
//EPropertyNodeFlags - Flags used internally by FPropertyNode
//-----------------------------------------------------------------------------
namespace EPropertyWindowFlags
{
	typedef UINT Type;

	//Child windows should be sorted
	const Type Sorted											= 1 << 0;
	/** Returns true if we want to allow the ENTER key to be intercepted by text property fields and used to
	apply the text input.  This is usually what you want.  Otherwise, the ENTER key will be ignored
	and the parent dialog window will have a chance to handle the event */
	const Type AllowEnterKeyToApplyChanges						= 1 << 1;
	// Custom property editor support
	const Type SupportsCustomControls							= 1 << 2;
	// Show categories
	const Type ShouldShowCategories								= 1 << 3;
	// Non-editable properties
	const Type ShouldShowNonEditable							= 1 << 4;
	// Read-only properties
	const Type ReadOnly											= 1 << 5;
	const Type CanBeHiddenByPropertyWindowManager				= 1 << 6;
	//TRUE if escape key should deselect the actor selected
	const Type ExecDeselectOnEscape								= 1 << 7;
	//															= 1 << 8;
	//Auto expand categories
	const Type ShowOnlyModifiedItems							= 1 << 10;
	const Type ShowOnlyDifferingItems							= 1 << 11;

	const Type DataReconnectionRequested						= 1 << 12;
	const Type IgnoreRefreshRequests							= 1 << 13;

	//Is this a "favorites" property window
	const Type Favorites										= 1 << 14;
	//If this class has any favorites set for it
	const Type HasAnyFavoritesSet								= 1 << 15;

	//Use the object base class name rather than the object name as the window's title
	const Type UseTypeAsTitle									= 1 << 16;

	const Type NoFlags											= 0;
	const Type AllFlags											= 0xffffffff;

};

namespace EPropertyNodeFlags
{
	typedef UINT Type;

	const Type	IsSeen						= 1 << 0;		/** TRUE if this node can be seen based on current parent expansion.  Does not take into account clipping*/
	const Type	IsSeenDueToFiltering		= 1 << 1;		/** TRUE if this node has been accepted by the filter*/
	const Type	IsSeenDueToChildFiltering	= 1 << 2;		/** TRUE if this node or one of it's children is seen due to filtering.  It will then be forced on as well.*/
	const Type	IsParentSeenDueToFiltering	= 1 << 3;		/** True if the parent was visible due to filtering*/
	const Type	IsSeenDueToChildFavorite	= 1 << 4;		/** True if this node is seen to it having a favorite as a child */

	const Type	Expanded					= 1 << 5;		/** TRUE if this node should display its children*/
	const Type	CanBeExpanded				= 1 << 6;		/** TRUE if this node is able to be expanded */
	const Type	ChildControlsCreated		= 1 << 7;		/** TRUE if the child controls have been created by the InputProxy*/
	const Type	SortChildren				= 1 << 8;		/** TRUE if this node should sort its children*/
	const Type	EditInline					= 1 << 9;		/** TRUE if the property can be expanded into the property window. */
	const Type	EditInlineUse				= 1 << 10;		/** TRUE if the property is EditInline with a use button. */
	const Type	SingleSelectOnly			= 1 << 11;		/** TRUE if only a single object is selected. */
	const Type	IsWindowReallyCreated		= 1 << 12;		/** TRUE if wxWindow::Create was ever called on this window or if it just there to provide override functionality.*/
	const Type  ShowCategories				= 1 << 13;		/** TRUE if this node should show categories.  Different*/
	const Type  IsRealTime					= 1 << 14;		/** TRUE if this node needs realtime update or not */

	const Type  HasEverBeenExpanded			= 1 << 15;		/** TRUE if expand has ever been called on this node */
	const Type  InitChildrenOnDemand		= 1 << 16;		/** TRUE if this node does NOT have CPF_Edit but was added as a consequence of ShouldShowNonEditable */

	const Type  IsFavorite					= 1 << 17;		/** TRUE if this item has been dubbed a favorite by the user */
	const Type  IsForceScriptOrder			= 1 << 18;		/** TRUE if the property (or a parent) has "ScriptOrder" true in the metadata */
	const Type  NoChildrenDueToCircularReference = 1 << 19;	/** TRUE if this node has no children (but normally would) due to circular referencing */

	const Type  IsPropertyEditConst			= 1 << 20;		/** TRUE if this property is const or the class declares this variable const */

	const Type  ForceEditConstDisabledStyle = 1 << 21;		/** TRUE if we need to disable darker paint style of EditConst. Used for Parametr Values in MIC editor */

	const Type NoFlags					= 0;
};

namespace FPropertyNodeConstants
{
	const INT NoDepthRestrictions = -1;

	/** Character used to deliminate sub-categories in category path names */
	const TCHAR CategoryDelimiterChar = TCHAR( ',' );
};

//favorites system
namespace EPropertyFavorites
{
	enum
	{
		Off,
		LevelDesign,
		Performance,
		MaxFavoriteSet,
	};
};

///////////////////////////////////////////////////////////////////////////////
//
// Property windows.
//
///////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
	WxPropertyControl
-----------------------------------------------------------------------------*/

/**
 * Base class for all window types that appear in the property window.
 */
class WxPropertyControl : public wxWindow, public FDeferredInitializationWindow
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyControl);

	/** Destructor */
	virtual ~WxPropertyControl();

	/**
	 * Virtual function to allow custom controls to do work BEFORE the are "created"
	 */
	virtual void InitBeforeCreate(void) {};
	/**
	 * Initialize this property window.  Must be the first function called after creating the window.
	 */
	virtual void Create(wxWindow* InParent);

	/**
	 * Function to setup the two way connection early.  The window is created during the hierarchy init but not wxWindow::created until CreateWindowINternal is called
	 */
	void SetPropertyNode (FPropertyNode* InPropertyTreeNode) { PropertyNode = InPropertyTreeNode; }
	/**
	 * Allow custom WxPropertyControl objects to add their own parameters however they like.  Default is to just use InitChildNodes
	 */
	virtual void InitWindowDefinedChildNodes (void) {}

	/**
	 * Something has caused data to be removed from under the property window.  Request a full investigation by the main property window
	 */
	void RequestDataValidation(void);

	/**
	 * Returns the matching PropertyNode
	 */
	FPropertyNode*			GetPropertyNode(void)				{ return PropertyNode; }
	const FPropertyNode*	GetPropertyNode(void) const	{ return PropertyNode; }

	//Accessor functions to get to the PropertyNode's property member
	UProperty*					GetProperty(void);
	const UProperty*			GetProperty(void) const;

	//Detailed Access functions that go to the node flags
	UINT HasNodeFlags (const EPropertyNodeFlags::Type InTestFlags) const;

	/**
	 * Allows custom hiding of windows.
	 */
	virtual UBOOL IsDerivedForcedHide (void) const { return FALSE;}
	/**
	 *	Allows custom hiding of controls if conditions aren't met for conditional items
	 */
	virtual UBOOL IsControlsSuppressed (void);
	/**
	 * virtualized to allow custom naming
	 */
	virtual FString GetDisplayName (void) const { return FString(); }

protected:

	/** Parent item/window. */
	FPropertyNode* PropertyNode;

public:
	/** The proxy used to draw this windows property. */
	UPropertyDrawProxy*		DrawProxy;

	/** The proxy used to handle user input for this property. */
	UPropertyInputProxy*	InputProxy;

	/** The last x position the splitter was at during a drag operation. */
	INT						LastX;


public:
	/** Map of menu identifiers to classes - for editinline popup menus. */
	TMap<INT,UClass*>		ClassMap;

	/** Archetype to use for new objects.  For editinline popup menus. */
	UObject*				NewObjArchetype;

	/**
	 * Returns the property window this node belongs to.  The returned pointer is always valid.
	 */
	WxPropertyWindow* GetPropertyWindow();
	const WxPropertyWindow* GetPropertyWindow()const;
	/**
	 *  @return	The height of the property item, as determined by the input and draw proxies.
	 */
	INT GetPropertyHeight();

	/**
	 * Returns a string representation of the contents of the property.
	 */
	FString GetPropertyText();

	/**
	 * Finds the top-most object window that contains this item and issues a Refresh() command
	 */
	void RefreshHierarchy();

	

	/**
	 * Resets the value of the property associated with this item to its default value, if applicable.
	 */
	virtual void ResetPropertyValueToDefault() {}

	/**
	 * Parent path creation common to all types.
	 */
	FString GetParentPath() const;
	/**
	 * Creates a path to the current item.
	 */
	virtual FString GetPath() const;


	/**
	 * Checks to see if an X position is on top of the property windows splitter bar.
	 */
	UBOOL IsOnSplitter( INT InX );

	/**
	 * Calls through to the enclosing property window's NotifyPreChange.
	 */
	void NotifyPreChange(UProperty* PropertyAboutToChange, UObject* Object=NULL);

	/**
	 * Calls through to the enclosing property window's NotifyPreChange.
	 */
	void NotifyPostChange(FPropertyChangedEvent& InPropertyChangedEvent);

	///////////////////////////////////////////////////////////////////////////
	// Item expansion

	/**
	 * Displays a context menu for this property window node.
	 *
	 * @param	In	the mouse event that caused the context menu to be requested
	 *
	 * @return	TRUE if the context menu was shown.
	 */
	virtual UBOOL ShowPropertyItemContextMenu( wxMouseEvent& In ) { return FALSE; }

	///////////////////////////////////////////////////////////////////////////
	// Event handling

	void OnLeftClick( wxMouseEvent& In );
	void OnLeftUnclick( wxMouseEvent& In );
	void OnLeftDoubleClick( wxMouseEvent& In );
	void OnRightClick( wxMouseEvent& In );

	/**
	 * Toggles Active Focus of this property
	 * @return - whether to set focus on this window or not
	 */
	UBOOL ToggleActiveFocus ( wxMouseEvent& In );

	void OnSize( wxSizeEvent& In );
	void OnChangeFocus( wxFocusEvent& In );
	void OnPropItemButton( wxCommandEvent& In );
	void OnEditInlineNewClass( wxCommandEvent& In );
	void OnMouseMove( wxMouseEvent& In );
	void OnChar( wxKeyEvent& In );
	void OnContext_ResetToDefaults( wxCommandEvent& Event );

	/**
	 * Returns the draw proxy class for the property contained by this item.
	 */
	UClass* GetDrawProxyClass() const;

	/**
	 * Get the input proxy class for the property contained by this item.
	 */
	UClass* GetInputProxyClass() const;

	DECLARE_EVENT_TABLE();

protected:
	/**
	 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
	 * and (if the property window item is expandable) to toggle expansion state.
	 *
	 * @param	Event	the mouse click input that generated the event
	 *
	 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
	 */
	virtual UBOOL ClickedPropertyItem( wxMouseEvent& In );

	/**
	 * Finalizes draw and input proxies.  Must be called by derived constructors of
	 * WxPropertyControl-derived classes.  Child class implementations of FinishSetup
	 * should call up to their parent.
	 */
	virtual void FinishSetup();

	/**
	 * Sets the target property value for property-based coloration to the value of
	 * this property window node's value.
	 */
	void SetPropertyColorationTarget();

	/**
	 * Implements OnLeftDoubleClick().
	 * @param InX - Window relative cursor x position
	 * @param InY - Window relative cursor y position
	 */
	virtual void PrivateLeftDoubleClick(const INT InX, const INT InY);
};

//-----------------------------------------------------------------------------
//FPropertyNode - Base class for non window intermediates to the property window
//-----------------------------------------------------------------------------
class FPropertyNode
{
public:
	FPropertyNode(void);
	virtual ~FPropertyNode(void);

	/**
	 * Init Tree Node internally (used only derived classes to pass through variables that are common to all nodes
	 * @param InWxWindow						- window that is made outside of the InitNode function and coupled with the new FPropertyNode
	 * @param InParentNode						- Parent node in the hierarchy
	 * @param InTopLevelWindow					- the wrapper PropertyWindow that houses all child windows
	 * @param InProperty						- The property this node is modifying
	 * @param InPropertyOffset					- @TEMP
	 * @param InArrayIndex						- @TEMP
	 * @param bInAllowChildren					- @Allows children to be created
	 */
	void InitNode(WxPropertyControl* InWxWindow,
		FPropertyNode*		InParentNode,
		WxPropertyWindow*	InTopPropertyWindow,
		UProperty*			InProperty,
		INT					InPropertyOffset,
		INT					InArrayIdx,
		UBOOL				bInAllowChildren=TRUE);

	/**
	 * Used for rebuilding this nodes children
	 */
	void RebuildChildren (void);
	/**
	 * Used to forceably recreate child controls (text ctrl, etc)
	 */
	void InvalidateChildControls ();

	/**
	 * For derived windows to be able to add their nodes to the child array
	 */
	void AddChildNode (FPropertyNode* InNode) { ChildNodes.AddItem(InNode); }


	/**
	 * Interface function to get at the dervied FObjectPropertyNode class
	 */
	virtual FObjectPropertyNode* GetObjectNode(void) { return NULL;}
	/**
	 * Interface function to get at the dervied FCategoryPropertyNode class
	 */
	virtual FCategoryPropertyNode* GetCategoryNode(void) { return NULL;}

	/**
	 * Follows the chain of items upwards until it finds the object window that houses this item.
	 */
	FObjectPropertyNode* FindObjectItemParent();

	/**
	 * Follows the top-most object window that contains this property window item.
	 */
	FObjectPropertyNode* FindRootObjectItemParent();

	/**
	 * Quickly ensure all the correct windows are visible
	 */
	void FastRefreshEntireTree(void);

	/**
	 * When a window finds it is not visible, it informs all child windows recursively that they should also not be visible
	 */
	void HideSelfAndAllChildren (void);

	/** 
	 * Used to see if any data has been destroyed from under the property tree.  Should only be called by PropertyWindow::OnIdle
	 */
	UBOOL IsAllDataValid(void);

	//////////////////////////////////////////////////////////////////////////
	//Flags
	UINT HasNodeFlags (const EPropertyNodeFlags::Type InTestFlags) const				{ return PropertyNodeFlags & InTestFlags; }
	/**
	 * Sets the flags used by the window and the root node
	 * @param InFlags - flags to turn on or off
	 * @param InOnOff - whether to toggle the bits on or off
	 */
	void  SetNodeFlags (const EPropertyNodeFlags::Type InFlags, const UBOOL InOnOff);
	/**
	 * Gets wrapper window
	 */
	WxPropertyWindow* GetMainWindow(void);
	const WxPropertyWindow* GetMainWindow(void) const;

	virtual FPropertyNode* FindPropertyNode(const FString& InPropertyName);

	/**
	 * Mark this node as needing to be expanded/collapsed (and WILL refresh the windows system)
	 */
	void SetExpanded   (const UBOOL bInExpanded, const UBOOL bInRecurse, const UBOOL bInExpandParents=FALSE);
	/**
	 * 	 Mark node with proper name as needing to be expanded/collapsed (and WILL refresh the windows system)
	 */
	void SetExpandedItem(const FString& PropertyName, INT ArrayIndex, const UBOOL bInExpanded, const UBOOL bInExpandParents=FALSE);

	/**
	 * Saving and restoring functions for expanding sub windows by string name
	 */
	void RememberExpandedItems(TArray<FString>& InExpandedItems);
	void RestoreExpandedItems(const TArray<FString>& InExpandedItems);

	/**
	 * Returns the parent node in the hierarchy
	 */
	FPropertyNode*			GetParentNode	(void)			{ return ParentNode; }
	const FPropertyNode*	GetParentNode	(void) const	{ return ParentNode; }
	/**
	 * Returns the Property this Node represents
	 */
	UProperty*			GetProperty	(void)				{ return Property; }
	const UProperty*	GetProperty	(void) const		{ return Property; }

	/**
	 * Accessor functions for internals
	 */
	const INT GetPropertyOffset		(void) const { return PropertyOffset; }
	const INT GetArrayIndex			(void) const { return ArrayIndex; }

	/**
	 * IndentX Accessors
	 */
	INT  GetIndentX(void) const				{ return IndentX; } 
	void SetIndentX(const INT InIndentX)	{ IndentX = InIndentX; }
	/**
	 * ChildHeight Accessors
	 */
	INT  GetChildHeight(void) const				{ return ChildHeight; } 
	/**
	 * ChildSpacer Accessors
	 */
	INT  GetChildSpacer(void) const				{ return ChildSpacer; } 
	/** Safety Value representing Depth in the property tree used to stop diabolical topology cases*/
	INT GetMaxChildDepthAllowed(void) const { return MaxChildDepthAllowed; }
	/**Index of the property defined by script*/
	virtual INT GetPropertyScriptOrder(void);

	/**
	 * Return number of children that survived being filtered
	 */
	INT GetNumChildNodes (void) const { return ChildNodes.Num(); }
	/**
	 * Returns the matching Child node
	 */
	FPropertyNode* GetChildNode (const INT ChildIndex) const
	{
		check(IsWithin(ChildIndex, 0, ChildNodes.Num()));
		check(ChildNodes(ChildIndex));
		return ChildNodes(ChildIndex);
	}

	/*
	 * Returns the current list of Filter sub-strings that must match to be displayed
	 */
	const TArray<FString>& GetFilterStrings(void) const;

	/** @return whether this window's property is constant (can't be edited by the user) */
	UBOOL IsEditConst() const;
	/**
	 * Gets the corresponding Window for this node
	 */
	WxPropertyControl*			GetNodeWindow(void)				{ return NodeWindow; };
	const WxPropertyControl*	GetNodeWindow(void) const	{ return NodeWindow; };

	/**
	 * Interface function for adding visible items (doesn't accept categories or the root)
	 */
	void AppendVisibleWindows (OUT TArray<WxPropertyControl*>& FocusArray);

	/**
	 * Serializes the input and draw proxies, as well as all child windows.
	 *
	 * @param		Ar		The archive to read/write.
	 */
	void Serialize( FArchive& Ar );
	/**
	 * Gets the full name of this node
	 * @param PathPlusIndex - return value with full path of node
	 * @param bWithArrayIndex - If True, adds an array index (where appropriate)
	 */
	virtual void GetQualifedName( FString& PathPlusIndex, const UBOOL bWithArrayIndex ) const;

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

	/**
	 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
	 * to dynamic array elements, the pointer returned will be the location for that element's data. 
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
	 */
	virtual BYTE* GetValueAddress( BYTE* Base );
	/**
	 * Creates the appropriate editing control for the property specified.
	 *
	 * @param	InProperty		the property that will use the new property item
	 * @param	InArrayIndex	specifies which element of an array property that this property window will represent.  Only valid
	 *							when creating property window items for individual elements of an array.
	 */
	WxItemPropertyControl* CreatePropertyItem( UProperty* InProperty, INT InArrayIndex=INDEX_NONE);

	/**
	 * Detects if the window is actually visible in the property panel and not clipped
	 */
	UBOOL IsPartiallyUnclipped (void);
	/**
	 * Gets the clipped rect, used for more efficient filling
	 */
	wxRect GetClippedClientRect (void);
	/**
	 * If there is a property, sees if it matches.  Otherwise sees if the entire parent structure matches
	 */
	UBOOL GetDiffersFromDefault (void);

	/**Walks up the hierachy and return TRUE if any parent node is a favorite*/
	UBOOL IsChildOfFavorite (void) const;

	/**
	 * Sorts Child Nodes according to name
	 */
	void SortChildren();
	
	/**
	 * Assigns the specified FavoriteSortIndex to this node.
	 *
	 * @param	InFavoriteSortIndex		The index value to assign.
	 */
	void SetFavoriteIndex(INT InFavoriteSortIndex) { FavoriteSortIndex = InFavoriteSortIndex; }
	INT GetFavoriteIndex() const { return FavoriteSortIndex; }
	/**
	 * Used for window creation, to ensure parenting is correct
	 */
	wxWindow* GetParentNodeWindow(void);

protected:

	/** Adjusts parent window if needed (for favorites)*/
	void ReparentWindow();

	/**
	 * Destroys all node within the hierarchy
	 */
	void DestroyTree(const UBOOL bInDestroySelf=TRUE);

	/**
	 * Shows or Hides windows that have already been created for a particular node OR creates the window if it has never been seen before
	 */
	void ProcessShowVisibleWindows (void);
	/**
	 * Shows or Hides windows that have already been created for favorites and their children
	 */
	void ProcessShowVisibleWindowsFavorites (void);
	/**
	 * Shows or Hides control windows based on focus, all windows needing controls, and window clipping
	 */
	void ProcessControlWindows (void);

	/**
	 * Marks window's seem due to filtering flags
	 * @param InFilterStrings	- List of strings that must be in the property name in order to display
	 *					Empty InFilterStrings means display as normal
	 *					All strings must match in order to be accepted by the filter
	 * @param bParentAllowsVisible - is NOT true for an expander that is NOT expanded
	 */
	void FilterWindows(const TArray<FString>& InFilterStrings, const UBOOL bParentSeenDueToFiltering = FALSE);

	/**
	 * Marks windows as visible based on the filter strings or standard visibility
	 * @param bParentAllowsVisible - is NOT true for an expander that is NOT expanded
	 */
	void ProcessSeenFlags(const UBOOL bParentAllowsVisible);
	/**
	 * Marks windows as visible based their favorites status
	 */
	void ProcessSeenFlagsForFavorites(void);


	/**
	 * Interface function for Custom Setup of Node (priot to node flags being set)
	 */
	virtual void InitBeforeNodeFlags (void) {};
	/**
	 * Interface function for Custom expansion Flags.  Default is objects and categories which always expand
	 */
	virtual void InitExpansionFlags (void){ SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, TRUE); };
	/**
	 * Interface function for Creating Child Nodes
	 */
	virtual void InitChildNodes(void) = 0;
	/**
	 * Interface function for creating wxPropertyControl
	 */
	virtual void CreateInternalWindow(void) = 0;
	/**
	 * Function to allow different nodes to add themselves to focus or not
	 */
	virtual void AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& OutVisibleArray) = 0;
protected:
	/**
	 * Does the string compares to ensure this Name is acceptable to the filter that is passed in
	 */
	UBOOL IsFilterAcceptable(const TArray<FString>& InAcceptableNames, const TArray<FString>& InFilterStrings);
	/**
	 * Make sure that parent nodes are expanded
	 */
	void ExpandParent( UBOOL bInRecursive );
	/**
	 * Mark this node as needing to be expanded/collapsed called from the wrapper function SetExpanded
	 */
	void SetExpandedInternal   (const UBOOL bInExpanded, const UBOOL bInRecurse, const UBOOL bInExpandParents);
	/**
	 * Recursively searches through children for a property named PropertyName and expands it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	void SetExpandedItemInternal (const FString& InPropertyName, INT InArrayIndex, const UBOOL bInExpanded, const UBOOL bInExpandParents);


	/** @return		The property stored at this node, to be passed to Pre/PostEditChange. */
	UProperty*		GetStoredProperty()		{ return NULL; }

	/**
	 * The window this property WILL point to (if it is visible)
	 */
	WxPropertyControl* NodeWindow;
	/**
	 * The WxPropertyWindow that hold all the children nodes
	 */
	WxPropertyWindow* TopPropertyWindow;
	/**
	 * The node that is the parent of this node or NULL for the root
	 */
	FPropertyNode* ParentNode;
	/**
	 * List of all child nodes this node is responsible for
	 */
	TArray<FPropertyNode*> ChildNodes;

	/** The property being displayed/edited. */
	UProperty*				Property;
	/** Offset to the properties data. */
	INT						PropertyOffset;
	INT						ArrayIndex;
	/** Used when the property window is positioning/sizing child items. */
	INT						ChildHeight;
	/** Used when the property window is positioning/sizing child items. */
	INT						ChildSpacer;
	/** How far the text label for this item is indented when drawn. */
	INT						IndentX;
	/** Safety Value representing Depth in the property tree used to stop diabolical topology cases
	 * -1 = No limit on children
	 *  0 = No more children are allowed.  Do not process child nodes
	 *  >0 = A limit has been set by the property and will tick down for successive children
	 */
	INT MaxChildDepthAllowed;

		/**
	 * Used for keeping track of favorite property node placement when using dynamic sorting.
	 */
	INT FavoriteSortIndex;

	/**
	 * Used for flags to determine if the node is seen (if not seen and never been created, don't create it on display)
	 */
	EPropertyNodeFlags::Type PropertyNodeFlags;

	friend class WxPropertyWindow;
};

#endif // __PROPERTY_NODE_H__

