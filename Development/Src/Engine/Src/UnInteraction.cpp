/*=============================================================================
	UnInteraction.cpp: See .UC for for info
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "UnUIKeys.h"

IMPLEMENT_CLASS(UConsole);
IMPLEMENT_CLASS(UInteraction);
IMPLEMENT_CLASS(UUIInteraction);

// Initialize all of the UI event keys.
#define DEFINE_UIKEY(Name) FName UIKEY_##Name;
	#include "UnUIKeys.h"
#undef DEFINE_UIKEY

#if STATS
extern FStatGroupFactory GroupFactory_STATGROUP_UI;
#endif

DECLARE_CYCLE_STAT(TEXT("Process Input Time"),STAT_UIProcessInput,STATGROUP_UI);


struct FMapCache : public FMapPackageFileCache
{
	void CacheMaps( const TCHAR* InPath)
	{
	}

public:
	FMapCache(const TCHAR* InPath)
	: FMapPackageFileCache()
	{
		CacheMaps(InPath);
	}
};

class FConsoleVariableAutoCompleteVisitor :public IConsoleVariableVisitor
{
public:
	// constructor
	FConsoleVariableAutoCompleteVisitor(TArrayNoInit<struct FAutoCompleteCommand>& InSink)
		:Sink(InSink)
	{
	}

	// interface IConsoleVariableVisitor ----------------------------------

	// @param Name must not be 0
	// @param CVar must not be 0
	virtual void OnConsoleVariable(const TCHAR *Name, IConsoleVariable* CVar)
	{
#if FINAL_RELEASE
		if(CVar->TestFlags(ECVF_Cheat))
		{
			return;
		}
#endif // FINAL_RELEASE
		if(CVar->TestFlags(ECVF_Unregistered))
		{
			return;
		}

		// can be optimized
		INT NewIdx = Sink.AddZeroed(1);

		FAutoCompleteCommand& Cmd = Sink(NewIdx);
		
		Cmd.Command = Name;
	}

private: // ------------------------------------------------

	TArrayNoInit<struct FAutoCompleteCommand>&		Sink;
};


void UConsole::BuildRuntimeAutoCompleteList(UBOOL bForce)
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if (!bForce)
	{
		// unless forced delay updating until needed
		bIsRuntimeAutoCompleteUpToDate = FALSE;
		return;
	}
	// clear the existing tree
	//@todo - probably only need to rebuild the tree + partial command list on level load
	for (INT Idx = 0; Idx < AutoCompleteTree.ChildNodes.Num(); Idx++)
	{
		FAutoCompleteNode *Node = AutoCompleteTree.ChildNodes(Idx);
		delete Node;
	}
	AutoCompleteTree.ChildNodes.Empty();
	// copy the manual list first
	AutoCompleteList.Empty();
	AutoCompleteList.AddZeroed(ManualAutoCompleteList.Num());
	for (INT Idx = 0; Idx < ManualAutoCompleteList.Num(); Idx++)
	{
		AutoCompleteList(Idx) = ManualAutoCompleteList(Idx);
	}

	// console variables
	{
		FConsoleVariableAutoCompleteVisitor Visitor(AutoCompleteList);

		GConsoleManager->ForEachConsoleVariable(&Visitor);
	}

	// iterate through script exec functions and append to the list
	INT ScriptExecCnt = 0;
	for (TObjectIterator<UFunction> It; It; ++It)
	{
		UFunction *Func = *It;
		// exec functions that either have no parent, or are in the global state (filtering some unnecessary dupes)
		if (Func->HasAnyFunctionFlags(FUNC_Exec) && (Func->GetSuperFunction() == NULL || Func->GetOuter()->IsA(UClass::StaticClass())))
		{
			FString FuncName = Func->GetName();
			INT NewIdx = AutoCompleteList.AddZeroed(1);
			AutoCompleteList(NewIdx).Command = FuncName;
			// build a help string
			// append each property (and it's type) to the help string
			for (TFieldIterator<UProperty> PropIt(Func); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				UProperty *Prop = *PropIt;
				FuncName = FString::Printf(TEXT("%s %s[%s]"),*FuncName,*Prop->GetName(),*Prop->GetCPPType());
			}
			AutoCompleteList(NewIdx).Desc = FuncName;
			ScriptExecCnt++;
		}
	}
	// enumerate all Kismet console and remote events
	INT KismetExecCnt = 0;
	USequence *BaseSeq = GWorld->CurrentLevel != NULL ? GWorld->GetGameSequence() : NULL;
	if (BaseSeq != NULL)
	{
		{
			TArray<USequenceObject*> ConsoleEvts;
			BaseSeq->FindSeqObjectsByClass(USeqEvent_Console::StaticClass(),ConsoleEvts);
			for (INT Idx = 0; Idx < ConsoleEvts.Num(); Idx++)
			{
				USeqEvent_Console* Evt = Cast<USeqEvent_Console>(ConsoleEvts(Idx));
				if (Evt->bEnabled && Evt->ConsoleEventName != NAME_None)
				{
					INT NewIdx = AutoCompleteList.AddZeroed(1);
					AutoCompleteList(NewIdx).Command = FString::Printf(TEXT("ce %s"),*Evt->ConsoleEventName.ToString());
					AutoCompleteList(NewIdx).Desc = FString::Printf(TEXT("ce %s (%s)"),*Evt->ConsoleEventName.ToString(),*Evt->EventDesc);
					KismetExecCnt++;
				}
			}
		}
		{
			TArray<USequenceObject*> RemoteEvts;
			BaseSeq->FindSeqObjectsByClass(USeqEvent_RemoteEvent::StaticClass(), RemoteEvts);
			for (INT Idx = 0; Idx < RemoteEvts.Num(); Idx++)
			{
				USeqEvent_RemoteEvent* Evt = Cast<USeqEvent_RemoteEvent>(RemoteEvts(Idx));
				if (Evt->bEnabled && Evt->EventName != NAME_None)
				{
					INT NewIdx = AutoCompleteList.AddZeroed(1);
					AutoCompleteList(NewIdx).Command = FString::Printf(TEXT("re %s"), *Evt->EventName.ToString());
					AutoCompleteList(NewIdx).Desc = FString::Printf(TEXT("re %s"), *Evt->EventName.ToString());
					KismetExecCnt++;
				}
			}
		}
	}
	// enumerate maps
	TArray<FString> Packages;
	appFindFilesInDirectory(Packages, *FString::Printf(TEXT("%sContent\\Maps"),*appGameDir()), TRUE, FALSE);
	for (INT PackageIndex = 0; PackageIndex < Packages.Num(); PackageIndex++)
	{
		FString Pkg = Packages(PackageIndex);
		INT ExtIdx = Pkg.InStr(*FURL::DefaultMapExt,TRUE);
		FString MapName;
		if (ExtIdx != INDEX_NONE && Pkg.Split(TEXT("\\"),NULL,&MapName,TRUE))
		{
			// try to peel off the extension
			FString TrimmedMapName;
			if (!MapName.Split(TEXT("."),&TrimmedMapName,NULL,TRUE))
			{
				TrimmedMapName = MapName;
			}
			INT NewIdx;
			// put _P maps at the front so that they match early, since those are generally the maps we want to actually open
			if (TrimmedMapName.EndsWith(TEXT("_P")))
			{
				NewIdx = 0;
				AutoCompleteList.InsertZeroed(0,2);
			}
			else
			{
				NewIdx = AutoCompleteList.AddZeroed(2);
			}
			AutoCompleteList(NewIdx).Command = FString::Printf(TEXT("open %s"),*TrimmedMapName);
			AutoCompleteList(NewIdx).Desc = FString::Printf(TEXT("open %s"),*TrimmedMapName);
			AutoCompleteList(NewIdx+1).Command = FString::Printf(TEXT("start %s"),*TrimmedMapName);
			AutoCompleteList(NewIdx+1).Desc = FString::Printf(TEXT("start %s"),*TrimmedMapName);
			//MapNames.AddItem(Pkg);
		}
	}
	// misc commands
	{
		INT NewIdx = AutoCompleteList.AddZeroed(1);
		AutoCompleteList(NewIdx).Command = FString(TEXT("open 127.0.0.1"));
		AutoCompleteList(NewIdx).Desc = FString(TEXT("open 127.0.0.1 (opens connection to localhost)"));
	}
	// build the magic tree!
	for (INT ListIdx = 0; ListIdx < AutoCompleteList.Num(); ListIdx++)
	{
		FString Command = AutoCompleteList(ListIdx).Command.ToLower();
		//debugf(TEXT("%d -> %s (%s)"),ListIdx,*Command,*AutoCompleteList(ListIdx).Desc);
		FAutoCompleteNode *Node = &AutoCompleteTree;
		for (INT Depth = 0; Depth < Command.Len(); Depth++)
		{
			INT Char = Command[Depth];
			INT FoundNodeIdx = INDEX_NONE;
			TArray<FAutoCompleteNode*> &NodeList = Node->ChildNodes;
			for (INT NodeIdx = 0; NodeIdx < NodeList.Num(); NodeIdx++)
			{
				if (NodeList(NodeIdx)->IndexChar == Char)
				{
					FoundNodeIdx = NodeIdx;
					Node = NodeList(FoundNodeIdx);
					NodeList(FoundNodeIdx)->AutoCompleteListIndices.AddItem(ListIdx);
					break;
				}
			}
			if (FoundNodeIdx == INDEX_NONE)
			{
				FAutoCompleteNode *NewNode = new FAutoCompleteNode(Char);
				NewNode->AutoCompleteListIndices.AddItem(ListIdx);
				Node->ChildNodes.AddItem(NewNode);
				Node = NewNode;
			}
		}
	}
	bIsRuntimeAutoCompleteUpToDate = TRUE;
	debugf(TEXT("Assembled %d auto-complete commands, manual: %d, exec: %d, kismet: %d"),AutoCompleteList.Num(),ManualAutoCompleteList.Num(),ScriptExecCnt,KismetExecCnt);
	//PrintNode(&AutoCompleteTree);
#endif
}

void UConsole::UpdateCompleteIndices()
{
	if (!bIsRuntimeAutoCompleteUpToDate)
	{
		BuildRuntimeAutoCompleteList(TRUE);
	}
	bNavigatingHistory = FALSE;
	AutoCompleteIndex = 0;
	AutoCompleteIndices.Empty();
	FAutoCompleteNode *Node = &AutoCompleteTree;
	FString LowerTypedStr = TypedStr.ToLower();
	for (INT Idx = 0; Idx < TypedStr.Len(); Idx++)
	{
		INT Char = LowerTypedStr[Idx];
		UBOOL bFoundMatch = FALSE;
		INT BranchCnt = 0;
		for (INT CharIdx = 0; CharIdx < Node->ChildNodes.Num(); CharIdx++)
		{
			BranchCnt += Node->ChildNodes(CharIdx)->ChildNodes.Num();
			if (Node->ChildNodes(CharIdx)->IndexChar == Char)
			{
				bFoundMatch = TRUE;
				Node = Node->ChildNodes(CharIdx);
				break;
			}
		}
		if (!bFoundMatch)
		{
			if (!bAutoCompleteLocked && BranchCnt > 0)
			{
				// we're off the grid!
				return;
			}
			else
			{
				break;
			}
		}
	}
	if (Node != &AutoCompleteTree)
	{
		AutoCompleteIndices = Node->AutoCompleteListIndices;
		/*
		debugf(TEXT("%s, %d"),*TypedStr,AutoCompleteIndices.Num());
		for (INT Idx = 0; Idx < AutoCompleteIndices.Num(); Idx++)
		{
			debugf(TEXT("- %d: %s"),AutoCompleteIndices(Idx),*AutoCompleteList(AutoCompleteIndices(Idx)));
		} 
		*/ 
	}
}

/**
 * Minimal initialization constructor.
 */
UInteraction::UInteraction()
{
	// Initialize script execution.
	InitExecution();
}

/**
 * Called when the interaction is added to the GlobalInteractions array
 */
void UInteraction::Init()
{
}

/* ==========================================================================================================
	UUIInteraction
========================================================================================================== */
/**
 * Initializes the singleton data store client that will manage the global data stores.
 */
void UUIInteraction::InitializeGlobalDataStore()
{
	if ( DataStoreManager == NULL )
	{
		// Explicitly set the transient package as the outer (UIControllers already indirectly created there)
		// otherwise perobjectconfig settings won't be found in anything other than DefaultEngine.ini
		DataStoreManager = CreateGlobalDataStoreClient(GetTransientPackage());
		DataStoreManager->InitializeDataStores();
	}
}

/**
 * Constructor
 */
UUIInteraction::UUIInteraction()
{
}

/**
 * Called when UIInteraction is added to the GameViewportClient's Interactions array
 */
void UUIInteraction::Init()
{
	Super::Init();

	// register this scene client to receive notifications when the viewport is resized
	check(GCallbackEvent);
	GCallbackEvent->Register(CALLBACK_ViewportResized, this);
	GCallbackEvent->Register(CALLBACK_PostLoadMap, this);

	// initialize the list of keys that can generate double-click events
#define DEFINE_KEY(Name, SupportedEvent) if ( SupportedEvent == SIE_MouseButton ) { SupportedDoubleClickKeys.AddItem(KEY_##Name); }
	#include "UnKeys.h"
#undef DEFINE_KEY

	// Initialize the UI Input Key Maps
	InitializeUIInputAliasNames();

	InitializeAxisInputEmulations();

	// create the global data store manager
	InitializeGlobalDataStore();

	UIManager = ConstructObject<UUIManager>(UIManagerClass, this, NAME_None, RF_Transient);

	SceneClient = ConstructObject<UGameUISceneClient>(SceneClientClass, this, NAME_None, RF_Transient);
	SceneClient->DataStoreManager = DataStoreManager;
	SceneClient->InitializeClient();
}

/**
 * Cleans up all objects created by this UIInteraction, including unrooting objects and unreferencing any other objects.
 * Called when the UI system is being closed down (such as when exiting PIE).
 */
void UUIInteraction::TearDownUI()
{
	// remove any helper objects that we've added to the root set so they can be GC'd once unreferenced
	if ( DataStoreManager != NULL )
	{
		DataStoreManager->RemoveFromRoot();
	}

	// now unreference any objects we created
	DataStoreManager = NULL;
	if ( GCallbackEvent != NULL )
	{
		// no longer receive notifications about anything
		GCallbackEvent->UnregisterAll(this);
	}
	SceneClient = NULL;
	// finally, remove ourselves from the root set
	RemoveFromRoot();
}

/**
 * Called to finish destroying the object.
 */
void UUIInteraction::FinishDestroy()
{
	if ( GCallbackEvent != NULL )
	{
		GCallbackEvent->UnregisterAll(this);
	}
	Super::FinishDestroy();
}


/* === FCallbackEventDevice interface === */
/**
 * Called for notifications that require no additional information.
 */
void UUIInteraction::Send( ECallbackEventType InType )
{
	if ( InType == CALLBACK_PostLoadMap && !GIsEditor )
	{
		debugf(NAME_DevUI, TEXT("Received map loaded notification.  Reinitializing widget input aliases."));

		if ( GFullScreenMovie != NULL )
		{
			AWorldInfo* WI = GWorld ? GWorld->GetWorldInfo() : NULL;
			const UBOOL bIsMenuLevel = WI ? WI->IsMenuLevel() : FALSE;
			const UBOOL bIsUIActive = FALSE;
			const UBOOL bTakeOverInput = bIsUIActive && bIsMenuLevel;
			GFullScreenMovie->GameThreadToggleInputProcessing(!bTakeOverInput);
		}

		if ( SceneClient != NULL && SceneClient->IsUIActive() )
		{
			// if we still have UI scenes open, they might need to update their cached viewport size after the next map loads if we are transitioning
			// from the front-end 
			SceneClient->bUpdateSceneViewportSizes = TRUE;
		}
	}
}

/**
 * Called when the viewport has been resized.
 */
void UUIInteraction::Send( ECallbackEventType InType, FViewport* InViewport, UINT InMessage)
{
	if ( InType == CALLBACK_ViewportResized )
	{
		if ( SceneClient != NULL )
		{
			SceneClient->NotifyViewportResized(InViewport);
		}
	}
}

/**
 * Returns the number of players currently active.
 */
INT UUIInteraction::GetPlayerCount()
{
	return GEngine->GamePlayers.Num();
}

/**
 * Retrieves the index (into the Engine.GamePlayers array) for the player which has the ControllerId specified
 *
 * @param	GamepadIndex	the gamepad index of the player to search for
 */
INT UUIInteraction::GetPlayerIndex( INT GamepadIndex )
{
	INT Result = INDEX_NONE;

	for ( INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
	{
		ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
		if ( Player != NULL && Player->ControllerId == GamepadIndex )
		{
			Result = PlayerIndex;
			break;
		}
	}

	return Result;
}

/**
 * Returns the index [into the Engine.GamePlayers array] for the player specified.
 *
 * @param	Player	the player to search for
 *
 * @return	the index of the player specified, or INDEX_NONE if the player is not in the game's list of active players.
 */
INT UUIInteraction::GetPlayerIndex( ULocalPlayer* Player )
{
	INT Result = INDEX_NONE;

	if ( Player != NULL && GEngine != NULL )
	{
		Result = GEngine->GamePlayers.FindItemIndex(Player);
	}

	return Result;
}

/**
 * Retrieves the ControllerId for the player specified.
 *
 * @param	PlayerIndex		the index [into the Engine.GamePlayers array] for the player to retrieve the ControllerId for
 *
 * @return	the ControllerId for the player at the specified index in the GamePlayers array, or INDEX_NONE if the index is invalid
 */
INT UUIInteraction::GetPlayerControllerId( INT PlayerIndex )
{
	INT Result = INDEX_NONE;

	if ( GEngine != NULL && GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		Result = GEngine->GamePlayers(PlayerIndex)->ControllerId;
	}

	return Result;
}

/**
 * Returns a reference to the global data store client, if it exists.
 *
 * @return	the global data store client for the game.
 */
UDataStoreClient* UUIInteraction::GetDataStoreClient()
{
	UDataStoreClient* Result = NULL;

	if ( GEngine != NULL && GEngine->GameViewport != NULL && GEngine->GameViewport->UIController != NULL )
	{
		Result = GEngine->GameViewport->UIController->DataStoreManager;
	}
	else
	{
		// dedicated server case
		UUIInteraction* CDO = UUIInteraction::StaticClass()->GetDefaultObject<UUIInteraction>();
		if ( CDO != NULL )
		{
			Result = CDO->DataStoreManager;
		}
	}

	return Result;
}


/**
 * Check a key event received by the viewport.
 *
 * @param	Viewport - The viewport which the key event is from.
 * @param	ControllerId - The controller which the key event is from.
 * @param	Key - The name of the key which an event occured for.
 * @param	Event - The type of event which occured.
 * @param	AmountDepressed - For analog keys, the depression percent.
 * @param	bGamepad - input came from gamepad (ie xbox controller)
 *
 * @return	True to consume the key event, false to pass it on.
 */
UBOOL UUIInteraction::InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed/*=1.f*/,UBOOL bGamepad/*=FALSE*/)
{
	SCOPE_CYCLE_COUNTER(STAT_UIProcessInput);

	UBOOL bResult = FALSE;

	const UBOOL bIsDoubleClickKey = SupportedDoubleClickKeys.ContainsItem(Key);
	if ( bProcessInput == TRUE && SceneClient != NULL )
	{
		if ( bIsDoubleClickKey )
		{
			DOUBLE CurrentTimeInSeconds = appSeconds();
			if ( Event == IE_Pressed )
			{
				if ( SceneClient->ShouldSimulateDoubleClick() )
				{
					Event = IE_DoubleClick;
				}

				// this is the first time we're sending a repeat event for this mouse button, so set the initial repeat delay
				MouseButtonRepeatInfo.NextRepeatTime = CurrentTimeInSeconds + MouseButtonRepeatDelay * 1.5f;
				MouseButtonRepeatInfo.CurrentRepeatKey = Key;
			}
			else if ( Event == IE_Repeat )
			{
				if ( MouseButtonRepeatInfo.CurrentRepeatKey == Key )
				{
					if ( CurrentTimeInSeconds < MouseButtonRepeatInfo.NextRepeatTime )
					{
						// this key hasn't been held long enough to generate the "repeat" keypress, so just swallow the input event
						bResult = TRUE;
					}
					else
					{
						// it's time to generate another key press; subsequence repeats should take a little less time than the initial one
						MouseButtonRepeatInfo.NextRepeatTime = CurrentTimeInSeconds + MouseButtonRepeatDelay * 0.5f;
					}
				}
				else
				{
					// this is the first time we're sending a repeat event for this mouse button, so set the initial repeat delay
					MouseButtonRepeatInfo.NextRepeatTime = CurrentTimeInSeconds + MouseButtonRepeatDelay * 1.5f;
					MouseButtonRepeatInfo.CurrentRepeatKey = Key;
					Event = IE_Pressed;
				}
			}
		}

		bResult = bResult || SceneClient->InputKey(ControllerId,Key,Event,AmountDepressed,bGamepad);

		if ( bIsDoubleClickKey && (Event == IE_Pressed || Event == IE_DoubleClick) )
		{
			SceneClient->ResetDoubleClickTracking(Event == IE_DoubleClick);
		}
	}

	if ( bIsDoubleClickKey && Event == IE_Repeat && !bResult )
	{
		// don't allow IE_Repeat events to be passed to the game for mouse buttons.
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Check an axis movement received by the viewport.
 *
 * @param	Viewport - The viewport which the axis movement is from.
 * @param	ControllerId - The controller which the axis movement is from.
 * @param	Key - The name of the axis which moved.
 * @param	Delta - The axis movement delta.
 * @param	DeltaTime - The time since the last axis update.
 *
 * @return	True to consume the axis movement, false to pass it on.
 */
UBOOL UUIInteraction::InputAxis(INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad)
{
	SCOPE_CYCLE_COUNTER(STAT_UIProcessInput);
	UBOOL bResult = FALSE;

	if ( bProcessInput == TRUE && SceneClient != NULL )
	{
		FUIAxisEmulationDefinition* EmulationDef = AxisEmulationDefinitions.Find(Key);
		const UBOOL bValidDelta = Abs<FLOAT>(Delta) >= UIJoystickDeadZone;
		const INT PlayerIndex = GetPlayerIndex(ControllerId);

		// If this axis input event was generated by an axis we can emulate, check that it is outside the UI's dead zone.
		if ( EmulationDef != NULL && EmulationDef->bEmulateButtonPress )
		{
			if ( PlayerIndex >= 0 && PlayerIndex < ARRAY_COUNT(AxisInputEmulation) && AxisInputEmulation[PlayerIndex].bEnabled )
			{
				FInputEventParameters EmulatedEventParms(PlayerIndex, ControllerId, EmulationDef->InputKeyToEmulate[Delta > 0 ? 0 : 1], IE_MAX,
					IsAltDown(SceneClient->RenderViewport), IsCtrlDown(SceneClient->RenderViewport), IsShiftDown(SceneClient->RenderViewport), 1.f);

				// if the current delta is within the dead-zone, and this key is set as the CurrentRepeatKey for that gamepad,
				// generate a "release" event
				if ( bValidDelta == FALSE )
				{
					// if this key was the key that was being held down, simulate the release event
					// Only signal a release if this is the last key pressed
					if ( AxisInputEmulation[PlayerIndex].CurrentRepeatKey == Key )
					{
						// change the event type to "release"
						EmulatedEventParms.EventType = IE_Released;

						// and clear the emulated repeat key for this player
						AxisInputEmulation[PlayerIndex].CurrentRepeatKey = NAME_None;
					}
					else
					{
						// otherwise, ignore it - if we're in this block, we have a scene open which processes axis input
						return TRUE;
					}
				}

				// we have a valid delta for this axis; need to determine what to do with it
				else
				{
					// if this is the same key as the current repeat key, it means the user is still holding the joystick in the same direction
					// so we'll need to determine whether enough time has passed to generate another button press event
					if ( AxisInputEmulation[PlayerIndex].CurrentRepeatKey == Key )
					{
						// we might need to simulate another "repeat" event
						EmulatedEventParms.EventType = IE_Repeat;
					}

					else
					{
						// if the new key isn't the same as the current repeat key, but the new key is another axis input, ignore it
						// this basically means that as long as we have a valid delta on one joystick axis, we're going to ignore all other joysticks for that player
						if ( AxisInputEmulation[PlayerIndex].CurrentRepeatKey != NAME_None && Key != EmulationDef->AdjacentAxisInputKey )
						{
							bResult = SceneClient->bCaptureUnprocessedInput;
						}
						else
						{
							EmulatedEventParms.EventType = IE_Pressed;
							AxisInputEmulation[PlayerIndex].CurrentRepeatKey = Key;
						}
					}
				}

				DOUBLE CurrentTimeInSeconds = appSeconds();
				if ( EmulatedEventParms.EventType == IE_Repeat )
				{
					// this key hasn't been held long enough to generate the "repeat" keypress, so just swallow the input event
					if ( CurrentTimeInSeconds < AxisInputEmulation[PlayerIndex].NextRepeatTime )
					{
						EmulatedEventParms.EventType = IE_MAX;
						bResult = TRUE;
					}
					else
					{
						// it's time to generate another key press; subsequence repeats should take a little less time than the initial one
						AxisInputEmulation[PlayerIndex].NextRepeatTime = CurrentTimeInSeconds + AxisRepeatDelay * 0.5f;
					}
				}
				else if ( EmulatedEventParms.EventType == IE_Pressed )
				{
					// this is the first time we're sending a keypress event for this axis's emulated button, so set the initial repeat delay
					AxisInputEmulation[PlayerIndex].NextRepeatTime = CurrentTimeInSeconds + AxisRepeatDelay * 1.5f;
				}

				// we're supposed to generate the emulated button input key event if the EmulatedEventParms.EventType is not IE_MAX
				if ( EmulatedEventParms.EventType != IE_MAX )
				{
					bResult = SceneClient->InputKey(ControllerId, EmulatedEventParms.InputKeyName, (EInputEvent)EmulatedEventParms.EventType, 1.f, bGamepad);
				}
			}
		}

		// when a new player is added, the bEnabled value of the AxisInputEmulation element for that player won't be accurate until the
		// next scene update - so in order for the UI to receive the axis input that is not outside the dead-zone, we have to wait until
		// the scene client is not waiting to update input processing status
		// likewise, if PlayerIndex isn't valid, it means this input is coming from a gamepad that isn't associated with a player, so don't
		// allow this input to be sent to the UI unless the user definitely intended to send the input (i.e. outside the dead-zone) or
		// the AxisInputEmulation for all players is up to date.
		if ( !bResult && (PlayerIndex != INDEX_NONE && (bValidDelta || !SceneClient->bUpdateInputProcessingStatus)) )
		{
			bResult = SceneClient->InputAxis(ControllerId, Key, Delta, DeltaTime, bGamepad);
		}
	}

	return bResult;
}

/**
 * Check a character input received by the viewport.
 *
 * @param	Viewport - The viewport which the axis movement is from.
 * @param	ControllerId - The controller which the axis movement is from.
 * @param	Character - The character.
 *
 * @return	True to consume the character, false to pass it on.
 */
UBOOL UUIInteraction::InputChar(INT ControllerId,TCHAR Character)
{
	SCOPE_CYCLE_COUNTER(STAT_UIProcessInput);
	UBOOL bResult = FALSE;

	if ( bProcessInput && SceneClient != NULL )
	{
		bResult = SceneClient->InputChar(ControllerId,Character);
	}

	return bResult;
}

/**
 * Initializes the axis button-press/release emulation map.
 */
void UUIInteraction::InitializeAxisInputEmulations()
{
	const TArray<FUIAxisEmulationDefinition>& EmulationDefinitions = ConfiguredAxisEmulationDefinitions;

	AxisEmulationDefinitions.Empty();
	for ( INT KeyIndex = 0; KeyIndex < EmulationDefinitions.Num(); KeyIndex++ )
	{
		const FUIAxisEmulationDefinition& Definition = EmulationDefinitions(KeyIndex);
		AxisEmulationDefinitions.Set(Definition.AxisInputKey, Definition);
	}
}

/**
 * Initializes all of the UI Input Key FNames.
 */
void UUIInteraction::InitializeUIInputAliasNames()
{
	#define DEFINE_UIKEY(Name) UIKEY_##Name = FName(TEXT(#Name));
		#include "UnUIKeys.h"
	#undef DEFINE_UIKEY
}

/**
 * Called once a frame to update the interaction's state.
 *
 * @param	DeltaTime - The time since the last frame.
 */
void UUIInteraction::Tick( FLOAT DeltaTime )
{
	// Only tick if enabled.
	extern UBOOL GTickAndRenderUI;
	if( GTickAndRenderUI )
	{
		SCOPE_CYCLE_COUNTER(STAT_UITickTime);

		Super::Tick(DeltaTime);

		SceneClient->Tick(DeltaTime);
	}
}

/**
 * Returns the CDO for the configured scene client class.
 */
UGameUISceneClient* UUIInteraction::GetDefaultSceneClient() const
{
	check(SceneClientClass);

	return SceneClientClass->GetDefaultObject<UGameUISceneClient>();
}

/**
 * Handles processing console commands.
 *
 * @param	Cmd		the text that was entered into the console
 * @param	Ar		the archive to use for serializing responses
 *
 * @return	TRUE if the command specified was processed
 */
UBOOL UUIInteraction::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( ScriptConsoleExec(Cmd, Ar, this) )
	{
		return TRUE;
	}
	else if ( SceneClient->Exec(Cmd, Ar) )
	{
		return TRUE;
	}

	return FALSE;
}


