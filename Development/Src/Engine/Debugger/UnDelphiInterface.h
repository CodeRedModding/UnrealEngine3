/*=============================================================================
	UnDelphiInterface.h: Delphi(new) Debugger Interface interface
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
=============================================================================*/

#ifndef __UDELPHIINTERFACE_H__
#define __UDELPHIINTERFACE_H__

typedef void (*DelphiVoidVoid)			(void);
typedef void (*DelphiVoidChar)			(const char*);
typedef void (*DelphiVoidVoidPtr)		(void*);
typedef void (*DelphiVoidCharInt)		(const char*, int);
typedef void (*DelphiVoidInt)			(int);
typedef void (*DelphiVoidIntInt)		(int,int);
typedef void (*DelphiVoidCharChar)		(const char*, const char*);
typedef void (*DelphiVoidIntChar)		(int, const char*);
typedef int  (*DelphiIntIntIntCharChar)	(int, int, const char*, const char*);

void DelphiCallback( const char* C );
 
class DelphiInterface : public UDebuggerInterface
{
public:
	DelphiInterface( const TCHAR* DLLName );
	~DelphiInterface();

	UBOOL Initialize( class UDebuggerCore* DebuggerOwner );
	virtual void Close();
	virtual void Show();
	virtual void Hide();
	virtual void AddToLog( const TCHAR* Line );
	virtual void Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName );
	virtual void UpdateCallStack( TArray<FString>& StackNames );
	virtual void SetBreakpoint( const TCHAR* ClassName, INT Line );
	virtual void RemoveBreakpoint( const TCHAR* ClassName, INT Line );
	virtual void UpdateClassTree();
	virtual int AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data);
	virtual void ClearAWatch(int watch);
	virtual void LockWatch(int watch);
	virtual void UnlockWatch(int watch);

	void Callback( const char* C );

protected:
	UBOOL IsLoaded() const { return LoadCount > 0; }

private:
	FString DllName;
	HMODULE hInterface;
	TMultiMap<UClass*, UClass*> ClassTree;

	void BindToDll();
	void UnbindDll();
	void RecurseClassTree( UClass* RClass );


	DelphiVoidVoid			DelphiShowDllForm;
	DelphiVoidChar			DelphiEditorCommand;
	DelphiVoidCharChar		DelphiEditorLoadTextBuffer;
	DelphiVoidChar			DelphiAddClassToHierarchy;
	DelphiVoidVoid			DelphiClearHierarchy;
	DelphiVoidVoid			DelphiBuildHierarchy;
	DelphiVoidInt			DelphiClearWatch;
	DelphiVoidIntChar		DelphiAddWatch;
	DelphiVoidVoidPtr		DelphiSetCallback;
	DelphiVoidCharInt		DelphiAddBreakpoint;
	DelphiVoidCharInt		DelphiRemoveBreakpoint;
	DelphiVoidIntInt		DelphiEditorGotoLine;
	DelphiVoidChar			DelphiAddLineToLog;
	DelphiVoidChar			DelphiEditorLoadClass;
	DelphiVoidVoid			DelphiCallStackClear;
	DelphiVoidChar			DelphiCallStackAdd;
	DelphiVoidInt			DelphiDebugWindowState;
	DelphiVoidInt			DelphiClearAWatch;
	DelphiIntIntIntCharChar	DelphiAddAWatch;
	DelphiVoidInt			DelphiLockList;
	DelphiVoidInt			DelphiUnlockList;
	DelphiVoidChar			DelphiSetCurrentObjectName;
};

#endif