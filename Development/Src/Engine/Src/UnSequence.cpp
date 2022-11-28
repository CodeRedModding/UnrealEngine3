/*=============================================================================
	UnSequence.cpp: Gameplay sequence native code
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Ray Davis
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"

// how many operations are we allowed to execute in a single frame?
#define MAX_SEQUENCE_STEPS				1000

// Base class declarations
IMPLEMENT_CLASS(USequenceObject);
IMPLEMENT_CLASS(USequenceAction);
IMPLEMENT_CLASS(USequence);
IMPLEMENT_CLASS(USequenceOp);
IMPLEMENT_CLASS(USequenceCondition);
IMPLEMENT_CLASS(USequenceVariable);
IMPLEMENT_CLASS(USequenceEvent);
IMPLEMENT_CLASS(USequenceFrame);

// Engine level variables
IMPLEMENT_CLASS(USeqVar_Bool);
IMPLEMENT_CLASS(USeqVar_Int);
IMPLEMENT_CLASS(USeqVar_Float);
IMPLEMENT_CLASS(USeqVar_Object);
IMPLEMENT_CLASS(USeqVar_String);
IMPLEMENT_CLASS(USeqVar_Player);
IMPLEMENT_CLASS(USeqVar_RandomInt);
IMPLEMENT_CLASS(USeqVar_External);
IMPLEMENT_CLASS(USeqVar_Named);
IMPLEMENT_CLASS(USeqVar_RandomFloat);

// Engine level conditions.
IMPLEMENT_CLASS(USeqCond_Increment);
IMPLEMENT_CLASS(USeqCond_CompareBool);
IMPLEMENT_CLASS(USeqCond_CompareInt);
IMPLEMENT_CLASS(USeqCond_CompareFloat);

// Engine level events
IMPLEMENT_CLASS(USeqEvent_Touch);
IMPLEMENT_CLASS(USeqEvent_UnTouch);
IMPLEMENT_CLASS(USeqEvent_Destroyed);
IMPLEMENT_CLASS(USeqEvent_Used);
IMPLEMENT_CLASS(USeqEvent_SequenceActivated);
IMPLEMENT_CLASS(USeqEvent_AISeeEnemy);
IMPLEMENT_CLASS(USeqEvent_ConstraintBroken);
IMPLEMENT_CLASS(USeqEvent_LevelStartup);

// Engine level actions
IMPLEMENT_CLASS(USeqAct_CauseDamage);
IMPLEMENT_CLASS(USeqAct_Latent);
IMPLEMENT_CLASS(USeqAct_Toggle);
IMPLEMENT_CLASS(USeqAct_SetCameraTarget);
IMPLEMENT_CLASS(USeqAct_Possess);
IMPLEMENT_CLASS(USeqAct_ActorFactory);
IMPLEMENT_CLASS(USeqAct_FinishSequence);
IMPLEMENT_CLASS(USeqAct_ModifyProperty);
IMPLEMENT_CLASS(USeqAct_GetDistance);
IMPLEMENT_CLASS(USeqAct_GetVelocity);
IMPLEMENT_CLASS(USeqAct_SetBlockRigidBody);
IMPLEMENT_CLASS(USeqAct_SetPhysics);

IMPLEMENT_CLASS(USeqAct_SetBool);
IMPLEMENT_CLASS(USeqAct_SetFloat);
IMPLEMENT_CLASS(USeqAct_SetInt);
IMPLEMENT_CLASS(USeqAct_SetObject);

IMPLEMENT_CLASS(USeqAct_AttachToEvent);
IMPLEMENT_CLASS(USeqAct_Delay);
IMPLEMENT_CLASS(USeqAct_Log);
IMPLEMENT_CLASS(USeqAct_PlaySound);
IMPLEMENT_CLASS(USeqAct_Switch);
IMPLEMENT_CLASS(USeqAct_RandomSwitch);
IMPLEMENT_CLASS(USeqAct_RangeSwitch);
IMPLEMENT_CLASS(USeqAct_DelaySwitch);

IMPLEMENT_CLASS(USeqAct_AIMoveToActor);

//==========================
// AActor sequence interface

void AActor::GetInterpPropertyNames(TArray<FName> &outNames)
{
	// first search for any properties in this actor
	for (TFieldIterator<UProperty> It(GetClass()); It; ++It)
	{
		if (It->PropertyFlags & CPF_Interp)
		{
			// add the property name
			outNames.AddItem(It->GetName());
		}
	}
	// also search through the components array
	for (INT compIdx = 0; compIdx < Components.Num(); compIdx++)
	{
		if (Components(compIdx) != NULL)
		{
			for (TFieldIterator<UProperty> It(Components(compIdx)->GetClass()); It; ++It)
			{
				if (It->PropertyFlags & CPF_Interp)
				{
					// add the property name, mangled to note the component
					outNames.AddItem(FName(*FString::Printf(TEXT("%s.%s"),Components(compIdx)->GetName(),It->GetName())));
				}
			}
		}
	}
}

/* epic ===============================================
* ::GetInterpPropertyRef
*
* Looks up the matching float property and returns a
* reference to the actual value.
*
* =====================================================
*/
FLOAT* AActor::GetInterpPropertyRef(FName inPropName)
{
	// search for the component containing the property
	FString propName, compName;
	// default to searching for the property in this object
	UObject* propObject = this;
	FName searchPropName = inPropName;
	if (FString(*inPropName).Split(TEXT("."),&compName,&propName))
	{
		// search for the matching component
		searchPropName = FName(*propName);
		FName searchCompName(*compName);
		for (INT compIdx = 0; compIdx < Components.Num() && propObject == this; compIdx++)
		{
			if (Components(compIdx) != NULL &&
				Components(compIdx)->GetFName() == searchCompName)
			{
				propObject = Components(compIdx);
			}
		}
	}
	if (propObject != NULL)
	{
		// search for the property in the class's list
		for (TFieldIterator<UFloatProperty> It(propObject->GetClass()); It; ++It)
		{
			if (It->GetFName() == searchPropName)
			{
				// return the resulting float ref
				return ((FLOAT*)(((BYTE*)propObject) + It->Offset));
			}
		}
	}
	return NULL;
}

/**
 * Pipes a script request for event activation to the native
 * implementation.
 */
void AActor::execActivateEvent(FFrame &Stack,RESULT_DECL)
{
	// read the event type
	P_GET_OBJECT(USequenceEvent,inEvent);
	P_GET_ACTOR(inInstigator);
	P_FINISH;
	// pipe to the real activate func
	UBOOL bActivated = 0;
	if (inEvent != NULL)
	{
		bActivated = inEvent->CheckActivate(this,inInstigator);
	}
	*(UBOOL*)Result = bActivated;
}

/** Notification that this Actor has been renamed. Used so we can rename any SequenceEvents that ref to us. */
void AActor::PostRename()
{
	// Only do this check outside gameplay
	if(!GetLevel()->GetLevelInfo()->bBegunPlay)
	{
		// If we have a Kismet sequence for this actor's level..
		if(GetLevel()->GameSequence)
		{
			// Find all SequenceEvents (will recurse into subsequences)
			TArray<USequenceObject*> SeqObjs;
			GetLevel()->GameSequence->FindSeqObjectsByClass( USequenceEvent::StaticClass(), SeqObjs );

			// Then see if any of them refer to this Actor.
			for(INT i=0; i<SeqObjs.Num(); i++)
			{
				USequenceEvent* Event = CastChecked<USequenceEvent>( SeqObjs(i) );

				if(Event->Originator == this)
				{
					USequenceEvent* DefEvent = (USequenceEvent*)( Event->GetClass()->GetDefaultObject() );

					Event->ObjName = FString::Printf( TEXT("%s %s"), GetName(), *DefEvent->ObjName );
				}
			}
		}
	}
}

	
/* epic ===============================================
* ::GetEventOfClass
*
* Searches a matching event object of the specified
* type and returns it.
*
* =====================================================
 */
USequenceEvent* AActor::GetEventOfClass(UClass *inEventClass,USequenceEvent* inLastEvent)
{
	// search for a matching sequence event in the actor
	USequenceEvent *newEvent = NULL;
	UBOOL bFoundLast = inLastEvent == NULL;
	if (inEventClass != NULL)
	{
		for (INT eventIdx = 0; eventIdx < GeneratedEvents.Num() && newEvent == NULL; eventIdx++)
		{
			if (!bFoundLast)
			{
				bFoundLast = (GeneratedEvents(eventIdx) == inLastEvent);
			}
			else
			// check if this is of the proper type
			if (GeneratedEvents(eventIdx) != NULL &&
				GeneratedEvents(eventIdx)->IsA(inEventClass) &&
				GeneratedEvents(eventIdx) != inLastEvent)
			{
				newEvent = GeneratedEvents(eventIdx);
			}
		}
	}
	return newEvent;
}

//==========================
// USequenceOp interface

/**
 * Traverses up the outer chain until the level is found, at which
 * point it returns it.
 * 
 * @return	current outer ULevel
 */
ULevel* USequenceObject::GetLevel()
{
	UObject* nextOuter = GetOuter();
	while (nextOuter != NULL &&
		   !nextOuter->IsA(ULevel::StaticClass()))
	{
		nextOuter = nextOuter->GetOuter();
	}
	check(nextOuter != NULL &&
		  nextOuter->IsA(ULevel::StaticClass()));
	return Cast<ULevel>(nextOuter);
}

/**
 * Looks for the outer sequence and redirects output to it's log
 * file.
 * 
 * @param	outText - string to log
 */
void USequenceObject::ScriptLog(const FString &outText)
{
	USequence *seq = Cast<USequence>(GetOuter());
	if (seq != NULL)
	{
		seq->ScriptLog(outText);
	}
}

/**
 * Script hook for script file log.
 */
void USequenceObject::execScriptLog(FFrame &Stack,RESULT_DECL)
{
	P_GET_STR(outText);
	P_FINISH;
	// redirect to main log func
	ScriptLog(outText);
}

/**
 * Called once this object is exported to a package, used to clean
 * up any actor refs, etc.
 */
void USequenceObject::OnExport()
{
}

/** Snap to ObjPosX/Y to an even multiple of Gridsize. */
void USequenceObject::SnapPosition(INT Gridsize)
{
	ObjPosX = appRound(ObjPosX/Gridsize) * Gridsize;
	ObjPosY = appRound(ObjPosY/Gridsize) * Gridsize;
}

/** Construct full path name of this sequence object, by traversing up through parent sequences until we get to the root sequence. */
FString USequenceObject::GetSeqObjFullName()
{
	FString SeqTitle = GetName();
	USequence *ParentSeq = Cast<USequence>(GetOuter());
	while (ParentSeq != NULL)
	{
		SeqTitle = FString::Printf(TEXT("%s.%s"), ParentSeq->GetName(), SeqTitle);
		ParentSeq = Cast<USequence>(ParentSeq->GetOuter());
	}

	return SeqTitle;
}

/** Keep traversing Outers until we find a Sequence who's Outer is not another Sequence. That is then our 'root' sequence. */
USequence* USequenceObject::GetRootSequence()
{
	USequence* RootSeq = Cast<USequence>(this);
	USequence* ParentSeq = Cast<USequence>(GetOuter());
	while(ParentSeq != NULL)
	{	
		RootSeq = ParentSeq;
		ParentSeq = Cast<USequence>(ParentSeq->GetOuter());
	}
	check(RootSeq);
	return RootSeq;
}

void USequenceOp::CheckForErrors()
{
	// validate variable links
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		for (INT varIdx = 0; varIdx < VariableLinks(idx).LinkedVariables.Num(); varIdx++)
		{
			if (VariableLinks(idx).LinkedVariables(varIdx) == NULL)
			{
				VariableLinks(idx).LinkedVariables.Remove(varIdx--,1);
			}
		}
	}
	// validate output links
	for (INT idx = 0; idx < OutputLinks.Num(); idx++)
	{
		for (INT linkIdx = 0; linkIdx < OutputLinks(idx).Links.Num(); linkIdx++)
		{
			if (OutputLinks(idx).Links(linkIdx).LinkedOp == NULL)
			{
				OutputLinks(idx).Links.Remove(linkIdx--,1);
			}
		}
	}
}

/**
 *	Ensure that any Output, Variable or Event connectors only point to objects with the same Outer (ie are in the same Sequence).
 */
void USequenceOp::CleanupConnections()
{
	// first check output logic links
	for(INT idx = 0; idx < OutputLinks.Num(); idx++)
	{
		for(INT linkIdx = 0; linkIdx < OutputLinks(idx).Links.Num(); linkIdx++)
		{
			USequenceOp* SeqOp = OutputLinks(idx).Links(linkIdx).LinkedOp;
			if(SeqOp && SeqOp->GetOuter() != GetOuter())
			{
				OutputLinks(idx).Links.Remove(linkIdx--,1);
			}
		}
	}

	// next check variables
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		for (INT varIdx = 0; varIdx < VariableLinks(idx).LinkedVariables.Num(); varIdx++)
		{
			USequenceVariable* SeqVar = VariableLinks(idx).LinkedVariables(varIdx);
			if(SeqVar &&  SeqVar->GetOuter() != GetOuter())
			{
				VariableLinks(idx).LinkedVariables.Remove(varIdx--,1);
			}
		}
	}

	// events
	for(INT idx = 0; idx < EventLinks.Num(); idx++)
	{
		for(INT evtIdx = 0; evtIdx < EventLinks(idx).LinkedEvents.Num(); evtIdx++)
		{
			USequenceEvent* SeqEvent = EventLinks(idx).LinkedEvents(evtIdx);
			if(SeqEvent && SeqEvent->GetOuter() != GetOuter())
			{
				EventLinks(idx).LinkedEvents.Remove(evtIdx--,1);
			}
		}
	}
}

//@todo: templatize these helper funcs
void USequenceOp::GetBoolVars(TArray<UBOOL*> &outBools, const TCHAR *inDesc)
{
	// search for all variables of the expected type
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		// if correct type, and
		// no desc requested, or matches requested desc
		if (VariableLinks(idx).ExpectedType == USeqVar_Bool::StaticClass() &&
			(inDesc == NULL ||
			 VariableLinks(idx).LinkDesc == inDesc))
		{
			// add the refs to out list
			for (INT linkIdx = 0; linkIdx < VariableLinks(idx).LinkedVariables.Num(); linkIdx++)
			{
				if (VariableLinks(idx).LinkedVariables(linkIdx) != NULL)
				{
					UBOOL *boolRef = VariableLinks(idx).LinkedVariables(linkIdx)->GetBoolRef();
					if (boolRef != NULL)
					{
						outBools.AddItem(boolRef);
					}
				}
			}
		}
	}
}

void USequenceOp::GetIntVars(TArray<INT*> &outInts, const TCHAR *inDesc)
{
	// search for all variables of the expected type
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		// if correct type, and
		// no desc requested, or matches requested desc
		if (VariableLinks(idx).ExpectedType == USeqVar_Int::StaticClass() &&
			(inDesc == NULL ||
			 VariableLinks(idx).LinkDesc == inDesc))
		{
			// add the refs to out list
			for (INT linkIdx = 0; linkIdx < VariableLinks(idx).LinkedVariables.Num(); linkIdx++)
			{
				if (VariableLinks(idx).LinkedVariables(linkIdx) != NULL)
				{
					INT *intRef = VariableLinks(idx).LinkedVariables(linkIdx)->GetIntRef();
					if (intRef != NULL)
					{
						outInts.AddItem(intRef);
					}
				}
			}
		}
	}
}

void USequenceOp::GetFloatVars(TArray<FLOAT*> &outFloats, const TCHAR *inDesc)
{
	// search for all variables of the expected type
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		// if correct type, and
		// no desc requested, or matches requested desc
		if (VariableLinks(idx).ExpectedType == USeqVar_Float::StaticClass() &&
			(inDesc == NULL ||
			 VariableLinks(idx).LinkDesc == inDesc))
		{
			// add the refs to out list
			for (INT linkIdx = 0; linkIdx < VariableLinks(idx).LinkedVariables.Num(); linkIdx++)
			{
				if (VariableLinks(idx).LinkedVariables(linkIdx) != NULL)
				{
					FLOAT *floatRef = VariableLinks(idx).LinkedVariables(linkIdx)->GetFloatRef();
					if (floatRef != NULL)
					{
						outFloats.AddItem(floatRef);
					}
				}
			}
		}
	}
}

void USequenceOp::GetObjectVars(TArray<UObject**> &outObjects, const TCHAR *inDesc)
{
	// search for all variables of the expected type
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		// if correct type, and
		// no desc requested, or matches requested desc
		if (VariableLinks(idx).ExpectedType->IsChildOf(USeqVar_Object::StaticClass()) &&
			(inDesc == NULL ||
			 VariableLinks(idx).LinkDesc == inDesc))
		{
			// add the refs to out list
			for (INT linkIdx = 0; linkIdx < VariableLinks(idx).LinkedVariables.Num(); linkIdx++)
			{
				if (VariableLinks(idx).LinkedVariables(linkIdx) != NULL)
				{
					UObject **objectRef = VariableLinks(idx).LinkedVariables(linkIdx)->GetObjectRef();
					if (objectRef != NULL)
					{
						outObjects.AddItem(objectRef);
					}
				}
			}
		}
	}
}

void USequenceOp::execGetObjectVars(FFrame &Stack,RESULT_DECL)
{
	P_GET_TARRAY_REF(outObjVars,UObject*);
	P_GET_STR_OPTX(inDesc,TEXT(""));
	P_FINISH;
	TArray<UObject**> objVars;
	GetObjectVars(objVars,inDesc != TEXT("") ? *inDesc : NULL);
	if (objVars.Num() > 0)
	{
		for (INT idx = 0; idx < objVars.Num(); idx++)
		{
			outObjVars->AddItem(*(objVars(idx)));
		}
	}
}

void USequenceOp::GetStringVars(TArray<FString*> &outStrings, const TCHAR *inDesc)
{
	// search for all variables of the expected type
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		// if correct type, and
		// no desc requested, or matches requested desc
		if (VariableLinks(idx).ExpectedType == USeqVar_String::StaticClass() &&
			(inDesc == NULL ||
			 VariableLinks(idx).LinkDesc == inDesc))
		{
			// add the refs to out list
			for (INT linkIdx = 0; linkIdx < VariableLinks(idx).LinkedVariables.Num(); linkIdx++)
			{
				if (VariableLinks(idx).LinkedVariables(linkIdx) != NULL)
				{
					FString *stringRef = VariableLinks(idx).LinkedVariables(linkIdx)->GetStringRef();
					if (stringRef != NULL)
					{
						outStrings.AddItem(stringRef);
					}
				}
			}
		}
	}
}

/* epic ===============================================
* ::UpdateOp
*
* Called from parent object, which allows this operation
* to update.
* Returns TRUE to indicate that this action has completed.
*
* =====================================================
*/
UBOOL USequenceOp::UpdateOp(FLOAT deltaTime)
{
	// dummy stub, immediately finish
	return 1;
}

/* epic ===============================================
* ::Activated
*
* Called once this op has been activated.
*
* =====================================================
*/
void USequenceOp::Activated()
{
}

/* epic ===============================================
* ::DeActivated
*
* Called once this op has been deactivated.
*
* =====================================================
*/
void USequenceOp::DeActivated()
{
	// activate all output impulses
	for (INT linkIdx = 0; linkIdx < OutputLinks.Num(); linkIdx++)
	{
		OutputLinks(linkIdx).bHasImpulse = 1;
	}
}

/* epic ===============================================
* ::FindConnectorIndex
*
* Utility for finding if a connector of the given type and with the given name exists.
* Returns its index if so, or INDEX_NONE otherwise.
*
* =====================================================
*/
INT USequenceOp::FindConnectorIndex(const FString& ConnName, INT ConnType)
{
	if(ConnType == LOC_INPUT)
	{
		for(INT i=0; i<InputLinks.Num(); i++)
		{
			if( InputLinks(i).LinkDesc == ConnName)
				return i;
		}
	}
	else if(ConnType == LOC_OUTPUT)
	{
		for(INT i=0; i<OutputLinks.Num(); i++)
		{
			if( OutputLinks(i).LinkDesc == ConnName)
				return i;
		}
	}
	else if(ConnType == LOC_VARIABLE)
	{
		for(INT i=0; i<VariableLinks.Num(); i++)
		{
			if( VariableLinks(i).LinkDesc == ConnName)
				return i;
		}
	}
	else if (ConnType == LOC_EVENT)
	{
		for (INT idx = 0; idx < EventLinks.Num(); idx++)
		{
			if (EventLinks(idx).LinkDesc == ConnName)
			{
				return idx;
			}
		}
	}

	return INDEX_NONE;
}



//==========================
// USequence interface

#include "FOutputDeviceFile.h"

/**
 * Adds all the variables linked to the external variable to the given variable
 * link, recursing as necessary for multiply linked external variables.
 */
static void AddExternalVariablesToLink(FSeqVarLink &varLink, USeqVar_External *extVar)
{
	if (extVar != NULL)
	{
		USequence *seq = Cast<USequence>(extVar->GetOuter());
		if (seq != NULL)
		{
			for (INT varIdx = 0; varIdx < seq->VariableLinks.Num(); varIdx++)
			{
				if (seq->VariableLinks(varIdx).LinkVar == extVar->GetFName())
				{
					for (INT idx = 0; idx < seq->VariableLinks(varIdx).LinkedVariables.Num(); idx++)
					{
						USequenceVariable *var = seq->VariableLinks(varIdx).LinkedVariables(idx);
						if (var != NULL)
						{
							debugf(TEXT("... %s"),var->GetName());
							// check for a linked external variable
							if (var->IsA(USeqVar_External::StaticClass()))
							{
								// and recursively add
								AddExternalVariablesToLink(varLink,(USeqVar_External*)var);
							}
							else
							{
								// otherwise add this normal instance
								varLink.LinkedVariables.AddUniqueItem(var);
							}
						}
					}
				}
			}
		}
	}
}

/** Find the variable referenced by name by the USeqVar_Named and add a reference to it. */
static void AddNamedVariableToLink(FSeqVarLink&varLink, USeqVar_Named *namedVar)
{
	// Do nothing if no variable or no type set.
	if(namedVar == NULL || namedVar->ExpectedType == NULL)
	{
		return;
	}

	check(namedVar->ExpectedType->IsChildOf(USequenceVariable::StaticClass()));

	// Warn and do nothing if FindVarName is left empty at start of game.
	if(namedVar->FindVarName == NAME_None)
	{
		namedVar->ScriptLog(FString::Printf(TEXT("WARNING! SeqVar_Named (%s) with empty FindVarName."), namedVar->GetName() ));
		return;
	}

	USequence* RootSeq = namedVar->GetRootSequence();
	check(RootSeq);

	TArray<USequenceVariable*> Vars;
	RootSeq->FindNamedVariables(namedVar->FindVarName, false, Vars);

	// Only assign if we get one result and its the right type. If not - do nothing and log the problem.
	if(Vars.Num() == 0)
	{
		namedVar->ScriptLog(FString::Printf(TEXT("WARNING! No variable found. Class: %s Name: %s"), namedVar->ExpectedType->GetName(), *(namedVar->FindVarName) ));
	}
	else if(Vars.Num() > 1)
	{
		namedVar->ScriptLog(FString::Printf(TEXT("WARNING! Duplicate variables found with same name. Class: %s Name: %s"), namedVar->ExpectedType->GetName(), *(namedVar->FindVarName) ));
	}
	else if( !Vars(0)->IsA(namedVar->ExpectedType) )
	{
		namedVar->ScriptLog(FString::Printf(TEXT("WARNING! Variable is of incorrect type. Class: %s Name: %s"), namedVar->ExpectedType->GetName(), *(namedVar->FindVarName) ));
	}
	else
	{
		// Assign the first one to this link
		varLink.LinkedVariables.AddUniqueItem( Vars(0) );
	}
}

/**
 * Called from Level startup, used to register all events to associated actors
 * and auto-fire any startup events.  Also creates the script log if our next
 * outer isn't another sequence.
 */
void USequence::BeginPlay()
{
	// create the script log archive
	if (Cast<USequence>(GetOuter()) == NULL &&
		LogFile == NULL)
	{
		LogFile = new FOutputDeviceFile();
		appStrcpy(((FOutputDeviceFile*)LogFile)->Filename,*FString::Printf(TEXT("%sKismet.log"),*appGameLogDir()));
	}
	// clear all actor's events
	ULevel *level = GetLevel();
	if (GetOuter() == level)
	{
		for (INT idx = 0; idx < level->Actors.Num(); idx++)
		{
			AActor *evtActor = level->Actors(idx);
			if (evtActor != NULL)
			{
				evtActor->GeneratedEvents.Empty();
			}
		}
	}
	// first register all events
	TArray<USequence*> nestedSeqs;
	for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
	{
		USeqVar_External* ExtVar = Cast<USeqVar_External>(SequenceObjects(idx));
		USeqVar_Named* NamedVar = Cast<USeqVar_Named>(SequenceObjects(idx));

		if (SequenceObjects(idx)->IsA(USequenceEvent::StaticClass()))
		{
			USequenceEvent *evt = (USequenceEvent*)(SequenceObjects(idx));
			ScriptLog(FString::Printf(TEXT("Event %s registering with actor %s"),evt->GetName(),evt->Originator!=NULL?evt->Originator->GetName():TEXT("NULL")));
			if (evt->Originator != NULL)
			{
				evt->Originator->GeneratedEvents.AddUniqueItem(evt);
			}
		}
		else
		// replace any external or named variables with their linked counterparts
		if (ExtVar || NamedVar)
		{
			// find all objects referencing this variable
			for (INT objIdx = 0; objIdx < SequenceObjects.Num(); objIdx++)
			{
				USequenceOp *op = Cast<USequenceOp>(SequenceObjects(objIdx));
				if (op != NULL &&
					op->VariableLinks.Num() > 0)
				{
					// search for a variable link to the var
					for (INT varIdx = 0; varIdx < op->VariableLinks.Num(); varIdx++)
					{
						if(ExtVar)
						{
							INT linkIdx = 0;
							if (op->VariableLinks(varIdx).LinkedVariables.FindItem(ExtVar,linkIdx))
							{
								// remove this variable's link
								op->VariableLinks(varIdx).LinkedVariables.Remove(linkIdx,1);
								// and add all the linked variables instead
								AddExternalVariablesToLink(op->VariableLinks(varIdx),ExtVar);
							}
						}
						else
						{
							INT linkIdx = 0;
							if (op->VariableLinks(varIdx).LinkedVariables.FindItem(NamedVar,linkIdx))
							{
								// remove this variable's link
								op->VariableLinks(varIdx).LinkedVariables.Remove(linkIdx,1);
								// and add all the linked variables instead
								AddNamedVariableToLink(op->VariableLinks(varIdx),NamedVar);
							}
						}
					}
				}
			}
		}
		else
		// recurse for any nested sequences
		if (SequenceObjects(idx)->IsA(USequence::StaticClass()))
		{
			USequence *seq = (USequence*)(SequenceObjects(idx));
			nestedSeqs.AddItem(seq);
		}
	}
	// initialize all nested sequences
	while (nestedSeqs.Num() > 0)
	{
		USequence *seq = nestedSeqs.Pop();
		seq->BeginPlay();
	}
	// check for any auto-fire events
	for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
	{
		// if outer most sequence, check for SequenceActivated events as well
		if (GetOuter()->IsA(ULevel::StaticClass()))
		{
			USeqEvent_SequenceActivated *evt = Cast<USeqEvent_SequenceActivated>(SequenceObjects(idx));
			if (evt != NULL)
			{
				evt->CheckActivate(GetLevel()->GetLevelInfo(),NULL,0);
			}
		}
		USeqEvent_LevelStartup *evt = Cast<USeqEvent_LevelStartup>(SequenceObjects(idx));
		if (evt != NULL)
		{
			evt->CheckActivate(GetLevel()->GetLevelInfo(),NULL,0);
		}
	}
}

/**
 * Overridden to release log file if opened.
 */
void USequence::Destroy()
{
	FOutputDeviceFile* LogFile = (FOutputDeviceFile*) this->LogFile;

	if( LogFile != NULL )
	{
		LogFile->TearDown();
		delete LogFile;
		LogFile = NULL;
	}
	Super::Destroy();
}

/**
 * If ScriptLogAr is valid, outputs the specified string to it,
 * otherwise attempts to redirect to outer sequence.
 * 
 * @param	outText - string to log to ScriptLogAr
 */
void USequence::ScriptLog(const FString &outText)
{
	if (LogFile != NULL)
	{
		FOutputDeviceFile *outFile = (FOutputDeviceFile*)LogFile;
		outFile->Serialize(*outText,NAME_Log);
		outFile->Flush();
	}
	else
	if (Cast<USequence>(GetOuter()) != NULL)
	{
		((USequence*)GetOuter())->ScriptLog(outText);
	}
}

/**
 * Looks for obvious errors and attempts to auto-correct them if
 * possible.
 */
void USequence::CheckForErrors()
{
	for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
	{
		USequenceObject *seqObj = SequenceObjects(idx);
		if (seqObj == NULL)
		{
			SequenceObjects.Remove(idx--,1);
		}
		else
		{
			if (seqObj->GetOuter() != this)
			{
				debugf(NAME_Warning,TEXT("Scripting object %s had outer of %s, fixing"),seqObj->GetName(),seqObj->GetOuter()->GetName());
				seqObj->Rename(seqObj->GetName(),this);
			}
			seqObj->CheckForErrors();
		}
	}
}

/* epic ===============================================
* ::BuildOpStack
*
* Iterates through all sequence objects and builds the
* stack of active operations.
*
* =====================================================
*/
void USequence::BuildOpStack(TArray<USequenceOp*> &opStack)
{
	// iterate through all events
	for (INT objIdx = 0; objIdx < SequenceObjects.Num(); objIdx++)
	{
		// if active event,
		if (SequenceObjects(objIdx)->IsA(USequenceEvent::StaticClass()))
		{
			USequenceEvent *event = (USequenceEvent*)(SequenceObjects(objIdx));
			if (event->bActive)
			{
				// make sure the event is enabled
				if (event->bEnabled)
				{
					// place new node at bottom of the stack
					opStack.Insert(0,1);
					opStack(0) = event;
				}
				else
				{
					// otherwise disable the activation
					event->bActive = 0;
				}
			}
		}
		else
		// if it's a latent action
		if (SequenceObjects(objIdx)->IsA(USeqAct_Latent::StaticClass()))
		{
			USeqAct_Latent *latent = (USeqAct_Latent*)(SequenceObjects(objIdx));
			if (latent != NULL &&
				latent->bActive)
			{
				// place at the top of the stack
				opStack.AddItem(latent);
			}
		}
		else
		// check for a subsequence with active outputs
		if (SequenceObjects(objIdx)->IsA(USequence::StaticClass()))
		{
			USequence *seq = (USequence*)(SequenceObjects(objIdx));
			// check to see if this sequence has activated any output links
			for (INT outputIdx = 0; outputIdx < seq->OutputLinks.Num(); outputIdx++)
			{
				FSeqOpOutputLink &link = seq->OutputLinks(outputIdx);
				// if it has an impulse
				if (link.bHasImpulse)
				{
					// iterate through all linked inputs
					for (INT inputIdx = 0; inputIdx < link.Links.Num(); inputIdx++)
					{
						if (link.Links(inputIdx).LinkedOp != NULL)
						{
							// mark the input link that is now active
							link.Links(inputIdx).LinkedOp->InputLinks(link.Links(inputIdx).InputLinkIdx).bHasImpulse = 1;
							// place new node on stack
							opStack.AddItem(link.Links(inputIdx).LinkedOp);
						}
					}
					link.bHasImpulse = 0;
				}
			}
		}
	}
	/*
	if (opStack.Num() > 0)
	{
		ScriptLog(FString::Printf(TEXT("Sequence %s active ops:"),GetName()));
		for (INT idx = 0; idx < opStack.Num(); idx++)
		{
			ScriptLog(FString::Printf(TEXT("+ %s"),opStack(idx)->GetName()));
		}
	}
	*/
}

/**
 * Used to save an op to activate and the impulse index.
 */
struct FActivateOp
{
	/** Op pending activation */
	USequenceOp *op;
	/** Input links to activate */
	INT idx;
};

/* epic ===============================================
* ::ExecuteOpStack
*
* Steps through the supplied operation stack, adding
* resulting operations to the top of the stack.  Can
* be used as a single step by setting maxSteps to !0.
*
* Returns TRUE when no more ops are available to execute.
*
* =====================================================
*/
UBOOL USequence::ExecuteOpStack(FLOAT deltaTime, TArray<USequenceOp*> &opStack, INT maxSteps)
{
	TArray<FActivateOp> activateOps;
	// while nodes are on stack
	INT steps = 0;
	while (opStack.Num() > 0 &&
		   (steps++ < maxSteps ||
			maxSteps == 0))
	{
		// make sure we haven't hit an infinite loop
		if (steps >= MAX_SEQUENCE_STEPS)
		{
			ScriptLog(FString(TEXT("Max scripting execution steps exceeded, aborting!")));
			break;
		}
		// pop top node
		USequenceOp *nextOp = opStack.Pop();
		// execute next action
		if (nextOp != NULL)
		{
			// if it isn't already active
			if (!nextOp->bActive)
			{
				ScriptLog(FString::Printf(TEXT("-> %s (%s) has been activated"),*nextOp->ObjName,nextOp->GetName()));
				// activate the op
				nextOp->bActive = 1;
				nextOp->Activated();
			}
			// update the op
			if (nextOp->bActive)
			{
				nextOp->bActive = !nextOp->UpdateOp(deltaTime);
				// if it's no longer active, or a latent action
				if (!nextOp->bActive ||
					nextOp->IsA(USeqAct_Latent::StaticClass()))
				{
					// if it's no longer active send the deactivated callback
					if(!nextOp->bActive)
					{
						ScriptLog(FString::Printf(TEXT("-> %s (%s) has finished execution"),*nextOp->ObjName,nextOp->GetName()));
						nextOp->DeActivated();
					}
					// iterate through all outputs,
					for (INT outputIdx = 0; outputIdx < nextOp->OutputLinks.Num(); outputIdx++)
					{
						FSeqOpOutputLink &link = nextOp->OutputLinks(outputIdx);
						// if it has an impulse
						if (link.bHasImpulse)
						{
							ScriptLog(FString::Printf(TEXT("--> link %s (%d) activated"),*link.LinkDesc,outputIdx));
							// iterate through all linked inputs
							for (INT inputIdx = 0; inputIdx < link.Links.Num(); inputIdx++)
							{
								if (link.Links(inputIdx).LinkedOp != NULL)
								{
									// add to the list of pending activation
									INT aIdx = activateOps.AddZeroed();
									activateOps(aIdx).op = link.Links(inputIdx).LinkedOp;
									activateOps(aIdx).idx = link.Links(inputIdx).InputLinkIdx;
								}
							}
						}
					}
				}
			}
			// clear inputs on this op
			for (INT inputIdx = 0; inputIdx < nextOp->InputLinks.Num(); inputIdx++)
			{
				nextOp->InputLinks(inputIdx).bHasImpulse = 0;
			}
			for (INT outputIdx = 0; outputIdx < nextOp->OutputLinks.Num(); outputIdx++)
			{
				nextOp->OutputLinks(outputIdx).bHasImpulse = 0;
			}
			// add all pending ops to be activated
			while (activateOps.Num() > 0)
			{
				INT idx = activateOps.Num() - 1;
				USequenceOp *op = activateOps(idx).op;
				op->InputLinks(activateOps(idx).idx).bHasImpulse = 1;
				opStack.AddItem(op);
				// and remove from list
				activateOps.Pop();
				ScriptLog(FString::Printf(TEXT("-> %s activating output to: %s"),nextOp->GetName(),op->GetName()));
			}
		}
	}
	return (opStack.Num() == 0);
}

/* epic ===============================================
* ::UpdateOp
*
* =====================================================
*/
UBOOL USequence::UpdateOp(FLOAT deltaTime)
{
	TArray<USequenceOp*> opStack;
	BuildOpStack(opStack);
	ExecuteOpStack(deltaTime,opStack);
	// iterate through all active child sequences and update them as well
	for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
	{
		USequence *seq = Cast<USequence>(SequenceObjects(idx));
		if (seq != NULL)
		{
			seq->UpdateOp(deltaTime);
		}
	}
	return 0;
}

/**
 * Iterates through all sequence objects look for finish actions, creating output links as needed.
 * Also handles inputs via SeqEvent_SequenceActivated, and variables via SeqVar_External.
 */
void USequence::UpdateConnectors()
{
	// look for changes in existing outputs, or new additions
	TArray<FName> outNames;
	TArray<FName> inNames;
	TArray<FName> varNames;
	for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
	{
		if (SequenceObjects(idx)->IsA(USeqAct_FinishSequence::StaticClass()))
		{
			USeqAct_FinishSequence *act = (USeqAct_FinishSequence*)(SequenceObjects(idx));
			// look for an existing output link
			UBOOL bFoundOutput = 0;
			for (INT linkIdx = 0; linkIdx < OutputLinks.Num() && !bFoundOutput; linkIdx++)
			{
				if (OutputLinks(linkIdx).LinkAction == act->GetFName())
				{
					bFoundOutput = 1;
					// update the text label
					OutputLinks(linkIdx).LinkDesc = act->OutputLabel;
					// and add to the list of known outputs
					outNames.AddItem(act->GetFName());
				}
			}
			// if we didn't find an output
			if (!bFoundOutput)
			{
				// create a new connector
				INT newIdx = OutputLinks.AddZeroed();
				OutputLinks(newIdx).LinkDesc = act->OutputLabel;
				OutputLinks(newIdx).LinkAction = act->GetFName();
				outNames.AddItem(act->GetFName());
			}
		}
		else
		if (SequenceObjects(idx)->IsA(USeqEvent_SequenceActivated::StaticClass()))
		{
			USeqEvent_SequenceActivated *evt = (USeqEvent_SequenceActivated*)(SequenceObjects(idx));
			// look for an existing input link
			UBOOL bFoundInput = 0;
			for (INT linkIdx = 0; linkIdx < InputLinks.Num() && !bFoundInput; linkIdx++)
			{
				if (InputLinks(linkIdx).LinkAction == evt->GetFName())
				{
					bFoundInput = 1;
					// update the text label
					InputLinks(linkIdx).LinkDesc = evt->InputLabel;
					// and add to the list of known inputs
					inNames.AddItem(evt->GetFName());
				}
			}
			// if we didn't find an input,
			if (!bFoundInput)
			{
				// create a new connector
				INT newIdx = InputLinks.AddZeroed();
				InputLinks(newIdx).LinkDesc = evt->InputLabel;
				InputLinks(newIdx).LinkAction = evt->GetFName();
				inNames.AddItem(evt->GetFName());
			}
		}
		else
		if (SequenceObjects(idx)->IsA(USeqVar_External::StaticClass()))
		{
			USeqVar_External *var = (USeqVar_External*)(SequenceObjects(idx));
			// look for an existing var link
			UBOOL bFoundVar = 0;
			for (INT varIdx = 0; varIdx < VariableLinks.Num() && !bFoundVar; varIdx++)
			{
				if (VariableLinks(varIdx).LinkVar == var->GetFName())
				{
					bFoundVar = 1;
					// update the text label
					VariableLinks(varIdx).LinkDesc = var->VariableLabel;
					VariableLinks(varIdx).ExpectedType = var->ExpectedType;
					// and add to the list of known vars
					varNames.AddItem(var->GetFName());
				}
			}
			if (!bFoundVar)
			{
				// add a new entry
				INT newIdx = VariableLinks.AddZeroed();
				VariableLinks(newIdx).LinkDesc = var->VariableLabel;
				VariableLinks(newIdx).LinkVar = var->GetFName();
				VariableLinks(newIdx).ExpectedType = var->ExpectedType;
				VariableLinks(newIdx).MinVars = 0;
				VariableLinks(newIdx).MaxVars = 255;
				varNames.AddItem(var->GetFName());
			}
		}
	}
	// clean up any outputs that may of been deleted
	for (INT idx = 0; idx < OutputLinks.Num(); idx++)
	{
		INT itemIdx = 0; // junk
		if (!outNames.FindItem(OutputLinks(idx).LinkAction,itemIdx))
		{
			OutputLinks.Remove(idx--,1);
		}
	}
	// and finally clean up any inputs that may of been deleted
	for (INT idx = 0; idx < InputLinks.Num(); idx++)
	{
		INT itemIdx = 0;
		if (!inNames.FindItem(InputLinks(idx).LinkAction,itemIdx))
		{
			InputLinks.Remove(idx--,1);
		}
	}
	// look for missing variable links
	for (INT idx = 0; idx < VariableLinks.Num(); idx++)
	{
		INT itemIdx = 0;
		if (!varNames.FindItem(VariableLinks(idx).LinkVar,itemIdx))
		{
			VariableLinks.Remove(idx--,1);
		}
	}
}

/**
 * Activates SeqEvent_SequenceActivated contained within this sequence that match
 * the LinkAction of the activated InputLink.
 */
void USequence::Activated()
{
	// figure out which inputs are active
	for (INT idx = 0; idx < InputLinks.Num(); idx++)
	{
		if (InputLinks(idx).bHasImpulse)
		{
			// find the matching sequence event to activate
			for (INT objIdx = 0; objIdx < SequenceObjects.Num(); objIdx++)
			{
				USeqEvent_SequenceActivated *evt = Cast<USeqEvent_SequenceActivated>(SequenceObjects(objIdx));
				if (evt != NULL &&
					evt->GetFName() == InputLinks(idx).LinkAction)
				{
					//@todo - figure out the instigator and run through CheckActivate
					evt->bActive = 1;
				}
			}
		}
	}
	// and deactivate this sequence
	bActive = 0;
}

/** 
 *	Find all the SequenceObjects of the specified class within this sequence and add them to the OutputObjects array. 
 *	Will look in any subsequences as well. 
 *	Objects in parent sequences are always added to array before children.
 *
 *	@param	DesiredClass	Subclass of SequenceObject to search for.
 *	@param	OutputObjects	Output array of objects of the desired class.
 */
void USequence::FindSeqObjectsByClass(UClass* DesiredClass, TArray<USequenceObject*>& OutputObjects)
{
	TArray<USequence*> SubSequences;

	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if(SequenceObjects(i)->IsA(DesiredClass))
		{
			OutputObjects.AddItem( SequenceObjects(i) );
		}

		// Note any subsequences to traverse after we have finished with this sequence.
		USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
		if(SubSeq)
		{
			SubSequences.AddItem( SubSeq );
		}
	}

	// Look in any subsequences of this one.
	for(INT i=0; i<SubSequences.Num(); i++)
	{
		SubSequences(i)->FindSeqObjectsByClass(DesiredClass, OutputObjects);
	}
}


/** 
 *	Searches this sequence (and subsequences) for SequenceObjects which contain the given string in their name (ie substring match). 
 *	Will only add results to OutputObjects if they are not already there.
 *
 *	@param Name				Name to search for
 *	@param bCheckComment	Search object comments as well
 *	@param OutputObjects	Output array of objects matching supplied name
 */
void USequence::FindSeqObjectsByName(const FString& Name, UBOOL bCheckComment, TArray<USequenceObject*>& OutputObjects)
{
	FString CapsName = Name.Caps();

	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if( (SequenceObjects(i)->ObjName.Caps().InStr(*CapsName) != -1) || (bCheckComment && (SequenceObjects(i)->ObjComment.Caps().InStr(*CapsName) != -1)) )
		{
			OutputObjects.AddUniqueItem( SequenceObjects(i) );
		}
	
		// Check any subsequences.
		USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
		if(SubSeq)
		{
			SubSeq->FindSeqObjectsByName( Name, bCheckComment, OutputObjects );
		}
	}
}

/** 
 *	Searches this sequence (and subsequences) for SequenceObjects which reference an object with the supplied name. Complete match, not substring.
 *	Will only add results to OutputObjects if they are not already there.
 *
 *	@param Name				Name of referenced object to search for
 *	@param OutputObjects	Output array of objects referencing the Object with the supplied name.
 */
void USequence::FindSeqObjectsByObjectName(FName Name, TArray<USequenceObject*>& OutputObjects)
{
	// Iterate over objects in this sequence
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		// If its an SeqVar_Object, check its contents.
		USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SequenceObjects(i) );
		if(ObjVar && ObjVar->ObjValue && ObjVar->ObjValue->GetFName() == Name)
		{
			OutputObjects.AddUniqueItem(ObjVar);
		}

		// If its a SequenceEvent, check the Originator.
		USequenceEvent* Event = Cast<USequenceEvent>( SequenceObjects(i) );
		if(Event && Event->Originator && Event->Originator->GetFName() == Name)
		{
			OutputObjects.AddUniqueItem(Event);
		}

		// Check any subsequences.
		USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
		if(SubSeq)
		{
			SubSeq->FindSeqObjectsByObjectName( Name, OutputObjects );
		}
	}
}

/** 
 *	Search this sequence and all subsequences to find USequenceVariables with the given VarName.
 *	This function can also find uses of a named variable.
 *
 *	@param	VarName			Checked against VarName (or FindVarName) to find particular Variable.
 *	@param	bFindUses		Instead of find declaration of variable with given name, will find uses of it.
 *  @param	OutputVars		Results of search - array containing found sequence Variables which match supplied name.
 */
void USequence::FindNamedVariables(FName VarName, UBOOL bFindUses, TArray<USequenceVariable*>& OutputVars)
{
	// If no name passed in, never return variables.
	if(VarName == NAME_None)
	{
		return;
	}

	// Iterate over objects in this sequence
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		if(bFindUses)
		{
			// Check any USeqVar_Named.
			USeqVar_Named* NamedVar = Cast<USeqVar_Named>( SequenceObjects(i) );
			if(NamedVar && NamedVar->FindVarName == VarName)
			{
				OutputVars.AddUniqueItem(NamedVar);
			}
		}
		else
		{
			// Check any SequenceVariables.
			USequenceVariable* SeqVar = Cast<USequenceVariable>( SequenceObjects(i) );
			if(SeqVar && SeqVar->VarName == VarName)
			{
				OutputVars.AddUniqueItem(SeqVar);
			}
		}

		// Check any subsequences.
		USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
		if(SubSeq)
		{
			SubSeq->FindNamedVariables( VarName, bFindUses, OutputVars );
		}
	}
}

/** Iterate over all subsequences  and SeqVar_Named variables updating their status. */
void USequence::UpdateNamedVarStatus()
{
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		USeqVar_Named* NameVar = Cast<USeqVar_Named>( SequenceObjects(i) );
		if( NameVar )
		{
			NameVar->UpdateStatus();
		}
	
		// Check any subsequences.
		USequence* SubSeq = Cast<USequence>( SequenceObjects(i) );
		if(SubSeq)
		{
			SubSeq->UpdateNamedVarStatus();
		}
	}
}

/** Iterate over all SeqAct_Interp (Matinee) actions calling UpdateConnectorsFromData on them, to ensure they are up to date. */
void USequence::UpdateInterpActionConnectors()
{
	TArray<USequenceObject*> MatineeActions;
	FindSeqObjectsByClass( USeqAct_Interp::StaticClass(), MatineeActions );

	for(INT i=0; i<MatineeActions.Num(); i++)
	{
		USeqAct_Interp* TestAction = CastChecked<USeqAct_Interp>( MatineeActions(i) );
		check(TestAction);

		TestAction->UpdateConnectorsFromData();
	}
}

/** 
 *	See if this object is referenced by any SeqVar_Objects or is the Originator of any SequenceEvents. 
 *
 *	@param	InObject Object to find references to.
 *	@return	If this object is references by any script objects.
 */
UBOOL USequence::ReferencesObject(UObject* InObject)
{
	// Return 'false' if NULL object passed in.
	if(!InObject)
	{
		return false;
	}

	// Iterate over objects in this sequence
	for(INT i=0; i<SequenceObjects.Num(); i++)
	{
		// If its an SeqVar_Object, check its contents.
		USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SequenceObjects(i) );
		if(ObjVar)
		{
			if(ObjVar->ObjValue == InObject)
			{
				return true;
			}
		}

		// If its a SequenceEvent, check the Originator.
		USequenceEvent* Event = Cast<USequenceEvent>( SequenceObjects(i) );
		if(Event)
		{
			if(Event->Originator == InObject)
			{
				return true;
			}
		}

		// If this is a subsequence, check it for the given object. If we find it, return true.
		USequence* Seq = Cast<USequence>( SequenceObjects(i) );
		if(Seq)
		{
			if( Seq->ReferencesObject(InObject) )
			{
				return true;
			}
		}
	}

	return false;
}

void USequence::execFindSeqObjectsByClass(FFrame &Stack,RESULT_DECL)
{
	P_GET_OBJECT(UClass, DesiredClass);
	P_GET_TARRAY_REF(OutputObjects,USequenceObject*);
	P_FINISH;

	check( DesiredClass->IsChildOf(USequenceObject::StaticClass()) );

	FindSeqObjectsByClass(DesiredClass, *OutputObjects);
}

FThumbnailDesc USequence::GetThumbnailDesc(FRenderInterface *InRI, FLOAT InZoom, INT InFixedSz)
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_AnimSet"), NULL, LOAD_NoFail, NULL);
	FThumbnailDesc	ThumbnailDesc;
	ThumbnailDesc.Width	= InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	ThumbnailDesc.Height = InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return ThumbnailDesc;
}

INT USequence::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();
	new( *InLabels )FString( GetName() );
	return InLabels->Num();
}

void USequence::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = LoadObject<UTexture2D>(NULL, TEXT("EngineMaterials.UnrealEdIcon_Sequence"), NULL, LOAD_NoFail, NULL);
	InRI->DrawTile( InX, InY, Icon->SizeX*InZoom, Icon->SizeY*InZoom, 0.0f,	0.0f, 1.0f, 1.0f, FLinearColor::White, Icon );
}

//==========================
// USequenceEvent interface

/**
 * Searches the owner chain looking for a player.
 */
static UBOOL IsPlayerOwned(AActor *inActor)
{
	AActor *testActor = inActor;
	while (testActor != NULL)
	{
		if (testActor->IsA(APlayerController::StaticClass()))
		{
			return 1;
		}
		testActor = testActor->Owner;
	}
	return 0;
}

/**
 * Checks if this event could be activated, and if bTest == false
 * then the event will be activated with the specified actor as the
 * instigator.
 * 
 * @param	inActor - actor to use as the instigator
 * 
 * @param	bTest - if true, then the event will not actually be
 * 			activated, only tested for success
 * 
 * @return	true if this event can be activated, or was activate if !bTest
 */
UBOOL USequenceEvent::CheckActivate(AActor *inOriginator,AActor *inInstigator,UBOOL bTest)
{
	UBOOL bActivated = 0;
	//ScriptLog(FString::Printf(TEXT("%s base check activate, %s/%s, triggercount: %d/%d, test: %s"),GetName(),inOriginator!=NULL?inOriginator->GetName():TEXT("NULL"),inInstigator!=NULL?inInstigator->GetName():TEXT("NULL"),TriggerCount,MaxTriggerCount,bTest?TEXT("yes"):TEXT("no")));
	// if passed a valid actor,
	// and meets player requirement
	// and match max trigger count condition
	// and retrigger delay condition
	if (inOriginator != NULL &&
		(!bPlayerOnly ||
		 IsPlayerOwned(inInstigator)) &&
		(MaxTriggerCount == 0 ||
		 TriggerCount < MaxTriggerCount) &&
		(ReTriggerDelay == 0.f ||
		 TriggerCount == 0 ||
		 (inOriginator->Level->TimeSeconds - ActivationTime) > ReTriggerDelay))
	{
		if (!bTest &&
			bEnabled)
		{
			// fill in any properties for this event
			Originator = inOriginator;
			Instigator = inInstigator;
			ActivationTime = inOriginator->Level->TimeSeconds;
			// increment the trigger count
			TriggerCount++;
			// activate this event
			bActive = 1;
			// see if any instigator variables are attached
			TArray<UObject**> objVars;
			GetObjectVars(objVars,TEXT("Instigator"));
			for (INT idx = 0; idx < objVars.Num(); idx++)
			{
				*(objVars(idx)) = inInstigator;
			}
			ScriptLog(FString(TEXT("- activated!")));
		}
		bActivated = 1;
	}
	return bActivated;
}


void USequenceEvent::execCheckActivate(FFrame &Stack, RESULT_DECL)
{
	P_GET_ACTOR(inOriginator);
	P_GET_ACTOR(inInstigator);
	P_GET_UBOOL_OPTX(bTest,0);
	P_FINISH;
	*(UBOOL*)Result = CheckActivate(inOriginator,inInstigator,bTest);
}

//==========================
// USeqEvent_Touch interface

/* epic ===============================================
* ::CheckActivate
*
* Checks whether or not the actor meets our criteria
* for a valid touch.
*
* =====================================================
*/
UBOOL USeqEvent_Touch::CheckActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest)
{
	UBOOL bPassed = 0;
	if (inInstigator != NULL)
	{
		ScriptLog(FString::Printf(TEXT("%s check activate, %s/%s"),GetName(),inOriginator->GetName(),inInstigator->GetName()));
		for (INT idx = 0; idx < ClassProximityTypes.Num() && !bPassed; idx++)
		{
			if (inInstigator->IsA(ClassProximityTypes(idx)))
			{
				bPassed = 1;
			}
		}
		if (bPassed)
		{
			//@fixme - temp fix for getting real instigator from projectiles
			AProjectile *proj = Cast<AProjectile>(inInstigator);
			if (proj != NULL &&
				proj->Instigator != NULL)
			{
				inInstigator = proj->Instigator;
			}
			bPassed = Super::CheckActivate(inOriginator,inInstigator,bTest);
		}
	}
	return bPassed;
}

//==========================
// USeqEvent_UnTouch interface

/* epic ===============================================
* ::CheckActivate
*
* Checks whether or not the actor meets our criteria
* for a valid untouch.
*
* =====================================================
*/
UBOOL USeqEvent_UnTouch::CheckActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest)
{
	UBOOL bPassed = 0;
	for (INT idx = 0; idx < ClassProximityTypes.Num() && !bPassed; idx++)
	{
		if (inInstigator->IsA(ClassProximityTypes(idx)))
		{
			bPassed = 1;
		}
	}
	if (bPassed)
	{
		bPassed = Super::CheckActivate(inOriginator,inInstigator,bTest);
	}
	return bPassed;
}

//==========================
// USequenceAction interface

/* epic ===============================================
* static GetHandlerName
*
* Takes a sequence action with a name ala "SeqAct_ActionName"
* and creates an FName for the handler "OnActionName".
*
* =====================================================
*/
static FName GetHandlerName(USequenceAction *inAction)
{
	FName handlerName = NAME_None;
	FString actionName(inAction->GetClass()->GetName());
	INT splitIdx = actionName.InStr(TEXT("_"),1);
	if (splitIdx != -1)
	{
		INT actionHandlers = 0;		//!!debug
		// build the handler func name "OnThisAction"
		actionName = FString::Printf(TEXT("On%s"),
									 *actionName.Mid(splitIdx+1,actionName.Len()));
		handlerName = FName(*actionName);
	}
	return handlerName;
}

struct FActionHandler_Parms
{
	UObject*		Action;
};

/* epic ===============================================
* ::Activated
*
* Handles calling the event on all linked object vars.
*
* =====================================================
*/
void USequenceAction::Activated()
{
	// split apart the action name,
	FName handlerName = GetHandlerName(this);
	if (handlerName != NAME_None)
	{
		INT actionHandlers = 0;		//!!debug
		// for each object variable associated with this action
		TArray<UObject**> objectVars;
		GetObjectVars(objectVars,TEXT("Target"));
		for (INT varIdx = 0; varIdx < objectVars.Num(); varIdx++)
		{
			UObject *obj = *(objectVars(varIdx));
			if (obj != NULL)
			{
				// look up the matching function
				UFunction *actionFunc = obj->FindFunction(handlerName);
				if (actionFunc == NULL)
				{
					// attempt to redirect between pawn/controller
					if (obj->IsA(APawn::StaticClass()) &&
						((APawn*)obj)->Controller != NULL)
					{
						obj = ((APawn*)obj)->Controller;
						actionFunc = obj->FindFunction(handlerName);
					}
					else
					if (obj->IsA(AController::StaticClass()) &&
						((AController*)obj)->Pawn != NULL)
					{
						obj = ((AController*)obj)->Pawn;
						actionFunc = obj->FindFunction(handlerName);
					}
				}
				if (actionFunc != NULL)
				{
					actionHandlers++;
					ScriptLog(FString::Printf(TEXT("--> Calling handler %s on actor %s"),actionFunc->GetName(),obj->GetName()));
					// perform any pre-handle logic
					AActor *targetActor = Cast<AActor>(obj);
					if (targetActor != NULL)
					{
						PreActorHandle(targetActor);
					}
					// call the actual function, with a pointer to this object as the only param
					FActionHandler_Parms parms;
					parms.Action = this;
					obj->ProcessEvent(actionFunc,&parms);
				}
				else
				{
					ScriptLog(FString::Printf(TEXT("Object %s has no handler for %s"),obj->GetName(),GetName()));
				}
			}
		}
		// warn if nothing handled this action
		if (actionHandlers == 0)
		{
			ScriptLog(FString::Printf(TEXT("No objects handled action %s"),GetName()));
		}
	}
	else
	{
		ScriptLog(FString::Printf(TEXT("Unable to determine action name for %s"),GetName()));
	}
	// explicitly clear inputs in case this action is activated multiple times in the same tick
	for (INT inputIdx = 0; inputIdx < InputLinks.Num(); inputIdx++)
	{
		InputLinks(inputIdx).bHasImpulse = 0;
	}
}

//==========================
// USeqAct_Latent interface

/* epic ===============================================
* ::PreActorHandle
*
* Called before an actor receives the handler function
* for this action, used to store a reference to the
* actor.
*
* =====================================================
*/
void USeqAct_Latent::PreActorHandle(AActor *inActor)
{
	if (inActor != NULL)
	{
		ScriptLog(FString::Printf(TEXT("--> Attaching latent action %s to actor %s"),GetName(),inActor->GetName()));
		LatentActors.AddItem(inActor);
		inActor->LatentActions.AddItem(this);
	}
}

void USeqAct_Latent::execAbortFor(FFrame &Stack, RESULT_DECL)
{
	P_GET_ACTOR(latentActor);
	P_FINISH;
	// make sure the actor exists in our list
	check(latentActor != NULL && "Trying abort latent action with a NULL actor");
	if (!bAborted)
	{
		UBOOL bFoundEntry = 0;
		ScriptLog(FString::Printf(TEXT("%s attempt abort by %s"),GetName(),latentActor->GetName()));
		for (INT idx = 0; idx < LatentActors.Num() && !bFoundEntry; idx++)
		{
			if (LatentActors(idx) == latentActor)
			{
				bFoundEntry = 1;
			}
		}
		if (bFoundEntry)
		{
			ScriptLog(FString::Printf(TEXT("%s aborted by %s"),GetName(),latentActor->GetName()));
			bAborted = 1;
		}
	}
}

/* epic ===============================================
* ::UpdateOp
*
* Latent actions don't complete until all actors that
* received the action have finished processing, or have
* been destroyed.
*
* =====================================================
*/
UBOOL USeqAct_Latent::UpdateOp(FLOAT deltaTime)
{										
	// check to see if this has been aborted
	if (bAborted)
	{
		// clear the latent actors list
		LatentActors.Empty();
	}
	else
	{
		// iterate through the latent actors list,
		for (INT idx = 0; idx < LatentActors.Num(); idx++)
		{
			// search for this action in the actor's latent list
			UBOOL bFoundMatch = 0;
			AActor *actor = LatentActors(idx);
			if (actor != NULL &&
				!actor->bDeleteMe)
			{
				for (INT actionIdx = 0; actionIdx < actor->LatentActions.Num() && !bFoundMatch; actionIdx++)
				{
					if (actor->LatentActions(actionIdx) == this)
					{
						bFoundMatch = 1;
					}
					else
					if (actor->LatentActions(actionIdx) == NULL)
					{
						actor->LatentActions.Remove(actionIdx--,1);
					}
				}
			}
			// if no match was found, remove the entry
			if (!bFoundMatch)
			{
				LatentActors.Remove(idx--,1);
			}
		}
	}
	// return true when our latentactors list is empty, to indicate all have finished processing
	return (LatentActors.Num() == 0);
}

void USeqAct_Latent::DeActivated()
{
	// if aborted then activate second link, otherwise default link
	OutputLinks(bAborted ? 1 : 0).bHasImpulse = 1;
	bAborted = 0;
}

//==========================
// USeqAct_Toggle interface

/* epic ===============================================
* ::Activated
*
* Overridden to check for any bools attached to the toggle
* action.  Also checks for any events and sets bEnabled
* accordingly.
*
* =====================================================
*/
void USeqAct_Toggle::Activated()
{
	// for each object variable associated with this action
	TArray<UBOOL*> boolVars;
	GetBoolVars(boolVars,TEXT("Bool"));
	for (INT varIdx = 0; varIdx < boolVars.Num(); varIdx++)
	{
		UBOOL *boolValue = boolVars(varIdx);
		if (boolValue != NULL)
		{
			// determine the new value for the variable
			if (InputLinks(0).bHasImpulse)
			{
				*boolValue = 1;
			}
			else
			if (InputLinks(1).bHasImpulse)
			{
				*boolValue = 0;
			}
			else
			if (InputLinks(2).bHasImpulse)
			{
				*boolValue = !(*boolValue);
			}
		}
	}
	// get a list of events
	for (INT idx = 0; idx < EventLinks(0).LinkedEvents.Num(); idx++)
	{
		USequenceEvent *event = EventLinks(0).LinkedEvents(idx);
		ScriptLog(FString::Printf(TEXT("---> event %s at %d"),event!=NULL?event->GetName():TEXT("NULL"),idx));
		if (event != NULL)
		{
			if (InputLinks(0).bHasImpulse)
			{
				ScriptLog(TEXT("----> enabled"));
				event->bEnabled = 1;
			}
			else
			if (InputLinks(1).bHasImpulse)
			{
				ScriptLog(TEXT("----> disabled"));
				event->bEnabled = 0;
			}
			else
			if (InputLinks(2).bHasImpulse)
			{
				event->bEnabled = !(event->bEnabled);
				ScriptLog(FString::Printf(TEXT("----> toggled, status? %s"),event->bEnabled?TEXT("Enabled"):TEXT("Disabled")));
			}
		}
	}
	// perform normal action activation
	Super::Activated();
}

//==========================
// USeqAct_CauseDamage interface

/* epic ===============================================
* ::Activated
*
* Calculates the total damage to apply and then passes
* off to normal execution.
*
* =====================================================
*/
void USeqAct_CauseDamage::Activated()
{
	// calculate damage to apply
	TArray<FLOAT*> floatVars;
	GetFloatVars(floatVars,TEXT("Amount"));
	DamageAmount = 0.f;
	for (INT varIdx = 0; varIdx < floatVars.Num(); varIdx++)
	{
		DamageAmount += *(floatVars(varIdx));
	}
	Super::Activated();
}

//==========================
// USequenceVariable

/** If we changed VarName, update NamedVar status globally. */
void USequenceVariable::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("VarName")) )
	{
		USequence* RootSeq = GetRootSequence();
		RootSeq->UpdateNamedVarStatus();
		RootSeq->UpdateInterpActionConnectors();
	}
}


//==========================
// USeqVar_Player interface

/**
 * If the current object value isn't a player it searches
 * through the level controller list for the first player.
 */
UObject** USeqVar_Player::GetObjectRef()
{
	// if we haven't already cached this value
	if (ObjValue == NULL ||
		!ObjValue->IsA(APlayerController::StaticClass()))
	{
		ULevel *level = GetLevel();
		if (level != NULL)
		{
			ObjValue = NULL;
			// iterate through the controller list
			for (AController *cont = level->GetLevelInfo()->ControllerList; cont != NULL && ObjValue == NULL; cont = cont->NextController)
			{
				// if it's a player cache it
				if (cont->IsA(APlayerController::StaticClass()))
				{
					//FIXME: match player index
					ObjValue = cont;
				}
			}
		}
	}
	return USeqVar_Object::GetObjectRef();
}

//==========================
// USeqVar_Object 

void USeqVar_Object::OnCreated()
{

}

//==========================
// USeqVar_External 


/** PostLoad to ensure colour is correct. */
void USeqVar_External::PostLoad()
{
	Super::PostLoad();

	if(ExpectedType)
	{
		ObjColor = ((USequenceVariable*)(ExpectedType->GetDefaultObject()))->ObjColor;
	}
}

/**
 * Overridden to update the expected variable type and parent
 * sequence connectors.
 */
void USeqVar_External::OnConnect(USequenceObject *connObj, INT connIdx)
{
	USequenceOp *op = Cast<USequenceOp>(connObj);
	if (op != NULL)
	{
		// figure out where we're connected, and what the type is
		INT varIdx = 0;
		if (op->VariableLinks(connIdx).LinkedVariables.FindItem(this,varIdx))
		{
			ExpectedType = op->VariableLinks(connIdx).ExpectedType;
			ObjColor = ((USequenceVariable*)(ExpectedType->GetDefaultObject()))->ObjColor;
		}
	}
	((USequence*)GetOuter())->UpdateConnectors();
	Super::OnConnect(connObj,connIdx);
}

FString USeqVar_External::GetValueStr()
{
	// if we have been linked, reflect that in the description
	if (ExpectedType != NULL &&
		ExpectedType != USequenceVariable::StaticClass())
	{
		return FString::Printf(TEXT("Ext. %s"),((USequenceObject*)ExpectedType->GetDefaultObject())->ObjName);
	}
	else
	{
		return FString(TEXT("Ext. ???"));
	}
}

//==========================
// USeqVar_Named

/** Overridden to update the expected variable type. */
void USeqVar_Named::OnConnect(USequenceObject *connObj, INT connIdx)
{
	USequenceOp *op = Cast<USequenceOp>(connObj);
	if (op != NULL)
	{
		// figure out where we're connected, and what the type is
		INT varIdx = 0;
		if (op->VariableLinks(connIdx).LinkedVariables.FindItem(this,varIdx))
		{
			ExpectedType = op->VariableLinks(connIdx).ExpectedType;
			ObjColor = ((USequenceVariable*)(ExpectedType->GetDefaultObject()))->ObjColor;
		}
	}
	Super::OnConnect(connObj,connIdx);
}

/** Text in Named variables is VarName we are linking to. */
FString USeqVar_Named::GetValueStr()
{
	// Show the VarName we this variable should link to.
	if(FindVarName != NAME_None)
	{
		return FString::Printf(TEXT("< %s >"), *FindVarName);
	}
	else
	{
		return FString(TEXT("< ??? >"));
	}
}

/** PostLoad to ensure colour is correct. */
void USeqVar_Named::PostLoad()
{
	Super::PostLoad();

	if(ExpectedType)
	{
		ObjColor = ((USequenceVariable*)(ExpectedType->GetDefaultObject()))->ObjColor;
	}
}

/** If we changed FindVarName, update NamedVar status globally. */
void USeqVar_Named::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("FindVarName")) )
	{
		USequence* RootSeq = GetRootSequence();
		RootSeq->UpdateNamedVarStatus();
		RootSeq->UpdateInterpActionConnectors();
	}
}

/** 
 *	Check this variable is ok, and set bStatusIsOk accordingly. 
 *	If FString pointer is supplied, will fill in the problem.
 */
void USeqVar_Named::UpdateStatus(FString* StatusMsg)
{
	// Do nothing if no variable or no type set.
	if(ExpectedType == NULL || FindVarName == NAME_None)
	{
		if(StatusMsg)
		{
			*StatusMsg = FString::Printf( TEXT("No FindVarName or ExpectedClass.") );
		}
		bStatusIsOk = false;
		return;
	}

	USequence* RootSeq = GetRootSequence();
	check(RootSeq);

	TArray<USequenceVariable*> Vars;
	RootSeq->FindNamedVariables(FindVarName, false, Vars);

	// Only assign if we get one result and its the right type. If not - do nothing and log the problem.
	if(Vars.Num() == 0)
	{
		if(StatusMsg)
		{
			*StatusMsg = FString::Printf(TEXT("No variable called '%s' found."), *FindVarName );
		}
		bStatusIsOk = false;
	}
	else if(Vars.Num() > 1)
	{
		if(StatusMsg)
		{
			*StatusMsg = FString::Printf( TEXT("Duplicate variables found with name '%s'"), *FindVarName );
		}
		bStatusIsOk = false;
	}
	else if( !Vars(0)->IsA(ExpectedType) )
	{
		if(StatusMsg)
		{
			*StatusMsg = FString::Printf( TEXT("Variable is of type '%s' instead of '%s'"), Vars(0)->GetClass()->GetName(), ExpectedType->GetName() );
		}
		bStatusIsOk = false;
	}
	else
	{
		if(StatusMsg)
		{
			*StatusMsg = FString( TEXT("Ok") );
		}
		bStatusIsOk = true;
	}
}


//==========================
// USeqAct_SetCameraTarget interface

/* epic ===============================================
* ::Activated
*
* Build the camera target then pass off to script handler.
*
* =====================================================
*/
void USeqAct_SetCameraTarget::Activated()
{
	// clear the previous target
	CameraTarget = NULL;
	// grab all object variables
	TArray<UObject**> objectVars;
	GetObjectVars(objectVars,TEXT("Cam Target"));
	for (INT varIdx = 0; varIdx < objectVars.Num() && CameraTarget == NULL; varIdx++)
	{
		// pick the first one
		CameraTarget = Cast<AActor>(*(objectVars(varIdx)));
	}
	Super::Activated();
}

//==========================
// USeqAct_Possess interface

/* epic ===============================================
* ::Activated
*
* Build the Pawn Target, then pass off to script handler
*
* =====================================================
*/
void USeqAct_Possess::Activated()
{
	// clear the previous target
	PawnToPossess = NULL;

	// grab all object variables
	TArray<UObject**> objectVars;
	GetObjectVars(objectVars,TEXT("Pawn Target"));
	for (INT varIdx = 0; varIdx < objectVars.Num(); varIdx++)
	{
		// pick the first one
		PawnToPossess = Cast<APawn>(*(objectVars(varIdx)));
		break;
	}
	Super::Activated();
}

//==========================
// USeqAct_ActorFactory interface

/**
 * Resets all transient properties for spawning.
 */
void USeqAct_ActorFactory::Activated()
{
	// reset all our transient properties
	RemainingDelay = 0.f;
	SpawnedCount = 0;
}

/**
 * Checks if the delay has been met, and creates a new
 * actor, choosing a spawn point and passing the values
 * to the selected factory.  Once an actor is created then
 * all object variables linked as "Spawned" are set to the
 * new actor, and the output links are activated.  The op
 * terminates once SpawnCount has been exceeded.
 * 
 * @param		deltaTime		time since last tick
 * @return						true to indicate that all
 * 								actors have been created
 */
UBOOL USeqAct_ActorFactory::UpdateOp(FLOAT deltaTime)
{
	if (Factory != NULL)
	{
		// if the delay has been exceeded
		if (RemainingDelay <= 0.f)
		{
			// build a list of valid spawn points
			TArray<UObject**> objVars;
			GetObjectVars(objVars,TEXT("Spawn Point"));
			TArray<AActor*> spawnPoints;
			for (INT idx = 0; idx < objVars.Num(); idx++)
			{
				AActor *testPoint = Cast<AActor>(*(objVars(idx)));
				if (testPoint != NULL &&
					!testPoint->bBlockActors)
				{
					spawnPoints.AddUniqueItem(testPoint);
				}
			}
			// process point selection if necessary
			switch (PointSelection)
			{
			case PS_Random:
				for (INT idx = 0; idx < spawnPoints.Num(); idx++)
				{
					INT newIdx = idx + (appRand()%(spawnPoints.Num()-idx));
					spawnPoints.SwapItems(newIdx,idx);
				}
				break;
			case PS_Normal:
				break;
			default:
				break;
			}

			// if a valid point was found,
			AActor *newSpawn = NULL;
			while (spawnPoints.Num() > 0 &&
				newSpawn == NULL)
			{
				AActor *point = spawnPoints.Pop();
				// attempt to create the new actor
				newSpawn = Factory->CreateActor(GetLevel(),&(point->Location),&(point->Rotation));
				// if we created the actor
				if (newSpawn != NULL)
				{
					ScriptLog(FString::Printf(TEXT("Spawned %s at %s"),newSpawn->GetName(),point->GetName()));
					// assign to any linked variables
					objVars.Empty();
					GetObjectVars(objVars,TEXT("Spawned"));
					for (INT idx = 0; idx < objVars.Num(); idx++)
					{
						*(objVars(idx)) = newSpawn;
					}
					// increment the spawned count
					SpawnedCount++;
					// activate all output impulses
					for (INT linkIdx = 0; linkIdx < OutputLinks.Num(); linkIdx++)
					{
						OutputLinks(linkIdx).bHasImpulse = 1;
					}
				}
			}

			// set a new delay
			RemainingDelay = SpawnDelay;
		}
		else
		{
			// reduce the delay
			RemainingDelay -= deltaTime;
		}
		// if we've spawned the limit return true to finish the op
		return (SpawnedCount >= SpawnCount);
	}
	else
	{
		debugf(NAME_Warning,TEXT("Actor factory action %s has an invalid factory!"),GetFullName());
		return 1;
	}
}

void USeqAct_ActorFactory::DeActivated()
{
	// do nothing, since outputs have already been activated
}

/**
 * Activates the associated output link of the outer sequence.
 */
void USeqAct_FinishSequence::Activated()
{
	USequence *seq = Cast<USequence>(GetOuter());
	if (seq != NULL)
	{
		// iterate through output links, looking for a match
		for (INT idx = 0; idx < seq->OutputLinks.Num(); idx++)
		{
			if (seq->OutputLinks(idx).LinkAction == GetName())
			{
				seq->OutputLinks(idx).bHasImpulse = 1;
				break;
			}
		}
	}
}

/**
 * Force parent sequence to update connector links.
 */
void USeqAct_FinishSequence::PostEditChange(UProperty* PropertyThatChanged)
{
	USequence *seq = Cast<USequence>(GetOuter());
	if (seq != NULL)
	{
		seq->UpdateConnectors();
	}
}

/**
 * Updates the links on the outer sequence.
 */
void USeqAct_FinishSequence::OnCreated()
{
	USequence *seq = Cast<USequence>(GetOuter());
	if (seq != NULL)
	{
		seq->UpdateConnectors();
	}
}

/**
 * Verifies max interact distance, then performs normal event checks.
 */
UBOOL USeqEvent_Used::CheckActivate(AActor *inOriginator, AActor *inInstigator, UBOOL bTest)
{
	UBOOL bActivated = 0;
	if ((inOriginator->Location - inInstigator->Location).Size() <= InteractDistance)
	{
		bActivated = Super::CheckActivate(inOriginator,inInstigator,bTest);
		if (bActivated &&
			inOriginator != NULL &&
			inInstigator != NULL)
		{
			// set the used distance
			TArray<FLOAT*> floatVars;
			GetFloatVars(floatVars,TEXT("Distance"));
			if (floatVars.Num() > 0)
			{
				FLOAT distance = (inInstigator->Location-inOriginator->Location).Size();
				for (INT idx = 0; idx < floatVars.Num(); idx++)
				{
					*(floatVars(idx)) = distance;
				}
			}
		}
	}
	return bActivated;
}

//==========================
// USeqAct_Log interface

/* epic ===============================================
* ::Activated
*
* Dumps all attached variables to the log.
*
* =====================================================
*/
void USeqAct_Log::Activated()
{
#ifndef XBOX
	//@todo xenon: this causes crashes with the XeReleaseLTCG configuration.
	// for all attached variables
	FString logString(TEXT("Kismet: "));
	for (INT varIdx = 0; varIdx < VariableLinks.Num(); varIdx++)
	{
		for (INT idx = 0; idx < VariableLinks(varIdx).LinkedVariables.Num(); idx++)
		{
			if (VariableLinks(varIdx).LinkedVariables(idx) != NULL)
			{
				USeqVar_RandomInt *randInt = Cast<USeqVar_RandomInt>(VariableLinks(varIdx).LinkedVariables(idx));
				if (randInt != NULL)
				{
					INT *intValue = randInt->GetIntRef();
					logString = FString::Printf(TEXT("%s %d"),*logString,*intValue);
				}
				else
				{
					logString += VariableLinks(varIdx).LinkedVariables(idx)->GetValueStr();
					logString += TEXT(" ");
				}
			}
		}
	}
	debugf(NAME_Log,TEXT("%s"),logString);
	if (bOutputToScreen)
	{
		ULevel *level = GetLevel();
		if (level != NULL)
		{
			// iterate through the controller list
			for (AController *cont = level->GetLevelInfo()->ControllerList; cont != NULL; cont = cont->NextController)
			{
				// if it's a player cache it
				if (cont->IsA(APlayerController::StaticClass()))
				{
					((APlayerController*)cont)->eventClientMessage(logString,NAME_None);
				}
			}
		}
	}
#endif
}

//==========================
// USeqCond_Increment interface

/**
 * Compares the sum of all variables attached at "A" to the sum of all
 * variables attached at "B", and activates the corresponding output
 * link.  If no variables are attached at either connector, then the default
 * value is used instead.  All variables attached to "A" will be incremented
 * by IncrementAmount before the comparison.
 */
void USeqCond_Increment::Activated()
{
	// grab our two int variables
	INT value1 = 0;
	INT value2 = 0;
	TArray<INT*> intValues1;
	GetIntVars(intValues1,TEXT("A"));
	TArray<INT*> intValues2;
	GetIntVars(intValues2,TEXT("B"));
	// increment all of the counter variables
	if (intValues1.Num() > 0)
	{
		for (INT varIdx = 0; varIdx < intValues1.Num(); varIdx++)
		{
			*(intValues1(varIdx)) += IncrementAmount;
			value1 += *(intValues1(varIdx));
		}
	}
	else
	{
		value1 = DefaultA;
		DefaultA += IncrementAmount;
	}
	if (intValues2.Num() > 0)
	{
		// get the actual values by adding up all linked variables
		for (INT varIdx = 0; varIdx < intValues2.Num(); varIdx++)
		{
			value2 += *(intValues2(varIdx));
		}
	}
	else
	{
		// use default value
		value2 = DefaultB;
	}
	// compare the values and set output impulse
	OutputLinks(value1 <= value2 ? 0 : 1).bHasImpulse = 1;
}

/**
 * Compares the sum of all variables attached at "A" to the sum of all
 * variables attached at "B", and activates the corresponding output
 * link.  If no variables are attached at either connector, then the default
 * value is used instead.
 */
void USeqCond_CompareInt::Activated()
{
	// grab our two int variables
	INT value1 = 0;
	INT value2 = 0;
	TArray<INT*> intValues1;
	GetIntVars(intValues1,TEXT("A"));
	TArray<INT*> intValues2;
	GetIntVars(intValues2,TEXT("B"));
	if (intValues1.Num() > 0)
	{
		for (INT varIdx = 0; varIdx < intValues1.Num(); varIdx++)
		{
			value1 += *(intValues1(varIdx));
		}
	}
	else
	{
		value1 = DefaultA;
	}
	if (intValues2.Num() > 0)
	{
		// get the actual values by adding up all linked variables
		for (INT varIdx = 0; varIdx < intValues2.Num(); varIdx++)
		{
			value2 += *(intValues2(varIdx));
		}
	}
	else
	{
		value2 = DefaultB;
	}
	// compare the values and set output impulse
	OutputLinks(value1 <= value2 ? 0 : 1).bHasImpulse = 1;
}

/**
 * Compares the sum of all variables attached at "A" to the sum of all
 * variables attached at "B", and activates the corresponding output
 * link.  If no variables are attached at either connector, then the default
 * value is used instead.
 */
void USeqCond_CompareFloat::Activated()
{
	// grab our two float variables
	FLOAT value1 = 0;
	FLOAT value2 = 0;
	TArray<FLOAT*> floatValues1;
	GetFloatVars(floatValues1,TEXT("A"));
	TArray<FLOAT*> floatValues2;
	GetFloatVars(floatValues2,TEXT("B"));
	if (floatValues1.Num() > 0)
	{
		for (INT varIdx = 0; varIdx < floatValues1.Num(); varIdx++)
		{
			value1 += *(floatValues1(varIdx));
		}
	}
	else
	{
		value1 = DefaultA;
	}
	if (floatValues2.Num() > 0)
	{
		// get the actual values by adding up all linked variables
		for (INT varIdx = 0; varIdx < floatValues2.Num(); varIdx++)
		{
			value2 += *(floatValues2(varIdx));
		}
	}
	else
	{
		value2 = DefaultB;
	}
	// compare the values and set output impulse
	OutputLinks(value1 <= value2 ? 0 : 1).bHasImpulse = 1;
}

//============================
// USeqAct_Delay interface

/* epic ===============================================
* ::Activated
*
* Builds the remaining time by adding all attached floats.
*
* =====================================================
*/
void USeqAct_Delay::Activated()
{
	check(VariableLinks.Num() >= 1 && "Delay requires at least one float for duration");
	// grab our duration variable
	RemainingTime = 0.f;
	TArray<FLOAT*> floatVars;
	GetFloatVars(floatVars);
	if (floatVars.Num() > 0)
	{
		for (INT varIdx = 0; varIdx < floatVars.Num(); varIdx++)
		{
			RemainingTime += *(floatVars(varIdx));
		}
	}
	else
	{
		RemainingTime = DefaultDuration;
	}
}

/* epic ===============================================
* ::UpdateOp
*
* Determines whether or not we've exceeded the end time.
*
* =====================================================
*/
UBOOL USeqAct_Delay::UpdateOp(FLOAT deltaTime)
{
	RemainingTime -= deltaTime;
	return (RemainingTime <= 0.f);
}

//============================
// USeqCond_CompareBool interface

/* epic ===============================================
* ::Activated
*
* Compares the bools linked to the condition, directing
* output based on the boolean result.
*
* =====================================================
*/
void USeqCond_CompareBool::Activated()
{
	UBOOL bResult = 1;
	// iterate through each of the linked bool variables
	TArray<UBOOL*> boolVars;
	GetBoolVars(boolVars,TEXT("Bool"));
	for (INT varIdx = 0; varIdx < boolVars.Num(); varIdx++)
	{
		bResult = bResult && *(boolVars(varIdx));
	}
	// activate the proper output based on the result value
	OutputLinks(bResult ? 0 : 1).bHasImpulse = 1;
}

//============================
// USeqAct_PlaySound interface

/* epic ===============================================
* ::Activated
*
* Plays the given soundcue on all attached actors.
*
* =====================================================
*/
void USeqAct_PlaySound::Activated()
{
	TArray<UObject**> objVars;
	GetObjectVars(objVars,TEXT("Target"));
	for (INT varIdx = 0; varIdx < objVars.Num(); varIdx++)
	{
		AActor *target = Cast<AActor>(*(objVars(varIdx)));
		if (target != NULL)
		{
			UAudioDevice::CreateComponent(PlaySound,target->GetLevel(),target,1);
		}
	}
}

/**
 * Applies new value to all variables attached to "Target" connector, using
 * either the variables attached to "Value" or DefaultValue.
 */
void USeqAct_SetBool::Activated()
{
	// read new value
	UBOOL bValue = 1;
	TArray<UBOOL*> boolVars;
	GetBoolVars(boolVars,TEXT("Value"));
	if (boolVars.Num() > 0)
	{
		for (INT idx = 0; idx < boolVars.Num(); idx++)
		{
			bValue = bValue && *(boolVars(idx));
		}
	}
	else
	{
		// no attached variables, use default value
		bValue = DefaultValue;
	}
	// and apply the new value
	boolVars.Empty();
	GetBoolVars(boolVars,TEXT("Target"));
	for (INT idx = 0; idx < boolVars.Num(); idx++)
	{
		*(boolVars(idx)) = bValue;
	}
}

/**
 * Applies new value to all variables attached to "Target" connector, using
 * either the variables attached to "Value" or DefaultValue.
 */
void USeqAct_SetInt::Activated()
{
	// read new value
	INT value = 0;
	TArray<INT*> intVars;
	GetIntVars(intVars,TEXT("Value"));
	if (intVars.Num() > 0)
	{
		for (INT idx = 0; idx < intVars.Num(); idx++)
		{
			value = value + *(intVars(idx));
		}
	}
	else
	{
		// no attached variables, use default value
		value = DefaultValue;
	}
	// and apply the new value
	intVars.Empty();
	GetIntVars(intVars,TEXT("Target"));
	for (INT idx = 0; idx < intVars.Num(); idx++)
	{
		*(intVars(idx)) = value;
	}
}

/**
 * Applies new value to all variables attached to "Target" connector, using
 * either the variables attached to "Value" or DefaultValue.
 */
void USeqAct_SetFloat::Activated()
{
	// read new value
	FLOAT value = 0;
	TArray<FLOAT*> floatVars;
	GetFloatVars(floatVars,TEXT("Value"));
	if (floatVars.Num() > 0)
	{
		for (INT idx = 0; idx < floatVars.Num(); idx++)
		{
			value = value + *(floatVars(idx));
		}
	}
	else
	{
		// no attached variables, use default value
		value = DefaultValue;
	}
	// and apply the new value
	floatVars.Empty();
	GetFloatVars(floatVars,TEXT("Target"));
	for (INT idx = 0; idx < floatVars.Num(); idx++)
	{
		*(floatVars(idx)) = value;
	}
}


/**
 * Applies new value to all variables attached to "Target" connector, using
 * either the variables attached to "Value" or DefaultValue.
 */
void USeqAct_SetObject::Activated()
{
	// read new value
	UObject* value = NULL;
	TArray<UObject**> objectVars;
	GetObjectVars(objectVars,TEXT("Value"));
	if (objectVars.Num() > 0)
	{
		for (INT idx = 0; idx < objectVars.Num(); idx++)
		{
			// only accept if it's a !null
			if (*(objectVars(idx)) != NULL)
			{
				value = *(objectVars(idx));
			}
		}
	}
	else
	{
		// no attached variables, use default value
		value = DefaultValue;
	}
	ScriptLog(FString::Printf(TEXT("New set object value: %s"),value!=NULL?value->GetName():TEXT("NULL")));
	// and apply the new value
	objectVars.Empty();
	GetObjectVars(objectVars,TEXT("Target"));
	for (INT idx = 0; idx < objectVars.Num(); idx++)
	{
		*(objectVars(idx)) = value;
	}
}

//============================
// USeqAct_AttachToEvent interface

/* epic ===============================================
* ::Activated
*
* Attaches the actor to a given event, well actually
* it adds the event to the actor, but it's basically
* the same thing, sort of.
*
* =====================================================
*/
void USeqAct_AttachToEvent::Activated()
{
	// get a list of actors
	TArray<UObject**> objVars;
	GetObjectVars(objVars,TEXT("Attachee"));
	TArray<AActor*> targets;
	for (INT idx = 0; idx < objVars.Num(); idx++)
	{
		AActor *actor = Cast<AActor>(*(objVars(idx)));
		if (actor != NULL)
		{
			// if target is a controller, try to use the pawn
			if (actor->IsA(AController::StaticClass()) &&
				((AController*)actor)->Pawn != NULL)
			{
				targets.AddUniqueItem(((AController*)actor)->Pawn);
			}
			else
			{
				targets.AddUniqueItem(actor);
			}
		}
	}
	// get a list of events
	TArray<USequenceEvent*> events;
	for (INT idx = 0; idx < EventLinks(0).LinkedEvents.Num(); idx++)
	{
		USequenceEvent *event = EventLinks(0).LinkedEvents(idx);
		if (event != NULL)
		{
			events.AddUniqueItem(event);
		}
	}
	// if we actually have actors and events,
	if (targets.Num() > 0 &&
		events.Num() > 0)
	{
		// get the outer sequence
		UObject *outer = GetOuter();
		USequence *seq = NULL;
		while (seq == NULL &&
			   outer != NULL)
		{
			seq = Cast<USequence>(outer);
			outer = outer->GetOuter();
		}
		// then add the events to the targets
		for (INT idx = 0; idx < targets.Num(); idx++)
		{
			for (INT eventIdx = 0; eventIdx < events.Num(); eventIdx++)
			{
				// create a duplicate of the event to avoid collision issues
				USequenceEvent *evt = (USequenceEvent*)StaticConstructObject(events(eventIdx)->GetClass(),seq,NAME_None,0,events(eventIdx));
				seq->SequenceObjects.AddItem(evt);
				evt->Originator = targets(idx);
				targets(idx)->GeneratedEvents.AddItem(evt);
			}
		}
	}
	else
	{
		if (targets.Num() == 0)
		{
			debugf(NAME_Warning,TEXT("Attach to event %s has no targets!"),GetName());
		}
		if (events.Num() == 0)
		{
			debugf(NAME_Warning,TEXT("Attach to event %s has no events!"),GetName());
		}
	}
}

/**
 * Overloaded to see if any new inputs have been activated for this
 * op, so as to add them to the list.
 */
UBOOL USeqAct_AIMoveToActor::UpdateOp(FLOAT deltaTime)
{
	UBOOL bNewInput = 0;
	for (INT idx = 0; idx < InputLinks.Num() && bNewInput == 0; idx++)
	{
		if (InputLinks(idx).bHasImpulse)
		{
			ScriptLog(FString::Printf(TEXT("Latent move %s detected new input"),GetName()));
			bNewInput = 1;
		}
	}
	if (bNewInput)
	{
		Activated();
	}
	return Super::UpdateOp(deltaTime);
}

/**
 * Force parent sequence to update connector links.
 */
void USeqEvent_SequenceActivated::PostEditChange(UProperty* PropertyThatChanged)
{
	USequence *seq = Cast<USequence>(GetOuter());
	if (seq != NULL)
	{
		seq->UpdateConnectors();
	}
}

/**
 * Updates the links on the outer sequence.
 */
void USeqEvent_SequenceActivated::OnCreated()
{
	USequence *seq = Cast<USequence>(GetOuter());
	if (seq != NULL)
	{
		seq->UpdateConnectors();
	}
}

/**
 * Calculates the average positions of all A objects, then gets the
 * distance from the average position of all B objects, and places
 * the results in all the attached Distance floats.
 */
void USeqAct_GetDistance::Activated()
{
	TArray<UObject**> aObjs, bObjs;
	GetObjectVars(aObjs,TEXT("A"));
	GetObjectVars(bObjs,TEXT("B"));
	TArray<FLOAT*> distanceVars;
	GetFloatVars(distanceVars,TEXT("Distance"));
	if (aObjs.Num() > 0 &&
		bObjs.Num() > 0 &&
		distanceVars.Num() > 0)
	{
		// get the average position of A
		FVector avgALoc(0,0,0);
		INT actorCnt = 0;
		for (INT idx = 0; idx < aObjs.Num(); idx++)
		{
			AActor *testActor = Cast<AActor>(*(aObjs(idx)));
			if (testActor != NULL)
			{
				if (testActor->IsA(AController::StaticClass()) &&
					((AController*)testActor)->Pawn != NULL)
				{
					// use the pawn instead of the controller
					testActor = ((AController*)testActor)->Pawn;
				}
				avgALoc += testActor->Location;
				actorCnt++;
			}
		}
		if (actorCnt > 0)
		{
			avgALoc /= actorCnt;
		}
		// and average position of B
		FVector avgBLoc(0,0,0);
		actorCnt = 0;
		for (INT idx = 0; idx < bObjs.Num(); idx++)
		{
			AActor *testActor = Cast<AActor>(*(bObjs(idx)));
			if (testActor != NULL)
			{
				if (testActor->IsA(AController::StaticClass()) &&
					((AController*)testActor)->Pawn != NULL)
				{
					// use the pawn instead of the controller
					testActor = ((AController*)testActor)->Pawn;
				}
				avgBLoc += testActor->Location;
				actorCnt++;
			}
		}
		if (actorCnt > 0)
		{
			avgBLoc /= actorCnt;
		}
		// figure out the distance
		FLOAT dist = (avgALoc - avgBLoc).Size();
		// write out the result
		for (INT idx = 0; idx < distanceVars.Num(); idx++)
		{
			*(distanceVars(idx)) = dist;
		}
	}
}

/** Takes the magnitude of the velocity of all attached Target Actors and sets the output Velocity float variable to the total. */
void USeqAct_GetVelocity::Activated()
{
	TArray<UObject**> Objs;
	GetObjectVars(Objs,TEXT("Target"));

	FLOAT TotalVel = 0.f;
	for(INT i=0; i<Objs.Num(); i++)
	{
		AActor* TestActor = Cast<AActor>( *(Objs(i)) );
		if(TestActor)
		{
			// use the pawn instead of the controller
			AController* C = Cast<AController>(TestActor);
			if(C && C->Pawn)
			{
				TestActor = C->Pawn;
			}

			TotalVel += TestActor->Velocity.Size();
		}
	}

	TArray<FLOAT*> VelVars;
	GetFloatVars(VelVars,TEXT("Velocity"));

	for(INT i=0; i<VelVars.Num(); i++)
	{
		*(VelVars(i)) = TotalVel;
	}
}

/**
 * Grabs the list of all attached objects, and then attempts to import any specified property
 * values for each one.
 */
void USeqAct_ModifyProperty::Activated()
{
	if (ObjectClass != NULL &&
		Properties.Num() > 0)
	{
		// for each attached object,
		TArray<UObject**> objVars;
		GetObjectVars(objVars,TEXT("Target"));
		TArray<UObject*> objList;
		// process the list of objects, adding in special cases (such as pawn/controller)
		for (INT idx = 0; idx < objVars.Num(); idx++)
		{
			UObject *obj = *(objVars(idx));
			if (obj != NULL)
			{
				objList.AddUniqueItem(obj);
				if (obj->IsA(APawn::StaticClass()))
				{
					APawn *pawn = (APawn*)(obj);
					if (pawn->Controller != NULL)
					{
						objList.AddUniqueItem(pawn->Controller);
					}
				}
				else
				if (obj->IsA(AController::StaticClass()))
				{
					AController *controller = (AController*)(obj);
					if (controller->Pawn != NULL)
					{
						objList.AddUniqueItem(controller->Pawn);
					}
				}
			}
		}
		// for each object found,
		for (INT idx = 0; idx < objList.Num(); idx++)
		{
			UObject *obj = objList(idx);
			// of the correct type,
			if (obj->IsA(ObjectClass))
			{
				// iterate through each property
				for (INT propIdx = 0; propIdx < Properties.Num(); propIdx++)
				{
					if (Properties(propIdx).bModifyProperty)
					{
						// find the property field
						UProperty *prop = Cast<UProperty>(obj->FindObjectField(Properties(propIdx).PropertyName,1));
						if (prop != NULL)
						{
							// import the property text for the new object
							prop->ImportText(*(Properties(propIdx).PropertyValue),(BYTE*)obj + prop->Offset,0,NULL);
						}
					}
				}
			}
		}
	}
}

/**
 * Checks to see if the property list should be autofilled with editable properties
 * of the selected class.
 */
void USeqAct_ModifyProperty::PostEditChange(UProperty *PropertyThatChanged)
{
	if (bAutoFill &&
		PropertyThatChanged != NULL &&
		PropertyThatChanged->GetFName() == FName(TEXT("ObjectClass"),FNAME_Find) &&
		ObjectClass != PrevObjectClass)
	{
		// rebuild the property list
		PrevObjectClass = ObjectClass;
		Properties.Empty();
		if (ObjectClass != NULL)
		{
			UObject *defaultObj = ObjectClass->GetDefaultObject();
			// iterate through all properties, looking for editable ones
			for (TFieldIterator<UProperty> It(ObjectClass); It; ++It)
			{
				if ((It->PropertyFlags & CPF_Edit) &&
					!(It->PropertyFlags & CPF_EditConst) &&
					!(It->PropertyFlags & CPF_Const))
				{
					// create a new entry
					INT idx = Properties.AddZeroed();
					Properties(idx).PropertyName = It->GetFName();
					// and grab the default value
					It->ExportTextItem(Properties(idx).PropertyValue,(BYTE*)defaultObj + It->Offset,NULL,0);
				}
			}
		}
	}
	Super::PostEditChange(PropertyThatChanged);
}
