/*=============================================================================
	KismetMenus.cpp: Menus/toolbars/dialogs for Kismet
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "Kismet.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "KismetDebugger.h"

#if WITH_GFx
#include "GFxUIClasses.h"
#include "GFxUIUISequenceClasses.h"
#endif

/*-----------------------------------------------------------------------------
	WxKismetToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxKismetToolBar, WxToolBar )
END_EVENT_TABLE()

WxKismetToolBar::WxKismetToolBar( wxWindow* InParent, wxWindowID InID )
	: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	// create the return to parent sequence button
	ParentSequenceB.Load(TEXT("UpArrow.png"));
	RenameSequenceB.Load(TEXT("KIS_Rename"));
	HideB.Load(TEXT("KIS_HideConnectors"));
	ShowB.Load(TEXT("KIS_ShowConnectors"));
	CurvesB.Load(TEXT("KIS_DrawCurves"));
	SearchB.Load( TEXT("KIS_Search") );
	ZoomToFitB.Load( TEXT("KIS_ZoomToFit") );
	UpdateB.Load(TEXT("KIS_Update"));
	OpenB.Load(TEXT("Kismet"));
	CreateSeqObjB.Load( TEXT("UnlitMovement") );
	ClearBreakPointsB.Load( TEXT("KIS_ClearBreakpoints") );
	
	SetToolBitmapSize( wxSize( 18, 18 ) );

	//AddSeparator();
	AddTool(IDM_KISMET_PARENTSEQ, ParentSequenceB, *LocalizeUnrealEd("OpenParentSequence"));
	AddTool(IDM_KISMET_RENAMESEQ, RenameSequenceB, *LocalizeUnrealEd("RenameSequence"));
	AddSeparator();
	AddTool(IDM_KISMET_BUTTON_ZOOMTOFIT, ZoomToFitB, *LocalizeUnrealEd("ZoomToFit"));
	AddSeparator();
	AddTool(IDM_KISMET_BUTTON_HIDE_CONNECTORS, HideB, *LocalizeUnrealEd("HideUnusedConnectors"));
	AddTool(IDM_KISMET_BUTTON_SHOW_CONNECTORS, ShowB, *LocalizeUnrealEd("ShowAllConnectors"));
	AddSeparator();
	AddCheckTool(IDM_KISMET_OPENCLASSSEARCH, *LocalizeUnrealEd("CreateSeqObj"), CreateSeqObjB, wxNullBitmap, *LocalizeUnrealEd("CreateSeqObj") );
	AddCheckTool(IDM_KISMET_OPENSEARCH, *LocalizeUnrealEd("SearchTool"), SearchB, wxNullBitmap, *LocalizeUnrealEd("SearchTool") );
	AddCheckTool(IDM_KISMET_OPENUPDATE, *LocalizeUnrealEd("UpdateList"), UpdateB, wxNullBitmap, *LocalizeUnrealEd("UpdateList") );
	AddSeparator();
	AddTool(IDM_KISMET_OPENNEWWINDOW, OpenB, *LocalizeUnrealEd("OpenNewKismetWindow"));
	AddSeparator();
	AddTool(IDM_KISMET_CLEARBREAKPOINTS, ClearBreakPointsB, *LocalizeUnrealEd("ClearKismetBreakpoints"));
	
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

	SetStatusText( *LocalizeUnrealEd("ObjectNone"), 0 );
}

WxKismetStatusBar::~WxKismetStatusBar()
{

}

void WxKismetStatusBar::SetMouseOverObject(USequenceObject* SeqObj)
{
	FString ObjInfo(*LocalizeUnrealEd("ObjectNone"));
	if(SeqObj)
	{
#if _DEBUG
		FString DebugObjectName = FString::Printf(TEXT("%s (%s)"), *SeqObj->ObjName, *SeqObj->GetFullName());
		ObjInfo = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Object_F"), *DebugObjectName) );
#else
		ObjInfo = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Object_F"), *SeqObj->ObjName) );
#endif
	}

	SetStatusText( *ObjInfo, 0 );
}

/*-----------------------------------------------------------------------------
	WxMBKismetNewObject.
-----------------------------------------------------------------------------*/

/**
 * Looks for a category in the op's name, returns TRUE if one is found.
 */
static UBOOL GetSequenceObjectCategory(USequenceObject *Op, FString &CategoryName, FString &OpName)
{
	OpName = Op->ObjName;
	if (Op->ObjCategory.Len() > 0)
	{
		CategoryName = Op->ObjCategory;
		return TRUE;
	}
	return FALSE;
}

WxMBKismetNewObject::WxMBKismetNewObject(WxKismet* SeqEditor)
{
	// create the top level menus
	ActionMenu = new wxMenu();
	Append( IDM_KISMET_NEW_ACTION, *LocalizeUnrealEd("NewAction"), ActionMenu );
	ConditionMenu = new wxMenu();
	VariableMenu = new wxMenu();
	EventMenu = new wxMenu();
	ExistingNamedVariablesInPMapMenu = new wxMenu();
	ExistingNamedVariablesInOtherLevelsMenu = new wxMenu();
	// set up map of sequence object classes to their respective menus
	TMap<UClass*,wxMenu*> MenuMap;
	MenuMap.Set(USequenceAction::StaticClass(),ActionMenu);
	MenuMap.Set(USequenceCondition::StaticClass(),ConditionMenu);
	MenuMap.Set(USequenceVariable::StaticClass(),VariableMenu);
	MenuMap.Set(USequenceEvent::StaticClass(),EventMenu);
	MenuMap.Set(USeqVar_Named::StaticClass(),ExistingNamedVariablesInPMapMenu);
	MenuMap.Set(USeqVar_Named::StaticClass(),ExistingNamedVariablesInOtherLevelsMenu);
	// list of categories for all the submenus
	TMap<FString,wxMenu*> ActionSubmenus;
	TMap<FString,wxMenu*> ConditionSubmenus;
	TMap<FString,wxMenu*> VariableSubmenus;
	TMap<FString,wxMenu*> EventSubmenus;
	TMap<FString,wxMenu*> ExistingNamedVariablesInOtherLevelsSubmenus;
	// map all the submenus to their parent menu
	TMap<wxMenu*,TMap<FString,wxMenu*>*> SubmenuMap;
	SubmenuMap.Set(ActionMenu,&ActionSubmenus);
	SubmenuMap.Set(ConditionMenu,&ConditionSubmenus);
	SubmenuMap.Set(VariableMenu,&VariableSubmenus);
	SubmenuMap.Set(EventMenu,&EventSubmenus);
	SubmenuMap.Set(ExistingNamedVariablesInOtherLevelsMenu,&ExistingNamedVariablesInOtherLevelsSubmenus);
	// iterate through all known sequence object classes
	INT CommentIndex = INDEX_NONE;
	INT WrappedCommentIndex = INDEX_NONE;
	FString CategoryName, OpName;

	for(INT i=0; i<SeqEditor->SeqObjClasses.Num(); i++)
	{
		UClass *SeqClass = SeqEditor->SeqObjClasses(i);
		if (SeqClass != NULL)
		{
			// special cases
			if( SeqClass == USeqAct_Interp::StaticClass() )
			{
				Append(IDM_NEW_SEQUENCE_OBJECT_START+i, *FString::Printf(TEXT("New %s"), *((USequenceObject*)SeqClass->GetDefaultObject())->ObjName), TEXT(""));
			}
			else if( SeqClass == USequenceFrame::StaticClass() )
			{
				CommentIndex = i;
			}
			else if( SeqClass == USequenceFrameWrapped::StaticClass() )
			{
				WrappedCommentIndex = i;
			}
			else
			{
				// grab the default object for reference
				USequenceObject* SeqObjDefault = (USequenceObject*)SeqClass->GetDefaultObject();
				// Skip item if it should be removed from this project
				if( SeqObjDefault->ObjRemoveInProject.ContainsItem( appGetGameName() ) )
				{
					continue;
				}				

				// first look up the menu type
				wxMenu* Menu = NULL;
				for (TMap<UClass*,wxMenu*>::TIterator It(MenuMap); It && Menu == NULL; ++It)
				{
					UClass *TestClass = It.Key();
					if (SeqClass->IsChildOf(TestClass))
					{
						Menu = It.Value();
					}
				}
				if (Menu != NULL)
				{
					// check for a category
					if (GetSequenceObjectCategory(SeqObjDefault,CategoryName,OpName))
					{
						// look up the submenu map
						TMap<FString,wxMenu*>* Submenus = SubmenuMap.FindRef(Menu);
						if (Submenus != NULL)
						{
							wxMenu* Submenu = Submenus->FindRef(CategoryName);
							if (Submenu == NULL)
							{
								// create a new submenu for the category
								Submenu = new wxMenu();
								// add it to the map
								Submenus->Set(*CategoryName,Submenu);
								// and add it to the parent menu
								Menu->Append(IDM_KISMET_NEW_ACTION_CATEGORY_START+(Submenus->Num()),*CategoryName, Submenu);
							}
							// add the object to the submenu
							Submenu->Append(IDM_NEW_SEQUENCE_OBJECT_START+i,*OpName,TEXT(""));
						}
					}
					else
					{
						// otherwise add it to the parent menu
						Menu->Append(IDM_NEW_SEQUENCE_OBJECT_START+i,*OpName,TEXT(""));
					}
				}
			}
		}
	}

	Append( IDM_KISMET_NEW_CONDITION, *LocalizeUnrealEd("NewCondition"), ConditionMenu );
	Append( IDM_KISMET_NEW_VARIABLE, *LocalizeUnrealEd("NewVariable"), VariableMenu );
	Append( IDM_KISMET_NEW_EVENT, *LocalizeUnrealEd("NewEvent"), EventMenu );


	// Build a list of all named objects from the PMap
	TMap<FString, TArray<USequenceVariable*>*> ClassTypeMap; // Type reference to the Sequence Variables
	TArray<USequenceVariable*>* References = NULL;

	// Add our two options to the variables context menu, One for named variables in the PMap, the other for all other levels
	VariableMenu->AppendSeparator();
	VariableMenu->Append( IDM_EXISTING_NAMED_VAR_START, *LocalizeUnrealEd("ExistingNamedVariableInPersistentLevel"), ExistingNamedVariablesInPMapMenu );
	VariableMenu->Append( IDM_EXISTING_NAMED_VAR_START+1, *LocalizeUnrealEd("ExistingNamedVariableInOtherLevels"), ExistingNamedVariablesInOtherLevelsMenu );
	
	INT NumAdded = 2; // Record of number of event ID's we have used so far

	// Clear the map which is used by the event handler to identify the selected object
	SeqEditor->NamedVariablesEventMap.Empty();


	// Look through the Persistence Level for all named variables
	for( INT LevelIt = 0; LevelIt < GWorld->Levels.Num(); LevelIt++ )
	{
		if( ( IDM_EXISTING_NAMED_VAR_START + NumAdded ) > IDM_EXISTING_NAMED_VAR_END )
		{
			break;
		}

		ULevel* CurrentLevel = GWorld->Levels(LevelIt);
		wxMenu* MenuToAddTo;

		ClassTypeMap.Empty(); // Clear our Map for use with new Level

		// If our level is not the PLevel, then we add a new submenu to the context to list the named variables from this menu
		if( CurrentLevel != GWorld->PersistentLevel )
		{
			wxMenu *NewSubMenu = new wxMenu();
			ExistingNamedVariablesInOtherLevelsSubmenus.Set( *CurrentLevel->GetOutermost()->GetName(), NewSubMenu );
			ExistingNamedVariablesInOtherLevelsMenu->Append( IDM_EXISTING_NAMED_VAR_START + NumAdded++, *CurrentLevel->GetOutermost()->GetName(), NewSubMenu );

			MenuToAddTo = NewSubMenu;
		}
		else
		{
			MenuToAddTo = ExistingNamedVariablesInPMapMenu;
		}

		// Look through all the sequences in the CurrentLevel for variables that are named
		for( INT i = 0; i < CurrentLevel->GameSequences.Num(); i++ )
		{
			USequence* seq = CurrentLevel->GameSequences(i);
			if( seq )
			{
				for( INT j = 0; j < seq->SequenceObjects.Num(); j++ )
				{
					USequenceVariable* SeqVar = Cast<USequenceVariable>( seq->SequenceObjects(j) );
					if( SeqVar && SeqVar->VarName != NAME_None )
					{
						// If we find a named variable, check if a container for the class (objName) type already exists
						TArray<USequenceVariable*>* Ref = ClassTypeMap.FindRef( SeqVar->ObjName );
						if( Ref == NULL )
						{
							// If not create a new array to hold these objects and add it to the ClassTypeMap
							References = new TArray<USequenceVariable*>;
							References->AddItem( SeqVar );
							ClassTypeMap.Set( SeqVar->ObjName, References );
						}
						else
						{
							// If the container exists, simply add our next variable to this
							Ref->AddItem( SeqVar );
						}
					}
				}
			}
		}

		// Clear the Kismet Windows, Map of Named Variables to their Event ID for use in the event handler
		for( TMap<FString, TArray<USequenceVariable*>*>::TIterator It(ClassTypeMap); It; ++It )
		{
			// Add all named variables to the right context menu, sorted by type
			TArray<USequenceVariable*>& Arr = *It.Value();
			for( INT Idx = 0; Idx < Arr.Num(); Idx++ )
			{
				if( ( IDM_EXISTING_NAMED_VAR_START + NumAdded ) <= IDM_EXISTING_NAMED_VAR_END )
				{
					MenuToAddTo->Append( IDM_EXISTING_NAMED_VAR_START + NumAdded, *Arr(Idx)->VarName.ToString(), *Arr(Idx)->VarName.ToString() );
					SeqEditor->NamedVariablesEventMap.Set( IDM_EXISTING_NAMED_VAR_START + NumAdded, Arr(Idx) );
					NumAdded++;
				}
			}
			// Add a separator to the context for usability
			MenuToAddTo->AppendSeparator();
		}

	}

	if(CommentIndex != INDEX_NONE)
	{
		AppendSeparator();
		Append( IDM_NEW_SEQUENCE_OBJECT_START+CommentIndex, *LocalizeUnrealEd("NewComment"), TEXT("") );
		if(WrappedCommentIndex != INDEX_NONE)
		{
			Append( IDM_NEW_SEQUENCE_OBJECT_START+WrappedCommentIndex, *LocalizeUnrealEd("NewWrappedComment"), TEXT("") );
		}
	}

	// Update SeqEditor->NewEventClasses and SeqEditor->NewObjActors.
	SeqEditor->BuildSelectedActorLists();

	// Add 'New Event Using' item.
	if(SeqEditor->NewEventClasses.Num() > 0)
	{
		// Don't add items, unless all actors belong to the sequence's level.
		ULevel* SequenceLevel		= SeqEditor->Sequence->GetLevel();
		UBOOL bCrossLevelReferences	= FALSE;
		for( INT ActorIndex = 0 ; ActorIndex < SeqEditor->NewObjActors.Num() ; ++ActorIndex )
		{
			AActor* Actor			= SeqEditor->NewObjActors(ActorIndex);
			ULevel* ActorLevel		= Actor->GetLevel();
			if ( ActorLevel != SequenceLevel )
			{
				bCrossLevelReferences = TRUE;
				break;
			}
		}
		if ( !bCrossLevelReferences )
		{
			AppendSeparator();
			ContextEventMenu = new wxMenu();

			FString NewEventString;
			FString NewObjVarString;

			FString FirstObjName = *SeqEditor->NewObjActors(0)->GetName();
			if( SeqEditor->NewObjActors.Num() > 1 )
			{
				NewEventString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("NewEventsUsing_F"), *FirstObjName) );
				NewObjVarString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("NewObjectVarsUsing_F"), *FirstObjName) );
			}
			else
			{
				NewEventString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("NewEventUsing_F"), *FirstObjName) );
				NewObjVarString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("NewObjectVarUsing_F"), *FirstObjName) );
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
	}

	AppendSeparator();

	// Append the option to make a new sequence
	FString NewSeqString = FString::Printf( TEXT("%s: %d Objs"), *LocalizeUnrealEd("CreateNewSequence"), SeqEditor->SelectedSeqObjs.Num() );
	Append(IDM_KISMET_CREATE_SEQUENCE,*NewSeqString,TEXT(""));

	// Append option for pasting clipboard at current mouse location
	Append(IDM_KISMET_PASTE_HERE, *LocalizeUnrealEd("PasteHere"), TEXT(""));

	// append option to import a new sequence
	USequence *SelectedSeq = Cast<USequence>(GEditor->GetSelectedObjects()->GetTop(USequence::StaticClass()));
	if (SelectedSeq != NULL)
	{
		// Disallow importing into cooked level packages.
		const UPackage* SeqPackage = SelectedSeq->GetOutermost();
		if ( !(SeqPackage->PackageFlags & PKG_Cooked) )
		{
			Append(IDM_KISMET_IMPORT_SEQUENCE,*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ImportSequence_F"),*SelectedSeq->GetName())),TEXT(""));
		}
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
	// look for actions that need to be updated
	USequenceOp *UpdateOp = NULL;
	for (INT idx = 0; idx < SeqEditor->SelectedSeqObjs.Num() && UpdateOp == NULL; idx++)
	{
		USequenceOp *Op = Cast<USequenceOp>(SeqEditor->SelectedSeqObjs(idx));
		if (Op != NULL && !Op->IsA(USequence::StaticClass()) && Op->eventGetObjClassVersion() != Op->ObjInstanceVersion)
		{
			UpdateOp = Op;
		}
	}
	if (UpdateOp != NULL)
	{
		Append(IDM_KISMET_UPDATE_ACTION, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("UpdateLatest_F"),*UpdateOp->ObjName)), TEXT(""));
		AppendSeparator();
	}

	NewVariableMenu = NULL;

	// If one Op is selected, and there are classes of optional variable connectors defined, create menu for them.
	if(SeqEditor->SelectedSeqObjs.Num() == 1)
	{
		UBOOL bSeparate = FALSE;
		USequenceOp* SeqOp = Cast<USequenceOp>( SeqEditor->SelectedSeqObjs(0) );
	
		USeqAct_Interp* Interp = Cast<USeqAct_Interp>( SeqOp );
		if(Interp)
		{
			Append( IDM_KISMET_OPEN_INTERPEDIT, *LocalizeUnrealEd("OpenMatinee"), TEXT("") );
			bSeparate = TRUE;
		}

		// If the operation is a switch class, give options for adding a switch node
		USeqCond_SwitchBase* Switch = Cast<USeqCond_SwitchBase>(SeqOp);
		if( Switch )
		{
			Append( IDM_KISMET_SWITCH_ADD, *LocalizeUnrealEd("AddSwitchConnector"), TEXT("") );
			bSeparate = TRUE;
		}

		USeqAct_ActivateRemoteEvent *Act = Cast<USeqAct_ActivateRemoteEvent>(SeqOp);
		if (Act != NULL)
		{
			Append(IDM_KISMET_SEARCH_REMOTEEVENTS, *LocalizeUnrealEd("SearchRemoteEvents"), TEXT(""));
			bSeparate = TRUE;
		}

		if( bSeparate )
		{
			AppendSeparator();
		}
	}

	UBOOL bNeedsSeparator = FALSE;

	// search for the first selected event
	USequenceEvent *selectedEvt = NULL;
	USeqVar_Object *selectedObjVar = NULL;
	SeqEditor->SelectedSeqObjs.FindItemByClass<USequenceEvent>(&selectedEvt);
	SeqEditor->SelectedSeqObjs.FindItemByClass<USeqVar_Object>(&selectedObjVar);
	AActor *selectedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
	if(selectedEvt != NULL)
	{
		// add option to link selected actor to all selected events
		if (selectedActor != NULL)
		{
			// make sure the selected actor supports the event selected
			UBOOL bFoundMatch = 0;
			for (INT idx = 0; idx < selectedActor->SupportedEvents.Num() && !bFoundMatch; idx++)
			{
				if (selectedActor->SupportedEvents(idx)->IsChildOf(selectedEvt->GetClass()) ||
					selectedEvt->GetClass()->IsChildOf(selectedActor->SupportedEvents(idx)))
				{
					bFoundMatch = 1;
				}
			}
			if (bFoundMatch)
			{
				Append(IDM_KISMET_LINK_ACTOR_TO_EVT, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("AssignToEvents_F"),*selectedActor->GetName())), TEXT(""));
				bNeedsSeparator = TRUE;
			}
		}
	}
	if (selectedObjVar != NULL &&
		selectedActor != NULL)
	{
		bNeedsSeparator = TRUE;
		Append(IDM_KISMET_LINK_ACTOR_TO_OBJ, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("AssignToObjectVars_F"),*selectedActor->GetName())), TEXT(""));

		// If working with an object list
		USeqVar_ObjectList* ObjList = Cast<USeqVar_ObjectList>(selectedObjVar);
		if( ObjList )
		{
			TArray<AActor*> SelectedActors;
			GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
			
			// Give option to insert selected actor into the list
			Append( IDM_KISMET_INSERT_ACTOR_TO_OBJLIST, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("InsertToObjectListVars_F"), SelectedActors.Num() > 1 ? TEXT("Selected Actors") : *selectedActor->GetName())), TEXT(""));
			Append( IDM_KISMET_REMOVE_ACTOR_FROM_OBJLIST, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("RemoveFromObjectListVars_F"), SelectedActors.Num() > 1 ? TEXT("Selected Actors") : *selectedActor->GetName())), TEXT(""));
		}
	}
	
	// if we have an object var selected,
	if (selectedObjVar != NULL)
	{
		// add an option to clear it's contents
		Append(IDM_KISMET_CLEAR_VARIABLE, *LocalizeUnrealEd("ClearObjectVars"), TEXT(""));
		bNeedsSeparator = TRUE;
	}

	// Add options for finding definitions/uses of named variables.
	if( SeqEditor->SelectedSeqObjs.Num() == 1 )
	{
		USequenceVariable* SeqVar = Cast<USequenceVariable>( SeqEditor->SelectedSeqObjs(0) );
		if(SeqVar && SeqVar->VarName != NAME_None)
		{
			FString FindUsesString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("FindUses_F"), *(SeqVar->VarName.ToString())) );
			Append( IDM_KISMET_FIND_NAMEDVAR_USES, *FindUsesString, TEXT("") );
			bNeedsSeparator = TRUE;
		}

		USeqVar_Named* NamedVar = Cast<USeqVar_Named>( SeqEditor->SelectedSeqObjs(0) );
		if(NamedVar && NamedVar->FindVarName != NAME_None)
		{
			FString FindDefsString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("FindDefinition_F"), *(NamedVar->FindVarName.ToString())) );
			Append( IDM_KISMET_FIND_NAMEDVAR, *FindDefsString, TEXT("") );
			bNeedsSeparator = TRUE;
		}

		USequenceOp *Op = Cast<USequenceOp>(SeqEditor->SelectedSeqObjs(0));
		if (Op != NULL)
		{
			// build the list of variable classes
			//@fixme - this is retarded!!!!
			TArray<UClass*> VariableClasses;
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->IsChildOf(USequenceVariable::StaticClass()))
                {
					VariableClasses.AddItem(*It);
				}
			}
			// get a list of all properties for this class
			TArray<UProperty*> OpProperties;
			SeqEditor->OpExposeVarLinkMap.Empty();
			UClass *SearchClass = Op->GetClass();
			while (SearchClass != NULL &&
				   SearchClass->IsChildOf(USequenceOp::StaticClass()))
			{
				for (TFieldIterator<UProperty> It(SearchClass,FALSE); It; ++It)
				{
					if (It->PropertyFlags & CPF_Edit)
					{
						OpProperties.AddItem(*It);
					}
				}
				SearchClass = SearchClass->GetSuperClass();
			}
			// check each property to see if there is a variable link for it
			for (INT Idx = 0; Idx < OpProperties.Num(); Idx++)
			{
				for (INT VarIdx = 0; VarIdx < Op->VariableLinks.Num(); VarIdx++)
				{
					if (Op->VariableLinks(VarIdx).PropertyName == OpProperties(Idx)->GetFName())
					{
						// already has a link exposed, remove from the list
						OpProperties.Remove(Idx--,1);
						break;
					}
				}
			}
			// look for a list of variable links that are just hidden
			TArray<INT> HiddenVariableLinks;
			for (INT Idx = 0; Idx < Op->VariableLinks.Num(); Idx++)
			{
				if (Op->VariableLinks(Idx).bHidden)
				{
					HiddenVariableLinks.AddItem(Idx);
				}
			}
			UBOOL bAppendedMenu = FALSE;
			// if we still have properties left to expose
			if (OpProperties.Num() > 0 || HiddenVariableLinks.Num() > 0)
			{
				bAppendedMenu = TRUE;
				if ( bNeedsSeparator )
				{
					bNeedsSeparator = FALSE;
					AppendSeparator();
				}
				// create the submenu
				wxMenu *ExposeMenu = new wxMenu();
				INT MapId = 0;
				for (INT Idx = 0; Idx < OpProperties.Num(); Idx++)
				{
					for (INT VarIdx = 0; VarIdx < VariableClasses.Num(); VarIdx++)
					{
						USequenceVariable *DefaultVar = (USequenceVariable*)(VariableClasses(VarIdx)->GetDefaultObject());
						if (DefaultVar != NULL &&
							DefaultVar->SupportsProperty(OpProperties(Idx)))
						{
							// if dealing w/ object property and object variable, check validity against the supported classes array
							UObjectProperty *ObjProp = OpProperties(Idx)->IsA(UArrayProperty::StaticClass()) ? Cast<UObjectProperty>(((UArrayProperty*)OpProperties(Idx))->Inner) : Cast<UObjectProperty>(OpProperties(Idx));
							USeqVar_Object *ObjVar = Cast<USeqVar_Object>(DefaultVar);
							if (ObjProp != NULL && ObjVar != NULL)
							{
								UBOOL bIsValidChildClass = FALSE;
								UClass *PropClass = ObjProp->PropertyClass;
								for (INT ClassIdx = 0; ClassIdx < ObjVar->SupportedClasses.Num(); ClassIdx++)
								{
									if (PropClass->IsChildOf(ObjVar->SupportedClasses(ClassIdx)))
									{
										bIsValidChildClass = TRUE;
										break;
									}
								}
								if (!bIsValidChildClass)
								{
									// not a valid child so skip adding an entry to expose
									continue;
								}

							}
							// add a property expose entry
							SeqEditor->OpExposeVarLinkMap.Set(MapId,FExposeVarLinkInfo(OpProperties(Idx),VariableClasses(VarIdx)));
							// and add it to the menu
							ExposeMenu->Append(IDM_KISMET_EXPOSE_VARIABLE_START+MapId,*FString::Printf(TEXT("%s - %s"),*OpProperties(Idx)->GetName(),*DefaultVar->ObjName),TEXT(""));
							// increment the id
							MapId++;
						}
					}
				}
				// add any hidden variable links
				for (INT Idx = 0; Idx < HiddenVariableLinks.Num(); Idx++)
				{
					INT VarLinkIdx = HiddenVariableLinks(Idx);
					USequenceVariable *DefaultVar = (USequenceVariable*)(Op->VariableLinks(VarLinkIdx).ExpectedType->GetDefaultObject());
					SeqEditor->OpExposeVarLinkMap.Set(MapId,FExposeVarLinkInfo(VarLinkIdx));
					// and add it to the menu
					ExposeMenu->Append(IDM_KISMET_EXPOSE_VARIABLE_START+MapId,*FString::Printf(TEXT("%s - %s *"),*Op->VariableLinks(VarLinkIdx).LinkDesc,*DefaultVar->ObjName),TEXT(""));
					// increment the id
					MapId++;
				}
#if WITH_GFx
                if (Cast<UGFxAction_Invoke>(Op))
                {
                    FExposeVarLinkInfo VarLink;
                    VarLink.Type = FExposeVarLinkInfo::TYPE_GFxArrayElement;
                    VarLink.VariableClass = USequenceVariable::StaticClass();
                    SeqEditor->OpExposeVarLinkMap.Set(MapId,VarLink);
                    ExposeMenu->Append(IDM_KISMET_EXPOSE_VARIABLE_START+MapId,*LocalizeUnrealEd("Argument"));
                    MapId++;
                }
#endif

				Append(-1,*LocalizeUnrealEd("ExposeVariable"),ExposeMenu);
			}

			// check for output links to expose
			TArray<INT> OutputIndices;
			for (INT Idx = 0; Idx < Op->OutputLinks.Num(); Idx++)
			{
				if (Op->OutputLinks(Idx).bHidden)
				{
					OutputIndices.AddItem(Idx);
				}
			}
			if (OutputIndices.Num() > 0)
			{
				bAppendedMenu = TRUE;
				if ( bNeedsSeparator )
				{
					bNeedsSeparator = FALSE;
					AppendSeparator();
				}
				wxMenu *ExposeMenu = new wxMenu();
				for (INT Idx = 0; Idx < OutputIndices.Num(); Idx++)
				{
					ExposeMenu->Append(IDM_KISMET_EXPOSE_OUTPUT_START+OutputIndices(Idx),*(Op->OutputLinks(OutputIndices(Idx)).LinkDesc));
				}
				Append(-1,*LocalizeUnrealEd("ExposeOutput"),ExposeMenu);
			}
			if (bAppendedMenu)
			{
				AppendSeparator();
			}
		}

	}

	Append(IDM_KISMET_BREAK_ALL_OP_LINKS,*LocalizeUnrealEd("BreakAllOpLinks"),TEXT(""));

	AppendSeparator();

	Append(IDM_KISMET_SELECT_DOWNSTREAM_NODES,*LocalizeUnrealEd("SelectDownstream"),TEXT(""));
	Append(IDM_KISMET_SELECT_UPSTREAM_NODES,*LocalizeUnrealEd("SelectUpstream"),TEXT(""));

	AppendSeparator();

	// Add option to select all actors referenced by the selected sequence objects if any of the sequence objects
	// contain actor references
	TArray<AActor*> ActorReferences;
	WxKismet::GetReferencedActors( ActorReferences, SeqEditor->SelectedSeqObjs, TRUE );
	if ( ActorReferences.Num() > 0 )
	{
		FString NameString = ( ActorReferences.Num() == 1 ) ? ActorReferences( 0 )->GetName() : LocalizeUnrealEd("Actors");
		Append( IDM_KISMET_SELECT_REFERENCED_ACTORS, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "SelectAssigned_F" ), *NameString  ), TEXT("") ) );

		AppendSeparator();
	}

	// add options to hide unused connectors or show all connectors
	Append(IDM_KISMET_HIDE_CONNECTORS,*LocalizeUnrealEd("HideUnusedConnectors"),TEXT(""));
	Append(IDM_KISMET_SHOW_CONNECTORS,*LocalizeUnrealEd("ShowAllConnectors"),TEXT(""));

	AppendSeparator();

	const UBOOL bIsSeqPlaySound = ContainsObjectOfClass( SeqEditor->SelectedSeqObjs, USeqAct_PlaySound::StaticClass() );
	const UBOOL bIsSeqPlayMusic = ContainsObjectOfClass( SeqEditor->SelectedSeqObjs, USeqAct_PlayMusicTrack::StaticClass() );

	// add options to play sounds/music track or to stop currently playing music track
	if( bIsSeqPlaySound || bIsSeqPlayMusic )
	{
		Append( IDM_KISMET_PLAYSOUNDSORMUSICTRACK, *LocalizeUnrealEd("PlaySoundsOrMusicTrack"), TEXT("") );

		AWorldInfo *WorldInfo = GWorld->GetWorldInfo();
		if( bIsSeqPlayMusic && WorldInfo && WorldInfo->MusicComp && WorldInfo->CurrentMusicTrack.TheSoundCue )
		{
			for( INT SeqIndex = 0; SeqIndex < SeqEditor->SelectedSeqObjs.Num(); SeqIndex++ )
			{
				USequenceObject* SeqObject = SeqEditor->SelectedSeqObjs(SeqIndex);
				USeqAct_PlayMusicTrack* SeqPlayMusic = Cast<USeqAct_PlayMusicTrack>( SeqObject );

				if( SeqPlayMusic && SeqPlayMusic->MusicTrack.TheSoundCue && SeqPlayMusic->MusicTrack.TheSoundCue == WorldInfo->CurrentMusicTrack.TheSoundCue )
				{
					Append( IDM_KISMET_STOPMUSICTRACK, *LocalizeUnrealEd("StopMusicTrack"), TEXT("") );
					break;
				}
			}
		}

		AppendSeparator();
	}

    // add option to create a new sequence
	Append(IDM_KISMET_CREATE_SEQUENCE,*LocalizeUnrealEd("CreateNewSequence"),TEXT(""));
	if ( ContainsObjectOfClass(SeqEditor->SelectedSeqObjs, USequence::StaticClass()) )
	{
		// and the option to rename
		Append(IDM_KISMET_RENAMESEQ,*LocalizeUnrealEd("RenameSelectedSequence"),TEXT(""));
		// if we have a sequence selected, add the option to export
		Append(IDM_KISMET_EXPORT_SEQUENCE,*LocalizeUnrealEd("ExportSequenceToPackage"),TEXT(""));
	}

	// Add 'apply style' menu
	USequenceFrame *SelectedComment = NULL;
	SeqEditor->SelectedSeqObjs.FindItemByClass<USequenceFrame>(&SelectedComment);
	if(SelectedComment)
	{
		// Make menu for all styles
		wxMenu* StyleMenu = new wxMenu();
	
		// Make a menu entry for each comment preset.
		UKismetBindings* Bindings = (UKismetBindings*)(UKismetBindings::StaticClass()->GetDefaultObject());
		for(INT i=0; i<Bindings->CommentPresets.Num(); i++)
		{
			FKismetCommentPreset& Preset = Bindings->CommentPresets(i);
			StyleMenu->Append(IDM_KISMET_APPLY_COMMENT_STYLE_START+i, *Preset.PresetName.ToString(), TEXT(""));
		}

		// Add style menu to context menu
		AppendSeparator();
		Append(IDM_KISMET_APPLY_COMMENT_STYLE_MENU, *LocalizeUnrealEd("ApplyCommentStyle"), StyleMenu);
	
		// Add menu options for moving a comment forwards/backwards in the stack.
		// Only really well defined if one object is selected.
		if(SeqEditor->SelectedSeqObjs.Num() == 1)
		{
			Append(IDM_KISMET_COMMENT_TO_FRONT, *LocalizeUnrealEd("CommentToFront"), TEXT(""));
			Append(IDM_KISMET_COMMENT_TO_BACK, *LocalizeUnrealEd("CommentToBack"), TEXT(""));
		}
	}

	UBOOL bAllSelectedObjsSameClass = TRUE;

	// See if all of the selected objects are of the same class...if so, present the option
	// to search for all instances of that class type
	if ( SeqEditor->SelectedSeqObjs.Num() > 0 )
	{
		USequenceObject* FirstSelectedObj = SeqEditor->SelectedSeqObjs(0);
		UClass* FirstSelectedClass = FirstSelectedObj ? FirstSelectedObj->GetClass() : NULL;

		// If the first object/class is NULL, not all objects are going to have the same class
		if ( !FirstSelectedClass )
		{
			bAllSelectedObjsSameClass = FALSE;
		}
		for ( TArray<USequenceObject*>::TConstIterator SelectedIter( SeqEditor->SelectedSeqObjs ); SelectedIter && bAllSelectedObjsSameClass; ++SelectedIter )
		{
			USequenceObject* CurSelectedObj = *SelectedIter;
			// Check if the current object's class is the same as the first selected object's class
			if ( CurSelectedObj && CurSelectedObj->GetClass() != FirstSelectedClass )
			{
				bAllSelectedObjsSameClass = FALSE;
			}
			// If the object is NULL, not all objects have the same class
			else if ( !CurSelectedObj )
			{
				bAllSelectedObjsSameClass = FALSE;
			}
		}

		// Append the search by object class option if all of the selected objects share the same class type
		if ( bAllSelectedObjsSameClass )
		{
			AppendSeparator();
			// Use the default object of the class instead of the ObjName so the string can specify a "friendly" class name. The actual
			// sequence objects themselves might have been renamed, so they can't be used for the ObjName string.
			USequenceObject* DefaultClassObj = FirstSelectedClass->GetDefaultObject<USequenceObject>();
			if ( DefaultClassObj )
			{
				const FString& SearchOptionString = FString::Printf( LocalizeSecure( LocalizeUnrealEd("SearchForObjectType"), *DefaultClassObj->ObjName ) );
				Append( IDM_KISMET_SEARCH_BYCLASS, *SearchOptionString, TEXT("") );
				
			}
		}
		Append(IDM_KISMET_SELECT_ALL_MATCHING, *LocalizeUnrealEd("SelectAllMatching"), TEXT(""));
	}
	// Iterate through selected sequence objects to see if any breakpoints are set 
	if( SeqEditor->SelectedSeqObjs.Num() > 0 )
	{
		UBOOL BreakpointNotSet = FALSE;
		UBOOL BreakpointIsSet = FALSE;
		// If any breakpoints are set or not set, we want to add menu options
		for ( INT ObjIdx = 0; ObjIdx < SeqEditor->SelectedSeqObjs.Num(); ObjIdx++)
		{
			USequenceObject* Object = SeqEditor->SelectedSeqObjs(ObjIdx);
			USequenceOp* Op = Cast<USequenceOp>(Object);
			if(Op)
			{
				if( Op->bIsBreakpointSet )
				{
					BreakpointIsSet = TRUE;
				}
				else
				{
					BreakpointNotSet = TRUE;
				}
			}
		}
		// if either is true, append a separator
		if( BreakpointIsSet || BreakpointNotSet )
		{
			AppendSeparator();
			// And then append the menu options that are needed
			if( BreakpointNotSet )
			{
				Append( IDM_KISMET_SET_BREAKPOINT, *LocalizeUnrealEd("KismetSetBreakpoint"), TEXT(""));
			}
			if( BreakpointIsSet )
			{
				Append( IDM_KISMET_CLEAR_BREAKPOINT, *LocalizeUnrealEd("KismetClearBreakpoint"), TEXT(""));
			}
		}

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
	INT ConnCount = 0;

	// Add menu option for breaking links. Only input and variable links actually store pointers.
	if( ConnType == LOC_OUTPUT )
	{
		Append( IDM_KISMET_BREAK_LINK_ALL, *LocalizeUnrealEd("BreakAllLinks"), TEXT("") );
		BreakLinkMenu = new wxMenu();
		Append(IDM_KISMET_BREAK_LINK_MENU,*LocalizeUnrealEd("BreakLinkTo"),BreakLinkMenu);
		// add individual links
		for (INT idx = 0; idx < ConnSeqOp->OutputLinks(ConnIndex).Links.Num(); idx++)
		{
			FSeqOpOutputInputLink &link = ConnSeqOp->OutputLinks(ConnIndex).Links(idx);
			if (link.LinkedOp != NULL)
			{
				// add a submenu for this link
				FString ObjName = link.LinkedOp->ObjName;
				FString OpName  =  link.LinkedOp->GetName();
				// If object has a comment use that instead
				if( link.LinkedOp->ObjComment.Len() )
				{
					ObjName = *link.LinkedOp->ObjComment;
				}
				// If input link has a description, use it
				if( link.LinkedOp->InputLinks(link.InputLinkIdx).LinkDesc.Len() )
				{
					OpName = *link.LinkedOp->InputLinks(link.InputLinkIdx).LinkDesc;
				}

				BreakLinkMenu->Append(IDM_KISMET_BREAK_LINK_START+idx,*FString::Printf(TEXT("%s (%s)"), *ObjName, *OpName),TEXT(""));

				ConnCount++;
			}
		}

		// If the operation is a switch class, give options for removing a switch node
		USeqCond_SwitchBase* Switch = Cast<USeqCond_SwitchBase>(ConnSeqOp);
		if( Switch )
		{
			Append( IDM_KISMET_SWITCH_REMOVE, *LocalizeUnrealEd("RemoveSwitchConnector"), TEXT("") );
		}

		// Connection copy/paste
		AppendSeparator();
		Append( IDM_KISMET_COPY_CONNECTIONS, *LocalizeUnrealEd("CopyConnections"), TEXT("") );
		if( ConnCount )
		{
			Append( IDM_KISMET_CUT_CONNECTIONS,  *LocalizeUnrealEd("CutConnections"),  TEXT("") );
		}
		if( SeqEditor->CopyConnInfo.Num() )
		{
			Append( IDM_KISMET_PASTE_CONNECTIONS, *LocalizeUnrealEd("PasteConnections"), TEXT("") );
		}
		AppendSeparator();

		FString Toggle = *LocalizeUnrealEd("ToggleLink");
		if( ConnSeqOp->OutputLinks(ConnIndex).bDisabled )
		{
			Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Enable") );
		}
		else
		{
			Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Disable") );
		}
		Append( IDM_KISMET_TOGGLE_DISABLE_LINK, *Toggle, TEXT("") );

		Toggle = *LocalizeUnrealEd("ToggleLinkPIE");
		if( ConnSeqOp->OutputLinks(ConnIndex).bDisabledPIE )
		{
			Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Enable") );
		}
		else
		{
			Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Disable") );
		}
		Append( IDM_KISMET_TOGGLE_DISABLE_LINK_PIE, *Toggle, TEXT("") );

		AppendSeparator();
		Append( IDM_KISMET_REMOVE_OUTPUT, *LocalizeUnrealEd("HideOutput"), TEXT("") );
		Append( IDM_KISMET_SET_OUTPUT_DELAY, *LocalizeUnrealEd("SetActivateDelay"), TEXT("") );
	}
	else if (ConnType == LOC_INPUT)
	{
		// figure out if there is a link to this connection
		TArray<USequenceOp*> outputOps;
		TArray<INT> outputIndices;
		UBOOL bAddedMenu = FALSE;
		if (WxKismet::FindOutputLinkTo(SeqEditor->Sequence,ConnSeqOp,ConnIndex,outputOps,outputIndices))
		{
			// add the break all option if there are multiple links
			Append(IDM_KISMET_BREAK_LINK_ALL,*LocalizeUnrealEd("BreakAllLinks"),TEXT(""));
			// add the submenu for breaking individual links
			BreakLinkMenu = new wxMenu();
			Append(IDM_KISMET_BREAK_LINK_MENU,*LocalizeUnrealEd("BreakLinkTo"),BreakLinkMenu);
			// and add the individual link options
			for (INT idx = 0; idx < outputOps.Num(); idx++)
			{
				// add a submenu for this link
				FString ObjName = *outputOps(idx)->ObjName;
				FString OpName  =  outputOps(idx)->GetName();
				// If object has a comment use that instead
				if( outputOps(idx)->ObjComment.Len() )
				{
					ObjName = *outputOps(idx)->ObjComment;
				}
				// If object link has a description, use it
				if( outputOps(idx)->OutputLinks(outputIndices(idx)).LinkDesc.Len() )
				{
					OpName = *outputOps(idx)->OutputLinks(outputIndices(idx)).LinkDesc;
				}

				BreakLinkMenu->Append(IDM_KISMET_BREAK_LINK_START+idx,*FString::Printf(TEXT("%s (%s)"), *ObjName, *OpName),TEXT(""));

				ConnCount++;
			}

			AppendSeparator();
			FString Toggle = *LocalizeUnrealEd("ToggleLink");
			if( ConnSeqOp->InputLinks(ConnIndex).bDisabled )
			{
				Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Enable") );
			}
			else
			{
				Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Disable") );
			}
			// add the toggle disable flag option
			Append( IDM_KISMET_TOGGLE_DISABLE_LINK, *Toggle, TEXT("") );

			Toggle = *LocalizeUnrealEd("ToggleLinkPIE");
			if( ConnSeqOp->InputLinks(ConnIndex).bDisabledPIE )
			{
				Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Enable") );
			}
			else
			{
				Toggle += *FString::Printf( TEXT("  (%s)"), *LocalizeUnrealEd("Disable") );
			}
			// add the toggle disable flag option
			Append( IDM_KISMET_TOGGLE_DISABLE_LINK_PIE, *Toggle, TEXT("") );

			Append( IDM_KISMET_SET_INPUT_DELAY, *LocalizeUnrealEd("SetActivateDelay"), TEXT("") );

			bAddedMenu = TRUE;
		}
		
		// Connection copy/paste
		if( bAddedMenu )
		{
			AppendSeparator();
		}
		Append( IDM_KISMET_COPY_CONNECTIONS, *LocalizeUnrealEd("CopyConnections"), TEXT("") );
		if( ConnCount )
		{
			Append( IDM_KISMET_CUT_CONNECTIONS,  *LocalizeUnrealEd("CutConnections"),  TEXT("") );
		}
		if( SeqEditor->CopyConnInfo.Num() )
		{
			Append( IDM_KISMET_PASTE_CONNECTIONS, *LocalizeUnrealEd("PasteConnections"), TEXT("") );
		}
	}
	else if( ConnType == LOC_VARIABLE )
	{
		if( ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables.Num() > 0 )
		{
			Append( IDM_KISMET_BREAK_LINK_ALL, *LocalizeUnrealEd("BreakAllLinks"), TEXT("") );
			BreakLinkMenu = new wxMenu();
			Append(IDM_KISMET_BREAK_LINK_MENU,*LocalizeUnrealEd("BreakLinkTo"),BreakLinkMenu);
			// add the individual variable links
			for (INT idx = 0; idx < ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables.Num(); idx++)
			{
				USequenceVariable *var = ConnSeqOp->VariableLinks(ConnIndex).LinkedVariables(idx);

				// add a submenu for this link
				FString ObjName =  var->GetName();
				FString OpName  = var->GetValueStr();
				// If variable has a name, use it instead of string value
				if( var->VarName != NAME_None )
				{
					ObjName = var->VarName.ToString();
				}
				// If object has a comment use that instead
				else if( var->ObjComment.Len() )
				{
					ObjName = *var->ObjComment;
				}
				BreakLinkMenu->Append(IDM_KISMET_BREAK_LINK_START+idx,*FString::Printf(TEXT("%s (%s)"), *ObjName, *OpName),TEXT(""));

				ConnCount++;
			}
		}
		FSeqVarLink &varLink = ConnSeqOp->VariableLinks(ConnIndex);
		// Do nothing if ExpectedType is NULL!
		if(varLink.ExpectedType != NULL)
		{
			Append(IDM_KISMET_CREATE_LINKED_VARIABLE, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("CreateNewVar_F"),*((USequenceVariable*)varLink.ExpectedType->GetDefaultObject())->ObjName)), TEXT(""));

			// If this expects an object variable - offer option of creating a SeqVar_Object for each selected Actor and connecting it.
			if( varLink.SupportsVariableType(USeqVar_Object::StaticClass()) )
			{
				SeqEditor->BuildSelectedActorLists();

				if(SeqEditor->NewObjActors.Num() > 0)
				{
					FString NewObjVarString;
					FString FirstObjName = SeqEditor->NewObjActors(0)->GetName();
					if( SeqEditor->NewObjActors.Num() > 1 )
					{
						NewObjVarString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("NewObjectVarsUsing_F"), *FirstObjName) );
					}
					else
					{
						NewObjVarString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("NewObjectVarUsing_F"), *FirstObjName) );
					}

					Append( IDM_KISMET_NEW_VARIABLE_OBJ_CONTEXT, *NewObjVarString, TEXT("") );
					SeqEditor->bAttachVarsToConnector = TRUE;
				}
			}

			Append(IDM_KISMET_REMOVE_VARIABLE,TEXT("Remove variable connector"),TEXT(""));
		}

		AppendSeparator();
		Append( IDM_KISMET_COPY_CONNECTIONS, *LocalizeUnrealEd("CopyConnections"), TEXT("") );
		if( ConnCount )
		{
			Append( IDM_KISMET_CUT_CONNECTIONS,  *LocalizeUnrealEd("CutConnections"),  TEXT("") );
		}
		if( SeqEditor->CopyConnInfo.Num() &&
			SeqEditor->CopyConnType == LOC_VARIABLE )
		{
			Append( IDM_KISMET_PASTE_CONNECTIONS, *LocalizeUnrealEd("PasteConnections"), TEXT("") );
		}
	}
	else if (ConnType == LOC_EVENT)
	{
		if( ConnSeqOp->EventLinks(ConnIndex).LinkedEvents.Num() > 0 )
		{
			Append(IDM_KISMET_BREAK_LINK_ALL, *LocalizeUnrealEd("BreakEventLinks"),TEXT(""));
			ConnCount = ConnSeqOp->EventLinks(ConnIndex).LinkedEvents.Num();
		}
		
		FSeqEventLink &eventLink = ConnSeqOp->EventLinks(ConnIndex);
		// Do nothing if ExpectedType is NULL!
		if(eventLink.ExpectedType != NULL)
		{
			Append(IDM_KISMET_CREATE_LINKED_EVENT, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("CreateNewEvent_F"),*((USequenceEvent*)eventLink.ExpectedType->GetDefaultObject())->ObjName)), TEXT(""));
		}

		AppendSeparator();
		Append( IDM_KISMET_COPY_CONNECTIONS, *LocalizeUnrealEd("CopyConnections"), TEXT("") );
		if( ConnCount )
		{
			Append( IDM_KISMET_CUT_CONNECTIONS,  *LocalizeUnrealEd("CutConnections"),  TEXT("") );
		}
		if( SeqEditor->CopyConnInfo.Num() && 
			SeqEditor->CopyConnType == LOC_EVENT )
		{
			Append( IDM_KISMET_PASTE_CONNECTIONS, *LocalizeUnrealEd("PasteConnections"), TEXT("") );
		}
	}
}

WxMBKismetConnectorOptions::~WxMBKismetConnectorOptions()
{

}

/*-----------------------------------------------------------------------------
	WxKismetSearch.
-----------------------------------------------------------------------------*/

static const INT KismetSearch_ControlBorder = 5;

BEGIN_EVENT_TABLE( WxKismetSearch, wxDialog )
	EVT_CLOSE( WxKismetSearch::OnClose )
	EVT_BUTTON( wxID_CANCEL, WxKismetSearch::OnCancel )
	EVT_COMBOBOX( IDM_KISMET_SEARCHTYPECOMBO, WxKismetSearch::OnSearchTypeChange )
	EVT_COMBOBOX( IDM_KISMET_SEARCHSCOPECOMBO, WxKismetSearch::OnSearchScopeChange )
	EVT_LISTBOX(IDM_KISMET_SEARCHRESULT, WxKismetSearch::OnSearchResultChanged )
	EVT_BUTTON(IDM_KISMET_DOSEARCH, WxKismetSearch::OnDoSearch )
	EVT_TEXT_ENTER(IDM_KISMET_SEARCHENTRY, WxKismetSearch::OnDoSearch )
END_EVENT_TABLE()

WxKismetSearch::WxKismetSearch(WxKismet* InKismet, wxWindowID InID)
	:	wxDialog( InKismet, InID, *LocalizeUnrealEd("KismetSearch"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER )
	,	Kismet( InKismet )
{
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

 	wxBoxSizer* TopSizerV = new wxBoxSizer( wxVERTICAL );
	SetSizer(TopSizerV);
	SetAutoLayout(true);

	// Result list box
    wxStaticText* ResultCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("Results"), wxDefaultPosition, wxDefaultSize, 0 );
    TopSizerV->Add(ResultCaption, 0, wxALIGN_LEFT|wxALL, KismetSearch_ControlBorder);

	ResultList = new wxListBox( this, IDM_KISMET_SEARCHRESULT, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE );
	TopSizerV->Add( ResultList, 1, wxGROW | wxALL, KismetSearch_ControlBorder );

	wxFlexGridSizer* GridSizer = new wxFlexGridSizer( 2, 2, 0, 0 );
	GridSizer->AddGrowableCol(1);
	TopSizerV->Add( GridSizer, 0, wxGROW | wxALL, KismetSearch_ControlBorder );

	// Name/Comment search entry
	wxStaticText* NameCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("SearchFor") );
	GridSizer->Add(NameCaption, 0, wxRIGHT | wxALL, KismetSearch_ControlBorder);

	// Add the name entry and object type combo boxes to the same sizer within the GridSizer so they
	// can easily be toggled on and off as needed
	wxBoxSizer* SearchForSizer = new wxBoxSizer( wxVERTICAL );
	NameEntry = new wxTextCtrl(this, IDM_KISMET_SEARCHENTRY, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	SearchForSizer->Add( NameEntry, 1, wxGROW | wxALL, KismetSearch_ControlBorder );

	ObjectTypeCombo = new WxComboBox(this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	SearchForSizer->Add( ObjectTypeCombo, 1, wxGROW | wxALL, KismetSearch_ControlBorder );
	GridSizer->Add( SearchForSizer, 1, wxGROW | wxALL );
	
	// Object search entry
	wxStaticText* SearchTypeCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("SearchType") );
	GridSizer->Add(SearchTypeCaption, 0, wxRIGHT | wxALL, KismetSearch_ControlBorder);

	SearchTypeCombo = new WxComboBox(this, IDM_KISMET_SEARCHTYPECOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	GridSizer->Add(SearchTypeCombo, 1, wxGROW | wxALL, KismetSearch_ControlBorder);

	SearchTypeCombo->Append( *LocalizeUnrealEd("CommentsNames") );			// KST_NameComment
	SearchTypeCombo->Append( *LocalizeUnrealEd("ReferencedObjectName") );	// KST_ObjName
	SearchTypeCombo->Append( *LocalizeUnrealEd("NamedVariable") );			// KST_VarName
	SearchTypeCombo->Append( *LocalizeUnrealEd("NamedVariableUse") );		// KST_VarNameUsed
	SearchTypeCombo->Append( *LocalizeUnrealEd("RemoteEvents") );			// KST_RemoteEvents
	SearchTypeCombo->Append( *LocalizeUnrealEd("ReferencesRemoteEvent") );	// KST_ReferencesRemoteEvent
	SearchTypeCombo->Append( *LocalizeUnrealEd("ObjectType") );				// KST_ObjectType
	SearchTypeCombo->Append( *LocalizeUnrealEd("KismetPropertyValue") );	// KST_ObjProperties

	wxStaticText* SearchScopeCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("SearchScope") );
	GridSizer->Add(SearchScopeCaption, 2, wxRIGHT | wxALL, KismetSearch_ControlBorder);

	SearchScopeCombo = new WxComboBox(this, IDM_KISMET_SEARCHSCOPECOMBO, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	GridSizer->Add(SearchScopeCombo, 3, wxGROW | wxALL, KismetSearch_ControlBorder);

	SearchScopeCombo->Append(*LocalizeUnrealEd("CurrentLevel"));
	SearchScopeCombo->Append(*LocalizeUnrealEd("CurrentSequence"));
	SearchScopeCombo->Append(*LocalizeUnrealEd("AllLevels"));

	wxBoxSizer* ButtonSizer = new wxBoxSizer( wxHORIZONTAL );
	TopSizerV->Add( ButtonSizer, 0, wxCENTER | wxALL, KismetSearch_ControlBorder );

	// Search button
	SearchButton = new wxButton( this, IDM_KISMET_DOSEARCH, *LocalizeUnrealEd("Search"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizer->Add(SearchButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	// Cancel button
	wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizer->Add(CancelButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	FWindowUtil::LoadPosSize( TEXT("KismetSearch"), this, 100, 100, 500, 300 );
	LoadSearchSettings();
}

/**
 * Called when user clicks on a search result. Changes active Sequence
 * and moves the view to center on the selected SequenceObject.
 */
void WxKismetSearch::OnSearchResultChanged(wxCommandEvent &In)
{
	SaveSearchSettings();
	USequenceObject* SeqObj = GetSelectedResult();
	if ( SeqObj )
	{
		Kismet->CenterViewOnSeqObj( SeqObj );
	}
}

/** 
 * Handler for pressing Search button (or pressing Enter on entry box) on the Kismet Search tool.
 * Searches all sequences and puts results into the ResultList list box.
 */
void WxKismetSearch::OnDoSearch( wxCommandEvent &In )
{
	// Save search settings to the editor user settings ini.
	SaveSearchSettings();
	const EKismetSearchScope Scope = (EKismetSearchScope)GetSearchScope();
	TArray<USequenceObject*> Results;
	Kismet->DoSilentSearch(Results, GetSearchString(), (EKismetSearchType)GetSearchType(), Scope, GetSearchObjectClass());
	UpdateSearchWindowResults(Results);
}

void WxKismetSearch::UpdateSearchWindowResults(TArray<USequenceObject*> &NewResults)
{
	// Update the list box with search results.
	ClearResultsList();

	for(INT Idx = 0; Idx < NewResults.Num(); Idx++)
	{
		USequenceObject *Obj = NewResults(Idx);
		if (Obj == NULL)
		{
			continue;
		}

		FString ResultName;

		// If there is a comment, put it after the object name in brackets.
		if(Obj->ObjComment.Len() > 0)
		{
			ResultName = FString::Printf( TEXT("%s (%s)"),*Obj->GetSeqObjFullName(),*Obj->ObjComment );
		}
		else
		{
			ResultName = Obj->GetSeqObjFullName();
		}

		// prepend the level name
		if ((EKismetSearchType)GetSearchScope() == KSS_AllLevels)
		{
			ULevel *Level = Cast<ULevel>(Obj->GetRootSequence()->GetOuter());
			AppendResult( *FString::Printf(TEXT("%s: %s"),*Level->GetOuter()->GetOuter()->GetName(),*ResultName), Obj );
		}
		else
		{
			AppendResult( *ResultName, Obj );
		}
	}
}

/**
 * Loads prior search settings from the editor user settings ini.
 */
void WxKismetSearch::LoadSearchSettings()
{
	EKismetSearchType SearchType	= KST_NameComment;
	EKismetSearchScope SearchScope	= KSS_AllLevels;
	FString SearchString;
	FString ClassString;

	GConfig->GetInt( TEXT("KismetSearch"), TEXT("SearchType"), (INT&)SearchType, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("KismetSearch"), TEXT("SearchScope"), (INT&)SearchScope, GEditorUserSettingsIni );
	GConfig->GetString( TEXT("KismetSearch"), TEXT("SearchString"), SearchString, GEditorUserSettingsIni );
	GConfig->GetString( TEXT("KismetSearch"), TEXT("SearchClass"), ClassString, GEditorUserSettingsIni );

	Clamp( SearchType, (EKismetSearchType)0, MAX_KST_TYPES );
	Clamp( SearchScope, (EKismetSearchScope)0, MAX_KSS_TYPES );

	SetSearchType( SearchType );
	SetSearchScope( SearchScope );
	if ( SearchString.Len() > 0 )
	{
		SetSearchString( *SearchString );
	}
	if ( ClassString.Len() > 0 && ObjectTypeCombo->GetCount() > 0 )
	{
		ObjectTypeCombo->SetSelection( 0 );
		SetSearchObjectClass( ClassString );
	}
}

/**
 * Saves prior search settings to the editor user settings ini.
 */
void WxKismetSearch::SaveSearchSettings()
{
	const INT SearchType		= GetSearchType();
	const INT SearchScope		= GetSearchScope();
	const FString SearchString	= GetSearchString();
	
	const UClass* SelectedClassType = GetSearchObjectClass();
	const FString ClassString	= SelectedClassType ? SelectedClassType->GetName() : TEXT("None");

	GConfig->SetInt( TEXT("KismetSearch"), TEXT("SearchType"), SearchType, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("KismetSearch"), TEXT("SearchScope"), SearchScope, GEditorUserSettingsIni );
	GConfig->SetString( TEXT("KismetSearch"), TEXT("SearchString"), *SearchString, GEditorUserSettingsIni );
	GConfig->SetString( TEXT("KismetSearch"), TEXT("SearchClass"), *ClassString, GEditorUserSettingsIni );
}

/**
 * Clears the results list, clearing references to the previous results.
 */
void WxKismetSearch::ClearResultsList()
{
	ResultList->Clear();
}

/**
 * Appends a results to the results list.
 *
 * @param	ResultString	The string to display in the results list.
 * @param	SequenceObj		The sequence object associated with the search result.
 */
void WxKismetSearch::AppendResult(const TCHAR* ResultString, USequenceObject* SequenceObject)
{
	ResultList->Append( ResultString, SequenceObject );
}

UBOOL WxKismetSearch::SetResultListSelection(INT Index)
{
	UBOOL bResult = FALSE;
	if ( Index >= 0 && Index <= static_cast<INT>(ResultList->GetCount()) )
	{
		ResultList->SetSelection( Index, TRUE );
		bResult = TRUE;
	}
	return bResult;
}

/**
 * @return		The selected search result sequence, or NULL if none selected.
 */
USequenceObject* WxKismetSearch::GetSelectedResult() const
{
	USequenceObject* SeqObj	= NULL;
	const INT SelIndex		= ResultList->GetSelection();
	if ( SelIndex != -1 )
	{
		SeqObj = (USequenceObject*)ResultList->GetClientData(SelIndex);
		check( SeqObj );
	}

	return SeqObj;
}

/**
 * @return		The number of results in the results list.
 */
INT WxKismetSearch::GetNumResults() const
{
	return ResultList->GetCount();
}

/**
 * @return		The current search string.
 */
FString WxKismetSearch::GetSearchString() const
{
	return (const TCHAR*)NameEntry->GetValue();
}

/**
 * Sets the search string field.
 *
 * @param	SearchString	The new search string.
 */
void WxKismetSearch::SetSearchString(const TCHAR* SearchString)
{
	NameEntry->SetValue( SearchString );
	NameEntry->SelectAll();
}

/**
* @return		The selected search type setting.
*/
INT WxKismetSearch::GetSearchType() const
{
	return SearchTypeCombo->GetSelection();
}

/**
 * Sets the search type.
 */
void WxKismetSearch::SetSearchType(EKismetSearchType SearchType)
{
	SearchTypeCombo->SetSelection( SearchType );

	// Update the controls, as the type of control displayed might have to change based on the user's selection
	UpdateSearchForControls();
}

/**
 * Sets the search scope.
 */
void WxKismetSearch::SetSearchScope(EKismetSearchScope SearchScope)
{
	SearchScopeCombo->SetSelection( SearchScope );
	
	// If the search type is by "object type," then the object type combo box
	// needs to be repopulated due to the scope change
	if ( GetSearchType() == KST_ObjectType )
	{
		UpdateObjectTypeControl();
	}
}

/**
 * @return		The selected search scope setting.
 */
INT WxKismetSearch::GetSearchScope() const
{
	return SearchScopeCombo->GetSelection();
}

/**
 * Returns the currently selected object class in the search dialog, if any
 *
 * @return	The object class type currently selected in the search dialog; NULL if no type is currently selected
 */
UClass* WxKismetSearch::GetSearchObjectClass() const
{
	UClass* SelectedClass = NULL;

	// Attempt to find the class, stored as client data, if the user has a valid selection in the combo box
	const UINT SelectedIndex = ObjectTypeCombo->GetSelection();
	if ( SelectedIndex != wxNOT_FOUND )
	{
		SelectedClass = static_cast<UClass*>( ObjectTypeCombo->GetClientData(SelectedIndex) );
	}

	return SelectedClass;
}

/**
 * Sets the currently selected object class in the search dialog, if possible
 *
 * @param	SelectedClass	Class to attempt to set the search dialog to; 
 *							No change is made if the passed in class is not currently an option in the combo box
 */
void WxKismetSearch::SetSearchObjectClass( const UClass* SelectedClass )
{
	// Only set the selection if the provided class matches one of the classes stored as client
	// data inside the combo box
	for ( UINT ClassIndex = 0; ClassIndex < ObjectTypeCombo->GetCount(); ++ClassIndex )
	{
		if ( SelectedClass == ObjectTypeCombo->GetClientData( ClassIndex ) )
		{
			ObjectTypeCombo->SetSelection( ClassIndex );
			break;
		}
	}
}

/**
 * Sets the currently selected object class in the search dialog, if possible
 *
 * @param	SelectedClassName	Name of the class to attempt to set the search dialog to; 
 *								No change is made if the passed in class name is not currently an option in the combo box
 */
void WxKismetSearch::SetSearchObjectClass( const FString& SelectedClassName )
{
	// Only set the selection if the provided class name matches one of the classes stored
	// as client data inside the combo box
	for ( UINT ClassIndex = 0; ClassIndex < ObjectTypeCombo->GetCount(); ++ClassIndex )
	{
		UClass* CurClass = static_cast<UClass*>( ObjectTypeCombo->GetClientData( ClassIndex ) );
		if ( CurClass && CurClass->GetName() == SelectedClassName )
		{
			ObjectTypeCombo->SetSelection( ClassIndex );
			break;
		}
	}	
}

/** 
 * Sorting macro to sort Sequence object classes by ObjName; 
 * Assumes the classes all produce valid default objects which are USequenceObjects 
 */
IMPLEMENT_COMPARE_POINTER( UClass, KismetMenus, 
{
	USequenceObject* SeqObjA = Cast<USequenceObject>( A->GetDefaultObject() );
	USequenceObject* SeqObjB = Cast<USequenceObject>( B->GetDefaultObject() );
	check( SeqObjA && SeqObjB );

	return appStricmp( *SeqObjA->ObjName, *SeqObjB->ObjName ); 
} )

/** Internal helper method to keep the object type combo box up-to-date based on the current search scope */
void WxKismetSearch::UpdateObjectTypeControl()
{
	// Cache the class that is currently selected so that it can be reselected after the update (if it's still a valid option)
	const UClass* CurSelectedClass = GetSearchObjectClass();

	ObjectTypeCombo->Clear();

	// Always perform a recursive sequence search unless explicitly only looking inside the current sequence
	EKismetSearchScope CurSearchScope = static_cast<EKismetSearchScope>( GetSearchScope() );
	UBOOL bRecursiveSearch = CurSearchScope != KSS_CurrentSequence;
	TArray<USequence*> SearchSeqs;

	// If the search scope specifies all levels, add the sequences of each valid, relevant level
	if ( CurSearchScope == KSS_AllLevels )
	{
		for ( TObjectIterator<ULevel> LevelIt; LevelIt; ++LevelIt )
		{
			// Ignore PIE levels
			if ( GEditor->PlayWorld && GEditor->PlayWorld->Levels.ContainsItem( *LevelIt ) )
			{
				continue;
			}
			if ( LevelIt->GameSequences.Num() > 0 )
			{
				SearchSeqs.AddItem( LevelIt->GameSequences(0) );
			}
		}
	}
	else
	{
		SearchSeqs.AddItem( Kismet->GetRootSequence() );
	}

	// Get all of the different classes represented within the sequences at the specified scope
	TArray<UClass*> UsedSequenceClasses;
	for ( TArray<USequence*>::TConstIterator SequenceIter(SearchSeqs); SequenceIter; ++SequenceIter )
	{
		GetClassesFromSequence( *SequenceIter, UsedSequenceClasses, bRecursiveSearch );
	}

	INT NewSelectionIndex = 0;

	// If the sequences weren't empty, populate the combo box with all of the represented classes
	if ( UsedSequenceClasses.Num() > 0 )
	{
		// Sort the classes by object name
		Sort<USE_COMPARE_POINTER( UClass, KismetMenus )>( &UsedSequenceClasses(0), UsedSequenceClasses.Num() );

		// Add each class to the combo box
		for ( TArray<UClass*>::TConstIterator SeqClassIter( UsedSequenceClasses ); SeqClassIter; ++SeqClassIter )
		{
			// Create a default object of each class type in order to extract the obj name. Displaying the obj name
			// in the combo box makes it easier to use/more familiar than using the actual class names.
			UClass* CurSeqClass = *SeqClassIter;
			USequenceObject* CurSeqClassDefaultObj = Cast<USequenceObject>( CurSeqClass->GetDefaultObject() );
			ObjectTypeCombo->Append( *CurSeqClassDefaultObj->ObjName, CurSeqClass );
			
			// If the class being currently added is the same as the one that was previously selected, cache the index
			// so it can be easily reselected later
			if ( CurSelectedClass && CurSeqClass == CurSelectedClass )
			{
				NewSelectionIndex = SeqClassIter.GetIndex();
			}
		}
	}
	// If the sequences were empty, provide a "None" option so the user isn't left with a completely empty combo box
	else
	{
		ObjectTypeCombo->Append( *LocalizeUnrealEd("None"), static_cast<void*>( NULL ) );
	}
	ObjectTypeCombo->SetSelection( NewSelectionIndex );
}

/**
 * Internal helper method to extract all of the sequence object classes represented within a particular sequence
 *
 * @param	CurSequence	Sequence to extract all sequence object classes from
 * @param	OutClasses	Array to populate with extracted object classes
 * @param	bRecursive	If TRUE, the method will be recursively called on any sub-sequences found while searching
 */
void WxKismetSearch::GetClassesFromSequence( const USequence* CurSequence, TArray<UClass*>& OutClasses, UBOOL bRecursive )
{
	if ( CurSequence )
	{
		// Iterate over each sequence object, adding its class to the passed in array
		for ( TArray<USequenceObject*>::TConstIterator SeqObjIter( CurSequence->SequenceObjects ); SeqObjIter; ++SeqObjIter )
		{
			const USequenceObject* CurSequenceObject = *SeqObjIter;
			OutClasses.AddUniqueItem( CurSequenceObject->GetClass() );
			
			// If a recursive search was specified, see if the current object is a sequence itself.
			// If it is, call this method on that sub-sequence as well.
			if ( bRecursive )
			{
				const USequence* CurObjAsSeq = ConstCast<USequence>( CurSequenceObject );
				if ( CurObjAsSeq )
				{
					GetClassesFromSequence( CurObjAsSeq, OutClasses, bRecursive );
				}
			}
		}
	}
}

/** 
 * Internal helper method to update which control should display next to the "Search For" label. For any search type except "object type,"
 * the search dialog should show a text box to allow the user to type in their search query. For "object type" searches, a combo box should
 * be displayed, auto-populated with all of the valid types to search for.
 */
void WxKismetSearch::UpdateSearchForControls()
{
	// Show/hide the object combo box and search string text box accordingly based on the
	// search type
	const EKismetSearchType& CurSearchType = static_cast<EKismetSearchType>( GetSearchType() );
	ObjectTypeCombo->Show( CurSearchType == KST_ObjectType  );
	NameEntry->Show( CurSearchType != KST_ObjectType  );
	
	// If the name entry box should be shown, select all its text and focus on it
	if ( CurSearchType != KST_ObjectType )
	{
		NameEntry->SelectAll();
		NameEntry->SetFocus();
	}
	// If the object type combo box should be shown, update its contents
	else
	{
		UpdateObjectTypeControl();
	}

	Layout();
}

void WxKismetSearch::OnClose(wxCloseEvent& In)
{
	FWindowUtil::SavePosSize( TEXT("KismetSearch"), this );	
	SaveSearchSettings();

	check(Kismet->SearchWindow == this);
	Kismet->SearchWindow = NULL;

	Kismet->ToolBar->ToggleTool(IDM_KISMET_OPENSEARCH, false);

	this->Destroy();
}

/**
 * Called in response to the user changing the search type combo box
 *
 * @param	In	Event generated by wxWidgets when the user changes the combo box selection
 */
void WxKismetSearch::OnSearchTypeChange( wxCommandEvent& In )
{
	// Update the controls, as the type of control displayed might have to change based on the user's selection
	UpdateSearchForControls();
}

/**
 * Called in response to the user changing the search scope combo box
 *
 * @param	In	Event generated by wxWidgets when the user changes the combo box selection
 */
void WxKismetSearch::OnSearchScopeChange( wxCommandEvent& In )
{
	// If the search type is by "object type," then the object type combo box
	// needs to be repopulated due to the scope change
	if ( GetSearchType() == KST_ObjectType )
	{
		UpdateObjectTypeControl();
	}
}


/*-----------------------------------------------------------------------------
	WxKismetClassSearchTextCtrl.
-----------------------------------------------------------------------------*/
class WxKismetClassSearchTextCtrl : public wxTextCtrl
{
public:
	WxKismetClassSearchTextCtrl( wxWindow* parent,
		wxWindowID id,
		const wxString& value = TEXT(""),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = 0 )
		: wxTextCtrl(parent, id, value, pos, size, style)
	{
		KismetParent = (WxKismetClassSearch*)(parent);
	}

	void OnKeyDown(wxKeyEvent &In);

private:
	WxKismetClassSearch* KismetParent;

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE( WxKismetClassSearchTextCtrl, wxTextCtrl )
	EVT_KEY_DOWN( WxKismetClassSearchTextCtrl::OnKeyDown )
END_EVENT_TABLE()


/*-----------------------------------------------------------------------------
	WxKismetClassSearch.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxKismetClassSearch, wxDialog )
	EVT_CLOSE( WxKismetClassSearch::OnClose )
	EVT_LIST_ITEM_ACTIVATED(IDM_KISMET_CLASSSEARCHRESULT, WxKismetClassSearch::OnSequenceClassActivated )
	EVT_LIST_COL_CLICK(IDM_KISMET_CLASSSEARCHRESULT, WxKismetClassSearch::OnColumnClicked )
	EVT_LIST_ITEM_SELECTED(IDM_KISMET_CLASSSEARCHRESULT, WxKismetClassSearch::ItemSelected )
	EVT_TEXT(IDM_KISMET_CLASSSEARCHENTRY, WxKismetClassSearch::OnDoClassSearch )
	EVT_TEXT_ENTER(IDM_KISMET_CLASSSEARCHENTRY, WxKismetClassSearch::OnSequenceClassSelected )
	EVT_BUTTON(IDM_KISMET_CREATEOBJECT, WxKismetClassSearch::OnSequenceClassSelected )
	EVT_BUTTON( wxID_CANCEL, WxKismetClassSearch::OnCancel )
END_EVENT_TABLE()


WxKismetClassSearch::WxKismetClassSearch(WxKismet* InKismet, wxWindowID InID)
		:	wxDialog( InKismet, InID, *LocalizeUnrealEd("CreateSeqObj"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER )
		,	Kismet( InKismet )
{
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

 	wxBoxSizer* TopSizerV = new wxBoxSizer( wxVERTICAL );
	SetSizer(TopSizerV);
	SetAutoLayout(true);

	// Result list box
    wxStaticText* ResultCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("Results"), wxDefaultPosition, wxDefaultSize, 0 );
    TopSizerV->Add(ResultCaption, 0, wxALIGN_LEFT|wxALL, KismetSearch_ControlBorder);

	ResultList = new wxListCtrl( this, IDM_KISMET_CLASSSEARCHRESULT, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES );
	TopSizerV->Add( ResultList, 1, wxGROW | wxALL, KismetSearch_ControlBorder );
	ResultList->InsertColumn( 0, *LocalizeUnrealEd("CreateSeqObj_Name"), wxLIST_FORMAT_LEFT, 250 );
	ResultList->InsertColumn( 1, *LocalizeUnrealEd("CreateSeqObj_Type"), wxLIST_FORMAT_LEFT, 70 );
	ResultList->InsertColumn( 2, *LocalizeUnrealEd("CreateSeqObj_Category"), wxLIST_FORMAT_LEFT, 160 );

	wxFlexGridSizer* GridSizer = new wxFlexGridSizer( 2, 2, 0, 0 );
	GridSizer->AddGrowableCol(1);
	TopSizerV->Add( GridSizer, 0, wxGROW | wxALL, KismetSearch_ControlBorder );

	// Name/Comment search entry
	wxStaticText* NameCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("SearchFor") );
	GridSizer->Add(NameCaption, 0, wxRIGHT | wxALL, KismetSearch_ControlBorder);

	NameEntry = new WxKismetClassSearchTextCtrl(this, IDM_KISMET_CLASSSEARCHENTRY, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	GridSizer->Add(NameEntry, 1, wxGROW | wxALL, KismetSearch_ControlBorder);

	wxBoxSizer *ButtonSizerV = new wxBoxSizer( wxHORIZONTAL );
	TopSizerV->Add( ButtonSizerV, 0, wxCENTER | wxALL, KismetSearch_ControlBorder );

	// Create button
	CreateButton = new wxButton( this, IDM_KISMET_CREATEOBJECT, *LocalizeUnrealEd("Create"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizerV->Add(CreateButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	// Cancel button
	wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizerV->Add(CancelButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	FWindowUtil::LoadPosSize( TEXT("KismetClassSearch"), this, 100, 100, 500, 300 );

	NameEntry->SelectAll();
	NameEntry->SetFocus();

	DoClassSearch();
}

void WxKismetClassSearch::OnClose(wxCloseEvent& In)
{
	DoClose();
}

void WxKismetClassSearch::DoClose()
{
	FWindowUtil::SavePosSize( TEXT("KismetClassSearch"), this );	

	check(Kismet->ClassSearchWindow == this);
	Kismet->ClassSearchWindow = NULL;

	Kismet->ToolBar->ToggleTool(IDM_KISMET_OPENCLASSSEARCH, false);

	Kismet->SetWindowFocus();

	this->Destroy();
}

/**
 * Called when user selects a sequence object class to add
 */
void WxKismetClassSearch::OnSequenceClassSelected(wxCommandEvent &In)
{
	PlaceSelectedClass();
}

/**
 * Called when user activates a sequence object class to add
 */
void WxKismetClassSearch::OnSequenceClassActivated(wxListEvent &In)
{
	PlaceSelectedClass();
}

/**
 * Adds a new class of the type selected in the results list
 */
void WxKismetClassSearch::PlaceSelectedClass()
{
	UClass* SeqObjClass = GetSelectedResult();
	if ( SeqObjClass )
	{
		INT NewPosX = (Kismet->LinkedObjVC->NewX - Kismet->LinkedObjVC->Origin2D.X)/Kismet->LinkedObjVC->Zoom2D;
		INT NewPosY = (Kismet->LinkedObjVC->NewY - Kismet->LinkedObjVC->Origin2D.Y)/Kismet->LinkedObjVC->Zoom2D;

		USequenceObject* SeqObj = Kismet->NewSequenceObject( SeqObjClass, NewPosX, NewPosY );
		if ( SeqObj )
		{
			Kismet->CenterViewOnSeqObj( SeqObj );
			DoClose();
		}
	}
}

UBOOL WxKismetClassSearch::OnKeyDown(wxKeyEvent &In)
{
	switch( In.GetKeyCode() )
	{
	case WXK_DOWN:
		{
			const INT SelIndex = ResultList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if ( SelIndex != -1 && SelIndex < (ResultList->GetItemCount() - 1) )
			{
				SetResultListSelection(SelIndex + 1);
			}
			return TRUE;
		}
		break;
	case WXK_UP:
		{
			const INT SelIndex = ResultList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
			if ( SelIndex > 0 )
			{
				SetResultListSelection(SelIndex - 1);
			}
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void WxKismetClassSearchTextCtrl::OnKeyDown(wxKeyEvent &In)
{
	const UBOOL bHandled = KismetParent->OnKeyDown(In);
	if (!bHandled)
	{
		In.Skip();
	}
}

FString WxKismetClassSearch::GetKismetObjectTypeString(const USequenceObject* InObject)
{
	FString ObjectType(TEXT(""));
	if ( InObject )
	{
		if (InObject->IsA(USequenceAction::StaticClass()))
		{
			ObjectType = LocalizeUnrealEd("KismetClassSearch_Action");
		}
		else if (InObject->IsA(USequenceCondition::StaticClass()))
		{
			ObjectType = LocalizeUnrealEd("KismetClassSearch_Condition");
		}
		else if (InObject->IsA(USequenceVariable::StaticClass()))
		{
			ObjectType = LocalizeUnrealEd("KismetClassSearch_Variable");
		}
		else if (InObject->IsA(USequenceEvent::StaticClass()))
		{
			ObjectType = LocalizeUnrealEd("KismetClassSearch_Event");
		}
	}
	return ObjectType;
}

/**
 * Gets the search string name for a given class type
 * 
 * @param	InClass					The class to get the string of
 * @param	SearchField				The type of search, name/type/category
 *
 * @return	FString					The string to use for the class
 */
FString WxKismetClassSearch::GetKismetClassSearchString( UClass* InClass, EKismetClassSearchFields SearchField )
{
	FString SearchString(TEXT(""));
	if ( InClass )
	{
		USequenceObject* TempDefaultObject = Cast<USequenceObject>(InClass->GetDefaultObject());
		switch ( SearchField )
		{
			// Handle name search
		case EKCSF_Name:	
			SearchString = TempDefaultObject->ObjName;
			break;

			// Handle level name search
		case EKCSF_Type:
			SearchString = GetKismetObjectTypeString(TempDefaultObject);
			break;

			// Handle tag search
		case EKCSF_Category:
			SearchString = TempDefaultObject->ObjCategory;
			break;
		}
	}
	return SearchString;
}

/**
 * 
 *
 * Orders items in the actor search dialog's list view.
 */
static int wxCALLBACK WxKismetClassResultsListSort(UPTRINT InItem1, UPTRINT InItem2, UPTRINT InSortData)
{
	// Determine the sort order of the provided actors using the current sort data
	UClass* A = reinterpret_cast<UClass*>( InItem1 );
	UClass* B = reinterpret_cast<UClass*>( InItem2 );
	const WxKismetClassSearch::FKismetClassSearchOptions* SearchOptions = reinterpret_cast<WxKismetClassSearch::FKismetClassSearchOptions*>( InSortData );
	check( A && B && SearchOptions );

	// Get the search string for each actor
	const FString StringA = WxKismetClassSearch::GetKismetClassSearchString( A, SearchOptions->Column );
	const FString StringB = WxKismetClassSearch::GetKismetClassSearchString( B, SearchOptions->Column );

	return appStricmp( *StringA, *StringB ) * ( SearchOptions->bSortAscending ? 1 : -1 );
}


/** 
 * Handler for pressing Search button (or pressing Enter on entry box) on the Kismet Class Search tool.
 * Searches all sequence object classes and puts results into the ResultList list box.
 */
void WxKismetClassSearch::OnDoClassSearch(wxCommandEvent &In)
{
	DoClassSearch();
}

void WxKismetClassSearch::DoClassSearch()
{
	const FString SearchString = GetSearchString();

	// Save search settings to the editor user settings ini.
	TArray<UClass*> Results;
	Kismet->DoClassSearch(Results, SearchString);

	// Update the list box with search results.
	ClearResultsList();

	for( INT Idx = 0; Idx < Results.Num(); ++Idx )
	{
		UClass *SeqClass = Results(Idx);
		if( SeqClass == NULL || SeqClass->HasAnyClassFlags(CLASS_Hidden | CLASS_Deprecated | CLASS_Abstract) )
		{
			continue;
		}

		USequenceObject* TempDefaultObject = Cast<USequenceObject>(SeqClass->GetDefaultObject());
		if( TempDefaultObject == NULL || !TempDefaultObject->bDeletable )
		{
			continue;
		}

		const FString TempObjectType = WxKismetClassSearch::GetKismetObjectTypeString(TempDefaultObject);
		AppendResult( *TempDefaultObject->ObjName, *TempObjectType, *TempDefaultObject->ObjCategory, SeqClass );
	}

	ResultList->SortItems( WxKismetClassResultsListSort, reinterpret_cast< UPTRINT >( &SearchOptions ) );

	INT BestIdx = 0;
	const INT ItemCount = ResultList->GetItemCount();
	for (INT Idx = 0; Idx < ItemCount; ++Idx)
	{
		UClass* SeqClass = (UClass*)ResultList->GetItemData(Idx);
		const USequenceObject* TempDefaultObject = Cast<USequenceObject>(SeqClass->GetDefaultObject());
		if (TempDefaultObject->ObjName.StartsWith(SearchString))
		{
			BestIdx = Idx;
			break;
		}
	}

	SetResultListSelection(BestIdx);
}

/**
 * Clears the results list, clearing references to the previous results.
 */
void WxKismetClassSearch::ClearResultsList()
{
	ResultList->DeleteAllItems();
}

/**
 * @return		The current search string.
 */
FString WxKismetClassSearch::GetSearchString() const
{
	return (const TCHAR*)NameEntry->GetValue();
}

/**
 * Sets the search string field.
 *
 * @param	SearchString	The new search string.
 */
void WxKismetClassSearch::SetSearchString(const TCHAR* SearchString)
{
	NameEntry->SetValue( SearchString );
}

/**
 * Appends a results to the results list.
 *
 * @param	ResultString			The string to display in the results list.
 * @param	ResultType				The type of the sequence object (Action, Event, etc.)
 * @param	ResultCategory			The category of the sequence object (Actor, Physics, etc.)
 * @param	SequenceObjectClass		The sequence object class associated with the search result.
 */
void WxKismetClassSearch::AppendResult(const TCHAR* ResultString, const TCHAR* ResultType, const TCHAR* ResultCategory, UClass* SequenceObjectClass)
{
	const INT CurrentCount = ResultList->GetItemCount();
	const LONG Index = ResultList->InsertItem( CurrentCount, ResultString );
	ResultList->SetItem( Index, 1, ResultType );
	ResultList->SetItem( Index, 2, ResultCategory );
	ResultList->SetItemPtrData( Index, (PTRINT)SequenceObjectClass );
}

void WxKismetClassSearch::ItemSelected(wxListEvent& event)
{
	UpdateItemsFont();
}

void WxKismetClassSearch::OnColumnClicked(wxListEvent& In)
{
	const INT Column = In.GetColumn();

	if( Column > -1 )
	{
		if( Column == SearchOptions.Column )
		{
			// Clicking on the same column will flip the sort order
			SearchOptions.bSortAscending = !SearchOptions.bSortAscending;
		}
		else
		{
			// Clicking on a new column will set that column as current and reset the sort order.
			SearchOptions.Column = static_cast<EKismetClassSearchFields>( In.GetColumn() );
			SearchOptions.bSortAscending = TRUE;
		}
	}

	ResultList->SortItems( WxKismetClassResultsListSort, reinterpret_cast< UPTRINT >( &SearchOptions ) );

	// Make sure the selection is still visible.
	const INT SelIndex = ResultList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if ( SelIndex != -1 )
	{
		ResultList->EnsureVisible( SelIndex );
	}
}

/**
 * Sets the selects items in the results list to bold
 */
void WxKismetClassSearch::UpdateItemsFont()
{
	const INT ItemCount = ResultList->GetItemCount();
	for (INT i = 0; i < ItemCount; ++i)
	{
		wxFont NodeFont = ResultList->GetItemFont(i);
		UBOOL FontIsBold = (NodeFont.IsOk() && NodeFont.GetWeight() == wxBOLD);
		UBOOL bSelected = ResultList->GetItemState(i, wxLIST_STATE_SELECTED) != 0;
		if (bSelected && !FontIsBold)
		{
			NodeFont.SetWeight(wxBOLD);
			ResultList->SetItemFont(i, NodeFont);
		}
		else if (!bSelected && FontIsBold)
		{
			NodeFont.SetWeight(wxNORMAL);
			ResultList->SetItemFont(i, NodeFont);
		}
	}
}

/**
 * Selects the item in the results list
 * 
 * @param	Index					The index into the result list
 *
 * @return	UBOOL					True if sucessful
 */
UBOOL WxKismetClassSearch::SetResultListSelection(INT Index)
{
	const INT SelIndex = ResultList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if ( SelIndex != -1 )
	{
		ResultList->SetItemState( SelIndex, 0, wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED );
	}
	UBOOL bResult = FALSE;
	if ( Index >= 0 && Index <= static_cast<INT>(ResultList->GetItemCount()) )
	{
		ResultList->SetItemState( Index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
		ResultList->EnsureVisible( Index );
		bResult = TRUE;
	}
	return bResult;
}

/**
 * @return		The selected search result class, or NULL if none selected.
 */
UClass* WxKismetClassSearch::GetSelectedResult() const
{
	UClass* SeqObjClass = NULL;
	const INT SelIndex = ResultList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if ( SelIndex != -1 )
	{
		SeqObjClass = (UClass*)ResultList->GetItemData(SelIndex);
		check( SeqObjClass );
	}

	return SeqObjClass;
}

/*-----------------------------------------------------------------------------
	WxKismetUpdate.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxKismetUpdate, wxDialog )
	EVT_CLOSE( WxKismetUpdate::OnClose )
	EVT_BUTTON( wxID_CANCEL, WxKismetUpdate::OnCancel )
	EVT_LISTBOX( IDM_KISMET_UPDATERESULT, WxKismetUpdate::OnUpdateListChanged )
	EVT_BUTTON( IDM_KISMET_DOUPDATE, WxKismetUpdate::OnContextUpdate )
	EVT_BUTTON( IDM_KISMET_DOUPDATEALL, WxKismetUpdate::OnContextUpdateAll )
END_EVENT_TABLE()

WxKismetUpdate::WxKismetUpdate(WxKismet* InKismet, wxWindowID InID)
	: wxDialog( InKismet, InID, *LocalizeUnrealEd("KismetUpdate"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxRESIZE_BORDER )
{
	Kismet = InKismet;

	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));

	wxBoxSizer* TopSizerV = new wxBoxSizer( wxVERTICAL );
	this->SetSizer(TopSizerV);
	this->SetAutoLayout(true);

	// Result list box
    wxStaticText* ResultCaption = new wxStaticText( this, -1, *LocalizeUnrealEd("UpdateList"), wxDefaultPosition, wxDefaultSize, 0 );
    TopSizerV->Add(ResultCaption, 0, wxALIGN_LEFT|wxALL, KismetSearch_ControlBorder);

	UpdateList = new wxListBox( this, IDM_KISMET_UPDATERESULT, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE );
	TopSizerV->Add( UpdateList, 1, wxGROW | wxALL, KismetSearch_ControlBorder );


	wxFlexGridSizer* GridSizer = new wxFlexGridSizer( 2, 2, 0, 0 );
	GridSizer->AddGrowableCol(1);
	TopSizerV->Add( GridSizer, 0, wxGROW | wxALL, KismetSearch_ControlBorder );

	wxBoxSizer *ButtonSizerV = new wxBoxSizer( wxHORIZONTAL );
	TopSizerV->Add( ButtonSizerV, 0, wxCENTER | wxALL, KismetSearch_ControlBorder );

	// Update button
	UpdateButton = new wxButton( this, IDM_KISMET_DOUPDATE, *LocalizeUnrealEd("Update"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizerV->Add(UpdateButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	// Update All button
	UpdateAllButton = new wxButton( this, IDM_KISMET_DOUPDATEALL, *LocalizeUnrealEd("UpdateAll"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizerV->Add(UpdateAllButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	// Cancel button
	wxButton* CancelButton = new wxButton( this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	ButtonSizerV->Add(CancelButton, 0, wxCENTER | wxALL, KismetSearch_ControlBorder);

	FWindowUtil::LoadPosSize( TEXT("KismetUpdate"), this, 100, 100, 500, 300 );

	BuildUpdateList();
}

void WxKismetUpdate::OnContextUpdate(wxCommandEvent &In)
{
	if (Kismet != NULL && UpdateList != NULL)
	{
		INT SelIdx = UpdateList->GetSelection();
		if (SelIdx != -1)
		{
			// Get the selected SequenceObject pointer.
			USequenceObject* SeqObj = (USequenceObject*)UpdateList->GetClientData(SelIdx);

			GEditor->BeginTransaction(*FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("KismetUndo_Update_f")), *SeqObj->GetSeqObjFullName())));
			SeqObj->UpdateObject();
			GEditor->EndTransaction();
		}
	}
	BuildUpdateList();
}

void WxKismetUpdate::OnContextUpdateAll(wxCommandEvent &In)
{
	if (Kismet != NULL && UpdateList != NULL)
	{
		// this indicates whether the undo transaction should be cancelled or not
		UBOOL bUpdatedObjects = FALSE;

		// begin a transaction
		INT CancelIndex = GEditor->BeginTransaction(*LocalizeUnrealEd(TEXT("KismetUndo_UpdateMultiple")));

		UpdateList->Clear();
		TArray<USequence*> Seqs;
		Seqs.AddItem(Kismet->GetRootSequence());
		for (INT SeqIdx = 0; SeqIdx < Seqs.Num(); SeqIdx++)
		{
			for (INT Idx = 0; Idx < Seqs(SeqIdx)->SequenceObjects.Num(); Idx++)
			{
				USequenceObject *Obj = Seqs(SeqIdx)->SequenceObjects(Idx);
				if (Obj != NULL)
				{
					if (Obj->IsA(USequence::StaticClass()))
					{
						Seqs.AddItem((USequence*)Obj);
					}
					else if (Obj->eventGetObjClassVersion() != Obj->ObjInstanceVersion)
					{
						bUpdatedObjects = TRUE;
						Obj->UpdateObject();
					}
				}
			}
		}

		// end the transaction if we actually updated some sequence objects
		if ( bUpdatedObjects == TRUE )
		{
			GEditor->EndTransaction();
		}
		else
		{
			// otherwise, cancel this transaction
			GEditor->CancelTransaction(CancelIndex);
		}
	}
	BuildUpdateList();
}

void WxKismetUpdate::OnUpdateListChanged(wxCommandEvent &In)
{
	if (Kismet != NULL)
	{
		INT SelIdx = UpdateList->GetSelection();
		if (SelIdx != -1)
		{
			// Get the selected SequenceObject pointer.
			USequenceObject* SeqObj = (USequenceObject*)UpdateList->GetClientData(SelIdx);
			Kismet->CenterViewOnSeqObj(SeqObj);
		}
	}
}

void WxKismetUpdate::BuildUpdateList()
{
	if (Kismet != NULL && UpdateList != NULL)
	{
		UpdateList->Clear();
		TArray<USequence*> Seqs;

		// This should never be called while GWorld is a PIE world.
		check( !GIsPlayInEditorWorld );
		for ( INT LevelIndex = 0 ; LevelIndex < GWorld->Levels.Num() ; ++LevelIndex )
		{
			ULevel* Level = GWorld->Levels(LevelIndex);
			if (Level->GameSequences.Num() > 0)
			{
				USequence* RootSeq = Level->GameSequences(0);
				check(RootSeq);
				Seqs.AddItem( RootSeq );
			}
		}

		for (INT SeqIdx = 0; SeqIdx < Seqs.Num(); SeqIdx++)
		{
			for (INT Idx = 0; Idx < Seqs(SeqIdx)->SequenceObjects.Num(); Idx++)
			{
				USequenceObject *Obj = Seqs(SeqIdx)->SequenceObjects(Idx);
				if (Obj != NULL)
				{
					if (Obj->IsA(USequence::StaticClass()))
					{
						Seqs.AddItem((USequence*)Obj);
					}
					else
					if (Obj->eventGetObjClassVersion() != Obj->ObjInstanceVersion)
					{
						UpdateList->Append(*Obj->GetSeqObjFullName(),Obj);
					}
				}
			}
		}
	}
}

WxKismetUpdate::~WxKismetUpdate()
{
}

void WxKismetUpdate::OnClose(wxCloseEvent& In)
{
	FWindowUtil::SavePosSize( TEXT("KismetUpdate"), this );	

	Kismet->ToolBar->ToggleTool(IDM_KISMET_OPENUPDATE, false);

	this->Destroy();
}

/* ==========================================================================================================
	WxSequenceTreeCtrl
========================================================================================================== */
IMPLEMENT_DYNAMIC_CLASS(WxSequenceTreeCtrl,WxTreeCtrl)

FArchive& operator<<(FArchive &Ar, wxTreeItemId &Id)
{
	return Ar;
}

/** FSerializableObject interface */
void WxSequenceTreeCtrl::Serialize( FArchive& Ar )
{
	Ar << TreeMap;
}

/** Constructor */
WxSequenceTreeCtrl::WxSequenceTreeCtrl()
: KismetEditor(NULL)
{
}

/** Destructor */
WxSequenceTreeCtrl::~WxSequenceTreeCtrl()
{
}

/**
 * Initialize this control.  Must be the first function called after creation.
 *
 * @param	InParent	the window that opened this dialog
 * @param	InID		the ID to use for this dialog
 * @param	InEditor	pointer to the editor window that contains this control
 * @param   InStyle		Style of this tree control.
 */
void WxSequenceTreeCtrl::Create( wxWindow* InParent, wxWindowID InID, WxKismet* InEditor, LONG InStyle)
{
	KismetEditor = InEditor;
	const UBOOL bSuccess = WxTreeCtrl::Create(InParent,InID,NULL, InStyle);
	check( bSuccess );
}

/** Utility for returning total number of sequence objects within a sequence (excluding objects in subsequences) */
static INT GetNumSequenceObjectsWithin(USequence* Seq)
{
	TArray<USequenceObject*> Objs;
	Seq->FindSeqObjectsByClass(USequenceObject::StaticClass(), Objs, FALSE);
	return Objs.Num();
}

/**
 * Adds the root sequence of the specified level to the sequence tree, if a sequence exists for the specified level.
 *
 * @param	Level		The level whose root sequence should be added.  Can be NULL.
 * @param	RootId		The ID of the tree's root node.
 */
void WxSequenceTreeCtrl::AddLevelToTree(ULevel* Level, wxTreeItemId RootId)
{
	// Only add the level if it contains any sequences.
	if ( Level && Level->GameSequences.Num() > 0 )
	{
		USequence* RootSeq = Level->GameSequences(0);
		check(RootSeq); // Should not be able to open Kismet without the level having a Sequence.

		const FString SeqTitle = FString::Printf(TEXT("%s - %s [%d]"),*RootSeq->GetOutermost()->GetName(),*RootSeq->ObjName, GetNumSequenceObjectsWithin(RootSeq));
		const wxTreeItemId RootSeqId = AppendItem( RootId, *SeqTitle, 0, 0, new WxTreeObjectWrapper(RootSeq) );
		TreeMap.Set( RootSeq, RootSeqId );

		AddChildren(RootSeq, RootSeqId);
	}
}

/**
 * Used to restore the expanded state of the tree items when the tree is refreshed
 */
void WxSequenceTreeCtrl::FindExpandedNodes(wxTreeItemId NodeItem, TArray<FString>& NodeList, UBOOL GenerateList)
{
	// Return if the node is invalid or we're restoring the state of tree but the list of nodes is empty
	if (!NodeItem.IsOk() || (!GenerateList && (NodeList.Num() == 0)))
	{
		return;
	}

	// Note: this method does not take into account tree items with identical names
	
	FString NodeName;
	FString NodeText = FString(GetItemText(NodeItem));
	
	// Nodes are tracked via their name, but the name contains the number of items in the sequence,
	// which can change when the tree is refreshed, so that number needs to be removed from the name
	// before it can be used as an identifier
	if (appStrstr(*NodeText, TEXT(" [")))
	{
		NodeText.Split(TEXT(" ["), &NodeName, NULL);
	}
	else
	{
		NodeName = NodeText;
	}
	
	if (GenerateList)
	{
		if (IsExpanded(NodeItem))
		{
			NodeList.AddItem(NodeName);
		}
	}
	else // If we're not generating the list of expanded nodes, that means we're using said list to restore the state of the tree instead
	{
		if (NodeList.ContainsItem(NodeName))
		{
			Expand(NodeItem);
		}
	}

	wxTreeItemIdValue Cookie;
	wxTreeItemId ChildItem = GetFirstChild(NodeItem, Cookie);

	while (ChildItem.IsOk())
	{
		FindExpandedNodes(ChildItem, NodeList, GenerateList);

		ChildItem = GetNextChild(NodeItem, Cookie);
	}
}

/**
 * Repopulates the tree control with the sequences of all levels in the world.
 */
void WxSequenceTreeCtrl::RefreshTree()
{
	// Save off a list of expanded tree items
	TArray<FString> ExpandedNodes;
	FindExpandedNodes(GetRootItem(), ExpandedNodes, TRUE);

	DeleteAllItems();
	TreeMap.Empty();

	//@fixme localization
	wxTreeItemId RootId = AddRoot( TEXT("Sequences"), 0, 0, NULL );

	// This should never be called while GWorld is a PIE world.
	check( !GIsPlayInEditorWorld );

	// Add the persistent level
	AddLevelToTree( GWorld->PersistentLevel, RootId );

	// Add any streaming levels.
	AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
	for( INT LevelIndex = 0 ; LevelIndex < WorldInfo->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* CurStreamingLevel = WorldInfo->StreamingLevels( LevelIndex );
		if( CurStreamingLevel )
		{
			AddLevelToTree( CurStreamingLevel->LoadedLevel, RootId );
		}
	}

	// Restore the expanded state of the tree
	FindExpandedNodes(RootId, ExpandedNodes, FALSE);
}

/**
 * Deletes all tree items from the list and clears the Sequence-ItemId map.
 */
void WxSequenceTreeCtrl::ClearTree()
{
	TreeMap.Empty();
	DeleteAllItems();
}

/**
 * Recursively adds all subsequences of the specified sequence to the tree control.
 */
void WxSequenceTreeCtrl::AddChildren( USequence* RootSeq, wxTreeItemId ParentId )
{
	check(RootSeq);

	// Find all sequences. The function will always return parents before children in the array.
	TArray<USequenceObject*> SeqObjs;
	RootSeq->FindSeqObjectsByClass( USequence::StaticClass(), SeqObjs, FALSE );

	// Iterate over sequences, adding them to the tree.
	for(INT i=0; i<SeqObjs.Num(); i++)
	{
		//debugf( TEXT("Item: %d %s"), i, SeqObjs(i)->GetName() );
		USequence* Seq = CastChecked<USequence>( SeqObjs(i) );

		// Only the root sequence should have an Outer that is not another Sequence (in the root case its the level).
		USequence* ParentSeq = CastChecked<USequence>( Seq->GetOuter() );
		wxTreeItemId* SeqParentId = TreeMap.Find(ParentSeq);

		// make sure that this sequence's Outer has the same ID that was passed in.
		check( *SeqParentId == ParentId );

		const FString SeqTitle = FString::Printf(TEXT("%s - [%d]"),*Seq->GetName(), GetNumSequenceObjectsWithin(Seq));
		wxTreeItemId NodeId = AppendItem( ParentId, *SeqTitle, 0, 0, new WxTreeObjectWrapper(Seq) );
		TreeMap.Set( Seq, NodeId );

		// add the child sequences for this sequence (this is new, so this may not be desired behavior)
		AddChildren( Seq, NodeId );
		SortChildren( ParentId );
	}
}

/**
 * De/selects the tree item corresponding to the specified object
 *
 * @param	SeqObj		the sequence object to select in the tree control
 * @param	bSelect		whether to select or deselect the sequence
 *
 * @return	True if the specified sequence object was found
 */
UBOOL WxSequenceTreeCtrl::SelectObject( USequenceObject* SeqObj, UBOOL bSelect/*=TRUE*/ )
{
	wxTreeItemId* TreeItemId = TreeMap.Find( SeqObj );
	if ( TreeItemId != NULL )
	{
		SelectItem( *TreeItemId, bSelect == TRUE );
		if ( bSelect )
		{
			EnsureVisible( *TreeItemId );
		}

		return TRUE;
	}

	return FALSE;
}

/*-----------------------------------------------------------------------------
	WxKismetDebuggerToolbar
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxKismetDebuggerToolBar, WxKismetToolBar )
END_EVENT_TABLE()

WxKismetDebuggerToolBar::WxKismetDebuggerToolBar(wxWindow* InParent, wxWindowID InID)
	:WxKismetToolBar( InParent, InID )
{
	ClearTools();
	PauseB.Load("KIS_Pause");
	ContinueB.Load("KIS_Continue");
	NextB.Load("KIS_Next");
	StepB.Load("KIS_Step"); 

	AddTool( IDM_KISMET_REALTIMEDEBUGGING_PAUSE, PauseB, *LocalizeUnrealEd("PauseRealtimeDebugging"));
	AddTool( IDM_KISMET_REALTIMEDEBUGGING_CONTINUE, ContinueB, *LocalizeUnrealEd("ContinueRealtimeDebugging"));
	AddTool( IDM_KISMET_REALTIMEDEBUGGING_NEXT, NextB, *LocalizeUnrealEd("NextRealtimeDebugging"));
	AddTool( IDM_KISMET_REALTIMEDEBUGGING_STEP, StepB, *LocalizeUnrealEd("StepRealtimeDebugging"));

	AddSeparator();
	AddTool(IDM_KISMET_CLEARBREAKPOINTS, ClearBreakPointsB, *LocalizeUnrealEd("ClearKismetBreakpoints"));

	UpdateDebuggerButtonState();
}

WxKismetDebuggerToolBar::~WxKismetDebuggerToolBar()
{

}

void WxKismetDebuggerToolBar::UpdateDebuggerButtonState()
{
	UBOOL IsPaused = GEditor->PlayWorld && GEditor->PlayWorld->GetWorldInfo()->bDebugPauseExecution;
	EnableTool(IDM_KISMET_REALTIMEDEBUGGING_PAUSE, !IsPaused? true: false);
	EnableTool(IDM_KISMET_REALTIMEDEBUGGING_CONTINUE, IsPaused? true: false);
	EnableTool(IDM_KISMET_REALTIMEDEBUGGING_NEXT, IsPaused? true: false);
	EnableTool(IDM_KISMET_REALTIMEDEBUGGING_STEP, IsPaused? true: false);
}
/*-----------------------------------------------------------------------------
	WxMBKismetDebuggerObjectOptions
-----------------------------------------------------------------------------*/
WxMBKismetDebuggerBasicOptions::WxMBKismetDebuggerBasicOptions(WxKismet* SeqEditor)
{
	if(SeqEditor->bIsDebuggerWindow)
	{
		WxKismetDebugger* KismetWindow = SeqEditor->GetDebuggerWindow();
		if(KismetWindow && KismetWindow->Callstack->CallstackHasNodes())
		{
			Append( IDM_KISMET_REALTIMEDEBUGGING_COPYCALLSTACK, *LocalizeUnrealEd("KismetRealtimeDebuggingCallstackCopy"), TEXT(""));
		}
	}
}

WxMBKismetDebuggerBasicOptions::~WxMBKismetDebuggerBasicOptions()
{

}

/*-----------------------------------------------------------------------------
	WxMBKismetDebuggerObjectOptions
-----------------------------------------------------------------------------*/

WxMBKismetDebuggerObjectOptions::WxMBKismetDebuggerObjectOptions(WxKismet* SeqEditor)
{
	// Iterate through selected sequence objects to see if any breakpoints are set 
	if( SeqEditor->SelectedSeqObjs.Num() > 0 )
	{
		UBOOL BreakpointNotSet = FALSE;
		UBOOL BreakpointIsSet = FALSE;
		// If any breakpoints are set or not set, we want to add menu options
		for ( INT ObjIdx = 0; ObjIdx < SeqEditor->SelectedSeqObjs.Num(); ObjIdx++)
		{
			USequenceObject* Object = SeqEditor->SelectedSeqObjs(ObjIdx);
			USequenceOp* Op = Cast<USequenceOp>(Object);
			if(Op)
			{
				if( Op->bIsBreakpointSet )
				{
					BreakpointIsSet = TRUE;
				}
				else
				{
					BreakpointNotSet = TRUE;
				}
			}
		}
		// if either is true, append a separator
		if( BreakpointIsSet || BreakpointNotSet )
		{
			if(!SeqEditor->bIsDebuggerWindow)
			{
				AppendSeparator();
			}
			// And then append the menu options that are needed
			if( BreakpointNotSet )
			{
				Append( IDM_KISMET_SET_BREAKPOINT, *LocalizeUnrealEd("KismetSetBreakpoint"), TEXT(""));
			}
			if( BreakpointIsSet )
			{
				Append( IDM_KISMET_CLEAR_BREAKPOINT, *LocalizeUnrealEd("KismetClearBreakpoint"), TEXT(""));
			}
			if(SeqEditor->bIsDebuggerWindow)
			{
				WxKismetDebugger* KismetWindow = SeqEditor->GetDebuggerWindow();
				if(KismetWindow && KismetWindow->Callstack->CallstackHasNodes())
				{
					Append( IDM_KISMET_REALTIMEDEBUGGING_COPYCALLSTACK, *LocalizeUnrealEd("KismetRealtimeDebuggingCallstackCopy"), TEXT(""));
				}
			}
		}

	}
}

WxMBKismetDebuggerObjectOptions::~WxMBKismetDebuggerObjectOptions()
{

}

/**
 *	WxKismetDebuggerNextToolBarButtonRightClick 
 */
WxKismetDebuggerNextToolBarButtonRightClick::WxKismetDebuggerNextToolBarButtonRightClick(WxKismetDebugger* Window)
{
	DebuggerWindow = Window;

	wxMenuItem* TempItem;
	wxEvtHandler* EvtHandler = GetEventHandler();
	INT CheckNextType;
	GConfig->GetInt(TEXT("KismetDebuggerOptions"), TEXT("NextType"), CheckNextType, GEditorUserSettingsIni);

	TempItem = AppendCheckItem(KDNT_NextAny, *LocalizeUnrealEd(TEXT("NextRealtimeDebuggingAny")));
	check(TempItem->GetId() == KDNT_NextAny);
	if (CheckNextType == KDNT_NextAny)
	{
		Check(KDNT_NextAny, TRUE);
	}
	EvtHandler->Connect(KDNT_NextAny, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxKismetDebuggerNextToolBarButtonRightClick::OnNextTypeButton));

	TempItem = AppendCheckItem(KDNT_NextBreakpointChain, *LocalizeUnrealEd(TEXT("NextRealtimeDebuggingBreakpoint")));
	check(TempItem->GetId() == KDNT_NextBreakpointChain);
	if (CheckNextType == KDNT_NextBreakpointChain)
	{
		Check(KDNT_NextBreakpointChain, TRUE);
	}
	EvtHandler->Connect(KDNT_NextBreakpointChain, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxKismetDebuggerNextToolBarButtonRightClick::OnNextTypeButton));

	TempItem = AppendCheckItem(KDNT_NextSelected, *LocalizeUnrealEd(TEXT("NextRealtimeDebuggingSelected")));
	check(TempItem->GetId() == KDNT_NextSelected);
	if (CheckNextType == KDNT_NextSelected)
	{
		Check(KDNT_NextSelected, TRUE);
	}
	EvtHandler->Connect(KDNT_NextSelected, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WxKismetDebuggerNextToolBarButtonRightClick::OnNextTypeButton));
}

WxKismetDebuggerNextToolBarButtonRightClick::~WxKismetDebuggerNextToolBarButtonRightClick()
{
}

void WxKismetDebuggerNextToolBarButtonRightClick::OnNextTypeButton( wxCommandEvent& In )
{
	if (DebuggerWindow)
	{
		INT NextType = Clamp<INT>(In.GetId(), KDNT_NextAny, KDNT_NextSelected);
		GConfig->SetInt(TEXT("KismetDebuggerOptions"), TEXT("NextType"), NextType, GEditorUserSettingsIni);
		if (DebuggerWindow->NextType != NextType)
		{
			DebuggerWindow->bAreHiddenBreakpointsSet = FALSE;
		}
		DebuggerWindow->NextType = NextType;
	}
}

/*-----------------------------------------------------------------------------
	WxKismetDebuggerCallstack
-----------------------------------------------------------------------------*/
IMPLEMENT_DYNAMIC_CLASS( WxKismetDebuggerCallstack, wxPanel );

BEGIN_EVENT_TABLE( WxKismetDebuggerCallstack, wxPanel )
	EVT_CLOSE( WxKismetDebuggerCallstack::OnClose )
END_EVENT_TABLE()

WxKismetDebuggerCallstack::~WxKismetDebuggerCallstack()
{
	ClearCallstack();
	delete NodeList;
	NodeList = NULL;
}


/**
 * Initialize the callstack panel.  Must be the first function called after creation.
 *
 * @param	InParent	the window that opened this dialog
 */
void WxKismetDebuggerCallstack::Create(wxWindow* InParent)
{
	NodeList = NULL;
	KismetDebugger = (WxKismetDebugger*)InParent;
	check(KismetDebugger);

	const bool bWasCreateSuccessful = wxPanel::Create(InParent, -1, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN);
	check (bWasCreateSuccessful);

	wxBoxSizer* TopSizerV = new wxBoxSizer( wxVERTICAL );
	SetSizer(TopSizerV);
	SetAutoLayout(true);

	NodeList = new wxListBox( this, IDM_KISMET_REALTIMEDEBUGGING_CALLSTACKNODE, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE );
	TopSizerV->Add( NodeList, 1, wxGROW | wxALL, KismetSearch_ControlBorder );
}

/**
 * Callback for when the editor closes, clear the callstack to ensure all data is cleaned up properly
 */
void WxKismetDebuggerCallstack::OnClose(wxCloseEvent& In)
{
	ClearCallstack();
	check(KismetDebugger->Callstack == this);
	KismetDebugger->Callstack = NULL;

	this->Destroy();
}

/**
 * Clears the callstack, clearing references to nodes
 */
void WxKismetDebuggerCallstack::ClearCallstack()
{
	USequenceOp* SeqOp = NULL;
	NodeList->SetSelection(wxNOT_FOUND);
	// Clear the Activator sequence ops in the callstack
	for(UINT Idx = 0; Idx < NodeList->GetCount(); Idx++)
	{
		SeqOp = (USequenceOp*)NodeList->GetClientData(Idx);
		SeqOp->ActivatorSeqOp = NULL;
	}
	SeqOp = NULL;
	NodeList->Clear();
}

/**
 * Appends a node to the nodes list
 *
 * @param NodeNameString The name of the node to display in the list
 * @param SequenceOp A reference to the sequenceop to jump to that node when selected
 */
UBOOL WxKismetDebuggerCallstack::AppendNode(USequenceOp* SequenceOp)
{
	FString DisplayName;
	USequenceOp* TempOp = NULL;
	// Check to make sure the Op isn't already in the callstack
	for(UINT Idx = 0; Idx < NodeList->GetCount(); Idx++)
	{
		TempOp = (USequenceOp*)NodeList->GetClientData(Idx);
		if(SequenceOp == TempOp)
		{
			TempOp = NULL;
			return FALSE;
		}
	}

	TempOp = NULL;

	// Build the name string
	if(SequenceOp->bIsBreakpointSet)
	{
		// Display if a breakpoint is set on the sequence op
		DisplayName += "(B) ";
	}
	if(SequenceOp->LastActivatedInputLink != -1)
	{
		DisplayName += SequenceOp->InputLinks(SequenceOp->LastActivatedInputLink).LinkDesc;
		DisplayName += " -> ";
	}
	DisplayName += SequenceOp->ObjName;
	if(SequenceOp->IsA(USeqAct_Delay::StaticClass()))
	{
		DisplayName += FString::Printf(TEXT(" (%d) "),((USeqAct_Delay*)SequenceOp)->Duration);
	}
	// We always start the callstack with the node we've set the breakpoint on, so don't add output if this is our first node
	if(SequenceOp->LastActivatedOutputLink != -1 && NodeList->GetCount() > 0)
	{
		FSeqOpOutputLink& Link = SequenceOp->OutputLinks(SequenceOp->LastActivatedOutputLink);
		DisplayName += " -> ";
		DisplayName += Link.LinkDesc;
		if (Link.ActivateDelay > 0.0f)
		{
			DisplayName += FString::Printf(TEXT(" (%f) "), Link.ActivateDelay);
		}
	}
	NodeList->Append( *DisplayName, SequenceOp );
	return TRUE;
}

/**
 * Sets the selection to the passed in index
 */
UBOOL WxKismetDebuggerCallstack::SetNodeListSelection(INT Index)
{
	UBOOL bResult = FALSE;
	if ( Index >= 0 && Index <= static_cast<INT>(NodeList->GetCount()) )
	{
		NodeList->SetSelection( Index, TRUE );
		bResult = TRUE;
	}
	return bResult;
}

/**
 * Sets the selection to the passed in SequenceObject if it exists
 *
 * @param SequenceObj the Object to search for in the listbox
 */
UBOOL WxKismetDebuggerCallstack::SetNodeListSelectionObject(USequenceObject* SequenceObj)
{
	// SequenceObj shouldn't ever be null, but this is a precaution
	if( SequenceObj && SequenceObj->IsA(USequenceOp::StaticClass()))
	{
		USequenceOp* SeqOp = NULL;
		// Seek through the list to find the op
		for(UINT Idx = 0; Idx < NodeList->GetCount(); Idx++)
		{
			SeqOp = (USequenceOp*)NodeList->GetClientData(Idx);
			if(SequenceObj == SeqOp)
			{
				SetNodeListSelection(Idx);
				SeqOp = NULL;
				return TRUE;
			}
		}
		SeqOp = NULL;	
	}
	return FALSE;
}

/**
 * Gets the selected SequenceOp
 *
 * @return Reference to the selected sequenceop
 */
USequenceOp* WxKismetDebuggerCallstack::GetSelectedNode() const
{
	USequenceOp* SeqOp = NULL;
	const INT SelIndex = NodeList->GetSelection();
	if ( SelIndex != -1 )
	{
		SeqOp = (USequenceOp*)NodeList->GetClientData(SelIndex);
		check( SeqOp );
	}

	return SeqOp;
}

wxListBox* WxKismetDebuggerCallstack::GetNodeList() const
{
	return NodeList;
}

/**
 * Returns true if callstack has nodes
 */
UBOOL WxKismetDebuggerCallstack::CallstackHasNodes() const
{
	return !NodeList->IsEmpty();
}

/**
 * Copy the callstack to the clipboard
 */
void WxKismetDebuggerCallstack::CopyCallstack() const
{
	FString CallstackString;
	for(UINT Idx = 0; Idx < NodeList->GetCount(); Idx++)
	{
		CallstackString += NodeList->GetString(Idx);
		CallstackString += "\r\n";
	}
	appClipboardCopy(*CallstackString);
}