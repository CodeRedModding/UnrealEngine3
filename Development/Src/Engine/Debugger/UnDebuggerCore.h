/*=============================================================================
	UnDebuggerCore.h: Debugger Core Logic
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
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
	/** the list of user watch value expressions */
	TArray<FDebuggerWatch>	Watches;

	/** When evaluting watch values for arrays, tracks the index of the element currently being evaluated */
	INT						ArrayMemberIndex;

	/** the location in the debugger's callstack current being used for evaluating watch variables */
	INT						CurrentStackPosition;

	/** Whether the UDebugger is currently attached to the game */
	UBOOL					bAttached;

public:
	/** Whether the UDebugger is allowed to process debug opcodes */
	UBOOL					bProcessDebugInfo;

	/** TRUE when the UDebugger is waiting for user input */
	UBOOL					bIsDebugging;

	/** TRUE when the user has closed the debuger via the UI */
	UBOOL					bIsClosing;

	/** Indicates that the engine detected an "accessed none" error; relevant only if bBreakOnNone is enabled */
	UBOOL					bAccessedNone;

	/** Controls whether the UDebugger will break when an "accessed none" error occurs */
	UBOOL					bBreakOnNone;

	/** Indicates that the UDebugger should break immediately */
	UBOOL					bBreakASAP;

	/** The UDebugger's log output device */
	FDebuggerLog*			DebuggerLog;

	/** Used for generating variable watch expression evaluation error messages */
	FStringOutputDevice		ErrorDevice;

	/** The state that the UDebugger will switch to the next time it receives a debug opcode */
	FDebuggerState*			PendingState;

	/** Manages all breakpoints */
	FBreakpointManager*		BreakpointManager;

	/** The list of stack nodes currently being debugged */
	FCallStack*				CallStack;

	/**
	 * Displays the Udebugger output; processes input from the user and routes that input to the UDebugger engine.
	 */
	UDebuggerInterface*		Interface;

private:
	// Insert a given property into the watch window.
	void PropertyToWatch( UProperty* Prop, BYTE* BaseAddr, UBOOL bResetParentIndex, INT watch, const TCHAR* PropName = NULL );
	void PropertyToWatch(UProperty* Prop, BYTE* PropAddr, INT CurrentDepth, INT watch, INT watchParent, const TCHAR* PropName = NULL );
	void BuildParentChain( INT WatchType, TMap<UClass*,INT>& ParentChain, UClass* BaseClass, INT ParentID = INDEX_NONE );

	// Update the Watch ListView with all the current variables the Stack/Object contain.
	void RefreshWatch(const FStackNode* CNode);
	void RefreshUserWatches();

	const TCHAR* GetShortName(UProperty* Prop)
	{
		if ( Cast<UBoolProperty>(Prop) )
		{
			return TEXT("Bool");
		}
		else if (  Cast<UStructProperty>(Prop) )
		{
			return TEXT("Struct");
		}
		else if (  Cast<UIntProperty>(Prop) )
		{
			return TEXT("Int");
		}
		else if (  Cast<UStrProperty>(Prop) )
		{
			return TEXT("String");
		}
		else if (  Cast<UFloatProperty>(Prop) )
		{
			return TEXT("Float");
		}
		else if (  Cast<UArrayProperty>(Prop) )
		{
			return TEXT("Array");
		}
		else if (  Cast<UByteProperty>(Prop) )
		{
			return Cast<UByteProperty>(Prop)->Enum != NULL ? TEXT("Enum") : TEXT("Byte");
		}
		else if (  Cast<UClassProperty>(Prop) )
		{
			return TEXT("Class");
		}
		else if (  Cast<UNameProperty>(Prop) )
		{
			return TEXT("Name");
		}
		else if (  Cast<UDelegateProperty>(Prop) )
		{
			return TEXT("Delegate");
		}
		else if (  Cast<UInterfaceProperty>(Prop) )
		{
			return TEXT("Interface");
		}
		else if (  Cast<UComponentProperty>(Prop) )
		{
			return TEXT("Component");
		}
		else if (  Cast<UObjectProperty>(Prop) )
		{
			return TEXT("Object");
		}

		return TEXT("Unknown");
	}

protected:

	/**
	 * The state the UDebugger is currently in.  States determine how the UDebugger behaves; the most important states are:
	 *	DSIdleState: UDebugger is attached and the game is running normally
	 *	DSWaitForInput: Game is paused and UDebugger is currently in interactive mode, waiting for user input (i.e. step into, out of)
	 *	DSStep*: Game is running normally but UDebugger will automatically pause when a certain class/line is reached.
	 */
	FDebuggerState*				CurrentState;

	INT MaxObjectRecursion, CurrentObjectRecursion;
	INT MaxStructRecursion, CurrentStructRecursion;
	INT MaxClassRecursion, CurrentClassRecursion;
	INT MaxStaticArrayRecursion, CurrentStaticArrayRecursion;
	INT MaxDynamicArrayRecursion, CurrentDynamicArrayRecursion;

	// Break immediately
	void SetDebuggerLine( const FStackNode* CNode );

private:
	// private to ensure InitializeDebugger is used
	UDebuggerCore();

public:
	~UDebuggerCore();

	/**
 	 * Early initialization of the script debugger
	 */
	static void InitializeDebugger();


	// Main Debugger Entry point
	void DebugInfo( const UObject* Debugee, const FFrame* Stack, BYTE OpCode, INT LineNumber, INT InputPos );

	// notification that the UDebugger is now in a new stack
	void StackChanged( const FStackNode* CurrentNode );

	void ChangeStack( INT StackNodeIndex );

	void SetDataBreakpoint( const TCHAR* WatchText );
	void SetCondition( const TCHAR* WatchText, const TCHAR* ConditionValue );

	FORCEINLINE UBOOL IsGameExiting() const
	{
		// check GIsUCC because GIsRequestingExit is always TRUE when running a commandlet
		return GIsRequestingExit && !GIsUCC;
	}

	/** Accessor for marking the debugger as shutting down */
	FORCEINLINE void SetDebuggerClosing( UBOOL bShouldClose )
	{
		if ( DebuggerLog != NULL )
		{
			DebuggerLog->Logf(TEXT("%s UnrealScript Debugger (currently %s)"), bShouldClose ? TEXT("Detaching") : TEXT("Attaching"), IsDebuggerAttached() ? TEXT("attached") : TEXT("detached"));
		}
		bIsClosing = bShouldClose;
		if ( bIsClosing && IsDebuggerAttached() )
		{
			AttachScriptDebugger(FALSE);
		}
	}
	FORCEINLINE UBOOL IsDebuggerClosing() const
	{
		return bIsClosing;
	}


	/** Accessor for enabling the processing of debug opcodes */
	FORCEINLINE void EnableDebuggerProcessing( UBOOL bShouldProcessDebugInfo )
	{
		bProcessDebugInfo = bShouldProcessDebugInfo;
	}
	FORCEINLINE UBOOL IsDebuggerProcessingEnabled() const
	{
		return bProcessDebugInfo;
	}


	/** Accessor for changing flag indicating whether the debugger should break on access nones */
	FORCEINLINE void SetBreakOnNone(UBOOL inBreakOnNone)
	{
		bBreakOnNone = inBreakOnNone;
		SetAccessedNone(FALSE);
	}
	FORCEINLINE UBOOL IsBreakOnNoneEnabled() const
	{
		return bBreakOnNone;
	}


	/* Accessor for forcing the debugger to break after it processes the next opcode it receives */
	FORCEINLINE void SetBreakASAP( UBOOL bInBreakASAP )
	{
		bBreakASAP = bInBreakASAP;
		if ( bBreakASAP && !IsDebuggerClosing() )
		{
			AttachScriptDebugger(TRUE);
		}
	}
	FORCEINLINE UBOOL IsPendingImmediateBreak() const
	{
		return bBreakASAP;
	}

	/** Accessor for marking the debugger as attached to the engine */
	void AttachScriptDebugger( UBOOL bShouldAttach );
	FORCEINLINE UBOOL IsDebuggerAttached() const
	{
		return bAttached;
	}

	/** Accessor for marking the debugger as active (i.e. game thread is suspended, debugger waiting for input */
	FORCEINLINE void ActivateDebugger( UBOOL bShouldBeActive )
	{
		bIsDebugging = bShouldBeActive && IsDebuggerAttached() && IsDebuggerProcessingEnabled();
	}
	FORCEINLINE UBOOL IsDebuggerActive() const
	{
		return bIsDebugging;
	}


	/** Accessor for indicating that the engine has encountered an access none error */
	FORCEINLINE void SetAccessedNone( UBOOL bHasAccessedNone )
	{
		bAccessedNone = bHasAccessedNone;
	}
	FORCEINLINE UBOOL HasReceivedAccessNone() const
	{
		return bAccessedNone;
	}


	// Notification handles from the engine
	virtual void NotifyAccessedNone()
	{
		SetAccessedNone(TRUE);
	}

	// Assertion failed
	virtual UBOOL NotifyAssertionFailed( const INT LineNumber );

	// Infinite recursion
	virtual UBOOL NotifyInfiniteLoop();

	// Called when garbage collection has begin - empty the call stack
	virtual void NotifyGC();

	// Called at the beginning of each tick
	virtual void NotifyBeginTick();

	/**
	 * Detach the UDebugger from the engine.
	 *
	 * @param	bUnbindInterfaceImmediately		specify TRUE to cleanup and unbind the interface .dll as well.  If calling this method
	 *											from the interface .dll's code, should NEVER specify TRUE.
	 */
	void Close( UBOOL bUnbindInterfaceImmediately=FALSE );

	void UpdateInterface();
	void LoadBreakpoints();

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

	/**
	 * Returns a string giving the current state of the UDebugger; used for debugging purposes.
	 */
	FString Describe() const;

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

/**
 * Specialized type of watch for breaking on data value changes
 * @todo - not yet implemented
 */
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
	/** cache of current debugger context */
	const UStruct* ContextClass;
	const UClass* TopClass;
	const UObject* TopObject, *ContextObject;
	const UFunction* Function;
	const FFrame* StackFrame;
	/** cache of memory locations relevant to current debugger context */
	const BYTE* GlobalData, *Base, *LocalData;
	/** property and address for this watch determined from above */
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

	virtual void ResetBase( const UClass* CurrentClass, const UObject* CurrentObject, const UFunction* CurrentFunction, const BYTE* CurrentBase, const FFrame* CurrentFrame );
	virtual UBOOL Refresh( const UStruct* RelativeClass, const UObject* RelativeObject, const BYTE* Data );

public:
	virtual INT   GetArrayIndex() const;
	virtual UBOOL GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride ) const;

	FDebuggerWatchNode( FStringOutputDevice& ErrorHandler, const TCHAR* NodeText );
	virtual ~FDebuggerWatchNode();
};

class FDebuggerArrayNode : public FDebuggerWatchNode
{
	mutable INT Value;

public:
	void ResetBase( const UClass* CurrentClass, const UObject* CurrentObject, const UFunction* CurrentFunction, const BYTE* CurrentBase, const FFrame* CurrentFrame );
	virtual INT GetArrayIndex() const;

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
	void Update( INT LineNumber, INT InputPos, BYTE OpCode, INT CurrentDepth );
	void SetBreakpoint(INT LineNumber) { BreakpointMutex = LineNumber; }

	const FFrame* GetFrame() const    { return StackNode;        }
	const UObject* GetObject() const  { return Object;           }
	const UClass* GetClass() const    { return Class;            }
	const INT GetLine() const         { return Lines.Last();     }
	const INT GetPos() const          { return Positions.Last(); }
	const UBOOL HasBreakpoint() const { return BreakpointMutex != 0;  }
	const TCHAR* GetInfo() const;

	void Show() const;

	const UObject* Object;
	const FFrame* StackNode;
	const UClass* Class;

	TArray<INT> Lines, Positions, Depths;
	TArray<BYTE> OpCodes;

	UBOOL operator==( const FStackNode& Other ) const
	{
		return StackNode == Other.StackNode;
	}

private:
	/** used to avoid breaking on the same line twice, since we might get multiple debuginfos per line depending on complexity and code structure
	 * value is last breakpoint line while on a breakpoint, or zero if we can break again
	 */
	INT BreakpointMutex;
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
	void StackCorruptionDetected();
	
	/** @return whether the two specified script stack frames represent the same script location
	 * this is not as simple as comparing pointers due to state code duplicating the state frame
	 * to preserve integrity when states are changed
	 */
	UBOOL AreStackFramesIdentical(const FFrame* A, const FFrame* B) const
	{
		// note that for states we cannot compare FFrame pointers as the state frame gets duplicated during state execution
		// since there can only be one state frame on the stack at a time, checking object pointers will do the job instead
		return (A == B || (A->Object == B->Object && A->Node->IsA(UState::StaticClass())));
	}

public:
	FCallStack( UDebuggerCore* InParent );
	~FCallStack();

	void Empty();
	UBOOL UpdateStack( const UObject* Debugee, const FFrame* FStack, int LineNumber, int InputPos, BYTE OpCode );


	// Inlines
	const INT FCallStack::GetStackDepth() const				{ return StackDepth;                                      }
	UBOOL IsValid( INT Test ) const							{ return Stack.IsValidIndex(Test);                        }
	const FStackNode* FCallStack::GetTopNode() const		{ return Stack.Num() ?     &(Stack.Top()) : NULL;         }
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
	friend void UDebuggerCore::Close( UBOOL bUnbindInterfaceImmediately );
	friend FDebuggerState* UDebuggerCore::GetCurrentState() const;

	/**
	 * The currently active stack node
	 */
	const FStackNode* CurrentNode;

protected:
	INT LineNumber;
	INT EvalDepth;


public:
	/** reference to the UDebugger */
	UDebuggerCore* Debugger;


	FDebuggerState(UDebuggerCore* const inDebugger);
	virtual ~FDebuggerState();


	virtual void Process(UBOOL bOptional=FALSE);			// Called each time execDebugInfo is called, if CallStack doesn't override
	virtual void HandleInput( EUserAction UserInput ) {};	// Allow the current state to process user input
	void UpdateStackInfo( const FStackNode* CNode );		// Update the state with the current stack node

	// Return true to prevent a debugger state change
	// Used in DSBreakOnChange to allow other states to be processed while we're watching a variable address
	virtual UBOOL InterceptNewState( FDebuggerState* NewState )	{ return 0; } 
	virtual UBOOL InterceptOldState( FDebuggerState* OldState ) { return 0; }

	/**
	 * Returns this state's current node.
	 */
	virtual const FStackNode* GetCurrentNode() const
	{
		return CurrentNode;
	}

	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("FDebuggerState");
	}

protected:
	/**
	 * Returns a pointer to the currently active child state (or this if this state doesn't support child states).
	 */
	virtual FDebuggerState* GetCurrent()					
	{
		return this;
	}

	/**
	 * Changes this state's current stack node.
	 */
	virtual void SetCurrentNode( const FStackNode* Node )
	{
		CurrentNode = Node;
	}

	/**
	 * Determines whether the UDebugger should break based on current state and location of the script VM.
	 */
	virtual UBOOL EvaluateCondition(UBOOL bOptional=FALSE);

	/**
	 * Processes windows messages.
	 */
	virtual void PumpMessages() {}
};


class DSIdleState : public FDebuggerState
{
	// TODO DSIdleState also needs a line number variable so that it doesn't get stuck when you click 'continue' if there are
	// more opcodes in the current line
public:
	DSIdleState(UDebuggerCore* const inDebugger);


	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("DSIdleState");
	}
};


class DSWaitForInput : public FDebuggerState
{
public:

	DSWaitForInput(UDebuggerCore* const inDebugger);

	virtual void Process(UBOOL bOptional=0);
	virtual void HandleInput( EUserAction UserInput );
	void ContinueExecution( UBOOL bShouldContinue=TRUE );

	
	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return FString::Printf(TEXT("DSWaitForInput (bContinue=%s)"), bContinue ? GTrue : GFalse);
	}

protected:
	/**
	 * Processes windows messages.
	 */
	virtual void PumpMessages();
	UBOOL bContinue;
};


class DSWaitForCondition : public FDebuggerState
{
public:
	DSWaitForCondition(UDebuggerCore* const inDebugger);
	void Process(UBOOL bOptional=0);

	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("DSWaitForCondition");
	}

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
};

/**
 * Not currently used.
 */
class DSRunToCursor : public DSWaitForCondition
{
public:

	DSRunToCursor( UDebuggerCore* const inDebugger );
	INT ExpectedPos;

	
	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return FString::Printf(TEXT("DSRunToCursor (ExpectedPos=%i)"), ExpectedPos);
	}

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
};

/**
 * Debugger state which breaks execution when a variable value is changed by the script VM.
 *
 * @todo not currently implemented.
 */
class DSBreakOnChange : public DSWaitForCondition
{
public:
	DSBreakOnChange( UDebuggerCore* const inDebugger, const TCHAR* WatchText, FDebuggerState* NewState=NULL);
	~DSBreakOnChange();

	const FStackNode* GetCurrentNode() const;

	virtual void Process(UBOOL bOptional);
	virtual void HandleInput( EUserAction Action );
	virtual UBOOL InterceptNewState( FDebuggerState* NewState );
	virtual UBOOL InterceptOldState( FDebuggerState* OldState );

	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("DSBreakOnChange");
	}

protected:
	virtual FDebuggerState* GetCurrent();
	virtual void  SetCurrentNode( const FStackNode* Node );
	virtual UBOOL EvaluateCondition(UBOOL bOptional=0);

	FDebuggerDataWatch* Watch;
	FDebuggerState* SubState;

	UBOOL bDataBreak;
};

class DSStepOut : public DSWaitForCondition
{
public:
	DSStepOut( UDebuggerCore* const inDebugger);

	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("DSStepOut");
	}

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
	const FFrame* OldStack;
};


class DSStepInto : public DSWaitForCondition
{
public:
	DSStepInto( UDebuggerCore* const inDebugger );

	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("DSStepInto");
	}

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
	const FFrame*  OldStack;
};


class DSStepOverStack : public DSWaitForCondition
{
public:
	DSStepOverStack( const UObject* inObject, UDebuggerCore* const inDebugger );

	/**
	 * Returns the name of this state; for debugging purposes
	 */
	virtual FString Describe() const
	{
		return TEXT("DSStepOverStack");
	}

protected:
	UBOOL EvaluateCondition(UBOOL bOptional=0);
	const UObject* EvalObject;
};

#include "UnDebuggerInterface.h"

#endif // __UNDEBUGGERCORE_H__
