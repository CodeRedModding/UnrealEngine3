/**
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "GameFramework.h"
#include "OpCode.h"


IMPLEMENT_CLASS(AGameAIController);
IMPLEMENT_CLASS(UGameAICommand);

IMPLEMENT_CLASS(UNavMeshPath_BiasAgainstPolysWithinDistanceOfLocations);
IMPLEMENT_CLASS(UNavMeshGoal_OutOfViewFrom);

/**
* Overridden to provide normal state ticking for AICommand
* 
* @note - AICommand abuses the stateframe of the owning AI in order to provide "within" latent function support
*         such that MoveToward() can be called from within a state for AICommand, but still operate correctly for
*         the AIController.
*/
void UGameAICommand::ProcessState(FLOAT DeltaSeconds)
{
	AGameAIController *AI = GetOuterAGameAIController();
	if( GetStateFrame() &&	
		GetStateFrame()->Code &&	
		!IsPendingKill() &&	
		AI != NULL &&	
		AI->Pawn != NULL &&
		!AI->ActorIsPendingKill()&&	
		AI->GetStateFrame() && 
		!bAborted )
	{
		//debug
		//debugf(TEXT("%s ProcessState %s %d"), *GetName(), *AI->GetName(), AI->GetStateFrame()->LatentAction );
		// If a latent action is in progress, update it.
		if (AI->GetStateFrame()->LatentAction != 0)
		{
			(AI->*GNatives[AI->GetStateFrame()->LatentAction])(*GetStateFrame(), (BYTE*)&DeltaSeconds);
		}

		if (AI->GetStateFrame()->LatentAction == 0)
		{
			//debug
			//			debugf(TEXT("%s PROCESSSTATE - EXECUTE STATE CODE"), *GetName() );


			// Execute code.
			INT NumStates = 0;
			DWORD Buffer[MAX_SIMPLE_RETURN_VALUE_SIZE_IN_DWORDS];
			// create a copy of the state frame to execute state code from so that if the state is changed from within the code, the executing frame's code pointer isn't modified while it's being used
			FStateFrame ExecStateFrame(*GetStateFrame());
			while(  !IsPendingKill() && 
				ExecStateFrame.Code != NULL && 
				AI != NULL && 
				AI->Pawn != NULL &&
				!AI->ActorIsPendingKill() && 
				AI->GetStateFrame()->LatentAction == 0 &&
				AI->GetActiveCommand() == this && // If we pushed a new command, we need to be proccessing the new one instead of this one
				!bAborted  // Stop processing state once this command is aborted
				) 
			{
				// if we are continuing interrupted state code, we need to manually push the frame onto the script debugger's stack
				if (GetStateFrame()->bContinuedState)
				{
#if !FINAL_RELEASE
					if (GDebugger != NULL)
					{
						GDebugger->DebugInfo(this, &ExecStateFrame, DI_NewStack, 0, 0);
					}
#endif
					GetStateFrame()->bContinuedState = FALSE;
				}

				// remember old starting point (+1 for the about-to-be-executed byte so we can detect a state/label jump back to the same byte we're at now)
				BYTE* OldCode = ++GetStateFrame()->Code;

				ExecStateFrame.Step( this, Buffer ); 

				//debug
				//				debugf(TEXT("%s has command changed? %s %s"), *GetName(), AI->GetActiveCommand()?*AI->GetActiveCommand()->GetName():TEXT("NULL"), (this!=AI->GetActiveCommand())?TEXT("!!!!!!!!!!!!!!!!!"):TEXT(""));

				// if a state was pushed onto the stack, we need to correct the originally executing state's code pointer to reflect the code *after* the last state command was executed
				if (GetStateFrame()->StateStack.Num() > ExecStateFrame.StateStack.Num())
				{
					GetStateFrame()->StateStack(ExecStateFrame.StateStack.Num()).Code = ExecStateFrame.Code;
				}
				// if the state frame's code pointer was directly modified by a state or label change, we need to update our copy
				if (GetStateFrame()->Node != ExecStateFrame.Node)
				{
					// we have changed states
					if( ++NumStates > 4 )
					{
						//debugf(TEXT("%s pause going from state %s to %s"), *ExecStateFrame.StateNode->GetName(), *GetStateFrame()->StateNode->GetName());
						// shouldn't do any copying as the StateFrame was modified for the new state/label
						break;
					}
					else
					{
						//debugf(TEXT("%s went from state %s to %s"), *GetName(), *ExecStateFrame.StateNode->GetName(), *GetStateFrame()->StateNode->GetName());
						ExecStateFrame = *GetStateFrame();
					}
				}
				else if (GetStateFrame()->Code != OldCode)
				{
					// transitioned to a new label
					//debugf(TEXT("%s went to new label in state %s"), *GetName(), *GetStateFrame()->StateNode->GetName());
					ExecStateFrame = *GetStateFrame();
				}
				else
				{
					// otherwise, copy the new code pointer back to the original state frame
					GetStateFrame()->Code = ExecStateFrame.Code;
				}
			}

#if !FINAL_RELEASE
			// notify the debugger if state code ended prematurely due to this Actor being destroyed
			if ((AI->ActorIsPendingKill() || IsPendingKill()) && GDebugger != NULL)
			{
				GDebugger->DebugInfo(this, &ExecStateFrame, DI_PrevStackState, 0, 0);
			}
#endif
		}
	}
}

void UGameAICommand::TickCommand(FLOAT DeltaTime)
{
	//debug
	//	debugf(TEXT("%s TickCommand %s"), *GetName(), ChildCommand?*ChildCommand->GetName():TEXT("NULL"));

	// pass tick through to child
	if( ChildCommand != NULL )
	{
		ChildCommand->TickCommand(DeltaTime);
	}
	if( ChildCommand == NULL )
	{
		eventInternalTick( DeltaTime );
		// by default only tick if there is no active child
		ProcessState( DeltaTime );
	}
}

void UGameAICommand::PopChildCommand()
{
	//debug
	AI_LOG(GameAIOwner,( NAME_AICommand, TEXT("%s PopChildCommand: %s"), *GetName(), ChildCommand?*ChildCommand->GetName():TEXT("NULL")));

	if ( ChildCommand != NULL )
	{
		FName ChildName = ChildCommand->GetClass()->GetFName();

		// recursively pop any children
		ChildCommand->bPendingPop = TRUE;

		// if the command we're about to pop has children, throw a warning because it probably should be aborted
#if DO_AI_LOGGING
		if(ChildCommand->ChildCommand != NULL && !ChildCommand->bAborted)
		{
			AI_LOG(GameAIOwner,( NAME_AICommand, TEXT("WARNING!! I'm about to pop %s but it has children!  You should use abort instead of pop for this case!!"),*ChildCommand->ChildCommand->GetName()));
			if(RUNTIME_DO_AI_LOGGING) debugf(NAME_Warning,TEXT("WARNING!! [%2.3f] (%s) %s is about to pop command %s but it has children!  You should use abort instead of pop for this case!!"),GWorld->GetTimeSeconds(),*GameAIOwner->GetName(),*GetName(),*ChildCommand->ChildCommand->GetName());
		}
#endif
		ChildCommand->PopChildCommand();

		// need to check ChildCommand again because it could have popped itself on resume
		if (ChildCommand != NULL)
		{
			// notify the child is being popped
			ChildCommand->eventInternalPopped();
			// and again
			if (ChildCommand != NULL)
			{
				// clear out refs and record the exit status
				ChildStatus  = ChildCommand->Status;
				if (GDebugger != NULL && ChildCommand->GetStateFrame() != NULL)
				{
					GDebugger->DebugInfo(this, ChildCommand->GetStateFrame(), DI_PrevStackState, 0, 0);
				}
				GetStateFrame()->bContinuedState = TRUE;
				ChildCommand->SetFlags(RF_PendingKill);
				ChildCommand = NULL;

				if( Status != NAME_Aborted  && !bPendingPop)
				{
					//debugf(TEXT("RESUMED on %s ChildStatus: %s %2.3f"), *GetName(), *ChildStatus.ToString(),GWorld->GetTimeSeconds());
					AI_LOG(GameAIOwner,( NAME_AICommand, TEXT("RESUMED on %s ChildStatus: %s"), *GetName(), *ChildStatus.ToString()));
					// send notification to this command
					eventInternalResumed( ChildName );
				}
			}
		}
	}
}

UBOOL UGameAICommand::ShouldIgnoreNotifies() const
{
	if(ChildCommand != NULL)
	{
		return ChildCommand->ShouldIgnoreNotifies();
	}

	return bIgnoreNotifies;
}

/** Overridden to clear the AI latent action on any state transition. */
EGotoState UGameAICommand::GotoState( FName State, UBOOL bForceEvents, UBOOL bKeepStack )
{
	AGameAIController *AI = GetOuterAGameAIController();
	if (AI != NULL && AI->GetStateFrame() != NULL)
	{
		AI->GetStateFrame()->LatentAction = 0;
	}
	return Super::GotoState(State,bForceEvents,bKeepStack);
}


/** 
* Overridden to prevent state transitions when operating AICommands, since any state transition for the base controller
* would break the "within" state functionality for the current command.
*/
EGotoState AGameAIController::GotoState( FName State, UBOOL bForceEvents, UBOOL bKeepStack )
{
	if (CommandList != NULL)
	{
		return GOTOSTATE_Preempted;
	}
	else
	{
		return Super::GotoState(State,bForceEvents,bKeepStack);
	}
}

/** 
*	Copy of AActor::ProcessState that stops state processing when a AICommand is pushed onto the stack 
*	Didn't want to add a virtual function right now because UT guys complained that they are slow on the PS3
*/
void AGameAIController::ProcessState( FLOAT DeltaSeconds )
{
	//debug
	//	debugf(TEXT("%s ProcessState %s"), *GetName(), CommandList?*CommandList->GetName():TEXT("NULL") );

	if(	GetStateFrame() && 
		GetStateFrame()->Code && 
		(Role>=ROLE_Authority || 
		(GetStateFrame()->StateNode->StateFlags & STATE_Simulated)) &&
		!IsPendingKill() &&
		CommandList == NULL )
	{
		if (GetStateFrame()->LatentAction != 0)
		{
			(this->*GNatives[GetStateFrame()->LatentAction])(*GetStateFrame(), (BYTE*)&DeltaSeconds);
		}

		if (GetStateFrame()->LatentAction == 0)
		{
			// Execute code.
			INT NumStates = 0;
			DWORD Buffer[MAX_SIMPLE_RETURN_VALUE_SIZE_IN_DWORDS];
			// create a copy of the state frame to execute state code from so that if the state is changed from within the code, the executing frame's code pointer isn't modified while it's being used
			FStateFrame ExecStateFrame(*GetStateFrame());
			while( !bDeleteMe && 
				ExecStateFrame.Code != NULL && 
				GetStateFrame()->LatentAction == 0 &&
				CommandList == NULL )	// Stop executing state code once an AICommand is pushed
			{
				// if we are continuing interrupted state code, we need to manually push the frame onto the script debugger's stack
				if (GetStateFrame()->bContinuedState)
				{
#if !FINAL_RELEASE
					if (GDebugger != NULL)
					{
						GDebugger->DebugInfo(this, &ExecStateFrame, DI_NewStack, 0, 0);
					}
#endif
					GetStateFrame()->bContinuedState = FALSE;
				}

				// remember old starting point (+1 for the about-to-be-executed byte so we can detect a state/label jump back to the same byte we're at now)
				BYTE* OldCode = ++GetStateFrame()->Code;

				ExecStateFrame.Step( this, Buffer ); 
				// if a state was pushed onto the stack, we need to correct the originally executing state's code pointer to reflect the code *after* the last state command was executed
				if (GetStateFrame()->StateStack.Num() > ExecStateFrame.StateStack.Num())
				{
					GetStateFrame()->StateStack(ExecStateFrame.StateStack.Num()).Code = ExecStateFrame.Code;
				}
				// if the state frame's code pointer was directly modified by a state or label change, we need to update our copy
				if (GetStateFrame()->Node != ExecStateFrame.Node)
				{
					// we have changed states
					if( ++NumStates > 4 )
					{
						//debugf(TEXT("%s pause going from state %s to %s"), *ExecStateFrame.StateNode->GetName(), *GetStateFrame()->StateNode->GetName());
						// shouldn't do any copying as the StateFrame was modified for the new state/label
						break;
					}
					else
					{
						//debugf(TEXT("%s went from state %s to %s"), *GetName(), *ExecStateFrame.StateNode->GetName(), *GetStateFrame()->StateNode->GetName());
						ExecStateFrame = *GetStateFrame();
					}
				}
				else if (GetStateFrame()->Code != OldCode)
				{
					// transitioned to a new label
					//debugf(TEXT("%s went to new label in state %s"), *GetName(), *GetStateFrame()->StateNode->GetName());
					ExecStateFrame = *GetStateFrame();
				}
				else
				{
					// otherwise, copy the new code pointer back to the original state frame
					GetStateFrame()->Code = ExecStateFrame.Code;
				}
			}

#if !FINAL_RELEASE
			// notify the debugger if state code ended prematurely due to this Actor being destroyed
			if (bDeleteMe && GDebugger != NULL)
			{
				GDebugger->DebugInfo(this, &ExecStateFrame, DI_PrevStackState, 0, 0);
			}
#endif
		}
	}
}

void AGameAIController::PushCommand( UGameAICommand *NewCommand )
{
	if( NewCommand != NULL )
	{
		//debug
		AI_LOG(this,( NAME_AICommand, TEXT("PushCommand: %s"), *NewCommand->GetName() ));

		// if the active command has the same class as the one we're pushing, give it special care 		
		UGameAICommand* ActiveCommand = GetActiveCommand();
		if( ActiveCommand != NULL && ActiveCommand->GetClass() == NewCommand->GetClass() )
		{
			// check bReplaceActiveSameClassInstance first, it trumps bAllowNewSameClassInstance
			if(NewCommand->bReplaceActiveSameClassInstance)
			{
				AbortCommand(ActiveCommand);
				//AILog( NAME_AICommand, TEXT("PushCommand ABORTING %s because Incoming command %s has same class, bReplaceActiveSameClassInstance:%i bAllowNewSameClassInstance:%i"), *ActiveCommand->GetName(), *NewCommand->GetName(), NewCommand->bReplaceActiveSameClassInstance, NewCommand->bAllowNewSameClassInstance);
			}
			else if(!NewCommand->bAllowNewSameClassInstance)
			{
				AI_LOG(this,( NAME_AICommand, TEXT("PushCommand IGNORED for : %s because ActiveCommand %s has same class, bReplaceActiveSameClassInstance:%i bAllowNewSameClassInstance:%i"), *NewCommand->GetName(), *ActiveCommand->GetName(), NewCommand->bReplaceActiveSameClassInstance, NewCommand->bAllowNewSameClassInstance));
				return;
			}

		}

		NewCommand->eventInternalPrePushed(this);

		if( CommandList == NULL )
		{
#if !FINAL_RELEASE
			if (GDebugger != NULL && GetStateFrame() != NULL)
			{
				// manually pop the debugger stack node for this state...we'll restore it later
				GDebugger->DebugInfo(this, GetStateFrame(), DI_PrevStackState, 0, 0);
			}
#endif
			CommandList = NewCommand;
		}
		else
		{
			UGameAICommand *LastCommand = GetActiveCommand();		
#if !FINAL_RELEASE
			if (GDebugger != NULL && LastCommand->GetStateFrame() != NULL)
			{
				// manually pop the debugger stack node for this state...we'll restore it later
				GDebugger->DebugInfo(LastCommand, LastCommand->GetStateFrame(), DI_PrevStackState, 0, 0);
			}
#endif
			LastCommand->ChildCommand = NewCommand;
			// notify the current command it is being paused
			LastCommand->eventInternalPaused( NewCommand );
		}

		//debug
		//DumpCommandStack();

		// clear any latent action currently being used by the last command on the stack
		GetStateFrame()->LatentAction = 0;
		// initial the command
		NewCommand->InitExecution();
		// and notify the command
		NewCommand->eventInternalPushed();

#if !FINAL_RELEASE
		StoreCommandHistory(NewCommand);
#endif
	}
}

void AGameAIController::StoreCommandHistory(UGameAICommand* Cmd)
{
#if !FINAL_RELEASE
	if (CommandHistoryNum > 0)
	{
		FAICmdHistoryItem Item(EC_EventParm);
		Item.CmdClass = Cmd->GetClass();
		Item.TimeStamp = WorldInfo->TimeSeconds;
		Item.VerboseString = Cmd->eventGetDebugVerboseText();
		CommandHistory.InsertItem(Item,0);

		if (CommandHistory.Num() >= CommandHistoryNum)
		{
			CommandHistory.Remove(CommandHistoryNum - 1, CommandHistory.Num() - CommandHistoryNum);
		}
	}
#endif
}

void AGameAIController::PopCommand( UGameAICommand *NewCommand )
{
	//debugf(TEXT("%s pop command; Child: %s"),*NewCommand->GetName(),(NewCommand->ChildCommand) ? *NewCommand->ChildCommand->GetName() : TEXT("NONE"));
	if( NewCommand != NULL )
	{
		//debug
		AI_LOG(this,( NAME_AICommand, TEXT("PopCommand: %s -- Status %s"), *NewCommand->GetName(),*NewCommand->Status.ToString() ));

		// if it is the head of the command list
		if( NewCommand == CommandList )
		{
			// Force it to pop children
			NewCommand->PopChildCommand();
			// Notify the child is being popped
			NewCommand->eventInternalPopped();
			if (GDebugger != NULL && NewCommand->GetStateFrame() != NULL)
			{
				GDebugger->DebugInfo(this, NewCommand->GetStateFrame(), DI_PrevStackState, 0, 0);
			}
			GetStateFrame()->bContinuedState = TRUE;
			NewCommand->SetFlags(RF_PendingKill);
			// Then just clear the ref
			CommandList = NULL;
		}
		else
		{
			// otherwise find the parent command
			UGameAICommand *ParentCommand = CommandList;
			while (ParentCommand != NULL && ParentCommand->ChildCommand != NewCommand)
			{
				ParentCommand = ParentCommand->ChildCommand;
			}
			if (ParentCommand != NULL)
			{
				// and tell it to begin popping children
				ParentCommand->PopChildCommand();
			}
		}

		//debug
		//DumpCommandStack();

		// ensure we don't leave a latent action lying around when this command is popped
		GetStateFrame()->LatentAction = 0;
	}
}

UBOOL AGameAIController::AbortCommand( UGameAICommand* AbortCmd, UClass* AbortClass )
{
	UBOOL bResult = FALSE;

	//debug
	AI_LOG(this,(TEXT("AbortCommand AbortCmd: %s  AbortClass: %s"), *AbortCmd->GetName(), *AbortClass->GetName() ));
	//	DumpCommandStack();

	UGameAICommand* Cmd = CommandList;
	while( Cmd != NULL )
	{
		// If command isn't already aborted and it is a class we want to abort
		if( !Cmd->bAborted && 
			((AbortCmd != NULL && Cmd == AbortCmd) || 
			(AbortClass != NULL && Cmd->GetClass()->IsChildOf( AbortClass ))) )
		{
			//debug
			AI_LOG(this,(TEXT("ABORTING... %s"), *Cmd->eventGetDumpString() ));

			// Set aborted status/flag
			Cmd->Status		= NAME_Aborted;
			Cmd->bAborted	= TRUE;

			// Abort all the children too
			UGameAICommand* ChildCmd = Cmd->ChildCommand;
			while( ChildCmd != NULL )
			{
				ChildCmd->Status	= NAME_Aborted;
				ChildCmd->bAborted	= TRUE;

				ChildCmd = ChildCmd->ChildCommand;
			}

			bResult = TRUE;
		}

		Cmd = Cmd->ChildCommand;
	}

	if( bResult )
	{
		// Handle any aborted commands
		Cmd = CommandList;
		while( Cmd != NULL )
		{
			// If this command was aborted, pop the command
			if( Cmd->bAborted )
			{
				PopCommand( Cmd );
				Cmd = CommandList;	// Start looking from the beginning of the list again
			}
			else
			{
				Cmd = Cmd->ChildCommand;
			}
		}
	}

	return bResult;
}

UGameAICommand* AGameAIController::GetActiveCommand()
{
	if( CommandList == NULL )
	{
		return NULL;
	}

	UGameAICommand* Cmd = CommandList;
	while( Cmd->ChildCommand )
	{
		Cmd = Cmd->ChildCommand;
	}

	return Cmd;
}


void AGameAIController::execAllCommands(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UClass, BaseClass);
	P_GET_OBJECT_REF(UGameAICommand, OutCmd);
	P_FINISH;

	UGameAICommand* CurrentCmd = CommandList;

	// if we have a valid subclass
	if( BaseClass && BaseClass != AController::StaticClass() )
	{
		PRE_ITERATOR;
		// get the next Controller in the iteration
		OutCmd = NULL;
		while (CurrentCmd && OutCmd == NULL)
		{
			if (CurrentCmd->IsA(BaseClass))
			{
				OutCmd = CurrentCmd;
			}
			CurrentCmd = CurrentCmd->ChildCommand;
		}
		if (OutCmd == NULL)
		{
			EXIT_ITERATOR;
			break;
		}
		POST_ITERATOR;
	}
	else
	{
		// do a faster iteration that doesn't check IsA()
		PRE_ITERATOR;
		// get the next Controller in the iteration
		if (CurrentCmd)
		{
			OutCmd = CurrentCmd;
			CurrentCmd = CurrentCmd->ChildCommand;
		}
		else
		{
			// we're out of Commands
			OutCmd = NULL;
			EXIT_ITERATOR;
			break;
		}
		POST_ITERATOR;
	}
}


void AGameAIController::CheckCommandCount()
{
	INT Cnt = 0;
	UGameAICommand* Cmd = CommandList;
	while( Cmd )
	{
		Cnt++;
		if (Cnt >= 50)
		{
			warnf(TEXT("Runaway Loop in AICommand list detected (more than 50 commands)... %s"), *GetName());
			DumpCommandStack();
			if( AILogFile != NULL && AILogFile->ArchivePtr != NULL )
			{
				AILogFile->ArchivePtr->Flush();
			}

			bHasRunawayCommandList = TRUE;
			// this appErrorf insures that any AI errors are immediately caught and reported.  We don't do this in Shipping nor if we are not ai logging as all of the pertinent info is outputted to the AILogs
#if !FINAL_RELEASE && DO_AI_LOGGING
//			appErrorf(TEXT("Runaway Loop in AICommand list detected (more than 50 commands)... %s"), *GetName());
#endif
			break;
		}
		Cmd = Cmd->ChildCommand;
	}
}

void AGameAIController::DumpCommandStack()
{
	AI_LOG(this,(TEXT("DUMP COMMAND STACK: %s"), *Pawn->GetName()));
	if( CommandList == NULL )
	{
		AI_LOG(this,(TEXT("\t\t [Empty]")));
	}
	else
	{
		UGameAICommand* Cmd = CommandList;
		INT Count = 0;
		while (Cmd)
		{
			if (++Count > 50)
			{
				AI_LOG(this,(TEXT("\t\t [Truncated due to stack depth]")));
				break;
			}
			AI_LOG(this,(TEXT("\t\t %s"), *Cmd->eventGetDumpString()));
			Cmd = Cmd->ChildCommand;
		}
	}
}


UGameAICommand* AGameAIController::FindCommandOfClass( UClass* SearchClass ) const
{
	if (SearchClass != NULL)
	{
		UGameAICommand* CurrCommand = CommandList;
		while (CurrCommand != NULL && !CurrCommand->IsA(SearchClass))
		{
			CurrCommand = CurrCommand->ChildCommand;
		}
		return CurrCommand;
	}
	else
	{
		return NULL;
	}
}


UBOOL AGameAIController::Tick(FLOAT DeltaTime, ELevelTick TickType)
{
	if (!WorldInfo->bPlayersOnly)
	{
		//debug
		//		debugf(TEXT("%s Tick -- %s"), *GetName(), CommandList?*CommandList->GetName():TEXT("NULL"));

		// tick the active AICommand if available
		if( CommandList != NULL )
		{
			CheckCommandCount();

			CommandList->TickCommand(DeltaTime);

			// so we have a run away commandlist.  So we will Abort it and then try try again
			if( bHasRunawayCommandList == TRUE )
			{
				AbortCommand( CommandList );  // abort his command he'll pop his entire command stack abort everythign he's doing and go back to default behavior might cause some weirdness but usually will be ok
				bHasRunawayCommandList = FALSE;
			}
		}
	}
	return Super::Tick(DeltaTime,TickType);
}

#if DO_AI_LOGGING
VARARG_BODY(void,AGameAIController::AILog,const TCHAR*,VARARG_NONE)
{
	// We need to use malloc here directly as GMalloc might not be safe.	
	if( !FName::SafeSuppressed(NAME_AILog) )
	{
		INT		BufferSize	= 1024;
		TCHAR*	Buffer		= NULL;
		INT		Result		= -1;

		while(Result == -1)
		{
			appSystemFree(Buffer);
			Buffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
			BufferSize *= 2;
		};
		Buffer[Result] = 0;

		eventAILog_Internal(Buffer);
		AI_PROFILER_AI_LOG( this, GetActiveCommand(), Buffer, NAME_None );

		appSystemFree( Buffer );
	}
}
VARARG_BODY(void,AGameAIController::AILog,const TCHAR*,VARARG_EXTRA(enum EName E))
{
	if( !FName::SafeSuppressed(NAME_AILog) )
	{
		INT		BufferSize	= 1024;
		TCHAR*	Buffer		= NULL;
		INT		Result		= -1;

		while(Result == -1)
		{
			appSystemFree(Buffer);
			Buffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
			BufferSize *= 2;
		};
		Buffer[Result] = 0;

		eventAILog_Internal(Buffer,FName(E));
		AI_PROFILER_AI_LOG( this, GetActiveCommand(), Buffer, FName(E) );

		appSystemFree( Buffer );

		
	}
}
#endif


UGameAICommand* AGameAIController::GetAICommandInStack( const UClass* InClass )
{
	UGameAICommand* Retval = NULL;

	if( CommandList == NULL )
	{
		return Retval;
	}

	UGameAICommand* Cmd = CommandList;
	while( Cmd != NULL )
	{
		//warnf( TEXT("GetAICommandInStack: %s %s"), *Cmd->GetClass()->GetName(), *InClass->GetClass()->GetName() );

		if( Cmd->GetClass() == InClass )
		{
			//warnf( TEXT( "Found!!!!!! %s"),  *Cmd->GetClass()->GetName() );
			Retval = Cmd;
			break;
		}

		Cmd = Cmd->ChildCommand;
	}

	return Retval;
}

UBOOL UNavMeshPath_BiasAgainstPolysWithinDistanceOfLocations::EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint )
{

	// go over the list of places we have spawned before and then if we are within the DistanceToCheck of them
	// add some heuristic cost to the path search
	UBOOL bWithinDistanceToCheck = FALSE;

	const FVector SrcPolyLoc = SrcPoly->GetPolyCenter();

	// do the check here
	for( INT LocIdx = 0; LocIdx < LocationsToCheck.Num(); ++LocIdx )
	{
		if( ( SrcPolyLoc - LocationsToCheck(LocIdx) ).Size() < DistanceToCheck )
		{
			bWithinDistanceToCheck = TRUE;
			break;
		}
	}


	if( bWithinDistanceToCheck == TRUE )
	{
		out_HeuristicCost += 512;
	}

	return TRUE;
}

UBOOL UNavMeshGoal_OutOfViewFrom::InitializeSearch( UNavigationHandle* Handle, const FNavMeshPathParams& PathParams )
{
	return Super::InitializeSearch( Handle, PathParams );
}

UBOOL UNavMeshGoal_OutOfViewFrom::EvaluateGoal( PathCardinalType PossibleGoal, const FNavMeshPathParams& PathParams, PathCardinalType& out_GenGoal )
{

	const FVector PolyCtr = PossibleGoal->GetPathDestinationPoly()->GetPolyCenter();

	// do line check
	FCheckResult Hit(1.f);

	// simulate eye sight here
	const FVector StartLocation = OutOfViewLocation + FVector(0,0,100);
	const FVector EndLocation = PolyCtr + FVector(0,0,176);

	// we need a bounding box check here or you get guys appearing around tiny corners
	GWorld->SingleLineCheck( Hit, NULL, EndLocation, StartLocation, TRACE_World|TRACE_StopAtAnyHit );

	if( Hit.Actor )
	{
		// green VALID NO SEE
		if( bShowDebug == TRUE )
		{
			warnf( TEXT( "UNavMeshGoal_OutOfViewFrom::EvaluateGoal NO SEE  %s  %s"), *Hit.Actor->GetFullName(), *PolyCtr.ToString() );
			GWorld->GetWorldInfo()->DrawDebugLine( EndLocation, StartLocation, 0, 255, 0, TRUE );
		}

		//warnf( TEXT( "UNavMeshGoal_OutOfViewFrom WE FOUND A GOAL!!!!!!!!!!!!!!! %s" ), *PossibleGoal->GetPolyCenter().ToString() );
		out_GenGoal = PossibleGoal;
		return TRUE;
	}
	else
	{
		// red INVALID CAN SEE
		if( bShowDebug == TRUE )
		{
			warnf( TEXT( "UNavMeshGoal_OutOfViewFrom::EvaluateGoal CAN SEE  %s"), *PolyCtr.ToString()  );
			GWorld->GetWorldInfo()->DrawDebugLine( EndLocation, StartLocation, 255, 0, 0, TRUE );
		}
	}

	return FALSE;
}

void UNavMeshGoal_OutOfViewFrom::NotifyExceededMaxPathVisits( PathCardinalType BestGuess, PathCardinalType& out_GenGoal )
{
	Super::NotifyExceededMaxPathVisits( BestGuess, out_GenGoal );
}

void UNavMeshGoal_OutOfViewFrom::RecycleNative()
{
	//Super::RecycleNative();
}


