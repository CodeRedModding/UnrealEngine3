/*=============================================================================
UnWTInterface.cpp: Debugger Interface Interface
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if _WINDOWS && !CONSOLE

#include "UnDebuggerCore.h"
#include "UnDebuggerInterface.h"
#include "UnWTInterface.h"

#include "PreWindowsApi.h"
#include <ShellApi.h>
#include "PostWindowsApi.h"

#define UCSIDE
#include "WTDebuggerInterface.h"

WTGlobals g_WTGlobals(L"WTUnrealDebuggerUCSide");
OnCommandToVSProto IPCSendCommandToVS = NULL;
NotifyBeginTickProto IPCNotifyBeginTick = NULL;
nFringeSupportProto IPCnFringeSupport = NULL;
NotifyDebugInfoProto IPCNotifyDebugInfo = NULL;

DWORD OnCommandToUC(INT cmdId, LPCWSTR cmdStr)
{
	if(GDebugger)
	{
		((WTInterface*)((UDebuggerCore*)GDebugger)->GetInterface())->Callback(cmdId, cmdStr);
	}
	return 0;
}

WTInterface::WTInterface( const TCHAR* InDLLName ) 
:	hInterface(NULL),
	m_pIPC(NULL)
{
	DllName = InDLLName;
	Debugger = NULL;
	m_locked = FALSE;
}

WTInterface::~WTInterface()
{
	Close();
	Debugger = NULL;
}

int WTInterface::AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data)
{
	if ( IsLoaded() )
	{
		return m_pIPC->SendCommandToVS(CMD_AddWatch, watch, ParentIndex, ObjectName, Data);
	}

	return -1;
// 	return VAAddAWatch(watch, ParentIndex, ObjectName, Data);
}


void WTInterface::ClearAWatch(int watch)
{
	if ( IsLoaded() )
	{
		m_pIPC->SendCommandToVS(CMD_ClearWatch, watch);
	}
// 	VAClearAWatch(watch);
}

UBOOL WTInterface::Initialize( UDebuggerCore* DebuggerOwner )
{
	Debugger = DebuggerOwner;
	if ( !IsLoaded() )
	{
		BindToDll();
		ClearAWatch( LOCAL_WATCH );
		ClearAWatch( GLOBAL_WATCH );
		ClearAWatch( WATCH_WATCH );
	}

	Show();
	return TRUE;
}

void WTInterface::NotifyBeginTick()
{
	if ( IsLoaded() && IPCNotifyBeginTick )
	{
		IPCNotifyBeginTick();
	}
}

UBOOL WTInterface::NotifyDebugInfo( UBOOL* bAllowDetach )
{
	if ( IsLoaded() && IPCNotifyDebugInfo )
	{
		return IPCNotifyDebugInfo( bAllowDetach );
	}


	// this interface doesn't detach after each operation
	if ( bAllowDetach )
	{
		*bAllowDetach = FALSE;
	}

	// not yet ready (since it's not loaded)
	return FALSE;
}

void WTInterface::Callback( INT cmdID,  LPCWSTR cmdStr )
{
	// uncomment to log all callback mesages from the UI
	const TCHAR* command = cmdStr; // ANSI_TO_TCHAR(C);
// 	Debugger->DebuggerLog->Logf(TEXT("Callback: %s"), command);
// 	Debugger->DebuggerLog->Logf(TEXT("> > %s"), *Debugger->Describe());

	if(ParseCommand(&command, TEXT("addbreakpoint")))
	{		
		TCHAR className[256];
		ParseToken(command, className, 256, 0);
		TCHAR lineNumString[10];
		ParseToken(command, lineNumString, 10, 0);
		SetBreakpoint(className, appAtoi(lineNumString));

	}
	else if(ParseCommand(&command, TEXT("removebreakpoint")))
	{
		TCHAR className[256];
		ParseToken(command, className, 256, 0);
		TCHAR lineNumString[10];
		ParseToken(command, lineNumString, 10, 0);
		RemoveBreakpoint(className, appAtoi(lineNumString));
	}
	else if(ParseCommand(&command, TEXT("addwatch")))
	{
		TCHAR watchName[256];
		ParseToken(command, watchName, 256, 0);
		Debugger->AddWatch(watchName);
	}
	else if(ParseCommand(&command, TEXT("removewatch")))
	{
		TCHAR watchName[256];
		ParseToken(command, watchName, 256, 0);
		Debugger->RemoveWatch(watchName);
	}
	else if(ParseCommand(&command, TEXT("clearwatch")))
	{
		Debugger->ClearWatches();
	}
	else if ( ParseCommand(&command,TEXT("setcondition")) )
	{
		FString ConditionName, Value;
		if ( !ParseToken(command,ConditionName,1) )
		{
			debugf(TEXT("Callback error (setcondition): Couldn't parse condition name"));
			return;
		}
		if ( !ParseToken(command,Value,1) )
		{
			debugf(TEXT("Callback error (setcondition): Failed parsing condition value"));
			return;
		}

		Debugger->SetCondition(*ConditionName,*Value);
		return;
	}
	else if ( ParseCommand(&command,TEXT("getstack")) )
	{
		UpdateCallStack();
		// I am still able to get IsDebugging == 0 when it should not be,
		// I think this is caused by this asynchronous callback calling "step" before
		// the engine has finished updating the stack.
		// Is there some sort of locking we should checking for?
		// This is a hack to just reset it, when the user presses break.
		if( !Debugger->IsDebuggerActive() )
		{
			Debugger->ActivateDebugger(TRUE);
		}
		return;
	}
	else if ( ParseCommand(&command,TEXT("changestack")) )
	{
		FString StackNum;
		if ( !ParseToken(command,StackNum,1) )
		{
			debugf(TEXT("Callback error (changestack): Couldn't parse stacknum"));
			return;
		}

		Debugger->ChangeStack( appAtoi(*StackNum) );
		return;
	}
	else if (ParseCommand(&command,TEXT("setdatawatch")))
	{
		FString WatchText;
		if ( !ParseToken(command,WatchText,1) )
		{
			debugf(TEXT("Callback error (setdatawatch): Failed parsing watch text"));
			return;
		}

		Debugger->SetDataBreakpoint(*WatchText);
		return;
	}
	else if ( ParseCommand(&command,TEXT("setdatavalue")) )
	{
		FString Prop;
		FString PropAddr;

		if ( !ParseToken( command, Prop, TRUE ) )
		{
			debugf( TEXT("Callback error (setdatavalue): Failed parsing UProperty address.") );
			return;
		}

		if ( !ParseToken( command, PropAddr, TRUE ) )
		{
			debugf( TEXT("Callback error (setdatavalue): Failed parsing target address.") );
			return;
		}

		// skip the whitespace
		command++;

		// import the text property value into the property (allows setting property values in the debugger)
		UProperty* Property = (UProperty*)appAtoi64( *Prop );
		const TCHAR* Buffer = command;
		BYTE* Data = (BYTE*)appAtoi64( *PropAddr );
		INT PortFlags = PPF_Delimited;
		UObject* Parent = NULL;
		FOutputDevice* ErrorText = NULL;
		const TCHAR* Result = Property->ImportText( Buffer, Data, PortFlags, Parent, ErrorText );
		Debugger->UpdateInterface();
	}
	else if(ParseCommand(&command, TEXT("breakonnone")))
	{
		TCHAR breakValueString[5];
		ParseToken(command, breakValueString, 5, 0);
		Debugger->SetBreakOnNone(appAtoi(breakValueString));
	}
	else if(ParseCommand(&command, TEXT("break")))
	{
		Debugger->SetBreakASAP(TRUE);
	}
	else if ( ParseCommand(&command, TEXT("stopdebugging")) )
	{
		Close();
		Debugger->Close();
		return;
	}
	else if ( ParseCommand(&command,TEXT("terminate")) )
	{
		GIsRequestingExit = TRUE;

		Close();
		Debugger->Close();
		return;
	}
	else if ( ParseCommand(&command,TEXT("clearbreaks")) )
	{
		// dunno what this is supposed to be for...
		return;
	}
	else if ( Debugger->IsDebuggerActive() )
	{
		EUserAction Action = UA_None;
		if(ParseCommand(&command, TEXT("go")))
		{
			Debugger->SetBreakASAP(FALSE);
			Action = UA_Go;
		}
		else if ( ParseCommand(&command,TEXT("stepinto")) )
		{
			Action = UA_StepInto;
		}
		else if ( ParseCommand(&command,TEXT("stepover")) )
		{
			Action = UA_StepOverStack;
		}
		else if(ParseCommand(&command, TEXT("stepoutof")))
		{
			Action = UA_StepOut;
		}
		Debugger->ProcessInput(Action);
	}

// 	Debugger->DebuggerLog->Logf(TEXT("<<< %s"), *Debugger->Describe());
}

UBOOL WTInterface::IsLoaded() const
{
	return m_pIPC && m_pIPC->IsLoaded();
}

void WTInterface::AddToLog( const TCHAR* Line )
{
	if ( IsLoaded() )
	{
		m_pIPC->SendCommandToVS(CMD_AddLineToLog, Line);
	}
}

void WTInterface::Show()
{
	if ( IsLoaded() && Debugger != NULL && Debugger->IsDebuggerActive() /*&& !Debugger->BreakASAP*/ )
	{
		m_pIPC->SendCommandToVS(CMD_ShowDllForm);
	}
}

void WTInterface::Close()
{
// 	if ( Debugger != NULL && Debugger->DebuggerLog != NULL )
// 	{
// 		Debugger->DebuggerLog->Logf(TEXT("Unloading UDebugger interface: %s"), IsLoaded() ? TEXT("Currently loaded") : TEXT("Already unloaded"));
// 	}
// 	else
// 	{
// 		warnf(TEXT("Unloading UDebugger interface: %s"), IsLoaded() ? TEXT("Currently loaded") : TEXT("Already unloaded"));
// 	}
// 	
	if ( IsLoaded() )
	{
		m_pIPC->Close();
	}
	UnbindDll();

}

void WTInterface::Hide()
{

}

void WTInterface::Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName )
{
	if ( IsLoaded() )
	{
		FString openName(PackageName);
		openName += TEXT(".");
		openName += ClassName;

		m_pIPC->SendCommandToVS(CMD_EditorLoadClass, *openName);
		m_pIPC->SendCommandToVS(CMD_EditorGotoLine, LineNumber, 1);

		m_pIPC->SendCommandToVS(CMD_EditorGotoLine, objectName);
	}
}
void WTInterface::UpdateCallStack( TArray<FString>& StackNames )
{
	UpdateCallStack();
}
void WTInterface::UpdateCallStack()
{
	if ( IsLoaded() )
	{
		m_pIPC->SendCommandToVS(CMD_CallStackClear);
		for(int i=0;i < Debugger->CallStack->StackDepth;i++) 
		{
			const FStackNode* TestNode = Debugger->CallStack->GetNode(i);
			if (TestNode && TestNode->StackNode && TestNode->StackNode->Node)
			{
				INT Line = TestNode->GetLine();
				INT Position = TestNode->GetPos();
				const FFrame* Frame = TestNode->StackNode;
				FString FrameName = Frame->Node->GetFullName();
				FString FrameData = FString::Printf( TEXT("%d,%p"), Frame->Node->Script.Num(), Frame->Node->Script.GetData() );
				m_pIPC->SendCommandToVS(CMD_CallStackAdd, Line, Position, *FrameName, *FrameData);
			}
		}
		Show();
	}
}

void WTInterface::SetBreakpoint( const TCHAR* ClassName, INT Line )
{
	if ( IsLoaded() && Debugger != NULL )
	{
		FString upper(ClassName);
		upper = upper.ToUpper();
		Debugger->GetBreakpointManager()->SetBreakpoint( ClassName, Line );
		m_pIPC->SendCommandToVS(CMD_AddBreakpoint, Line, 0, *upper);
	}
}

void WTInterface::RemoveBreakpoint( const TCHAR* ClassName, INT Line )
{
	if ( IsLoaded() && Debugger != NULL )
	{
		FString upper(ClassName);
		upper = upper.ToUpper();
		Debugger->GetBreakpointManager()->RemoveBreakpoint( ClassName, Line );
		m_pIPC->SendCommandToVS(CMD_RemoveBreakpoint, Line, 0, *upper);
	}
}	

void WTInterface::UpdateClassTree()
{
	ClassTree.Empty();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* ParentClass = Cast<UClass>(It->SuperStruct);
		if (ParentClass)
		{
			ClassTree.Add( ParentClass, *It );
		}
	}

	if ( IsLoaded() )
	{
		m_pIPC->SendCommandToVS(CMD_ClearHierarchy);
		m_pIPC->SendCommandToVS(CMD_AddClassToHierarchy,  L"Core..Object" );
		RecurseClassTree( UObject::StaticClass() );
		m_pIPC->SendCommandToVS(CMD_BuildHierarchy);
	}
}

void WTInterface::RecurseClassTree( UClass* ParentClass )
{
	TArray<UClass*> ChildClasses;
	ClassTree.MultiFind( ParentClass, ChildClasses );

	for (INT i = 0; i < ChildClasses.Num(); i++)
	{
		// Get package name
		FString FullName = ChildClasses(i)->GetFullName();
		int CutPos = FullName.InStr( TEXT(".") );

		// Extract the package name and chop off the 'Class' thing.
		FString PackageName = FullName.Left( CutPos );
		PackageName = PackageName.Right( PackageName.Len() - 6 );
		m_pIPC->SendCommandToVS(CMD_AddClassToHierarchy, ( *FString::Printf( TEXT("%s.%s.%s"), *PackageName, *ParentClass->GetName(), *ChildClasses(i)->GetName() )) );

		RecurseClassTree( ChildClasses(i) );
	}

	for ( INT i = ChildClasses.Num() - 1; i >= 0; i-- )
	{
		ClassTree.RemovePair( ParentClass, ChildClasses(i) );
	}
}

void WTInterface::LockWatch(int watch)
{
	if ( IsLoaded() )
	{
		m_locked = TRUE;
		m_pIPC->SendCommandToVS(CMD_LockList, watch);
	}
}

void WTInterface::UnlockWatch(int watch)
{
	if ( IsLoaded() )
	{
		m_pIPC->SendCommandToVS(CMD_UnlockList, watch);
		m_locked = FALSE;
	}
}

void WTInterface::BindToDll()
{
// 	warnf(TEXT("BINDING TO DLL (%s)"), m_pIPC != NULL ? TEXT("ALREADY BOUND!") : TEXT("GTG"));
	if ( m_pIPC == NULL )
	{
		m_pIPC = new IPC_UC;
		m_pIPC->Load();
	}
}

void WTInterface::UnbindDll()
{
// 	if ( Debugger != NULL && Debugger->DebuggerLog != NULL )
// 	{
// 		Debugger->DebuggerLog->Logf(TEXT("UNBINDING (%s)"), m_pIPC ? TEXT("GTG") : TEXT("NOT BOUND!!"));
// 	}
// 	warnf(TEXT("=> UNBINDING (%s)"), m_pIPC ? TEXT("GTG") : TEXT("NOT BOUND!!"));
	if ( m_pIPC )
	{
		delete m_pIPC;
		m_pIPC = NULL;
	}
}

#endif
