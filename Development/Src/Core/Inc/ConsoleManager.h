/*=============================================================================
ConsoleManager.h: console command handling
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __CONSOLEMANAGER_H__
#define __CONSOLEMANAGER_H__

#include "IConsoleManager.h"

class FConsoleManager :public IConsoleManager
{
public:
	/** destructor */
	~FConsoleManager()
	{
		for(TMap<FString, IConsoleVariable*>::TConstIterator PairIt(Variables); PairIt; ++PairIt)
		{
			IConsoleVariable* Var = PairIt.Value();

			delete Var;
		}
	}

	// interface IConsoleManager -----------------------------------

	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, INT DefaultValue, const TCHAR* Help, UINT Flags);
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, FLOAT DefaultValue, const TCHAR* Help, UINT Flags);
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, const TCHAR *DefaultValue, const TCHAR* Help, UINT Flags);
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, INT& RefValue, const TCHAR* Help, UINT Flags);
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, FLOAT& RefValue, const TCHAR* Help, UINT Flags);
	virtual void UnregisterConsoleVariable(IConsoleVariable* CVar);
	virtual IConsoleVariable* FindConsoleVariable(const TCHAR* Name, UBOOL bCaseSensitive) const;
	virtual void ForEachConsoleVariable(IConsoleVariableVisitor* Visitor, const TCHAR* ThatStartsWith) const;
	virtual UBOOL ProcessUserConsoleInput(const TCHAR* InInput, FOutputDevice& Ar) const;

private: // ----------------------------------------------------

	// [name] = pointer (pointer must not be 0)
	TMap<FString, IConsoleVariable*>		Variables;

	class FFindVisitor : public IConsoleVariableVisitor
	{
	public:
		/** constructor */
		FFindVisitor(const TCHAR* InNameToFind) :Result(0), LengthToFind(appStrlen(InNameToFind))
		{
		}

		// interface IConsoleVariableVisitor --------------------

		virtual void OnConsoleVariable(const TCHAR *Name, IConsoleVariable* CVar)
		{
			// we reject partical matches
			if(appStrlen(Name) == LengthToFind)
			{
				Result = CVar;
			}
		}

		IConsoleVariable*			Result;
		UINT						LengthToFind;
	};

	/** 
	 * @param Name must not be 0, must not be empty
	 * @param Var must not be 0
	 * @return 0 if the name was already in use
	 */
	IConsoleVariable* AddConsoleVariable(const TCHAR* Name, IConsoleVariable* Var);
	/** @param InVar must not be 0 */
	FString FindConsoleVariableName(IConsoleVariable* InVar) const;
	/**
	 * @param Stream must not be 0
	 * @param Pattern must not be 0
	 */
	static UBOOL MatchPartialName(const TCHAR* Stream, const TCHAR* Pattern);
	/**
	 * Get string till whitespace, jump over whitespace
	 * inefficient but this code is not performance critical
	 */
	static FString GetTextSection(const TCHAR* &It);
	/** same as FindConsoleVariable() but ECVF_CreatedFromIni are not filtered out (for internal use) */
	IConsoleVariable* FindConsoleVariableUnfiltered(const TCHAR* Name, UBOOL bCaseSensitive) const;
};

#endif // __ICONSOLEMANAGER_H__
