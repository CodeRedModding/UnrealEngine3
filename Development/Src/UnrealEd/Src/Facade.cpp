/*=============================================================================
	Facade.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineProcBuildingClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "Facade.h"
#include "StaticMeshEditor.h"
#include "PropertyWindow.h"
#include "Factories.h"
#include "UnEdTran.h"

static INT	DuplicateOffset = 30;

IMPLEMENT_CLASS( UFacadeHelper );
IMPLEMENT_DYNAMIC_CLASS(WxFacade, WxLinkedObjEd);

/** Default constructor */
UFacadeHelper::UFacadeHelper()
:	AllRules( this )
{
}

/**
 * Serialize this class' UObject references
 *
 * @param	Ar	Archive to serialize to
 */
void UFacadeHelper::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << AllRules;
	}
}

/*-----------------------------------------------------------------------------
	WxMBFacadeConnectorOptions
-----------------------------------------------------------------------------*/

class WxMBFacadeConnectorOptions : public wxMenu
{
public:
	WxMBFacadeConnectorOptions( WxFacade* Facade )
	{
		Append( IDM_FACADE_BREAK_LINK, *LocalizeUnrealEd("BreakLink"), TEXT("") );
	}

	~WxMBFacadeConnectorOptions()
	{

	}
};

/*-----------------------------------------------------------------------------
	WxMBFacadeNodeOptions
-----------------------------------------------------------------------------*/

class WxMBFacadeNodeOptions : public wxMenu
{
public:
	WxMBFacadeNodeOptions( WxFacade* Facade )
	{
		Append( IDM_FACADE_SELECT_DOWNSTREAM_NODES, *LocalizeUnrealEd("SelectDownstream"), TEXT("") );
		Append( IDM_FACADE_SELECT_UPSTREAM_NODES, *LocalizeUnrealEd("SelectUpstream"), TEXT("") );
	}

	~WxMBFacadeNodeOptions()
	{

	}
};

/*-----------------------------------------------------------------------------
	WxMBFacadeNewNode
-----------------------------------------------------------------------------*/

class WxMBFacadeNewNode : public wxMenu
{
public:
	WxMBFacadeNewNode( WxFacade* Facade )
	{
		for(INT i=0; i<Facade->PBRuleClasses.Num(); i++)
		{
			Append( IDM_FACADE_NEW_RULE_START + i, *Facade->PBRuleClasses(i)->GetDescription(), TEXT( "" ) );
		}
	}
	
	~WxMBFacadeNewNode()
	{
	
	}
};

/*-----------------------------------------------------------------------------
	WxFacade
-----------------------------------------------------------------------------*/



UBOOL				WxFacade::bPBRuleClassesInitialized = false;
TArray<UClass *>	WxFacade::PBRuleClasses;

BEGIN_EVENT_TABLE( WxFacade, wxFrame )
	EVT_MENU( IDM_FACADE_SELECT_DOWNSTREAM_NODES, WxFacade::OnSelectDownsteamNodes )
	EVT_MENU( IDM_FACADE_SELECT_UPSTREAM_NODES, WxFacade::OnSelectUpsteamNodes )
	EVT_MENU_RANGE( IDM_FACADE_NEW_RULE_START, IDM_FACADE_NEW_RULE_END, WxFacade::OnContextNewRule )
	EVT_MENU( IDM_FACADE_BREAK_LINK, WxFacade::OnContextBreakLink )
	EVT_TOOL( IDM_FACADE_REAPPLY, WxFacade::OnReapplyRuleset)
	EVT_TOOL( IDM_FACADE_UNDO, WxFacade::OnFacadeUndo )
	EVT_TOOL( IDM_FACADE_REDO, WxFacade::OnFacadeRedo )
	EVT_UPDATE_UI( IDM_FACADE_UNDO, WxFacade::UpdateFacadeUndoUI )
	EVT_UPDATE_UI( IDM_FACADE_REDO, WxFacade::UpdateFacadeRedoUI )
END_EVENT_TABLE()

IMPLEMENT_COMPARE_POINTER( UClass, Facade, { return appStricmp( *A->GetName(), *B->GetName() ); } )

/** Static functions that fills in array of all available UPBRuleNode classes and sorts them alphabetically. */
void WxFacade::InitPBRuleClasses()
{
	if( bPBRuleClassesInitialized )
	{
		return;
	}

	// Construct list of non-abstract rule node classes.
	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->IsChildOf(UPBRuleNodeBase::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			PBRuleClasses.AddItem(*It);
		}
	}

	// Sort
	Sort<USE_COMPARE_POINTER( UClass, Facade )>( &PBRuleClasses( 0 ), PBRuleClasses.Num() );

	bPBRuleClassesInitialized = true;
}

WxFacade::WxFacade() : WxLinkedObjEd( NULL, 0, TEXT( "Facade" ) ),
	Ruleset( NULL ),
	ConnObj( NULL ),
	ConnType( LOC_INPUT ),
	ConnIndex( 0 ),
	PasteCount( 0 ),
	DuplicationCount( 0 ),
	FacadeHelper( NULL ),
	FacadeTrans( NULL )
{
}

WxFacade::WxFacade( wxWindow* InParent, wxWindowID InID, UProcBuildingRuleset* InRuleset )
:	WxLinkedObjEd( InParent, InID, TEXT( "Facade" ) ),
	Ruleset( InRuleset ),
	ConnObj( NULL ),
	ConnType( LOC_INPUT ),
	ConnIndex( 0 ),
	PasteCount( 0 ),
	DuplicationCount( 0 ),
	FacadeHelper( NULL ),
	FacadeTrans( NULL )
{
	check( Ruleset );

	FacadeTrans = new UTransBuffer( 8 * 1024 * 1024 );
	check( FacadeTrans );

	FacadeHelper = new ( UObject::GetTransientPackage(), NAME_None, RF_Transactional ) UFacadeHelper();
	check( FacadeHelper );

	Ruleset->bBeingEdited = TRUE;
	
	if ( Ruleset->RootRule )
	{
		TArray<UPBRuleNodeBase*> RuleNodes;
		Ruleset->RootRule->GetRuleNodes( RuleNodes );

		// Copy the rule nodes into the special TTransArray for tracking all of the rules to use in Facade
		// *NOTE*: The FacadeHelper->AllRules *could* have been passed into GetRuleNodes, however none of TArray's
		// methods are virtual, which will result in the wrong methods being called!
		for ( TArray<UPBRuleNodeBase*>::TConstIterator RuleNodeIter( RuleNodes ); RuleNodeIter; ++RuleNodeIter )
		{
			FacadeHelper->AllRules.AddItem( *RuleNodeIter );
		}
	}

	// Add each comment node to the all rules array as well
	for ( TArray<UPBRuleNodeComment*>::TIterator CommentIter( Ruleset->Comments ); CommentIter; ++CommentIter )
	{
		UPBRuleNodeComment* CurComment = *CommentIter;
		if ( CurComment )
		{
			FacadeHelper->AllRules.AddItem( CurComment );
		}
	}

	// Iterate over each relevant rule and flag them as transactional in order to support undo/redo functionality
	for ( TArray<UPBRuleNodeBase*>::TIterator RuleIter( FacadeHelper->AllRules ); RuleIter; ++RuleIter )
	{
		UPBRuleNodeBase* CurRule = *RuleIter;
		if ( CurRule )
		{
			CurRule->SetFlags( RF_Transactional );
		}
	}

	// Flag the ruleset as transactional to support undo/redo functionality
	Ruleset->SetFlags( RF_Transactional );

	// Set the editor window title to include the ruleset being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd( "Facade_F" ), *InRuleset->GetPathName()) ) );	

	CreateControls( FALSE );

	LoadProperties();

	InitPBRuleClasses();

	UpdatePropertyWindow();

	// Shift origin so origin is roughly in the middle. Would be nice to use Viewport size, but doesn't seem initialized here...
	LinkedObjVC->Origin2D.X = 150;
	LinkedObjVC->Origin2D.Y = 300;

	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.AnimTreeBackGround"), NULL, LOAD_None, NULL);
}

WxFacade::~WxFacade()
{
	if( Ruleset )
	{
		Ruleset->bBeingEdited = FALSE;
	}
	
	SaveProperties();

	FacadeHelper->MarkPendingKill();
	FacadeTrans->Reset( TEXT("QuitFacade") );
}

void WxFacade::InitEditor()
{

}

/**
* Creates the controls for this window
*/
void WxFacade::CreateControls( UBOOL bTreeControl )
{
	WxLinkedObjEd::CreateControls( bTreeControl );

	if( PropertyWindow != NULL )
	{
		SetDockingWindowTitle( PropertyWindow, *FString::Printf( LocalizeSecure(LocalizeUnrealEd( "PropertiesCaption_F" ), *Ruleset->GetPathName()) ) );
	}

	wxMenuBar* MenuBar = new wxMenuBar();
	AppendWindowMenu( MenuBar );
	SetMenuBar( MenuBar );

	ToolBar = new WxFacadeToolBar( this, -1 );
	SetToolBar( ToolBar );
}

void WxFacade::Serialize( FArchive& Ar )
{
	WxLinkedObjEd::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << Ruleset << SelectedRules << ConnObj << PBRuleClasses << FacadeHelper << FacadeTrans;
	}
}

void WxFacade::OpenNewObjectMenu()
{
	WxMBFacadeNewNode menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxFacade::OpenObjectOptionsMenu()
{
	WxMBFacadeNodeOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}

void WxFacade::OpenConnectorOptionsMenu()
{
	WxMBFacadeConnectorOptions menu( this );
	FTrackPopupMenu tpm( this, &menu );
	tpm.Show();
}



void WxFacade::DrawObjects(FViewport* Viewport, FCanvas* Canvas)
{
	WxLinkedObjEd::DrawObjects( Viewport, Canvas );
	
	// Draw special 'root' input connector.
	if( Canvas->IsHitTesting() )
	{
		Canvas->SetHitProxy( new HLinkedObjConnectorProxy(Ruleset, LOC_OUTPUT, 0) );
	}
	DrawTile( Canvas, -LO_CONNECTOR_LENGTH, -LO_CONNECTOR_WIDTH/2, LO_CONNECTOR_LENGTH, LO_CONNECTOR_WIDTH, 0.f, 0.f, 0.f, 0.f, FLinearColor::Black );
	if( Canvas->IsHitTesting() ) 
	{
		Canvas->SetHitProxy( NULL );
	}
	
	// Draw link to root rule
	if(Ruleset->RootRule)
	{
		FIntPoint Start(0, 0);
		FIntPoint End = Ruleset->RootRule->GetConnectionLocation(LOC_INPUT, 0);
		
		FLOAT Tension = Abs<INT>(End.X - Start.X);
		FLinkedObjDrawUtils::DrawSpline(Canvas, Start, Tension * FVector2D(1,0), End, Tension * FVector2D(1,0), FColor(0,0,0), TRUE);
	}
	
	// Now draw all the other rule nodes

	// First pass - comments
	for(INT i=0; i<FacadeHelper->AllRules.Num(); i++)
	{
		UPBRuleNodeBase* Rule = FacadeHelper->AllRules(i);
		if( Rule && Rule->IsA(UPBRuleNodeComment::StaticClass()) )
		{
			Rule->DrawRuleNode( this, Viewport, Canvas, SelectedRules.ContainsItem(Rule) );			
		}
	}

	// Second pass - non-comments
	for(INT i=0; i<FacadeHelper->AllRules.Num(); i++)
	{
		UPBRuleNodeBase* Rule = FacadeHelper->AllRules(i);
		if( Rule && !Rule->IsA(UPBRuleNodeComment::StaticClass()) )
		{
			Rule->DrawRuleNode( this, Viewport, Canvas, SelectedRules.ContainsItem(Rule) );			
		}
	}
}

void WxFacade::DrawThumbnail (UObject* PreviewObject, TArray<UMaterialInterface*>& InMaterialOverrides, FViewport* Viewport, FCanvas* Canvas, const FIntRect& InRect)
{
	check(PreviewObject);
	check(Viewport);
	check(Canvas);

	if ((InRect.Max.X >=0) && (InRect.Min.X < (INT)Viewport->GetSizeX()) && (InRect.Max.Y >= 0) && (InRect.Min.Y < (INT)Viewport->GetSizeY()))
	{
		UThumbnailManager* ThumbnailManager = GUnrealEd->GetThumbnailManager();
		check(ThumbnailManager);
		// Get the rendering info for this object
		ThumbnailManager->SetMeshPreviewMaterial(InMaterialOverrides);
		FThumbnailRenderingInfo* RenderInfo = ThumbnailManager->GetRenderingInfo(PreviewObject);
		if (RenderInfo)
		{
			check(RenderInfo->Renderer);
			RenderInfo->Renderer->Draw(PreviewObject,TPT_None,
						InRect.Min.X, InRect.Min.Y, InRect.Width(), InRect.Height(), Viewport,
						Canvas,TBT_None,
						GEditor->GetUserSettings().PreviewThumbnailBackgroundColor,
						GEditor->GetUserSettings().PreviewThumbnailTranslucentMaterialBackgroundColor);
		}
	}
}

void WxFacade::UpdatePropertyWindow()
{
	if( SelectedRules.Num() )
	{
		PropertyWindow->SetObjectArray( SelectedRules, EPropertyWindowFlags::ShouldShowCategories);
	}
	else
	{
		PropertyWindow->SetObject( Ruleset, EPropertyWindowFlags::ShouldShowCategories );
	}
}

void WxFacade::EmptySelection()
{
	DuplicationCount = 0;

	SelectedRules.Empty();
	UpdatePropertyWindow();
}

void WxFacade::AddToSelection( UObject* Obj )
{
	check( Obj->IsA( UPBRuleNodeBase::StaticClass() ) );

	SelectedRules.AddUniqueItem( (UPBRuleNodeBase*)Obj );
	UpdatePropertyWindow();
}

void WxFacade::RemoveFromSelection( UObject* Obj )
{
	check( Obj->IsA( UPBRuleNodeBase::StaticClass() ) );

	SelectedRules.RemoveItem( (UPBRuleNodeBase*)Obj );
	UpdatePropertyWindow();
}

UBOOL WxFacade::IsInSelection( UObject* Obj ) const
{
	check( Obj->IsA( UPBRuleNodeBase::StaticClass() ) );

	return SelectedRules.ContainsItem( ( UPBRuleNodeBase* )Obj );
}

INT WxFacade::GetNumSelected() const
{
	return SelectedRules.Num();
}

void WxFacade::SetSelectedConnector( FLinkedObjectConnector& Connector )
{
	check( Connector.ConnObj->IsA( UPBRuleNodeBase::StaticClass() ) || Connector.ConnObj->IsA( UProcBuildingRuleset::StaticClass() ) );

	ConnObj = Connector.ConnObj;
	ConnType = Connector.ConnType;
	ConnIndex = Connector.ConnIndex;
}

FIntPoint WxFacade::GetSelectedConnLocation( FCanvas* Canvas )
{
	// If no ConnNode, return origin. This works in the case of connecting a node to the 'root'.
	if( !ConnObj )
	{
		return FIntPoint( 0, 0 );
	}

	// Special case of connection from 'root' connector.
	if( ConnObj == Ruleset )
	{
		return FIntPoint( 0, 0 );
	}

	UPBRuleNodeBase* Rule = CastChecked<UPBRuleNodeBase>( ConnObj );

	return Rule->GetConnectionLocation( ConnType, ConnIndex );
}

INT WxFacade::GetSelectedConnectorType()
{
	return ConnType;
}


void WxFacade::MakeConnectionToConnector( FLinkedObjectConnector& Connector )
{
	// Avoid connections to yourself.
	if( Connector.ConnObj == ConnObj )
	{
		return;
	}

	// Handle special case of connecting a node to the 'root'.
	if( Connector.ConnObj == Ruleset )
	{
		if( ConnType == LOC_INPUT )
		{
			check( Connector.ConnType == LOC_OUTPUT );
			check( Connector.ConnIndex == 0 );

			BeginFacadeTransaction( *LocalizeUnrealEd("Facade_ConnectToRoot") );
			UPBRuleNodeBase* Rule = CastChecked<UPBRuleNodeBase>( ConnObj );
			Ruleset->Modify();
			Ruleset->RootRule = Rule;

			Ruleset->PostEditChange();
			EndFacadeTransaction();
		}
		return;
	}
	else if( ConnObj == Ruleset )
	{
		if( Connector.ConnType == LOC_INPUT )
		{
			check( ConnType == LOC_OUTPUT );
			check( ConnIndex == 0 );

			BeginFacadeTransaction( *LocalizeUnrealEd("Facade_ConnectToRoot") );
			UPBRuleNodeBase* Rule = CastChecked<UPBRuleNodeBase>( Connector.ConnObj );
			Ruleset->Modify();
			Ruleset->RootRule = Rule;

			Ruleset->PostEditChange();
			EndFacadeTransaction();
		}
		return;
	}

	// Normal case - connecting an input of one node to the output of another.
	if( ConnType == LOC_OUTPUT && Connector.ConnType == LOC_INPUT )
	{
		check( Connector.ConnIndex == 0 );

		UPBRuleNodeBase* FromRule = CastChecked<UPBRuleNodeBase>( ConnObj );
		UPBRuleNodeBase* ToRule = CastChecked<UPBRuleNodeBase>( Connector.ConnObj );
		ConnectRules( FromRule, ConnIndex, ToRule );
	}
	else if( ConnType == LOC_INPUT && Connector.ConnType == LOC_OUTPUT )
	{
		check( ConnIndex == 0 );

		UPBRuleNodeBase* FromRule = CastChecked<UPBRuleNodeBase>( Connector.ConnObj );
		UPBRuleNodeBase* ToRule = CastChecked<UPBRuleNodeBase>( ConnObj );
		ConnectRules( FromRule, Connector.ConnIndex, ToRule );
	}
}

void WxFacade::MakeConnectionToObject( UObject* EndObj )
{
}


void WxFacade::AltClickConnector( FLinkedObjectConnector& Connector )
{
	wxCommandEvent DummyEvent( 0, IDM_FACADE_BREAK_LINK );
	OnContextBreakLink( DummyEvent );
}

void WxFacade::ClickedLine(FLinkedObjectConnector &Src, FLinkedObjectConnector &Dest)
{
	// Alt-click is break link
	UBOOL bAltDown = (LinkedObjVC->Viewport->KeyState(KEY_LeftAlt) || LinkedObjVC->Viewport->KeyState(KEY_RightAlt));
	if(bAltDown)
	{
		SetSelectedConnector(Src);
	
		wxCommandEvent DummyEvent( 0, IDM_FACADE_BREAK_LINK );
		OnContextBreakLink( DummyEvent );	
	}
}

void WxFacade::DoubleClickedObject(UObject* Obj)
{
	UPBRuleNodeSubRuleset* SubRuleNode = Cast<UPBRuleNodeSubRuleset>(Obj);
	if(SubRuleNode && SubRuleNode->SubRuleset)
	{
		if(SubRuleNode->SubRuleset->bBeingEdited)
		{
			// If being edited, find the window and restore it/put to front
			TArray<FTrackableEntry> TrackableWindows;
			WxTrackableWindow::GetTrackableWindows( TrackableWindows );
			for(INT WinIdx=0; WinIdx<TrackableWindows.Num(); WinIdx++)
			{
				wxWindow* Window = TrackableWindows(WinIdx).Window;
				WxFacade* Facade = wxDynamicCast(Window, WxFacade);
				if(Facade && Facade->Ruleset == SubRuleNode->SubRuleset)
				{
					Facade->Raise();
					Facade->Maximize(false);
				}
			}
		}
		else
		{
			WxFacade* Facade = new WxFacade( (wxWindow*)GApp->EditorFrame, -1, SubRuleNode->SubRuleset );
			Facade->Show(1);
		}
	}

	UPBRuleNodeMesh* MeshNode = Cast<UPBRuleNodeMesh>(Obj);
	if(MeshNode && MeshNode->BuildingMeshes.Num() > 0)
	{
		UStaticMesh* Mesh = MeshNode->BuildingMeshes(0).Mesh;
		if(Mesh)
		{
			WxStaticMeshEditor* StaticMeshEditor = new WxStaticMeshEditor( GApp->EditorFrame, -1, Mesh, FALSE /*bForceSimplificationWindowVisible*/ );
			StaticMeshEditor->Show(1);
		}
	}
	
}

UBOOL WxFacade::ClickOnBackground()
{
	UPBRuleNodeBase* NewRuleNode = NewShortcutObject();
	if(NewRuleNode)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


void WxFacade::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for( INT i=0; i<SelectedRules.Num(); i++ )
	{
		UPBRuleNodeBase* Rule = SelectedRules(i);
		if(Rule)
		{
			Rule->RulePosX += DeltaX;
			Rule->RulePosY += DeltaY;		
		}
	}
}

void WxFacade::EdHandleKeyInput( FViewport* Viewport, FName Key, EInputEvent Event )
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	if( Event == IE_Pressed )
	{
		if( Key == KEY_Delete )
		{
			DeleteSelectedRules();
		}
		else if( (bCtrlDown && Key == KEY_W) || (bShiftDown && Key == KEY_D) )
		{
			BeginFacadeTransaction( *LocalizeUnrealEd("Facade_DuplicateSelectedRules") );
			// preserve the duplication count despite the selection change
			// (apologies for the crappiness of this code, such a nightmare to fix w/o breaking normal pasting, grr)
			INT OldDuplicationCount = DuplicationCount;
			Copy();
			PasteCount = OldDuplicationCount;
			Paste();
			DuplicationCount = ++OldDuplicationCount;
			EndFacadeTransaction();
		}
		else if( Key == KEY_R )
		{
			ReapplyRuleset();
		}
		else if( bCtrlDown && Key == KEY_C )
		{
			Copy();
		}
		else if( bCtrlDown && Key == KEY_V )
		{
			BeginFacadeTransaction( *LocalizeUnrealEd("Facade_PasteRule") );
			Paste();
			EndFacadeTransaction();
		}		
		else if( bCtrlDown && Key == KEY_Right )
		{
			SelectRulesAfterSelected();
		}
		else if ( bCtrlDown && Key == KEY_Z )
		{
			FacadeUndo();
		}
		else if ( bCtrlDown && Key == KEY_Y )
		{
			FacadeRedo();
		}
	}
}

void WxFacade::SpecialDrag( INT DeltaX, INT DeltaY, INT NewX, INT NewY, INT SpecialIndex )
{
	// Can only 'special drag' one object at a time.
	if(SelectedRules.Num() != 1)
	{
		return;
	}

	UPBRuleNodeComment* Comment = Cast<UPBRuleNodeComment>(SelectedRules(0));
	if(Comment)
	{
		if(SpecialIndex == 1)
		{
			Comment->SizeX += DeltaX;
			Comment->SizeX = ::Max<INT>(Comment->SizeX, 64);

			Comment->SizeY += DeltaY;
			Comment->SizeY = ::Max<INT>(Comment->SizeY, 64);
		}
	}
}

void WxFacade::NotifyObjectsChanged()
{
	if (Ruleset != NULL)
	{
		Ruleset->MarkPackageDirty();
	}
}

void WxFacade::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	GEditor->EndTransaction();

	// If the ruleset is being edited (ie no rules are selected), make variation nodes update, as we may have added one
	if(SelectedRules.Num() == 0)
	{
		for(INT RuleIdx=0; RuleIdx<FacadeHelper->AllRules.Num(); RuleIdx++)
		{
			UPBRuleNodeVariation* VarRule = Cast<UPBRuleNodeVariation>( FacadeHelper->AllRules(RuleIdx) );
			if(VarRule)
			{
				VarRule->RegenVariationOutputs(Ruleset);
			}
		}
	}

	RefreshViewport();
}

/** Connect FromConnIndex output connector on FromNode to the input of ToNode */
void WxFacade::ConnectRules( UPBRuleNodeBase* FromRule, INT FromConnIndex, UPBRuleNodeBase* ToRule )
{
	check( (FromConnIndex >= 0) && (FromConnIndex < FromRule->NextRules.Num()) );

	// Check for loops - ie make sure that 'to' does not already reference 'from' somehow
	TArray<UPBRuleNodeBase*> Nodes;
	ToRule->GetRuleNodes(Nodes);
	if(Nodes.ContainsItem(FromRule))
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_PBLoopDetected"));
		return;
	}

	BeginFacadeTransaction( *LocalizeUnrealEd("Facade_ConnectRules") );
	FromRule->Modify();
	FromRule->NextRules(FromConnIndex).NextRule = ToRule;

	FromRule->PostEditChange();
	EndFacadeTransaction();

	UpdatePropertyWindow();
	RefreshViewport();
}

/** Delete all selected rule nodes - will clear connections to them */
void WxFacade::DeleteSelectedRules()
{
	BeginFacadeTransaction( *LocalizeUnrealEd("Facade_DeleteSelectedRules") );

	// For each selected rule node..
	for( INT i=0; i<SelectedRules.Num(); i++ )
	{
		UPBRuleNodeBase* Rule = SelectedRules(i);
		
		// .. break all links to it
		BreakLinksToRule(Rule);		

		// .. and remove from the AllRules set
		FacadeHelper->AllRules.RemoveItem(Rule);
		
		// .. and remove from comments array, if it is one
		UPBRuleNodeComment* Comment = Cast<UPBRuleNodeComment>(Rule);
		if(Comment)
		{
			Ruleset->Comments.RemoveItem(Comment);
		}

		// Do nothing more-  should be no refs, will not be saved		
	}

	SelectedRules.Empty();
	EndFacadeTransaction();

	UpdatePropertyWindow();
	RefreshViewport();
}

/** Select all rules after the supplied one */
void WxFacade::SelectRulesAfterSelected()
{
	TArray<class UPBRuleNodeBase*> CurrentSelection;

	// If we have something selected..
	if(SelectedRules.Num() > 0)
	{
		CurrentSelection = SelectedRules;

		// clear current selection
		EmptySelection();

		// for each selected node
		for(INT SelIdx=0; SelIdx<CurrentSelection.Num(); SelIdx++)
		{
			UPBRuleNodeBase* FromNode = CurrentSelection(SelIdx);
			if(FromNode)
			{
				// Get all nodes following it
				TArray<UPBRuleNodeBase*> AfterNodes;
				FromNode->GetRuleNodes(AfterNodes);

				// Add to selection
				for (INT RuleIdx=0; RuleIdx<AfterNodes.Num(); RuleIdx++)
				{
					AddToSelection( AfterNodes(RuleIdx) );
				}
			}
		}

		// once all done, update viewport/property window
		UpdatePropertyWindow();
		RefreshViewport();
	}
}

/** Util for breaking all links to a specified rule */
void WxFacade::BreakLinksToRule(UPBRuleNodeBase* Rule, UBOOL bBreakNextRulesLinks /*= TRUE*/ )
{
	// Break output/child links, if requested
	if ( bBreakNextRulesLinks )
	{
		Rule->Modify();
		for ( INT RuleIndex = 0; RuleIndex < Rule->NextRules.Num(); ++RuleIndex )
		{
			Rule->NextRules(RuleIndex).NextRule = NULL;
		}
	}

	// Break root rule connection if it goes to this node.
	if(Ruleset && Ruleset->RootRule == Rule)
	{
		Ruleset->Modify();
		Ruleset->RootRule = NULL;
		Ruleset->PostEditChange();
	}

	// Look over all rules..
	for(INT j=0; j<FacadeHelper->AllRules.Num(); j++)
	{
		UPBRuleNodeBase* TestRule = FacadeHelper->AllRules(j);
		// Look over all connections
		for(INT k=0; k<TestRule->NextRules.Num(); k++)
		{
			// If this goes to Rule, set to zero
			if(TestRule->NextRules(k).NextRule == Rule)
			{
				TestRule->Modify();
				TestRule->NextRules(k).NextRule = NULL;
				TestRule->PostEditChange();
			}
		}
	}
}


/** Remesh all building actors in level using edited ruleset */
void WxFacade::ReapplyRuleset()
{
	for( FActorIterator It; It; ++It )
	{
		AProcBuilding* Building = Cast<AProcBuilding>(*It);
		if(Building && !Building->IsTemplate())
		{
			UBOOL bRegen = FALSE;
			UProcBuildingRuleset* BuildingRuleset = Building->GetRuleset();

			// Get all rulesets referenced by the ruleset on the building
			TArray<UProcBuildingRuleset*> RefdRulesets;
			if(BuildingRuleset)
			{
				BuildingRuleset->GetReferencedRulesets(RefdRulesets);
			}

			// If this is main ruleset, of ref's this ruleset, regen
			if((BuildingRuleset == Ruleset) || (RefdRulesets.ContainsItem(Ruleset)))
			{
				bRegen = TRUE;
			}

			// Regen if desired
			if(bRegen)
			{
				AProcBuilding* BuildingToUpdate = Building->GetBaseMostBuilding();
				GEngine->DeferredCommands.AddUniqueItem(FString::Printf(TEXT("ProcBuildingUpdate %s"), *BuildingToUpdate->GetPathName()));				
			}
		}
	}
}

/** Called when mouse is clicked on background, to see if we want to make a new 'shortcut' object (key+click) */
UPBRuleNodeBase* WxFacade::NewShortcutObject()
{
	if( LinkedObjVC->Viewport->KeyState(KEY_M) )
	{
		return CreateNewRuleOfClass(UPBRuleNodeMesh::StaticClass());
	}
	
	return NULL;
}

/** Create a new rule of the given class, at current cursor location */
UPBRuleNodeBase* WxFacade::CreateNewRuleOfClass(UClass* NewRuleClass)
{
	if( !NewRuleClass )
	{
		debugf(TEXT("CreateNewRuleOfClass: No class supplied"));
		return NULL;
	}

	if( !NewRuleClass->IsChildOf(UPBRuleNodeBase::StaticClass()) )
	{
		debugf(TEXT("CreateNewRuleOfClass: '%s' is not child of UPBRuleNodeBase"), *NewRuleClass->GetName());
		return NULL;
	}

	BeginFacadeTransaction( *LocalizeUnrealEd("Facade_CreateNewRule") );
	UPBRuleNodeBase* NewRule = ConstructObject<UPBRuleNodeBase>( NewRuleClass, Ruleset, NAME_None, RF_Transactional );
	check(NewRule);

	// Set position to under cursor
	NewRule->RulePosX = ( LinkedObjVC->NewX - LinkedObjVC->Origin2D.X ) / LinkedObjVC->Zoom2D;
	NewRule->RulePosY = ( LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y ) / LinkedObjVC->Zoom2D;

	// Auto-fill in Mesh if
	UPBRuleNodeMesh* MeshNode = Cast<UPBRuleNodeMesh>(NewRule);
	if(MeshNode)
	{
		GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
		UStaticMesh* SelectedMesh = GEditor->GetSelectedObjects()->GetTop<UStaticMesh>();
		if(SelectedMesh)
		{
			INT NewMeshIndex = MeshNode->BuildingMeshes.Add();
			MeshNode->BuildingMeshes(NewMeshIndex).InitToDefaults();
			MeshNode->BuildingMeshes(NewMeshIndex).Mesh = SelectedMesh;
		}
	}

	Ruleset->Modify();

	// Let node do things when created
	NewRule->RuleNodeCreated(Ruleset);

	// If its a comment, add to the special array for saving them
	UPBRuleNodeComment* Comment = Cast<UPBRuleNodeComment>(NewRule);
	if(Comment)
	{
		Ruleset->Comments.AddItem(Comment);
	}

	// Add to set of all visible rule nodes
	FacadeHelper->AllRules.AddItem(NewRule);

	// We set the selection to being the newly created nodes.
	EmptySelection();
	AddToSelection(NewRule);

	RefreshViewport();
	
	EndFacadeTransaction();

	return NewRule;
}

/** Break link at a specific connector */
void WxFacade::BreakConnection(UObject* BreakConnObj, INT BreakConnType, INT BreakConnIndex)
{
	BeginFacadeTransaction( *LocalizeUnrealEd("Facade_BreakConnection") );

	// Breaking an output link
	if(BreakConnType == LOC_OUTPUT)
	{
		// Handle special root connector
		if(BreakConnObj == Ruleset)
		{
			Ruleset->Modify();
			Ruleset->RootRule = NULL;
		}
		// Regular node output
		else
		{
			UPBRuleNodeBase* Rule = CastChecked<UPBRuleNodeBase>(BreakConnObj);
			check(BreakConnIndex >= 0 && BreakConnIndex < Rule->NextRules.Num());
			Rule->Modify();
			Rule->NextRules(BreakConnIndex).NextRule = NULL;
			Rule->PostEditChange();
		}
	}
	// Breaking an input link
	else if(BreakConnType == LOC_INPUT)
	{
		// Root connection is only an output!
		check(BreakConnObj != Ruleset);

		UPBRuleNodeBase* Rule = CastChecked<UPBRuleNodeBase>(BreakConnObj);

		// Rules only have 1 input, so just break all links to this rule
		BreakLinksToRule(Rule, FALSE);		
	}

	EndFacadeTransaction();
}


/** Export selected sequence objects to text and puts into Windows clipboard. */
void WxFacade::Copy()
{
	// Iterate over all objects making sure they import/export flags are unset. 
	// These are used for ensuring we export each object only once etc.
	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagImp | RF_TagExp );
	}

	FStringOutputDevice Ar;
	const FExportObjectInnerContext Context;
	for(INT i=0; i<SelectedRules.Num(); i++)
	{
		UExporter::ExportToOutputDevice( &Context, SelectedRules(i), NULL, Ar, TEXT("copy"), 0, PPF_ExportsNotFullyQualified );
	}

	appClipboardCopy( *Ar );

	PasteCount = 0;
}

/** 
*	Take contents of windows clipboard and use USequenceFactory to create new SequenceObjects (possibly new subsequences as well). 
*	If bAtMousePos is TRUE, it will move the objects so the top-left corner of their bounding box is at the current mouse position (NewX/NewY in LinkedObjVC)
*/
void WxFacade::Paste()
{
	INT PasteOffset = 30;

	PasteCount++;

	// Get pasted text.
	FString PasteString = appClipboardPaste();
	const TCHAR* Paste = *PasteString;

	ULinkedObjectFactory* Factory = new ULinkedObjectFactory;
	Factory->AllowedCreateClass = UPBRuleNodeBase::StaticClass();
	Factory->FactoryCreateText( NULL, Ruleset, NAME_None, RF_Transactional, NULL, TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );

	// Select the newly pasted stuff, and offset a bit.
	EmptySelection();

	for ( USelection::TObjectIterator It( GEditor->GetSelectedObjects()->ObjectItor() ) ; It ; ++It )
	{
		if ((*It)->IsA(UPBRuleNodeBase::StaticClass()))
		{
			AddToSelection(*It);
		}
	}

	// If we want to paste the copied objects at the current mouse position (NewX/NewY in LinkedObjVC) and we actually pasted something...
	if(SelectedRules.Num() > 0)
	{
		// Find where we want the top-left corner of selection to be.
		INT DesPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
		INT DesPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
		// By default offset pasted objects by the default value.
		// This is a fallback case for SequenceClasses that have a DrawHeight or DrawWidth of 0.
		INT DeltaX = PasteCount * PasteOffset;
		INT DeltaY = PasteCount * PasteOffset;

		// Apply to all selected objects.
		for(INT i=0; i<SelectedRules.Num(); i++)
		{
			SelectedRules(i)->RulePosX += DeltaX;
			SelectedRules(i)->RulePosY += DeltaY;

			FacadeHelper->AllRules.AddItem(SelectedRules(i));

			// Track if its a comment rule
			UPBRuleNodeComment* Comment = Cast<UPBRuleNodeComment>(SelectedRules(i));
			if(Comment)
			{
				Ruleset->Comments.AddItem(Comment);
			}
		}
	}

	// Update property window to reflect new selection.
	UpdatePropertyWindow();

	RefreshViewport();
	NotifyObjectsChanged();
}

/**
 * Begin a transaction within Facade that should be tracked for undo/redo purposes.
 *
 * @param	SessionName	Name of this transaction session
 */
void WxFacade::BeginFacadeTransaction( const TCHAR* SessionName )
{
	FacadeTrans->Begin( SessionName );
	FacadeHelper->Modify();
}

/** End a transaction within Facade */
void WxFacade::EndFacadeTransaction()
{
	FacadeTrans->End();
}

/** Undo the last Facade transaction, if possible */
void WxFacade::FacadeUndo()
{
	FacadeTrans->Undo();
	RefreshViewport();
}

/** Redo the last Facade transaction, if possible */
void WxFacade::FacadeRedo()
{
	FacadeTrans->Redo();
	RefreshViewport();
}

//////////////////////////////////////////////////////////////////////////
// Context menu handlers

void WxFacade::OnSelectDownsteamNodes( wxCommandEvent& In )
{
	TArray<UPBRuleNodeBase*> RulesToEvaluate;	// Graph nodes that need to be traced downstream
	TArray<UPBRuleNodeBase*> RulesEvalated;		// Keep track of evaluated graph nodes so we don't re-evaluate them
	TArray<UPBRuleNodeBase*> RulesToSelect;		// Downstream graph nodes that will end up being selected

	// Add currently selected graph nodes to the "need to be traced downstream" list
	for ( TArray<UPBRuleNodeBase*>::TConstIterator RuleIter(SelectedRules); RuleIter; ++RuleIter )
	{
		RulesToEvaluate.AddItem(*RuleIter);
	}

	// Generate a list of downstream nodes
	while (RulesToEvaluate.Num() > 0)
	{
		UPBRuleNodeBase* CurrentRule = RulesToEvaluate.Last();
		if (CurrentRule)
		{
			for (INT RuleIndex = 0; RuleIndex < CurrentRule->NextRules.Num(); ++RuleIndex)
			{
				INT index = -1;
				UPBRuleNodeBase* NextRule = CurrentRule->NextRules(RuleIndex).NextRule;

				if (NextRule)
				{
					RulesEvalated.FindItem(NextRule, index);
					if (index < 0)
					{
						// This node is a downstream node (so, we'll need to select it) 
						// and it's children need to be traced as well
						RulesToSelect.AddItem(NextRule);
						RulesToEvaluate.AddItem(NextRule);
					}
				}
			}
		}

		// This graph node has now been examined
		RulesEvalated.AddItem(CurrentRule);
		RulesToEvaluate.RemoveItem(CurrentRule);
	}

	// Select all downstream nodes
	if (RulesToSelect.Num() > 0)
	{
		for ( TArray<UPBRuleNodeBase*>::TConstIterator RuleIter(RulesToSelect); RuleIter; ++RuleIter )
		{
			AddToSelection(*RuleIter);
		}

		UpdatePropertyWindow();
	}
}

void WxFacade::OnSelectUpsteamNodes( wxCommandEvent& In )
{
	if ( Ruleset->RootRule )
	{
		TArray<UPBRuleNodeBase*> RuleNodes;
		
		TArray<UPBRuleNodeBase*> RulesToEvaluate;	// Graph nodes that need to be traced upstream
		TArray<UPBRuleNodeBase*> RulesEvalated;		// Keep track of evaluated graph nodes so we don't re-evaluate them
		TArray<UPBRuleNodeBase*> RulesToSelect;		// Upstream graph nodes that will end up being selected

		Ruleset->RootRule->GetRuleNodes( RuleNodes );

		// Add currently selected graph nodes to the "need to be traced upstream" list
		for ( TArray<UPBRuleNodeBase*>::TConstIterator RuleIter(SelectedRules); RuleIter; ++RuleIter )
		{
			RulesToEvaluate.AddItem(*RuleIter);
		}

		// Generate a list of upstream nodes
		while (RulesToEvaluate.Num() > 0)
		{
			UPBRuleNodeBase* CurrentRule = RulesToEvaluate.Last();
			
			if (CurrentRule)
			{
				for ( TArray<UPBRuleNodeBase*>::TConstIterator RuleNodeIter( RuleNodes ); RuleNodeIter; ++RuleNodeIter )
				{
					UPBRuleNodeBase* TestRule = *RuleNodeIter;
					for (INT RuleIndex = 0; RuleIndex < TestRule->NextRules.Num(); ++RuleIndex)
					{
						INT index = -1;
						UPBRuleNodeBase* NextRule = TestRule->NextRules(RuleIndex).NextRule;
						
						if (NextRule && (NextRule == CurrentRule))
						{
							RulesEvalated.FindItem(TestRule, index);
							if (index < 0)
							{
								// This node is a upstream node (so, we'll need to select it) 
								// and it's children need to be traced as well
								RulesToSelect.AddItem(TestRule);
								RulesToEvaluate.AddItem(TestRule);
							}
						}
					}
				}
			}

			// This graph node has now been examined
			RulesEvalated.AddItem(CurrentRule);
			RulesToEvaluate.RemoveItem(CurrentRule);
		}

		// Select all upstream nodes
		if (RulesToSelect.Num() > 0)
		{
			for ( TArray<UPBRuleNodeBase*>::TConstIterator RuleIter(RulesToSelect); RuleIter; ++RuleIter )
			{
				AddToSelection(*RuleIter);
			}

			UpdatePropertyWindow();
		}
	}
}

void WxFacade::OnContextNewRule( wxCommandEvent& In )
{
	INT NewRuleClassIndex = In.GetId() - IDM_FACADE_NEW_RULE_START;	
	check( NewRuleClassIndex >= 0 && NewRuleClassIndex < PBRuleClasses.Num() );

	UClass* NewRuleClass = PBRuleClasses( NewRuleClassIndex );
	check( NewRuleClass->IsChildOf( UPBRuleNodeBase::StaticClass() ) );
	
	CreateNewRuleOfClass(NewRuleClass);
}

void WxFacade::OnContextBreakLink( wxCommandEvent& In )
{
	BreakConnection( ConnObj, ConnType, ConnIndex );
}


void WxFacade::OnReapplyRuleset( wxCommandEvent& In )
{
	ReapplyRuleset();
}

/**
 * Handle user selecting toolbar option to undo
 *
 * @param	In	Event automatically generated by wxWidgets when the user clicks the undo toolbar option
 */
void WxFacade::OnFacadeUndo( wxCommandEvent& In )
{
	FacadeUndo();
}

/**
 * Handle user selecting toolbar option to redo
 *
 * @param	In	Event automatically generated by wxWidgets when the user clicks the redo toolbar option
 */
void WxFacade::OnFacadeRedo( wxCommandEvent& In )
{
	FacadeRedo();
}

/**
 * Update the UI for the undo toolbar option
 *
 * @param	In	Event automatically generated by wxWidgets to update the UI
 */
void WxFacade::UpdateFacadeUndoUI( wxUpdateUIEvent& In )
{
	In.Enable( FacadeTrans->CanUndo() == TRUE );
}

/**
 * Update the UI for the redo toolbar option
 *
 * @param	In	Event automatically generated by wxWidgets to update the UI
 */
void WxFacade::UpdateFacadeRedoUI( wxUpdateUIEvent& In )
{
	In.Enable( FacadeTrans->CanRedo() == TRUE );
}

//////////////////////////////////////////////////////////////////////////
// WxFacadeToolBar


WxFacadeToolBar::WxFacadeToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{	
	ReApplyB.Load( TEXT("MaterialEditor_Apply") );
	UndoB.Load( TEXT("Facade_Undo.png") );
	RedoB.Load( TEXT("Facade_Redo.png") );

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddSeparator();
	AddTool(IDM_FACADE_REAPPLY, ReApplyB, *LocalizeUnrealEd("FacadeRepplyRuleset"));
	AddSeparator();
	AddTool( IDM_FACADE_UNDO, UndoB, *LocalizeUnrealEd("Undo") );
	AddTool( IDM_FACADE_REDO, RedoB, *LocalizeUnrealEd("Redo") );
	AddSeparator();

	Realize();
}

WxFacadeToolBar::~WxFacadeToolBar()
{
}
