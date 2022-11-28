/*=============================================================================
	UMakeCommandlet.cpp: UnrealEd script recompiler.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"


IMPLEMENT_COMPARE_POINTER( UClass, UMakeCommandlet, { return appStricmp(A->GetName(),B->GetName()); } )
IMPLEMENT_COMPARE_CONSTREF( FString, UMakeCommandlet, { return appStricmp(*A,*B); } )

/**
 * Helper class used to cache UClass* -> TCHAR* name lookup for finding the named used for C++ declaration.
 */
struct FNameLookupCPP
{
	/**
	 * Destructor, cleaning up allocated memory.	
	 */
	~FNameLookupCPP()
	{
		for( TMap<UStruct*,TCHAR*>::TIterator It(StructNameMap); It; ++It )
		{
			TCHAR* Name = It.Value();
			delete [] Name;
		}
	}

	/**
	 * Returns the name used for declaring the passed in struct in C++
	 * 
	 * @param	UStruct to obtain C++ name for
	 * @return	Name used for C++ declaration
	 */
	const TCHAR* GetNameCPP( UStruct* Struct )
	{
		TCHAR* NameCPP = StructNameMap.FindRef( Struct );
		if( NameCPP )
		{
			return NameCPP;
		}
		else
		{
			FString	TempName		= FString::Printf( TEXT("%s%s"), Struct->GetPrefixCPP(), Struct->GetName() );
			INT		StringLength	= TempName.Len();
			NameCPP					= new TCHAR[StringLength + 1];
			appStrcpy( NameCPP, *TempName );
			NameCPP[StringLength]	= 0;
			StructNameMap.Set( Struct, NameCPP );
			return NameCPP;
		}
	}

private:
	/** Map of UStruct pointers to C++ names */
	TMap<UStruct*,TCHAR*> StructNameMap;
};

/** C++ name lookup helper */
static FNameLookupCPP* NameLookupCPP;

//
//	FNativeClassHeaderGenerator
//

struct FNativeClassHeaderGenerator
{
private:

	TArray<UClass*>		Classes;
	FString				ClassHeaderFilename,
						API;
	UObject*			Package;
	UBOOL				MasterHeader;
	FStringOutputDevice	HeaderText;

	//	ExportClassHeader - Appends the header definition for an inheritance heirarchy of classes to the header.

	void ExportClassHeader(UClass* Class,UBOOL ExportedParent = 0)
	{
		INT	TextIndent = 0;

		// Export all classes we are dependent on.
		for( INT DependsOnIndex=0; DependsOnIndex<Class->DependentOn.Num(); DependsOnIndex++ )
		{
			for( INT ClassIndex=0; ClassIndex<Classes.Num(); ClassIndex++ )
			{
				if( Classes(ClassIndex)->GetFName() == Class->DependentOn(DependsOnIndex) )
				{
					UClass* DependsOnClass = FindObject<UClass>(ANY_PACKAGE,*Class->DependentOn(DependsOnIndex));
					if( !DependsOnClass )
					{
						warnf(NAME_Error,TEXT("Unknown class %s used in conjunction with dependson"), *Class->DependentOn(DependsOnIndex));
						return;
					}
					else if( !(DependsOnClass->ClassFlags & CLASS_Exported) )
					{
						// Find first base class of DependsOnClass that is not a base class of Class. We can assume there is no manual 
						// dependency on the base class as script compiler treats this as an error. Furthermore we can also assume that
						// there is no circular dependency chain as this is also caught by the compiler.
						while( !Class->IsChildOf( DependsOnClass->GetSuperClass() ) )
						{
							DependsOnClass = DependsOnClass->GetSuperClass();
						}
						ExportClassHeader(DependsOnClass,DependsOnClass->GetSuperClass()->GetFlags() & RF_TagExp);
					}
				}
			}
		}

		// We're  no longer dependent on any classes as this class has been parsed and now exported.
		Class->DependentOn.Empty();

		// Export class header.
		if((Class->GetFlags() & RF_Native) && Class->ScriptText && Class->GetOuter() == Package && !(Class->ClassFlags & (CLASS_NoExport|CLASS_Exported)))
		{
			if(Class->ClassHeaderFilename == ClassHeaderFilename)
			{
				// Mark class as exported.
				Class->ClassFlags |= CLASS_Exported;

				// Enum definitions.
				for( TFieldIterator<UEnum> ItE(Class); ItE && ItE.GetStruct()==Class; ++ItE )
				{
					// Export enum.
					if( ItE->GetOuter()==Class )
					{
						HeaderText.Logf( TEXT("%senum %s\r\n{\r\n"), appSpc(TextIndent), ItE->GetName() );
						INT i;
						for( i=0; i<ItE->Names.Num(); i++ )
						{
							HeaderText.Logf( TEXT("%s    %-24s=%i,\r\n"), appSpc(TextIndent), *ItE->Names(i), i );
						}
						// Find the longest common prefix of all items in the enumeration.
						const TArray<FName>& Names = ItE->Names;
						if (Names.Num() > 0)
						{
							FString Prefix = *Names(0);

							// For each item in the enumeration, trim the prefix as much as necessary to keep it a prefix.
							// This ensures that once all items have been processed, a common prefix will have been constructed.
							// This will be the longest common prefix since as little as possible is trimmed at each step.
							for (INT NameIdx = 1; NameIdx < Names.Num(); NameIdx++)
							{
								const FString& EnumItemName = *Names(NameIdx);
								
								// Find the length of the longest common prefix of Prefix and EnumItemName.
								INT PrefixIdx = 0;
								while (PrefixIdx < Prefix.Len() && PrefixIdx < EnumItemName.Len() && Prefix[PrefixIdx] == EnumItemName[PrefixIdx])
								{
									PrefixIdx++;
								}

								// Trim the prefix to the length of the common prefix.
								Prefix = Prefix.Left(PrefixIdx);
							}

							// Find the index of the rightmost underscore in the prefix.
							INT UnderscoreIdx = Prefix.InStr(TEXT("_"), true);

							// If an underscore was found, trim the prefix so only the part before the rightmost underscore is included.
							if (UnderscoreIdx > 0)
							{
								Prefix = Prefix.Left(UnderscoreIdx);
							}

							// If no common prefix was found, use the name of the enumeration instead.
							if (Prefix.Len() == 0)
							{
								Prefix = ItE->GetName();
							}

							// Print out the MAX enumeration item using the prefix.
							FString MaxEnumItem = Prefix + FString("_MAX");
							HeaderText.Logf( TEXT("%s    %-24s=%i,\r\n"), appSpc(TextIndent), *MaxEnumItem, i );
						}
						HeaderText.Logf( TEXT("};\r\n") );
					}
					else HeaderText.Logf( TEXT("%senum %s;\r\n"), appSpc(TextIndent), ItE->GetName() );
				}

				// Struct definitions.
				TArray<UStruct*> NativeStructs;
				for( TFieldIterator<UStruct> ItS(Class); ItS && ItS.GetStruct()==Class; ++ItS )
				{
					if( ( ItS->GetFlags() & RF_Native) || (ItS->StructFlags & STRUCT_Native) )
						NativeStructs.AddItem(*ItS);
				}
				// reverse the order.
				for( INT i=NativeStructs.Num()-1; i>=0; --i )
				{
					UStruct* ItS = NativeStructs(i);

					// Export struct.
					HeaderText.Logf( TEXT("struct %s"), NameLookupCPP->GetNameCPP( ItS ) );
					if( ItS->SuperField )
						HeaderText.Logf(TEXT(" : public %s\r\n"), NameLookupCPP->GetNameCPP( ItS->GetSuperStruct() ) );
					HeaderText.Logf( TEXT("\r\n{\r\n") );
					TFieldIterator<UProperty,CLASS_IsAUProperty> LastIt = NULL;
					for( TFieldIterator<UProperty,CLASS_IsAUProperty> It2(ItS); It2; ++It2 )
					{
						if( It2.GetStruct()==ItS && It2->ElementSize )
						{
							HeaderText.Logf( appSpc(TextIndent+4) );
							It2->ExportCpp( HeaderText, 0, 0, 0, ItS->StructFlags&STRUCT_Export ? 1 : 0 );
							if (It2->IsA(UBoolProperty::StaticClass()))
							{
								if( !LastIt || !LastIt->IsA(UBoolProperty::StaticClass()) )
									HeaderText.Logf( TEXT(" GCC_PACK(PROPERTY_ALIGNMENT)") );
							}
							else
							{
								if ( LastIt != NULL )
								{
									if ( (LastIt->IsA(UBoolProperty::StaticClass())) ||
										(LastIt->IsA(UByteProperty::StaticClass()) && !It2->IsA(UByteProperty::StaticClass()))
									)
									{
										HeaderText.Logf( TEXT(" GCC_PACK(PROPERTY_ALIGNMENT)") );
									}
								}
							}
							LastIt = It2;
							HeaderText.Logf(TEXT(";\r\n"));
						}
					}

					// Export serializer
					if( ItS->StructFlags&STRUCT_Export )
					{
						HeaderText.Logf( TEXT("%sfriend FArchive& operator<<(FArchive& Ar,%s& My%s)\r\n"), appSpc(TextIndent+4), NameLookupCPP->GetNameCPP( ItS ), ItS->GetName() );
						HeaderText.Logf( TEXT("%s{\r\n"), appSpc(TextIndent+4) );
						HeaderText.Logf( TEXT("%sreturn Ar"), appSpc(TextIndent+8) );
						for( TFieldIterator<UProperty,CLASS_IsAUProperty> It2(ItS); It2; ++It2 )
						{
							if( It2.GetStruct()==ItS && It2->ElementSize )
							{
								if( It2->ArrayDim > 1 )
								{
									for( INT i=0;i<It2->ArrayDim;i++ )
										HeaderText.Logf( TEXT(" << My%s.%s[%d]"), ItS->GetName(), It2->GetName(), i );
								}
								else
								{
									HeaderText.Logf( TEXT(" << My%s.%s"), ItS->GetName(), It2->GetName() );
								}
							}
						}
						HeaderText.Logf( TEXT(";\r\n%s}\r\n"), appSpc(TextIndent+4) );
					}

					HeaderText.Logf( TEXT("};\r\n\r\n") );
				}
			
				// Constants.
				for( TFieldIterator<UConst> ItC(Class); ItC && ItC.GetStruct()==Class; ++ItC )
				{
					FString V = ItC->Value;
					while( V.Left(1)==TEXT(" ") )
						V=V.Mid(1);
					if( V.Len()>1 && V.Left(1)==TEXT("'") && V.Right(1)==TEXT("'") )
						V = V.Mid(1,V.Len()-2);
					HeaderText.Logf( TEXT("#define UCONST_%s %s\r\n"), ItC->GetName(), *V );
				}
				if( TFieldIterator<UConst>(Class) )
					HeaderText.Logf( TEXT("\r\n") );

				// Parms struct definitions.
				TFieldIterator<UFunction> Function(Class);
				TFieldIterator<UProperty,CLASS_IsAUProperty> It(Class);
				for( Function = TFieldIterator<UFunction>(Class); Function && Function.GetStruct()==Class; ++Function )
				{
					if
					(	(Function->FunctionFlags & (FUNC_Event|FUNC_Delegate))
					&&	(!Function->GetSuperFunction()) )
					{
						HeaderText.Logf( TEXT("struct %s_event%s_Parms\r\n"), Class->GetName(), Function->GetName() );
						HeaderText.Log( TEXT("{\r\n") );
							for( It=TFieldIterator<UProperty,CLASS_IsAUProperty>(*Function); It && (It->PropertyFlags&CPF_Parm); ++It )
						{
							HeaderText.Log( TEXT("    ") );
							It->ExportCpp( HeaderText, 1, 0, 1, 0 );
							HeaderText.Log( TEXT(";\r\n") );
						}
						HeaderText.Log( TEXT("};\r\n") );
					}
				}
				
				const TCHAR* ClassCPPName		= NameLookupCPP->GetNameCPP( Class );
				const TCHAR* SuperClassCPPName	= NameLookupCPP->GetNameCPP( Class->GetSuperClass() );

				// Class definition.
				HeaderText.Logf( TEXT("class %s"), ClassCPPName );
				if( Class->GetSuperClass() )
					HeaderText.Logf( TEXT(" : public %s\r\n"), SuperClassCPPName );
				HeaderText.Logf( TEXT("{\r\npublic:\r\n") );

				// All per-object properties defined in this class.
				TFieldIterator<UProperty,CLASS_IsAUProperty> LastIt = NULL;
				for( TFieldIterator<UProperty,CLASS_IsAUProperty> It = TFieldIterator<UProperty,CLASS_IsAUProperty>(Class); It; ++It )
				{
					if( It.GetStruct()==Class && It->ElementSize && !(It->PropertyFlags&CPF_NoExport) )
					{
						HeaderText.Logf( appSpc(TextIndent+4) );
						It->ExportCpp( HeaderText, 0, 0, 0, 0 );
						if (It->IsA(UBoolProperty::StaticClass()))
						{
							if (LastIt == NULL || !LastIt->IsA(UBoolProperty::StaticClass()))
								HeaderText.Logf( TEXT(" GCC_PACK(PROPERTY_ALIGNMENT)") );
						}
						else
						{
							if ( LastIt != NULL )
							{
								if ( (LastIt->IsA(UBoolProperty::StaticClass())) ||
									(LastIt->IsA(UByteProperty::StaticClass()) && !It->IsA(UByteProperty::StaticClass()))
								)
								{
									HeaderText.Logf( TEXT(" GCC_PACK(PROPERTY_ALIGNMENT)") );
								}
							}
						}
						HeaderText.Logf( TEXT(";\r\n") );
					}
					LastIt = It;
				}

				// C++ -> UnrealScript stubs.
				for( TFieldIterator<UFunction> Function = TFieldIterator<UFunction>(Class); Function && Function.GetStruct()==Class; ++Function )
					if( Function->FunctionFlags & FUNC_Native )
						HeaderText.Logf( TEXT("    DECLARE_FUNCTION(exec%s);\r\n"), Function->GetName() );

				// UnrealScript -> C++ proxies.
				for( TFieldIterator<UFunction> Function = TFieldIterator<UFunction>(Class); Function && Function.GetStruct()==Class; ++Function )
				{
					if
					(	(Function->FunctionFlags & (FUNC_Event|FUNC_Delegate))
					&&	(!Function->GetSuperFunction()) )
					{
						// Return type.
						UProperty* Return = Function->GetReturnProperty();
						HeaderText.Log( TEXT("    ") );
						if( !Return )
							HeaderText.Log( TEXT("void") );
						else
							Return->ExportCppItem( HeaderText );

						// Function name and parms.
						INT ParmCount=0;
						if( Function->FunctionFlags & FUNC_Delegate )
							HeaderText.Logf( TEXT(" delegate%s("), Function->GetName() );
						else
							HeaderText.Logf( TEXT(" event%s("), Function->GetName() );
						for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(*Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
						{
							if( ParmCount++ )
								HeaderText.Log(TEXT(", "));
							It->ExportCpp( HeaderText, 0, 1, 1, 0 );
						}
						HeaderText.Log( TEXT(")\r\n") );

						// Function call.
						HeaderText.Log( TEXT("    {\r\n") );
						UBOOL ProbeOptimization = (Function->GetFName().GetIndex()>=NAME_PROBEMIN && Function->GetFName().GetIndex()<NAME_PROBEMAX);
						if( ParmCount || Return )
						{
							HeaderText.Logf( TEXT("        %s_event%s_Parms Parms;\r\n"), Class->GetName(), Function->GetName() );
							if( Return && Cast<UNameProperty>(Return) )
								HeaderText.Logf( TEXT("        Parms.%s=NAME_None;\r\n"), Return->GetName() );
							else if( Return && !Cast<UStrProperty>(Return) )
								HeaderText.Logf( TEXT("        Parms.%s=0;\r\n"), Return->GetName() );
						}
						if( ProbeOptimization )
							HeaderText.Logf(TEXT("        if(IsProbing(NAME_%s)) {\r\n"),Function->GetName());
						if( ParmCount || Return )
						{
							// Parms struct initialization.
							for( TFieldIterator<UProperty,CLASS_IsAUProperty> It=TFieldIterator<UProperty,CLASS_IsAUProperty>(*Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
							{
								if( It->ArrayDim>1 )
									HeaderText.Logf( TEXT("        appMemcpy(&Parms.%s,&%s,sizeof(Parms.%s));\r\n"), It->GetName(), It->GetName(), It->GetName() );
								else
									HeaderText.Logf( TEXT("        Parms.%s=%s;\r\n"), It->GetName(), It->GetName() );
							}
							if( Function->FunctionFlags & FUNC_Delegate )
								HeaderText.Logf( TEXT("        ProcessDelegate(%s_%s,&__%s__Delegate,&Parms);\r\n"), *API, Function->GetName(), Function->GetName() );
							else
								HeaderText.Logf( TEXT("        ProcessEvent(FindFunctionChecked(%s_%s),&Parms);\r\n"), *API, Function->GetName() );
						}
						else
						{
							if( Function->FunctionFlags & FUNC_Delegate )
								HeaderText.Logf( TEXT("        ProcessDelegate(%s_%s,&__%s__Delegate,NULL);\r\n"), *API, Function->GetName(), Function->GetName() );
							else
								HeaderText.Logf( TEXT("        ProcessEvent(FindFunctionChecked(%s_%s),NULL);\r\n"), *API, Function->GetName() );
						}
						if( ProbeOptimization )
							HeaderText.Logf(TEXT("        }\r\n"));

						// Out parm copying.
						for( TFieldIterator<UProperty,CLASS_IsAUProperty> It=TFieldIterator<UProperty,CLASS_IsAUProperty>(*Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
						{
							if( It->PropertyFlags & CPF_OutParm )
							{
								if( It->ArrayDim>1 )
									HeaderText.Logf( TEXT("        appMemcpy(&%s,&Parms.%s,sizeof(%s));\r\n"), It->GetName(), It->GetName(), It->GetName() );
								else
									HeaderText.Logf( TEXT("        %s=Parms.%s;\r\n"), It->GetName(), It->GetName() );
							}
						}

						// Return value.
						if( Return )
							HeaderText.Logf( TEXT("        return Parms.%s;\r\n"), Return->GetName() );
						HeaderText.Log( TEXT("    }\r\n") );
					}
				}

				// Code.
				HeaderText.Logf( TEXT("    DECLARE_CLASS(%s,"), ClassCPPName );
				HeaderText.Logf( TEXT("%s,0"), SuperClassCPPName );
				if( Class->ClassFlags & CLASS_Transient      )
					HeaderText.Log( TEXT("|CLASS_Transient") );
				if( Class->ClassFlags & CLASS_Config )
					HeaderText.Log( TEXT("|CLASS_Config") );
				if( Class->ClassFlags & CLASS_NativeReplication )
					HeaderText.Log( TEXT("|CLASS_NativeReplication") );
				HeaderText.Logf( TEXT(",%s)\r\n"), Class->GetOuter()->GetName() );
				if(Class->ClassWithin != Class->GetSuperClass()->ClassWithin)
					HeaderText.Logf(TEXT("    DECLARE_WITHIN(%s)\r\n"), NameLookupCPP->GetNameCPP( Class->ClassWithin ) );
				FString Filename = GEditor->EditPackagesInPath * Class->GetOuter()->GetName() * TEXT("Inc") * ClassCPPName + TEXT(".h");
				if( Class->CppText )
					HeaderText.Log( *Class->CppText->Text );
				else
				if( GFileManager->FileSize(*Filename) > 0 )
					HeaderText.Logf( TEXT("    #include \"%s.h\"\r\n"), ClassCPPName );
				else
					HeaderText.Logf( TEXT("    NO_DEFAULT_CONSTRUCTOR(%s)\r\n"), ClassCPPName );

				// End of class.
				HeaderText.Logf( TEXT("};\r\n") );

				// End.
				HeaderText.Logf( TEXT("\r\n") );
			}
			else if(ExportedParent && !MasterHeader)
				appErrorf(TEXT("Encountered natively exported class with inheritance dependency on parent in another header file(%s:%s depending on %s:%s)."),Class->GetName(),*Class->ClassHeaderFilename,Class->GetSuperClass()->GetName(),*Class->GetSuperClass()->ClassHeaderFilename);
		}

		// Export all child classes that are tagged for export.
		for( INT i=0; i<Classes.Num(); i++ )
			if( Classes(i)->GetSuperClass()==Class )
				ExportClassHeader(Classes(i),Class->GetFlags() & RF_TagExp);
	}

public:

	// Constructor.

	FNativeClassHeaderGenerator(const TCHAR* InClassHeaderFilename,UObject* InPackage,UBOOL InMasterHeader):
		ClassHeaderFilename(InClassHeaderFilename),
		Package(InPackage),
		API(FString(InPackage->GetName()).Caps()),
		MasterHeader(InMasterHeader)
	{
		// Tag native classes in this package for export.
		INT ClassCount=0;
		for( INT i=0; i<FName::GetMaxNames(); i++ )
			if( FName::GetEntry(i) )
				FName::GetEntry(i)->Flags &= ~RF_TagExp;
		for( TObjectIterator<UClass> It; It; ++It )
			It->ClearFlags( RF_TagImp | RF_TagExp );
		for( TObjectIterator<UClass> It=TObjectIterator<UClass>(); It; ++It )
			if( It->GetOuter()==Package && It->ScriptText && (It->GetFlags()&RF_Native) && !(It->ClassFlags&CLASS_NoExport) && It->ClassHeaderFilename == ClassHeaderFilename )
				ClassCount++, It->SetFlags(RF_TagExp);

		if(!ClassCount)
			return;

		// Iterate over all classes and sort them by name.
		for( TObjectIterator<UClass> It; It; ++It )
			Classes.AddItem( *It );
		Sort<USE_COMPARE_POINTER(UClass,UMakeCommandlet)>( &Classes(0), Classes.Num() );

		debugf( TEXT("Autogenerating C++ header: %s"), *ClassHeaderFilename );

		for( INT i=0; i<Classes.Num(); i++ )
		{
			UClass* Class=Classes(i);
			if((Class->GetFlags() & RF_TagExp) && (Class->GetFlags() & RF_Native))
				for(TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function)
					if( (Function->FunctionFlags & (FUNC_Event|FUNC_Delegate)) && !Function->GetSuperFunction() )
						Function->GetFName().SetFlags(RF_TagExp);
		}

		HeaderText.Logf(
			TEXT("/*===========================================================================\r\n")
			TEXT("    C++ class definitions exported from UnrealScript.\r\n")
			TEXT("    This is automatically generated by the tools.\r\n")
			TEXT("    DO NOT modify this manually! Edit the corresponding .uc files instead!\r\n")
			TEXT("===========================================================================*/\r\n")
			TEXT("#if SUPPORTS_PRAGMA_PACK\r\n")
			TEXT("#pragma pack (push,%i)\r\n")
			TEXT("#endif\r\n")
			TEXT("\r\n")
			TEXT("\r\n"),
			(BYTE)PROPERTY_ALIGNMENT,
			*API,
			*API
			);

		HeaderText.Logf(
			TEXT("#ifndef NAMES_ONLY\r\n")
			TEXT("#define AUTOGENERATE_NAME(name) extern FName %s_##name;\r\n")
			TEXT("#define AUTOGENERATE_FUNCTION(cls,idx,name)\r\n")
			TEXT("#endif\r\n")
			TEXT("\r\n"),
			*API
			);

		// Autogenerate names (alphabetically sorted).
		TArray<FString> Names;
		for( INT i=0; i<FName::GetMaxNames(); i++ )
			if( FName::GetEntry(i) && (FName::GetEntry(i)->Flags & RF_TagExp) )
				new(Names) FString(*FName((EName)(i)));
		Sort<USE_COMPARE_CONSTREF(FString,UMakeCommandlet)>( &Names(0), Names.Num() );
		for( INT i=0; i<Names.Num(); i++ )
			HeaderText.Logf( TEXT("AUTOGENERATE_NAME(%s)\r\n"), *Names(i) );
	
		for( INT i=0; i<FName::GetMaxNames(); i++ )
			if( FName::GetEntry(i) )
				FName::GetEntry(i)->Flags &= ~RF_TagExp;

		HeaderText.Logf( TEXT("\r\n#ifndef NAMES_ONLY\r\n\r\n") );

		ExportClassHeader(UObject::StaticClass());

		HeaderText.Logf( TEXT("#endif\r\n") );
		HeaderText.Logf( TEXT("\r\n") );

		for( INT i=0; i<Classes.Num(); i++ )
		{
			UClass* Class = Classes(i);
			if( Class->GetFlags() & RF_TagExp )
				for( TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
					if( Function->FunctionFlags & FUNC_Native )
						HeaderText.Logf( TEXT("AUTOGENERATE_FUNCTION(%s,%i,exec%s);\r\n"), NameLookupCPP->GetNameCPP( Class ), Function->iNative ? Function->iNative : -1, Function->GetName() );
		}

		HeaderText.Logf( TEXT("\r\n") );
		HeaderText.Logf( TEXT("#ifndef NAMES_ONLY\r\n") );
		HeaderText.Logf( TEXT("#undef AUTOGENERATE_NAME\r\n") );
		HeaderText.Logf( TEXT("#undef AUTOGENERATE_FUNCTION\r\n") );
		HeaderText.Logf( TEXT("#endif\r\n") );

		HeaderText.Logf( TEXT("\r\n") );
		HeaderText.Logf( TEXT("#if SUPPORTS_PRAGMA_PACK\r\n") );
		HeaderText.Logf( TEXT("#pragma pack (pop)\r\n") );
		HeaderText.Logf( TEXT("#endif\r\n") );

		HeaderText.Logf( TEXT("\r\n") );
	
		if( MasterHeader )
		{
			HeaderText.Logf( TEXT("#ifdef STATIC_LINKING_MOJO\r\n"));

			HeaderText.Logf( TEXT("#ifndef %s_NATIVE_DEFS\r\n"), *API);
			HeaderText.Logf( TEXT("#define %s_NATIVE_DEFS\r\n"), *API);
			HeaderText.Logf( TEXT("\r\n") );

			for( INT i=0; i<Classes.Num(); i++ )
				if( Classes(i)->HasNativesToExport( Package ) )
					HeaderText.Logf( TEXT("DECLARE_NATIVE_TYPE(%s,%s);\r\n"), Package->GetName(), NameLookupCPP->GetNameCPP(Classes(i)) );
			HeaderText.Logf( TEXT("\r\n") );

			HeaderText.Logf( TEXT("#define AUTO_INITIALIZE_REGISTRANTS_%s \\\r\n"), *API );
			for( INT i=0; i<Classes.Num(); i++ )
			{
				UClass* Class = Classes(i);
				if( (Class->GetOuter() == Package) && (Class->GetFlags() & RF_Native) )
				{
					HeaderText.Logf( TEXT("\t%s::StaticClass(); \\\r\n"), NameLookupCPP->GetNameCPP( Class ) );
					if( Class->HasNativesToExport( Package ) )
					{
						for( TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
						{
							if( Function->FunctionFlags & FUNC_Native )
							{
								HeaderText.Logf( TEXT("\tGNativeLookupFuncs[Lookup++] = &Find%s%sNative; \\\r\n"), Package->GetName(), NameLookupCPP->GetNameCPP(Class) );
								break;
							}
						}
					}
				}
			}
			HeaderText.Logf( TEXT("\r\n") );

			HeaderText.Logf( TEXT("#endif // %s_NATIVE_DEFS\r\n"), *API ); // #endif // s_NATIVE_DEFS
			HeaderText.Logf( TEXT("\r\n") );

			HeaderText.Logf( TEXT("#ifdef NATIVES_ONLY\r\n") );
			for( INT i=0; i<Classes.Num(); i++ )
			{
				UClass* Class = Classes(i);
				if( Class->HasNativesToExport( Package ) )
				{
					INT NumNatives = 0;
					for( TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
					{
						if( Function->FunctionFlags & FUNC_Native )
						{
							NumNatives++;
							break;
						}
					}

					if( NumNatives )
					{
						HeaderText.Logf( TEXT("NATIVE_INFO(%s) G%s%sNatives[] = \r\n"), NameLookupCPP->GetNameCPP(Class), Package->GetName(), NameLookupCPP->GetNameCPP(Class) );
						HeaderText.Logf( TEXT("{ \r\n"));
						for( TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
						{
							if( Function->FunctionFlags & FUNC_Native )
								HeaderText.Logf( TEXT("\tMAP_NATIVE(%s,exec%s)\r\n"), NameLookupCPP->GetNameCPP(Class), Function->GetName() );
						}
						HeaderText.Logf( TEXT("\t{NULL,NULL}\r\n") );
						HeaderText.Logf( TEXT("};\r\n") );
						HeaderText.Logf( TEXT("IMPLEMENT_NATIVE_HANDLER(%s,%s);\r\n"), Package->GetName(), NameLookupCPP->GetNameCPP(Class) );
						HeaderText.Logf( TEXT("\r\n") );
					}
				}
			}
			HeaderText.Logf( TEXT("#endif // NATIVES_ONLY\r\n"), *API ); // #endif // NAMES_ONLY
			HeaderText.Logf( TEXT("#endif // STATIC_LINKING_MOJO\r\n"), *API ); // #endif // STATIC_LINKING_MOJO

			// Generate code to automatically verify class offsets and size.
			HeaderText.Logf( TEXT("\r\n#ifdef VERIFY_CLASS_SIZES\r\n") ); // #ifdef VERIFY_CLASS_SIZES
			for( INT i=0; i<Classes.Num(); i++ )
			{		
				UClass* Class = Classes(i);
				if( (Class->GetFlags() & RF_Native) && (Class->GetOuter() == Package) )
				{	
					// Only verify property offsets for noexport classes to avoid running into compiler limitations.
					if( Class->ClassFlags & CLASS_NoExport ) 
					{
						// Iterate over all properties that are new in this class.
						for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Class); It && It.GetStruct()==Class; ++It )
						{
							// We can't verify bools due to the packing and we skip noexport variables so you can e.g. declare a placeholder for the virtual function table in script.
							if( It->ElementSize && !It->IsA(UBoolProperty::StaticClass()) && !(It->PropertyFlags & CPF_NoExport) )
							{
								HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),Class->GetPrefixCPP(),Class->GetName(),It->GetName());				
							}
						}
					}
					HeaderText.Logf( TEXT("VERIFY_CLASS_SIZE_NODIE(%s)\r\n"), NameLookupCPP->GetNameCPP(Class) );
				}
			}
			HeaderText.Logf( TEXT("#endif // VERIFY_CLASS_SIZES\r\n") ); // #endif // VERIFY_CLASS_SIZES
		}

		// Save the header file.

		FString	HeaderPath = GEditor->EditPackagesInPath * Package->GetName() * TEXT("Inc") * Package->GetName() + ClassHeaderFilename + TEXT("Classes.h"),
				OriginalHeader;
		if(!appLoadFileToString(OriginalHeader,*HeaderPath) || appStrcmp(*OriginalHeader,*HeaderText))
		{
			if(OriginalHeader.Len() && ParseParam(appCmdLine(),TEXT("silent")) && !ParseParam(appCmdLine(),TEXT("auto")))
				warnf(NAME_Error,TEXT("Cannot export %s while in silent mode."),*HeaderPath);
			else if(!OriginalHeader.Len() || ParseParam(appCmdLine(), TEXT("auto")) || GWarn->YesNof(*LocalizeQuery(TEXT("Overwrite"),TEXT("Core")),*HeaderPath))
			{
				if( GFileManager->IsReadOnly( *HeaderPath ) )
				{
					if( ParseParam(appCmdLine(), TEXT("auto")) || GWarn->YesNof(*LocalizeQuery(TEXT("CheckoutPerforce"),TEXT("Core")), *HeaderPath) )
					{
						INT Code;
						void* Handle = appCreateProc( TEXT("p4"), *FString::Printf(TEXT("edit %s"), *HeaderPath) );
						while( !appGetProcReturnCode( Handle, &Code ) )
							appSleep(1);
					}
				}
				debugf(TEXT("Exported updated C++ header: %s"), *HeaderPath);
				if(!appSaveStringToFile(*HeaderText,*HeaderPath))
					warnf(*LocalizeError(TEXT("ExportOpen"),TEXT("Core")),*UObject::StaticClass()->GetFullName(),*HeaderPath);
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	UMakeCommandlet.
-----------------------------------------------------------------------------*/

void UMakeCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;

}
INT UMakeCommandlet::Main( const TCHAR* Parms )
{
	GIsUCCMake		= 1;

	// Create the editor class.
	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
	GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();
	GIsRequestingExit			= 1; // Causes ctrl-c to immediately exit.
	NameLookupCPP				= new FNameLookupCPP();

	// Load classes for editing.
	UClassFactoryUC* ClassFactory = new UClassFactoryUC;
	for( INT PackageIndex=0; PackageIndex<GEditor->EditPackages.Num(); PackageIndex++ )
	{
		// Try to load class.
		const TCHAR* Pkg = *GEditor->EditPackages( PackageIndex );
		FString Filename = FString::Printf(TEXT("%s") PATH_SEPARATOR TEXT("%s.u"), *GEditor->EditPackagesOutPath, Pkg );
		GWarn->Log( NAME_Heading, FString::Printf(TEXT("%s - %s"),Pkg,ParseParam(appCmdLine(), TEXT("DEBUG"))? TEXT("Debug") : TEXT("Release"))); //DEBUGGER
		
		// Check whether this package needs to be recompiled because a class is newer than the package.
		//@warning: This e.g. won't detect changes to resources being compiled into packages.
		if(GFileManager->FileSize(*Filename) > 0)
		{
			DOUBLE			PackageFileSeconds	= GFileManager->GetFileAgeSeconds(*Filename);
			FString			ClassesWildcard		= GEditor->EditPackagesInPath * Pkg * TEXT("Classes") * TEXT("*.uc");
			TArray<FString> ClassesFiles;
			
			GFileManager->FindFiles( ClassesFiles, *ClassesWildcard, 1, 0 );
			
			// Iterate over all .uc files in the package's Classes folder.
			for( INT ClassIndex=0; ClassIndex<ClassesFiles.Num(); ClassIndex++ )
			{
				FString		ClassName			= GEditor->EditPackagesInPath * Pkg * TEXT("Classes") * ClassesFiles(ClassIndex);
				DOUBLE		ClassFileSeconds	= GFileManager->GetFileAgeSeconds(*ClassName);

				// Delete package (and subsequent ones) if there is a class newer than the package. This relies on package not already bound which
				// is taken care of by setting GIsUCCMake to true.
				if( ClassFileSeconds <= PackageFileSeconds )
				{
					// Delete package and all the ones following in EditPackages. This is required in certain cases so we rather play safe.
					for( INT DeleteIndex=PackageIndex; DeleteIndex<GEditor->EditPackages.Num(); DeleteIndex++ )
					{
						FString Filename = FString::Printf(TEXT("%s") PATH_SEPARATOR TEXT("%s.u"), *GEditor->EditPackagesOutPath, *GEditor->EditPackages( DeleteIndex ) );
						GFileManager->Delete( *Filename, 0, 0 );
					}
					warnf(TEXT("Class %s changed, recompiling"), *ClassName);
					break;
				}
			}
		}
		
		if( !LoadPackage( NULL, *Filename, LOAD_NoWarn ) )
		{
			// Create package.
			GWarn->Log( TEXT("Analyzing...") );
			UPackage* Package = CreatePackage( NULL, Pkg );

			// Try reading from package's .ini file.
			Package->PackageFlags &= ~(PKG_AllowDownload|PKG_ClientOptional|PKG_ServerSideOnly);
			FString IniName = GEditor->EditPackagesInPath * Pkg * TEXT("Classes") * Pkg + TEXT(".upkg");
			UBOOL B=0;
			if( GConfig->GetBool(TEXT("Flags"), TEXT("AllowDownload"), B, *IniName) && B )
				Package->PackageFlags |= PKG_AllowDownload;
			if( GConfig->GetBool(TEXT("Flags"), TEXT("ClientOptional"), B, *IniName) && B )
				Package->PackageFlags |= PKG_ClientOptional;
			if( GConfig->GetBool(TEXT("Flags"), TEXT("ServerSideOnly"), B, *IniName) && B )
				Package->PackageFlags |= PKG_ServerSideOnly;

			// Rebuild the class from its directory.
			FString Spec = GEditor->EditPackagesInPath * Pkg * TEXT("Classes") * TEXT("*.uc");
			TArray<FString> Files;
			GFileManager->FindFiles( Files, *Spec, 1, 0 );
			if( Files.Num() == 0 )
				appErrorf( TEXT("Can't find files matching %s"), *Spec );

			// Make script compilation deterministic by sorting .uc files by name.
			Sort<USE_COMPARE_CONSTREF(FString,UMakeCommandlet)>( &Files(0), Files.Num() );

			for( INT i=0; i<Files.Num(); i++ )
			{
				// Import class.
				FString Filename  = GEditor->EditPackagesInPath * Pkg * TEXT("Classes") * Files(i);
				FString ClassName = Files(i).LeftChop(3);
				ImportObject<UClass>( GEditor->Level, Package, *ClassName, RF_Public|RF_Standalone, *Filename, NULL, ClassFactory );
			}

			// Verify that all script declared superclasses exist.
			for( TObjectIterator<UClass> ItC; ItC; ++ItC )
				if( ItC->ScriptText && ItC->GetSuperClass() )
					if( !ItC->GetSuperClass()->ScriptText )
						appErrorf( TEXT("Superclass %s of class %s not found"), ItC->GetSuperClass()->GetName(), ItC->GetName() );

			// Bootstrap-recompile changed scripts.
			GEditor->Bootstrapping = 1;
			GEditor->ParentContext = Package;
			UBOOL Success = GEditor->MakeScripts( NULL, GWarn, 0, 1, 1 );
			GEditor->ParentContext = NULL;
			GEditor->Bootstrapping = 0;

            if( !Success )
            {
                warnf ( TEXT("Compile aborted due to errors.") );
                break;
            }

			// Export native class definitions to package header files.
			TArray<FString>	ClassHeaderFilenames;
			new(ClassHeaderFilenames) FString(TEXT(""));
			for( TObjectIterator<UClass> It; It; ++It )
				if( It->GetOuter()==Package && It->ScriptText && (It->GetFlags()&RF_Native) && !(It->ClassFlags&CLASS_NoExport) )
					if(ClassHeaderFilenames.FindItemIndex(It->ClassHeaderFilename) == INDEX_NONE)
						new(ClassHeaderFilenames) FString(It->ClassHeaderFilename);
			FNativeClassHeaderGenerator(*ClassHeaderFilenames(0),Package,1);
			for(UINT HeaderIndex = 1;HeaderIndex < (UINT)ClassHeaderFilenames.Num();HeaderIndex++)
				FNativeClassHeaderGenerator(*ClassHeaderFilenames(HeaderIndex),Package,0);

			// Save package.
			ULinkerLoad* Conform = NULL;
			
			if( !ParseParam(appCmdLine(),TEXT("NOCONFORM")) )
			{
				BeginLoad();
				Conform = UObject::GetPackageLinker( CreatePackage(NULL,*(US+Pkg+TEXT("_OLD"))), *(FString(TEXT("..")) * TEXT("GUIRes") * Pkg + TEXT(".u")), LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
				EndLoad();
				if( Conform )
					debugf( TEXT("Conforming: %s"), Pkg );
			}

			SavePackage( Package, NULL, RF_Standalone, *Filename, GError, Conform );
		}
	}

	delete NameLookupCPP;
	GIsRequestingExit=1;
	return 0;

}
IMPLEMENT_CLASS(UMakeCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

