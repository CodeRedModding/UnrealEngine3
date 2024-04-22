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

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectIterator		TPropObjectIterator;

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectConstIterator	TPropObjectConstIterator;

//-----------------------------------------------------------------------------
//
// FObjectPropertyNode
//
//-----------------------------------------------------------------------------
FObjectPropertyNode::FObjectPropertyNode(void)
: FPropertyNode(),
  BaseClass(NULL)
{
}
FObjectPropertyNode::~FObjectPropertyNode(void)
{
}

UObject* FObjectPropertyNode::GetObject (const INT InIndex)
{
	check(IsWithin(InIndex, 0, Objects.Num()));
	return Objects(InIndex);
}

// Adds a new object to the list.
void FObjectPropertyNode::AddObject( UObject* InObject )
{
	Objects.AddUniqueItem( InObject );
}

// Removes an object from the list.
void FObjectPropertyNode::RemoveObject( UObject* InObject )
{
	const INT idx = Objects.FindItemIndex( InObject );

	if( idx != INDEX_NONE )
	{
		Objects.Remove( idx, 1 );
	}

	if( !Objects.Num() )
	{
		const UBOOL bExpand = FALSE;
		const UBOOL bRecurse = FALSE;
		SetExpanded(bExpand, bRecurse);
	}
}

// Removes all objects from the list.
void FObjectPropertyNode::RemoveAllObjects()
{
	while( Objects.Num() )
	{
		RemoveObject( Objects(0) );
	}
}


// Called when the object list is finalized, Finalize() finishes the property window setup.
void FObjectPropertyNode::Finalize()
{
	check(TopPropertyWindow);

	// May be NULL...
	UClass* OldBase = GetObjectBaseClass();

	// Find an appropriate base class.
	SetBestBaseClass();
	if (BaseClass && BaseClass->HasAnyClassFlags(CLASS_CollapseCategories) )
	{
		SetNodeFlags(EPropertyNodeFlags::ShowCategories, FALSE );
	}

	// Only automatically expand and position the child windows if this is the top level object window.
	if (ParentNode == NULL)
	{
		SetNodeFlags(EPropertyNodeFlags::Expanded, TRUE);
		// Only reset thumb pos if base class has changed
		if(OldBase != GetObjectBaseClass())
		{
			TopPropertyWindow->SetThumbPos(0);
		}

		// Finally, try to update the window title.
		check(TopPropertyWindow);
		WxPropertyWindowHost* HostWindow = TopPropertyWindow->GetParentHostWindow();
		if (HostWindow)
		{
			WxPropertyWindowFrame* ParentFrame = wxDynamicCast(HostWindow->GetParent(),WxPropertyWindowFrame);
			if( ParentFrame )
			{
				ParentFrame->UpdateTitle();
			}
		}
	}

	SetNodeFlags(EPropertyNodeFlags::Expanded, TRUE);		//don't actually expand since we're in the of construciton.  We'll toggle tree state after.
}


UBOOL FObjectPropertyNode::GetReadAddress(FPropertyNode* InNode,
											   UBOOL InRequiresSingleSelection,
											   TArray<BYTE*>& OutAddresses,
											   UBOOL bComparePropertyContents,
											   UBOOL bObjectForceCompare,
											   UBOOL bArrayPropertiesCanDifferInSize)
{
	// Are any objects selected for property editing?
	if( !GetNumObjects())
	{
		return FALSE;
	}

	UProperty* InItemProperty = InNode->GetProperty();
	// Is there a InItemProperty bound to the InItemProperty window?
	if( !InItemProperty )
	{
		return FALSE;
	}

	// Requesting a single selection?
	if( InRequiresSingleSelection && GetNumObjects() > 1)
	{
		// Fail: we're editing properties for multiple objects.
		return FALSE;
	}

	//assume all properties are the same unless proven otherwise
	UBOOL bAllTheSame = TRUE;

	//////////////////////////////////////////

	// If this item is the child of an array, return NULL if there is a different number
	// of items in the array in different objects, when multi-selecting.

	if( Cast<UArrayProperty>(InItemProperty->GetOuter()) )
	{
		FPropertyNode* ParentNode = InNode->GetParentNode();
		check(ParentNode);
		UObject* TempObject = GetObject(0);
		const INT Num = ((FScriptArray*)ParentNode->GetValueBaseAddress((BYTE*)TempObject))->Num();
		for( INT i = 1 ; i < GetNumObjects(); i++ )
		{
			TempObject = GetObject(i);
			if( Num != ((FScriptArray*)ParentNode->GetValueBaseAddress((BYTE*)TempObject))->Num() )
			{
				bAllTheSame = FALSE;
			}
		}
	}

	BYTE* Base = InNode->GetValueBaseAddress( (BYTE*)(GetObject(0)) );
	if (Base)
	{
		// If the item is an array itself, return NULL if there are a different number of
		// items in the array in different objects, when multi-selecting.

		if( Cast<UArrayProperty>(InItemProperty) )
		{
			// This flag is an override for array properties which want to display e.g. the "Clear" and "Empty"
			// buttons, even though the array properties may differ in the number of elements.
			if ( !bArrayPropertiesCanDifferInSize )
			{
				UObject* TempObject = GetObject(0);
				INT const Num = ((FScriptArray*)InNode->GetValueBaseAddress( (BYTE*)TempObject))->Num();
				for( INT i = 1 ; i < GetNumObjects() ; i++ )
				{
					UObject* TempObject = GetObject(i);
					if( Num != ((FScriptArray*)InNode->GetValueBaseAddress((BYTE*)TempObject))->Num() )
					{
						bAllTheSame = FALSE;
					}
				}
			}
		}
		else
		{
			if ( bComparePropertyContents || !Cast<UObjectProperty>(InItemProperty,CLASS_IsAUObjectProperty) || bObjectForceCompare )
			{
				// Make sure the value of this InItemProperty is the same in all selected objects.
				for( INT i = 1 ; i < GetNumObjects() ; i++ )
				{
					UObject* TempObject = GetObject(i);
					if( !InItemProperty->Identical( Base, InNode->GetValueBaseAddress( (BYTE*)TempObject ) ) )
					{
						bAllTheSame = FALSE;
					}
				}
			}
			else
			{
				if ( Cast<UObjectProperty>(InItemProperty,CLASS_IsAUObjectProperty) )
				{
					// We don't want to exactly compare component properties.  However, we
					// need to be sure that all references are either valid or invalid.

					// If BaseObj is NON-NULL, all other objects' properties should also be non-NULL.
					// If BaseObj is NULL, all other objects' properties should also be NULL.
					UObject* BaseObj = *(UObject**) Base;

					for( INT i = 1 ; i < GetNumObjects() ; i++ )
					{
						UObject* TempObject = GetObject(i);
						UObject* CurObj = *(UObject**) InNode->GetValueBaseAddress( (BYTE*)TempObject );
						if (   ( !BaseObj && CurObj )			// BaseObj is NULL, but this InItemProperty is non-NULL!
							|| ( BaseObj && !CurObj ) )			// BaseObj is non-NULL, but this InItemProperty is NULL!
						{

							bAllTheSame = FALSE;
						}
					}
				}
			}
		}
	}

	// Write addresses to the output.
	for ( INT i = 0 ; i < GetNumObjects(); ++i )
	{
		UObject* TempObject = GetObject(i);
		OutAddresses.AddItem( InNode->GetValueBaseAddress( (BYTE*)(TempObject) ) );
	}

	// Everything checked out and we have usable addresses.
	return bAllTheSame;
}

/**
 * fills in the OutAddresses array with the addresses of all of the available objects.
 * @param InItem		The property to get objects from.
 * @param OutAddresses	Storage array for all of the objects' addresses.
 */
UBOOL FObjectPropertyNode::GetReadAddress(FPropertyNode* InNode,
											   TArray<BYTE*>& OutAddresses)
{
	// Are any objects selected for property editing?
	if( !GetNumObjects())
	{
		return FALSE;
	}

	UProperty* InItemProperty = InNode->GetProperty();
	// Is there a InItemProperty bound to the InItemProperty window?
	if( !InItemProperty )
	{
		return FALSE;
	}


	// Write addresses to the output.
	for ( INT ObjectIdx = 0 ; ObjectIdx < GetNumObjects() ; ++ObjectIdx )
	{
		UObject* TempObject = GetObject(ObjectIdx);

		OutAddresses.AddItem( InNode->GetValueBaseAddress( (BYTE*)(TempObject) ) );
	}

	// Everything checked out and we have usable addresses.
	return TRUE;
}


/**
 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
 */
BYTE* FObjectPropertyNode::GetValueBaseAddress( BYTE* StartAddress )
{
	BYTE* Result = StartAddress;

	UClass* ClassObject;
	if ( (ClassObject=Cast<UClass>((UObject*)Result)) != NULL )
	{
		Result = ClassObject->GetDefaults();
	}

	return Result;
}
/**
 * Initializes the entire hierarchy below this point
 * @param InRootObject - the top level object who this is the recursive property tree for
 * @param InTopLevelWindow - the wrapper PropertyWindow that houses all child windows
 */
void FObjectPropertyNode::InitBeforeNodeFlags (void)
{
	check(NodeWindow);

	StoredProperty = Property;
	Property = NULL;

	Finalize();
}

/**
 * Overriden function for Creating Child Nodes
 */
void FObjectPropertyNode::InitChildNodes(void)
{
	check(TopPropertyWindow);
	//if the categories draw the borders OR the object is in the middle of the hierarchy (no need for a border at all)
	if( HasNodeFlags(EPropertyNodeFlags::ShowCategories) || (ParentNode != NULL))
	{
		ChildHeight = ChildSpacer = 0;
	}
	else
	{
		ChildHeight = ChildSpacer = PROP_CategorySpacer;
	}

	if(HasNodeFlags(EPropertyNodeFlags::EditInlineUse))
	{
		SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, FALSE);
		return;
	}

	//temp fix for material expression ordering
	if (BaseClass && (BaseClass->IsChildOf(UMaterialExpression::StaticClass()) || BaseClass->bForceScriptOrder))
	{
		SetNodeFlags(EPropertyNodeFlags::IsForceScriptOrder, TRUE);
	}

	//////////////////////////////////////////
	// Assemble a list of category names by iterating over all fields of BaseClass.

	// build a list of classes that we need to look at
	TArray<UClass*> ClassesToConsider;
	for( INT i = 0; i < GetNumObjects(); ++i )
	{
		UObject* TempObject = GetObject( i );
		if( TempObject )
		{
			UClass* Class = TempObject->IsA( UClass::StaticClass() ) ? Cast<UClass>(TempObject) : TempObject->GetClass();
			ClassesToConsider.AddUniqueItem( Class );
		}
	}

	TArray<FName> Categories;
	for( TFieldIterator<UProperty> It(BaseClass); It; ++It )
	{
		UBOOL bHidden = FALSE;
		for( INT ClassIndex = 0; ClassIndex < ClassesToConsider.Num(); ClassIndex++ )
		{
			UClass* Class = ClassesToConsider( ClassIndex );
			if( Class->HideCategories.ContainsItem( It->Category ) )
			{
				bHidden = TRUE;
				break;
			}
		}

		UBOOL bMetaDataAllowVisible = TRUE;
		FString MetaDataVisibilityCheckString = It->GetMetaData(TEXT("bShowOnlyWhenTrue"));
		if (MetaDataVisibilityCheckString.Len())
		{
			//ensure that the metadata visiblity string is actually set to true in order to show this property
			GConfig->GetBool(TEXT("UnrealEd.PropertyFilters"), *MetaDataVisibilityCheckString, bMetaDataAllowVisible, GEditorUserSettingsIni);
		}

		if (bMetaDataAllowVisible)
		{
			if(TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable) || (It->HasAnyPropertyFlags(CPF_Edit) && !bHidden) )
			{
				Categories.AddUniqueItem( It->Category );
			}
		}
	}

	//////////////////////////////////////////
	// Add the category headers and the child items that belong inside of them.

	// Only show category headers if this is the top level object window and the parent window allows headers.
	if( /*ParentItem == NULL &&*/ HasNodeFlags(EPropertyNodeFlags::ShowCategories))
	{
		UBOOL bAllowAutoExpansion = TRUE;

		//Check to see if we have a layout saved, if we do, we do not want to auto-expand items.
		if(BaseClass != NULL && Categories.Num() > 0)
		{
			TArray<FString> WkExpandedItems;
			GPropertyWindowManager->GetExpandedItems(BaseClass->GetName(), WkExpandedItems);

			if(WkExpandedItems.Num() > 0)
			{
				bAllowAutoExpansion = FALSE;
			}
		}

		FString CategoryDelimiterString;
		CategoryDelimiterString.AppendChar( FPropertyNodeConstants::CategoryDelimiterChar );

		TArray< FPropertyNode* > ParentNodesToSort;

		// This is the main WxObjectsPropertyControl node for the window and categories are allowed.
		for( INT x = 0 ; x < Categories.Num() ; ++x )
		{
			const FName& FullCategoryPath = Categories(x);

			// Figure out the nesting level for this category
			TArray< FString > FullCategoryPathStrings;
			FullCategoryPath.ToString().ParseIntoArray( &FullCategoryPathStrings, *CategoryDelimiterString, TRUE );

			FPropertyNode* ParentLevelNode = this;
			FString CurCategoryPathString;
			for( INT PathLevelIndex = 0; PathLevelIndex < FullCategoryPathStrings.Num(); ++PathLevelIndex )
			{
				// Build up the category path name for the current path level index
				if( CurCategoryPathString.Len() != 0 )
				{
					CurCategoryPathString += FPropertyNodeConstants::CategoryDelimiterChar;
				}
				CurCategoryPathString += FullCategoryPathStrings( PathLevelIndex );
				const FName CategoryName( *CurCategoryPathString );

				// Check to see if we've already created a category at the specified path level
				UBOOL bFoundMatchingCategory = FALSE;
				{
					for( INT CurNodeIndex = 0; CurNodeIndex < ParentLevelNode->GetNumChildNodes(); ++CurNodeIndex )
					{
						FPropertyNode* ChildNode = ParentLevelNode->GetChildNode( CurNodeIndex );
						check( ChildNode != NULL );

						// Is this a category node?
						FCategoryPropertyNode* ChildCategoryNode = ChildNode->GetCategoryNode();
						if( ChildCategoryNode != NULL )
						{
							// Does the name match?
							if( ChildCategoryNode->GetCategoryName() == CategoryName )
							{
								// Descend by using the child node as the new parent
								bFoundMatchingCategory = TRUE;
								ParentLevelNode = ChildCategoryNode;
								break;
							}
						}
					}
				}

				// If we didn't find the category, then we'll need to create it now!
				if( !bFoundMatchingCategory )
				{
					// Create the category node and assign it to its parent node
					FCategoryPropertyNode* NewCategoryNode = new FCategoryPropertyNode;
					{
						WxCategoryPropertyControl* pwc = new WxCategoryPropertyControl;
						NewCategoryNode->SetCategoryName( CategoryName );

						NewCategoryNode->InitNode(pwc, ParentLevelNode, TopPropertyWindow, NULL, PROP_OFFSET_None, INDEX_NONE);

						//Check if we are allowed to do auto-expansion.
						if(bAllowAutoExpansion == TRUE)
						{
							// Recursively expand category properties if the category has been flagged for auto-expansion.
							if (BaseClass->AutoExpandCategories.ContainsItem(CategoryName)
								&&	!BaseClass->AutoCollapseCategories.ContainsItem(CategoryName))
							{
								NewCategoryNode->SetNodeFlags(EPropertyNodeFlags::Expanded, TRUE);
							}
						}

						// Add this node to it's parent.  Note that no sorting happens here, so the parent's
						// list of child nodes will not be in the correct order.  We'll keep track of which
						// nodes we added children to so we can sort them after we're finished adding new nodes.
						ParentLevelNode->AddChildNode(NewCategoryNode);
						ParentNodesToSort.AddUniqueItem( ParentLevelNode );
					}

					// Descend into the newly created category by using this node as the new parent
					ParentLevelNode = NewCategoryNode;
				}
			}
		}

		// Make sure all nodes that we added children to are properly sorted
		for( INT CurParentNodeIndex = 0; CurParentNodeIndex < ParentNodesToSort.Num(); ++CurParentNodeIndex )
		{
			ParentNodesToSort( CurParentNodeIndex )->SortChildren();
		}
	}
	else
	{
		const UBOOL bShouldShowNonEditable = TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable);
		// Iterate over all fields, creating items.
		for( TFieldIterator<UProperty> It(BaseClass); It; ++It )
		{
			if( bShouldShowNonEditable || (It->PropertyFlags&CPF_Edit && BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE) )
			{
				UProperty* CurProp = *It;
				FItemPropertyNode* NewItemNode = new FItemPropertyNode;
				WxItemPropertyControl* pwi = CreatePropertyItem(CurProp);
				NewItemNode->InitNode(pwi, this, TopPropertyWindow, CurProp, CurProp->Offset, INDEX_NONE);

				ChildNodes.AddItem(NewItemNode);
			}
		}
	}
}
/**
 * Overriden function for Creating the corresponding wxPropertyWindow_Base
 */
void FObjectPropertyNode::CreateInternalWindow(void)
{
	NodeWindow->Create(GetParentNodeWindow());
	if(!HasNodeFlags(EPropertyNodeFlags::ShowCategories))
	{
		WxObjectsPropertyControl* ObjectWindow = wxDynamicCast(NodeWindow, WxObjectsPropertyControl);
		check(ObjectWindow);
		ObjectWindow->SetWindowStyleFlag( wxCLIP_CHILDREN|wxCLIP_SIBLINGS|NodeWindow->GetWindowStyleFlag() );
	}
}
/**
 * Function to allow different nodes to add themselves to focus or not
 */
void FObjectPropertyNode::AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& OutVisibleArray)
{
	if (ParentNode != NULL)
	{
		OutVisibleArray.AddItem(GetNodeWindow());
	}
}

/**
 * Appends my path, including an array index (where appropriate)
 */
void FObjectPropertyNode::GetQualifedName(FString& PathPlusIndex, const UBOOL bWithArrayIndex) const
{
	if( ParentNode )
	{
		ParentNode->GetQualifedName(PathPlusIndex, bWithArrayIndex);
		PathPlusIndex += TEXT(".");
	}
	PathPlusIndex += TEXT("Object");
}

// Looks at the Objects array and returns the best base class.  Called by
// Finalize(); that is, when the list of selected objects is being finalized.
void FObjectPropertyNode::SetBestBaseClass()
{
	BaseClass = NULL;

	for( INT x = 0 ; x < Objects.Num() ; ++x )
	{
		UObject* Obj = Objects(x);
		UClass* ObjClass = Obj->GetClass() == UClass::StaticClass() ? Cast<UClass>(Obj) : Obj->GetClass();

		check( Obj );
		check( ObjClass );

		// Initialize with the class of the first object we encounter.
		if( BaseClass == NULL )
		{
			BaseClass = ObjClass;
		}

		// If we've encountered an object that's not a subclass of the current best baseclass,
		// climb up a step in the class hierarchy.
		while( !ObjClass->IsChildOf( BaseClass ) )
		{
			BaseClass = BaseClass->GetSuperClass();
		}
	}
}

/*-----------------------------------------------------------------------------
	WxObjectsPropertyControl
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxObjectsPropertyControl,WxPropertyControl);

BEGIN_EVENT_TABLE( WxObjectsPropertyControl, WxPropertyControl )
	EVT_PAINT( WxObjectsPropertyControl::OnPaint )
	EVT_ERASE_BACKGROUND( WxObjectsPropertyControl::OnEraseBackground )
END_EVENT_TABLE()

void WxObjectsPropertyControl::Create(wxWindow* InParent)
{
	WxPropertyControl::Create(InParent);

	FinishSetup();

}



/** Does nothing to avoid flicker when we redraw the screen. */
void WxObjectsPropertyControl::OnEraseBackground( wxEraseEvent& Event )
{
	wxDC* DC = Event.GetDC();

	if(DC)
	{
		// Clear background
		DC->SetBackground( wxBrush( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE), wxSOLID ) );
		DC->Clear();
	}
}

void WxObjectsPropertyControl::OnPaint( wxPaintEvent& In )
{
	check(PropertyNode);

	WxPropertyWindow* MainWindow = GetPropertyWindow();
	if( HasNodeFlags(EPropertyNodeFlags::ShowCategories) || (PropertyNode->GetParentNode()!= NULL))
	{
		In.Skip();
		return;
	}

	wxBufferedPaintDC dc( this );
	const wxRect rc = GetClientRect();

	//change color to more green
	INT GreenOffset = 0;
	wxColour BaseColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	if (PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering)) 
	{
		GreenOffset = 64;
		BaseColor = wxColour( Max(BaseColor.Red()-32, 0), BaseColor.Green(), Max(BaseColor.Blue()-32, 0) );
	} 

	// Clear background
// 	dc.SetBackground( wxBrush( BaseColor, wxSOLID ) );
// 	dc.Clear();

	dc.SetBrush( wxBrush( wxColour(64,64+GreenOffset,64), wxSOLID ) );
	dc.SetPen( *wxMEDIUM_GREY_PEN );
	dc.DrawRoundedRectangle( rc.x+1,rc.y+1, rc.width-2,rc.height-2, 5 );
}

// Creates a path to the current item.
FString WxObjectsPropertyControl::GetPath() const
{
	FString Name = GetParentPath();

	Name += TEXT("Object");

	return Name;
}


