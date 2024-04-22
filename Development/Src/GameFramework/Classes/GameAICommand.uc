/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameAICommand extends AICommandBase within GameAIController
	abstract
    native;

/** Current child node executing on top of this command */
var() const transient editinline GameAICommand ChildCommand;

/** Exiting status of the last child command to execute */
var() const transient Name ChildStatus;

/** Extra reference to the AI this command is being used by */
var() transient GameAIController GameAIOwner;

/** Exiting status of this command */
var() transient Name Status;

/** if this is FALSE and we're trying to push a new instance of a given class, but the top of the stack is already an instance of that class ignore the attempted push */
var bool bAllowNewSameClassInstance;

/** if this is TRUE, when we try to push a new instance of a command who has the same class as the command on the top of the stack, pop the one on the stack, and push the new one
	 NOTE: This trumps bAllowNewClassInstance (e.g. if this is true and bAllowNewClassInstance is false the active instance will still be replaced) */
var bool bReplaceActiveSameClassInstance;

/** Command has been aborted and should be popped next frame */
var transient private bool bAborted;

var bool bIgnoreNotifies;
var bool bIgnoreStepAside;

/** this command is about to be popped, and shouldn't have resumed called on it when children are popped */
var transient private bool bPendingPop;


cpptext
{
    virtual void TickCommand(FLOAT DeltaTime);
    void ProcessState(FLOAT DeltaSeconds);
    virtual EGotoState GotoState( FName State, UBOOL bForceEvents = 0, UBOOL bKeepStack = 0 );
    void PopChildCommand();
}


/** Simple constructor that takes one extra userdata param **/
static function bool InitCommandUserActor( GameAIController AI, Actor UserActor )
{
	//default is to call the constructor
	return InitCommand(AI);
}
/** Simple constructor that pushes a new instance of the command for the AI */
static function bool InitCommand( GameAIController AI )
{
	local GameAICommand Cmd;

	if( AI != None )
	{
		Cmd = new(AI) Default.Class;
		if( Cmd != None )
		{
			AI.PushCommand(Cmd);
			return TRUE;
		}
	}

	return FALSE;
}

/** == INTERNAL INTERFACE == */
/** called to set up internal pointers to the owning AI, before Pushed is called */
final event InternalPrePushed(GameAIController AI)
{
	GameAIOwner = AI;
	PrePushed(AI);
}
/** Called when command pushed to perform any necessary work, independent of the individual command implementations - @see Pushed() instead */
final event InternalPushed()
{
	GotoState('Auto');
	// call the overrideable notification
	Pushed();
}

/** Called when command popped to perform any necessary cleanup, independent of the individual command implementations - @see Popped() instead */
event InternalPopped()
{
	// call the overrideable notifications
	Popped();
	GameAIOwner=none;
	PostPopped();
}

/** Called when another command is pushed on top of this one */
final event InternalPaused( GameAICommand NewCommand )
{
	Paused( NewCommand );
}

/** Called when the command that was on top of this one in the stack is popped */
final event InternalResumed( Name OldCommandName )
{
	Resumed( OldCommandName );
}

final event InternalTick( float DeltaTime )
{
	Tick( DeltaTime );
}


final native function bool ShouldIgnoreNotifies() const;


/** == OVERRIDABLE INTERFACE == */
function Tick( float DeltaTime )
{
}

function bool AllowTransitionTo( class<GameAICommand> AttemptCommand )
{
	return (ChildCommand == None || ChildCommand.AllowTransitionTo( AttemptCommand ));
}

function bool AllowStateTransitionTo(Name StateName)
{
	return (ChildCommand == None || ChildCommand.AllowStateTransitionTo(StateName));
}

/** called to set up internal pointers to the owning AI, before Pushed is called */
function PrePushed(GameAIController AI);
/** called just before popped.. useful for cleaning up things before the popped chain gets called */
function PostPopped();
/** Notification called when this command has pushed */
function Pushed()
{
	//debug
	`AILog( "COMMAND PUSHED:"@self );
}

/** Notification when this command has popped */
function Popped()
{
	//debug
	`AILog( "COMMAND POPPED:"@self@"with"@Status );
}

function Paused(GameAICommand NewCommand)
{
	//debug
	`AILog( "COMMAND PAUSED:"@self@"by"@NewCommand );
}

function Resumed( Name OldCommandName )
{
	//debug
	`AILog( "COMMAND RESUMED:"@self@"from"@OldCommandName@"with"@ChildStatus );
}

event String GetDumpString()
{
	return String(self);
}

/**
 * ===========
 * DEBUG STATES
 * ===========
 */
state DEBUGSTATE
{
	function BeginState( Name PreviousStateName )
	{
		//debug
		`AILog( "BEGINSTATE"@PreviousStateName, 'State' );
	}

	function EndState( Name NextStateName )
	{
		//debug
		`AILog( "ENDSTATE"@NextStateName, 'State' );
	}

	function PushedState()
	{
		//debug
		`AILog( "PUSHED", 'State' );
	}

	function PoppedState()
	{
		//debug
		`AILog( "POPPED", 'State' );
	}

	function ContinuedState()
	{
		//debug
		`AILog( "CONTINUED", 'State' );
	}

	function PausedState()
	{
		//debug
		`AILog( "PAUSED", 'State' );
	}
}

/**
 *	Command has failed but delay pop to avoid infinite recursion
 */
state DelayFailure `DEBUGSTATE
{
Begin:
	Sleep( 0.5f );

	Status = 'Failure';
	PopCommand( self );
}

state DelaySuccess `DEBUGSTATE
{
Begin:
	Sleep( 0.1f );
	Status = 'Success';
	PopCommand( self );
}

event DrawDebug( HUD H, Name Category );

/** Used to get text from the AICmds **/
function GetDebugOverheadText( PlayerController PC, out array<string> OutText );

event String GetDebugVerboseText();

function NotifyNeedRepath();

function bool MoveUnreachable( Vector AttemptedDest, Actor AttemptedTarget );

function bool HandlePathObstruction( Actor BlockedBy )
{
	if ( ChildCommand!= none )
	{
		return ChildCommand.HandlePathObstruction(BlockedBy);
	}

	return false;
}



defaultproperties
{
	bAllowNewSameClassInstance=FALSE
	bReplaceActiveSameClassInstance=FALSE
}
