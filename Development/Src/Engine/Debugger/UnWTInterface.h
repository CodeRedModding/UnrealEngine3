/*=============================================================================
UnWTInterface.h: VA(new) Debugger Interface interface
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UWTINTERFACE_H__
#define __UWTINTERFACE_H__

typedef void (*VAVoidVoid)			(void);
typedef void (*VAVoidChar)			(LPCWSTR);
typedef void (*VAVoidVoidPtr)		(void*);
typedef void (*VAVoidCharInt)		(LPCWSTR, int);
typedef void (*VAVoidInt)			(int);
typedef void (*VAVoidIntInt)		(int,int);
typedef void (*VAVoidCharChar)		(LPCWSTR, LPCWSTR);
typedef void (*VAVoidIntChar)		(int, LPCWSTR);
typedef int  (*VAIntIntIntCharChar)	(int, int, LPCWSTR, LPCWSTR);

class IPC_UC;
class WTInterface : public UDebuggerInterface
{
public:
	BOOL m_locked;
	WTInterface( const TCHAR* DLLName );
	~WTInterface();

	UBOOL Initialize( class UDebuggerCore* DebuggerOwner );
	virtual void NotifyBeginTick();
	virtual UBOOL NotifyDebugInfo( UBOOL* allowDetach );
	virtual void Close();
	virtual void Show();
	virtual void Hide();
	virtual void AddToLog( const TCHAR* Line );
	virtual void Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName );
	virtual void UpdateCallStack( TArray<FString>& StackNames );
	virtual void UpdateCallStack( );
	virtual void SetBreakpoint( const TCHAR* ClassName, INT Line );
	virtual void RemoveBreakpoint( const TCHAR* ClassName, INT Line );
	virtual void UpdateClassTree();
	virtual int AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data);
	virtual void ClearAWatch(int watch);
	virtual void LockWatch(int watch);
	virtual void UnlockWatch(int watch);

	void Callback( INT cmdID,  LPCWSTR cmdStr );

protected:
	UBOOL IsLoaded() const;

private:
	FString DllName;
	HMODULE hInterface;
	TMultiMap<UClass*, UClass*> ClassTree;

	void BindToDll();
	void UnbindDll();
	void RecurseClassTree( UClass* RClass );


// 	VAVoidVoid			VAShowDllForm;
// 	VAVoidChar			VAEditorCommand;
// 	VAVoidCharChar		VAEditorLoadTextBuffer;
// 	VAVoidChar			VAAddClassToHierarchy;
// 	VAVoidVoid			VAClearHierarchy;
// 	VAVoidVoid			VABuildHierarchy;
// 	VAVoidInt			VAClearWatch;
// 	VAVoidIntChar		VAAddWatch;
// 	VAVoidVoidPtr		VASetCallback;
// 	VAVoidCharInt		VAAddBreakpoint;
// 	VAVoidCharInt		VARemoveBreakpoint;
// 	VAVoidIntInt		VAEditorGotoLine;
// 	VAVoidChar			VAAddLineToLog;
// 	VAVoidChar			VAEditorLoadClass;
// 	VAVoidVoid			VACallStackClear;
// 	VAVoidChar			VACallStackAdd;
// 	VAVoidInt			VADebugWindowState;
// 	VAVoidInt			VAClearAWatch;
// 	VAIntIntIntCharChar	VAAddAWatch;
// 	VAVoidInt			VALockList;
// 	VAVoidInt			VAUnlockList;
// 	VAVoidChar			VASetCurrentObjectName;
	IPC_UC *m_pIPC;
};

#endif
