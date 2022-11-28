/*=============================================================================
	UnDelphiInterface.cpp: Debugger Interface Interface
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
=============================================================================*/

#include "..\Src\EnginePrivate.h"

#include "UnDebuggerCore.h"
#include "UnDebuggerInterface.h"
#include "UnDelphiInterface.h"

void DelphiCallback( const char* C )
{
	((DelphiInterface*)((UDebuggerCore*)GDebugger)->GetInterface())->Callback( C );
}

DelphiInterface::DelphiInterface( const TCHAR* InDLLName ) 
:	hInterface(NULL),
	DelphiShowDllForm(NULL),
	DelphiAddClassToHierarchy(NULL),
	DelphiBuildHierarchy(NULL),
	DelphiClearHierarchy(NULL),
	DelphiSetCallback(NULL),
	DelphiEditorCommand(NULL),
	DelphiEditorLoadTextBuffer(NULL),
	DelphiClearWatch(NULL),
	DelphiAddWatch(NULL),
	DelphiAddBreakpoint(NULL),
	DelphiRemoveBreakpoint(NULL),
	DelphiEditorGotoLine(NULL),
	DelphiAddLineToLog(NULL),
	DelphiEditorLoadClass(NULL),
	DelphiCallStackClear(NULL),
	DelphiCallStackAdd(NULL),
	DelphiDebugWindowState(NULL),
	DelphiClearAWatch(NULL),
	DelphiAddAWatch(NULL),
	DelphiLockList(NULL),
	DelphiUnlockList(NULL),
	DelphiSetCurrentObjectName(NULL)
{
	DllName = InDLLName;
	Debugger = NULL;
	LoadCount = 0;
}

DelphiInterface::~DelphiInterface()
{
	Debugger = 0;
}

int DelphiInterface::AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data)
{
	return DelphiAddAWatch(watch, ParentIndex, TCHAR_TO_ANSI(ObjectName), TCHAR_TO_ANSI(Data));
}


void DelphiInterface::ClearAWatch(int watch)
{
	DelphiClearAWatch(watch);
}

UBOOL DelphiInterface::Initialize( UDebuggerCore* DebuggerOwner )
{
	Debugger = DebuggerOwner;
	if ( LoadCount == 0 )
	{
		BindToDll();
		DelphiSetCallback( &DelphiCallback );
		DelphiClearWatch( LOCAL_WATCH );
		DelphiClearWatch( GLOBAL_WATCH );
		DelphiClearWatch( WATCH_WATCH );
	}
	Show();
	return TRUE;
}

void DelphiInterface::Callback( const char* C )
{
	// uncomment to log all callback mesages from the UI
	const TCHAR* command = ANSI_TO_TCHAR(C);
//	debugf(TEXT("Callback: %s"), command);

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
	else if(ParseCommand(&command, TEXT("breakonnone")))
	{
		TCHAR breakValueString[5];
		ParseToken(command, breakValueString, 5, 0);
		Debugger->SetBreakOnNone(appAtoi(breakValueString));
	}
	else if(ParseCommand(&command, TEXT("break")))
		Debugger->BreakASAP = 1;

	else if(ParseCommand(&command, TEXT("stopdebugging")))
	{
		Debugger->Close();
		return;
	}
	else if (Debugger->IsDebugging)
	{
		EUserAction Action = UA_None;
		if(ParseCommand(&command, TEXT("go")))
			Action = UA_Go;
		else if ( ParseCommand(&command,TEXT("stepinto")) )
			Action = UA_StepInto;
		else if ( ParseCommand(&command,TEXT("stepover")) )
			Action = UA_StepOverStack;
		else if(ParseCommand(&command, TEXT("stepoutof")))
			Action = UA_StepOut;
		Debugger->ProcessInput(Action);
	}
}

void DelphiInterface::AddToLog( const TCHAR* Line )
{
	DelphiAddLineToLog(TCHAR_TO_ANSI(Line));
}

void DelphiInterface::Show()
{
	DelphiShowDllForm();
}

void DelphiInterface::Close()
{
	if ( hInterface )
	{
		if ( !FreeLibrary(hInterface) )
		{
			debugf(NAME_Warning, TEXT("Unknown error attempting to unload interface dll '%s'"), *DllName);
			return;
		}
        LoadCount--;

		if ( LoadCount == 0 )
			UnbindDll();
	}

	hInterface = NULL;
}

void DelphiInterface::Hide()
{

}

void DelphiInterface::Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName )
{
	FString openName(PackageName);
	openName += TEXT(".");
	openName += ClassName;

	DelphiEditorLoadClass(TCHAR_TO_ANSI(*openName));
	DelphiEditorGotoLine(LineNumber, 1);

	DelphiSetCurrentObjectName(TCHAR_TO_ANSI(objectName));
}

void DelphiInterface::UpdateCallStack( TArray<FString>& StackNames )
{
	DelphiCallStackClear();
	for(INT i = 0; i<StackNames.Num(); i++)
		DelphiCallStackAdd(TCHAR_TO_ANSI(*StackNames(i)));
}

void DelphiInterface::SetBreakpoint( const TCHAR* ClassName, INT Line )
{
	FString upper(ClassName);
	upper = upper.Caps();
	Debugger->GetBreakpointManager()->SetBreakpoint( ClassName, Line );
	DelphiAddBreakpoint(TCHAR_TO_ANSI(*upper), Line);
}

void DelphiInterface::RemoveBreakpoint( const TCHAR* ClassName, INT Line )
{
	FString upper(ClassName);
	upper = upper.Caps();
	Debugger->GetBreakpointManager()->RemoveBreakpoint( ClassName, Line );
	DelphiRemoveBreakpoint(TCHAR_TO_ANSI(*upper), Line);
}	

void DelphiInterface::UpdateClassTree()
{
	ClassTree.Empty();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* ParentClass = Cast<UClass>(It->SuperField);
		if (ParentClass)
			ClassTree.Add( ParentClass, *It );
	}

	DelphiClearHierarchy();
	DelphiAddClassToHierarchy( "Core..Object" );
	RecurseClassTree( UObject::StaticClass() );
	DelphiBuildHierarchy();
}

void DelphiInterface::RecurseClassTree( UClass* ParentClass )
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
		DelphiAddClassToHierarchy( TCHAR_TO_ANSI(*FString::Printf( TEXT("%s.%s.%s"), *PackageName, ParentClass->GetName(), ChildClasses(i)->GetName() )) );

		RecurseClassTree( ChildClasses(i) );
	}

	for ( INT i = ChildClasses.Num() - 1; i >= 0; i-- )
		ClassTree.RemovePair( ParentClass, ChildClasses(i) );
}

void DelphiInterface::LockWatch(int watch)
{
	DelphiLockList(watch);
}

void DelphiInterface::UnlockWatch(int watch)
{
	DelphiUnlockList(watch);
}

void DelphiInterface::BindToDll()
{
	if ( LoadCount > 0 )
		return;

	hInterface = LoadLibrary( *DllName );
	if ( hInterface == NULL )
	{
		debugf( NAME_Warning, TEXT("Couldn't load interface dll '%s'"), *DllName );
		hInterface = LoadLibrary(TEXT("DebuggerInterface.dll"));
	}

	if ( hInterface == NULL )
	{
		appThrowf(TEXT("No suitable interface dll found!"));
		return;
	}

	LoadCount++;

	// Get pointers to the delphi functions
	DelphiShowDllForm			= (DelphiVoidVoid)			GetProcAddress( hInterface, "ShowDllForm" );
	DelphiEditorCommand			= (DelphiVoidChar)			GetProcAddress( hInterface, "EditorCommand" );
	DelphiEditorLoadTextBuffer	= (DelphiVoidCharChar)		GetProcAddress( hInterface, "EditorLoadTextBuffer" );
	DelphiAddClassToHierarchy	= (DelphiVoidChar)			GetProcAddress( hInterface, "AddClassToHierarchy" );
	DelphiBuildHierarchy		= (DelphiVoidVoid)			GetProcAddress( hInterface, "BuildHierarchy" );
	DelphiClearHierarchy		= (DelphiVoidVoid)			GetProcAddress( hInterface, "ClearHierarchy" );
	DelphiBuildHierarchy		= (DelphiVoidVoid)			GetProcAddress( hInterface, "BuildHierarchy" );
	DelphiClearWatch			= (DelphiVoidInt)			GetProcAddress( hInterface, "ClearWatch" );
	DelphiAddWatch				= (DelphiVoidIntChar)		GetProcAddress( hInterface, "AddWatch" );
	DelphiSetCallback			= (DelphiVoidVoidPtr)		GetProcAddress( hInterface, "SetCallback" );
	DelphiAddBreakpoint			= (DelphiVoidCharInt)		GetProcAddress( hInterface, "AddBreakpoint" );
	DelphiRemoveBreakpoint		= (DelphiVoidCharInt)		GetProcAddress( hInterface, "RemoveBreakpoint" );
	DelphiEditorGotoLine		= (DelphiVoidIntInt)		GetProcAddress( hInterface, "EditorGotoLine" );
	DelphiAddLineToLog			= (DelphiVoidChar)			GetProcAddress( hInterface, "AddLineToLog" );
	DelphiEditorLoadClass		= (DelphiVoidChar)			GetProcAddress( hInterface, "EditorLoadClass" );
	DelphiCallStackClear		= (DelphiVoidVoid)			GetProcAddress( hInterface, "CallStackClear" );
	DelphiCallStackAdd			= (DelphiVoidChar)			GetProcAddress( hInterface, "CallStackAdd" );
	DelphiDebugWindowState		= (DelphiVoidInt)			GetProcAddress( hInterface, "DebugWindowState" );
	DelphiClearAWatch			= (DelphiVoidInt)			GetProcAddress( hInterface, "ClearAWatch" );
	DelphiAddAWatch				= (DelphiIntIntIntCharChar)	GetProcAddress( hInterface, "AddAWatch" );
	DelphiLockList				= (DelphiVoidInt)			GetProcAddress( hInterface, "LockList" );
	DelphiUnlockList			= (DelphiVoidInt)			GetProcAddress( hInterface, "UnlockList" );
	DelphiSetCurrentObjectName	= (DelphiVoidChar)			GetProcAddress( hInterface, "SetCurrentObjectName" );
}

void DelphiInterface::UnbindDll()
{
	DelphiShowDllForm			= NULL;
	DelphiEditorCommand			= NULL;
	DelphiEditorLoadTextBuffer	= NULL;
	DelphiAddClassToHierarchy	= NULL;
	DelphiBuildHierarchy		= NULL;
	DelphiClearHierarchy		= NULL;
	DelphiBuildHierarchy		= NULL;
	DelphiClearWatch			= NULL;
	DelphiAddWatch				= NULL;
	DelphiSetCallback			= NULL;
	DelphiAddBreakpoint			= NULL;
	DelphiRemoveBreakpoint		= NULL;
	DelphiEditorGotoLine		= NULL;
	DelphiAddLineToLog			= NULL;
	DelphiEditorLoadClass		= NULL;
	DelphiCallStackClear		= NULL;
	DelphiCallStackAdd			= NULL;
	DelphiDebugWindowState		= NULL;
	DelphiClearAWatch			= NULL;
	DelphiAddAWatch				= NULL;
	DelphiLockList				= NULL;
	DelphiUnlockList			= NULL;
	DelphiSetCurrentObjectName	= NULL;
}

