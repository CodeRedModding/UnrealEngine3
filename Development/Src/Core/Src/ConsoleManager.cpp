/*=============================================================================
ConsoleManager.cpp: console command handling
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ConsoleManager.h"

inline UBOOL IsWhiteSpace(TCHAR Value) { return Value == TCHAR(' '); }

static INT GetHexOrDecimal(const TCHAR* InValue)
{
	INT Value;

	if(InValue[0] == (TCHAR)'0'	&& InValue[1] == (TCHAR)'x')
	{
		appSSCANF(&InValue[2], TEXT("%x"), &Value);
	}
	else
	{
		Value = appAtoi(InValue);
	}

	return Value;
}

static FString GetHexOrDecimalString(INT Value, EConsoleVariableFlags Flags)
{
	if(Flags & ECVF_HexValue)
	{
		return FString::Printf(TEXT("0x%x"), Value);
	}
	else
	{
		return FString::Printf(TEXT("%d"), Value);
	}
}

class FConsoleVariableBase :public IConsoleVariable
{
public:
	/*
	 * Constructor
	 * @param InHelp must not be 0, must not be empty
	 */
	FConsoleVariableBase(const TCHAR* InHelp, EConsoleVariableFlags InFlags)
		:Help(InHelp), Flags(InFlags)
	{
		check(Help);
		check(*Help != 0);
	}

	// interface IConsoleVariable -----------------------------------

	virtual const TCHAR* GetHelp() const
	{
		return Help;
	}
	virtual void SetHelp(const TCHAR* Value)
	{
		check(Value);
		check(*Value != 0);

		Help = Value;
	}
	virtual EConsoleVariableFlags GetFlags() const
	{
		return Flags;
	}
	virtual void SetFlags(const EConsoleVariableFlags Value)
	{
		Flags = Value;
	}

private: // -----------------------------------------

	const TCHAR* Help;
	EConsoleVariableFlags Flags;

protected: // ---------------------------------------

	void OnChanged()
	{
		Flags = (EConsoleVariableFlags)((UINT)Flags | ECVF_Changed);
	}
};

class FConsoleVariableInt : public FConsoleVariableBase
{
public:
	FConsoleVariableInt(INT DefaultValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Value(DefaultValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(INT InValue)
	{
		Value = InValue;
		OnChanged();
	}
	virtual void Set(FLOAT InValue)
	{
		Value = (INT)InValue;
		OnChanged();
	}
	virtual void Set(const TCHAR* InValue)
	{
		Value = GetHexOrDecimal(InValue);
		OnChanged();
	}
	virtual INT GetInt() const
	{
		return Value;
	}
	virtual FLOAT GetFloat() const
	{
		return (FLOAT)Value;
	}
	virtual FString GetString() const
	{
		return GetHexOrDecimalString(Value, GetFlags());
	}

private: // ----------------------------------------------------

	INT Value;
};

class FConsoleVariableFloat : public FConsoleVariableBase
{
public:
	FConsoleVariableFloat(FLOAT DefaultValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Value(DefaultValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(INT InValue)
	{
		Value = (FLOAT)InValue;
		OnChanged();
	}
	virtual void Set(FLOAT InValue)
	{
		Value = InValue;
		OnChanged();
	}
	virtual void Set(const TCHAR* InValue)
	{
		Value = appAtof(InValue);
		OnChanged();
	}
	virtual INT GetInt() const
	{
		return (INT)Value;
	}
	virtual FLOAT GetFloat() const
	{
		return Value;
	}
	virtual FString GetString() const
	{
		return FString::Printf(TEXT("%g"), Value);
	}

private: // ----------------------------------------------------

	FLOAT Value;
};


class FConsoleVariableString : public FConsoleVariableBase
{
public:
	FConsoleVariableString(const TCHAR* DefaultValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Value(DefaultValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(INT InValue)
	{
		Value = FString::Printf(TEXT("%d"), InValue);
		OnChanged();
	}
	virtual void Set(FLOAT InValue)
	{
		Value = FString::Printf(TEXT("%g"), InValue);
		OnChanged();
	}
	virtual void Set(const TCHAR* InValue)
	{
		Value = InValue;
		OnChanged();
	}
	virtual INT GetInt() const
	{
		return appAtoi(*Value);
	}
	virtual FLOAT GetFloat() const
	{
		return appAtof(*Value);
	}
	virtual FString GetString() const
	{
		return Value;
	}

private: // ----------------------------------------------------

	FString Value;
};

class FConsoleVariableIntRef : public FConsoleVariableBase
{
public:
	FConsoleVariableIntRef(INT& InRefValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Value(InRefValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(INT InValue)
	{
		Value = InValue;
		OnChanged();
	}
	virtual void Set(FLOAT InValue)
	{
		Value = (INT)InValue;
		OnChanged();
	}
	virtual void Set(const TCHAR* InValue)
	{
		Value = GetHexOrDecimal(InValue);
		OnChanged();
	}
	virtual INT GetInt() const
	{
		return Value;
	}
	virtual FLOAT GetFloat() const
	{
		return (FLOAT)Value;
	}
	virtual FString GetString() const
	{
		return GetHexOrDecimalString(Value, GetFlags());
	}


private: // ----------------------------------------------------

	INT& Value;
};


class FConsoleVariableFloatRef : public FConsoleVariableBase
{
public:
	FConsoleVariableFloatRef(FLOAT& InRefValue, const TCHAR* Help, EConsoleVariableFlags Flags) 
		: FConsoleVariableBase(Help, Flags), Value(InRefValue)
	{
	}

	// interface IConsoleVariable -----------------------------------

	virtual void Release()
	{
		delete this; 
	} 
	virtual void Set(INT InValue)
	{
		Value = (FLOAT)InValue;
		OnChanged();
	}
	virtual void Set(FLOAT InValue)
	{
		Value = InValue;
		OnChanged();
	}
	virtual void Set(const TCHAR* InValue)
	{
		Value = appAtof(InValue);
		OnChanged();
	}
	virtual INT GetInt() const
	{
		return (INT)Value;
	}
	virtual FLOAT GetFloat() const
	{
		return Value;
	}
	virtual FString GetString() const
	{
		return FString::Printf(TEXT("%g"), Value);
	}

private: // ----------------------------------------------------

	FLOAT& Value;
};

IConsoleVariable* FConsoleManager::RegisterConsoleVariable(const TCHAR* Name, INT DefaultValue, const TCHAR* Help, UINT Flags)
{
	return AddConsoleVariable(Name, new FConsoleVariableInt(DefaultValue, Help, (EConsoleVariableFlags)Flags));
}
IConsoleVariable* FConsoleManager::RegisterConsoleVariable(const TCHAR* Name, FLOAT DefaultValue, const TCHAR* Help, UINT Flags)
{
	return AddConsoleVariable(Name, new FConsoleVariableFloat(DefaultValue, Help, (EConsoleVariableFlags)Flags));
}
IConsoleVariable* FConsoleManager::RegisterConsoleVariable(const TCHAR* Name, const TCHAR *DefaultValue, const TCHAR* Help, UINT Flags)
{
	return AddConsoleVariable(Name, new FConsoleVariableString(DefaultValue, Help, (EConsoleVariableFlags)Flags));
}
IConsoleVariable* FConsoleManager::RegisterConsoleVariableRef(const TCHAR* Name, INT& RefValue, const TCHAR* Help, UINT Flags)
{
	return AddConsoleVariable(Name, new FConsoleVariableIntRef(RefValue, Help, (EConsoleVariableFlags)Flags));
}
IConsoleVariable* FConsoleManager::RegisterConsoleVariableRef(const TCHAR* Name, FLOAT& RefValue, const TCHAR* Help, UINT Flags)
{
	return AddConsoleVariable(Name, new FConsoleVariableFloatRef(RefValue, Help, (EConsoleVariableFlags)Flags));
}

IConsoleVariable* FConsoleManager::FindConsoleVariable(const TCHAR* Name, UBOOL bCaseSensitive) const
{
	IConsoleVariable* CVar = FindConsoleVariableUnfiltered(Name, bCaseSensitive);

	if(CVar && CVar->TestFlags(ECVF_CreatedFromIni))
	{
		return 0;
	}

	return CVar;
}

IConsoleVariable* FConsoleManager::FindConsoleVariableUnfiltered(const TCHAR* Name, UBOOL bCaseSensitive) const
{
	if(bCaseSensitive)
	{
		// much faster
		IConsoleVariable* const* VarPtr = Variables.Find(Name);

		if(!VarPtr)
		{
			return 0;
		}

		return *VarPtr;
	}
	else
	{
		// This should be improved:
		// We currently use TMap<FString, IConsoleVariable*> which is actually
		// can insensitive and faster than the iteration code here but ideally
		// we use a case insensitive map which is faster and keep the slow
		// iteration for the rare case of user input.

		// convenient but slow O(n)
		FFindVisitor Visitor(Name);

		ForEachConsoleVariable(&Visitor, Name);

		return Visitor.Result;
	}
}

void FConsoleManager::UnregisterConsoleVariable(IConsoleVariable* CVar)
{
	if(!CVar)
	{
		return;
	}

	UINT New = (UINT)CVar->GetFlags() | (UINT)ECVF_Unregistered;

	CVar->SetFlags((EConsoleVariableFlags)New);
}

void FConsoleManager::ForEachConsoleVariable(IConsoleVariableVisitor* Visitor, const TCHAR* ThatStartsWith) const
{
	check(Visitor);
	check(ThatStartsWith);

	for(TMap<FString, IConsoleVariable*>::TConstIterator PairIt(Variables); PairIt; ++PairIt)
	{
		const FString& Name = PairIt.Key();
		IConsoleVariable* CVar = PairIt.Value();

		if(MatchPartialName(&Name[0], ThatStartsWith))
		{
			Visitor->OnConsoleVariable(&Name[0], CVar);
		}
	}
}

UBOOL FConsoleManager::ProcessUserConsoleInput(const TCHAR* InInput, FOutputDevice& Ar) const
{
	check(InInput);

	const TCHAR* It = InInput;

	FString Param1 = GetTextSection(It);
	if(Param1.IsEmpty())
	{
		return FALSE;
	}

	IConsoleVariable* CVar = FindConsoleVariable(&Param1[0], FALSE);
	if(!CVar)
	{
		return FALSE;
	}

#if FINAL_RELEASE
	if(CVar->TestFlags(ECVF_Cheat))
	{
		return FALSE;
	}
#endif // FINAL_RELEASE

	if(CVar->TestFlags(ECVF_Unregistered))
	{
		return FALSE;
	}

	// fix case for nicer printout
	Param1 = FindConsoleVariableName(CVar);

	if(*It == 0)
	{
		// get current state
		Ar.Logf(TEXT("%s = %s"), *Param1, *CVar->GetString());
	}
	else
	{
		FString Param2 = GetTextSection(It);

		UBOOL bReadOnly = CVar->TestFlags(ECVF_ReadOnly);

		if(Param2 == TEXT("?"))
		{
			// get help
			Ar.Logf(TEXT("HELP for '%s'%s:\n%s"), *Param1, bReadOnly ? TEXT("(ReadOnly)") : TEXT(""), CVar->GetHelp());
		}
		else
		{
			if(bReadOnly)
			{
				Ar.Logf(TEXT("Error: %s is read only!"), *Param1, *CVar->GetString());
			}
			else
			{
				// set value
				CVar->Set(&Param2[0]);

				Ar.Logf(TEXT("%s = %s"), *Param1, *CVar->GetString());
			}
		}
	}

	return TRUE;
}

IConsoleVariable* FConsoleManager::AddConsoleVariable(const TCHAR* Name, IConsoleVariable* Var)
{
	check(Name);
	check(*Name != 0);
	check(Var);

	IConsoleVariable* ExistingVar = FindConsoleVariableUnfiltered(Name, FALSE);

	if(ExistingVar)
	{
		if(ExistingVar->TestFlags(ECVF_Unregistered))
		{
			if(ExistingVar->TestFlags(ECVF_CreatedFromIni))
			{
				// The existing one came from the ini, get the value and destroy the existing one.
				Var->Set(*ExistingVar->GetString());
				ExistingVar->Release();

				Variables.Set(Name, Var);
				return Var;
			}
			else
			{
				// Copy data over from the new variable,
				// but keep the value from the existing one.
				// This way references to the old variable are preserved (no crash).
				// Changing the type of a variable however is not possible with this.
				ExistingVar->SetFlags(Var->GetFlags());
				ExistingVar->SetHelp(Var->GetHelp());

				// Name was already registered but got unregistered
				Var->Release();

				return ExistingVar;
			}
		}

		// Name was already in use
		Var->Release();
		return 0;
	}
	else
	{
		Variables.Set(Name, Var);
		return Var;
	}
}

FString FConsoleManager::GetTextSection(const TCHAR* &It)
{
	FString ret;

	while(*It)
	{
		if(IsWhiteSpace(*It))
		{
			break;
		}

		ret += *It++;
	}

	while(IsWhiteSpace(*It))
	{
		++It;
	}

	return ret;
}

FString FConsoleManager::FindConsoleVariableName(IConsoleVariable* InVar) const
{
	check(InVar);

	for(TMap<FString, IConsoleVariable*>::TConstIterator PairIt(Variables); PairIt; ++PairIt)
	{
		IConsoleVariable* Var = PairIt.Value();

		if(Var == InVar)
		{
			const FString& Name = PairIt.Key();

			return Name;
		}
	}

	return FString();
}

UBOOL FConsoleManager::MatchPartialName(const TCHAR* Stream, const TCHAR* Pattern)
{
	while(*Pattern)
	{
		if(appToLower(*Stream) != appToLower(*Pattern))
		{
			return FALSE;
		}

		++Stream;
		++Pattern;
	}

	return TRUE;
}
