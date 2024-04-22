/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnrealEdPrivateClasses.h"
#include "PropertyNode.h"
#include "ObjectsPropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ItemPropertyNode.h"
#include "PropertyWindow.h"
#include "PropertyUtils.h"
#include "PropertyWindowManager.h"
#include "ScopedTransaction.h"
#include "ScopedPropertyChange.h"	// required for access to FScopedPropertyChange
#include "CurveEditorWindow.h"

#if WITH_MANAGED_CODE
	#include "ColorPickerShared.h"
#endif

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectIterator		TPropObjectIterator;

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectConstIterator	TPropObjectConstIterator;

//-----------------------------------------------------------------------------
//FPropertyNode
//-----------------------------------------------------------------------------

FPropertyNode::FPropertyNode(void)
: NodeWindow(NULL),
  TopPropertyWindow(NULL),
  ParentNode(NULL),
  Property(NULL),
  PropertyOffset(0),
  ArrayIndex(-1),
  ChildSpacer(0),
  ChildHeight(PROP_GenerateHeight),
  IndentX(0),
  MaxChildDepthAllowed(FPropertyNodeConstants::NoDepthRestrictions),
  PropertyNodeFlags (EPropertyNodeFlags::NoFlags),
  FavoriteSortIndex(PROP_Default_Favorite_Index)
{
}


FPropertyNode::~FPropertyNode(void)
{
#if WITH_MANAGED_CODE
	UnBindColorPickers(this);
#endif

	DestroyTree();
}

/**
 * Init Tree Node internally (used only derived classes to pass through variables that are common to all nodes
 * @param InWxWindow						- window that is made outside of the InitNode function and coupled with the new FPropertyNode
 * @param InParentNode					- Parent node in the hierarchy
 * @param InTopLevelWindow				- the wrapper PropertyWindow that houses all child windows
 * @param InProperty						- The property this node is modifying
 * @param InPropertyOffset				- @TEMP
 * @param InArrayIndex					- @TEMP
 */
void FPropertyNode::InitNode(WxPropertyControl* InWxWindow,
								 FPropertyNode*	InParentNode,
								 WxPropertyWindow*	InTopPropertyWindow,
								 UProperty*			InProperty,
								 INT				InPropertyOffset,
								 INT				InArrayIdx,
								 UBOOL				bInAllowChildren)
{
	//Dismantle the previous tree
	DestroyTree();

	//wx windows
	check(InWxWindow);
	NodeWindow = InWxWindow;		//default to NO visual representation
	InWxWindow->SetPropertyNode(this);	//make the reciprocal connection
	check(InTopPropertyWindow);
	TopPropertyWindow = InTopPropertyWindow;

	//tree hierarchy
	check(ParentNode != this);
	ParentNode = InParentNode;
	if (ParentNode)
	{
		//default to parents max child depth
		MaxChildDepthAllowed = ParentNode->MaxChildDepthAllowed;
		//if limitless or has hit the full limit
		if (MaxChildDepthAllowed > 0)
		{
			--MaxChildDepthAllowed;
		}
	}

	//Property Data
	Property		= InProperty;
	PropertyOffset	= InPropertyOffset;
	ArrayIndex		= InArrayIdx;

	//set some defaults for new windows
	NodeWindow->DrawProxy = NULL;
	NodeWindow->InputProxy = NULL;
	NodeWindow->Hide();

	PropertyNodeFlags = EPropertyNodeFlags::NoFlags;

	//default to copying from the parent
	if (ParentNode)
	{
		SetNodeFlags(EPropertyNodeFlags::SortChildren, ParentNode->HasNodeFlags(EPropertyNodeFlags::SortChildren));
		SetNodeFlags(EPropertyNodeFlags::ShowCategories, ParentNode->HasNodeFlags(EPropertyNodeFlags::ShowCategories));
		SetNodeFlags(EPropertyNodeFlags::IsForceScriptOrder, ParentNode->HasNodeFlags(EPropertyNodeFlags::IsForceScriptOrder));
	}
	else
	{
		SetNodeFlags(EPropertyNodeFlags::SortChildren, TopPropertyWindow->HasFlags(EPropertyWindowFlags::Sorted));
		SetNodeFlags(EPropertyNodeFlags::ShowCategories, TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowCategories));
	}

	//Custom code run prior to setting property flags
	//needs to happen after the above SetNodeFlags calls so that ObjectPropertyNode can properly respond to CollapseCategories
	InitBeforeNodeFlags();

	if ( !Property )
	{
		// Disable all flags if no property is bound.
		SetNodeFlags(EPropertyNodeFlags::SingleSelectOnly | EPropertyNodeFlags::EditInline | EPropertyNodeFlags::EditInlineUse, FALSE);
	}
	else
	{
		TArray<BYTE*> ReadAddresses;
		const UBOOL GotReadAddresses = GetReadAddress( this, FALSE, ReadAddresses, FALSE );
		const UBOOL bSingleSelectOnly = GetReadAddress( this, TRUE, ReadAddresses );
		SetNodeFlags(EPropertyNodeFlags::SingleSelectOnly, bSingleSelectOnly);

		const UBOOL bIsObjectOrInterface = Cast<UObjectProperty>(Property) || Cast<UInterfaceProperty>(Property);

		// TRUE if the property can be expanded into the property window; that is, instead of seeing
		// a pointer to the object, you see the object's properties.
		const UBOOL bEditInline = ( (Property->PropertyFlags&CPF_EditInline) && bIsObjectOrInterface && GotReadAddresses );
		SetNodeFlags(EPropertyNodeFlags::EditInline, bEditInline);

		// TRUE if the property is EditInline with a use button.
		const UBOOL bEditInlineUse = ( (Property->PropertyFlags&CPF_EditInlineUse) && bIsObjectOrInterface && GotReadAddresses );
		SetNodeFlags(EPropertyNodeFlags::EditInlineUse, bEditInlineUse);

		//Get the property max child depth
		if (Property->HasMetaData(TEXT("MaxPropertyDepth")))
		{
			INT NewMaxChildDepthAllowed = Property->GetINTMetaData(TEXT("MaxPropertyDepth"));
			//Ensure new depth is valid.  Otherwise just let the parent specified value stand
			if (NewMaxChildDepthAllowed > 0)
			{
				//if there is already a limit on the depth allowed, take the minimum of the allowable depths
				if (MaxChildDepthAllowed >= 0)
				{
					MaxChildDepthAllowed = Min(MaxChildDepthAllowed, NewMaxChildDepthAllowed);
				}
				else
				{
					//no current limit, go ahead and take the new limit
					MaxChildDepthAllowed = NewMaxChildDepthAllowed;
				}
			}
		}

		//If this node was added by means of ShouldShowNonEditable, and isn't an array internal (they can't specify CPF_Edit)
		if (!(Property->PropertyFlags & CPF_Edit) && (ArrayIndex==-1))
		{
			SetNodeFlags(EPropertyNodeFlags::InitChildrenOnDemand, TRUE);
		}

		if (Property && Property->HasMetaData(TEXT("ScriptOrder")))
		{
			UBOOL bForceScriptOrder = Property->GetBoolMetaData(TEXT("ScriptOrder"));
			SetNodeFlags(EPropertyNodeFlags::IsForceScriptOrder, bForceScriptOrder);
		}
	}

	InitExpansionFlags();

	//offers the UN-CREATED window to do some startup work (create display names for proper sorting etc)
	InWxWindow->InitBeforeCreate();

	if (bInAllowChildren)
	{
		RebuildChildren();
	}
}

/**
 * Used for rebuilding a sub portion of the tree
 */
void FPropertyNode::RebuildChildren (void)
{
	WxPropertyControl* OldWindow = NodeWindow;
	UBOOL bDestroySelf = FALSE;
	DestroyTree(bDestroySelf);

	//if this is an init on demand node, that hasn't been expanded yet, abort!
	if (HasNodeFlags(EPropertyNodeFlags::InitChildrenOnDemand) && !HasNodeFlags(EPropertyNodeFlags::HasEverBeenExpanded))
	{
		return;
	}

	if (MaxChildDepthAllowed != 0)
	{
		NodeWindow->InitWindowDefinedChildNodes();
		//the case where we don't want init child nodes is when an Item has children that we don't want to display
		//the other option would be to make each node "Read only" under that item.
		//The example is a material assigned to a static mesh.
		if (HasNodeFlags(EPropertyNodeFlags::CanBeExpanded) && (ChildNodes.Num() == 0))
		{
			InitChildNodes();
		}
	}

	SortChildren();

	//see if they support some kind of edit condition
	if (Property && Property->GetBoolMetaData(TEXT("FullyExpand")))
	{
		UBOOL bExpand = TRUE;
		UBOOL bRecurse = TRUE;
		SetExpanded(bExpand, bRecurse);
	}
}
/**
 * Used to forceably recreate child controls (text ctrl, etc)
 */
void FPropertyNode::InvalidateChildControls ()
{
	if (HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated))
	{
		check(NodeWindow->InputProxy);
		NodeWindow->InputProxy->DeleteControls();
		SetNodeFlags(EPropertyNodeFlags::ChildControlsCreated, FALSE);
		FastRefreshEntireTree();
	}
}


// Follows the chain of items upwards until it finds the object window that houses this item.
FObjectPropertyNode* FPropertyNode::FindObjectItemParent()
{
	FPropertyNode* Cur = this;
	FObjectPropertyNode* Found;

	while ( true )
	{
		Found = Cur->GetObjectNode();
		if ( Found )
		{
			break;
		}
		Cur = Cur->GetParentNode();
	}

	return Found;
}

/**
 * Follows the top-most object window that contains this property window item.
 */
FObjectPropertyNode* FPropertyNode::FindRootObjectItemParent()
{
	// not every type of change to property values triggers a proper refresh of the hierarchy, so find the topmost container window and trigger a refresh manually.
	FObjectPropertyNode* TopmostObjectItem=NULL;

	FObjectPropertyNode* NextObjectItem = FindObjectItemParent();
	while ( NextObjectItem != NULL )
	{
		TopmostObjectItem = NextObjectItem;
		FPropertyNode* NextObjectParent = NextObjectItem->GetParentNode();
		if ( NextObjectParent != NULL )
		{
			NextObjectItem = NextObjectParent->FindObjectItemParent();
		}
		else
		{
			break;
		}
	}

	return TopmostObjectItem;
}

/**
 * Quickly ensure all the correct windows are visible
 */
void FPropertyNode::FastRefreshEntireTree(void)
{
	//jumps to wrapper window
	check(TopPropertyWindow);
	TopPropertyWindow->RefreshEntireTree();
	if (TopPropertyWindow->HasFlags(EPropertyWindowFlags::Favorites))
	{
		WxPropertyWindowHost* HostWindow = TopPropertyWindow->GetParentHostWindow();
		if (HostWindow)
		{
			HostWindow->AdjustFavoritesSplitter();
		}
	}
}

/**
 * When a window finds it is not visible, it informs all child windows recursively that they should also not be visible
 */
void FPropertyNode::HideSelfAndAllChildren (void)
{
	if (NodeWindow != NULL)
	{
		NodeWindow->Hide();
		// This node is being destroyed, so clear references to it in the property window.
		if ( TopPropertyWindow->GetLastFocused() == NodeWindow )
		{
			TopPropertyWindow->ClearLastFocused();
		}
	}

	for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
	{
		FPropertyNode* ChildNode = ChildNodes(scan);
		check(ChildNode);
		ChildNode->HideSelfAndAllChildren();
	}
}


	/** 
	 * Used to see if any data has been destroyed from under the property tree.  Should only be called by PropertyWindow::OnIdle
	 */
	UBOOL FPropertyNode::IsAllDataValid(void)
	{
		//Figure out if an array mismatch can be ignored
		UBOOL bIgnoreAllMismatch = HasNodeFlags(EPropertyNodeFlags::InitChildrenOnDemand) && !HasNodeFlags(EPropertyNodeFlags::HasEverBeenExpanded);
		//make sure that force depth-limited trees don't cause a refresh
		bIgnoreAllMismatch |= (MaxChildDepthAllowed==0);

		//check my property
		if (Property)
		{
			//verify that the number of array children is correct
			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
			//default to unknown array length
			INT NumArrayChildren = -1;
			//assume all arrays have the same length
			UBOOL bArraysHaveEqualNum = TRUE;
			//assume all arrays match the number of property window children
			UBOOL bArraysMatchChildNum = TRUE;

			//verify that the number of object children are the same too
			UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);
			//check to see, if this an object property, whether the contents are NULL or not.
			//This is the check to see if an object property was changed from NULL to non-NULL, or vice versa, from non-property window code.
			UBOOL bObjectPropertyNull = TRUE;

			//Edit inline properties can change underneath the window
			UBOOL bIgnoreChangingChildren = !HasNodeFlags(EPropertyNodeFlags::EditInline);
			//ignore this node if the consistency check should happen for the children
			UBOOL bIgnoreStaticArray = (Property->ArrayDim > 1) && (ArrayIndex == -1);

			//if this node can't possibly have children (or causes a circular reference loop) then ignore this as a object property
			if (bIgnoreChangingChildren || bIgnoreStaticArray || HasNodeFlags(EPropertyNodeFlags::NoChildrenDueToCircularReference))
			{
				//this will bypass object property consistency checks
				ObjectProperty = NULL;
			}

			TArray<BYTE*> ReadAddresses;
			const UBOOL bSuccess = GetReadAddress( this, ReadAddresses );
			//make sure we got the addresses correctly
			if (!bSuccess)
			{
				return FALSE;
			}


			//check for null, if we find one, there is a problem.
			for (int scan = 0; scan < ReadAddresses.Num(); ++scan)
			{
				//make sure the data still exists
				if (ReadAddresses(scan)==NULL)
				{
					//ERROR!!!!!!!!
					return FALSE;
				}

				if( ArrayProperty && !bIgnoreAllMismatch)
				{
					//ensure that array structures have the proper number of children
					FScriptArray* Array = (FScriptArray*)ReadAddresses(scan);
					check(Array);
					INT ArrayNum = Array->Num();
					//if first child
					if (NumArrayChildren == -1)
					{
						NumArrayChildren = ArrayNum;
					}
					//make sure multiple arrays match
					bArraysHaveEqualNum = bArraysHaveEqualNum && (NumArrayChildren == ArrayNum);
					//make sure the array matches the number of property node children
					bArraysMatchChildNum = bArraysMatchChildNum && (GetNumChildNodes() == ArrayNum);
				}

				if (ObjectProperty && !bIgnoreAllMismatch)
				{

					// Expand struct.
					for (INT i = 0; i < ReadAddresses.Num(); ++i)
					{
						//make sure 
						UObject* obj = *(UObject**)ReadAddresses(i);
						if (obj != NULL)
						{
							bObjectPropertyNull = FALSE;
							break;
						}
					}
				}
			}

			//if all arrays match each other but they do NOT match the property structure, cause a rebuild
			if (bArraysHaveEqualNum && !bArraysMatchChildNum)
			{
				return FALSE;
			}

			const UBOOL bHasChildren = (GetNumChildNodes() != 0);
			if (ObjectProperty && ((!bObjectPropertyNull && !bHasChildren) || (bObjectPropertyNull && bHasChildren)))
			{
				return FALSE;
			}

			// If real time flag, refresh it
			if (HasNodeFlags(EPropertyNodeFlags::IsRealTime))
			{
				WxPropertyControl* NodeWindow = GetNodeWindow();
				if ( NodeWindow )
				{
					NodeWindow->Refresh();
				}
			}
		}

		//go through my children
		for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
		{
			FPropertyNode* ChildNode = ChildNodes(scan);
			check(ChildNode);
			if (ChildNode->HasNodeFlags(EPropertyNodeFlags::IsSeen))
			{
				UBOOL bChildDataValid = ChildNode->IsAllDataValid();
				if (!bChildDataValid)
				{
					return FALSE;
				}
			}
		}

		return TRUE;
	}

/**
 * Sets the flags used by the window and the root node
 * @param InFlags - flags to turn on or off
 * @param InOnOff - whether to toggle the bits on or off
 */
void FPropertyNode::SetNodeFlags (const EPropertyNodeFlags::Type InFlags, const UBOOL InOnOff)
{
	if (InOnOff)
	{
		PropertyNodeFlags |= InFlags;
	}
	else
	{
		PropertyNodeFlags &= (~InFlags);
	}
}
/**
 * Gets wrapper window
 */
WxPropertyWindow* FPropertyNode::GetMainWindow(void)
{
	check(TopPropertyWindow);
	return TopPropertyWindow;
}
/**
 * Gets wrapper window
 */
const WxPropertyWindow* FPropertyNode::GetMainWindow(void) const
{
	check(TopPropertyWindow);
	return TopPropertyWindow;
}
/**
 * Mark this node as needing to be expanded/collapsed (and WILL refresh the windows system)
 */
void FPropertyNode::SetExpanded( const UBOOL bInExpanded, const UBOOL bInRecurse, const UBOOL bInExpandParents )
{
	//if we can't see the arrows, do not allow the state to be toggled
	check(TopPropertyWindow);
	if (!TopPropertyWindow->IsNormalExpansionAllowed(this))
	{
		return;
	}
	SetExpandedInternal(bInExpanded, bInRecurse, bInExpandParents);
	FastRefreshEntireTree();

	GCallbackEvent->Send( CALLBACK_PropertySelectionChange );
}

FPropertyNode* FPropertyNode::FindPropertyNode(const FString& InPropertyName)
{
	// If the name of the property stored here matches the input name . . .
	if( Property && InPropertyName == Property->GetName() )
	{
		return this;
	}
	else
	{
		FPropertyNode* PropertyNode;
		// Extend the search down into children.
		for(INT i=0; i<ChildNodes.Num(); i++)
		{
			PropertyNode = ChildNodes(i)->FindPropertyNode(InPropertyName);
			if( PropertyNode )
			{
				return PropertyNode;
			}
		}
	}

	// Return NULL if not found...
	return NULL;
}

/**
 * Recursively searches through children for a property named PropertyName and expands it.
 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
 */
void FPropertyNode::SetExpandedItem( const FString& PropertyName, INT ArrayIndex, const UBOOL bInExpanded, const UBOOL bInExpandParents )
{
	//if we can't see the arrows, do not allow the state to be toggled
	check(TopPropertyWindow);
	if (!TopPropertyWindow->IsNormalExpansionAllowed(this))
	{
		return;
	}
	SetExpandedItemInternal(PropertyName, ArrayIndex, bInExpanded, bInExpandParents );
	FastRefreshEntireTree();
}

/**
 * Saving and restoring functions for expanding sub windows by string name
 */
void FPropertyNode::RememberExpandedItems(TArray<FString>& InExpandedItems)
{
	UBOOL bExpanded = HasNodeFlags(EPropertyNodeFlags::Expanded);
	UBOOL bPassThroughForFavorites = HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFavorite);
	if( bExpanded || bPassThroughForFavorites )
	{
		//save the children first
		for( INT x = 0 ; x < ChildNodes.Num() ; ++x )
		{
			ChildNodes(x)->RememberExpandedItems( InExpandedItems );
		}

		if (bExpanded)
		{
			//don't save the root, it gets expanded by default
			if (GetParentNode())
			{
				const UBOOL bWithArrayIndex = TRUE;
				FString Path;
				Path.Empty(128);
				GetQualifedName(Path, bWithArrayIndex);

				//make sure my path isn't already included (if A, A.B, and A.B.C are all open, then only A.B.C will be recorded since A and A.B are implicit
				for (INT i = 0; i < InExpandedItems.Num(); ++i)
				{
					if (InExpandedItems(i).InStr(Path) != INDEX_NONE)
					{
						//redundant, do not add a new path
						return;
					}
				}
				new( InExpandedItems )FString( Path );
			}
		}
	}
}

void FPropertyNode::RestoreExpandedItems(const TArray<FString>& InExpandedItems)
{
	// Expand this property window if the current item's name exists in the list of expanded items.
	const UBOOL bWithArrayIndex = TRUE;

	FString Path;
	Path.Empty(128);
	GetQualifedName(Path, bWithArrayIndex);
	for (INT i = 0; i < InExpandedItems.Num(); ++i)
	{
		if (InExpandedItems(i).InStr(Path) != INDEX_NONE)
		{
			SetNodeFlags(EPropertyNodeFlags::Expanded, TRUE);

			for( INT x = 0 ; x < ChildNodes.Num() ; ++x )
			{
				ChildNodes(x)->RestoreExpandedItems( InExpandedItems );
			}
			//NO MORE Searching, we found it!
			break;
		}
	}
}

/**Index of the property defined by script*/
INT FPropertyNode::GetPropertyScriptOrder(void)
{
	INT ScriptOrder = -1;

	if (Property && Property->HasMetaData(TEXT("OrderIndex")))
	{
		ScriptOrder = Property->GetINTMetaData(TEXT("OrderIndex"));
	}

	return ScriptOrder;
}

/*
 * Returns the current list of Filter sub-strings that must match to be displayed
 */const TArray<FString>& FPropertyNode::GetFilterStrings(void) const
{
	check(TopPropertyWindow);
	return TopPropertyWindow->GetFilterStrings(); 
}

/** @return whether this window's property is constant (can't be edited by the user) */
UBOOL FPropertyNode::IsEditConst() const
{
	return HasNodeFlags(EPropertyNodeFlags::IsPropertyEditConst);
}

/**
 * Interface function for adding visible items (doesn't accept categories or the root)
 */
void FPropertyNode::AppendVisibleWindows (OUT TArray<WxPropertyControl*>& OutVisibleArray)
{
	if (HasNodeFlags(EPropertyNodeFlags::IsSeen | EPropertyNodeFlags::IsSeenDueToChildFavorite))
	{
		if (HasNodeFlags(EPropertyNodeFlags::IsSeen) && ( HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated) || HasNodeFlags(EPropertyNodeFlags::IsFavorite)))
		{
			AddSelfToVisibleArray(OutVisibleArray);
		}

		for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
		{
			FPropertyNode* IterNode = ChildNodes(scan);
			check(IterNode);
			IterNode->AppendVisibleWindows(OUT OutVisibleArray);
		}
	}
}

/**
 * Serializes the input and draw proxies, as well as all child windows.
 *
 * @param		Ar		The archive to read/write.
 */
void FPropertyNode::Serialize( FArchive& Ar )
{
	if (NodeWindow)
	{
		// Serialize input and draw proxies.
		Ar << NodeWindow->InputProxy << NodeWindow->DrawProxy;

		// Recursively serialize children
		for( INT x = 0 ; x < ChildNodes.Num() ; ++x )
		{
			ChildNodes(x)->Serialize( Ar );
		}
	}
}
/**
 * Appends my path, including an array index (where appropriate)
 */
void FPropertyNode::GetQualifedName( FString& PathPlusIndex, const UBOOL bWithArrayIndex ) const
{
	if( ParentNode )
	{
		ParentNode->GetQualifedName(PathPlusIndex, bWithArrayIndex);
		PathPlusIndex += TEXT(".");
	}

	if( Property )
	{
		Property->AppendName(PathPlusIndex);
	}

	if ( bWithArrayIndex && (ArrayIndex != INDEX_NONE) )
	{
		PathPlusIndex += TEXT("[");
		appItoaAppend(ArrayIndex,PathPlusIndex);
		PathPlusIndex += TEXT("]");
	}
}


UBOOL FPropertyNode::GetReadAddress(FPropertyNode* InNode,
											UBOOL InRequiresSingleSelection,
											TArray<BYTE*>& OutAddresses,
											UBOOL bComparePropertyContents,
											UBOOL bObjectForceCompare,
											UBOOL bArrayPropertiesCanDifferInSize)
{
	// The default behaviour is to defer to the parent node, if one exists.
	if (ParentNode)
	{
		return ParentNode->GetReadAddress( InNode, InRequiresSingleSelection, OutAddresses, bComparePropertyContents, bObjectForceCompare, bArrayPropertiesCanDifferInSize );
	}
	return FALSE;
}

/**
 * fills in the OutAddresses array with the addresses of all of the available objects.
 * @param InItem		The property to get objects from.
 * @param OutAddresses	Storage array for all of the objects' addresses.
 */
UBOOL FPropertyNode::GetReadAddress(FPropertyNode* InNode,
											TArray<BYTE*>& OutAddresses)
{
	if (ParentNode)
	{
		return ParentNode->GetReadAddress( InNode, OutAddresses );
	}
	return FALSE;
}


/**
 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
 */
BYTE* FPropertyNode::GetValueBaseAddress( BYTE* StartAddress )
{
	BYTE* Result = NULL;

	if ( ParentNode != NULL )
	{
		Result = ParentNode->GetValueAddress(StartAddress);
	}
	return Result;
}

/**
 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
 * to dynamic array elements, the pointer returned will be the location for that element's data. 
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
 */
BYTE* FPropertyNode::GetValueAddress( BYTE* StartAddress )
{
	return GetValueBaseAddress( StartAddress );
}

/**
 * Creates the appropriate editing control for the property specified.
 *
 * @param	InProperty		the property that will use the new property item
 * @param	InArrayIndex	specifies which element of an array property that this property window will represent.  Only valid
 *							when creating property window items for individual elements of an array.
 */
WxItemPropertyControl* FPropertyNode::CreatePropertyItem( UProperty* InProperty, INT InArrayIndex/*=INDEX_NONE*/)
{
	check(InProperty);

	WxItemPropertyControl* Result = NULL;
	if ( TopPropertyWindow->HasFlags(EPropertyWindowFlags::SupportsCustomControls))
	{
		UCustomPropertyItemBindings* Bindings = UCustomPropertyItemBindings::StaticClass()->GetDefaultObject<UCustomPropertyItemBindings>();
		check(Bindings);

		// if we support custom controls, see if this property has a custom editing control class setup
		Result = Bindings->GetCustomPropertyWindow(InProperty,InArrayIndex);
	}

	if ( Result == NULL )
	{
		UBOOL bStaticArray = (InProperty->ArrayDim > 1) && (InArrayIndex == INDEX_NONE);
		//see if they support some kind of edit condition and this isn't the "parent" property of a static array
		if (InProperty->HasMetaData(TEXT("EditCondition")) && !bStaticArray) 
		{
			Result = new WxCustomPropertyItem_ConditionalItem;
		}
		else
		{
			// don't support custom properties or couldn't load the custom editing control class, so just create a normal item
			Result = new WxItemPropertyControl;
		}
	}

	return Result;
}

/**
 * Detects if the window is actually visible in the property panel and not clipped
 */
UBOOL FPropertyNode::IsPartiallyUnclipped (void)
{
	//make sure the window has at least been initialized
	if (HasNodeFlags(EPropertyNodeFlags::IsWindowReallyCreated))
	{
		//Make sure the window is at least showing
		check(TopPropertyWindow);
		check(NodeWindow);
		const wxRect MainRect = TopPropertyWindow->GetScreenRect();
		const wxRect NodeWindowRect = NodeWindow->GetScreenRect();
		if (MainRect.Intersects(NodeWindowRect))
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Destroys all node within the hierarchy
 */
wxRect FPropertyNode::GetClippedClientRect (void)
{
	const wxRect MainRect = TopPropertyWindow->GetScreenRect();
	const wxRect NodeWindowRect = NodeWindow->GetScreenRect();

	INT OffsetX = NodeWindowRect.GetX();
	INT OffsetY = NodeWindowRect.GetY();

	INT NewLeft   = Max(NodeWindowRect.GetX(),		MainRect.GetX());
	INT NewRight  = Min(NodeWindowRect.GetRight(),	MainRect.GetRight());
	INT NewTop    = Max(NodeWindowRect.GetY(),		MainRect.GetY());
	INT NewBottom = Min(NodeWindowRect.GetBottom(),	MainRect.GetBottom());

	//Add 1 to get the height or width
	//putting the offset in there to move it back to client space
	return wxRect(NewLeft-OffsetX, NewTop-OffsetY, NewRight - NewLeft+1, NewBottom-NewTop+1);
}

/**
 * If there is a property, sees if it matches.  Otherwise sees if the entire parent structure matches
 */
UBOOL FPropertyNode::GetDiffersFromDefault (void)
{
	FObjectPropertyNode* ObjectNode = FindObjectItemParent();
	if (Property)
	{
		WxItemPropertyControl* ItemWindow = wxDynamicCast (NodeWindow, WxItemPropertyControl);
		check(ItemWindow);
		// Get an iterator for the enclosing objects.
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			if ( ItemWindow->DiffersFromDefault( *Itor, Property ) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**Walks up the hierachy and return TRUE if any parent node is a favorite*/
UBOOL FPropertyNode::IsChildOfFavorite (void) const
{
	for (const FPropertyNode* TestParentNode = GetParentNode(); TestParentNode != NULL; TestParentNode = TestParentNode->GetParentNode())
	{
		if (TestParentNode->HasNodeFlags(EPropertyNodeFlags::IsFavorite))
		{
			return TRUE;
		}
	}
	return FALSE;
}


/** Adjusts parent window if needed (for favorites)*/
void FPropertyNode::ReparentWindow()
{
	//ensure this window has the proper parent (favorites can change their parent based on if they are top level or not
	if (HasNodeFlags(EPropertyNodeFlags::IsWindowReallyCreated))
	{
		wxWindow* CorrectParent = GetParentNodeWindow();
		if (NodeWindow->GetParent() != CorrectParent)
		{
			//reparent windows that have since changed their parent (favorites)
			NodeWindow->Reparent(CorrectParent);
		}
	}
}

/**
 * Destroys all node within the hierarchy
 */
void FPropertyNode::DestroyTree(const UBOOL bInDestroySelf)
{
	//Copy the old child list
	TArray<FPropertyNode*> OldChildNodes = ChildNodes;
	//reset the child node list in case it had focus.  
	ChildNodes.Empty();

	for (INT scan = 0; scan < OldChildNodes.Num(); ++scan)
	{
		FPropertyNode* OldChildNode = OldChildNodes(scan);
		check(OldChildNode);
		delete OldChildNode;
	}

	if (bInDestroySelf && (NodeWindow != NULL))
	{
		//despite what wx has done behind the curtain, if focus has been given back to a child of this window, GIVE IT AWAY!  You can't keep focus when being destroyed!!
		if (IsChildWindowOf(wxWindow::FindFocus(), NodeWindow))
		{
			TopPropertyWindow->RequestMainWindowTakeFocus();
		}
		// Delete controls before destroying node window
		if (HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated))
		{
			check(NodeWindow->InputProxy);
			NodeWindow->InputProxy->DeleteControls();
			SetNodeFlags(EPropertyNodeFlags::ChildControlsCreated, FALSE);
		}

		NodeWindow->Destroy();
		NodeWindow = NULL;
	}
}

/**
 * Used for window creation, to ensure parenting is correct
 */
wxWindow* FPropertyNode::GetParentNodeWindow(void)
{
	check(TopPropertyWindow);
	if (ParentNode)
	{
		UBOOL bIsFavoritesWindow = TopPropertyWindow->HasFlags(EPropertyWindowFlags::Favorites);
		//all nodes that are visible because they are favorites will not have child favorites set
		UBOOL bIsTopLevelFavorite = ParentNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFavorite);

		//if this is the favorites window, this should be based on the parent nodes window
		//if this is not a top level favorite, this should be based on the parent nodes window
		if (!bIsFavoritesWindow || !bIsTopLevelFavorite)
		{
			return ParentNode->NodeWindow;
		}
	}
	return TopPropertyWindow;	//only should happen for the root
}


/**
 * Shows or Hides windows that have already been created for a particular node OR creates the window if it has never been seen before
 */
void FPropertyNode::ProcessShowVisibleWindows (void)
{
	if (HasNodeFlags(EPropertyNodeFlags::IsSeen))
	{
		check(NodeWindow);	//windows MUST already exist.
		if (!HasNodeFlags(EPropertyNodeFlags::IsWindowReallyCreated))
		{
			//make new window to go in this place
			CreateInternalWindow();
			
			if (Property)
			{
				// Look for localized tooltip
				FString TooltipName = FString::Printf( TEXT("%s.tooltip"), *(Property->GetName()));
				FString LocalizedTooltipName = LocalizeProperty( *(Property->GetFullGroupName(TRUE)), *TooltipName );
				if (LocalizedTooltipName.Len() > 0)
				{
					NodeWindow->SetToolTip(*LocalizedTooltipName);
				}
				// Look for tooltip metadata.
				else if (Property->HasMetaData(TEXT("tooltip")) )
				{
					NodeWindow->SetToolTip(*Property->GetMetaData(TEXT("tooltip")));
				}
			}
			SetNodeFlags(EPropertyNodeFlags::IsWindowReallyCreated, TRUE);
		}
		
		ReparentWindow ();
		NodeWindow->Show();

		//rock through the children
		for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
		{
			FPropertyNode* ChildNode = ChildNodes(scan);
			ChildNode->ProcessShowVisibleWindows();
		}
	} 
	else 
	{
		ReparentWindow ();
		HideSelfAndAllChildren();
	}
}


/**
 * Shows or Hides windows that have already been created for favorites and their children
 */
void FPropertyNode::ProcessShowVisibleWindowsFavorites (void)
{
	if (HasNodeFlags(EPropertyNodeFlags::IsFavorite))
	{
		//send through the standard property path
		ProcessShowVisibleWindows();
	}
	else
	{
		ReparentWindow();
		if (HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFavorite))
		{
			//if a node window ever existed for this node, hide it.  It is just place holder for now.
			if (NodeWindow != NULL)
			{
				NodeWindow->Hide();
			}
			//rock through the children
			for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
			{
				FPropertyNode* ChildNode = ChildNodes(scan);
				ChildNode->ProcessShowVisibleWindowsFavorites();
			}
		}
		else
		{
			HideSelfAndAllChildren();
		}
	}
}
/**
 * Shows or Hides control windows based on focus, all windows needing controls, and window clipping
 */
void FPropertyNode::ProcessControlWindows (void)
{
	UBOOL bNodeHasFocus     = TopPropertyWindow->GetLastFocused() == NodeWindow;
	UBOOL bWindowIsVisible  = HasNodeFlags(EPropertyNodeFlags::IsSeen);
	UBOOL bControlsSuppressed = FALSE;
	if (bWindowIsVisible)
	{
		//recalculate "const" flags
		SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, FALSE);
		if (Property != NULL)
		{
			SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, (Property->PropertyFlags & CPF_EditConst) ? TRUE : FALSE);

			if (!HasNodeFlags(EPropertyNodeFlags::IsPropertyEditConst))
			{
				if (TopPropertyWindow->HasFlags(EPropertyWindowFlags::ReadOnly))
				{
					SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, TRUE);
				}
			}

			if (!HasNodeFlags(EPropertyNodeFlags::IsPropertyEditConst))
			{
				// travel up the chain to see if this property's owner struct is editconst - if it is, so is this property
				FPropertyNode* NextParent = ParentNode;
				while (NextParent != NULL && Cast<UStructProperty>(NextParent->Property, CLASS_IsAUStructProperty) != NULL)
				{
					if (NextParent->HasNodeFlags(EPropertyNodeFlags::IsPropertyEditConst))
					{
						SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, TRUE);
						break;
					}
					NextParent = NextParent->ParentNode;
				}
			}

			if( !HasNodeFlags(EPropertyNodeFlags::IsPropertyEditConst) )
			{
				// Ask the objects whether this property can be changed
				const FObjectPropertyNode* ObjectPropertyNode = const_cast< FPropertyNode* >( this )->FindObjectItemParent();
				for( TPropObjectConstIterator CurObjectIt( ObjectPropertyNode->ObjectConstIterator() ); CurObjectIt; ++CurObjectIt )
				{
					const UObject* CurObject = *CurObjectIt;
					if( CurObject != NULL )
					{
						if( !CurObject->CanEditChange( Property ) )
						{
							// At least one of the objects didn't like the idea of this property being changed.
							SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, TRUE);
							break;
						}
					}
				}
			}
		}

		check(NodeWindow);
		bControlsSuppressed = NodeWindow->IsControlsSuppressed();
	}

	UBOOL bNeedsControls = bWindowIsVisible && (!bControlsSuppressed);
	bNeedsControls = bNeedsControls & IsPartiallyUnclipped();	//make sure at least a LITTLE visible

	if (bNeedsControls)
	{
		//if we haven't created them yet, do so
		if (!HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated))
		{
			SetNodeFlags(EPropertyNodeFlags::ChildControlsCreated, TRUE);
			WxPropertyWindow* MainWindow = NodeWindow->GetPropertyWindow();
			check(MainWindow);
			UBOOL bFocusBelongsUnderMainWindow = IsChildWindowOf(wxWindow::FindFocus(), MainWindow);
			FObjectPropertyNode* ObjectNode = FindObjectItemParent();
			check(ObjectNode);

			const wxRect rc = NodeWindow->GetClientRect();
			const wxRect ControlRect = wxRect( rc.x + MainWindow->GetSplitterPos(), rc.y, rc.width - MainWindow->GetSplitterPos(), NodeWindow->GetPropertyHeight());

			NodeWindow->InputProxy->CreateControls(NodeWindow, ObjectNode->GetObjectBaseClass(), ControlRect, *NodeWindow->GetPropertyText() );
			NodeWindow->DrawProxy->AssociateWithControl(NodeWindow);
			//Only if this window used to have focus AND this window SHOULD actually have REAL focus
			if (bFocusBelongsUnderMainWindow && (NodeWindow == MainWindow->GetLastFocused()))	//got clicked on before the child windows were created
			{
				NodeWindow->InputProxy->OnSetFocus(NodeWindow);
			}
		}
	}

	//if controls exist at all (they've been inited at least once before
	if (HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated))
	{
		for(wxWindowList::compatibility_iterator node = NodeWindow->GetChildren().GetFirst(); node; node=node->GetNext())
		{
			wxWindow* ScanWindow = node->GetData();
			WxPropertyControl* NonControlWindow = wxDynamicCast(ScanWindow, WxPropertyControl);
			if (NonControlWindow == NULL) //unable to upcast to a "tree" widget.  Must be a control widget!
			{
				if (bNeedsControls)
				{
					ScanWindow->Show();
				}
				else
				{
					ScanWindow->Hide();
				}
			}
		}
		// notify the input proxy that we hid/shown children
		NodeWindow->InputProxy->NotifyHideShowChildren(bNeedsControls);
	}

	//rock through the children
	for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
	{
		FPropertyNode* ChildNode = ChildNodes(scan);
		ChildNode->ProcessControlWindows();
	}
}

/**
 * Marks windows as visible based on the filter strings (EVEN IF normally NOT EXPANDED)
 */
void FPropertyNode::FilterWindows(const TArray<FString>& InFilterStrings, const UBOOL bParentSeenDueToFiltering)
{
	//clear flags first.  Default to hidden
	SetNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering | EPropertyNodeFlags::IsSeenDueToChildFiltering | EPropertyNodeFlags::IsParentSeenDueToFiltering, FALSE);

	UBOOL bMultiObjectOnlyShowDiffering = TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShowOnlyDifferingItems) && (TopPropertyWindow->GetNumObjects()>1);
	check(TopPropertyWindow);
	if ((InFilterStrings.Num() > 0) || (TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShowOnlyModifiedItems) || bMultiObjectOnlyShowDiffering))
	{
		//if filtering, default to NOT-seen
		UBOOL bPassedFilter = FALSE;	//assuming that we aren't filtered

		//see if this is a filter-able primitive
		FString DisplayName = NodeWindow->GetDisplayName();
		TArray <FString> AcceptableNames;
		AcceptableNames.AddItem(DisplayName);

		//get the basic name as well of the property
		UProperty* Property = GetProperty();
		if (Property && (Property->GetName() != DisplayName))
		{
			AcceptableNames.AddItem(Property->GetName());
		}

		bPassedFilter = IsFilterAcceptable(AcceptableNames, InFilterStrings);

		if (bPassedFilter)
		{
			SetNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering, TRUE);
		} 
		SetNodeFlags(EPropertyNodeFlags::IsParentSeenDueToFiltering, bParentSeenDueToFiltering);
	}
	else
	{
		//indicating that this node should not be force displayed, but opened normally
		SetNodeFlags(EPropertyNodeFlags::IsParentSeenDueToFiltering, TRUE);
	}

	//default to doing only one pass
	//UBOOL bCategoryOrObject = (GetObjectNode()) || (GetCategoryNode()!=NULL);
	INT StartRecusionPass = HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering) ? 1 : 0;
	//Pass 1, if a pass 1 exists (object or category), is to see if there are any children that pass the filter, if any do, trim the tree to the leaves.
	//	This will stop categories from showing ALL properties if they pass the filter AND a child passes the filter
	//Pass 0, if no child exists that passes the filter OR this node didn't pass the filter
	for (INT RecursionPass = StartRecusionPass; RecursionPass >= 0; --RecursionPass)
	{
		for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
		{
			FPropertyNode* ScanNode = ChildNodes(scan);
			check(ScanNode);
			//default to telling the children this node is NOT visible, therefore if not in the base pass, only filtered nodes will survive the filtering process.
			UBOOL bChildParamParentVisible = FALSE;
			//if we're at the base pass, tell the children the truth about visiblity
			if (RecursionPass == 0)
			{
				bChildParamParentVisible = bParentSeenDueToFiltering || HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering);
			}
			ScanNode->FilterWindows(InFilterStrings, bChildParamParentVisible);

			if (ScanNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering | EPropertyNodeFlags::IsSeenDueToChildFiltering))
			{
				SetNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFiltering, TRUE);
			}
		}
		//now that we've tried a pass at our children, if any of them have been successfully seen due to filtering, just quit now
		if (HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFiltering))
		{
			break;
		}
	}
}

/**
 * Marks windows as visible based on the filter strings or standard visibility
 * @param bParentAllowsVisible - is NOT true for an expander that is NOT expanded
 */
void FPropertyNode::ProcessSeenFlags(const UBOOL bParentAllowsVisible)
{
	//clear flags first.  Default to hidden
	SetNodeFlags(EPropertyNodeFlags::IsSeen | EPropertyNodeFlags::IsSeenDueToChildFavorite, FALSE);

	BOOL bAllowChildrenVisible;
	//if expanded or the root and MUST be expanded because it's the root and we're showing categories.
	if (HasNodeFlags(EPropertyNodeFlags::Expanded) || ((GetObjectNode()) && (TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowCategories))) )
	{
		bAllowChildrenVisible = TRUE;
	}
	else
	{
		//can't show children unless they are seen due to child filtering
		bAllowChildrenVisible = HasNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFiltering);	
	}

	//process children
	for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
	{
		FPropertyNode* ScanNode = ChildNodes(scan);
		check(ScanNode);
		ScanNode->ProcessSeenFlags(bParentAllowsVisible && bAllowChildrenVisible);	//both parent AND myself have to allow children
	}

	check(TopPropertyWindow);
	if (HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering | EPropertyNodeFlags::IsSeenDueToChildFiltering))
	{
		SetNodeFlags(EPropertyNodeFlags::IsSeen, TRUE); 
	}
	else 
	{
		//Finally, apply the REAL IsSeen
		SetNodeFlags(EPropertyNodeFlags::IsSeen, bParentAllowsVisible && HasNodeFlags(EPropertyNodeFlags::IsParentSeenDueToFiltering));
	}

	if (NodeWindow->IsDerivedForcedHide())
	{
		SetNodeFlags(EPropertyNodeFlags::IsSeen | EPropertyNodeFlags::IsSeenDueToFiltering | EPropertyNodeFlags::IsSeenDueToChildFiltering, FALSE);
	}
}

/**
 * Marks windows as visible based their favorites status
 */
void FPropertyNode::ProcessSeenFlagsForFavorites(void)
{
	//clear flags first.  Default to hidden
	SetNodeFlags(EPropertyNodeFlags::IsSeen | EPropertyNodeFlags::IsSeenDueToChildFavorite, FALSE);

	//if this node is a favorite, mark as seen
	if (HasNodeFlags(EPropertyNodeFlags::IsFavorite))
	{
		//treat as a normal node that is now visible
		ProcessSeenFlags(TRUE);
	}
	else
	{
		UBOOL bAnyChildFavorites = FALSE;
		//process children
		for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
		{
			FPropertyNode* ScanNode = ChildNodes(scan);
			check(ScanNode);
			ScanNode->ProcessSeenFlagsForFavorites();
			bAnyChildFavorites = bAnyChildFavorites || ScanNode->HasNodeFlags(EPropertyNodeFlags::IsFavorite | EPropertyNodeFlags::IsSeenDueToChildFavorite);
		}
		if (bAnyChildFavorites)
		{
			SetNodeFlags(EPropertyNodeFlags::IsSeenDueToChildFavorite, TRUE);
		}
	}
}

/**Script Defined Sort*/
IMPLEMENT_COMPARE_POINTER( FPropertyNode, PropertiesScriptOrder,							\
{																							\
	INT APropertyScriptOrder = A->GetPropertyScriptOrder();									\
	APropertyScriptOrder = APropertyScriptOrder == -1 ? MAXINT : APropertyScriptOrder;		\
	INT BPropertyScriptOrder = B->GetPropertyScriptOrder();									\
	BPropertyScriptOrder = BPropertyScriptOrder == -1 ? MAXINT : BPropertyScriptOrder;		\
																							\
	if (APropertyScriptOrder == BPropertyScriptOrder)										\
	{																						\
		WxPropertyControl* AWindow = A->GetNodeWindow();								\
		WxPropertyControl* BWindow = B->GetNodeWindow();								\
		if( wxDynamicCast( AWindow, WxCategoryPropertyControl ) )						\
		{																				\
			return appStricmp( *((WxCategoryPropertyControl*)AWindow)->GetCategoryName().ToString(), *((WxCategoryPropertyControl*)BWindow)->GetCategoryName().ToString() ); \
		}																				\
		else																			\
		{																				\
			return appStricmp( *AWindow->GetDisplayName(), *BWindow->GetDisplayName() );\
		}																				\
		return 0;																			\
	}																						\
	return APropertyScriptOrder < BPropertyScriptOrder ? -1 : 1;							\
} )

/**Alphabet Sort*/
IMPLEMENT_COMPARE_POINTER( FPropertyNode, Properties,			\
{																	\
	WxPropertyControl* AWindow = A->GetNodeWindow();			\
	WxPropertyControl* BWindow = B->GetNodeWindow();			\
	if( wxDynamicCast( AWindow, WxCategoryPropertyControl ) )		\
		return appStricmp( *((WxCategoryPropertyControl*)AWindow)->GetCategoryName().ToString(), *((WxCategoryPropertyControl*)BWindow)->GetCategoryName().ToString() ); \
	else \
		return appStricmp( *AWindow->GetDisplayName(), *BWindow->GetDisplayName() ); \
} )

void FPropertyNode::SortChildren()
{
	check(TopPropertyWindow);
	if(HasNodeFlags(EPropertyNodeFlags::SortChildren))
	{
		if (GPropertyWindowManager->GetUseScriptDefinedOrder() || HasNodeFlags(EPropertyNodeFlags::IsForceScriptOrder))
		{
			Sort<USE_COMPARE_POINTER(FPropertyNode,PropertiesScriptOrder)>( &ChildNodes(0), ChildNodes.Num() );
		}
		else
		{
			Sort<USE_COMPARE_POINTER(FPropertyNode,Properties)>( &ChildNodes(0), ChildNodes.Num() );
		}
	}
}

/**
 * Does the string compares to ensure this Name is acceptable to the filter that is passed in
 * @return		Return True if this property should be displayed.  False if it should be culled
 */
UBOOL FPropertyNode::IsFilterAcceptable(const TArray<FString>& InAcceptableNames, const TArray<FString>& InFilterStrings)
{
	check(TopPropertyWindow);
	if (TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShowOnlyModifiedItems))
	{
		UBOOL Differs = GetDiffersFromDefault();
		if (!Differs)
		{
			return FALSE;		//reject things that do not differ
		}
	}
	if (TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShowOnlyDifferingItems) && TopPropertyWindow->GetNumObjects() > 1)
	{
		TArray<BYTE*> Addresses;
		UBOOL bAllSame = GetReadAddress( this, HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses);
		UBOOL bHasValidDifferableData = (Addresses.Num() > 0);  //only true for item property nodes
		if (bAllSame || !bHasValidDifferableData)
		{
			return FALSE;		//reject things that do not differ
		}
	}
	UBOOL bCompleteMatchFound = TRUE;
	if (InFilterStrings.Num())
	{
		//we have to make sure one name matches all criteria
		for (INT TestNameIndex = 0; TestNameIndex < InAcceptableNames.Num(); ++TestNameIndex)
		{
			bCompleteMatchFound = TRUE;

			FString TestName = InAcceptableNames(TestNameIndex);
			for (INT scan = 0; scan < InFilterStrings.Num(); scan++)
			{
				UBOOL bStartFromEnd = FALSE;
				UBOOL bIgnoreCase   = TRUE;
				if (TestName.InStr(InFilterStrings(scan), bStartFromEnd, bIgnoreCase) == INDEX_NONE) 
				{
					bCompleteMatchFound = FALSE;
					break;
				}
			}
			if (bCompleteMatchFound)
			{
				break;
			}
		}
	}
	return bCompleteMatchFound;
}

/**
 * Expand all parent nodes of this property
 */
void FPropertyNode::ExpandParent( UBOOL bInRecursive )
{
	FPropertyNode* ParentNode = GetParentNode();
	if( ParentNode )
	{
		ParentNode->SetExpandedInternal( TRUE, FALSE, FALSE );
		if( bInRecursive )
		{
			ParentNode->ExpandParent( TRUE );
		}
	}
}

/**
 * Mark this node as needing to be expanded/collapsed called from the wrapper function SetExpanded
 */
void FPropertyNode::SetExpandedInternal( const UBOOL bInExpanded, const UBOOL bInRecurse, const UBOOL bInExpandParents )
{
	SetNodeFlags(EPropertyNodeFlags::Expanded, bInExpanded);

	if( bInExpanded && bInExpandParents )
	{
		ExpandParent( TRUE );
	}

	//if collapsing and focus was a child of this collapse.  Give focus back to the window
	if (!bInExpanded)
	{
		WxPropertyControl* NodeWindow = GetNodeWindow();
		if (IsChildWindowOf(NodeWindow->FindFocus(), NodeWindow))
		{
			WxPropertyWindow* MainWindow = GetMainWindow();
			check(MainWindow);
			MainWindow->RequestMainWindowTakeFocus();
		}
	}
	else
	{
		UBOOL bHadBeenPreviouslyExpanded = HasNodeFlags(EPropertyNodeFlags::HasEverBeenExpanded);
		//mark that it was expanded at some point
		SetNodeFlags(EPropertyNodeFlags::HasEverBeenExpanded, TRUE);
		//Test if this is an "InitChildrenOnDemand" node that hasn't been inited yet
		if (HasNodeFlags(EPropertyNodeFlags::InitChildrenOnDemand) && !bHadBeenPreviouslyExpanded)
		{
			WxPropertyWindow* MainWindow = GetMainWindow();
			check(MainWindow);
			MainWindow->RebuildSubTree(this);
		}

	}

	if (bInRecurse)
	{
		for (INT scan = 0; scan < ChildNodes.Num(); ++scan)
		{
			FPropertyNode* ScanNode = ChildNodes(scan);
			check(ScanNode);
			ScanNode->SetExpandedInternal(bInExpanded, bInRecurse, FALSE);
		}
	}
}
/**
 * Recursively searches through children for a property named PropertyName and expands it.
 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
 */
void FPropertyNode::SetExpandedItemInternal (const FString& InPropertyName, INT InArrayIndex, const UBOOL bInExpanded, const UBOOL bInExpandParents)
{
	// If the name of the property stored here matches the input name . . .
	if( Property && InPropertyName == Property->GetName() )
	{
		UBOOL bRecurse = FALSE;
		SetExpandedInternal(bInExpanded, bRecurse, bInExpandParents);

		// If the property is an array, expand the InArrayIndex'th item.
		if( Property->IsA(UArrayProperty::StaticClass()) && 
			InArrayIndex != INDEX_NONE && 
			InArrayIndex >= 0 && InArrayIndex < ChildNodes.Num() )
		{
			ChildNodes(InArrayIndex)->SetExpandedInternal(bInExpanded, bRecurse, bInExpandParents);
		}
	}
	else
	{
		// Extend the search down into children.
		for(INT i=0; i<ChildNodes.Num(); i++)
		{
			ChildNodes(i)->SetExpandedItemInternal( InPropertyName, InArrayIndex, bInExpanded, bInExpandParents );
		}
	}
}
/*-----------------------------------------------------------------------------
	WxPropertyControl
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyControl,wxWindow);

BEGIN_EVENT_TABLE( WxPropertyControl, wxWindow )
	EVT_LEFT_DOWN( WxPropertyControl::OnLeftClick )
	EVT_LEFT_UP( WxPropertyControl::OnLeftUnclick )
	EVT_LEFT_DCLICK( WxPropertyControl::OnLeftDoubleClick )
	EVT_RIGHT_DOWN( WxPropertyControl::OnRightClick )
	EVT_SET_FOCUS( WxPropertyControl::OnChangeFocus )
	EVT_KILL_FOCUS( WxPropertyControl::OnChangeFocus )
	EVT_COMMAND_RANGE( ID_PROP_CLASSBASE_START, ID_PROP_CLASSBASE_END, wxEVT_COMMAND_MENU_SELECTED, WxPropertyControl::OnEditInlineNewClass )
	EVT_BUTTON( ID_PROP_PB_ADD, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_EMPTY, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_INSERT, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_DELETE, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_BROWSE, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_CLEAR, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_FIND, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_USE, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_NEWOBJECT, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_DUPLICATE, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_TEXTBOX, WxPropertyControl::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_CURVEEDIT, WxPropertyControl::OnPropItemButton )
	EVT_MOTION( WxPropertyControl::OnMouseMove )
	EVT_CHAR( WxPropertyControl::OnChar )
	EVT_MENU(ID_PROP_RESET_TO_DEFAULT, WxPropertyControl::OnContext_ResetToDefaults)
END_EVENT_TABLE()

void WxPropertyControl::Create(wxWindow* InParent)
{
	const bool bWasCreationSuccessful = wxWindow::Create( InParent, -1, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS );
	check( bWasCreationSuccessful );

	//PropertyClass = NULL;

	RegisterCreation();
}

/**
 * Something has caused data to be removed from under the property window.  Request a full investigation by the main property window
 */
void WxPropertyControl::RequestDataValidation(void)
{
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->RequestReconnectToData();
}


//Accessor functions to get to the PropertyNode's property member
UProperty* WxPropertyControl::GetProperty(void)
{
	FPropertyNode* PropertyNode = GetPropertyNode();
	check(PropertyNode);
	UProperty* NodeProperty = PropertyNode->GetProperty();
	return NodeProperty;
}
const UProperty* WxPropertyControl::GetProperty(void) const
{
	const FPropertyNode* PropertyNode = GetPropertyNode();
	check(PropertyNode);
	const UProperty* NodeProperty = PropertyNode->GetProperty();
	return NodeProperty;
}

//Detailed Access functions that go to the node flags
UINT WxPropertyControl::HasNodeFlags (const EPropertyNodeFlags::Type InTestFlags) const
{
	check (PropertyNode);
	return PropertyNode->HasNodeFlags(InTestFlags);
}

/**
 *	Allows custom hiding of controls if conditions aren't met for conditional items
 */
UBOOL WxPropertyControl::IsControlsSuppressed (void)
{
	check( PropertyNode != NULL );
	return PropertyNode->IsEditConst();
}

void WxPropertyControl::FinishSetup()
{
	// Find and construct proxies for this property

	DrawProxy = NULL;
	InputProxy = NULL;

	UClass* DrawProxyClass = GetDrawProxyClass();
	UClass* InputProxyClass = GetInputProxyClass();

	// If we couldn't find a specialized proxies, use the base proxies.
	DrawProxy	= ConstructObject<UPropertyDrawProxy>( DrawProxyClass );
	InputProxy	= ConstructObject<UPropertyInputProxy>( InputProxyClass );

	SetWindowStyleFlag( wxCLIP_CHILDREN|wxCLIP_SIBLINGS|GetWindowStyleFlag() );

}

WxPropertyControl::~WxPropertyControl()
{
}

/**
 *  @return	The height of the property item, as determined by the input and draw proxies.
 */
INT WxPropertyControl::GetPropertyHeight()
{
	INT ItemHeight = PROP_DefaultItemHeight;
	WxPropertyWindow* MainWindow = PropertyNode->GetMainWindow();
	check(MainWindow);
	const UBOOL bIsFocused = MainWindow->GetLastFocused() == this;

	// Return the height of the input proxy if we are focused, otherwise return the height of the draw proxy.
	if(bIsFocused == TRUE && InputProxy != NULL)
	{
		ItemHeight = InputProxy->GetProxyHeight();
	}
	else
	{
		ItemHeight = DrawProxy->GetProxyHeight();
	}

	return ItemHeight;
}

// Returns a string representation of the contents of the property.
FString WxPropertyControl::GetPropertyText()
{
	check(PropertyNode);

	FString	Result;

	TArray<BYTE*> ReadAddresses;
	const UBOOL bSuccess = PropertyNode->GetReadAddress( PropertyNode, PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), ReadAddresses );
	if ( bSuccess )
	{
		UProperty* Property = GetProperty();
		// Export the first value into the result string.
		BYTE* Data = ReadAddresses(0);
		if (!Data)
		{
			//Undo redo can cause this.
			WxPropertyWindow* MainWindow = GetPropertyWindow();
			check(MainWindow);
			MainWindow->RequestReconnectToData();
			return FString();
		}
		Property->ExportText( 0, Result, Data - Property->Offset, Data - Property->Offset, NULL, PPF_Localized|PPF_PropertyWindow );
#if 0
		// If there are multiple objects selected, compare subsequent values against the first.
		if ( ReadAddresses.Num() > 1 )
		{
			FString CurResult;
			for ( INT i = 0 ; i < ReadAddresses.Num() ; ++i )
			{
				BYTE* Data = ReadAddresses(i);
				Property->ExportText( 0, CurResult, Data - Property->Offset, Data - Property->Offset, NULL, PPF_Localized );
				if ( CurResult != Result )
				{
					return FString();
				}
			}
		}
#endif
	}

	return Result;
}

// Returns the property window this node belongs to.  The returned pointer is always valid.
WxPropertyWindow* WxPropertyControl::GetPropertyWindow()
{
	return PropertyNode->GetMainWindow();
}
const WxPropertyWindow* WxPropertyControl::GetPropertyWindow(void)const
{
	return PropertyNode->GetMainWindow();
}

/**
 * Sets the target property value for property-based coloration to the value of
 * this property window node's value.
 */
void WxPropertyControl::SetPropertyColorationTarget()
{
	const FString Value( GetPropertyText() );
	if ( Value.Len() )
	{
		UClass* CommonBaseClass = GetPropertyWindow()->GetRoot()->GetObjectBaseClass();
		FEditPropertyChain* PropertyChain = WxPropertyWindow::BuildPropertyChain( this, GetProperty() );
		GEditor->SetPropertyColorationTarget( Value, GetProperty(), CommonBaseClass, PropertyChain );
	}
}

void WxPropertyControl::OnLeftClick( wxMouseEvent& In )
{
	UBOOL bShouldSetFocus = TRUE;
	if( IsOnSplitter( In.GetX() ) )
	{
		// Set up to draw the window splitter
		GetPropertyWindow()->StartDraggingSplitter();
		LastX = In.GetX();
		CaptureMouse();
		bShouldSetFocus = FALSE;
	}
	else
	{
		if ( In.ShiftDown() )
		{
			// Shift-clicking on a property sets the target property value for property-based coloration.
			SetPropertyColorationTarget();
			bShouldSetFocus = FALSE;
		}
		else if( !InputProxy->LeftClick( this, In.GetX(), In.GetY() ) )
		{
			bShouldSetFocus = ClickedPropertyItem(In);
			if (bShouldSetFocus)
			{
				bShouldSetFocus = ToggleActiveFocus(In);
			}
		}
	}
	In.Skip( bShouldSetFocus == TRUE );
}

void WxPropertyControl::OnRightClick( wxMouseEvent& In )
{
	// Right clicking on an item expands all items rooted at this property window node.
	UBOOL bShouldSetFocus = TRUE;
	UBOOL bShouldSkip = FALSE;

	const UBOOL bProxyHandledClick = InputProxy->RightClick( this, In.GetX(), In.GetY() );
	if( !bProxyHandledClick )
	{
		//try giving it context first
		ToggleActiveFocus(In);
		GetPropertyWindow()->RefreshEntireTree();

		if ( ShowPropertyItemContextMenu(In) )
		{
			bShouldSetFocus = FALSE;
			bShouldSkip = TRUE;
		}
		else
		{
			check(PropertyNode);
			// The proxy didn't handle the click -- toggle expansion state?
			if( PropertyNode->HasNodeFlags(EPropertyNodeFlags::CanBeExpanded))
			{
				UBOOL bExpand = !PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded);
				UBOOL bRecurse = TRUE;
				PropertyNode->SetExpanded(bExpand, bRecurse);
				bShouldSetFocus = FALSE;
			}
		}
	}

	if ( bShouldSetFocus )
	{
		// Item focus is changing so, make sure the property has
		// been updated with the input proxy's state.
		InputProxy->SendToObjects( this );

		// Flag the parent for updating.
		RefreshHierarchy();
	}

	In.Skip((bShouldSetFocus || bShouldSkip) == TRUE);
}

/**
 * Toggle Focus
 * @return - whether to set focus on this window or not
 */
UBOOL WxPropertyControl::ToggleActiveFocus ( wxMouseEvent& In )
{
	//get main property window
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	WxPropertyWindowHost* HostWindow = MainWindow->GetParentHostWindow();
	check(HostWindow);

	//if control is down, always 
	if (!In.ControlDown())
	{
		HostWindow->ClearActiveFocus();
	}
	//if already selected, unselect.  Or add to selection
	HostWindow->SetActiveFocus(this, !HostWindow->HasActiveFocus(this) || In.m_rightDown);
	MainWindow->RefreshEntireTree();

	GCallbackEvent->Send( CALLBACK_PropertySelectionChange );

	INT NumActiveFocusWindows = HostWindow->NumActiveFocusWindows();
	//if nothing has focus or multiple things have focus
	if (NumActiveFocusWindows != 1)
	{
		//give it control as we're about to do a multi-select
		MainWindow->RequestMainWindowTakeFocus();
		return FALSE;
	}
	return TRUE;
}

/**
 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
 * and (if the property window item is expandable) to toggle expansion state.
 *
 * @param	Event	the mouse click input that generated the event
 *
 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
 */
UBOOL WxPropertyControl::ClickedPropertyItem( wxMouseEvent& Event )
{
	UBOOL bShouldGainFocus = TRUE;

	if(PropertyNode->HasNodeFlags(EPropertyNodeFlags::CanBeExpanded) )
	{
		// Only expand if clicking on the left side of the splitter
		if (Event.GetX() < GetPropertyWindow()->GetSplitterPos() || PropertyNode->IsEditConst())
		{
			const UBOOL bExpand = !PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded);
			const UBOOL bRecurse = FALSE;
			PropertyNode->SetExpanded(bExpand, bRecurse);

			// We want to still gain focus to allow the expanded node to be shifted if it is a favorite node
			bShouldGainFocus = HasNodeFlags(EPropertyNodeFlags::IsFavorite);
		}
	}
	else
	{
		// Item focus is changing so, make sure the property has
		// been updated with the input proxy's state.
		InputProxy->SendToObjects( this );

		// Flag the parent for updating.
		FPropertyNode* ParentNode = PropertyNode->GetParentNode();
		if (ParentNode)
		{
			WxPropertyControl* ParentWindow = ParentNode->GetNodeWindow();
			ParentWindow->Refresh();
		}
	}

	return bShouldGainFocus;
}


void WxPropertyControl::OnLeftUnclick( wxMouseEvent& In )
{
	// Stop dragging the splitter
	if( GetPropertyWindow()->IsDraggingSplitter() )
	{
		GetPropertyWindow()->StopDraggingSplitter();
		LastX = 0;
		ReleaseMouse();
		wxSizeEvent se;
		GetPropertyWindow()->OnSize( se );
	}
	else
	{
		InputProxy->LeftUnclick( this, In.GetX(), In.GetY());
	}
}

void WxPropertyControl::OnMouseMove( wxMouseEvent& In )
{
	if( GetPropertyWindow()->IsDraggingSplitter() )
	{
		GetPropertyWindow()->MoveSplitterPos( -(LastX - In.GetX()) );
		LastX = In.GetX();
		SetCursor( wxCursor( wxCURSOR_SIZEWE ) );
	}

	if( IsOnSplitter( In.GetX() ) )
	{
		SetCursor( wxCursor( wxCURSOR_SIZEWE ) );
	}
	else
	{
		SetCursor( wxCursor( wxCURSOR_ARROW ) );
	}
}

void WxPropertyControl::OnLeftDoubleClick( wxMouseEvent& In )
{
	PrivateLeftDoubleClick(In.GetX(), In.GetY());
}

// Implements OnLeftDoubleClick().  This is a standalone function so that double click events can occur apart from wx callbacks.
/**
 * Implements OnLeftDoubleClick().
 * @param InX - Window relative cursor x position
 * @param InY - Window relative cursor y position
 */
void WxPropertyControl::PrivateLeftDoubleClick(const INT InX, const INT InY)
{
	FString Value = GetPropertyText();
	UBOOL bForceValue = FALSE;
	if( !Value.Len() )
	{
		Value = GTrue;
		bForceValue = TRUE;
	}

	// Assemble a list of objects and their addresses.
	TArray<FObjectBaseAddress> ObjectsToModify;

	check(PropertyNode);
	FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();
	for( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject*	Object = *Itor;
		BYTE*		Addr = PropertyNode->GetValueBaseAddress( (BYTE*) Object );
		ObjectsToModify.AddItem( FObjectBaseAddress( Object, Addr ) );
	}

	// Process the double click.
	InputProxy->DoubleClick( this, ObjectsToModify, *Value, bForceValue );

	Refresh();
}

void WxPropertyControl::OnChangeFocus( wxFocusEvent& In )
{
	FPropertyNode* PropertyNode = GetPropertyNode();
	check(PropertyNode);
	WxPropertyWindow* MainWindow = PropertyNode->GetMainWindow();

	if (MainWindow->HasFlags(EPropertyWindowFlags::ReadOnly))
	{
		return;
	}

	if (In.GetEventType() == wxEVT_SET_FOCUS)
	{
		if( MainWindow->GetLastFocused()==this && InputProxy->IgnoreDoubleSetFocus() )
		{
			return;
		}
	}


	MainWindow->Freeze();

	if (In.GetEventType() == wxEVT_KILL_FOCUS)
	{
		WxPropertyControl* UpCastWindow = wxDynamicCast(In.GetEventObject(), WxPropertyControl);
		check(UpCastWindow);
		UpCastWindow->InputProxy->SendToObjects( UpCastWindow );
		UpCastWindow->InputProxy->OnKillFocus(UpCastWindow);
	} 
	else if (In.GetEventType() == wxEVT_SET_FOCUS)
	{
		InputProxy->OnSetFocus(this);
		if (MainWindow->GetLastFocused() != this)
		{
			MainWindow->SetLastFocused( this );
		}
	}

	// Set this node as being in focus.
	MainWindow->Thaw();
	MainWindow->RefreshEntireTree();

	GCallbackEvent->Send( CALLBACK_PropertySelectionChange );
}



/**
 * Parent path creation common to all types.
 */
FString WxPropertyControl::GetParentPath() const
{
	FString Name;


	FPropertyNode* ParentNode = PropertyNode->GetParentNode();
	if( ParentNode )
	{
		WxPropertyControl* ParentWindow = ParentNode->GetNodeWindow();
		Name += ParentWindow->GetPath();
		Name += TEXT(".");
	}

	return Name;
}
// Creates a path to the current item.
FString WxPropertyControl::GetPath() const
{
	FString Name = GetParentPath();

	const UProperty* Property = GetProperty();
	if( Property )
	{
		Name += Property->GetName();
	}

	return Name;
}


/**
 * Finds the top-most object window that contains this item and issues a Refresh() command
 */
void WxPropertyControl::RefreshHierarchy()
{
	// not every type of change to property values triggers a proper refresh of the hierarchy, so find the topmost container window and trigger a refresh manually.
	check(PropertyNode);
	FObjectPropertyNode* TopmostObjectItem=PropertyNode->FindRootObjectItemParent();
	if ( TopmostObjectItem )
	{
		wxWindow* TopmostWindow = TopmostObjectItem->GetNodeWindow();
		TopmostWindow->Refresh();
	}
	else
	{
		for ( FPropertyNode* NextParent = PropertyNode; NextParent; NextParent = NextParent->GetParentNode())
		{
			wxWindow* ParentWindow = NextParent->GetNodeWindow();
			ParentWindow->Refresh();
		}
	}
}

void WxPropertyControl::OnPropItemButton( wxCommandEvent& In )
{
	this->SetFocus();

	// Note the current property window so that CALLBACK_ObjectPropertyChanged doesn't
	// destroy the window out from under us.
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	//ensure that no child text control still has focus, and will rebuild during destruction
	//call must be above GApp->CurrentPropertyWindow = MainWindow.
	MainWindow->RequestMainWindowTakeFocus();

	MainWindow->ChangeActiveCallbackCount(1);

	FPropertyNode* RegularRebuildNode = PropertyNode;
	check(RegularRebuildNode);
	//node in the case of duplication, insertion, deletion
	FPropertyNode* ParentRebuildNode = RegularRebuildNode;
	FPropertyNode* ParentNode = PropertyNode->GetParentNode();
	if (ParentNode)
	{
		ParentRebuildNode = ParentNode;		//in the case of duplicate or insert
	}

	switch( In.GetId() )
	{
		case ID_PROP_PB_ADD:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Add );
			MainWindow->RebuildSubTree(RegularRebuildNode);
			break;
		case ID_PROP_PB_EMPTY:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Empty );
			MainWindow->RebuildSubTree(RegularRebuildNode);
			break;
		case ID_PROP_PB_INSERT:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Insert );
			MainWindow->RebuildSubTree(ParentRebuildNode);
			break;
		case ID_PROP_PB_DELETE:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Delete );
			MainWindow->RebuildSubTree(ParentRebuildNode);
			break;
		case ID_PROP_PB_BROWSE:
			if (GApp != NULL)
			{
				InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Browse );
			}
			break;
		case ID_PROP_PB_CLEAR:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Clear );
			break;
		case ID_PROP_PB_FIND:
			if (GApp != NULL)
			{
				InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Find );
			}
			break;
		case ID_PROP_PB_USE:
			if (GApp != NULL)
			{
				InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Use );
			}
			break;
		case ID_PROP_PB_NEWOBJECT:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_NewObject );
			MainWindow->RebuildSubTree(RegularRebuildNode);
			break;
		case ID_PROP_PB_DUPLICATE:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_Duplicate );
			MainWindow->RebuildSubTree(ParentRebuildNode);
			break;
		case ID_PROP_PB_TEXTBOX:
			InputProxy->ButtonClick( this, UPropertyInputProxy::PB_TextBox );
			break;
		case ID_PROP_PB_CURVEEDIT:
			if ( GApp != NULL && GApp->EditorFrame != NULL && PropertyNode->GetNumChildNodes() > 0 &&
				PropertyNode->GetChildNode(0)->GetObjectNode() != NULL && PropertyNode->GetChildNode(0)->GetObjectNode()->GetObject(0) != NULL )
			{
				FString Label(PropertyNode->GetProperty()->GetName());
				CurveEditorWindow *CurveEditor = new CurveEditorWindow(GApp->EditorFrame, -1,
					PropertyNode->GetChildNode(0)->GetObjectNode()->GetObject(0), Label);
				CurveEditor->Show(1);
			}
			break;
		default:
			check( 0 );
			break;
	}

	// Unset, effectively making this property window updatable by CALLBACK_ObjectPropertyChanged.
	MainWindow->ChangeActiveCallbackCount(-1);
}

void WxPropertyControl::OnEditInlineNewClass( wxCommandEvent& In )
{
	check(PropertyNode);

	UClass* NewClass = *ClassMap.Find( In.GetId() );
	UObject* ObjArchetype = NULL;
	if( !NewClass )
	{
		if (NewObjArchetype)
		{
			NewClass = NewObjArchetype->GetClass();
			ObjArchetype = NewObjArchetype;
		}
		else
		{
			debugf(TEXT("WARNING -- couldn't find match EditInlineNewClass"));
			return;
		}
	}

	UBOOL bDoReplace = TRUE;
	FString CurrValue = this->GetPropertyText(); // this is used to check for a Null component and if we find one we do not want to pop up the dialog

	if( CurrValue != TEXT("None") )
	{
		// Prompt the user that this action will eliminate existing data.
		// DistributionFloat derived classes aren't prompted because they're often changed (eg for particles).
		const UBOOL bIsADistribution = NewClass->IsChildOf( UDistributionFloat::StaticClass() ) || NewClass->IsChildOf( UDistributionVector::StaticClass() );
		bDoReplace = bIsADistribution || appMsgf( AMT_YesNo, *LocalizeUnrealEd("EditInlineNewAreYouSure") );
	}

	if( bDoReplace )
	{
		//Collapse();

		TArray<FObjectBaseAddress> ObjectsToModify;
		TArray<FString> Values;

		// Create a new object for all objects for which properties are being edited.
		for ( TPropObjectIterator Itor( PropertyNode->FindObjectItemParent()->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject*		Parent				= *Itor;
			BYTE*			Addr				= PropertyNode->GetValueBaseAddress( (BYTE*) Parent );
			UObject*		UseOuter			= ( NewClass->IsChildOf( UClass::StaticClass() ) ? Cast<UClass>(Parent)->GetDefaultObject() : Parent );
			EObjectFlags	MaskedOuterFlags	= UseOuter ? UseOuter->GetMaskedFlags(RF_PropagateToSubObjects) : 0;
			UObject*		NewObject			= GEngine->StaticConstructObject( NewClass, UseOuter, NAME_None, MaskedOuterFlags | ((NewClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0), ObjArchetype );

			// If the object was created successfully, send its name to the parent object.
			if( NewObject )
			{
				ObjectsToModify.AddItem( FObjectBaseAddress( Parent, Addr ) );
				Values.AddItem( *NewObject->GetPathName() );
			}

			//if( GetPropertyWindow()->NotifyHook )
			//	GetPropertyWindow()->NotifyHook->NotifyExec( this, *FString::Printf(TEXT("NEWNEWOBJECT CLASS=%s PARENT=%s PARENTCLASS=%s"), *NewClass->GetName(), *(*Itor)->GetPathName(), *(*Itor)->GetClass()->GetName() ) );
		}

		if ( ObjectsToModify.Num() )
		{
			FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();

			// If more than one object is selected, an empty field indicates their values for this property differ.
			// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good).
			if( ObjectNode->GetNumObjects() > 1 && !Values(0).Len() )
			{
				check(0);
				return;
			}
			InputProxy->ImportText( this, ObjectsToModify, Values );
		}

		PropertyNode->FastRefreshEntireTree();
		//Expand(GetPropertyWindow()->GetFilterStrings());
	}
}

// Checks to see if an X position is on top of the property windows splitter bar.
UBOOL WxPropertyControl::IsOnSplitter( INT InX )
{
	return abs( InX - GetPropertyWindow()->GetSplitterPos() ) < 3;
}

// Calls through to the enclosing property window's NotifyPreChange.
void WxPropertyControl::NotifyPreChange(UProperty* PropertyAboutToChange, UObject* Object)
{
	GetPropertyWindow()->NotifyPreChange( this, PropertyAboutToChange, Object );
}

// Calls through to the enclosing property window's NotifyPreChange.
void WxPropertyControl::NotifyPostChange(FPropertyChangedEvent& InPropertyChangedEvent)
{
	GetPropertyWindow()->NotifyPostChange( this, InPropertyChangedEvent);
	RefreshHierarchy();
}

void WxPropertyControl::OnChar( wxKeyEvent& In )
{
	WxPropertyWindow* MainWindow = GetPropertyWindow();

	// Make sure the property has been updated with the input proxy's state.
	InputProxy->SendToObjects( this );

	// Flag the parent for updating.
	check(PropertyNode);
	FPropertyNode* ParentNode = PropertyNode->GetParentNode();
	if (ParentNode)
	{
		WxPropertyControl* ParentWindow = ParentNode->GetNodeWindow();
		ParentWindow->Refresh();
	}

	/////////////////////////////////////////////////////////
	// Parse the key and set flags for requested actions.

	UBOOL bToggleRequested = FALSE;		// Flag indicating whether or not we should toggle (expand or collapse) this property.
	UBOOL bExpandRequested = FALSE;		// Flag indicating whether or not we should try *just* expanding this property.
	UBOOL bCollapseRequested = FALSE;	// Flag indicating whether or not we should try *just* collapsing this property.
	UBOOL bFocusChangeRequest = FALSE;

	switch( In.GetKeyCode() )
	{
		case WXK_DOWN:
			bFocusChangeRequest = TRUE;
			break;

		case WXK_UP:
			bFocusChangeRequest = TRUE;
			break;

		case WXK_TAB:
			bFocusChangeRequest = TRUE;
			break;

		case WXK_LEFT:
			// Pressing LEFT on a struct header item will collapse it.
			bCollapseRequested = TRUE;
			break;

		case WXK_RIGHT:
			// Pressing RIGHT on a struct header item will expand it.
			bExpandRequested = TRUE;
			break;

		case WXK_RETURN:
			if ( Cast<UPropertyInputBool>( InputProxy ) )
			{
				PrivateLeftDoubleClick(0, 0);
			}
			// Pressing ENTER on a struct header item or object reference will toggle it's expanded status.
			bToggleRequested = TRUE;
			break;
	}

	/////////////////////////////////////////////////////////
	// We've got our requested actions.  Now, check if the focused item is exapndable/collapsable.
	// If it is, executed requested expand/collapse action.

	// The property is expandable if it's a struct property, or if it's a valid object reference.
	const UBOOL bIsStruct = ( Cast<UStructProperty>(PropertyNode->GetProperty(),CLASS_IsAUStructProperty) ? TRUE : FALSE );

	const UBOOL bSingleSelectOnly = PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly);
	TArray<BYTE*> Addresses;
	const UBOOL bIsValidObjectRef = Cast<UObjectProperty>(PropertyNode->GetProperty(),CLASS_IsAUObjectProperty) && PropertyNode->GetReadAddress( PropertyNode, bSingleSelectOnly, Addresses, FALSE );

	if ( bIsStruct || bIsValidObjectRef )
	{
		const UBOOL bRecurse = FALSE;
		if ( bToggleRequested )
		{
			// Toggle the expand/collapse state.
			PropertyNode->SetExpanded(!PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded), bRecurse);	//collapse
		}
		else if ( bExpandRequested )
		{
			PropertyNode->SetExpanded(TRUE, bRecurse);	//expand
		}
		else if ( bCollapseRequested )
		{
			PropertyNode->SetExpanded(FALSE, bRecurse);	//collapse
		}
	}

	UBOOL bNothingRequested = (!bToggleRequested && !bExpandRequested && !bCollapseRequested && !bFocusChangeRequest);

	//allow the event to bubble up
	if (bFocusChangeRequest || bNothingRequested)
	{
		if (wxDynamicCast(In.GetEventObject(), WxPropertyControl) == NULL)
		{
			In.SetEventObject(this);	//we want the FIRST window to get this event
		}
		In.ResumePropagation(wxEVENT_PROPAGATE_MAX);
		In.Skip();	//let parent deal with this
	}
}

/**
 * Returns the draw proxy class for the property contained by this item.
 */
UClass* WxPropertyControl::GetDrawProxyClass() const
{
	const WxPropertyWindow* MainWindow = GetPropertyWindow();

	UClass* Result = UPropertyDrawProxy::StaticClass();
	if ( GetProperty() != NULL )
	{
		if ( MainWindow->HasFlags(EPropertyWindowFlags::SupportsCustomControls))
		{
			UCustomPropertyItemBindings* Bindings = UCustomPropertyItemBindings::StaticClass()->GetDefaultObject<UCustomPropertyItemBindings>();
			check(Bindings);
			
			// if we support custom controls, see if this property has a custom draw proxy control class setup
			UClass* CustomPropertyClass = Bindings->GetCustomDrawProxy(this, PropertyNode->GetArrayIndex());
			if ( CustomPropertyClass != NULL )
			{
				Result = CustomPropertyClass;
			}
		}

		if ( Result == UPropertyDrawProxy::StaticClass() )
		{
			TArray<UPropertyDrawProxy*> StandardDrawProxies;
			StandardDrawProxies.AddItem( UPropertyDrawRotationHeader::StaticClass()->GetDefaultObject<UPropertyDrawProxy>() );
			StandardDrawProxies.AddItem( UPropertyDrawRotation::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()		);
			StandardDrawProxies.AddItem( UPropertyDrawNumeric::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()		);
			StandardDrawProxies.AddItem( UPropertyDrawColor::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()			);
			StandardDrawProxies.AddItem( UPropertyDrawBool::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()			);
			StandardDrawProxies.AddItem( UPropertyDrawArrayHeader::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()	);
			StandardDrawProxies.AddItem( UPropertyDrawTextEditBox::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()	);
			StandardDrawProxies.AddItem( UPropertyDrawMultilineTextBox::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()	);
			StandardDrawProxies.AddItem( UPropertyDrawRangedNumeric::StaticClass()->GetDefaultObject<UPropertyDrawProxy>()	);

			for( INT i=0; i<StandardDrawProxies.Num(); i++ )
			{
				check(StandardDrawProxies(i));
				if( StandardDrawProxies(i)->Supports( PropertyNode, PropertyNode->GetArrayIndex() ) )
				{
					Result = StandardDrawProxies(i)->GetClass();
				}
			}
		}
	}

	return Result;
}

/**
 * Returns the input proxy class for the property contained by this item.
 */
UClass* WxPropertyControl::GetInputProxyClass() const
{
	const WxPropertyWindow* MainWindow = GetPropertyWindow();

	UClass* Result = UPropertyInputProxy::StaticClass();
	if ( GetProperty() != NULL )
	{
		if (MainWindow->HasFlags(EPropertyWindowFlags::SupportsCustomControls))
		{
			UCustomPropertyItemBindings* Bindings = UCustomPropertyItemBindings::StaticClass()->GetDefaultObject<UCustomPropertyItemBindings>();
			check(Bindings);

			// if we support custom controls, see if this property has a custom draw proxy control class setup
			check(PropertyNode);
			UClass* CustomPropertyClass = Bindings->GetCustomInputProxy(this, PropertyNode->GetArrayIndex());
			if ( CustomPropertyClass != NULL )
			{
				Result = CustomPropertyClass;
			}
		}

		if ( Result == UPropertyInputProxy::StaticClass() )
		{
			TArray<UPropertyInputProxy*> StandardInputProxies;

			/** This list is order dependent, properties at the top are checked AFTER properties at the bottom. */
			StandardInputProxies.AddItem( UPropertyInputArrayItem::StaticClass()->GetDefaultObject<UPropertyInputProxy>()	);
			StandardInputProxies.AddItem( UPropertyInputColor::StaticClass()->GetDefaultObject<UPropertyInputProxy>()		);
			StandardInputProxies.AddItem( UPropertyInputBool::StaticClass()->GetDefaultObject<UPropertyInputProxy>()		);
			StandardInputProxies.AddItem( UPropertyInputNumeric::StaticClass()->GetDefaultObject<UPropertyInputProxy>()		);
			StandardInputProxies.AddItem( UPropertyInputInteger::StaticClass()->GetDefaultObject<UPropertyInputProxy>()	);
			StandardInputProxies.AddItem( UPropertyInputText::StaticClass()->GetDefaultObject<UPropertyInputProxy>()		);
			StandardInputProxies.AddItem( UPropertyInputTextEditBox::StaticClass()->GetDefaultObject<UPropertyInputTextEditBox>());
			StandardInputProxies.AddItem( UPropertyInputRotation::StaticClass()->GetDefaultObject<UPropertyInputProxy>()	);
			StandardInputProxies.AddItem( UPropertyInputEditInline::StaticClass()->GetDefaultObject<UPropertyInputProxy>()	);
			StandardInputProxies.AddItem( UPropertyInputCombo::StaticClass()->GetDefaultObject<UPropertyInputProxy>()		);
			StandardInputProxies.AddItem( UPropertyInputArray::StaticClass()->GetDefaultObject<UPropertyInputProxy>()		);

			StandardInputProxies.AddItem( UPropertyInputRangedNumeric::StaticClass()->GetDefaultObject<UPropertyInputProxy>());

			check(PropertyNode);
			for( INT i=0; i<StandardInputProxies.Num(); i++ )
			{
				check(StandardInputProxies(i));
				if( StandardInputProxies(i)->Supports( PropertyNode, PropertyNode->GetArrayIndex() ) )
				{
					Result = StandardInputProxies(i)->GetClass();
				}
			}
		}
	}

	return Result;
}
