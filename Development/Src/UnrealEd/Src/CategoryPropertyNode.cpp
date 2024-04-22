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
// FCategoryPropertyNode
//
//-----------------------------------------------------------------------------
FCategoryPropertyNode::FCategoryPropertyNode(void)
: FPropertyNode()
{
}
FCategoryPropertyNode::~FCategoryPropertyNode(void)
{
}

/**
 * Overriden function for special setup
 */
void FCategoryPropertyNode::InitBeforeNodeFlags (void)
{
	if( IsSubcategory() )
	{
		ChildHeight = PROP_SubcategoryHeight;
		ChildSpacer = PROP_SubcategorySpacer;
	}
	else
	{
		ChildHeight = PROP_CategoryHeight;
		ChildSpacer = PROP_CategorySpacer;
	}
}
/**
 * Overriden function for Creating Child Nodes
 */
void FCategoryPropertyNode::InitChildNodes(void)
{
	// Check to see if this category is excluded from sorting
	FObjectPropertyNode* ParentObjectWindow = FindObjectItemParent();
	if ( ParentObjectWindow != NULL )
	{
		UClass* BaseClass = ParentObjectWindow->GetObjectBaseClass();
		if( BaseClass != NULL )
		{
			if( BaseClass->DontSortCategories.ContainsItem( CategoryName ) )
			{
				SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);
			}
		}
	}

	TArray<UProperty*> Properties;

	// The parent of a category window has to be an object window.
	FObjectPropertyNode* itemobj = FindObjectItemParent();

	const UBOOL bShouldShowNonEditable = TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable);

	// Get a list of properties that are in the same category
	for( TFieldIterator<UProperty> It(itemobj->GetObjectBaseClass()); It; ++It )
	{
		UBOOL bMetaDataAllowVisible = TRUE;
		FString MetaDataVisibilityCheckString = It->GetMetaData(TEXT("bShowOnlyWhenTrue"));
		if (MetaDataVisibilityCheckString.Len())
		{
			//ensure that the metadata visiblity string is actually set to true in order to show this property
			GConfig->GetBool(TEXT("UnrealEd.PropertyFilters"), *MetaDataVisibilityCheckString, bMetaDataAllowVisible, GEditorUserSettingsIni);
		}

		if (bMetaDataAllowVisible)
		{
			// Add if we are showing non-editable props and this is the 'None' category, 
			// or if this is the right category, and we are either showing non-editable
			if ( (bShouldShowNonEditable && CategoryName == NAME_None && It->Category == NAME_None) ||
				(It->Category == CategoryName && (bShouldShowNonEditable || (It->PropertyFlags & CPF_Edit))) )
			{
				Properties.AddItem( *It );
			}
		}
	}

	for( INT x = 0 ; x < Properties.Num() ; ++x )
	{
		FItemPropertyNode* NewItemNode = new FItemPropertyNode;//CreatePropertyItem(Properties(x));
		WxItemPropertyControl* pwi = CreatePropertyItem(Properties(x));
		NewItemNode->InitNode(pwi, this, TopPropertyWindow, Properties(x), Properties(x)->Offset, INDEX_NONE);

		ChildNodes.AddItem(NewItemNode);
	}
}
/**
 * Overriden function for Creating the corresponding wxPropertyWindow_Base
 */

void FCategoryPropertyNode::CreateInternalWindow(void)
{
	check(NodeWindow);
	WxCategoryPropertyControl* CategoryWindow = wxDynamicCast(NodeWindow, WxCategoryPropertyControl);
	CategoryWindow->Create(GetParentNodeWindow());
}

/**
 * Function to allow different nodes to add themselves to focus or not
 */
void FCategoryPropertyNode::AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& OutVisibleArray)
{
}

/**
 * Appends my path, including an array index (where appropriate)
 */
void FCategoryPropertyNode::GetQualifedName( FString& PathPlusIndex, const UBOOL bWithArrayIndex ) const
{
	if( ParentNode )
	{
		ParentNode->GetQualifedName(PathPlusIndex, bWithArrayIndex);
		PathPlusIndex += TEXT(".");
	}
	check(NodeWindow);
	WxCategoryPropertyControl* CategoryWindow = wxDynamicCast(NodeWindow, WxCategoryPropertyControl);
	CategoryWindow->GetCategoryName().AppendString(PathPlusIndex);
}

/**Index of the first child property defined by script*/
INT FCategoryPropertyNode::GetPropertyScriptOrder(void)
{
	INT ScriptOrder = -1;
	if (GetNumChildNodes() > 0)
	{
		ScriptOrder = GetChildNode(0)->GetPropertyScriptOrder();
	}
	return ScriptOrder;
}

/*-----------------------------------------------------------------------------
	WxCategoryPropertyControl
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxCategoryPropertyControl,WxPropertyControl);

BEGIN_EVENT_TABLE( WxCategoryPropertyControl, WxPropertyControl )
	EVT_LEFT_DOWN( WxCategoryPropertyControl::OnLeftClick )
	EVT_RIGHT_DOWN( WxCategoryPropertyControl::OnRightClick )
	EVT_PAINT( WxCategoryPropertyControl::OnPaint )
	EVT_ERASE_BACKGROUND( WxCategoryPropertyControl::OnEraseBackground )
	EVT_MOTION( WxCategoryPropertyControl::OnMouseMove )
END_EVENT_TABLE()




/**
 * Virtual function to allow generation of display name
 */
void WxCategoryPropertyControl::InitBeforeCreate()
{
	const FString SubcategoryName = GetSubcategoryName();

	// Clean up the category name
	{
		//only create the display name if it hasn't been created before.
		if (DisplayName.Len() == 0)
		{
			FString CategoryDisplayName = SubcategoryName;
			const UBOOL bIsBoolProperty = FALSE;
			SanitizePropertyDisplayName( CategoryDisplayName, bIsBoolProperty );
			DisplayName = *CategoryDisplayName;
		}
	}
}


void WxCategoryPropertyControl::Create(wxWindow* InParent) 
{
	WxPropertyControl::Create( InParent);
	//should already be based in....CategoryName = InObjectTreeNode->GetCategoryName();

	FinishSetup();

	SetWindowStyleFlag( wxCLIP_CHILDREN|wxCLIP_SIBLINGS|GetWindowStyleFlag() );
}

void WxCategoryPropertyControl::OnPaint( wxPaintEvent& In )
{
	check(PropertyNode);
	const FCategoryPropertyNode* CategoryPropertyNode = PropertyNode->GetCategoryNode();
	check( CategoryPropertyNode != NULL );

	const UBOOL bIsSubcategory = CategoryPropertyNode->IsSubcategory();

	INT IndentX = PropertyNode->GetIndentX();
	wxBufferedPaintDC dc( this );

	wxRect rc = GetClientRect();

	INT GreenOffset = 0;
	wxColour BaseColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	if (PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering)) 
	{
		//change color to more green
		GreenOffset = 64;
		BaseColor = wxColour( Max(BaseColor.Red()-32, 0), BaseColor.Green(), Max(BaseColor.Blue()-32, 0) );
	} 

	// Clear background
	dc.SetBackground( wxBrush( BaseColor, wxSOLID ) );
	dc.Clear();

	dc.SetFont( *wxNORMAL_FONT );
	dc.SetPen( *wxMEDIUM_GREY_PEN );
	wxFont Font = dc.GetFont();
	Font.SetWeight( wxBOLD );
	dc.SetFont( Font );
	const UBOOL bExpanded = PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded);

	if( bIsSubcategory )
	{
		WxPropertyWindow* MainWindow = GetPropertyWindow();
		check(MainWindow);


		// Draw an expansion arrow on the left
		{
			//set up as solid or transparent
			UBOOL bNormalExpansion = TRUE;	// @todo: MainWindow->IsNormalExpansionAllowed(PropertyNode);
			INT BrushAndPenStyle = bNormalExpansion ? wxSOLID : wxTRANSPARENT;
			dc.SetBrush( wxBrush( BaseColor, BrushAndPenStyle ) );
			dc.SetPen( wxPen( BaseColor, 1, BrushAndPenStyle ) );

			wxBitmap* ArrowBitmap = NULL;
			if (bNormalExpansion)
			{
				ArrowBitmap = bExpanded ? &(GPropertyWindowManager->ArrowDownB): &(GPropertyWindowManager->ArrowRightB);
			} 
			else 
			{
				ArrowBitmap = bExpanded ? &(GPropertyWindowManager->TransparentArrowDownB): &(GPropertyWindowManager->TransparentArrowRightB);
			}

			check(ArrowBitmap);
			// Draw the down or right arrow, depending on whether the node is expanded.
			dc.DrawBitmap( *ArrowBitmap, IndentX - PROP_ARROW_Width, rc.y+((PROP_DefaultItemHeight - GPropertyWindowManager->ArrowDownB.GetHeight())/2), bNormalExpansion ? true : false);

			dc.SetBrush( wxBrush( BaseColor, wxSOLID ) );
		}

		// Vertical splitter line
		{
			const INT SplitterPos = MainWindow->GetSplitterPos()-2;
			const wxColour DarkerColor( BaseColor.Red()-64, BaseColor.Green()-64, BaseColor.Blue()-64 );
			dc.SetPen( wxPen( DarkerColor, 1, wxSOLID ) );
			dc.DrawLine( SplitterPos,0, SplitterPos,rc.GetHeight() );
		}

		// Category label underline, if the category is expanded
		if( bExpanded )
		{
			const INT SplitterPos = MainWindow->GetSplitterPos()-2;
			const wxColour DarkerColor( BaseColor.Red()-64, BaseColor.Green()-64, BaseColor.Blue()-64 );
			dc.SetPen( wxPen( DarkerColor, 1, wxSOLID ) );
			const INT UnderlineY = PROP_SubcategoryHeight - 1;
			dc.DrawLine( IndentX, UnderlineY, rc.GetWidth(), UnderlineY );
		}

		// Draw the category name
		dc.SetTextBackground( BaseColor );
		dc.SetTextForeground( *wxBLACK );
		dc.DrawText( *GetDisplayName(), rc.x+IndentX, rc.y+1 );
	}
	else
	{
		wxBitmap* ArrowBitmap = NULL;
		// Top level categories have darker edge borders.
		if( PropertyNode->GetParentNode() == PropertyNode->FindRootObjectItemParent())
		{
			dc.SetBrush( wxBrush( wxColour(64,64+GreenOffset,64), wxSOLID ) );
		}
		else
		{
			dc.SetBrush( wxBrush( wxColour(96,96+GreenOffset,96), wxSOLID ) );
		}
		ArrowBitmap = bExpanded ? &(GPropertyWindowManager->WhiteArrowDownB): &(GPropertyWindowManager->WhiteArrowRightB);
		dc.DrawRoundedRectangle( rc.x+IndentX+1,rc.y+1, rc.width-2,rc.height-2, 5 );

		dc.SetTextForeground( *wxWHITE );

		dc.DrawText( *GetDisplayName(), rc.x+IndentX+4+PROP_ARROW_Width,rc.y+5 );


		check(ArrowBitmap);
		dc.DrawBitmap( *ArrowBitmap, IndentX+4, rc.y + ((PROP_DefaultItemHeight - GPropertyWindowManager->WhiteArrowDownB.GetHeight())/2+2), true);
	}
}

void WxCategoryPropertyControl::OnLeftClick( wxMouseEvent& In )
{
	const UBOOL bExpand = !PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded);
	const UBOOL bRecurse = FALSE;
	PropertyNode->SetExpanded(bExpand, bRecurse);

	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->RequestMainWindowTakeFocus();

	In.Skip(FALSE);	//stop receiving events
}

void WxCategoryPropertyControl::OnRightClick( wxMouseEvent& In )
{
	// Right clicking on an item expands all items rooted at this property window node.
	const UBOOL bExpand = !PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded);
	const UBOOL bRecurse = bExpand;		//expand all if expanding
	PropertyNode->SetExpanded(bExpand, bRecurse);

	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->RequestMainWindowTakeFocus();

	In.Skip(FALSE);	//stop receiving events
}

void WxCategoryPropertyControl::OnMouseMove( wxMouseEvent& In )
{
	// Blocks message from the base class
	In.Skip(FALSE);	//stop receiving events
}


// Creates a path to the current item.
FString WxCategoryPropertyControl::GetPath() const
{
	FString Name = GetParentPath();

	Name += GetCategoryName().ToString();

	return Name;
}

/**
 * Accessor to get category name from the PropertyTreeCategoryNode
 */
const FName& WxCategoryPropertyControl::GetCategoryName() const
{
	check(PropertyNode);
	FCategoryPropertyNode* CategoryNode = PropertyNode->GetCategoryNode();
	check(CategoryNode);

	return CategoryNode->GetCategoryName();
}


/**
 * Gets only the last part of the category name (the subcategory)
 *
 * @return	Returns the subcategory name string
 */
const FString WxCategoryPropertyControl::GetSubcategoryName() const
{
	FString SubcategoryName;
	{
		// The category name may actually contain a path of categories.  When displaying this category
		// in the property window, we only want the leaf part of the path
		const FString& CategoryPath = GetCategoryName().ToString();
		FString CategoryDelimiterString;
		CategoryDelimiterString.AppendChar( FPropertyNodeConstants::CategoryDelimiterChar );  
		const INT LastDelimiterCharIndex = CategoryPath.InStr( CategoryDelimiterString, TRUE );
		if( LastDelimiterCharIndex != INDEX_NONE )
		{
			// Grab the last sub-category from the path
			SubcategoryName = CategoryPath.Mid( LastDelimiterCharIndex + 1 );
		}
		else
		{
			// No sub-categories, so just return the original (clean) category path
			SubcategoryName = CategoryPath;
		}
	}
	return SubcategoryName;
}


/**
 * virtualized to allow custom naming
 */
FString WxCategoryPropertyControl::GetDisplayName (void) const
{
	FString SubcategoryName =  GetSubcategoryName();
	//if in "readable display name mode" return that
	if (GPropertyWindowManager->GetShowFriendlyPropertyNames())
	{
		//if there is a localized version, return that
		FString LocalizedName = LocalizeProperty( TEXT("Category"), *SubcategoryName );
		if (LocalizedName.Len() > 0)
		{
			return LocalizedName;
		}
		return DisplayName;
	}
	return SubcategoryName;
}
