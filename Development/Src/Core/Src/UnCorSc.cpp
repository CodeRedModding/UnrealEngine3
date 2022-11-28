/*=============================================================================
	UnCorSc.cpp: UnrealScript execution and support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	UnrealScript execution and support code.

Revision history:
	* Created by Tim Sweeney 

=============================================================================*/

#include "CorePrivate.h"
#include "OpCode.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

void (UObject::*GNatives[EX_Max])( FFrame &Stack, RESULT_DECL );
INT GNativeDuplicate=0;

void (UObject::*GCasts[CST_Max])( FFrame &Stack, RESULT_DECL );
INT GCastDuplicate=0;

#define RUNAWAY_LIMIT 1000000
#define RECURSE_LIMIT 250

#if DO_GUARD
	static INT Runaway=0;
	static INT Recurse=0;
	#define CHECK_RUNAWAY {if( ++Runaway > RUNAWAY_LIMIT ) {if(!ParseParam(appCmdLine(),TEXT("norunaway"))&&(!GDebugger||GDebugger->NotifyInfiniteLoop())) Stack.Logf( NAME_Critical, TEXT("Runaway loop detected (over %i iterations)"), RUNAWAY_LIMIT ); Runaway=0;}}
	void GInitRunaway() {Recurse=Runaway=0;}
#else
	#define CHECK_RUNAWAY
	void GInitRunaway() {}
#endif

#define IMPLEMENT_CAST_FUNCTION(cls,num,func) \
		IMPLEMENT_FUNCTION(cls,-1,func); \
		static BYTE cls##func##CastTemp = GRegisterCast( num, int##cls##func );
 
TCHAR* GetOpCodeName( BYTE OpCode )
{
	switch ( OpCode )
	{
	case DI_Let:				return TEXT("Let");
	case DI_SimpleIf:			return TEXT("SimpleIf");
	case DI_Switch:				return TEXT("Switch");
	case DI_While:				return TEXT("While");
	case DI_Assert:				return TEXT("Assert");
	case DI_Return:				return TEXT("Return");
	case DI_ReturnNothing:		return TEXT("ReturnNothing");
	case DI_NewStack:			return TEXT("NewStack");
	case DI_NewStackLatent:		return TEXT("NewStackLatent");
	case DI_NewStackLabel:		return TEXT("NewStackLabel");
	case DI_PrevStack:			return TEXT("PrevStack");
	case DI_PrevStackLatent:	return TEXT("PrevStackLatent");
	case DI_PrevStackLabel:		return TEXT("PrevStackLabel");
	case DI_PrevStackState:		return TEXT("PrevStackState");
	case DI_EFP:				return TEXT("EFP");
	case DI_EFPOper:			return TEXT("EFPOper");
	case DI_EFPIter:			return TEXT("EFPIter");
	case DI_ForInit:			return TEXT("ForInit");
	case DI_ForEval:			return TEXT("ForEval");
	case DI_ForInc:				return TEXT("ForInc");
	case DI_BreakLoop:			return TEXT("BreakLoop");
	case DI_BreakFor:			return TEXT("BreakFor");
	case DI_BreakForEach:		return TEXT("BreakForEach");
	case DI_BreakSwitch:		return TEXT("BreakSwitch");
	case DI_ContinueLoop:		return TEXT("ContinueLoop");
	case DI_ContinueForeach:	return TEXT("ContinueForeach");	
	case DI_ContinueFor:		return TEXT("ContinueFor");
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

//
// Error or warning handler.
//
void FFrame::Serialize( const TCHAR* V, EName Event )
{
	if( Event==NAME_Critical ) appErrorf
	(
		TEXT("%s (%s:%04X) %s"),
		*Object->GetFullName(),
		*Node->GetFullName(),
		Code - &Node->Script(0),
		V
	);
	else debugf
	(
		NAME_ScriptWarning,
		TEXT("%s (%s:%04X) %s"),
		*Object->GetFullName(),
		*Node->GetFullName(),
		Code - &Node->Script(0),
		V
	);
}

/*-----------------------------------------------------------------------------
	Global script execution functions.
-----------------------------------------------------------------------------*/

//
// Have an object go to a named state, and idle at no label.
// If state is NAME_None or was not found, goes to no state.
// Returns 1 if we went to a state, 0 if went to no state.
//
EGotoState UObject::GotoState( FName NewState, UBOOL bForceEvents )
{
	// check to see if this object even supports states
	if (StateFrame == NULL) 
	{
		return GOTOSTATE_NotFound;
	}
	// find the new state
	UState* StateNode = NULL;
	FName OldStateName = StateFrame->StateNode != Class ? StateFrame->StateNode->GetFName() : FName(NAME_None);
	if (NewState != NAME_Auto) 
	{
		// Find regular state.
		StateNode = FindState( NewState );
	}
	else
	{
		// find the auto state.
        for( TFieldIterator<UState,CLASS_IsAUState> It(GetClass()); It && !StateNode; ++It )
		{
			if( It->StateFlags & STATE_Auto )
			{
				StateNode = *It;
			}
		}
	}
	// if no state was found, then go to the default class state
	if (!StateNode) 
	{
		NewState  = NAME_None;
		StateNode = GetClass();
	}
	else
	if (NewState == NAME_Auto)
	{
		// going to auto state.
		NewState = StateNode->GetFName();
	}
	// Send EndState notification.
	if (bForceEvents ||
		(OldStateName!=NAME_None && 
		 NewState!=OldStateName &&
		 IsProbing(NAME_EndState) &&
		 !(GetFlags() & RF_InEndState)))
	{
		ClearFlags( RF_StateChanged );
		SetFlags( RF_InEndState );
		eventEndState();
		ClearFlags( RF_InEndState );
		if( GetFlags() & RF_StateChanged )
		{
			return GOTOSTATE_Preempted;
		}
	}
	// clear the nested state stack
	StateFrame->StateStack.Empty();
	// clear the previous latent action if any
	StateFrame->LatentAction = 0;
	// set the new state
	StateFrame->Node = StateNode;
	StateFrame->StateNode = StateNode;
	StateFrame->Code = NULL;
	StateFrame->ProbeMask = (StateNode->ProbeMask | GetClass()->ProbeMask) & StateNode->IgnoreMask;
	// Send BeginState notification.
	if (bForceEvents ||
		(NewState!=NAME_None &&
		 NewState!=OldStateName &&
		 IsProbing(NAME_BeginState)))
	{
		ClearFlags( RF_StateChanged );
		eventBeginState();
		// check to see if a new state change was instigated
		if (GetFlags() & RF_StateChanged) 
		{
			return GOTOSTATE_Preempted;
		}
	}
	// Return result.
	if( NewState != NAME_None )
	{
		SetFlags(RF_StateChanged);
		return GOTOSTATE_Success;
	}
	return GOTOSTATE_NotFound;
}

//
// Goto a label in the current state.
// Returns 1 if went, 0 if not found.
//
UBOOL UObject::GotoLabel( FName FindLabel )
{
	if( StateFrame )
	{
		StateFrame->LatentAction = 0;
		if( FindLabel != NAME_None )
		{
			for( UState* SourceState=StateFrame->StateNode; SourceState; SourceState=SourceState->GetSuperState() )
			{
				if( SourceState->LabelTableOffset != MAXWORD )
				{
					for( FLabelEntry* Label = (FLabelEntry *)&SourceState->Script(SourceState->LabelTableOffset); Label->Name!=NAME_None; Label++ )
					{
						if( Label->Name==FindLabel )
						{
							StateFrame->Node = SourceState;
							StateFrame->Code = &SourceState->Script(Label->iCode);
							return 1;
						}
					}
				}
			}
		}
		StateFrame->Code = NULL;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
	Natives.
-----------------------------------------------------------------------------*/

//////////////////////////////
// Undefined native handler //
//////////////////////////////

void UObject::ExecutingBadStateCode(FFrame& Stack) 
{
	Stack.Logf( NAME_Critical, TEXT("Unknown code token %02X"), Stack.Code[-1] );
}

void UObject::execUndefined( FFrame& Stack, RESULT_DECL  )
{
	ExecutingBadStateCode(Stack);
}

///////////////
// Variables //
///////////////

void UObject::execLocalVariable( FFrame& Stack, RESULT_DECL )
{
	checkSlow(Stack.Object==this);
	checkSlow(Stack.Locals!=NULL);
	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = Stack.Locals + GProperty->Offset;
	GPropObject = NULL;
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );
}
IMPLEMENT_FUNCTION( UObject, EX_LocalVariable, execLocalVariable );

void UObject::execInstanceVariable( FFrame& Stack, RESULT_DECL)
{
	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = (BYTE*)this + GProperty->Offset;
	GPropObject = this;
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );
}
IMPLEMENT_FUNCTION( UObject, EX_InstanceVariable, execInstanceVariable );

void UObject::execDefaultVariable( FFrame& Stack, RESULT_DECL )
{
	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = &GetClass()->Defaults(GProperty->Offset);
	GPropObject = NULL;
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );
}
IMPLEMENT_FUNCTION( UObject, EX_DefaultVariable, execDefaultVariable );

void UObject::execClassContext( FFrame& Stack, RESULT_DECL )
{
	// Get class expression.
	UClass* ClassContext=NULL;
	Stack.Step( Stack.Object, &ClassContext );

	// Execute expression in class context.
	if( ClassContext )
	{
		Stack.Code += 3;
		Stack.Step( ClassContext->GetDefaultObject(), Result );
	}
	else
	{
		if ( GProperty )
		{
			Stack.Logf( NAME_Error, TEXT("Accessed null class context '%s'"), GProperty->GetName() );
		}
		else
		{ 
			Stack.Logf( NAME_Error, TEXT("Accessed null class context") );
		}

		if ( GDebugger )
			GDebugger->NotifyAccessedNone();

		INT wSkip = Stack.ReadWord();
		BYTE bSize = *Stack.Code++;
		Stack.Code += wSkip;
		GPropAddr = NULL;
		GPropObject = NULL;
		GProperty = NULL;
		if( Result )
			appMemzero( Result, bSize );
	}
}
IMPLEMENT_FUNCTION( UObject, EX_ClassContext, execClassContext );

void UObject::execArrayElement( FFrame& Stack, RESULT_DECL )
{
	// Get array index expression.
	INT Index=0;
	Stack.Step( Stack.Object, &Index );

	// Get base element (must be a variable!!).
	GProperty = NULL;
	Stack.Step( this, NULL );
	GPropObject = this;

	// Add scaled offset to base pointer.
	if( GProperty && GPropAddr )
	{
		// Bounds check.
		if( Index>=GProperty->ArrayDim || Index<0 )
		{
			// Display out-of-bounds warning and continue on with index clamped to valid range.
			Stack.Logf( NAME_Error, TEXT("Accessed array '%s' out of bounds (%i/%i)"), GProperty->GetName(), Index, GProperty->ArrayDim );
			Index = Clamp( Index, 0, GProperty->ArrayDim - 1 );
		}

		// Update address.
		GPropAddr += Index * GProperty->ElementSize;
		if( Result )//!!
			GProperty->CopySingleValue( Result, GPropAddr );
	}
}
IMPLEMENT_FUNCTION( UObject, EX_ArrayElement, execArrayElement );

void UObject::execDebugInfo( FFrame& Stack, RESULT_DECL ) //DEBUGGER
{
	INT GVER		= Stack.ReadInt();
	
	// Correct version?
	if( GVER != 100 )
	{
		Stack.Code -= sizeof(INT);
		Stack.Code--;
		return;
	}
	
	INT LineNumber	= Stack.ReadInt();
	INT InputPos	= Stack.ReadInt();
	BYTE OpCode		= *Stack.Code++;

	// Only valid when the debugger is running.
	if ( GDebugger != NULL )
		GDebugger->DebugInfo( this, &Stack, OpCode, LineNumber, InputPos );
}
IMPLEMENT_FUNCTION( UObject, EX_DebugInfo, execDebugInfo );

void UObject::execDynArrayElement( FFrame& Stack, RESULT_DECL )
{
	// Get array index expression.
	INT Index=0;
	Stack.Step( Stack.Object, &Index );

	GProperty = NULL;
	Stack.Step( this, NULL );
	GPropObject = this;

	checkSlow(GProperty);
	checkSlow(GProperty->IsA(UArrayProperty::StaticClass()));

	// Add scaled offset to base pointer.
	if( GProperty && GPropAddr )
	{
		FArray* Array=(FArray*)GPropAddr;
		UArrayProperty* ArrayProp = (UArrayProperty*)GProperty;
		if( Index>=Array->Num() || Index<0 )
		{
			//if we are returning a value, check for out-of-bounds
			if ( Result || Index<0 )
			{
				Stack.Logf( NAME_Error, TEXT("Accessed array '%s' out of bounds (%i/%i)"), ArrayProp->GetName(), Index, Array->Num() );
				GPropAddr = 0;
				GPropObject = NULL;
				if (Result) appMemzero( Result, ArrayProp->Inner->ElementSize );
				return;
			}
			//if we are setting a value, allow the array to be resized
			else
			{
				Array->AddZeroed(ArrayProp->Inner->ElementSize,Index-Array->Num()+1);
			}
		}

		GPropAddr = (BYTE*)Array->GetData() + Index * ArrayProp->Inner->ElementSize;

		// Add scaled offset to base pointer.
		if( Result )
		{
			ArrayProp->Inner->CopySingleValue( Result, GPropAddr );
		}
	}
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayElement, execDynArrayElement );

void UObject::execDynArrayLength( FFrame& Stack, RESULT_DECL )
{
	GProperty = NULL;
	Stack.Step( this, NULL );
	GPropObject = this;

	if (GPropAddr)
	{
		FArray* Array=(FArray*)GPropAddr;
		if ( !Result )
			GRuntimeUCFlags |= RUC_ArrayLengthSet; //so that EX_Let knows that this is a length 'set'-ting
		else
			*(INT*)Result = Array->Num();
	}
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayLength, execDynArrayLength );

void UObject::execDynArrayInsert( FFrame& Stack, RESULT_DECL )
{
	GPropObject = this;
	GProperty = NULL;
	Stack.Step( this, NULL );
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(GProperty);
	FArray* Array=(FArray*)GPropAddr;

	P_GET_INT(Index);
	P_GET_INT(Count);
	if (Array && Count)
	{
		if ( Count < 0 )
		{
			Stack.Logf( TEXT("Attempt to insert a negative number of elements '%s'"), ArrayProperty->GetName() );
			return;
		}
		if ( Index < 0 || Index > Array->Num() )
		{
			Stack.Logf( TEXT("Attempt to insert %i elements at %i an %i-element array '%s'"), Count, Index, Array->Num(), ArrayProperty->GetName() );
			Index = Clamp(Index, 0,Array->Num());
		}
		Array->InsertZeroed( Index, Count, ArrayProperty->Inner->ElementSize);
	}
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayInsert, execDynArrayInsert );

void UObject::execDynArrayRemove( FFrame& Stack, RESULT_DECL )
{	
	GProperty = NULL;
	GPropObject = this;
	Stack.Step( this, NULL );
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(GProperty);
	FArray* Array=(FArray*)GPropAddr;

	P_GET_INT(Index);
	P_GET_INT(Count);
	if (Array && Count)
	{
		if ( Count < 0 )
		{
			Stack.Logf( TEXT("Attempt to remove a negative number of elements '%s'"), ArrayProperty->GetName() );
			return;
		}
		if ( Index < 0 || Index >= Array->Num() || Index + Count > Array->Num() )
		{
			if (Count == 1)
				Stack.Logf( TEXT("Attempt to remove element %i in an %i-element array '%s'"), Index, Array->Num(), ArrayProperty->GetName() );
			else
				Stack.Logf( TEXT("Attempt to remove elements %i through %i in an %i-element array '%s'"), Index, Index+Count-1, Array->Num(), ArrayProperty->GetName() );
			Index = Clamp(Index, 0,Array->Num());
			if ( Index + Count > Array->Num() )
				Count = Array->Num() - Index;
		}

		for (INT i=Index+Count-1; i>=Index; i--)
			ArrayProperty->Inner->DestroyValue((BYTE*)Array->GetData() + ArrayProperty->Inner->ElementSize*i);
		Array->Remove( Index, Count, ArrayProperty->Inner->ElementSize);
	}
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayRemove, execDynArrayRemove );

void UObject::execBoolVariable( FFrame& Stack, RESULT_DECL )
{ 
	// Get bool variable.
	BYTE B = *Stack.Code++;
	UBoolProperty* Property;
#if SERIAL_POINTER_INDEX
	INT idx = *(INT*)Stack.Code;
	checkSlow(idx < GTotalSerializedPointers);
	Property = (UBoolProperty*) GSerializedPointers[idx];
#else
	Property = *(UBoolProperty**)Stack.Code;
#endif

	(this->*GNatives[B])( Stack, NULL );
	GProperty = Property;
	GPropObject = this;

	// Note that we're not returning an in-place pointer to to the bool, so EX_Let 
	// must take special precautions with bools.
	if( Result )
		*(BITFIELD*)Result = (GPropAddr && (*(BITFIELD*)GPropAddr & ((UBoolProperty*)GProperty)->BitMask)) ? 1 : 0;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolVariable, execBoolVariable );

void UObject::execStructMember( FFrame& Stack, RESULT_DECL )
{
	// Get structure element.
	UProperty* Property = (UProperty*)Stack.ReadObject();

	// Get struct expression.
	UStruct* Struct = CastChecked<UStruct>(Property->GetOuter());
	BYTE* Buffer = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer, Struct->PropertiesSize );
	GPropAddr = NULL;
	Stack.Step( this, Buffer );

	// Set result.
	GProperty = Property;
	GPropObject = this;
	if( GPropAddr )
		GPropAddr += Property->Offset;
	if( Result )
		Property->CopyCompleteValue( Result, Buffer+Property->Offset );
	for( UProperty* P=Struct->ConstructorLink; P; P=P->ConstructorLinkNext )
		P->DestroyValue( Buffer + P->Offset );
}
IMPLEMENT_FUNCTION( UObject, EX_StructMember, execStructMember );

/////////////
// Nothing //
/////////////

void UObject::execNothing( FFrame& Stack, RESULT_DECL )
{
	// Do nothing.
}
IMPLEMENT_FUNCTION( UObject, EX_Nothing, execNothing );

void UObject::execNativeParm( FFrame& Stack, RESULT_DECL )
{
	UProperty* Property = (UProperty*)Stack.ReadObject();
	if( Result )
	{
		GPropAddr = Stack.Locals + Property->Offset;
		GPropObject = NULL;  
		Property->CopyCompleteValue( Result, Stack.Locals + Property->Offset );
	}
}
IMPLEMENT_FUNCTION( UObject, EX_NativeParm, execNativeParm );

void UObject::execEndFunctionParms( FFrame& Stack, RESULT_DECL )
{
	// For skipping over optional function parms without values specified.
	GPropObject = NULL;  
	Stack.Code--;
}
IMPLEMENT_FUNCTION( UObject, EX_EndFunctionParms, execEndFunctionParms );

//////////////
// Commands //
//////////////

void UObject::execStop( FFrame& Stack, RESULT_DECL )
{
	Stack.Code = NULL;
}
IMPLEMENT_FUNCTION( UObject, EX_Stop, execStop );

//@warning: Does not support UProperty's fully, will break when TArray's are supported in UnrealScript!
void UObject::execSwitch( FFrame& Stack, RESULT_DECL )
{
	// Get switch size.
	BYTE bSize = *Stack.Code++;

	// Get switch expression.
	BYTE SwitchBuffer[1024], Buffer[1024];
	appMemzero( Buffer,       sizeof(FString) );
	appMemzero( SwitchBuffer, sizeof(FString) );
	Stack.Step( Stack.Object, SwitchBuffer );

	// Check each case clause till we find a match.
	for( ; ; )
	{
		// Skip over case token.
		checkSlow(*Stack.Code==EX_Case);
		Stack.Code++;

		// Get address of next handler.
		INT wNext = Stack.ReadWord();
		if( wNext == MAXWORD ) // Default case or end of cases.
			break;

		// Get case expression.
		Stack.Step( Stack.Object, Buffer );

		// Compare.
		if( bSize ? (appMemcmp(SwitchBuffer,Buffer,bSize)==0) : (*(FString*)SwitchBuffer==*(FString*)Buffer) )
			break;

		// Jump to next handler.
		Stack.Code = &Stack.Node->Script(wNext);
	}
	if( !bSize )
	{
		(*(FString*)SwitchBuffer).~FString();
		(*(FString*)Buffer      ).~FString();
	}
}
IMPLEMENT_FUNCTION( UObject, EX_Switch, execSwitch );

void UObject::execCase( FFrame& Stack, RESULT_DECL )
{
	INT wNext = Stack.ReadWord();
	if( wNext != MAXWORD )
	{
		// Skip expression.
		BYTE Buffer[1024];
		appMemzero( Buffer, sizeof(FString) );
		Stack.Step( Stack.Object, Buffer );
	}
}
IMPLEMENT_FUNCTION( UObject, EX_Case, execCase );

void UObject::execJump( FFrame& Stack, RESULT_DECL )
{
	CHECK_RUNAWAY;

	// Jump immediate.
	INT Offset = Stack.ReadWord();
	Stack.Code = &Stack.Node->Script(Offset);
//	Stack.Code = &Stack.Node->Script(Stack.ReadWord() );
}
IMPLEMENT_FUNCTION( UObject, EX_Jump, execJump );

void UObject::execJumpIfNot( FFrame& Stack, RESULT_DECL )
{
	CHECK_RUNAWAY;

	// Get code offset.
	INT wOffset = Stack.ReadWord();

	// Get boolean test value.
	UBOOL Value=0;
	Stack.Step( Stack.Object, &Value );

	// Jump if false.
	if( !Value )
		Stack.Code = &Stack.Node->Script( wOffset );
}
IMPLEMENT_FUNCTION( UObject, EX_JumpIfNot, execJumpIfNot );

void UObject::execAssert( FFrame& Stack, RESULT_DECL )
{
	// Get line number.
	INT wLine = Stack.ReadWord();

	// Get boolean assert value.
	DWORD Value=0;
	Stack.Step( Stack.Object, &Value );

	// Check it.
	if( !Value && (!GDebugger || !GDebugger->NotifyAssertionFailed( wLine )) )
	{
		Stack.Logf( NAME_Critical, TEXT("Assertion failed, line %i"), wLine );
	}
}
IMPLEMENT_FUNCTION( UObject, EX_Assert, execAssert );

void UObject::execGotoLabel( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(N);
	if( !GotoLabel( N ) )
        Stack.Logf( NAME_Error, TEXT("GotoLabel (%s): Label not found"), *N ); 
}
IMPLEMENT_FUNCTION( UObject, EX_GotoLabel, execGotoLabel );

////////////////
// Assignment //
////////////////

void UObject::execLet( FFrame& Stack, RESULT_DECL )
{
	checkSlow(!IsA(UBoolProperty::StaticClass()));

	// Get variable address.
	GPropAddr = NULL;
	Stack.Step( Stack.Object, NULL ); // Evaluate variable.
	if( !GPropAddr )
	{
		Stack.Logf( NAME_ScriptWarning, TEXT("Attempt to assign variable through None") );
		static BYTE Crud[1024];//@temp
		GPropAddr = Crud;
		appMemzero( GPropAddr, sizeof(FString) );
	}
	else if ( GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net) )
		GPropObject->NetDirty(GProperty); //FIXME - use object property instead for performance

	if (GRuntimeUCFlags & RUC_ArrayLengthSet)
	{
		GRuntimeUCFlags &= ~RUC_ArrayLengthSet;
		FArray* Array=(FArray*)GPropAddr;
		UArrayProperty* ArrayProp = (UArrayProperty*)GProperty;
		INT NewSize = 0;
		Stack.Step( Stack.Object, &NewSize); // Evaluate expression into variable.
		if (NewSize > Array->Num())
		{
			Array->AddZeroed(ArrayProp->Inner->ElementSize, NewSize-Array->Num());
		}
		else if (NewSize < Array->Num())
		{
			for (INT i=Array->Num()-1; i>=NewSize; i--)
				ArrayProp->Inner->DestroyValue((BYTE*)Array->GetData() + ArrayProp->Inner->ElementSize*i);
			Array->Remove(NewSize, Array->Num()-NewSize, ArrayProp->Inner->ElementSize );
		}
	} else
		Stack.Step( Stack.Object, GPropAddr ); // Evaluate expression into variable.
}
IMPLEMENT_FUNCTION( UObject, EX_Let, execLet );

void UObject::execLetBool( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	GPropAddr = NULL;
	GProperty = NULL;
	GPropObject = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.
	BITFIELD*      BoolAddr     = (BITFIELD*)GPropAddr;
	UBoolProperty* BoolProperty = (UBoolProperty*)GProperty;
	INT Value=0;  // must be INT...execTrue/execFalse return 32 bits. --ryan.
	if ( GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net) )
		GPropObject->NetDirty(GProperty);
	Stack.Step( Stack.Object, &Value );
	if( BoolAddr )
	{
		checkSlow(BoolProperty->IsA(UBoolProperty::StaticClass()));
		if( Value ) *BoolAddr |=  BoolProperty->BitMask;
		else        *BoolAddr &= ~BoolProperty->BitMask;
	}
}
IMPLEMENT_FUNCTION( UObject, EX_LetBool, execLetBool );

/////////////////////////
// Context expressions //
/////////////////////////

void UObject::execSelf( FFrame& Stack, RESULT_DECL )
{
	// Get Self actor for this context.
	*(UObject**)Result = this;
}
IMPLEMENT_FUNCTION( UObject, EX_Self, execSelf );

void UObject::execContext( FFrame& Stack, RESULT_DECL )
{
	// Get actor variable.
	UObject* NewContext=NULL;
	Stack.Step( this, &NewContext );

	// Execute or skip the following expression in the actor's context.
	if( NewContext != NULL )
	{
		Stack.Code += 3;
		Stack.Step( NewContext, Result );
	}
	else
	{
		if ( GProperty )
			Stack.Logf( NAME_Warning, TEXT("Accessed None '%s'"), GProperty->GetName() );
		else
			Stack.Logf( NAME_Warning, TEXT("Accessed None"));

		// DEBUGGER

		if ( GDebugger )
			GDebugger->NotifyAccessedNone();	// jmw

		INT wSkip = Stack.ReadWord();
		BYTE bSize = *Stack.Code++;
		Stack.Code += wSkip;
		GPropAddr = NULL;
		GProperty = NULL;
		GPropObject = NULL;
		if( Result )
			appMemzero( Result, bSize );
	}
}
IMPLEMENT_FUNCTION( UObject, EX_Context, execContext );

////////////////////
// Function calls //
////////////////////

void UObject::execVirtualFunction( FFrame& Stack, RESULT_DECL )
{
	// read the super byte
	BYTE bSuper = *(BYTE*)Stack.Code;
	Stack.Code += sizeof(BYTE);
	UFunction *func = NULL;
	FName funcName = Stack.ReadName();
	// if calling super function
	if (bSuper)
	{
		// get the last function on stack
		UFunction *searchFunc = (UFunction*)Stack.Node;
		if (searchFunc != NULL)
		{
			// build the list of states to search through
			TArray<UState*> searchStates;
			UState *testState = NULL;
			if (StateFrame != NULL &&
				StateFrame->StateNode != NULL &&
				StateFrame->StateNode != GetClass())
			{
				UState *baseState = StateFrame->StateNode;
				// if transitioned states within the function call,
				if (StateFrame->StateNode != searchFunc->GetOuter())
				{
					// use the previous state as a base for searching
					baseState = (UState*)searchFunc->GetOuter();
				}
				// otherwise use the current state as a base
				searchStates.AddItem(baseState);
				// search the state stack
				if (StateFrame->StateStack.Num() > 0)
				{
					for (INT idx = StateFrame->StateStack.Num() - 1; idx >= 0; idx--)
					{
						searchStates.AddItem(StateFrame->StateStack(idx).State);
					}
				}
				// next search the state inheritance chain
				testState = baseState->GetSuperState();
				while (testState != NULL)
				{
					searchStates.AddItem(testState);
					testState = testState->GetSuperState();
				}
			}
			// and last search the class inheritance chain
			testState = GetClass();
			while (testState != NULL)
			{
				searchStates.AddUniqueItem(testState);
				testState = testState->GetSuperState();
			}
			// first, find where this function is in the state/class list
			// then call the next version of the function that is found
			UBOOL bFoundState = 0;
			UFunction *testFunc = NULL;
			for (INT idx = 0; idx < searchStates.Num() && func == NULL; idx++)
			{
				if (!bFoundState &&
					searchStates(idx) == searchFunc->GetOuter())
				{
					bFoundState = 1;
				}
				else
				if (bFoundState)
				{
					testFunc = searchStates(idx)->FuncMap.FindRef(funcName);
					if (testFunc != NULL)
					{
						// found the next one up the chain
						func = testFunc;
					}
				}
			}
		}
		if (func == NULL)
		{
			appErrorf(TEXT("Failed to find super function %s for object %s"),*funcName,GetFullName());
		}
	}
	// default function search
	if (func == NULL)
	{
		func = FindFunctionChecked(funcName,0);
	}
	// Call the virtual function.
	CallFunction( Stack, Result, func );
}
IMPLEMENT_FUNCTION( UObject, EX_VirtualFunction, execVirtualFunction );

void UObject::execFinalFunction( FFrame& Stack, RESULT_DECL )
{
	// Call the final function.
	CallFunction( Stack, Result, (UFunction*)Stack.ReadObject() );
}
IMPLEMENT_FUNCTION( UObject, EX_FinalFunction, execFinalFunction );

void UObject::execGlobalFunction( FFrame& Stack, RESULT_DECL )
{
	// Call global version of virtual function.
	CallFunction( Stack, Result, FindFunctionChecked(Stack.ReadName(),1) );
}
IMPLEMENT_FUNCTION( UObject, EX_GlobalFunction, execGlobalFunction );

///////////////////////
// Delegates         //
///////////////////////

void UObject::execDelegateFunction( FFrame& Stack, RESULT_DECL )
{
	BYTE bLocalProp = *(BYTE*)Stack.Code;
	Stack.Code += sizeof(BYTE);
	// Look up delegate property.
	UProperty* DelegateProperty = (UProperty*)Stack.ReadObject();
	// read the delegate info
	FScriptDelegate* Delegate = NULL;
	if (bLocalProp)
	{
		// read off the stack
		Delegate = (FScriptDelegate*)(Stack.Locals + DelegateProperty->Offset);
	}
	else
	{
		// read off the object
		Delegate = (FScriptDelegate*)((BYTE*)this + DelegateProperty->Offset);
	}
	FName DelegateName = Stack.ReadName();
	if( Delegate->Object && Delegate->Object->IsPendingKill() )
	{
		Delegate->Object = NULL;
		Delegate->FunctionName = NAME_None;
	}
	if( Delegate->Object )
		Delegate->Object->CallFunction( Stack, Result, Delegate->Object->FindFunctionChecked(Delegate->FunctionName) );	
	else
		CallFunction( Stack, Result, FindFunctionChecked(DelegateName) );
}
IMPLEMENT_FUNCTION( UObject, EX_DelegateFunction, execDelegateFunction );

void UObject::execDelegateProperty( FFrame& Stack, RESULT_DECL )
{
	FName FunctionName = Stack.ReadName();
	((FScriptDelegate*)Result)->FunctionName = FunctionName;
	if( FunctionName == NAME_None )
		((FScriptDelegate*)Result)->Object = NULL;
	else
		((FScriptDelegate*)Result)->Object = this;
}
IMPLEMENT_FUNCTION( UObject, EX_DelegateProperty, execDelegateProperty );

void UObject::execLetDelegate( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	GPropAddr = NULL;
	GProperty = NULL;
	GPropObject = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.
	FScriptDelegate* DelegateAddr = (FScriptDelegate*)GPropAddr;
	FScriptDelegate Delegate;
	Stack.Step( Stack.Object, &Delegate );
	if( DelegateAddr )
	{
		DelegateAddr->FunctionName = Delegate.FunctionName;
		DelegateAddr->Object	   = Delegate.Object;
	}
}
IMPLEMENT_FUNCTION( UObject, EX_LetDelegate, execLetDelegate );


///////////////////////
// Struct comparison //
///////////////////////

void UObject::execStructCmpEq( FFrame& Stack, RESULT_DECL )
{
	UStruct* Struct  = (UStruct*)Stack.ReadObject();
	BYTE*    Buffer1 = (BYTE*)appAlloca(Struct->PropertiesSize);
	BYTE*    Buffer2 = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer1, Struct->PropertiesSize );
	appMemzero( Buffer2, Struct->PropertiesSize );
	Stack.Step( this, Buffer1 );
	Stack.Step( this, Buffer2 );
	*(DWORD*)Result  = Struct->StructCompare( Buffer1, Buffer2 );
}
IMPLEMENT_FUNCTION( UObject, EX_StructCmpEq, execStructCmpEq );

void UObject::execStructCmpNe( FFrame& Stack, RESULT_DECL )
{
	UStruct* Struct = (UStruct*)Stack.ReadObject();
	BYTE*    Buffer1 = (BYTE*)appAlloca(Struct->PropertiesSize);
	BYTE*    Buffer2 = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer1, Struct->PropertiesSize );
	appMemzero( Buffer2, Struct->PropertiesSize );
	Stack.Step( this, Buffer1 );
	Stack.Step( this, Buffer2 );
	*(DWORD*)Result = !Struct->StructCompare(Buffer1,Buffer2);
}
IMPLEMENT_FUNCTION( UObject, EX_StructCmpNe, execStructCmpNe );

///////////////
// Constants //
///////////////

void UObject::execIntConst( FFrame& Stack, RESULT_DECL )
{
	*(INT*)Result = Stack.ReadInt();
}
IMPLEMENT_FUNCTION( UObject, EX_IntConst, execIntConst );

void UObject::execFloatConst( FFrame& Stack, RESULT_DECL )
{
	*(FLOAT*)Result = Stack.ReadFloat();
}
IMPLEMENT_FUNCTION( UObject, EX_FloatConst, execFloatConst );

void UObject::execStringConst( FFrame& Stack, RESULT_DECL )
{
	*(FString*)Result = (ANSICHAR*)Stack.Code;
	while( *Stack.Code )
		Stack.Code++;
	Stack.Code++;
}
IMPLEMENT_FUNCTION( UObject, EX_StringConst, execStringConst );

void UObject::execUnicodeStringConst( FFrame& Stack, RESULT_DECL )
{
	*(FString*)Result = FString((UNICHAR*)Stack.Code);
	while( *(_WORD*)Stack.Code )
		Stack.Code+=sizeof(_WORD);
	Stack.Code+=sizeof(_WORD);
}
IMPLEMENT_FUNCTION( UObject, EX_UnicodeStringConst, execUnicodeStringConst );

void UObject::execObjectConst( FFrame& Stack, RESULT_DECL )
{
	*(UObject**)Result = (UObject*)Stack.ReadObject();
}
IMPLEMENT_FUNCTION( UObject, EX_ObjectConst, execObjectConst );

void UObject::execNameConst( FFrame& Stack, RESULT_DECL )
{
	*(FName*)Result = Stack.ReadName();
}
IMPLEMENT_FUNCTION( UObject, EX_NameConst, execNameConst );

void UObject::execByteConst( FFrame& Stack, RESULT_DECL )
{
	*(BYTE*)Result = *Stack.Code++;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteConst, execByteConst );

void UObject::execIntZero( FFrame& Stack, RESULT_DECL )
{
	*(INT*)Result = 0;
}
IMPLEMENT_FUNCTION( UObject, EX_IntZero, execIntZero );

void UObject::execIntOne( FFrame& Stack, RESULT_DECL )
{
	*(INT*)Result = 1;
}
IMPLEMENT_FUNCTION( UObject, EX_IntOne, execIntOne );

void UObject::execTrue( FFrame& Stack, RESULT_DECL )
{
	*(INT*)Result = 1;
}
IMPLEMENT_FUNCTION( UObject, EX_True, execTrue );

void UObject::execFalse( FFrame& Stack, RESULT_DECL )
{
	*(DWORD*)Result = 0;
}
IMPLEMENT_FUNCTION( UObject, EX_False, execFalse );

void UObject::execNoObject( FFrame& Stack, RESULT_DECL )
{
	*(UObject**)Result = NULL;
}
IMPLEMENT_FUNCTION( UObject, EX_NoObject, execNoObject );

void UObject::execIntConstByte( FFrame& Stack, RESULT_DECL )
{
	*(INT*)Result = *Stack.Code++;
}
IMPLEMENT_FUNCTION( UObject, EX_IntConstByte, execIntConstByte );

/////////////////
// Conversions //
/////////////////

void UObject::execDynamicCast( FFrame& Stack, RESULT_DECL )
{
	UClass* Class = (UClass *)Stack.ReadObject();

	// Compile object expression.
	UObject* Castee = NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(Class)) ? Castee : NULL;
}
IMPLEMENT_FUNCTION( UObject, EX_DynamicCast, execDynamicCast );

void UObject::execMetaCast( FFrame& Stack, RESULT_DECL )
{
	UClass* MetaClass = (UClass*)Stack.ReadObject();

	// Compile actor expression.
	UObject* Castee=NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(UClass::StaticClass()) && ((UClass*)Castee)->IsChildOf(MetaClass)) ? Castee : NULL;
}
IMPLEMENT_FUNCTION( UObject, EX_MetaCast, execMetaCast );

void UObject::execPrimitiveCast( FFrame& Stack, RESULT_DECL )
{
	INT B = *(Stack.Code)++;
	(Stack.Object->*GCasts[B])( Stack, Result );
}
IMPLEMENT_FUNCTION( UObject, EX_PrimitiveCast, execPrimitiveCast );

void UObject::execByteToInt( FFrame& Stack, RESULT_DECL )
{
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(INT*)Result = B;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToInt, execByteToInt );

void UObject::execByteToBool( FFrame& Stack, RESULT_DECL )
{
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(DWORD*)Result = B ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToBool, execByteToBool );

void UObject::execByteToFloat( FFrame& Stack, RESULT_DECL )
{
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(FLOAT*)Result = B;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToFloat, execByteToFloat );

void UObject::execByteToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE(B);
	// dump the enum string if possible
	UByteProperty *ByteProp = Cast<UByteProperty>(GProperty);
	if (ByteProp != NULL &&
		ByteProp->Enum != NULL &&
		B >= 0 && B < ByteProp->Enum->Names.Num())
	{
		*(FString*)Result = *ByteProp->Enum->Names(B);
	}
	else
	{
		*(FString*)Result = FString::Printf(TEXT("%i"),B);
	}
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToString, execByteToString );

void UObject::execIntToByte( FFrame& Stack, RESULT_DECL )
{
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(BYTE*)Result = I;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToByte, execIntToByte );

void UObject::execIntToBool( FFrame& Stack, RESULT_DECL )
{
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(INT*)Result = I ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToBool, execIntToBool );

void UObject::execIntToFloat( FFrame& Stack, RESULT_DECL )
{
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(FLOAT*)Result = I;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToFloat, execIntToFloat );

void UObject::execIntToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(I);
	*(FString*)Result = FString::Printf(TEXT("%i"),I);
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToString, execIntToString );

void UObject::execBoolToByte( FFrame& Stack, RESULT_DECL )
{
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(BYTE*)Result = B & 1;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToByte, execBoolToByte );

void UObject::execBoolToInt( FFrame& Stack, RESULT_DECL )
{
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(INT*)Result = B & 1;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToInt, execBoolToInt );

void UObject::execBoolToFloat( FFrame& Stack, RESULT_DECL )
{
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(FLOAT*)Result = B & 1;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToFloat, execBoolToFloat );

void UObject::execBoolToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(B);
	*(FString*)Result = B ? GTrue : GFalse;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToString, execBoolToString );

void UObject::execFloatToByte( FFrame& Stack, RESULT_DECL )
{
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(BYTE*)Result = (BYTE)F;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToByte, execFloatToByte );

void UObject::execFloatToInt( FFrame& Stack, RESULT_DECL )
{
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(INT*)Result = (INT)F;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToInt, execFloatToInt );

void UObject::execFloatToBool( FFrame& Stack, RESULT_DECL )
{
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(DWORD*)Result = F!=0.f ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToBool, execFloatToBool );

void UObject::execFloatToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(F);
	*(FString*)Result = FString::Printf(TEXT("%.2f"),F);
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToString, execFloatToString );

void UObject::execObjectToBool( FFrame& Stack, RESULT_DECL )
{
	UObject* Obj=NULL;
	Stack.Step( Stack.Object, &Obj );
	*(DWORD*)Result = Obj!=NULL;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ObjectToBool, execObjectToBool );

void UObject::execObjectToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UObject,Obj);
	*(FString*)Result = Obj ? Obj->GetPathName() : TEXT("None");
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ObjectToString, execObjectToString );

void UObject::execNameToBool( FFrame& Stack, RESULT_DECL )
{
	FName N=NAME_None;
	Stack.Step( Stack.Object, &N );
	*(DWORD*)Result = N!=NAME_None ? 1 : 0;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_NameToBool, execNameToBool );

void UObject::execNameToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(N);
	*(FString*)Result = *N;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_NameToString, execNameToString );

void UObject::execStringToName( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(String);
	*(FName*)Result = FName(*String);
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToName, execStringToName );

void UObject::execStringToByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);
	*(BYTE*)Result = appAtoi( *Str );
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToByte, execStringToByte );

void UObject::execStringToInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);
	*(INT*)Result = appAtoi( *Str );
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToInt, execStringToInt );

void UObject::execStringToBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);
	if( appStricmp(*Str,TEXT("True") )==0 || appStricmp(*Str,GTrue)==0 )
	{
		*(INT*)Result = 1;
	}
	else if( appStricmp(*Str,TEXT("False"))==0 || appStricmp(*Str,GFalse)==0 )
	{
		*(INT*)Result = 0;
	}
	else
	{
		*(INT*)Result = appAtoi(*Str) ? 1 : 0;
	}
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToBool, execStringToBool );

void UObject::execStringToFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);
	*(FLOAT*)Result = appAtof( *Str );
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToFloat, execStringToFloat );

/////////////////////////////////////////
// Native bool operators and functions //
/////////////////////////////////////////

void UObject::execNot_PreBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(A);
	P_FINISH;

	*(DWORD*)Result = !A;
}
IMPLEMENT_FUNCTION( UObject, 129, execNot_PreBool );

void UObject::execEqualEqual_BoolBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = ((!A) == (!B));
}
IMPLEMENT_FUNCTION( UObject, 242, execEqualEqual_BoolBool );

void UObject::execNotEqual_BoolBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = ((!A) != (!B));
}
IMPLEMENT_FUNCTION( UObject, 243, execNotEqual_BoolBool );

void UObject::execAndAnd_BoolBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(A);
	P_GET_SKIP_OFFSET(W);

	if( A )
	{
		P_GET_UBOOL(B);
		*(DWORD*)Result = A && B;
		Stack.Code++; //DEBUGGER
	}
	else
	{
		*(DWORD*)Result = 0;
		Stack.Code += W;
	}
}
IMPLEMENT_FUNCTION( UObject, 130, execAndAnd_BoolBool );

void UObject::execXorXor_BoolBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = !A ^ !B;
}
IMPLEMENT_FUNCTION( UObject, 131, execXorXor_BoolBool );

void UObject::execOrOr_BoolBool( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(A);
	P_GET_SKIP_OFFSET(W);
	if( !A )
	{
		P_GET_UBOOL(B);
		*(DWORD*)Result = A || B;
		Stack.Code++; //DEBUGGER
	}
	else
	{
		*(DWORD*)Result = 1;
		Stack.Code += W;
	}
}
IMPLEMENT_FUNCTION( UObject, 132, execOrOr_BoolBool );

/////////////////////////////////////////
// Native byte operators and functions //
/////////////////////////////////////////

void UObject::execMultiplyEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A *= B);
}
IMPLEMENT_FUNCTION( UObject, 133, execMultiplyEqual_ByteByte );

void UObject::execDivideEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = B ? (*A /= B) : 0;
}
IMPLEMENT_FUNCTION( UObject, 134, execDivideEqual_ByteByte );

void UObject::execAddEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A += B);
}
IMPLEMENT_FUNCTION( UObject, 135, execAddEqual_ByteByte );

void UObject::execSubtractEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A -= B);
}
IMPLEMENT_FUNCTION( UObject, 136, execSubtractEqual_ByteByte );

void UObject::execAddAdd_PreByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = ++(*A);
}
IMPLEMENT_FUNCTION( UObject, 137, execAddAdd_PreByte );

void UObject::execSubtractSubtract_PreByte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = --(*A);
}
IMPLEMENT_FUNCTION( UObject, 138, execSubtractSubtract_PreByte );

void UObject::execAddAdd_Byte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = (*A)++;
}
IMPLEMENT_FUNCTION( UObject, 139, execAddAdd_Byte );

void UObject::execSubtractSubtract_Byte( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = (*A)--;
}
IMPLEMENT_FUNCTION( UObject, 140, execSubtractSubtract_Byte );

/////////////////////////////////
// Int operators and functions //
/////////////////////////////////

void UObject::execComplement_PreInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = ~A;
}
IMPLEMENT_FUNCTION( UObject, 141, execComplement_PreInt );

void UObject::execGreaterGreaterGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = ((DWORD)A) >> B;
}
IMPLEMENT_FUNCTION( UObject, 196, execGreaterGreaterGreater_IntInt );

void UObject::execSubtract_PreInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = -A;
}
IMPLEMENT_FUNCTION( UObject, 143, execSubtract_PreInt );

void UObject::execMultiply_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A * B;
}
IMPLEMENT_FUNCTION( UObject, 144, execMultiply_IntInt );

void UObject::execDivide_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	if (B == 0)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Divide by zero"));
	}

	*(INT*)Result = B ? A / B : 0;
}
IMPLEMENT_FUNCTION( UObject, 145, execDivide_IntInt );

void UObject::execAdd_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A + B;
}
IMPLEMENT_FUNCTION( UObject, 146, execAdd_IntInt );

void UObject::execSubtract_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A - B;
}
IMPLEMENT_FUNCTION( UObject, 147, execSubtract_IntInt );

void UObject::execLessLess_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A << B;
}
IMPLEMENT_FUNCTION( UObject, 148, execLessLess_IntInt );

void UObject::execGreaterGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A >> B;
}
IMPLEMENT_FUNCTION( UObject, 149, execGreaterGreater_IntInt );

void UObject::execLess_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A < B;
}
IMPLEMENT_FUNCTION( UObject, 150, execLess_IntInt );

void UObject::execGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A > B;
}
IMPLEMENT_FUNCTION( UObject, 151, execGreater_IntInt );

void UObject::execLessEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A <= B;
}
IMPLEMENT_FUNCTION( UObject, 152, execLessEqual_IntInt );

void UObject::execGreaterEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A >= B;
}
IMPLEMENT_FUNCTION( UObject, 153, execGreaterEqual_IntInt );

void UObject::execEqualEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A == B;
}
IMPLEMENT_FUNCTION( UObject, 154, execEqualEqual_IntInt );

void UObject::execNotEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A != B;
}
IMPLEMENT_FUNCTION( UObject, 155, execNotEqual_IntInt );

void UObject::execAnd_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A & B;
}
IMPLEMENT_FUNCTION( UObject, 156, execAnd_IntInt );

void UObject::execXor_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A ^ B;
}
IMPLEMENT_FUNCTION( UObject, 157, execXor_IntInt );

void UObject::execOr_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A | B;
}
IMPLEMENT_FUNCTION( UObject, 158, execOr_IntInt );

void UObject::execMultiplyEqual_IntFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(INT*)Result = *A = (INT)(*A * B);
}
IMPLEMENT_FUNCTION( UObject, 159, execMultiplyEqual_IntFloat );

void UObject::execDivideEqual_IntFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(INT*)Result = *A = (INT)(B ? *A/B : 0.f);
}
IMPLEMENT_FUNCTION( UObject, 160, execDivideEqual_IntFloat );

void UObject::execAddEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = (*A += B);
}
IMPLEMENT_FUNCTION( UObject, 161, execAddEqual_IntInt );

void UObject::execSubtractEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = (*A -= B);
}
IMPLEMENT_FUNCTION( UObject, 162, execSubtractEqual_IntInt );

void UObject::execAddAdd_PreInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = ++(*A);
}
IMPLEMENT_FUNCTION( UObject, 163, execAddAdd_PreInt );

void UObject::execSubtractSubtract_PreInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = --(*A);
}
IMPLEMENT_FUNCTION( UObject, 164, execSubtractSubtract_PreInt );

void UObject::execAddAdd_Int( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = (*A)++;
}
IMPLEMENT_FUNCTION( UObject, 165, execAddAdd_Int );

void UObject::execSubtractSubtract_Int( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = (*A)--;
}
IMPLEMENT_FUNCTION( UObject, 166, execSubtractSubtract_Int );

void UObject::execRand( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = A>0 ? (appRand() % A) : 0;
}
IMPLEMENT_FUNCTION( UObject, 167, execRand );

void UObject::execMin( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Min(A,B);
}
IMPLEMENT_FUNCTION( UObject, 249, execMin );

void UObject::execMax( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Max(A,B);
}
IMPLEMENT_FUNCTION( UObject, 250, execMax );

void UObject::execClamp( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(V);
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Clamp(V,A,B);
}
IMPLEMENT_FUNCTION( UObject, 251, execClamp );

///////////////////////////////////
// Float operators and functions //
///////////////////////////////////

void UObject::execSubtract_PreFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = -A;
}	
IMPLEMENT_FUNCTION( UObject, 169, execSubtract_PreFloat );

void UObject::execMultiplyMultiply_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = appPow(A,B);
}	
IMPLEMENT_FUNCTION( UObject, 170, execMultiplyMultiply_FloatFloat );

void UObject::execMultiply_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A * B;
}	
IMPLEMENT_FUNCTION( UObject, 171, execMultiply_FloatFloat );

void UObject::execDivide_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A / B;
}	
IMPLEMENT_FUNCTION( UObject, 172, execDivide_FloatFloat );

void UObject::execPercent_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = appFmod(A,B);
}	
IMPLEMENT_FUNCTION( UObject, 173, execPercent_FloatFloat );

void UObject::execAdd_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + B;
}	
IMPLEMENT_FUNCTION( UObject, 174, execAdd_FloatFloat );

void UObject::execSubtract_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A - B;
}	
IMPLEMENT_FUNCTION( UObject, 175, execSubtract_FloatFloat );

void UObject::execLess_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A < B;
}	
IMPLEMENT_FUNCTION( UObject, 176, execLess_FloatFloat );

void UObject::execGreater_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A > B;
}	
IMPLEMENT_FUNCTION( UObject, 177, execGreater_FloatFloat );

void UObject::execLessEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A <= B;
}	
IMPLEMENT_FUNCTION( UObject, 178, execLessEqual_FloatFloat );

void UObject::execGreaterEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A >= B;
}	
IMPLEMENT_FUNCTION( UObject, 179, execGreaterEqual_FloatFloat );

void UObject::execEqualEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A == B;
}	
IMPLEMENT_FUNCTION( UObject, 180, execEqualEqual_FloatFloat );

void UObject::execNotEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A != B;
}	
IMPLEMENT_FUNCTION( UObject, 181, execNotEqual_FloatFloat );

void UObject::execComplementEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = Abs(A - B) < (1.e-4);
}	
IMPLEMENT_FUNCTION( UObject, 210, execComplementEqual_FloatFloat );

void UObject::execMultiplyEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A *= B);
}	
IMPLEMENT_FUNCTION( UObject, 182, execMultiplyEqual_FloatFloat );

void UObject::execDivideEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	if (B == 0.f)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Divide by zero"));
	}

	*(FLOAT*)Result = (*A /= B);
}	
IMPLEMENT_FUNCTION( UObject, 183, execDivideEqual_FloatFloat );

void UObject::execAddEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A += B);
}	
IMPLEMENT_FUNCTION( UObject, 184, execAddEqual_FloatFloat );

void UObject::execSubtractEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A -= B);
}	
IMPLEMENT_FUNCTION( UObject, 185, execSubtractEqual_FloatFloat );

void UObject::execAbs( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = Abs(A);
}	
IMPLEMENT_FUNCTION( UObject, 186, execAbs );

void UObject::execSin( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appSin(A);
}	
IMPLEMENT_FUNCTION( UObject, 187, execSin );

void UObject::execAsin( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appAsin(A);
}	
IMPLEMENT_FUNCTION( UObject, -1, execAsin );

void UObject::execCos( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appCos(A);
}	
IMPLEMENT_FUNCTION( UObject, 188, execCos );

void UObject::execAcos( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appAcos(A);
}	
IMPLEMENT_FUNCTION( UObject, -1, execAcos );

void UObject::execTan( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appTan(A);
}	
IMPLEMENT_FUNCTION( UObject, 189, execTan );

void UObject::execAtan( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
    P_GET_FLOAT(B); //amb
	P_FINISH;

    *(FLOAT*)Result = appAtan2(A,B); //amb: changed to atan2
}	
IMPLEMENT_FUNCTION( UObject, 190, execAtan );

void UObject::execExp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appExp(A);
}	
IMPLEMENT_FUNCTION( UObject, 191, execExp );

void UObject::execLoge( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appLoge(A);
}	
IMPLEMENT_FUNCTION( UObject, 192, execLoge );

void UObject::execSqrt( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	FLOAT Sqrt = 0.f;
	if( A > 0.f )
	{
		// Can't use appSqrt(0) as it computes it as 1 / appInvSqrt(). Not a problem as Sqrt variable defaults to 0 == sqrt(0).
		Sqrt = appSqrt( A );
	}
	else if (A < 0.f)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Attempt to take Sqrt() of negative number - returning 0."));
	}

	*(FLOAT*)Result = Sqrt;
}	
IMPLEMENT_FUNCTION( UObject, 193, execSqrt );

void UObject::execSquare( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = Square(A);
}	
IMPLEMENT_FUNCTION( UObject, 194, execSquare );

void UObject::execFRand( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(FLOAT*)Result = appFrand();
}	
IMPLEMENT_FUNCTION( UObject, 195, execFRand );

void UObject::execFMin( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Min(A,B);
}	
IMPLEMENT_FUNCTION( UObject, 244, execFMin );

void UObject::execFMax( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Max(A,B);
}	
IMPLEMENT_FUNCTION( UObject, 245, execFMax );

void UObject::execFClamp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Clamp(V,A,B);
}	
IMPLEMENT_FUNCTION( UObject, 246, execFClamp );

void UObject::execLerp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + V*(B-A);
}	
IMPLEMENT_FUNCTION( UObject, 247, execLerp );

void UObject::execSmerp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + V*V*(3.f - 2.f*V)*(B-A);
}
IMPLEMENT_FUNCTION( UObject, 248, execSmerp );

void UObject::execRotationConst( FFrame& Stack, RESULT_DECL )
{
	((FRotator*)Result)->Pitch = Stack.ReadInt();
	((FRotator*)Result)->Yaw   = Stack.ReadInt();
	((FRotator*)Result)->Roll  = Stack.ReadInt();
}
IMPLEMENT_FUNCTION( UObject, EX_RotationConst, execRotationConst );

void UObject::execVectorConst( FFrame& Stack, RESULT_DECL )
{
	*(FVector*)Result = *(FVector*)Stack.Code;
	Stack.Code += sizeof(FVector);
}
IMPLEMENT_FUNCTION( UObject, EX_VectorConst, execVectorConst );

/////////////////
// Conversions //
/////////////////

void UObject::execStringToVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);

	const TCHAR* Stream = *Str;
	FVector Value(0,0,0);
	Value.X = appAtof(Stream);
	Stream = appStrstr(Stream,TEXT(","));
	if( Stream )
	{
		Value.Y = appAtof(++Stream);
		Stream = appStrstr(Stream,TEXT(","));
		if( Stream )
			Value.Z = appAtof(++Stream);
	}
	*(FVector*)Result = Value;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToVector, execStringToVector );

void UObject::execStringToRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Str);

	const TCHAR* Stream = *Str;
	FRotator Rotation(0,0,0);
	Rotation.Pitch = appAtoi(Stream);
	Stream = appStrstr(Stream,TEXT(","));
	if( Stream )
	{
		Rotation.Yaw = appAtoi(++Stream);
		Stream = appStrstr(Stream,TEXT(","));
		if( Stream )
			Rotation.Roll = appAtoi(++Stream);
	}
	*(FRotator*)Result = Rotation;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToRotator, execStringToRotator );

void UObject::execVectorToBool( FFrame& Stack, RESULT_DECL )
{
	FVector V(0,0,0);
	Stack.Step( Stack.Object, &V );
	*(DWORD*)Result = V.IsZero() ? 0 : 1;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_VectorToBool, execVectorToBool );

void UObject::execVectorToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(V);
	*(FString*)Result = FString::Printf( TEXT("%.2f,%.2f,%.2f"), V.X, V.Y, V.Z );
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_VectorToString, execVectorToString );

void UObject::execVectorToRotator( FFrame& Stack, RESULT_DECL )
{
	FVector V(0,0,0);
	Stack.Step( Stack.Object, &V );
	*(FRotator*)Result = V.Rotation();
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_VectorToRotator, execVectorToRotator );

void UObject::execRotatorToBool( FFrame& Stack, RESULT_DECL )
{
	FRotator R(0,0,0);
	Stack.Step( Stack.Object, &R );
	*(DWORD*)Result = R.IsZero() ? 0 : 1;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_RotatorToBool, execRotatorToBool );

void UObject::execRotatorToVector( FFrame& Stack, RESULT_DECL )
{
	FRotator R(0,0,0);
	Stack.Step( Stack.Object, &R );
	*(FVector*)Result = R.Vector();
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_RotatorToVector, execRotatorToVector );

void UObject::execRotatorToString( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(R);
	*(FString*)Result = FString::Printf( TEXT("%i,%i,%i"), R.Pitch&65535, R.Yaw&65535, R.Roll&65535 );
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_RotatorToString, execRotatorToString );

////////////////////////////////////
// Vector operators and functions //
////////////////////////////////////

void UObject::execSubtract_PreVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_FINISH;

	*(FVector*)Result = -A;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 83, execSubtract_PreVector );

void UObject::execMultiply_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_FLOAT (B);
	P_FINISH;

	*(FVector*)Result = A*B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 84, execMultiply_VectorFloat );

void UObject::execMultiply_FloatVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT (A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A*B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 85, execMultiply_FloatVector );

void UObject::execMultiply_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A*B;
}	
IMPLEMENT_FUNCTION( UObject, 296, execMultiply_VectorVector );

void UObject::execDivide_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_FLOAT (B);
	P_FINISH;

	if (B == 0.f)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Divide by zero"));
	}

	*(FVector*)Result = A/B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 86, execDivide_VectorFloat );

void UObject::execAdd_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A+B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 87, execAdd_VectorVector );

void UObject::execSubtract_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A-B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 88, execSubtract_VectorVector );

void UObject::execLessLess_VectorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FVector*)Result = FRotationMatrix(B).Transpose().TransformNormal( A );
}	
IMPLEMENT_FUNCTION( UObject, 275, execLessLess_VectorRotator );

void UObject::execGreaterGreater_VectorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FVector*)Result = FRotationMatrix(B).TransformNormal( A );
}	
IMPLEMENT_FUNCTION( UObject, 276, execGreaterGreater_VectorRotator );

void UObject::execEqualEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(DWORD*)Result = A.X==B.X && A.Y==B.Y && A.Z==B.Z;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 89, execEqualEqual_VectorVector );

void UObject::execNotEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(DWORD*)Result = A.X!=B.X || A.Y!=B.Y || A.Z!=B.Z;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 90, execNotEqual_VectorVector );

void UObject::execDot_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FLOAT*)Result = A|B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 91, execDot_VectorVector );

void UObject::execCross_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A^B;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 92, execCross_VectorVector );

void UObject::execMultiplyEqual_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FVector*)Result = (*A *= B);
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 93, execMultiplyEqual_VectorFloat );

void UObject::execMultiplyEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A *= B);
}	
IMPLEMENT_FUNCTION( UObject, 297, execMultiplyEqual_VectorVector );

void UObject::execDivideEqual_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	if (B == 0.f)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Divide by zero"));
	}

	*(FVector*)Result = (*A /= B);
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 94, execDivideEqual_VectorFloat );

void UObject::execAddEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A += B);
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 95, execAddEqual_VectorVector );

void UObject::execSubtractEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A -= B);
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 96, execSubtractEqual_VectorVector );

void UObject::execVSize( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_FINISH;

	*(FLOAT*)Result = A.Size();
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 97, execVSize );

void UObject::execVSize2D( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_FINISH;

	*(FLOAT*)Result = A.Size2D();
}
IMPLEMENT_FUNCTION( UObject, -1, execVSize2D );

void UObject::execVSizeSq( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_FINISH;

	*(FLOAT*)Result = A.SizeSquared();
}
IMPLEMENT_FUNCTION( UObject, -1, execVSizeSq );

void UObject::execNormal( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_FINISH;

	*(FVector*)Result = A.SafeNormal();
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 98, execNormal );

void UObject::execVLerp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A + V*(B-A);
}	
IMPLEMENT_FUNCTION( UObject, -1, execVLerp );

void UObject::execVSmerp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A + V*V*(3.f - 2.f*V)*(B-A);
}
IMPLEMENT_FUNCTION( UObject, -1, execVSmerp );

void UObject::execVRand( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*((FVector*)Result) = VRand();
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 124, execVRand );

void UObject::execRotRand( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL_OPTX(bRoll, 0);
	P_FINISH;

	FRotator RRot;
	RRot.Yaw = ((2 * appRand()) % 65535);
	RRot.Pitch = ((2 * appRand()) % 65535);
	if ( bRoll )
		RRot.Roll = ((2 * appRand()) % 65535);
	else
		RRot.Roll = 0;
	*((FRotator*)Result) = RRot;
}
IMPLEMENT_FUNCTION( UObject, 320, execRotRand );

void UObject::execMirrorVectorByNormal( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	B = B.SafeNormal();
	*(FVector*)Result = A - 2.f * B * (B | A);
}
IMPLEMENT_FUNCTION( UObject, 300, execMirrorVectorByNormal );

//scion ===========================================================
//	SCION Function: execProjectOnTo
//	Author: superville
//
//	Description: Projects on vector (X) onto another (Y) and returns
//		the projected vector without modifying either of the arguments
//
//	Last Modified: 03/06/04 (superville)
// ================================================================
void UObject::execProjectOnTo(FFrame &Stack, RESULT_DECL)
{
	P_GET_VECTOR( X );
	P_GET_VECTOR( Y );
	P_FINISH;

	*(FVector*)Result = X.ProjectOnTo( Y );
}
IMPLEMENT_FUNCTION(UObject,1500,execProjectOnTo);

//scion ===========================================================
//	SCION Function: execIsZero
//	Author: superville
//
//	Description: Returns true if all components of the vector are zero
//
//	Last Modified: 03/06/04 (superville)
// ================================================================
void UObject::execIsZero(FFrame &Stack, RESULT_DECL)
{
	P_GET_VECTOR(A);
	P_FINISH;

	*(UBOOL*)Result = (A.X == 0 && A.Y == 0 && A.Z == 0);
}
IMPLEMENT_FUNCTION(UObject,1501,execIsZero);

//////////////////////////////////////
// Rotation operators and functions //
//////////////////////////////////////

void UObject::execEqualEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(DWORD*)Result = A.Pitch==B.Pitch && A.Yaw==B.Yaw && A.Roll==B.Roll;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 14, execEqualEqual_RotatorRotator );

void UObject::execNotEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(DWORD*)Result = A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 75, execNotEqual_RotatorRotator );

void UObject::execMultiply_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = A * B;
}	
IMPLEMENT_FUNCTION( UObject, 287, execMultiply_RotatorFloat );

void UObject::execMultiply_FloatRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = B * A;
}	
IMPLEMENT_FUNCTION( UObject, 288, execMultiply_FloatRotator );

void UObject::execDivide_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	if (B == 0.f)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Divide by zero"));
	}

	*(FRotator*)Result = A * (1.f/B);
}	
IMPLEMENT_FUNCTION( UObject, 289, execDivide_RotatorFloat );

void UObject::execMultiplyEqual_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = (*A *= B);
}	
IMPLEMENT_FUNCTION( UObject, 290, execMultiplyEqual_RotatorFloat );

void UObject::execDivideEqual_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	if (B == 0.f)
	{
		Stack.Logf(NAME_ScriptWarning,TEXT("Divide by zero"));
	}

	*(FRotator*)Result = (*A *= (1.f/B));
}	
IMPLEMENT_FUNCTION( UObject, 291, execDivideEqual_RotatorFloat );

void UObject::execAdd_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A + B;
}
IMPLEMENT_FUNCTION( UObject, 316, execAdd_RotatorRotator );

void UObject::execSubtract_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A - B;
}
IMPLEMENT_FUNCTION( UObject, 317, execSubtract_RotatorRotator );

void UObject::execAddEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR_REF(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = (*A += B);
}
IMPLEMENT_FUNCTION( UObject, 318, execAddEqual_RotatorRotator );

void UObject::execSubtractEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR_REF(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = (*A -= B);
}
IMPLEMENT_FUNCTION( UObject, 319, execSubtractEqual_RotatorRotator );

void UObject::execGetAxes( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FRotationMatrix R(A);
	*X = R.GetAxis(0);
	*Y = R.GetAxis(1);
	*Z = R.GetAxis(2);
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 101, execGetAxes );

void UObject::execGetUnAxes( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FMatrix R = FRotationMatrix(A).Transpose();
	*X = R.GetAxis(0);
	*Y = R.GetAxis(1);
	*Z = R.GetAxis(2);
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 102, execGetUnAxes );

void UObject::execOrthoRotation( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(X);
	P_GET_VECTOR(Y);
	P_GET_VECTOR(Z);
	P_FINISH;

	FMatrix M = FMatrix::Identity;
	M.SetAxis( 0, X );
	M.SetAxis( 1, Y );
	M.SetAxis( 2, Z );

	*(FRotator*)Result = M.Rotator();
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execOrthoRotation );

void UObject::execNormalize( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(Rot);
	P_FINISH;

	*(FRotator*)Result = Rot.Normalize();
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execNormalize );

void UObject::execRLerp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A + V*(B-A);
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execRLerp );

void UObject::execRSmerp( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(V);
	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A + V*V*(3.f - 2.f*V)*(B-A);
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execRSmerp );

void UObject::execClockwiseFrom_IntInt( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(IntA);
	P_GET_INT(IntB);
	P_FINISH;

	IntA = IntA & 0xFFFF;
	IntB = IntB & 0xFFFF;

	*(DWORD*)Result = ( Abs(IntA - IntB) > 32768 ) ? ( IntA < IntB ) : ( IntA > IntB );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execClockwiseFrom_IntInt );

////////////////////////////////////
// Quaternion functions           //
////////////////////////////////////

void UObject::execQuatProduct( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FQuat, A);
	P_GET_STRUCT(FQuat, B);
	P_FINISH;

	*(FQuat*)Result = A * B;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatProduct);

void UObject::execQuatInvert( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FQuat, A);
	P_FINISH;

	FQuat invA(-A.X, -A.Y, -A.Z, A.W);
	*(FQuat*)Result = invA;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatInvert);

void UObject::execQuatRotateVector( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FQuat, A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A.RotateVector(B);
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatRotateVector);

// Generate the 'smallest' (geodesic) rotation between these two vectors.
void UObject::execQuatFindBetween( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	FVector cross = A ^ B;
	FLOAT crossMag = cross.Size();

	// If these vectors are basically parallel - just return identity quaternion (ie no rotation).
	if(crossMag < KINDA_SMALL_NUMBER)
	{
		*(FQuat*)Result = FQuat::Identity;
		return;
	}

	FLOAT angle = appAsin(crossMag);

	FLOAT dot = A | B;
	if(dot < 0.f)
		angle = PI - angle;

	FLOAT sinHalfAng = appSin(0.5f * angle);
	FLOAT cosHalfAng = appCos(0.5f * angle);
	FVector axis = cross / crossMag;

	*(FQuat*)Result = FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatFindBetween);

void UObject::execQuatFromAxisAndAngle( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Axis);
	P_GET_FLOAT(Angle);
	P_FINISH;

	FLOAT sinHalfAng = appSin(0.5f * Angle);
	FLOAT cosHalfAng = appCos(0.5f * Angle);
	FVector normAxis = Axis.SafeNormal();

	*(FQuat*)Result = FQuat(
		sinHalfAng * normAxis.X,
		sinHalfAng * normAxis.Y,
		sinHalfAng * normAxis.Z,
		cosHalfAng );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatFromAxisAndAngle);

void UObject::execQuatFromRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(A);
	P_FINISH;

	*(FQuat*)Result = FQuat( FRotationMatrix(A) );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatFromRotator);

void UObject::execQuatToRotator( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FQuat, A);
	P_FINISH;

	*(FRotator*)Result = FMatrix( FVector(0.f), A).Rotator();
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatToRotator);

////////////////////////////////////
// Str operators and functions //
////////////////////////////////////

void UObject::execEatString( FFrame& Stack, RESULT_DECL )
{
	// Call function returning a string, then discard the result.
	FString String;
	Stack.Step( this, &String );
}
IMPLEMENT_FUNCTION( UObject, EX_EatString, execEatString );

void UObject::execConcat_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (A+B);
}
IMPLEMENT_FUNCTION( UObject, 112, execConcat_StringString );

void UObject::execAt_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (A+TEXT(" ")+B);
}
IMPLEMENT_FUNCTION( UObject, 168, execAt_StringString );

void UObject::execLess_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)<0;;
}
IMPLEMENT_FUNCTION( UObject, 115, execLess_StringString );

void UObject::execGreater_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)>0;
}
IMPLEMENT_FUNCTION( UObject, 116, execGreater_StringString );

void UObject::execLessEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)<=0;
}
IMPLEMENT_FUNCTION( UObject, 120, execLessEqual_StringString );

void UObject::execGreaterEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)>=0;
}
IMPLEMENT_FUNCTION( UObject, 121, execGreaterEqual_StringString );

void UObject::execEqualEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)==0;
}
IMPLEMENT_FUNCTION( UObject, 122, execEqualEqual_StringString );

void UObject::execNotEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)!=0;
}
IMPLEMENT_FUNCTION( UObject, 123, execNotEqual_StringString );

void UObject::execComplementEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStricmp(*A,*B)==0;
}
IMPLEMENT_FUNCTION( UObject, 124, execComplementEqual_StringString );

void UObject::execLen( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(S);
	P_FINISH;

	*(INT*)Result = S.Len();
}
IMPLEMENT_FUNCTION( UObject, 125, execLen );

void UObject::execInStr( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(S);
	P_GET_STR(A);
	P_FINISH;
	*(INT*)Result = S.InStr(A);
}
IMPLEMENT_FUNCTION( UObject, 126, execInStr );

void UObject::execMid( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_INT(I);
	P_GET_INT_OPTX(C,65535);
	P_FINISH;

	*(FString*)Result = A.Mid(I,C);
}
IMPLEMENT_FUNCTION( UObject, 127, execMid );

void UObject::execLeft( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_INT(N);
	P_FINISH;

	*(FString*)Result = A.Left(N);
}
IMPLEMENT_FUNCTION( UObject, 128, execLeft );

void UObject::execRight( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_GET_INT(N);
	P_FINISH;

	*(FString*)Result = A.Right(N);
}
IMPLEMENT_FUNCTION( UObject, 234, execRight );

void UObject::execCaps( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(A);
	P_FINISH;

	*(FString*)Result = A.Caps();
}
IMPLEMENT_FUNCTION( UObject, 235, execCaps );

void UObject::execChr( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(i);
	P_FINISH;

	TCHAR Temp[2];
	Temp[0] = i;
	Temp[1] = 0;
	*(FString*)Result = Temp;
}
IMPLEMENT_FUNCTION( UObject, 236, execChr );

void UObject::execAsc( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(S);
	P_FINISH;

	*(INT*)Result = **S;	
}
IMPLEMENT_FUNCTION( UObject, 237, execAsc );

/////////////////////////////////////////
// Native name operators and functions //
/////////////////////////////////////////

void UObject::execEqualEqual_NameName( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(A);
	P_GET_NAME(B);
	P_FINISH;

	*(DWORD*)Result = A == B;
}
IMPLEMENT_FUNCTION( UObject, 254, execEqualEqual_NameName );

void UObject::execNotEqual_NameName( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(A);
	P_GET_NAME(B);
	P_FINISH;

	*(DWORD*)Result = A != B;
}
IMPLEMENT_FUNCTION( UObject, 255, execNotEqual_NameName );

////////////////////////////////////
// Object operators and functions //
////////////////////////////////////

void UObject::execEqualEqual_ObjectObject( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UObject,A);
	P_GET_OBJECT(UObject,B);
	P_FINISH;

	*(DWORD*)Result = A == B;
}
IMPLEMENT_FUNCTION( UObject, 114, execEqualEqual_ObjectObject );

void UObject::execNotEqual_ObjectObject( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UObject,A);
	P_GET_OBJECT(UObject,B);
	P_FINISH;

	*(DWORD*)Result = A != B;
}
IMPLEMENT_FUNCTION( UObject, 119, execNotEqual_ObjectObject );

/////////////////////////////
// Log and error functions //
/////////////////////////////

void UObject::execLog( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(S);
	P_GET_NAME_OPTX(N,NAME_ScriptLog);
	P_FINISH;

	debugf( (EName)N.GetIndex(), TEXT("%s"), *S );
}
IMPLEMENT_FUNCTION( UObject, 231, execLog );

void UObject::execWarn( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(S);
	P_FINISH;

	Stack.Logf( TEXT("%s"), *S );
}
IMPLEMENT_FUNCTION( UObject, 232, execWarn );

void UObject::execLocalize( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(SectionName);
	P_GET_STR(KeyName);
	P_GET_STR(PackageName);
	P_FINISH;

	*(FString*)Result = Localize( *SectionName, *KeyName, *PackageName );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execLocalize );

//////////////////
// High natives //
//////////////////

#define HIGH_NATIVE(n) \
void UObject::execHighNative##n( FFrame& Stack, RESULT_DECL ) \
{ \
	BYTE B = *Stack.Code++; \
	(this->*GNatives[ n*0x100 + B ])( Stack, Result ); \
} \
IMPLEMENT_FUNCTION( UObject, 0x60 + n, execHighNative##n );

HIGH_NATIVE(0);
HIGH_NATIVE(1);
HIGH_NATIVE(2);
HIGH_NATIVE(3);
HIGH_NATIVE(4);
HIGH_NATIVE(5);
HIGH_NATIVE(6);
HIGH_NATIVE(7);
HIGH_NATIVE(8);
HIGH_NATIVE(9);
HIGH_NATIVE(10);
HIGH_NATIVE(11);
HIGH_NATIVE(12);
HIGH_NATIVE(13);
HIGH_NATIVE(14);
HIGH_NATIVE(15);
#undef HIGH_NATIVE

/////////////////////////
// Object construction //
/////////////////////////

void UObject::execNew( FFrame& Stack, RESULT_DECL )
{
	// Get parameters.
	P_GET_OBJECT_OPTX(UObject,Outer,NULL);
	P_GET_STR_OPTX(Name,TEXT(""));
	P_GET_INT_OPTX(Flags,0);
	P_GET_OBJECT_OPTX(UClass,Cls,NULL);

	// Validate parameters.
	if( Flags & ~RF_ScriptMask )
		Stack.Logf( TEXT("new: Flags %08X not allowed"), Flags & ~RF_ScriptMask );

	// Construct new object.
	if( !Outer )
		Outer = GetTransientPackage();
	*(UObject**)Result = StaticConstructObject( Cls, Outer, Name.Len()?FName(*Name):NAME_None, Flags&RF_ScriptMask, NULL, &Stack );
}
IMPLEMENT_FUNCTION( UObject, EX_New, execNew );

/////////////////////////////
// Class related functions //
/////////////////////////////

void UObject::execClassIsChildOf( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,K);
	P_GET_OBJECT(UClass,C);
	P_FINISH;

	*(DWORD*)Result = (C && K) ? K->IsChildOf(C) : 0;
}
IMPLEMENT_FUNCTION( UObject, 258, execClassIsChildOf );

///////////////////////////////
// State and label functions //
///////////////////////////////

void UObject::execGotoState( FFrame& Stack, RESULT_DECL )
{
	FName CurrentStateName = (StateFrame && StateFrame->StateNode!=Class) ? StateFrame->StateNode->GetFName() : FName(NAME_None);
	P_GET_NAME_OPTX( S, CurrentStateName );
	P_GET_NAME_OPTX( L, NAME_None );
	P_GET_UBOOL_OPTX( bForceEvents, 0 );
	P_FINISH;

	// Go to the state.
	EGotoState ResultState = GOTOSTATE_Success;
	if( S!=CurrentStateName || bForceEvents)
	{
		ResultState = GotoState( S, bForceEvents );
	}

	// Handle success.
	if( ResultState==GOTOSTATE_Success )
	{
		// Now go to the label.
		if( !GotoLabel( L==NAME_None ? FName(NAME_Begin) : L ) && L!=NAME_None )
		{
			Stack.Logf( TEXT("GotoState (%s %s): Label not found"), *S, *L );
		}
	}
	else if( ResultState==GOTOSTATE_NotFound )
	{
		// Warning.
		if( S!=NAME_None && S!=NAME_Auto )
			Stack.Logf( TEXT("GotoState (%s %s): State not found"), *S, *L );
		else if ( S==NAME_None && GDebugger )
			GDebugger->DebugInfo(this, &Stack,DI_PrevStackState,0,0);
	}
	else
	{
		// Safely preempted by another GotoState.
	}
}
IMPLEMENT_FUNCTION( UObject, 113, execGotoState );

void UObject::execPushState(FFrame &Stack, RESULT_DECL)
{
	P_GET_NAME(newState);
	P_GET_NAME_OPTX(newLabel, NAME_None);
	P_FINISH;
	// if we support states
	if (StateFrame != NULL)
	{
		// find the new state
		UState *stateNode = FindState(newState);
		if (stateNode != NULL)
		{
			// notify the current state
			eventPausedState();
			// save the current state information
			INT idx = StateFrame->StateStack.AddZeroed();
			StateFrame->StateStack(idx).State = StateFrame->StateNode;
			StateFrame->StateStack(idx).Node = StateFrame->Node;
			StateFrame->StateStack(idx).Code = StateFrame->Code;
			// push the new state
			StateFrame->StateNode = stateNode;
			StateFrame->Node = stateNode;
			StateFrame->Code = NULL;
			StateFrame->ProbeMask = (stateNode->ProbeMask | GetClass()->ProbeMask) & stateNode->IgnoreMask;
			// and send pushed event
			eventPushedState();
			// and jump to label if specified
			GotoLabel(newLabel != NAME_None ? newLabel : NAME_Begin);
		}
		else
		{
			debugf(NAME_Warning,TEXT("Failed to find state %s"),*newState);
		}
	}
}
IMPLEMENT_FUNCTION(UObject, -1, execPushState);

void UObject::execPopState(FFrame &Stack, RESULT_DECL)
{
	P_GET_UBOOL_OPTX(bPopAll,0);
	P_FINISH;
	// if we support states, and we have a nested state
	if (StateFrame != NULL &&
		StateFrame->StateNode != NULL &&
		StateFrame->StateStack.Num())
	{
		// if popping all then find the lowest state, otherwise go one level up
		INT popCnt = 0;
		while (StateFrame->StateStack.Num() &&
			   (bPopAll ||
				popCnt == 0))
		{
			// send the popped event
			eventPoppedState();
			// grab the new desired state
			INT idx = StateFrame->StateStack.Num() - 1;
			UState *stateNode = StateFrame->StateStack(idx).State;
			UStruct *node = StateFrame->StateStack(idx).Node;
			BYTE *code = StateFrame->StateStack(idx).Code;
			// remove from the current stack
			StateFrame->StateStack.Pop();
			// and set the new state
			StateFrame->StateNode = stateNode;
			StateFrame->Node = node;
			StateFrame->Code = code;
			StateFrame->ProbeMask = (stateNode->ProbeMask | GetClass()->ProbeMask) & stateNode->IgnoreMask;
			popCnt++;
			// send continue event
			eventContinuedState();
		}
	}
	else
	{
		debugf(NAME_Warning,TEXT("%s called PopState() w/o a valid state stack!"),GetName());
	}
}
IMPLEMENT_FUNCTION(UObject, -1, execPopState);

void UObject::execEnable( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(N);
	if( N.GetIndex()>=NAME_PROBEMIN && N.GetIndex()<NAME_PROBEMAX && StateFrame )
	{
		QWORD BaseProbeMask = (GetStateFrame()->StateNode->ProbeMask | GetClass()->ProbeMask) & GetStateFrame()->StateNode->IgnoreMask;
		GetStateFrame()->ProbeMask |= (BaseProbeMask & ((QWORD)1<<(N.GetIndex()-NAME_PROBEMIN)));
	}
	else Stack.Logf( TEXT("Enable: '%s' is not a probe function"), *N );
	P_FINISH;
}
IMPLEMENT_FUNCTION( UObject, 117, execEnable );

void UObject::execDisable( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(N);
	P_FINISH;

	if( N.GetIndex()>=NAME_PROBEMIN && N.GetIndex()<NAME_PROBEMAX && StateFrame )
		GetStateFrame()->ProbeMask &= ~((QWORD)1<<(N.GetIndex()-NAME_PROBEMIN));
	else
		Stack.Logf( TEXT("Enable: '%s' is not a probe function"), *N );
}
IMPLEMENT_FUNCTION( UObject, 118, execDisable );

///////////////////
// Property text //
///////////////////

void UObject::execGetPropertyText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(PropName);
	P_FINISH;

	UProperty* Property=FindField<UProperty>( Class, *PropName );
	if( Property && (Property->GetFlags() & RF_Public) )
	{
		FString	Temp;
		Property->ExportText( 0, Temp, (BYTE*)this, (BYTE*)this, this, PPF_Localized );
		*(FString*)Result = Temp;
	}
	else *(FString*)Result = TEXT("");
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetPropertyText );

void UObject::execSetPropertyText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(PropName);
	P_GET_STR(PropValue);
	P_FINISH;

	UProperty* Property=FindField<UProperty>( Class, *PropName );
	if
	(	(Property)
	&&	(Property->GetFlags() & RF_Public)
	&&	!(Property->PropertyFlags & CPF_Const) )
		Property->ImportText( *PropValue, (BYTE*)this + Property->Offset, PPF_Localized, this );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execSetPropertyText );

void UObject::execSaveConfig( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	SaveConfig();
}
IMPLEMENT_FUNCTION( UObject, 536, execSaveConfig);

void UObject::execStaticSaveConfig( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Class->GetDefaultObject()->SaveConfig();
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execStaticSaveConfig);

void UObject::execGetEnum( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UObject,E);
	P_GET_INT(i);
	P_FINISH;

	*(FName*)Result = NAME_None;
	if( Cast<UEnum>(E) && i>=0 && i<Cast<UEnum>(E)->Names.Num() )
		*(FName*)Result = Cast<UEnum>(E)->Names(i);
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetEnum);

void UObject::execDynamicLoadObject( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Name);
	P_GET_OBJECT(UClass,Class);
	P_GET_UBOOL_OPTX(bMayFail,0);
	P_FINISH;

	*(UObject**)Result = StaticLoadObject( Class, NULL, *Name, NULL, LOAD_NoWarn | (bMayFail?LOAD_Quiet:0), NULL );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execDynamicLoadObject );

void UObject::execFindObject( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Name);
	P_GET_OBJECT(UClass,Class);
	P_FINISH;

	*(UObject**)Result = StaticFindObject( Class, NULL, *Name );
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execFindObject );

void UObject::execIsInState( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(StateName);
	P_FINISH;

	if( StateFrame )
		for( UState* Test=StateFrame->StateNode; Test; Test=Test->GetSuperState() )
			if( Test->GetFName()==StateName )
				{*(DWORD*)Result=1; return;}
	*(DWORD*)Result = 0;
}
IMPLEMENT_FUNCTION( UObject, 281, execIsInState );

void UObject::execGetStateName( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FName*)Result = (StateFrame && StateFrame->StateNode) ? StateFrame->StateNode->GetFName() : FName(NAME_None);
}
IMPLEMENT_FUNCTION( UObject, 284, execGetStateName );

void UObject::execGetFuncName(FFrame &Stack, RESULT_DECL)
{
	P_FINISH;
	*(FName*)Result = Stack.Node != NULL ? Stack.Node->GetFName() : NAME_None;
}
IMPLEMENT_FUNCTION(UObject,-1,execGetFuncName);

void UObject::execScriptTrace(FFrame &Stack, RESULT_DECL)
{
	P_FINISH;
	// grab the call stack
	TArray<FFrame*> frameStack;
	FFrame *frame = &Stack;
	while (frame != NULL)
	{
		frameStack.AddItem(frame);
		frame = frame->PreviousFrame;
	}
	// and log it
	debugf(TEXT("Script call stack:"));
	TCHAR spacer[256] = TEXT("\0");
	for (INT idx = frameStack.Num() - 1; idx >= 0; idx--)
	{
		debugf(TEXT("%s-> %s"),spacer,frameStack(idx)->Node->GetFullName());
		appStrcat(spacer,TEXT("  "));
	}
}
IMPLEMENT_FUNCTION(UObject,-1,execScriptTrace);

void UObject::execIsA( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME(ClassName);
	P_FINISH;

	UClass* TempClass;
	for( TempClass=GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
		if( TempClass->GetFName() == ClassName )
			break;
	*(DWORD*)Result = (TempClass!=NULL);
}
IMPLEMENT_FUNCTION(UObject,303,execIsA);

/*-----------------------------------------------------------------------------
	Native iterator functions.
-----------------------------------------------------------------------------*/

void UObject::execIterator( FFrame& Stack, RESULT_DECL )
{}
IMPLEMENT_FUNCTION( UObject, EX_Iterator, execIterator );

/*-----------------------------------------------------------------------------
	Native registry.
-----------------------------------------------------------------------------*/


// Optimizations break GRegisterNative with static linking on the PC, causing UObject::execUndefined to be called early on during script execution.
#if _MSC_VER > 1500 && !defined XBOX
#error re-evaluate need for workaround
#endif
#pragma DISABLE_OPTIMIZATION
//
// Register a native function.
// Warning: Called at startup time, before engine initialization.
//
BYTE GRegisterNative( INT iNative, const Native& Func )
{
	static int Initialized = 0;
	if( !Initialized )
	{
		Initialized = 1;
		for( int i=0; i<ARRAY_COUNT(GNatives); i++ )
			GNatives[i] = &UObject::execUndefined;
	}
	if( iNative != INDEX_NONE )
	{
		if( iNative<0 || iNative>ARRAY_COUNT(GNatives) || GNatives[iNative]!=&UObject::execUndefined) 
			GNativeDuplicate = iNative;
		GNatives[iNative] = Func;
	}
	return 0;
}
#pragma ENABLE_OPTIMIZATION

BYTE GRegisterCast( INT CastCode, const Native& Func )
{
	static int Initialized = 0;
	if( !Initialized )
	{
		Initialized = 1;
		for( int i=0; i<ARRAY_COUNT(GCasts); i++ )
			GCasts[i] = &UObject::execUndefined;
	}
	if( CastCode != INDEX_NONE )
	{
		if( CastCode<0 || CastCode>ARRAY_COUNT(GCasts) || GCasts[CastCode]!=&UObject::execUndefined) 
			GCastDuplicate = CastCode;
		GCasts[CastCode] = Func;
	}
	return 0;
}


/*-----------------------------------------------------------------------------
	Call graph profiling.
-----------------------------------------------------------------------------*/

/**
 * Constructor
 *
 * @param InSoftMemoryLimit		Max number of bytes used by call graph data (soft limit).
 */
FScriptCallGraph::FScriptCallGraph( DWORD InSoftMemoryLimit )
{
	check( InSoftMemoryLimit );
	Reset( InSoftMemoryLimit );
}

/**
 * Resets data collection and memory use.
 *
 * @param InSoftMemoryLimit		Max number of bytes used by call graph data (soft limit).
 */
void FScriptCallGraph::Reset( DWORD InSoftMemoryLimit )
{
	if( InSoftMemoryLimit )
	{
		SoftMemoryLimit = InSoftMemoryLimit;
	}

	Data.Empty( SoftMemoryLimit * 11 / 10 / sizeof(DWORD) );
}

/**
 * Serializes the profiling data to the passed in archive.	
 *
 * @param Ar	Archive to serialize data to.
 */
void FScriptCallGraph::Serialize( FArchive& Ar )
{
	if( Data.Num() )
	{
		INT StartIndex	= 0;
		INT EndIndex	= 0;

		// Skip the first frame as we most likely are in the middle of the call stack.
		while( StartIndex<Data.Num() && !IsFrameEndMarker(Data(StartIndex)) )
		{
			StartIndex++;
		}
		check(StartIndex<Data.Num());

		// Find the end of the call graph data.
		EndIndex = StartIndex;
		while( EndIndex<Data.Num() && !IsEndMarker(Data(EndIndex)) )
		{
			EndIndex++;
		}

		// Create a list of all functions currently loaded.
		TArray<UFunction*> Functions;
		for( TObjectIterator<UFunction> It; It; ++It )
		{
			Functions.AddItem( *It );
		}

		// First value to serialize is the number of usecs per cycle. (DOUBLE)
		DOUBLE usecsPerCycle = GSecondsPerCycle * 1000.0 * 1000.0;
		Ar << usecsPerCycle;

		// Second value is the number of functions that are being serialized (DWORD).
		DWORD FunctionCount = Functions.Num();
		Ar << FunctionCount;

		for( INT FunctionIndex=0; FunctionIndex<Functions.Num(); FunctionIndex++ )
		{
			FString			Name		= Functions(FunctionIndex)->GetPathName();
			DWORD			Length		= Name.Len();
			DWORD			Pointer		= (DWORD) Functions(FunctionIndex);
		
			// Serialize function pointer, length of name in characters and then serialize name without the
			// trailing '\0' as a chain of ANSI characters.
			Ar << Pointer;	// (DWORD)
			Ar << Length;	// (DWORD)
			for( DWORD i=0; i<Length; i++ )
			{
				ANSICHAR ACh = ToAnsi( Name[i] );
				Ar << ACh;	// (ANSICHAR)
			}
		}

		// Serialize raw collected data as tokens of DWORDs.
		Ar.Serialize( &Data(StartIndex), (EndIndex - StartIndex) * sizeof(DWORD) );
	}
}

/**
 * Emits the end of frame marker. Needs to be called when there are no script functions
 * on the stack. Also responsible for reseting call graph data if memory limit is reached.
 */
void FScriptCallGraph::Tick()
{
	EmitFrameEnd();
	if( (Data.Num()*sizeof(DWORD)) > SoftMemoryLimit )
	{
		Reset( SoftMemoryLimit );
	}
}

/** Global script call graph profiler */
FScriptCallGraph* GScriptCallGraph = NULL;

/**
 * Helper class for call graph profiling. Scoped to correctly deal with multiple return
 * paths.
 */
struct FScopedScriptStats
{
	/** 
	 * Constructor, emiting function pointer to stream.
	 *
	 * @param InFunction	Function about to be called.
	 */
	FScopedScriptStats( UFunction* InFunction )
	{
		if( GScriptCallGraph )
		{
			StartCycles = appCycles();
			Function	= InFunction;
			GScriptCallGraph->EmitFunction( Function );
		}
	}

	/**
	 * Destructor, emiting cycles spent in function call.	
	 */
	~FScopedScriptStats()
	{
		if( GScriptCallGraph )
		{
			DWORD Cycles = appCycles() - StartCycles;
			GScriptCallGraph->EmitCycles( Cycles );
		}
	}

private:
	/** Function associated with callss */
	UFunction*	Function;
	/** Cycle count before function was being called. */
	DWORD		StartCycles;
};

/*-----------------------------------------------------------------------------
	Script processing function.
-----------------------------------------------------------------------------*/

//
// Information remembered about an Out parameter.
//
struct FOutParmRec
{
	UProperty* Property;
	BYTE*      PropAddr;
};

//
// Call a function.
//
void UObject::CallFunction( FFrame& Stack, RESULT_DECL, UFunction* Function )
{
	FScopedScriptStats ScriptStats( Function );

	// Found it.
	UBOOL SkipIt = 0;
	if( Function->iNative )
	{
		// Call native final function.
		(this->*Function->Func)( Stack, Result );
	}
	else if( Function->FunctionFlags & FUNC_Native )
	{
		// Call native networkable function.
		BYTE Buffer[1024];
		if( !ProcessRemoteFunction( Function, Buffer, &Stack ) )
		{
			// Call regular native function.
			(this->*Function->Func)( Stack, Result );
		}
		else
		{
			// Eat up the remaining parameters in the stream.
			SkipIt = 1;
			goto Temporary;
		}
	}
	else
	{
		// Make new stack frame in the current context.
		Temporary:
		BYTE* Frame = (BYTE*)appAlloca(Function->PropertiesSize);
		appMemzero( Frame, Function->PropertiesSize );
		FFrame NewStack( this, Function, 0, Frame, &Stack );
		FOutParmRec Outs[MAX_FUNC_PARMS], *Out = Outs;
		for( UProperty* Property=(UProperty*)Function->Children; *Stack.Code!=EX_EndFunctionParms; Property=(UProperty*)Property->Next )
		{
			GPropAddr = NULL;
			GPropObject = NULL;
			BYTE* Param = NewStack.Locals + Property->Offset;
			Stack.Step( Stack.Object, Param );
#if !__INTEL_BYTE_ORDER__
			if( Property->GetID()==NAME_BoolProperty  )
 			{
				if( *(DWORD*)(Param) )
					*(DWORD*)(Param) = FIRST_BITFIELD;
				else
					*(DWORD*)(Param) = 0;
			}
#endif
			if( (Property->PropertyFlags & CPF_OutParm) && GPropAddr )
			{
				Out->PropAddr = GPropAddr;
				Out->Property = Property;
				Out++;
				if ( GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net) )
					GPropObject->NetDirty(GProperty);
			}
		}
		Stack.Code++;

		//DEBUGGER
		// This is necessary until I find a better solution, otherwise, 
		// when the DebugInfo gets read out of the bytecode, it'll be called 
		// AFTER the function returns, which is not useful. This is one of the
		// few places I check for debug information.
		if ( *Stack.Code == EX_DebugInfo )
			Stack.Step( Stack.Object, NULL );

		// Execute the code.
		if( !SkipIt )
			ProcessInternal( NewStack, Result );

		// Copy back outparms.
		while( --Out >= Outs )
			Out->Property->CopyCompleteValue( Out->PropAddr, NewStack.Locals + Out->Property->Offset );

		// Destruct properties on the stack.
		for( UProperty* Destruct=Function->ConstructorLink; Destruct; Destruct=Destruct->ConstructorLinkNext )
			Destruct->DestroyValue( NewStack.Locals + Destruct->Offset );
	}
}

//
// Internal function call processing.
// @warning: might not write anything to Result if singular or proper type isn't returned.
//
void UObject::ProcessInternal( FFrame& Stack, RESULT_DECL )
{
	DWORD SingularFlag = ((UFunction*)Stack.Node)->FunctionFlags & FUNC_Singular;
	if
	(	!ProcessRemoteFunction( (UFunction*)Stack.Node, Stack.Locals, NULL )
	&&	IsProbing( Stack.Node->GetFName() )
	&&	!(ObjectFlags & SingularFlag) )
	{
		ObjectFlags |= SingularFlag;
		BYTE Buffer[1024];//@hack hardcoded size
		appMemzero( Buffer, sizeof(FString) );//@hack
#if DO_GUARD
		if( ++Recurse > RECURSE_LIMIT )
		{
			if ( GDebugger && GDebugger->NotifyInfiniteLoop() )
			{
				Recurse = 0;
			}
			else
			{
				Stack.Logf( NAME_Critical, TEXT("Infinite script recursion (%i calls) detected"), RECURSE_LIMIT );
			}
		}
#endif
		while( *Stack.Code != EX_Return )
			Stack.Step( Stack.Object, Buffer );
		Stack.Code++;
		Stack.Step( Stack.Object, Result );

		//DEBUGGER: Necessary for the call stack. Grab an optional 'PREVSTACK' debug info.
		if ( *Stack.Code == EX_DebugInfo )
			Stack.Step( Stack.Object, Result );
		
		ObjectFlags &= ~SingularFlag;
#if DO_GUARD
		--Recurse;
#endif
	}
}

//
// Script processing functions.
//
void UObject::ProcessEvent( UFunction* Function, void* Parms, void* UnusedResult )
{
	static INT ScriptEntryTag = 0;

	// Reject.
	if
	(	!IsProbing( Function->GetFName() )
	||	IsPendingKill()
	||	Function->iNative
	||	((Function->FunctionFlags & FUNC_Native) && ProcessRemoteFunction( Function, Parms, NULL )) )
		return;
	checkSlow(Function->ParmsSize==0 || Parms!=NULL);

	if(++ScriptEntryTag == 1)
	{
		clock(GScriptCycles);
	}

	// Scope required for scoped script stats.
	{
		FScopedScriptStats ScriptStats( Function );
		
		// Create a new local execution stack.
		FFrame NewStack( this, Function, 0, appAlloca(Function->PropertiesSize) );
		appMemcpy( NewStack.Locals, Parms, Function->ParmsSize );
		appMemzero( NewStack.Locals+Function->ParmsSize, Function->PropertiesSize-Function->ParmsSize );

		// Call native function or UObject::ProcessInternal.
		(this->*Function->Func)( NewStack, NewStack.Locals+Function->ReturnValueOffset );

		// Copy everything back.
		appMemcpy( Parms, NewStack.Locals, Function->ParmsSize );

		// Destroy local variables except function parameters.!! see also UObject::ScriptConsoleExec
		for( UProperty* P=Function->ConstructorLink; P; P=P->ConstructorLinkNext )
			if( P->Offset >= Function->ParmsSize )
				P->DestroyValue( NewStack.Locals + P->Offset );
	}

	if(--ScriptEntryTag == 0)
	{
		unclock(GScriptCycles);
	}
}

void UObject::ProcessDelegate( FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* UnusedResult )
{
	if( Delegate->Object && Delegate->Object->IsPendingKill() )
	{
		Delegate->Object = NULL;
		Delegate->FunctionName = NAME_None;
	}
	if( Delegate->Object )
		Delegate->Object->ProcessEvent( Delegate->Object->FindFunctionChecked(Delegate->FunctionName), Parms, UnusedResult );
	else
		ProcessEvent( FindFunctionChecked(DelegateName), Parms, UnusedResult );
}

//
// Execute the state code of the object.
//
void UObject::ProcessState( FLOAT DeltaSeconds )
{}

//
// Process a remote function; returns 1 if remote, 0 if local.
//
UBOOL UObject::ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

