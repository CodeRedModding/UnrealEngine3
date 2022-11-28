/*=============================================================================
	KismetMenus.cpp: Menus/toolbars/dialogs for Kismet
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"


/*-----------------------------------------------------------------------------
	WxKismetToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxKismetToolBar, wxToolBar )
END_EVENT_TABLE()

WxKismetToolBar::WxKismetToolBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_3DBUTTONS )
{
	// create the return to parent sequence button
	ParentSequenceB.Load(TEXT("KIS_Parent"));
	RenameSequenceB.Load(TEXT("KIS_Rename"));
	HideB.Load(TEXT("KIS_HideConnectors"));
	ShowB.Load(TEXT("KIS_ShowConnectors"));
	CurvesB.Load(TEXT("KIS_DrawCurves"));
	SearchB.Load( TEXT("KIS_Search") );
	ZoomToFitB.Load( TEXT("KIS_ZoomToFit") );

	SetToolBitmapSize( wxSize( 18, 18 ) );

	AddSeparator();
	AddTool(IDM_KISMET_PARENTSEQ, ParentSequenceB, TEXT("Open Parent Sequence"));
	AddTool(IDM_KISMET_RENAMESEQ, RenameSequenceB, TEXT("Rename Sequence"));
	AddSeparator();
	AddTool(IDM_KISMET_BUTTON_ZOOMTOFIT, ZoomToFitB, TEXT("Zoom To Fit"));
	AddSeparator();
	AddTool(IDM_KISMET_BUTTON_HIDE_CONNECTORS, HideB, TEXT("Hide unused connectors"));
	AddTool(IDM_KISMET_BUTTON_SHOW_CONNECTORS, ShowB, TEXT("Show all connectors"));
	AddSeparator();
	AddCheckTool(IDM_KISMET_BUTTON_SHOW_CURVES, TEXT("Toggle curved connections"), CurvesB);
	ToggleTool(IDM_KISMET_BUTTON_SHOW_CURVES, true);
	AddSeparator();
	AddCheckTool(IDM_KISMET_OPENSEARCH, TEXT("Search Tool"), SearchB);

	Realize();
}

WxKismetToolBar::~WxKismetToolBar()
{
}

/*-----------------------------------------------------------------------------
	WxKismetStatusBar.
-----------------------------------------------------------------------------*/

WxKismetStatusBar::WxKismetStatusBar( wxWindow* InParent, wxWindowID InID )
	: wxStatusBar( InParent, InID )
{
	INT Widths[2] = {-1, 150};

	SetFieldsCount(2, Widths);

	SetStatusText( TEXT("Object: None"), 0 );
}

WxKismetStatusBar::~WxKismetStatusBar()
{

}

void WxKismetStatusBar::SetMouseOverObject(USequenceObject* SeqObj)
{
	FString ObjInfo(TEXT("Object: [None]"));
	if(SeqObj)
	{
		USeqVar_Named* NamedVar = Cast<USeqVar_Named>(SeqObj);
		if(NamedVar)
		{
			FString NamedStatus;
			NamedVar->UpdateStatus(&NamedStatus);
			ObjInfo = FString::Printf( TEXT("Object: [%s]      Status: %s"), *SeqObj->ObjName, *NamedStatus );
		}
		else
		{
			ObjInfo = FString::Printf( TEXT("Object: [%s]"), *SeqObj->ObjName );
		}
	}

	SetStatusText( *ObjInfo, 0 );
}

/*-----------------------------------------------------------------------------
	WxMBKismetNewObject.
-----------------------------------------------------------------------------*/

WxMBKismetNewObject::WxMBKismetNewObject(WxKismet* SeqEditor)
{
	ActionMenu = new wxMenu();
	Append( IDM_KISMET_NEW_ACTION, TEXT("New Action"), ActionMenu );

	ConditionMenu = new wxMenu();
	Append( IDM_KISMET_NEW_CONDITION, TEXT("New Condition"), ConditionMenu );

	VariableMenu = new wxMenu();
	Append( IDM_KISMET_NEW_VARIABLE, TEXT("New Variable"), VariableMenu );

	EventMenu = new wxMenu();
	Append( IDM_KISMET_NEW_EVENT, TEXT("New Event"), EventMenu );

	for(INT i=0; i<SeqEditor->SeqObjClasses.Num(); i++)
	{
		USequenceObject* SeqObjDefault = (USequenceObject*)SeqEditor->SeqObjClasses(i)->GetDefaultObject();

		if( SeqEditor->SeqObjClasses(i)->IsChildOf(USequenceAction::StaticClass()) )
		{
			ActionMenu->Append( IDM_NEW_SEQUENCE_OBJECT_START+i, *SeqObjDefault->ObjName, TEXT("") );
		}
		else if( SeqEditor->SeqObjClasses(i)->IsChildOf(USequenceCondition::StaticClass()) )
		{
			ConditionMenu->Append( IDM_NEW_SEQUENCE_OBJECT_START+i, *SeqObjDefault->ObjName, TEXT("") );
		}
		else if( SeqEditor->SeqObjClasses(i)->IsChildOf(USequenceVariable::StaticClass()) )
		{
			VariableMenu->Append( IDM_NEW_SEQUENCE_OBJECT_START+i, *SeqObjDefault->ObjName, TEXT("") );
		}
		else if( SeqEditor->SeqObjClasses(i)->IsChildOf(USequenceEvent::StaticClass()) )
		{
			EventMenu->Append( IDM_NEW_SEQUENCE_OBJECT_START+i, *SeqObjDefault->ObjName, TEXT("") );
		}
		else if( SeqEditor->SeqObjClasses(i)->IsChildOf(USequenceFrame::StaticClass()) )
		{
			Append( IDM_NEW_SEQUENCE_OBJECT_START+i, TEXT("New Comment"), TEXT("") );
		}
	}

	// Update SeqEditor->NewEventClasses and SeqEditor->NewObjActors.
	SeqEditor->BuildSelectedActorLists();

	// Add 'New Event Using' item.
	if(SeqEditor->NewEventClasses.Num() > 0)
	{
		ContextEventMenu = new wxMenu();

		FString NewEventString;
		FString NewObjVarString;

		const TCHAR* FirstObjName = SeqEditor->NewObjActors(0)->GetName();
		if( SeqEditor->NewObjActors.Num() > 1 )
		{
			NewEventString = FString::Printf( TEXT("New Events Using %s,..."), FirstObjName );
			NewObjVarString = FString::Printf( TEXT("New Object Vars Using %s,..."), FirstObjName );
		}
		else
		{
			NewEventString = FString::Printf( TEXT("New Event Using %s"), FirstObjName );
			NewObjVarString = FString::Printf( TEXT("New Object Var Using %s"), FirstObjName );
		}

		Append( IDM_KISMET_NEW_VARIABLE_OBJ_CONTEXT, *NewObjVarString, TEXT("") );
		SeqEditor->bAttachVarsToConnector = false;

		Append( IDM_KISMET_NEW_EVENT_CONTEXT, *NewEventString, ContextEventMenu );

		for(INT i=0; i<SeqEditor->NewEventClasses.Num(); i++)
		{
			USequenceEvent* SeqEvtDefault = (USequenceEvent*)SeqEditor->NewEventClasses(i)->GetDefaultObject();
			ContextEventMenu->Append( IDM_NEW_SEQUENCE_EVENT_START+i, *SeqEvtDefault->ObjName, TEXT("") );
		}
	}
	
	// Append the option to make a new sequence
	Append(IDM_KISMET_CREATE_SEQUENCE,TEXT("Create new sequence"),TEXT(""));

	// Append option for pasting clipboard at current mouse location
	Append(IDM_KISMET_PASTE_HERE, TEXT("Paste Here"), TEXT(""));

	// append option to import a new sequence
	USequence *selectedSeq = Cast<USequence>(GSelectionTools.GetTop(USequence::StaticClass()));
	if (selectedSeq != NULL)
	{
		Append(IDM_KISMET_IMPORT_SEQUENCE,*FString::Printf(TEXT("Import sequence %s"),selectedSeq->GetName()),TEXT(""));
	}

}

WxMBKismetNewObject::~WxMBKismetNewObject()
{
}

/*-----------------------------------------------------------------------------
	WxMBKismetObjectOptions.
-----------------------------------------------------------------------------*/

WxMBKismetObjectOptions::WxMBKismetObjectOptions(WxKismet* SeqEditor)
{
	NewVariableMenu = NULL;

	// If one Op is selected, and there are classes of optional variable connectors defined, create menu for them.
	if(SeqEditor->SelectedSeqObjs.Num() == 1)
	{
		USequenceOp* SeqOp = Cast<USequenceOp>( SeqEditor->SelectedSeqObjs(0) );
	
		USeqAct_Interp* Interp = Cast<USeqAct_Interp>( SeqOp );
		if(Interp)
		{
			// HI JAMES!!!!
			Append( IDM_KISMET_OPEN_INTERPEDIT, TEXT("Open Matinee"), TEXT("") );
		}
	}

	Append( IDM_KISMET_DELETE_OBJECT, TEXT("Delete selected object(s)"), TEXT("") );

	// search for the first selected event
	USequenceEvent *selectedEvt = NULL;
	USeqVar_Object *selectedObjVar = NULL;
	for(INT i=0; i<SeqEditor->SelectedSeqObjs.Num() && selectedEvt == NULL && selectedObjVar == NULL; i++)
	{
		if (selectedEvt == NULL)
		{
			selectedEvt = Cast<USequenceEvent>( SeqEditor->SelectedSeqObjs(i) );
		}
		if (selectedObjVar == NULL)
		{
			selectedObjVar = Cast<USeqVar_Object>( SeqEditor->SelectedSeqObjs(i) );
		}
	}
	AActor *selectedActor = Cast<AActor>(GSelectionTools.GetTop(AActor::StaticClass()));
	if(selectedEvt != NULL)
	{
		/*@todo - reenable once we have editor execution of kismet re-implemented
		Append( IDM_KISMET_SELECT_EVENT_ACTORS, TEXT("Select Event(s) Actor(s)"), TEXT("") );
		Append( IDM_KISMET_FIRE_EVENT, TEXT("Force Fire Event"), TEXT("") );
		*/
		// add option to link selected actor to all selected events
		if (selectedActor != NULL)
		{
			// make sure the selected actor supports the event selected
			UBOOL bFoundMatch = 0;
			for (INT idx = 0; idx < selectedActor->SupportedEvents.Num() && !bFoundMatch; idx++)
			{
				if (selectedActor->SupportedEvents(idx)->IsChildOf(selectedEvt->GetClass()))
				{
					bFoundMatch = 1;
				}
			}
			if (bFoundMatch)
			{
				Append(IDM_KISMET_LINK_ACTOR_TO_EVT, *FString::Printf(TEXT("Assign %s to event(s)"),selectedActor->GetName()), TEXT(""));
			}
		}
	}
	if (selectedObjVar != NULL &&
		selectedActor != NULL)
	{
		Append(IDM_KISMET_LINK_ACTOR_TO_OBJ, *FString::Printf(TEXT("Assign %s to object variable(s)"),selectedActor->GetName()), TEXT(""));
	}

	// if we have an object var selected,
	if (selectedObjVar != NULL)
	{
		// add an option to clear it's contents
		Append(IDM_KISMET_CLEAR_VARIABLE, TEXT("Clear object variable(s)"), TEXT(""));
	}


	// Add options for finding definitions/uses of named variables.
	if( SeqEditor->SelectedSeqObjs.Num() == 1 )
	{
		USequenceVariable* SeqVar = Cast<USequenceVariable>( SeqEditor->SelectedSeqObjs(0) );
		if(SeqVar && SeqVar->VarName != NAME_None)
		{
			FString FindUsesString = FString::Printf( TEXT("Find Uses Of %s"), *(SeqVar->VarName) );
			Append( IDM_KISMET_FIND_NAMEDVAR_USES, *FindUsesString, TEXT("") );
		}

		USeqVar_Named* NamedVar = Cast<USeqVar_Named>( SeqEditor->SelectedSeqObjs(0) );
		if(NamedVar && NamedVar->FindVarName != NAME_None)
		{
			FString FindDefsString = FString::Printf( TEXT("Find Definition Of %s"), *(NamedVar->FindVarName) );
			Append( IDM_KISMET_FIND_NAMEDVAR, *FindDefsString, TEXT("") );
		}
	}

	AppendSeparator();

	// add options to hide unused connectors or show all connectors
	Append(IDM_KISMET_HIDE_CONNECTORS,TEXT("Hide unused connectors"),TEXT(""));
	Append(IDM_KISMET_SHOW_CONNECTORS,TEXT("Show all connectors"),TEXT(""));

	AppendSeparator();

    // add option to create a new sequence
	Append(IDM_KISMET_CREATE_SEQUENCE,TEXT("Create new sequence"),TEXT(""));
	if (SeqEditor->SelectedSeqObjs.Num() == 1 &&
		Cast<USequence>(SeqEditor->SelectedSeqObjs(0)) != NULL)
	{
		// and the option to rename
		Append(IDM_KISMET_RENAMESEQ,TEXT("Rename selected sequence"),TEXT(""));
		// if we have a sequence selected, add the option to export
		Append(IDM_KISMET_EXPORT_SEQUENCE,TEXT("Export sequence to package"),TEXT(""));
	}
}

WxMBKismetObjectOptions::~WxMBKismetObjectOptions()
{

}

/*-----------------------------------------------------------------------------
	WxMBKismetConnectorOptions.
-----------------------------------------------------------------------------*/

WxMBKismetConnectorOptions::WxMBKismetConnectorOptions(WxKismet* SeqEditor)
{
	USequenceOp* ConnSeqOp = SeqEditor->ConnSeqOp;
	INT ConnIndex = SeqEditor->ConnIndex;
	INT ConnType = SeqEditor->ConnType;

	// Add menu option for breaking links. Only input and variable links actually store pointers.
	if( ConnType == LOC_OUTPUT )
	{
		Append( IDM_KISMET_BREAK_LINK_ALL, TEXT("Break All Links"), TEXT("") );
		BreakLinkMenu = new wxMenu();
		Append(IDM_KISMET_BREAK_LINK_MENU,TEXT("Break Link To"),BreakLinkMenu);
		// add individual links
		for (INT idx = 0; idx < ConnSeqOp->OutputLinks(ConnIndex).Links.Num(); idx++)
		{
			FSeqOpOutputInputLink &link = ConnSeqOp->OutputLinks(ConnIndex).Links(idx);
			if (link.LinkedOp != NULL)
			{
				BreakLinkMenu->Append(IDM_KISMET_BREAK_LINK_START+idx,
									  *FString::Printf(TEXT("%s (%s)"),*link.LinkedOp->ObjName,link.LinkedOp->GetName()),
									  TEXT(""));
			}
		}
	}
	else
	if (ConnType == LOC_INPUT)
	{
		// figure out if there is a link to this connection
		TArray<USequenceOp*> outputOps;
		TArray<INT> outputIndices;
		if (WxKismet::FindOutputLinkTo(SeqEditor->Sequence,ConnSeqOp,ConnIndex,outputOps,outputIndices))
		{
			// add the break all option if there are multiple links
			Append(IDM_KISMET_BREAK_LINK_ALL,TEXT("Break All Links"),TEXT(""));
			// add the submenu for breaking individual links
			BreakLinkMenu = new wxMenu();
			Append(IDM_KISMET_BREAK_LINK_MENU,TEXT("Break Link To"),BreakLinkMenu);
			// and add the individual link options
			for (INT idx = 0; idx < outputOps.Num(); idx++)
			{
				// add a submenu for this link
				BreakLinkMenu->Append(IDM_KISMET_BREAK_LINK_START+idx,*FString::Printf(TEXT("%s (%s)"),*outputOps(idx)->ObjName,outputOps(idx)->GetName()),TEXT(""));
			}
		}
	}
	else if( ConnType == LOC_VARIABLE )
	{
		if( ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables.Num() > 0 )
		{
			Append( IDM_KISMET_BREAK_LINK_ALL, TEXT("Break All Links"), TEXT("") );
			BreakLinkMenu = new wxMenu();
			Append(IDM_KISMET_BREAK_LINK_MENU,TEXT("Break Link To"),BreakLinkMenu);
			// add the individual variable links
			for (INT idx = 0; idx < ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables.Num(); idx++)
			{
				USequenceVariable *var = ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables(idx);
				BreakLinkMenu->Append(IDM_KISMET_BREAK_LINK_START+idx,
									  *FString::Printf(TEXT("%s (%s)"),*var->GetValueStr(),var->GetName()),
									  TEXT(""));
			}
		}
		FSeqVarLink &varLink = ConnSeqOp->VariableLinks(ConnIndex);
		// Do nothing if ExpectedType is NULL!
		if(varLink.ExpectedType != NULL)
		{
			Append(IDM_KISMET_CREATE_LINKED_VARIABLE, *FString::Printf(TEXT("Create new %s variable"),((USequenceVariable*)varLink.ExpectedType->GetDefaultObject())->ObjName), TEXT(""));

			// If this expects an object variable - offer option of creating a SeqVar_Object for each selected Actor and connecting it.
			if( varLink.ExpectedType == USeqVar_Object::StaticClass() )
			{
				SeqEditor->BuildSelectedActorLists();

				if(SeqEditor->NewObjActors.Num() > 0)
				{
					FString NewObjVarString;
					const TCHAR* FirstObjName = SeqEditor->NewObjActors(0)->GetName();
					if( SeqEditor->NewObjActors.Num() > 1 )
					{
						NewObjVarString = FString::Printf( TEXT("New Object Vars Using %s,..."), FirstObjName );
					}
					else
					{
						NewObjVarString = FString::Printf( TEXT("New Object Var Using %s"), FirstObjName );
					}

					Append( IDM_KISMET_NEW_VARIABLE_OBJ_CONTEXT, *NewObjVarString, TEXT("") );
					SeqEditor->bAttachVarsToConnector = true;
				}
			}
		}
	}
	else if (ConnType == LOC_EVENT)
	{
		if (ConnSeqOp->EventLinks(ConnIndex).LinkedEvents.Num() > 0)
		{
			Append(IDM_KISMET_BREAK_LINK_ALL, TEXT("Break Event Links"),TEXT(""));
		}
	}
}

WxMBKismetConnectorOptions::~WxMBKismetConnectorOptions()
{

}

/*-----------------------------------------------------------------------------
	WxKismetSearch.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxKismetSearch, wxFrame )
	EVT_CLOSE( WxKismetSearch::OnClose )
END_EVENT_TABLE()


static const INT ControlBorder = 5;

WxKismetSearch::WxKismetSearch(WxKismet* InKismet, wxWindowID InID)
	: wxFrame( InKismet, InID, TEXT("Kismet Search"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR )
{
	Kismet = InKismet;

	// Why do we need to do this? Why is it an unpleasant grey by default??
	this->SetBackgroundColour( wxColor(224, 223, 227) );

	wxBoxSizer* TopSizerV = new wxBoxSizer( wxVERTICAL );
	this->SetSizer(TopSizerV);
	this->SetAutoLayout(true);

	// Result list box
    wxStaticText* ResultCaption = new wxStaticText( this, -1, TEXT("Results:"), wxDefaultPosition, wxDefaultSize, 0 );
    TopSizerV->Add(ResultCaption, 0, wxALIGN_LEFT|wxALL, ControlBorder);

	ResultList = new wxListBox( this, IDM_KISMET_SEARCHRESULT, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE );
	TopSizerV->Add( ResultList, 1, wxGROW | wxALL, ControlBorder );


	wxFlexGridSizer* GridSizer = new wxFlexGridSizer( 2, 2, 0, 0 );
	GridSizer->AddGrowableCol(1);
	TopSizerV->Add( GridSizer, 0, wxGROW | wxALL, ControlBorder );

	// Name/Comment search entry
	wxStaticText* NameCaption = new wxStaticText( this, -1, TEXT("Search:") );
	GridSizer->Add(NameCaption, 0, wxRIGHT | wxALL, ControlBorder);

	NameEntry = new wxTextCtrl(this, IDM_KISMET_SEARCHENTRY, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	GridSizer->Add(NameEntry, 1, wxGROW | wxALL, ControlBorder);
	
	// Object search entry
	wxStaticText* SearchTypeCaption = new wxStaticText( this, -1, TEXT("Search Type:") );
	GridSizer->Add(SearchTypeCaption, 0, wxRIGHT | wxALL, ControlBorder);

	SearchTypeCombo = new wxComboBox(this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	GridSizer->Add(SearchTypeCombo, 1, wxGROW | wxALL, ControlBorder);

	SearchTypeCombo->Append( TEXT("Comments/Names") );			// KST_NameComment
	SearchTypeCombo->Append( TEXT("Referenced Object Name") );	// KST_ObjName
	SearchTypeCombo->Append( TEXT("Named Variable") );			// KST_VarName
	SearchTypeCombo->Append( TEXT("Named Variable Use") );		// KST_VarNameUsed

	SearchTypeCombo->SetSelection(KST_NameComment); // Default

	// Search button
	SearchButton = new wxButton( this, IDM_KISMET_DOSEARCH, TEXT("Search"), wxDefaultPosition, wxDefaultSize, 0 );
	TopSizerV->Add(SearchButton, 0, wxCENTER | wxALL, ControlBorder);

	FWindowUtil::LoadPosSize( TEXT("KismetSearch"), this, 100, 100, 500, 300 );
}

WxKismetSearch::~WxKismetSearch()
{
}

void WxKismetSearch::OnClose(wxCloseEvent& In)
{
	FWindowUtil::SavePosSize( TEXT("KismetSearch"), this );	

	check(Kismet->SearchWindow == this);
	Kismet->SearchWindow = NULL;

	Kismet->ToolBar->ToggleTool(IDM_KISMET_OPENSEARCH, false);

	this->Destroy();
}