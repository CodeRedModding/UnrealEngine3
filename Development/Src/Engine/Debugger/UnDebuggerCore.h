/*=============================================================================
	UnDebuggerCore.h: Debugger Core Logic
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
=============================================================================*/

#ifndef __UNDEBUGGERCORE_H__
#define __UNDEBUGGERCORE_H__

/*-----------------------------------------------------------------------------
	Forward Declarations.
-----------------------------------------------------------------------------*/

class	UDebuggerInterface;
class	FBreakpoint;
class		FInstanceBreakpoint;
class		FExecutionBreakpoint;
class	FBreakpointManager;
class	FStackNode;
class	FCallStack;
class	FDebuggerState;
class		DSWaitForInput;
class		DSWaitForCondition;
class			DSRunToCursor;
class			DSStepOut;
class			DSStepOverStack;
class			DSStepIntoStack;
class		DSIdleState;
class   FDebuggerWatch;
class   FDebuggerWatchNode;
class       FDebuggerArrayNode;

enum    EUserAction;
enum    EBreakType;


/**
 * Output device implementation routing serialized data to the debugger interface.
 */
class FDebuggerLog : public FOutputDevice
{
public:
	/**
	 * Routes message to the debugger if present.
	 *
	 * @param	Msg		Message to route
	 * @param	Event	Event type of message
	 */
	void Serialize(	const TCHAR* Msg, EName Event );
};


/*-----------------------------------------------------------------------------
	UDebuggerCore.
-----------------------------------------------------------------------------*/

class UDebuggerCore : public UDebugger
{
	TArray<FDebuggerWatch>		Watches;
	FString						InterfaceDllName;
	INT							ArrayMemberIndex;

	// Insert a given property into the watch window.
	void PropertyToWatch( UProperty* Prop, BYTE* BaseAddr, UBOOL bResetParentIndex, INT watch, const TCHAR* PropName = NULL );
	void PropertyToWatch(UProperty* Prop, BYTE* PropAddr, INT CurrentDepth, INT watch, INT watchParent, const TCHAR* PropName = NULL );
	void BuildParentChain( INT WatchType, TMap<UClass*,INT>& ParentChain, UClass* BaseClass, INT ParentID = INDEX_NONE );

	// Update the Watch ListView with all the current variables the Stack/Object contain.
	void RefreshWatch(const FStackNode* CNode);
	void RefreshUserWatches();

	TCHAR* GetShortName(UProperty* Prop)
	{
		if (Prop->IsA( UBoolProperty::StaticClass() ) )
			return TEXT("Bool");
		else if (Prop->IsA( UObjectProperty::StaticClass() ) )
			return TEXT("Object");
		else if (Prop->IsA( UStructProperty::StaticClass() ) )
			return TEXT("Struct");
		else if (Prop->IsA( UIntProperty::StaticClass() ) )
			return TEXT("Int");
		else if (Prop->IsA( UStrProperty::StaticClass() ) )
			return TEXT("String");
		else if (Prop->IsA( UFloatProperty::StaticClass() ) )
			return TEXT("Float");
		else if (Prop->IsA( UArrayProperty::StaticClass() ) )
			return TEXT("Array");
		else if (Prop->IsA( UByteProperty::StaticClass() ) )
			return TEXT("Byte");
		else if (Prop->IsA( UClassProperty::StaticClass() ) )
			return TEXT("Class");
		else if (Prop->IsA( UNameProperty::StaticClass() ) )
			return TEXT("Name");
		else if (Prop->IsA( UDelegateProperty::StaticClass() ) )
			return TEXT("Delegate");
		else return TEXT("Unknown");
	}

protected:
	FDebuggerState*				CurrentState;

	INT MaxObjectRecursion, CurrentObjectRecursion;
	INT MaxStructRecursion, CurrentStructRecursion;
	INT MaxClassRecursion, CurrentClassRecursion;
	INT MaxStaticArrayRecursion, CurrentStaticArrayRecursion;
	INT MaxDynamicArrayRecursion, CurrentDynamicArrayRecursion;

	// Break immediately
	void SetDebuggerLine( const FStackNode* CNode );

public:
	UDebuggerCore();
	~UDebuggerCore();

	// Main Debugger Entry point
	void DebugInfo( const UObject* Debugee, const FFrame* Stack, BYTE OpCode, INT LineNumber, INT InputPos );

	// notification that the UDebugger is now in a new stack
	void StackChanged( const FStackNode* CurrentNode );

	// Tells the debugger if it should be breaking on None
	void SetBreakOnNone(UBOOL inBreakOnNone);

	void SetDataBreakpoint( const TCHAR* WatchText );
	void SetCondition( const TCHAR* WatchText, const TCHAR* ConditionValue );

	// Notification handles from the engine
	virtual void NotifyAccessedNone();

	// Assertion failed
	UBOOL NotifyAssertionFailed( const INT LineNumber );

	// Infinite recursion
	UBOOL NotifyInfiniteLoop();

	// Called when garbage collection has begin - empty the call stack
	void NotifyGC();

	// Called at the beginning of each tick
	void NotifyBeginTick();

	// Utility
	void Close();

	void UpdateInterface();
	void LoadBreakpoints();
	void LoadEditPackages();

	// Inline / Accessors
	FDebuggerState*				GetCurrentState()		const { return CurrentState;		}
	FBreakpointManager*			GetBreakpointManager()	const { return BreakpointManager;	}
	FCallStack*					GetCallStack()			const { return CallStack;			}
	UDebuggerInterface*			GetInterface()			const { return Interface;			}


	// Helpers
	UClass* GetStackOwnerClass( const FFrame* Stack ) const;	// Derive the actual class associated with this stack.

	void Break();
	void ProcessInput( enum EUserAction UserAction );
	void ProcessPendingState();
	void ChangeState( FDebuggerState* NewState, UBOOL bImmediately = 0 );
	void AddWatch(const TCHAR* watchName);
	void RemoveWatch(const TCHAR* watchName);
	void ClearWatches();
	void DumpStack();
	const FStackNode* GetCurrentNode() const;

	UBOOL					ProcessDebugInfo;
	UBOOL					IsDebugging;
	UBOOL					IsClosing;	// True when the user has closed the debugger via the UI...
	UBOOL					AccessedNone;
	UBOOL					BreakOnNone;
	UBOOL					BreakASAP;

	FDebuggerLog*			DebuggerLog;
	FStringOutputDevice		ErrorDevice;
	FDebuggerState*			PendingState;
	FBreakpointManager*		BreakpointManager;
	FCallStack*				CallStack;
	UDebuggerInterface*		Interface;

	// Debugging
};

/*-----------------------------------------------------------------------------
	FDebuggerWatch.
-----------------------------------------------------------------------------*/

class FDebuggerWatch
{
protected:
	FDebuggerWatchNode* WatchNode;

	const UClass*    Class;
	const UFunction* Function;
	const UObject*   Object;

public:
	FString WatchText;

	virtual void Refresh( const UObject* CurrentObject, const FFrame* CurrentFrame );
	virtual UBOOL GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride );

	FDebuggerWatch( FStringOutputDevice& ErrorHandler, const TCHAR* WatchString = NULL );
	virtual ~FDebuggerWatch();
};

// Specialized type of watch for breaking on data value changes
class FDebuggerDataWatch : public FDebuggerWatch
{
	const UObject*		DataObject;
	const UProperty*	Property;
	const BYTE*			DataAddress;

	BYTE*				OriginalValue;

public:
	void Refresh( const UObject* CurrentObject, const FFrame* CurrentFrame );
	UBOOL GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride );
	virtual UBOOL Modified() const;

	FDebuggerDataWatch( FStringOutputDevice& ErrorHandler, const TCHAR* WatchString = NULL );
};


/*-----------------------------------------------------------------------------
	FDebuggerWatchNode.
-----------------------------------------------------------------------------*/

class FDebuggerWatchNode
{
	friend class FDebuggerWatch;

	// Properties
	const UStruct* ContextClass;
	const UClass* TopClass;
	const UObject* TopObject, *ContextObject;
	const UFunction* Function;

	const BYTE* GlobalData, *Base, *LocalData;

	const UProperty* Property;
	const BYTE*      PropAddr;

protected:
	FStringOutputDevice& Error;
	FDebuggerWatchNode* NextNode;
	FDebuggerArrayNode* ArrayNode;

	FString    PropertyName;
	INT        ArrayIndex;

// Methods

	// Property name parsing
	virtual UBOOL GetArrayDelimiter( FString& Test, FString& ArrayDelimiter ) const;

	virtual void AddArrayNode( const TCHAR* ArrayText );

	virtual void ResetBase( const UClass* CurrentClass, const UObject* CurrentObject, const UFunction* CurrentFunction, const BYTE* CurrentBase, const BYTE* CurrentLocals );
	virtual UBOOL Refresh( const UStruct* RelativeClass, const UObject* RelativeObject, const BYTE* Data );

public:
	virtual INT   GetArrayIndex() const;
	virtual UBOOL GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride );

	FDebuggerWatchNode( FStringOutputDevice& ErrorHandler, const TCHAR* NodeText );
	virtual ~FDebuggerWatchNode();
};

class FDebuggerArrayNode : public FDebuggerWatchNode
{
	INT Value;

public:
	void ResetBase( const UClass* CurrentClass, const UObject* CurrentObject, const UFunction* CurrentFunction, const BYTE* CurrentBase, const BYTE* CurrentLocals );
	INT GetArrayIndex();

	FDebuggerArrayNode(FStringOutputDevice& ErrorHandler, const TCHAR* ArrayText );
	~FDebuggerArrayNode();
};


/*-----------------------------------------------------------------------------
	FBreakpointManager.
-----------------------------------------------------------------------------*/


class FBreakpoint
{
public:
	FBreakpoint( const TCHAR* InClassName, INT InLine );
	UBOOL IsEnabled;
	FString ClassName;
	INT Line;
};

class FBreakpointManager
{
public:
	UBOOL QueryBreakpoint( const TCHAR* ClassName, INT Line  );
	void SetBreakpoint( const TCHAR* Class, INT Line );
	void RemoveBreakpoint( const TCHAR* Class, INT Line );

	TArray<FBreakpoint> Breakpoints;
};



/*-----------------------------------------------------------------------------
	FStackNode.
-----------------------------------------------------------------------------*/

class FStackNode
{
public:
	FStackNode( const UObject* Debugee, const FFrame* Stack, UClass* InClass, INT CurrentDepth, INT InLineNumber = INDEX_NONE, INT InPos = INDEX_NONE, BYTE Code = 255 );
	void Update( INT LineNumber, INT InputPos, BYTE Code, INT CurrentDepth )
	{
		Lines.AddItem(LineNumber);
		Positions.AddItem(InputPos);
		Depths.AddItem(CurrentDepth);
		OpCodes.AddItem(Code);
	}

	const FFrame* GetFrame() const    { return StackNode;        }
	const UObject* GetObject() const  { return Object;           }
	const UClass* GetClass() const    { return Class;            }
	const INT GetLine() const         { return Lines.Last();     }
	const INT GetPos() const          { return Positions.Last(); }
	const TCHAR* GetInfo() const;

	void Show() const;

	const UObject* Object;
	const FFrame* StackNode;
	const UClass* Class;

	TArray<INT> Lines, Positions, Depths;
	TArray<BYTE> OpCodes;
};



/*-----------------------------------------------------------------------------
	FCallStack.
-----------------------------------------------------------------------------*/

struct StackCommand
{
	const FFrame* Frame;
	INT     LineNumber;
	BYTE	OpCode;

	StackCommand( const FFrame* InFrame = NULL, BYTE InOpCode = 255, INT InLineNumber = 0 ) :
	Frame(InFrame), 
	OpCode(InOpCode), 
	LineNumber(InLineNumber)
	{ }

	StackCommand( const StackCommand& Other )
	{
		Frame = Other.Frame;
		LineNumber = Other.LineNumber;
		OpCode = Other.OpCode;
	}
};

class FCallStack
{
public:
	FCallStack( UDebuggerCore* InParent );
	~FCallStack();

	void Empty();
	UBOOL UpdateStack( const UObject* Debugee, const FFrame* FStack, int LineNumber, int InputPos, BYTE OpCode );

	// Inlines
	const INT FCallStack::GetStackDepth() const				{ return StackDepth;                                      }
	UBOOL IsValid( INT Test ) const							{ return (Test >= 0 && Test < Stack.Num());               }
	const FStackNode* FCallStack::GetTopNode() const		{ return Stack.Num() ?     &(Stack.Last()) : NULL;        }
	const FStackNode* FCallStack::GetPreviousNode() const	{ return Stack.Num() > 1 ? &(Stack(StackDepth-2)) : NULL; }
	      FStackNode* FCallStack::GetNode( INT i = 0 )		{ return IsValid(i) ? &(Stack(i)) : NULL;                 }
	const FStackNode* FCallStack::GetNode( INT i = 0 ) const{ return IsValid(i) ? &(Stack(i)) : NULL;                 }

	INT StackDepth;
	UDebuggerCore* Parent;
	TArray<FStackNode> Stack;
	TArray<StackCommand> QueuedCommands;
};



/*-----------------------------------------------------------------------------
	FDebuggerState.
-----------------------------------------------------------------------------*/
class FDebuggerState
{
	friend class DSBreakOnChange;
	friend void UDebuggerCore::Close();
	const FStackNode* CurrentNode;

public:
	FDebuggerState(UDebuggerCore* const inDebugger);
	virtual ~FDebuggerState();

	virtual const FStackNode* GetCurrentNode() const	{ return CurrentNode; }

	virtual void Process(UBOOL bOptional=0);				// Called each time execDebugInfo is called, if CallStack doesn't override
	virtual void HandleInput( EUserAction UserInput ) {};	// Allow the current state to process user input
	void UpdateStackInfo( const FStackNode* CNode );		// Update the state with the current stack node

	// Return true to prevent a debugger state change
	// Used in DSBreakOnChange to allow other states to be processed while we're watching a variable address
	virtual UBOOL InterceptNewState( FDebuggerState* NewState )	{ return 0; } 
	virtual UBOOL InterceptOldState( FDebuggerState* OldState ) { return 0; }
	UDebuggerCore* Debugger;

protected:
	INT LineNumber, EvalDepth;

	virtual FDebuggerState* GetCurrent()					{ return this; }
	virtual void SetCurrentNode( const FStackNode* Node )	{ CurrentNode = Node; }
	virtual UBOOL EvaluateCondition(UBOOL bOptional=0);		// return true to break the debugger
	virtual void PumpMessages() {}


	friend FDebuggerState* UDebuggerCore::GetCurrentState() const;
};


class DSIdleState : public FDebuggerState
{
	// TODO DSIdleState also needs a line number variable so that it doesn't get stuck when you click 'continue' if there are
	// more opcodes in the current line
public:
	DSIdleState(UDebuggerCore* const inDebugger);
};


class DSWaitForInput : public FDebuggerState
{
public:

	DSWaitForInput(UDebuggerCore* const inDebugger);

	virtual void Process(UBOOL bOptional=0);
	virtual void HandleInput( EUserAction UserInput );
	virtual void ContinueExecution();

protected:
	void PumpMessages();
	UBOOL bContinue;
};


class DSWaitForCondition : public FDebuggerState
{
public:
	DSWaitForCondition(UDebuggerCore* const inDebugger);
	void Process(UBOOL bOptional=0);

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
};

class DSRunToCursor : public DSWaitForCondition
{
public:

	DSRunToCursor( UDebuggerCore* const inDebugger );
	INT ExpectedPos;

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
};

class DSBreakOnChange : public DSWaitForCondition
{
public:
	DSBreakOnChange( UDebuggerCore* const inDebugger, const TCHAR* WatchText, FDebuggerState* NewState=NULL);
	~DSBreakOnChange();

	const FStackNode* GetCurrentNode() const;

	void Process(UBOOL bOptional);
	void HandleInput( EUserAction Action );
	UBOOL InterceptNewState( FDebuggerState* NewState );
	UBOOL InterceptOldState( FDebuggerState* OldState );

protected:
	FDebuggerState* GetCurrent();
	void  SetCurrentNode( const FStackNode* Node );
	UBOOL EvaluateCondition(UBOOL bOptional=0);

	FDebuggerDataWatch* Watch;
	FDebuggerState* SubState;

	UBOOL bDataBreak;
};

class DSStepOut : public DSWaitForCondition
{
public:
	DSStepOut( UDebuggerCore* const inDebugger);

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
	const FFrame* OldStack;
};


class DSStepInto : public DSWaitForCondition
{
public:
	DSStepInto( UDebuggerCore* const inDebugger );

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
	const FFrame*  OldStack;
};


class DSStepOverStack : public DSWaitForCondition
{
public:
	DSStepOverStack( const UObject* inObject, UDebuggerCore* const inDebugger );

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
	const UObject* EvalObject;
};

#include "UnDebuggerInterface.h"

#endif