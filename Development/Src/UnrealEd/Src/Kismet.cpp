/*=============================================================================
	Kismet.cpp: Gameplay sequence editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"

static INT	MultiEventSpacing = 100;
static INT	PasteOffset = 30;
static INT	FitCommentToSelectedBorder = 15;

IMPLEMENT_CLASS(UKismetBindings);

/* epic ===============================================
* static FindOutputLinkTo
*
* Finds all of the ops in the sequence's SequenceObjects
* list that contains an output link to the specified
* op's input.
*
* Returns TRUE to indicate at least one output was
* returned.
*
* =====================================================
*/
UBOOL WxKismet::FindOutputLinkTo(const USequence *sequence, const USequenceOp *targetOp, const INT inputIdx, TArray<USequenceOp*> &outputOps, TArray<INT> &outputIndices)
{
	outputOps.Empty();
	outputIndices.Empty();
	// iterate through all sequence objects in the sequence,
	for (INT objIdx = 0; objIdx < sequence->SequenceObjects.Num(); objIdx++)
	{
		// if this is an op
		USequenceOp *chkOp = Cast<USequenceOp>(sequence->SequenceObjects(objIdx));
		if (chkOp != NULL)
		{
			// iterate through each of the outputs
			for (INT linkIdx = 0; linkIdx < chkOp->OutputLinks.Num(); linkIdx++)
			{
				// iterate through each input linked to this output
				UBOOL bFoundMatch = 0;
				for (INT linkInputIdx = 0; linkInputIdx < chkOp->OutputLinks(linkIdx).Links.Num() && !bFoundMatch; linkInputIdx++)
				{
					// if this matches the op
					if (chkOp->OutputLinks(linkIdx).Links(linkInputIdx).LinkedOp == targetOp &&
						chkOp->OutputLinks(linkIdx).Links(linkInputIdx).InputLinkIdx == inputIdx)
					{
						// add to the list
						outputOps.AddItem(chkOp);
						outputIndices.AddItem(linkIdx);
						bFoundMatch = 1;
					}
				}
			}
		}
	}
	return (outputOps.Num() > 0);
}

void WxKismet::OpenNewObjectMenu()
{
	wxMenu* menu = new WxMBKismetNewObject(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxKismet::OpenObjectOptionsMenu()
{
	wxMenu* menu = new WxMBKismetObjectOptions(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxKismet::OpenConnectorOptionsMenu()
{
	wxMenu* menu = new WxMBKismetConnectorOptions(this);

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

void WxKismet::DoubleClickedObject(UObject* Obj)
{
	USeqAct_Interp* InterpAct = Cast<USeqAct_Interp>(Obj);
	if(InterpAct)
	{
		// Release the mouse before we pop up the Matinee window.
		LinkedObjVC->Viewport->LockMouseToWindow(false);
		LinkedObjVC->Viewport->Invalidate();

		// Open matinee to edit the selected Interp action.
		OpenMatinee(InterpAct);
	}
	else
	{
		// if double clicking on an object variable
		if (Obj->IsA(USeqVar_Object::StaticClass()))
		{
			USeqVar_Object *objVar = (USeqVar_Object*)Obj;
			if (objVar->ObjValue != NULL &&
				objVar->ObjValue->IsA(AActor::StaticClass()))
			{
				AActor *actor = (AActor*)(objVar->ObjValue);
				// select the actor and
				// move the camera to a view of this actor
				GUnrealEd->SelectNone( GUnrealEd->Level, 0 );
				GUnrealEd->Exec(*FString::Printf(TEXT("CAMERA ALIGN NAME=%s"),actor->GetName()));
				GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
			}
		}
		else
		// if double clicking on an event
		if (Obj->IsA(USequenceEvent::StaticClass()))
		{
			// select the associated actor and move the camera
			USequenceEvent *evt = (USequenceEvent*)(Obj);
			if (evt->Originator != NULL)
			{
				GUnrealEd->SelectNone( GUnrealEd->Level, 0 );
				GUnrealEd->Exec(*FString::Printf(TEXT("CAMERA ALIGN NAME=%s"),evt->Originator->GetName()));
				GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
			}
		}
		else
		// if double clicking on a sequence
		if (Obj->IsA(USequence::StaticClass()))
		{
			// make it the active sequence
			ChangeActiveSequence((USequence*)Obj);
			GEditor->Trans->Reset( TEXT("OpenSequence") );
		}
	}
}

/** 
 *	Executed when user clicks and releases on background without moving mouse much. 
 *	We use the Kismet key bindings to see if we should spawn an object now.
 *	If we return 'true
 */
UBOOL WxKismet::ClickOnBackground()
{
	UClass* NewSeqObjClass = NULL;

	// Use default object (properties set from ..Editor.ini) for bindings.
	UKismetBindings* Bindings = (UKismetBindings*)(UKismetBindings::StaticClass()->GetDefaultObject());
	UBOOL bCtrlDown = (LinkedObjVC->Viewport->KeyState(KEY_LeftControl) || LinkedObjVC->Viewport->KeyState(KEY_RightControl));

	// Iterate over each bind and see if we are holding those keys down.
	for(INT i=0; i<Bindings->Bindings.Num() && !NewSeqObjClass; i++)
	{
		FKismetKeyBind& Bind = Bindings->Bindings(i);
		UBOOL bKeyCorrect = LinkedObjVC->Viewport->KeyState( Bind.Key );
		UBOOL bCtrlCorrect = (bCtrlDown == Bind.bControl);
		if(bKeyCorrect && bCtrlCorrect)
		{
			NewSeqObjClass = Bind.SeqObjClass;
		}
	}

	// If we got a class, create a new object.
	if(NewSeqObjClass)
	{
		// Slight hack special case for creating subsequences from selected...
		if( NewSeqObjClass == USequence::StaticClass() )
		{
			// Unlock mouse from viewport so you can move cursor to 'New Sequence Name' diaog.
			LinkedObjVC->Viewport->LockMouseToWindow(false);

			OnContextCreateSequence( wxCommandEvent() );
		}
		else
		{
			INT NewPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
			INT NewPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

			// Create new object. This will clear current selection and select the new object.
			NewSequenceObject(NewSeqObjClass, NewPosX, NewPosY);
		}

		// Return false to avoid releasing current selection.
		return false;
	}
	else
	{
		// Did nothing, release current selection (default behaviour).
		return true;
	}
}


void WxKismet::DrawLinkedObjects(FRenderInterface* RI)
{
	WxLinkedObjEd::DrawLinkedObjects(RI);

	USequenceObject* MouseOverSeqObj = NULL;
	if(LinkedObjVC->MouseOverObject)
	{
		MouseOverSeqObj = CastChecked<USequenceObject>(LinkedObjVC->MouseOverObject);
	}

	Sequence->DrawSequence(RI, SelectedSeqObjs, MouseOverSeqObj, LinkedObjVC->MouseOverConnType, LinkedObjVC->MouseOverConnIndex, appSeconds() - LinkedObjVC->MouseOverTime, bDrawCurves);
}

void WxKismet::UpdatePropertyWindow()
{
	PropertyWindow->SetObjectArray( (TArray<UObject*>*)&SelectedSeqObjs, 1,1,0 );
}

void WxKismet::EmptySelection()
{
	SelectedSeqObjs.Empty();
}

void WxKismet::AddToSelection( UObject* Obj )
{
	check( Obj->IsA(USequenceObject::StaticClass()) );
	SelectedSeqObjs.AddUniqueItem( (USequenceObject*)Obj );
	((USequenceObject*)Obj)->OnSelected();
}

void WxKismet::RemoveFromSelection( UObject* Obj )
{
	check( Obj->IsA(USequenceObject::StaticClass()) );
	SelectedSeqObjs.RemoveItem( (USequenceObject*)Obj );
}

UBOOL WxKismet::IsInSelection( UObject* Obj )
{
	check( Obj->IsA(USequenceObject::StaticClass()) );

	return SelectedSeqObjs.ContainsItem( (USequenceObject*)Obj );
}

UBOOL WxKismet::HaveObjectsSelected()
{
	return( SelectedSeqObjs.Num() > 0 );
}

void WxKismet::SetSelectedConnector( UObject* InConnObj, INT InConnType, INT InConnIndex )
{
	check( InConnObj->IsA(USequenceOp::StaticClass()) );

	ConnSeqOp = (USequenceOp*)InConnObj;
	ConnType = InConnType;
	ConnIndex = InConnIndex;
}

FIntPoint WxKismet::GetSelectedConnLocation(FRenderInterface* RI)
{
	if(!ConnSeqOp)
		return FIntPoint(0,0);

	return ConnSeqOp->GetConnectionLocation(ConnType, ConnIndex);
}

INT WxKismet::GetSelectedConnectorType()
{
	return ConnType;
}

UBOOL WxKismet::ShouldHighlightConnector(UObject* InObj, INT InConnType, INT InConnIndex)
{
	// Always highlight when not making a line.
	if(!LinkedObjVC->bMakingLine)
	{
		return true;
	}

	// If making line from variable of event, can never link to another type of connector.
	if(ConnType == LOC_VARIABLE || ConnType == LOC_EVENT)
	{
		return false;
	}

	// If making line from input/output, can't link to var/event connector
	if(InConnType == LOC_VARIABLE || InConnType == LOC_EVENT)
	{
		return false;
	}

	// Can't link connectors of the same type.
	if(InConnType == ConnType)
	{
		return false;
	}

	// Can't link to yourself.
	if( InObj == ConnSeqOp && 
		ConnType == InConnType && 
		ConnIndex == InConnIndex)
	{
		return false;
	}

	return true;
}

FColor WxKismet::GetMakingLinkColor()
{
	if(!LinkedObjVC->bMakingLine)
	{
		// Shouldn't really be here!
		return FColor(0,0,0);
	}

	if(ConnType == LOC_INPUT || ConnType == LOC_OUTPUT)
	{
		return FColor(0,0,0);
	}
	else if(ConnType == LOC_EVENT)
	{
		// The constant comes from MouseOverColorScale in UnSequenceDraw.cpp. Bit hacky - sorry!
		return FColor( FColor(255,0,0).Plane() * 0.8f );
	}
	else // variable connector
	{
		return FColor( ConnSeqOp->GetVarConnectorColor(ConnIndex).Plane() * 0.8f );
	}
}

void WxKismet::MakeConnectionToConnector( UObject* EndConnObj, INT EndConnType, INT EndConnIndex )
{
	check( EndConnObj->IsA(USequenceOp::StaticClass()) );

	if(ConnType == LOC_INPUT && EndConnType == LOC_OUTPUT)
		MakeLogicConnection((USequenceOp*)EndConnObj, EndConnIndex, ConnSeqOp, ConnIndex);
	else if(ConnType == LOC_OUTPUT && EndConnType == LOC_INPUT)
		MakeLogicConnection(ConnSeqOp, ConnIndex, (USequenceOp*)EndConnObj, EndConnIndex);
}

void WxKismet::MakeConnectionToObject( UObject* EndObj )
{
	if (ConnType == LOC_VARIABLE)
	{
		USequenceVariable* SeqVar = Cast<USequenceVariable>(EndObj);
		if (SeqVar != NULL)
		{
			MakeVariableConnection(ConnSeqOp, ConnIndex, SeqVar);
		}
	}
	else
	if (ConnType == LOC_EVENT)
	{
		USequenceEvent *SeqEvt = Cast<USequenceEvent>(EndObj);
		if (SeqEvt != NULL)
		{
			ConnSeqOp->EventLinks(ConnIndex).LinkedEvents.AddUniqueItem(SeqEvt);
			SeqEvt->OnConnect(ConnSeqOp,ConnIndex);
		}
	}
}

/* epic ===============================================
* ::PositionSelectedObjects
*
* Called once the mouse is released after moving objects,
* used to snap things to the grid.
*
* =====================================================
*/
void WxKismet::PositionSelectedObjects()
{
	for (INT idx = 0; idx < SelectedSeqObjs.Num(); idx++)
	{
		USequenceObject *seqObj = SelectedSeqObjs(idx);
		seqObj->SnapPosition(KISMET_GRIDSIZE);
	}
}

/* epic ===============================================
* ::MoveSelectedObjects
*
* Updates the position of all objects selected with
* mouse movement.
*
* =====================================================
*/
void WxKismet::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for(INT i=0; i<SelectedSeqObjs.Num(); i++)
	{
		USequenceObject* SeqObj = SelectedSeqObjs(i);

		SeqObj->ObjPosX += DeltaX;
		SeqObj->ObjPosY += DeltaY;
	}
}

void WxKismet::EdHandleKeyInput(FChildViewport* Viewport, FName Key, EInputEvent Event)
{
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);
	UBOOL bAltDown = Viewport->KeyState(KEY_LeftAlt) || Viewport->KeyState(KEY_RightAlt);

	if( Event == IE_Pressed )
	{
		if( Key == KEY_SpaceBar )
		{
			SingleTickSequence();
		}
		else if( Key == KEY_Delete )
		{
			DeleteSelected();
		}
		else if( (bCtrlDown && Key == KEY_W) || (bShiftDown && Key == KEY_D) )
		{
			KismetCopy();
			KismetPaste();
		}
		else if( Key == KEY_BackSpace )
		{
			OnButtonParentSequence( wxCommandEvent() );
		}
		else if( bCtrlDown && Key == KEY_C )
		{
			KismetCopy();
		}
		else if( bCtrlDown && Key == KEY_V )
		{
			KismetPaste();
		}
		else if( bCtrlDown && Key == KEY_X )
		{
			KismetCopy();
			DeleteSelected();
		}
		else if( bCtrlDown && Key == KEY_Z )
		{
			KismetUndo();
		}
		else if( bCtrlDown && Key == KEY_Y )
		{
			KismetRedo();
		}
		else if( Key == KEY_C )
		{
			if(SelectedSeqObjs.Num() > 0)
			{
				NewSequenceObject( USequenceFrame::StaticClass(), 0, 0 );
			}
		}
		else if( Key == KEY_A )
		{
			OnButtonZoomToFit( wxCommandEvent() );
		}
		else if( bCtrlDown && Key == KEY_Tab )
		{
			if(OldSequence)
			{
				ChangeActiveSequence(OldSequence);
			}
		}
	}
}

void WxKismet::OnMouseOver(UObject* Obj)
{
	USequenceObject* SeqObj = NULL;
	if(Obj)
	{
		SeqObj = CastChecked<USequenceObject>(Obj);
	}

	StatusBar->SetMouseOverObject( SeqObj );
}

void WxKismet::BeginTransactionOnSelected()
{
	GEditor->Trans->Begin( TEXT("LinkedObject Modify") );

	for(INT i=0; i<SelectedSeqObjs.Num(); i++)
	{
		USequenceObject* SeqObj = SelectedSeqObjs(i);
		SeqObj->Modify();
	}
}

void WxKismet::EndTransactionOnSelected()
{
	GEditor->Trans->End();
}

/*-----------------------------------------------------------------------------
	WxKismet
-----------------------------------------------------------------------------*/

UBOOL				WxKismet::bSeqObjClassesInitialized = false;
TArray<UClass*>		WxKismet::SeqObjClasses;

BEGIN_EVENT_TABLE( WxKismet, wxFrame )
	EVT_MENU_RANGE(IDM_NEW_SEQUENCE_OBJECT_START, IDM_NEW_SEQUENCE_OBJECT_END, WxKismet::OnContextNewScriptingObject)
	EVT_MENU_RANGE(IDM_NEW_SEQUENCE_EVENT_START, IDM_NEW_SEQUENCE_EVENT_END, WxKismet::OnContextNewScriptingEventContext)
	EVT_MENU( IDM_KISMET_NEW_VARIABLE_OBJ_CONTEXT, WxKismet::OnContextNewScriptingObjVarContext)
	EVT_MENU(IDM_KISMET_CREATE_LINKED_VARIABLE, WxKismet::OnContextCreateLinkedVariable)
	EVT_MENU(IDM_KISMET_CLEAR_VARIABLE, WxKismet::OnContextClearVariable)
	EVT_MENU(IDM_KISMET_BREAK_LINK_ALL, WxKismet::OnContextBreakLink)
	EVT_MENU_RANGE(IDM_KISMET_BREAK_LINK_START, IDM_KISMET_BREAK_LINK_END, WxKismet::OnContextBreakLink)
	EVT_MENU(IDM_KISMET_RENAMESEQ, WxKismet::OnContextRenameSequence)
	EVT_MENU(IDM_KISMET_DELETE_OBJECT, WxKismet::OnContextDelSeqObject)
	EVT_MENU(IDM_KISMET_SELECT_EVENT_ACTORS, WxKismet::OnContextSelectEventActors)
	EVT_MENU(IDM_KISMET_FIRE_EVENT, WxKismet::OnContextFireEvent)
	EVT_MENU(IDM_KISMET_OPEN_INTERPEDIT, WxKismet::OnContextOpenInterpEdit)
	EVT_MENU(IDM_KISMET_BUTTON_ZOOMTOFIT, WxKismet::OnButtonZoomToFit)
	EVT_MENU(IDM_KISMET_HIDE_CONNECTORS, WxKismet::OnContextHideConnectors)
	EVT_MENU(IDM_KISMET_SHOW_CONNECTORS, WxKismet::OnContextShowConnectors)
	EVT_MENU(IDM_KISMET_FIND_NAMEDVAR_USES, WxKismet::OnContextFindNamedVarUses)
	EVT_MENU(IDM_KISMET_FIND_NAMEDVAR, WxKismet::OnContextFindNamedVarDefs)
	EVT_MENU(IDM_KISMET_CREATE_SEQUENCE, WxKismet::OnContextCreateSequence)
	EVT_MENU(IDM_KISMET_EXPORT_SEQUENCE, WxKismet::OnContextExportSequence)
	EVT_MENU(IDM_KISMET_IMPORT_SEQUENCE, WxKismet::OnContextImportSequence)
	EVT_MENU(IDM_KISMET_PASTE_HERE, WxKismet::OnContextPasteHere)
	EVT_MENU(IDM_KISMET_LINK_ACTOR_TO_EVT, WxKismet::OnContextLinkEvent)
	EVT_MENU(IDM_KISMET_LINK_ACTOR_TO_OBJ, WxKismet::OnContextLinkObject)
	EVT_TOOL(IDM_KISMET_PARENTSEQ, WxKismet::OnButtonParentSequence)
	EVT_TOOL(IDM_KISMET_RENAMESEQ, WxKismet::OnButtonRenameSequence)
	EVT_TOOL(IDM_KISMET_BUTTON_HIDE_CONNECTORS, WxKismet::OnButtonHideConnectors)
	EVT_TOOL(IDM_KISMET_BUTTON_SHOW_CONNECTORS, WxKismet::OnButtonShowConnectors)
	EVT_TOOL(IDM_KISMET_BUTTON_SHOW_CURVES, WxKismet::OnButtonShowCurves)
	EVT_TOOL(IDM_KISMET_OPENSEARCH, WxKismet::OnOpenSearch)
	EVT_TREE_SEL_CHANGED( IDM_LINKEDOBJED_TREE, WxKismet::OnTreeItemSelected )
	EVT_TREE_ITEM_EXPANDING( IDM_LINKEDOBJED_TREE, WxKismet::OnTreeExpanding )
	EVT_TREE_ITEM_COLLAPSED( IDM_LINKEDOBJED_TREE, WxKismet::OnTreeCollapsed )
	EVT_BUTTON(IDM_KISMET_DOSEARCH, WxKismet::OnDoSearch )
	EVT_TEXT_ENTER(IDM_KISMET_SEARCHENTRY, WxKismet::OnDoSearch )
	EVT_LISTBOX( IDM_KISMET_SEARCHRESULT, WxKismet::OnSearchResultChanged )
END_EVENT_TABLE()

static int CDECL SeqObjNameCompare(const void *A, const void *B)
{
	USequenceObject* ADef = (USequenceObject*)(*(UClass**)A)->GetDefaultObject();
	USequenceObject* BDef = (USequenceObject*)(*(UClass**)B)->GetDefaultObject();

	return appStricmp( *(ADef->ObjName), *(BDef->ObjName) );
}

/** Static functions that fills in array of all available USequenceObject classes and sorts them alphabetically. */
void WxKismet::InitSeqObjClasses()
{
	if(bSeqObjClassesInitialized)
		return;

	// Construct list of non-abstract gameplay sequence object classes.
	for(TObjectIterator<UClass> It; It; ++It)
	{
		if( It->IsChildOf(USequenceObject::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			SeqObjClasses.AddItem(*It);
		}
	}

	appQsort( &SeqObjClasses(0), SeqObjClasses.Num(), sizeof(UClass*), SeqObjNameCompare );

	bSeqObjClassesInitialized = true;
}

/** 
 *	Static function that takes a level and creates a new sequence in it if there isn't on already. 
 *	Then opens Kismet window to edit Level's root sequence. 
 */
void WxKismet::OpenKismet(UUnrealEdEngine* Editor)
{
	// If level contains no sequence - make a new one now.
	if(!Editor->Level->GameSequence)
	{
		Editor->Level->GameSequence = ConstructObject<USequence>( USequence::StaticClass(), Editor->Level, TEXT("Main_Sequence"), RF_Transactional );
		if(!Editor->Level->GameSequence)
			return;
	}

	Editor->Level->GameSequence->CheckForErrors();
	Editor->Level->GameSequence->UpdateNamedVarStatus();

	WxKismet* SequenceEditor = new WxKismet( GApp->EditorFrame, -1, Editor->Level->GameSequence );
	SequenceEditor->Show();
}

/** 
 *	Searchs the current levels Kismet sequences to find any references to supplied Actor.
 *	Opens a Kismet window if none are currently open, or re-uses first one. Opens SearchWindow as well if not currently open.
 *
 *	@param FindActor	Actor to find references to.
 */
void WxKismet::FindActorReferences(AActor* FindActor)
{
	// If no Kismet windows open - open one now.
	if( GApp->KismetWindows.Num() == 0 )
	{
		WxKismet::OpenKismet(GUnrealEd);
	}
	check( GApp->KismetWindows.Num() > 0 );

	WxKismet* UseKismet = GApp->KismetWindows(0);

	// This will handle opening search window, jumping to first result etc.
	UseKismet->DoSearch( FindActor->GetName(), KST_ObjName, true );
}

/**
 * Activates the passed in sequence for editing.
 *
 * @param	newSeq - new sequence to edit
 */
void WxKismet::ChangeActiveSequence(USequence *newSeq, UBOOL bNotifyTree)
{
	check(newSeq != NULL && "Tried to activate NULL sequence");
	if (newSeq != Sequence)
	{
		// clear all existing refs
		SelectedSeqObjs.Empty();
		// update any connectors on both sequences, to catch recent changes
		if (Sequence != NULL)
		{
			Sequence->UpdateConnectors();
		}
		newSeq->UpdateConnectors();

		// remember the last sequence we were on.
		OldSequence = Sequence;

		// and set the new sequence
		Sequence = newSeq;

		// Slight hack to ensure all Sequences and SequenceObjects are RF_Transactional
		Sequence->SetFlags( RF_Transactional );

		for(INT i=0; i<Sequence->SequenceObjects.Num(); i++)
		{
			Sequence->SequenceObjects(i)->SetFlags( RF_Transactional );
		}
	}

	FString seqTitle = Sequence->GetSeqObjFullName();
	SetTitle( *FString::Printf( TEXT("UnrealKismet: %s"), *seqTitle ) );

	// Set position the last remember 
	LinkedObjVC->Origin2D.X = Sequence->DefaultViewX;
	LinkedObjVC->Origin2D.Y = Sequence->DefaultViewY;
	LinkedObjVC->Zoom2D = Sequence->DefaultViewZoom;

	// When changing sequence, clear selection (don't want to have things selected that aren't in current sequence)
	EmptySelection();

	if(bNotifyTree)
	{
		// Select the new sequence in the tree control.
		wxTreeItemId* CurrentId = TreeMap.Find(Sequence);
		if(CurrentId) // If this is initial call to ChangeActiveSequence, we may not have built the tree at all yet...
		{
			TreeControl->SelectItem(*CurrentId);
		}
	}

	RefreshViewport();
}

/** Switch Kismet to the Sequence containing the supplied object and move view to center on it. */
void WxKismet::CenterViewOnSeqObj(USequenceObject* ViewObj)
{
	// Get the parent sequence of this object
	USequence* ParentSeq = Cast<USequence>(ViewObj->GetOuter());

	// Can't do anything if this object has no parent...
	if(ParentSeq)
	{
		// Change view to that sequence
		ChangeActiveSequence(ParentSeq, true);

		// Then adjust origin to put selected SequenceObject at the center.
		INT SizeX = LinkedObjVC->Viewport->GetSizeX();
		INT SizeY = LinkedObjVC->Viewport->GetSizeY();

		FLOAT DrawOriginX = (ViewObj->ObjPosX + 20)- ((SizeX*0.5f)/LinkedObjVC->Zoom2D);
		FLOAT DrawOriginY = (ViewObj->ObjPosY + 20)- ((SizeY*0.5f)/LinkedObjVC->Zoom2D);

		LinkedObjVC->Origin2D.X = -(DrawOriginX * LinkedObjVC->Zoom2D);
		LinkedObjVC->Origin2D.Y = -(DrawOriginY * LinkedObjVC->Zoom2D);
		ViewPosChanged();

		// Select object we are centering on.
		EmptySelection();
		AddToSelection(ViewObj);

		// Update view
		RefreshViewport();
	}
}

/** Change view to encompass selection. This will not adjust zoom beyone the 0.1 - 1.0 range though. */
void WxKismet::ZoomSelection()
{
	FIntRect SelectedBox = CalcBoundingBoxOfSelected();
	if(SelectedBox.Area() > 0)
	{
		ZoomToBox(SelectedBox);
	}
}

/** Change view to encompass all objects in current sequence. This will not adjust zoom beyone the 0.1 - 1.0 range though. */
void WxKismet::ZoomAll()
{
	FIntRect AllBox = CalcBoundingBoxOfAll();
	if(AllBox.Area() > 0)
	{
		ZoomToBox(AllBox);
	}
}

/** Change view to encompass the supplied box. This will not adjust zoom beyone the 0.1 - 1.0 range though. */
void WxKismet::ZoomToBox(const FIntRect& Box)
{
	// Get center point and radius of supplied box.
	FIntPoint Center, Extent;
	Box.GetCenterAndExtents(Center, Extent);
	Extent += FIntPoint(20, 20); // A little padding around the outside of selection.
		
	// Calculate the zoom factor we need to fit the box onto the screen.
	INT SizeX = LinkedObjVC->Viewport->GetSizeX();
	INT SizeY = LinkedObjVC->Viewport->GetSizeY();

	FLOAT NewZoomX = ((FLOAT)SizeX)/((FLOAT)(2 * Extent.X));
	FLOAT NewZoomY = ((FLOAT)SizeY)/((FLOAT)(2 * Extent.Y));
	FLOAT NewZoom = ::Min(NewZoomX, NewZoomY);
	LinkedObjVC->Zoom2D = Clamp(NewZoom, 0.1f, 1.0f);

	// Set origin to center on box center.
	FLOAT DrawOriginX = Center.X - ((SizeX*0.5f)/LinkedObjVC->Zoom2D);
	FLOAT DrawOriginY = Center.Y - ((SizeY*0.5f)/LinkedObjVC->Zoom2D);

	LinkedObjVC->Origin2D.X = -(DrawOriginX * LinkedObjVC->Zoom2D);
	LinkedObjVC->Origin2D.Y = -(DrawOriginY * LinkedObjVC->Zoom2D);

	// Notify that view has changed.
	ViewPosChanged();
}

/** Handler for 'zoom to fit' button. Will fit to selection if there is one, or entire sequence otherwise. */
void WxKismet::OnButtonZoomToFit(wxCommandEvent &In)
{
	if(SelectedSeqObjs.Num() == 0)
	{
		ZoomAll();
	}
	else
	{
		ZoomSelection();
	}
}

/** Close all currently open Kismet windows. Uses the KismetWindows array in GApp.  */
void WxKismet::CloseAllKismetWindows()
{
	// Because the Kismet destructor removes itself from the array, we need to copy the array first.
	// Destructor may not be called on Close - it happens as part of the wxWindows idle process.

	TArray<WxKismet*> KisWinCopy;
	KisWinCopy.Add( GApp->KismetWindows.Num() );
	for(INT i=0; i<GApp->KismetWindows.Num(); i++)
	{
		KisWinCopy(i) = GApp->KismetWindows(i);
	}

	for(INT i=0; i<KisWinCopy.Num(); i++)
	{
		KisWinCopy(i)->Close(true);
	}
}

/** Kismet constructor. Initialise variables, create toolbar and status bar, set view to supplied sequence and add to global list of Kismet windows. */
WxKismet::WxKismet( wxWindow* InParent, wxWindowID InID, class USequence* InSequence )
	: WxLinkedObjEd( InParent, InID, TEXT("Kismet"), true )
{
	Sequence = NULL;
	OldSequence = NULL;
	bDrawCurves = true;	
	PasteCount = 0;
	bAttachVarsToConnector = false;
	SearchWindow = NULL;

	// activate the base sequence
	ChangeActiveSequence(InSequence);

	// Create toolbar
	ToolBar = new WxKismetToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Create status bar
	StatusBar = new WxKismetStatusBar( this, -1 );
	SetStatusBar( StatusBar );

	// load the background texture
	BackgroundTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorMaterials.KismetBackground"), NULL, LOAD_NoFail, NULL);

	WxKismet::InitSeqObjClasses();

	TreeImages->Add( WxBitmap( "KIS_SeqTreeClosed" ), wxColor( 192,192,192 ) ); // 0
	TreeImages->Add( WxBitmap( "KIS_SeqTreeOpen" ), wxColor( 192,192,192 ) ); // 1

	// Initialise the tree control.
	RebuildTreeControl();

	// Keep a list of all Kismets in case we need to close them etc.
	GApp->KismetWindows.AddItem(this);
}

/** Kismet destructor. Will remove itself from global list of Kismet windows. */
WxKismet::~WxKismet()
{
	if(SearchWindow)
	{
		SearchWindow->Close();
	}

	// Remove from global list of Kismet windows.
	check( GApp->KismetWindows.ContainsItem(this) );
	GApp->KismetWindows.RemoveItem(this);

	GEditor->Trans->Reset( TEXT("QuitKismet") );
}

/**
 * Creates a link between the ouptut and input of two sequence
 * operations.  First searches for a previous entry in the
 * output links, and if none is found adds a new link.
 * 
 * @param	OutOp - op creating the output link
 * @param	OutConnIndex - index in OutOp's OutputLinks[] to use
 * @param	InOp - op creating the input link
 * @param	InConnIndex - index in InOp's InputLinks[] to use
 */
void WxKismet::MakeLogicConnection(USequenceOp* OutOp, INT OutConnIndex, USequenceOp* InOp, INT InConnIndex)
{
	GEditor->Trans->Begin( TEXT("MakeLogicConnection") );
	OutOp->Modify();

	// grab the output link
	FSeqOpOutputLink &outLink = OutOp->OutputLinks(OutConnIndex);
	// make sure the link doesn't already exist
	INT newIdx = -1;
	UBOOL bFoundEntry = 0;
	for (INT linkIdx = 0; linkIdx < outLink.Links.Num() && !bFoundEntry; linkIdx++)
	{
		// if it's an empty entry save the index
		if (outLink.Links(linkIdx).LinkedOp == NULL)
		{
			newIdx = linkIdx;
		}
		else
		// check for a match
		if (outLink.Links(linkIdx).LinkedOp == InOp &&
			outLink.Links(linkIdx).InputLinkIdx == InConnIndex)
		{
			bFoundEntry = 1;
		}
	}
	// if we didn't find an entry, add one
	if (!bFoundEntry)
	{
		// if we didn't find an empty entry, create a new one
		if (newIdx == -1)
		{
			newIdx = outLink.Links.AddZeroed();
		}
		// add the entry
		outLink.Links(newIdx).LinkedOp = InOp;
		outLink.Links(newIdx).InputLinkIdx = InConnIndex;
		// notify the op that it has connected to something
		OutOp->OnConnect(InOp,OutConnIndex);
	}

	GEditor->Trans->End();
}

/** 
 *	Checks to see if a particular variable connector may connect to a particular variable. 
 *	Looks at variable type and allowed type of connector.
 */
static UBOOL CanConnectVarToLink(FSeqVarLink* VarLink, USequenceVariable* Var)
{
	// In cases where variable connector has no type (eg. uninitialised external vars), disallow all connections
	if(VarLink->ExpectedType == NULL)
	{
		return false;
	}		

	// Check we are not trying to attach something new to a variable which is at capacity already.
	if(VarLink->LinkedVariables.Num() == VarLink->MaxVars)
	{
		return false;
	}

	// Usual case - connecting normal variable to connector. Check class is as expected.
	if(Var->IsA(VarLink->ExpectedType))
	{
		return true;
	}

	// Check cases where variables can change type (Named and External variables)
	USeqVar_External* ExtVar = Cast<USeqVar_External>(Var);
	if(ExtVar && (!ExtVar->ExpectedType || ExtVar->ExpectedType == VarLink->ExpectedType))
	{
		return true;
	}

	USeqVar_Named* NamedVar = Cast<USeqVar_Named>(Var);
	if(NamedVar && (!NamedVar->ExpectedType || NamedVar->ExpectedType == VarLink->ExpectedType))
	{
		return true;
	}

	return false;
}

/** Create a connection between the variable connector at index OpConnIndex on the SequenceOp Op to the variable Var. */
void WxKismet::MakeVariableConnection(USequenceOp* Op, INT OpConnIndex, USequenceVariable* Var)
{
	FSeqVarLink* VarLink = &Op->VariableLinks(OpConnIndex);

	// Check the type is correct before making line.
	if( CanConnectVarToLink(VarLink, Var) )
	{
		GEditor->Trans->Begin( TEXT("MakeLogicConnection") );
		Op->Modify();

		VarLink->LinkedVariables.AddItem(Var);
		Var->OnConnect(Op,OpConnIndex);

		GEditor->Trans->End();

		// If connection is of InterpData type, ensure all SeqAct_Interps are up to date.
		if(VarLink->ExpectedType == UInterpData::StaticClass())
		{
			GEditor->Level->GameSequence->UpdateInterpActionConnectors();
		}
	}
}

/** 
 *	Iterate over selected SequenceObjects and remove them from the sequence.
 *	This will clear any references to those objects by other objects in the sequence.
 */
void WxKismet::DeleteSelected()
{
	GEditor->Trans->Begin( TEXT("DeleteSelected") );
	Sequence->Modify();

	TArray<UObject*> ModifiedObjects;
		
	UBOOL bDeletingSequence = false;
	for(INT ObjIdx=0; ObjIdx<SelectedSeqObjs.Num(); ObjIdx++)
	{
		if( SelectedSeqObjs(ObjIdx)->IsA(USequence::StaticClass()) )
		{
			bDeletingSequence = true;
		}

		SelectedSeqObjs(ObjIdx)->Modify();

		// If this is a sequence op then we check all other ops inputs to see if any reference to it.
		USequenceOp* SeqOp = Cast<USequenceOp>(SelectedSeqObjs(ObjIdx));
		if (SeqOp != NULL)
		{
			// iterate through all other objects, looking for output links that point to this op
			for (INT chkIdx = 0; chkIdx < Sequence->SequenceObjects.Num(); chkIdx++)
			{
				// if it is a sequence op,
				USequenceOp *ChkOp = Cast<USequenceOp>(Sequence->SequenceObjects(chkIdx));
				if (ChkOp != NULL)
				{
					// iterate through this op's output links
					for (INT linkIdx = 0; linkIdx < ChkOp->OutputLinks.Num(); linkIdx++)
					{
						// iterate through all the inputs linked to this output
						for (INT inputIdx = 0; inputIdx < ChkOp->OutputLinks(linkIdx).Links.Num(); inputIdx++)
						{
							if (ChkOp->OutputLinks(linkIdx).Links(inputIdx).LinkedOp == SeqOp)
							{
								// remove the entry
								ChkOp->OutputLinks(linkIdx).Links.Remove(inputIdx--,1);

								if( !ModifiedObjects.ContainsItem(ChkOp) )
								{
									ChkOp->Modify();
									ModifiedObjects.AddItem(ChkOp);
								}
							}
						}
					}
				}
			}
			// clear the refs in this op, so that none are left dangling
			SeqOp->InputLinks.Empty();
			SeqOp->OutputLinks.Empty();
			SeqOp->VariableLinks.Empty();
		}

		// If we are removing a variable - we must look through all Ops to see if there are any references to it and clear them.
		USequenceVariable* SeqVar = Cast<USequenceVariable>( SelectedSeqObjs(ObjIdx) );
		if(SeqVar)
		{
			for(INT ChkIdx=0; ChkIdx<Sequence->SequenceObjects.Num(); ChkIdx++)
			{
				USequenceOp* ChkOp = Cast<USequenceOp>( Sequence->SequenceObjects(ChkIdx) );
				if(ChkOp)
				{
					for(INT ChkLnkIdx=0; ChkLnkIdx < ChkOp->VariableLinks.Num(); ChkLnkIdx++)
					{
						for (INT varIdx = 0; varIdx < ChkOp->VariableLinks(ChkLnkIdx).LinkedVariables.Num(); varIdx++)
						{
							if (ChkOp->VariableLinks(ChkLnkIdx).LinkedVariables(varIdx) == SeqVar)
							{
								// Remove from list
								ChkOp->VariableLinks(ChkLnkIdx).LinkedVariables.Remove(varIdx--,1);

								if( !ModifiedObjects.ContainsItem(ChkOp) )
								{
									ChkOp->Modify();
									ModifiedObjects.AddItem(ChkOp);
								}
							}
						}
					}
				}
			}
		}

		USequenceEvent* SeqEvt = Cast<USequenceEvent>( SelectedSeqObjs(ObjIdx) );
		if(SeqEvt)
		{
			// search for any ops that have a link to this event
			for (INT idx = 0; idx < Sequence->SequenceObjects.Num(); idx++)
			{
				USequenceOp *ChkOp = Cast<USequenceOp>(Sequence->SequenceObjects(idx));
				if (ChkOp != NULL &&
					ChkOp->EventLinks.Num() > 0)
				{
					for (INT eventIdx = 0; eventIdx < ChkOp->EventLinks.Num(); eventIdx++)
					{
						for (INT chkIdx = 0; chkIdx < ChkOp->EventLinks(eventIdx).LinkedEvents.Num(); chkIdx++)
						{
							if (ChkOp->EventLinks(eventIdx).LinkedEvents(chkIdx) == SeqEvt)
							{
								ChkOp->EventLinks(eventIdx).LinkedEvents.Remove(chkIdx--,1);

								if( !ModifiedObjects.ContainsItem(ChkOp) )
								{
									ChkOp->Modify();
									ModifiedObjects.AddItem(ChkOp);
								}
							}
						}
					}
				}
			}
		}

		Sequence->SequenceObjects.RemoveItem( SelectedSeqObjs(ObjIdx) );
	}

	GEditor->Trans->End();

	// Ensure the SearchWindow (if up) doesn't contain references to deleted SequenceObjects.
	if(SearchWindow)
	{
		SearchWindow->ResultList->Clear();
	}

	SelectedSeqObjs.Empty();
	UpdatePropertyWindow();

	// May have deleted a named variable, so update their global status.
	GEditor->Level->GameSequence->UpdateNamedVarStatus();
	GEditor->Level->GameSequence->UpdateInterpActionConnectors();

	// If we removed a subsequence, we need to rebuild the tree control.
	if(bDeletingSequence)
	{
		RebuildTreeControl();
	}

	RefreshViewport();
}

/** NOT CURRENTLY SUPPORTED */
void WxKismet::SingleTickSequence()
{
	/*TODO: figure out editor ticking!
	debugf(TEXT("Ticking scripting system"));
	Sequence->UpdateOp();

	LinkedObjVC->Viewport->Invalidate();
	*/
}

/** Take the current view position and store it in the sequence object. */
void WxKismet::ViewPosChanged()
{
	Sequence->DefaultViewX = LinkedObjVC->Origin2D.X;
	Sequence->DefaultViewY = LinkedObjVC->Origin2D.Y;
	Sequence->DefaultViewZoom = LinkedObjVC->Zoom2D;
}

/** 
 *	Handling for draggin on 'special' hit proxies. Should only have 1 object selected when this is being called. 
 *	In this case used for handling resizing handles on Comment objects. 
 *
 *	@param	DeltaX			Horizontal drag distance this frame (in pixels)
 *	@param	DeltaY			Vertical drag distance this frame (in pixels)
 *	@param	SpecialIndex	Used to indicate different classes of action. @see HLinkedObjProxySpecial.
 */
void WxKismet::SpecialDrag( INT DeltaX, INT DeltaY, INT SpecialIndex )
{
	// Can only 'special drag' one object at a time.
	if(SelectedSeqObjs.Num() != 1)
	{
		return;
	}

	USequenceFrame* SeqFrame = Cast<USequenceFrame>(SelectedSeqObjs(0));
	if(SeqFrame)
	{
		if(SpecialIndex == 1)
		{
			// Apply dragging to 
			SeqFrame->SizeX += DeltaX;
			SeqFrame->SizeX = ::Max<INT>(SeqFrame->SizeX, 64);

			SeqFrame->SizeY += DeltaY;
			SeqFrame->SizeY = ::Max<INT>(SeqFrame->SizeY, 64);
		}
	}
}

/** Kismet-specific Undo. Uses the global Undo transactor to reverse changes and updates viewport etc. */
void WxKismet::KismetUndo()
{
	GEditor->Trans->Undo();

	RefreshViewport();
	UpdatePropertyWindow();
	GEditor->Level->GameSequence->UpdateNamedVarStatus(); // May have changed named variable status
	GEditor->Level->GameSequence->UpdateInterpActionConnectors(); // May have changed SeqAct_Interp/InterpData connectedness.
}

/** Kismet-specific Redo. Uses the global Undo transactor to redo changes and updates viewport etc. */
void WxKismet::KismetRedo()
{
	GEditor->Trans->Redo();
	RefreshViewport();
	GEditor->Level->GameSequence->UpdateNamedVarStatus();
	GEditor->Level->GameSequence->UpdateInterpActionConnectors();
}

/** Export selected sequence objects to text and puts into Windows clipboard. */
void WxKismet::KismetCopy()
{
	// Add sequence objects selected in Kismet to global selection so the USequenceExporterT3D knows which objects to export.
	GSelectionTools.SelectNone( USequenceObject::StaticClass() );
	for(INT i=0; i<SelectedSeqObjs.Num(); i++)
	{
		GSelectionTools.Select( SelectedSeqObjs(i) );
	}

	FStringOutputDevice Ar;
	UExporter::ExportToOutputDevice( Sequence, NULL, Ar, TEXT("copy"), 0 );
	appClipboardCopy( *Ar );

	PasteCount = 0;

	GSelectionTools.SelectNone( USequenceObject::StaticClass() );
}

/** 
 *	Take contents of windows clipboard and use USequenceFactory to create new SequenceObjects (possibly new subsequences as well). 
 *	If bAtMousePos is true, it will move the objects so the top-left corner of their bounding box is at the current mouse position (NewX/NewY in LinkedObjVC)
 */
void WxKismet::KismetPaste(UBOOL bAtMousePos)
{
	GEditor->Trans->Begin( TEXT("Paste") );
	Sequence->Modify();

	PasteCount++;

	// Get pasted text.
	FString PasteString = appClipboardPaste();
	const TCHAR* Paste = *PasteString;

	USequenceFactory* Factory = new USequenceFactory;
	Factory->FactoryCreateText( NULL, NULL, Sequence, NAME_None, RF_Transactional, NULL, TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );
	delete Factory;

	// Select the newly pasted stuff, and offset a bit.
	EmptySelection();

	UBOOL bPastingSequence = false;
	for( TSelectedObjectIterator It(USequenceObject::StaticClass()) ; It ; ++It )
	{
		USequenceObject* NewSeqObj = CastChecked<USequenceObject>(*It);
		AddToSelection(NewSeqObj);

		NewSeqObj->ObjPosX += (PasteCount * PasteOffset);
		NewSeqObj->ObjPosY += (PasteCount * PasteOffset);

		if( NewSeqObj->IsA(USequence::StaticClass()) )
		{
			bPastingSequence = true;
		}
	}

	// If we want to paste the copied objects at the current mouse position (NewX/NewY in LinkedObjVC) and we actually pasted something...
	if(bAtMousePos && SelectedSeqObjs.Num() > 0)
	{
		// First calculate current bounding box of selection.
		FIntRect SelectedBox = CalcBoundingBoxOfSelected();
		if(SelectedBox.Area() > 0)
		{
			// Find where we want the top-left corner of selection to be.
			INT DesPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
			INT DesPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

			// Amount we need to move it by.
			INT DeltaX = DesPosX - SelectedBox.Min.X;
			INT DeltaY = DesPosY - SelectedBox.Min.Y;

			// Apply to all selected objects.
			for(INT i=0; i<SelectedSeqObjs.Num(); i++)
			{
				SelectedSeqObjs(i)->ObjPosX += DeltaX;
				SelectedSeqObjs(i)->ObjPosY += DeltaY;
			}
		}
	}

	GEditor->Trans->End();

	// If we created a new sequence through pasting, update tree control.
	if(bPastingSequence)
	{
		RebuildTreeControl();
	}

	// We may have pasted a variable with a VarName, so update status of SeqVar_Named
	GEditor->Level->GameSequence->UpdateNamedVarStatus();
	GEditor->Level->GameSequence->UpdateInterpActionConnectors();

	RefreshViewport();
}

/** Refresh Kismet tree control showing heirarchy of Sequences. */
void WxKismet::RebuildTreeControl()
{
	check(TreeControl);

	// Empty whats currently there.
	TreeControl->DeleteAllItems();
	TreeMap.Empty();
	
	// Is there a better way to get the level?
	USequence* RootSeq = GEditor->Level->GameSequence;
	check(RootSeq); // Should not be able to open Kismet without the level having a Sequence.

	wxTreeItemId RootId = TreeControl->AddRoot( RootSeq->GetName(), 0, 0, new WxKismetTreeLeafData(RootSeq) );
	TreeMap.Set( RootSeq, RootId );

	// Find all sequences. The function will always return parents before children in the array.
	TArray<USequenceObject*> SeqObjs;
	RootSeq->FindSeqObjectsByClass( USequence::StaticClass(), SeqObjs );
	
	// Iterate over sequences, adding them to the tree.
	for(INT i=0; i<SeqObjs.Num(); i++)
	{
		//debugf( TEXT("Item: %d %s"), i, SeqObjs(i)->GetName() );
		USequence* Seq = CastChecked<USequence>( SeqObjs(i) );

		// Only the root sequence should have an Outer that is not another Sequence (in the root case its the level).
		USequence* ParentSeq = CastChecked<USequence>( Seq->GetOuter() );
		wxTreeItemId* ParentId = TreeMap.Find(ParentSeq);
		check(ParentId);

		wxTreeItemId NodeId = TreeControl->AppendItem( *ParentId, Seq->GetName(), 0, 0, new WxKismetTreeLeafData(Seq) );
		TreeMap.Set( Seq, NodeId );

		TreeControl->SortChildren( *ParentId );
	}

	TreeControl->Expand(RootId);

	// Select the sequence we are currently editing.
	wxTreeItemId* CurrentId = TreeMap.Find(Sequence);
	check(CurrentId);

	TreeControl->SelectItem(*CurrentId);
}

/** Open the Matinee tool to edit the supplied SeqAct_Interp (Matinee) action. Will check that SeqAct_Interp has an InterpData attached. */
void WxKismet::OpenMatinee(USeqAct_Interp* Interp)
{
	UInterpData* Data = Interp->FindInterpDataFromVariable();
	if(!Data)
	{
		appMsgf( 0, TEXT("Matinee action must have Matinee Data attached before being edited.") );
		return;
	}

	// If already in Matinee mode, exit out before going back in with new Interpolation.
	if( GEditorModeTools.GetCurrentModeID() == EM_InterpEdit )
	{
		GApp->SetCurrentMode( EM_Default );
	}

	GApp->SetCurrentMode( EM_InterpEdit );

	FEdModeInterpEdit* InterpEditMode = (FEdModeInterpEdit*)GEditorModeTools.GetCurrentMode();

	InterpEditMode->InitInterpMode( Interp );

	// Minimize Kismet here?
	//this->Iconize(true);
	//this->Close();
}

/** Notification that the user clicked on an element of the tree control. Will find the corresponing USequence and jump to it. */
void WxKismet::OnTreeItemSelected( wxTreeEvent &In )
{
	wxTreeItemId TreeId = In.GetItem();

	WxKismetTreeLeafData* LeafData = (WxKismetTreeLeafData*)(TreeControl->GetItemData(TreeId));
	if(LeafData)
	{
		USequence* ClickedSeq = LeafData->Data;
		ChangeActiveSequence(ClickedSeq, false);
	}
}

/** Notification that a tree node expanded. Changes icon in tree view. */
void WxKismet::OnTreeExpanding( wxTreeEvent& In )
{
	TreeControl->SetItemImage( In.GetItem(), 1 );
}

/** Notification that a tree node collapsed. Changes icon in tree view. */
void WxKismet::OnTreeCollapsed( wxTreeEvent& In )
{
	TreeControl->SetItemImage( In.GetItem(), 0 );
}

/** 
 *	Create a new USequenceObject in the currently active Sequence of the give class and at the given position.
 *	This function does special handling if creating a USequenceFrame object and we have objects selected - it will create frame around selection.
 */
void WxKismet::NewSequenceObject(UClass* NewSeqObjClass, INT NewPosX, INT NewPosY)
{
	GEditor->Trans->Begin( TEXT("NewObject") );
	Sequence->Modify();

	USequenceObject* NewSeqObj = ConstructObject<USequenceObject>( NewSeqObjClass, Sequence, NAME_None,	RF_Transactional);
	debugf(TEXT("Created new %s in sequence %s"),NewSeqObj->GetName(),Sequence->GetName());

	// If creating a SequenceFrame (comment) object, and we have things selected, create frame around selection.
	USequenceFrame* SeqFrame = Cast<USequenceFrame>(NewSeqObj);
	if(SeqFrame && SelectedSeqObjs.Num() > 0)
	{
		FIntRect SelectedRect = CalcBoundingBoxOfSelected();
		SeqFrame->ObjPosX = SelectedRect.Min.X - FitCommentToSelectedBorder;
		SeqFrame->ObjPosY = SelectedRect.Min.Y - FitCommentToSelectedBorder;
		SeqFrame->SnapPosition(KISMET_GRIDSIZE);
		SeqFrame->SizeX = (SelectedRect.Max.X - SelectedRect.Min.X) + (2 * FitCommentToSelectedBorder);
		SeqFrame->SizeY = (SelectedRect.Max.Y - SelectedRect.Min.Y) + (2 * FitCommentToSelectedBorder);
		SeqFrame->bDrawBox = true;
	}
	else
	{
		NewSeqObj->ObjPosX = NewPosX;
		NewSeqObj->ObjPosY = NewPosY;
		NewSeqObj->SnapPosition(KISMET_GRIDSIZE);
	}

	Sequence->SequenceObjects.AddItem(NewSeqObj);
	NewSeqObj->OnCreated();

	NewSeqObj->Modify();

	EmptySelection();	
	AddToSelection(NewSeqObj);

	GEditor->Trans->End();
	RefreshViewport();
	UpdatePropertyWindow();
}

/** 
 *	Handler for menu option for creating arbitary new sequence object.
 *	Find class from the SeqObjClasses array and event id.
 */
void WxKismet::OnContextNewScriptingObject( wxCommandEvent& In )
{
	INT NewSeqObjIndex = In.GetId() - IDM_NEW_SEQUENCE_OBJECT_START;
	check( NewSeqObjIndex >= 0 && NewSeqObjIndex < SeqObjClasses.Num() );

	UClass* NewSeqObjClass = SeqObjClasses(NewSeqObjIndex);
	check( NewSeqObjClass->IsChildOf(USequenceObject::StaticClass()) );

	// Calculate position in sequence from mouse position.
	INT NewPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
	INT NewPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;

	NewSequenceObject(NewSeqObjClass, NewPosX, NewPosY);
}

/** 
 *	Create a new Object Variable and fill it in with the currently selected Actor.
 *	If multiple Actors are selected, will create an Object Variable for each, offsetting them.
 *	If bAttachVarsToConnector is true, it will also connect these new variables to the selected variable connector.
 */
void WxKismet::OnContextNewScriptingObjVarContext(wxCommandEvent& In)
{
	if (NewObjActors.Num() > 0)
	{
		GEditor->Trans->Begin( TEXT("NewEvent") );
		Sequence->Modify();
		EmptySelection();	

		for(INT i=0; i<NewObjActors.Num(); i++)
		{
			AActor* Actor = NewObjActors(i);
			USeqVar_Object* NewObjVar = ConstructObject<USeqVar_Object>( USeqVar_Object::StaticClass(), Sequence, NAME_None, RF_Transactional);
			debugf(TEXT("Created new %s in sequence %s"),NewObjVar->GetName(),Sequence->GetName());

			NewObjVar->ObjPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D + (MultiEventSpacing * i);
			NewObjVar->ObjPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
			NewObjVar->SnapPosition(KISMET_GRIDSIZE);

			NewObjVar->ObjValue = Actor;

			NewObjVar->Modify();
			Sequence->SequenceObjects.AddItem(NewObjVar);
			NewObjVar->OnCreated();

			AddToSelection(NewObjVar);

			// If desired, and we have a connector selected, attach to it now
			if(bAttachVarsToConnector && ConnSeqOp && (ConnType == LOC_VARIABLE))
			{
				MakeVariableConnection(ConnSeqOp, ConnIndex, NewObjVar);
			}
		}

		GEditor->Trans->End();
		RefreshViewport();
	}
}

/**
 * Creates a new variable at the selected connector and automatically links it.
 */
void WxKismet::OnContextCreateLinkedVariable(wxCommandEvent &In)
{
	if(ConnSeqOp != NULL && ConnType == LOC_VARIABLE)
	{
		UClass* varClass = ConnSeqOp->VariableLinks(ConnIndex).ExpectedType;
		if(varClass)
		{
			GEditor->Trans->Begin(TEXT("NewVar"));
			Sequence->Modify();

			USequenceVariable *var = ConstructObject<USequenceVariable>( varClass, Sequence, NAME_None, RF_Transactional );
			debugf(TEXT("Created new %s in sequence %s"),var->GetName(),Sequence->GetName());
			var->ObjPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
			var->ObjPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
			var->SnapPosition(KISMET_GRIDSIZE);

			var->Modify();
			Sequence->SequenceObjects.AddItem(var);
			var->OnCreated();

			EmptySelection();	
			AddToSelection(var);

			MakeVariableConnection(ConnSeqOp, ConnIndex, var);

			GEditor->Trans->End();
			RefreshViewport();
		}
	}
}

/**
 * Clears the values of all selected object variables.
 */
void WxKismet::OnContextClearVariable(wxCommandEvent &In)
{
	// iterate through for all sequence obj vars
	for (INT idx = 0; idx < SelectedSeqObjs.Num(); idx++)
	{
		USeqVar_Object *objVar = Cast<USeqVar_Object>(SelectedSeqObjs(idx));
		if (objVar != NULL)
		{
			// set their values to NULL
			objVar->ObjValue = NULL;
		}
	}
}

/**
 * Creates a new event using the selected actor(s), allowing only events that the
 * actor(s) support.
 */
void WxKismet::OnContextNewScriptingEventContext( wxCommandEvent& In )
{
	if (NewObjActors.Num() > 0)
	{
		GEditor->Trans->Begin( TEXT("NewEvent") );
		Sequence->Modify();
		EmptySelection();	

		INT NewSeqEvtIndex = In.GetId() - IDM_NEW_SEQUENCE_EVENT_START;
		check( NewSeqEvtIndex >= 0 && NewSeqEvtIndex < NewEventClasses.Num() );

		UClass* NewSeqEvtClass = NewEventClasses(NewSeqEvtIndex);

		for(INT i=0; i<NewObjActors.Num(); i++)
		{
			AActor* Actor = NewObjActors(i);
			USequenceEvent* NewSeqEvt = ConstructObject<USequenceEvent>( NewSeqEvtClass, Sequence, NAME_None, RF_Transactional);
			debugf(TEXT("Created new %s in sequence %s"),NewSeqEvt->GetName(),Sequence->GetName());

			NewSeqEvt->ObjPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
			NewSeqEvt->ObjPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D + (MultiEventSpacing * i);
			NewSeqEvt->SnapPosition(KISMET_GRIDSIZE);

			NewSeqEvt->ObjName = FString::Printf( TEXT("%s %s"), Actor->GetName(), *NewSeqEvt->ObjName );
			NewSeqEvt->Originator = Actor;

			NewSeqEvt->Modify();
			Sequence->SequenceObjects.AddItem(NewSeqEvt);
			NewSeqEvt->OnCreated();

			AddToSelection(NewSeqEvt);
		}

		GEditor->Trans->End();
		RefreshViewport();
	}
}

/**
 * Break link from selected input or variable connector 
 * (output connectors don't store pointer to target)
 */
void WxKismet::OnContextBreakLink( wxCommandEvent& In )
{
	GEditor->Trans->Begin( TEXT("BreakLink") );

	if (ConnType == LOC_EVENT)
	{
		ConnSeqOp->Modify();

		ConnSeqOp->EventLinks(ConnIndex).LinkedEvents.Empty();
	}
	else
	if(ConnType == LOC_VARIABLE)
	{
		ConnSeqOp->Modify();

		if (In.m_id == IDM_KISMET_BREAK_LINK_ALL)
		{
			// empty the linked vars array
			ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables.Empty();
		}
		else
		{
			// determine the selected break
			INT breakIdx = In.m_id - IDM_KISMET_BREAK_LINK_START;
			ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables.Remove(breakIdx,1);
		}
	}
	else
	if (ConnType == LOC_INPUT)
	{
		// find all ops that point to this
		TArray<USequenceOp*> outputOps;
		TArray<INT> outputIndices;
		FindOutputLinkTo(Sequence,ConnSeqOp,ConnIndex,outputOps,outputIndices);
		// if break all...
		if (In.m_id == IDM_KISMET_BREAK_LINK_ALL)
		{
			// break all the links individually
			for (INT idx = 0; idx < outputOps.Num(); idx++)
			{
				outputOps(idx)->Modify();

				for (INT linkIdx = 0; linkIdx < outputOps(idx)->OutputLinks(outputIndices(idx)).Links.Num(); linkIdx++)
				{
					// if this is the linked index
					if (outputOps(idx)->OutputLinks(outputIndices(idx)).Links(linkIdx).LinkedOp == ConnSeqOp)
					{
						// remove the entry
						outputOps(idx)->OutputLinks(outputIndices(idx)).Links.Remove(linkIdx--,1);
					}
				}
			}
		}
		else
		{
			// determine the selected break
			INT breakIdx = In.m_id - IDM_KISMET_BREAK_LINK_START;
			if (breakIdx >= 0 &&
				breakIdx < outputOps.Num())
			{
				outputOps(breakIdx)->Modify();

				// find the actual index of the link
				for (INT linkIdx = 0; linkIdx < outputOps(breakIdx)->OutputLinks(outputIndices(breakIdx)).Links.Num(); linkIdx++)
				{
					// if this is the linked index
					if (outputOps(breakIdx)->OutputLinks(outputIndices(breakIdx)).Links(linkIdx).LinkedOp == ConnSeqOp)
					{
						// remove the entry
						outputOps(breakIdx)->OutputLinks(outputIndices(breakIdx)).Links.Remove(linkIdx--,1);
					}
				}
			}
		}
	}
	else
	if (ConnType == LOC_OUTPUT)
	{
		ConnSeqOp->Modify();

		if (In.m_id == IDM_KISMET_BREAK_LINK_ALL)
		{
			// empty the links for this output
			ConnSeqOp->OutputLinks(ConnIndex).Links.Empty();
		}
		else
		{
			// determine the selected break
			INT breakIdx = In.m_id - IDM_KISMET_BREAK_LINK_START;
			if (breakIdx >= 0 &&
				breakIdx < ConnSeqOp->OutputLinks(ConnIndex).Links.Num())
			{
				// remove the link
				ConnSeqOp->OutputLinks(ConnIndex).Links.Remove(breakIdx,1);
			}
		}
	}

	GEditor->Trans->End();

	GEditor->Level->GameSequence->UpdateInterpActionConnectors();

	RefreshViewport();
}

/** Delete the selected sequence objects. */
void WxKismet::OnContextDelSeqObject( wxCommandEvent& In )
{
	DeleteSelected();

	RefreshViewport();
}

/** For all selected events, select the actors that correspond to them. */
void WxKismet::OnContextSelectEventActors( wxCommandEvent& In )
{
	GUnrealEd->SelectNone( GUnrealEd->Level, true);

	for(INT i=0; i<SelectedSeqObjs.Num(); i++)
	{
		USequenceEvent* SeqEvt = Cast<USequenceEvent>( SelectedSeqObjs(i) );
		if(SeqEvt && SeqEvt->Originator)
			GUnrealEd->SelectActor( GUnrealEd->Level, SeqEvt->Originator, 1, 0 );
	}

	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
}

/** Force-fire an event in the script. Useful for debugging. NOT CURRENTLY IMPLEMENTED. */
void WxKismet::OnContextFireEvent( wxCommandEvent& In )
{
	/*@todo - re-evaluate activation of events within kismet
	for(INT i=0; i<SelectedSeqObjs.Num(); i++)
	{
		USequenceEvent* SeqEvt = Cast<USequenceEvent>( SelectedSeqObjs(i) );
		if(SeqEvt && SeqEvt->Originator)
			SeqEvt->Originator->ActivateEvent(SeqEvt);
	}
	*/
}

/** Handler for double-clicking on Matinee action. Will open Matinee to edit the selected USeqAct_Interp (Matinee) action. */
void WxKismet::OnContextOpenInterpEdit( wxCommandEvent& In )
{
	check( SelectedSeqObjs.Num() == 1 );

	USeqAct_Interp* Interp = Cast<USeqAct_Interp>( SelectedSeqObjs(0) );
	check(Interp);

	OpenMatinee(Interp);
}

/**
 * Iterates through all specified sequence objects and sets
 * bHidden for any connectors that have no links.
 */
void WxKismet::HideConnectors(USequence *Sequence,TArray<USequenceObject*> &objList)
{
	// iterate through all selected sequence objects
	for (INT idx = 0; idx < objList.Num(); idx++)
	{
		USequenceOp *op = Cast<USequenceOp>(objList(idx));
		// if it is a valid operation
		if (op != NULL)
		{
			// first check all input connectors for active links
			for (INT linkIdx = 0; linkIdx < op->InputLinks.Num(); linkIdx++)
			{
				// if not already hidden,
				if (!op->InputLinks(linkIdx).bHidden)
				{
					// search for all output links pointing at this input link
					TArray<USequenceOp*> outputOps;
					TArray<INT> outputIndices;
					if (!FindOutputLinkTo(Sequence,op,linkIdx,outputOps,outputIndices))
					{
						// no links to this input, set as hidden
						op->InputLinks(linkIdx).bHidden = 1;
					}
				}
			}
			// next check all output connectors...
			for (INT linkIdx = 0; linkIdx < op->OutputLinks.Num(); linkIdx++)
			{
				// if not already hidden,
				if (!op->OutputLinks(linkIdx).bHidden)
				{
					if (op->OutputLinks(linkIdx).Links.Num() == 0)
					{
						// no links out, hidden
						op->OutputLinks(linkIdx).bHidden = 1;
					}
				}
			}
			// variable connectors...
			for (INT linkIdx = 0; linkIdx < op->VariableLinks.Num(); linkIdx++)
			{
				// if not already hidden,
				if (!op->VariableLinks(linkIdx).bHidden)
				{
					if (op->VariableLinks(linkIdx).LinkedVariables.Num() == 0)
					{
						// hidden
						op->VariableLinks(linkIdx).bHidden = 1;
					}
				}
			}
			// and finally events
			for (INT linkIdx = 0; linkIdx < op->EventLinks.Num(); linkIdx++)
			{
				// if not already hidden,
				if (!op->EventLinks(linkIdx).bHidden)
				{
					// check for any currently linked events
					if (op->EventLinks(linkIdx).LinkedEvents.Num() == 0)
					{
						// hide away
						op->EventLinks(linkIdx).bHidden = 1;
					}
				}
			}
		}
	}
}

/**
 * Iterates through all selected sequence objects and sets
 * bHidden for any connectors that have no links.
 */
void WxKismet::OnContextHideConnectors( wxCommandEvent& In )
{
	HideConnectors(Sequence,SelectedSeqObjs);
}

/**
 * Iterates through all sequence objects and hides any
 * unused connectors.
 */
void WxKismet::OnButtonHideConnectors(wxCommandEvent& In)
{
	HideConnectors(Sequence,Sequence->SequenceObjects);
}

/**
 * Shows all connectors for each object passed in.
 * 
 * @param	objList - list of all objects to show connectors
 */
static void ShowConnectors(TArray<USequenceObject*> &objList)
{
	// iterate through all objects
	for (INT idx = 0; idx < objList.Num(); idx++)
	{
		USequenceOp *op = Cast<USequenceOp>(objList(idx));
		// if it is a valid sequence op
		if (op != NULL)
		{
			// show all input connectors
			for (INT linkIdx = 0; linkIdx < op->InputLinks.Num(); linkIdx++)
			{
				op->InputLinks(linkIdx).bHidden = 0;
			}
			// output connectors
			for (INT linkIdx = 0; linkIdx < op->OutputLinks.Num(); linkIdx++)
			{
				op->OutputLinks(linkIdx).bHidden = 0;
			}
			// variable connectors
			for (INT linkIdx = 0; linkIdx < op->VariableLinks.Num(); linkIdx++)
			{
				op->VariableLinks(linkIdx).bHidden = 0;
			}
			// event connectors
			for (INT linkIdx = 0; linkIdx < op->EventLinks.Num(); linkIdx++)
			{
				op->EventLinks(linkIdx).bHidden = 0;
			}
		}
	}
}

/**
 * Iterates through all selected objects and unhides any perviously
 * hidden connectors, input/output/variable/event.
 */
void WxKismet::OnContextShowConnectors( wxCommandEvent& In )
{
	ShowConnectors(SelectedSeqObjs);
}

/**
 * Iterates through all sequence objects and shows all hidden connectors.
 */
void WxKismet::OnButtonShowConnectors( wxCommandEvent& In )
{
	ShowConnectors(Sequence->SequenceObjects);
}

/** Toggle drawing logic connections as straight lines or curves. */
void WxKismet::OnButtonShowCurves(wxCommandEvent &In)
{
	bDrawCurves = In.IsChecked();
	RefreshViewport();
}

/**
 * Iterates through all selected objects, removes them from their current
 * sequence and places them in a new one.
 */
void WxKismet::OnContextCreateSequence( wxCommandEvent& In )
{
	// pop up an option to name the new sequence
	WxDlgGenericStringEntry dlg;
	INT Result = dlg.ShowModal( TEXT("Name Sequence"), TEXT("Sequence Name:"), TEXT("Sub_Sequence") );
	if (Result == wxID_OK)
	{
		GEditor->Trans->Begin( TEXT("NewSequence") );
		Sequence->Modify();

		FString newSeqName = dlg.EnteredString;
		newSeqName = newSeqName.Replace(TEXT(" "),TEXT("_"));
		// create the new sequence
		USequence *newSeq = ConstructObject<USequence>(USequence::StaticClass(), Sequence, *newSeqName, RF_Transactional);
		check(newSeq != NULL && "Failed to create new sub sequence");
		newSeq->ObjName = newSeqName;
		newSeq->ObjPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
		newSeq->ObjPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
		newSeq->SnapPosition(KISMET_GRIDSIZE);

		newSeq->Modify();

		// add the new sequence to this sequence
		Sequence->SequenceObjects.AddItem(newSeq);

		// Now we have created our new sub-sequence, we basically cut and paste our selected objects into it.

		// Add current selection to GSelectionTools
		GSelectionTools.SelectNone( USequenceObject::StaticClass() );
		for(INT i=0; i<SelectedSeqObjs.Num(); i++)
		{
			GSelectionTools.Select( SelectedSeqObjs(i) );
		}

		// Export to text.
		FStringOutputDevice TempAr;
		UExporter::ExportToOutputDevice( Sequence, NULL, TempAr, TEXT("copy"), 0 );

		GSelectionTools.SelectNone( USequenceObject::StaticClass() );

		// Import into new sequence.
		const TCHAR* TempStr = *TempAr;
		USequenceFactory* Factory = new USequenceFactory;
		Factory->FactoryCreateText( NULL, NULL, newSeq, NAME_None, RF_Transactional, NULL, TEXT(""), TempStr, TempStr+appStrlen(TempStr), GWarn );
		delete Factory;

		newSeq->OnCreated();

		// May have disconnected some Matinee actions from their data...
		newSeq->UpdateInterpActionConnectors();

		GEditor->Trans->End();

		// Delete the selected SequenceObjects that we copied into new sequence. 
		// This is its own undo transaction.
		DeleteSelected();

		// Select just the newly created sequence.
		EmptySelection();
		AddToSelection(newSeq);

		// Rebuild the tree control to show new sequence.
		RebuildTreeControl();
	}
}

/**
 * Takes the currently selected sequence and exports it to a package.
 */
void WxKismet::OnContextExportSequence( wxCommandEvent& In )
{
	// iterate through selected objects, looking for any sequences
	for (INT idx = 0; idx < SelectedSeqObjs.Num(); idx++)
	{
		USequence *seq = Cast<USequence>(SelectedSeqObjs(idx));
		if (seq != NULL)
		{
			// first get the package to export to
			WxDlgRename renameDlg;
			renameDlg.SetTitle(*FString::Printf(TEXT("Export sequence %s"),seq->GetName()));
			FString newPkg, newGroup, newName = seq->GetName();
			if (renameDlg.ShowModal(newPkg,newGroup,newName) == wxID_OK)
			{
				// search for an existing object of that type
				if (renameDlg.NewName.Len() == 0 ||
					renameDlg.NewPackage.Len() == 0)
				{
					appMsgf(0,TEXT("Must specify valid sequence name and package."));
				}
				else
				{
					UPackage *pkg = GEngine->CreatePackage(NULL,*renameDlg.NewPackage);
					pkg->SetFlags(pkg->GetFlags() & RF_Standalone);
					if (renameDlg.NewGroup.Len() != 0)
					{
						pkg = GEngine->CreatePackage(pkg,*renameDlg.NewGroup);
						pkg->SetFlags(pkg->GetFlags() & RF_Standalone);
					}

					FStringOutputDevice TempAr;
					UExporter::ExportToOutputDevice( seq, NULL, TempAr, TEXT("t3d"), 0 );

					const TCHAR* TempStr = *TempAr;
					USequenceFactory* Factory = new USequenceFactory;
					UObject* NewObj = Factory->FactoryCreateText( NULL, NULL, pkg, NAME_None, RF_Public|RF_Standalone|RF_Transactional, NULL, TEXT(""), TempStr, TempStr+appStrlen(TempStr), GWarn );
					delete Factory;

					check(NewObj);
					check(NewObj->GetOuter() == pkg);
					USequence* NewSeq = CastChecked<USequence>(NewObj);

					// Set name of new sequence in package to desired name.
					NewSeq->Rename( *renameDlg.NewName, pkg );
					NewSeq->ObjName = FString(NewSeq->GetName());

					// and clear any unwanted properties
					NewSeq->OnExport();

					// Mark the package we exported this into as dirty
					NewSeq->MarkPackageDirty();
				}
			}
		}
	}
}

/** Handler for importing a USequence from a package. Finds the first currently selected USequence and copies it into the current sequence. */
void WxKismet::OnContextImportSequence( wxCommandEvent &In )
{
	USequence *seq = Cast<USequence>(GSelectionTools.GetTop(USequence::StaticClass()));
	// make a copy of the sequence
	if (seq != NULL)
	{
		GEditor->Trans->Begin( TEXT("ImportSequence") );
		Sequence->Modify();

		FStringOutputDevice TempAr;
		UExporter::ExportToOutputDevice( seq, NULL, TempAr, TEXT("t3d"), 0 );

		const TCHAR* TempStr = *TempAr;
		USequenceFactory* Factory = new USequenceFactory;
		UObject* NewObj = Factory->FactoryCreateText( NULL, NULL, Sequence, NAME_None, RF_Transactional, NULL, TEXT("paste"), TempStr, TempStr+appStrlen(TempStr), GWarn );
		delete Factory;

		check(NewObj);
		USequence* NewSeq = CastChecked<USequence>(NewObj);

		// and set the object position
		NewSeq->ObjPosX = (LinkedObjVC->NewX - LinkedObjVC->Origin2D.X)/LinkedObjVC->Zoom2D;
		NewSeq->ObjPosY = (LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y)/LinkedObjVC->Zoom2D;
		NewSeq->SnapPosition(KISMET_GRIDSIZE);

		EmptySelection();
		AddToSelection(NewSeq);

		GEditor->Trans->End();

		// Rebuild the tree to show new sequence.
		RebuildTreeControl();

		// Importing sequence may have changed named variable status, so update now.
		GEditor->Level->GameSequence->UpdateNamedVarStatus();
		GEditor->Level->GameSequence->UpdateInterpActionConnectors();
	}
}

/** Handler from context menu for pasting current selection of clipboard at mouse location. */
void WxKismet::OnContextPasteHere(wxCommandEvent& In)
{
	KismetPaste(true);
}

/**
 * Sets the currently selected actor as the originator of all selected
 * events that the actor supports.
 */
void WxKismet::OnContextLinkEvent( wxCommandEvent& In )
{
	GEditor->Trans->Begin( TEXT("LinkEvent") );

	for (INT idx = 0; idx < SelectedSeqObjs.Num(); idx++)
	{
		USequenceEvent *evt = Cast<USequenceEvent>(SelectedSeqObjs(idx));
		if (evt != NULL)
		{
			UBOOL bFoundMatch = 0;
			for (TSelectedObjectIterator It(AActor::StaticClass()); It && !bFoundMatch; ++It)
			{
				// if this actor supports this event
				AActor *actor = Cast<AActor>(*It);
				for (INT eIdx = 0; eIdx < actor->SupportedEvents.Num() && !bFoundMatch; eIdx++)
				{
					if (actor->SupportedEvents(eIdx)->IsChildOf(evt->GetClass()))
					{
						bFoundMatch = 1;
					}
				}
				if (bFoundMatch)
				{
					evt->Modify();

					evt->Originator = actor;
					// reset the title bar
					evt->ObjName = FString::Printf( TEXT("%s %s"), actor->GetName(), *((USequenceObject*)evt->GetClass()->GetDefaultObject())->ObjName );
				}
			}
		}
	}

	GEditor->Trans->End();
}

/**
 * Sets the currently selected actor as the value for all selected
 * object variables.
 */
void WxKismet::OnContextLinkObject( wxCommandEvent& In )
{
	GEditor->Trans->Begin( TEXT("LinkObject") );

	for (INT idx = 0; idx < SelectedSeqObjs.Num(); idx++)
	{
		USeqVar_Object *objVar = Cast<USeqVar_Object>(SelectedSeqObjs(idx));
		if (objVar != NULL)
		{
			objVar->Modify();
			objVar->ObjValue = GSelectionTools.GetTop(AActor::StaticClass());
		}
	}

	GEditor->Trans->End();
}

/**
 * Pops up a rename dialog box for each currently selected sequence.
 */
void WxKismet::OnContextRenameSequence( wxCommandEvent& In )
{
	GEditor->Trans->Begin( TEXT("RenameSequence") );

	for (INT idx = 0; idx < SelectedSeqObjs.Num(); idx++)
	{
		USequence *seq = Cast<USequence>(SelectedSeqObjs(idx));
		if (seq != NULL)
		{
			WxDlgGenericStringEntry dlg;
			INT Result = dlg.ShowModal( TEXT("Name Sequence"), TEXT("Sequence Name:"), seq->GetName() );
			if (Result == wxID_OK)
			{
				seq->Modify();
				FString newSeqName = dlg.EnteredString;
				newSeqName = newSeqName.Replace(TEXT(" "),TEXT("_"));
				seq->Rename(*newSeqName,seq->GetOuter());
				seq->ObjName = FString(seq->GetName());
			}
		}
	}

	GEditor->Trans->End();

	RebuildTreeControl();
}

/** Find the uses the selected named variable. */
void WxKismet::OnContextFindNamedVarUses( wxCommandEvent& In )
{
	// Only works if only 1 thing selected
	if(SelectedSeqObjs.Num() == 1)
	{
		// Must be a SequenceVariable
		USequenceVariable* SeqVar = Cast<USequenceVariable>( SelectedSeqObjs(0) );
		if(SeqVar && SeqVar->VarName != NAME_None)
		{
			DoSearch( *(SeqVar->VarName), KST_VarNameUses, false );
		}
	}
}

/** Find the variable(s) definition with the name used by the selected SeqVar_Named. */
void WxKismet::OnContextFindNamedVarDefs( wxCommandEvent& In )
{
	// Only works if only 1 thing selected
	if(SelectedSeqObjs.Num() == 1)
	{
		// Must be a USeqVar_Named
		USeqVar_Named* NamedVar = Cast<USeqVar_Named>( SelectedSeqObjs(0) );
		if(NamedVar && NamedVar->FindVarName != NAME_None)
		{
			DoSearch( *(NamedVar->FindVarName), KST_VarName, true );
		}
	}
}

/**
 * Activates the outer sequence of the current sequence.
 */
void WxKismet::OnButtonParentSequence( wxCommandEvent &In )
{
	USequence *outerSeq = Cast<USequence>(Sequence->GetOuter());
	if (outerSeq != NULL)
	{
		ChangeActiveSequence(outerSeq);
		GEditor->Trans->Reset( TEXT("ParentSequence") );
	}
}

/**
 * Pops up a dialog box and renames the current sequence.
 */
void WxKismet::OnButtonRenameSequence( wxCommandEvent &In )
{
	GEditor->Trans->Begin( TEXT("LinkObject") );

	WxDlgGenericStringEntry dlg;

	INT Result = dlg.ShowModal( TEXT("Name Sequence"), TEXT("Sequence Name:"), Sequence->GetName() );
	if (Result == wxID_OK)
	{
		Sequence->Modify();

		FString newSeqName = dlg.EnteredString;
		newSeqName = newSeqName.Replace(TEXT(" "),TEXT("_"));
		Sequence->Rename(*newSeqName,Sequence->GetOuter());
		Sequence->ObjName = FString( Sequence->GetName() );

		// reactivate with new name
		ChangeActiveSequence(Sequence);
	}

	GEditor->Trans->End();

	RebuildTreeControl();
}

/** Toggle the 'search for SequencesObjects'. */
void WxKismet::OnOpenSearch(wxCommandEvent &In)
{
	if(!SearchWindow)
	{
		SearchWindow = new WxKismetSearch( this, -1 );
		SearchWindow->Show();

		ToolBar->ToggleTool(IDM_KISMET_OPENSEARCH, true);
	}
	else
	{
		SearchWindow->Close();

		ToolBar->ToggleTool(IDM_KISMET_OPENSEARCH, false);
	}
}

/** 
 *	Handler for pressing Search button (or pressing Enter on entry box) on the Kismet Search tool.
 *	Searches all sequences and puts results into the ResultList list box.
 */
void WxKismet::OnDoSearch(wxCommandEvent &In)
{
	// Should not be able to call this if the Search dialog is not up!
	check(SearchWindow);

	TArray<USequenceObject*> Results;

	// Get strings to look for from entry dialogs
	FString SearchString = SearchWindow->NameEntry->GetValue();

	if(SearchString.Len() > 0)
	{
		INT TypeIndex = SearchWindow->SearchTypeCombo->GetSelection();
		FName SearchName = FName(*SearchString);

		// Search the entire levels sequence.
		USequence* RootSeq = GEditor->Level->GameSequence;
		check(RootSeq); // Should not have a Kismet open if no sequence in level!

		// Comment/Name
		if(TypeIndex == KST_NameComment)
		{
			RootSeq->FindSeqObjectsByName(SearchString, true, Results);
		}
		// Reference Object
		else if(TypeIndex == KST_ObjName)
		{
			RootSeq->FindSeqObjectsByObjectName(SearchName, Results);
		}
		// Named Variable (uses and declarations)
		else if(TypeIndex == KST_VarName || TypeIndex == KST_VarNameUses)
		{
			TArray<USequenceVariable*> ResultVars;

			if(TypeIndex == KST_VarNameUses)
			{
				RootSeq->FindNamedVariables(SearchName, true, ResultVars);
			}
			else
			{
				RootSeq->FindNamedVariables(SearchName, false, ResultVars);
			}

			// Copy results into Results array.
			for(INT i=0; i<ResultVars.Num(); i++)
			{
				Results.AddItem( ResultVars(i) );
			}
		}
	}

	// Update the list box with search results.
	SearchWindow->ResultList->Clear();

	for(INT i=0; i<Results.Num(); i++)
	{
		check( Results(i) );
		FString ResultName = Results(i)->GetSeqObjFullName();

		SearchWindow->ResultList->Append( *ResultName, Results(i) );
	}
}

/** Called when user clicks on a search result. Changes active Sequence and moves the view to center on the selected SequenceObject. */
void WxKismet::OnSearchResultChanged(wxCommandEvent &In)
{
	INT SelIndex = SearchWindow->ResultList->GetSelection();
	if(SelIndex == -1)
		return;

	// Get the selected SequenceObject pointer.
	USequenceObject* SeqObj = (USequenceObject*)SearchWindow->ResultList->GetClientData(SelIndex);
	check(SeqObj);

	CenterViewOnSeqObj(SeqObj);
}

/** 
 *	Run a search now. 
 *	This will open the search window if not already open, set the supplied settings and run the search.
 *	It will also jump to the first search result (if there are any) if you wish.
 */
void WxKismet::DoSearch(const TCHAR* InText, EKismetSearchType Type, UBOOL bJumpToFirstResult)
{
	// If search window is not currently open - open it now.
	if(!SearchWindow)
	{
		OnOpenSearch( wxCommandEvent() );
	}
	check(SearchWindow);
	
	// Enter the selected text into the search box, select search type, and start the search.
	SearchWindow->NameEntry->SetValue(InText);
	SearchWindow->SearchTypeCombo->SetSelection(Type);
	OnDoSearch( wxCommandEvent() );

	// If we had some valid results, select the first one and move to it.
	if(bJumpToFirstResult && SearchWindow->ResultList->GetCount() > 0 )
	{
		// Highlight first element (this doesn't generate an Event, so we manually call OnSearchResultChanged afterwards).
		SearchWindow->ResultList->SetSelection(0, true);
		OnSearchResultChanged(  wxCommandEvent() );
	}
}

/** Update the NewObjActors and NewEventClasses arrays based on currently selected Actors. */
void WxKismet::BuildSelectedActorLists()
{
	// add menu for context sensitive events using the selected actors
	UBOOL bHaveClasses = false;
	NewEventClasses.Empty();
	NewObjActors.Empty();

	for(INT i=0; i<GUnrealEd->Level->Actors.Num(); i++)
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if( Actor && GSelectionTools.IsSelected( Actor ) )
		{
			// If first actor - add all the events it supports.
			if(!bHaveClasses)
			{
				for(INT j=0; j<Actor->SupportedEvents.Num(); j++)
				{
					check( Actor->SupportedEvents(j)->IsChildOf(USequenceEvent::StaticClass()) );
					NewEventClasses.AddItem( Actor->SupportedEvents(j) );
				}

				bHaveClasses = true;
			}
			else // If a subsequent actor, remove any events from NewEventClasses which are not in this actors list.
			{
				INT EvtIdx = 0;
				while( EvtIdx < NewEventClasses.Num() )
				{
					if( !Actor->SupportedEvents.ContainsItem( NewEventClasses(EvtIdx) ) )
						NewEventClasses.Remove( EvtIdx );
					else
						EvtIdx++;
				}
			}

			NewObjActors.AddItem( Actor );
		}
	}
}
	
/** 
 *	Calculate the bounding box that encompasses the selected SequenceObjects. 
 *	Does not produce sensible result if nothing is selected. 
 */
FIntRect WxKismet::CalcBoundingBoxOfSelected()
{
	FIntRect Result(0, 0, 0, 0);
	UBOOL bResultValid = false;

	for(INT i=0; i<SelectedSeqObjs.Num(); i++)
	{
		FIntRect ObjBox = SelectedSeqObjs(i)->GetSeqObjBoundingBox();

		if(bResultValid)
		{
			// Expand result box to encompass
			Result.Min.X = ::Min(Result.Min.X, ObjBox.Min.X);
			Result.Min.Y = ::Min(Result.Min.Y, ObjBox.Min.Y);

			Result.Max.X = ::Max(Result.Max.X, ObjBox.Max.X);
			Result.Max.Y = ::Max(Result.Max.Y, ObjBox.Max.Y);
		}
		else
		{
			// Use this objects bounding box to initialise result.
			Result = ObjBox;
			bResultValid = true;
		}
	}

	return Result;
}

/** 
 *	Calculate the bounding box that encompasses SequenceObjects in the current Sequence. 
 *	Does not produce sensible result if no objects in sequence. 
 */
FIntRect WxKismet::CalcBoundingBoxOfAll()
{
	FIntRect Result(0, 0, 0, 0);
	UBOOL bResultValid = false;

	for(INT i=0; i<Sequence->SequenceObjects.Num(); i++)
	{
		FIntRect ObjBox = Sequence->SequenceObjects(i)->GetSeqObjBoundingBox();

		if(bResultValid)
		{
			Result.Min.X = ::Min(Result.Min.X, ObjBox.Min.X);
			Result.Min.Y = ::Min(Result.Min.Y, ObjBox.Min.Y);

			Result.Max.X = ::Max(Result.Max.X, ObjBox.Max.X);
			Result.Max.Y = ::Max(Result.Max.Y, ObjBox.Max.Y);
		}
		else
		{
			Result = ObjBox;
			bResultValid = true;
		}
	}

	return Result;
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
