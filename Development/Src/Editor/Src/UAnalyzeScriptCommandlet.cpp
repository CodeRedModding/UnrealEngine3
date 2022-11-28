
#include "EditorPrivate.h"

void UAnalyzeScriptCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}

UFunction* UAnalyzeScriptCommandlet::FindSuperFunction(UFunction *evilFunc)
{
	check(evilFunc != NULL);
	UFunction *func = NULL;
	UState *searchState = Cast<UState>(evilFunc->GetOuter());
	while (searchState != NULL &&
			func == NULL)
	{
		func = searchState->FuncMap.FindRef(evilFunc->GetFName());
		if (func == evilFunc)
		{
			func = NULL;
		}
		searchState = searchState->GetSuperState();
	}
	if (func == NULL)
	{
		UClass *searchClass = NULL;
		UObject *testOuter = evilFunc->GetOuter();
		while (searchClass == NULL &&
				testOuter != NULL)
		{
			searchClass = Cast<UClass>(testOuter);
			testOuter = testOuter->GetOuter();
		}
		while (searchClass != NULL &&
				func == NULL)
		{
			func = searchClass->FuncMap.FindRef(evilFunc->GetFName());
			if (func == evilFunc)
			{
				func = NULL;
			}
			searchClass = searchClass->GetSuperClass();
		}
	}
	return func;
}

INT UAnalyzeScriptCommandlet::Main(const TCHAR *Parms)
{
	UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound = 0;
	GEditor->InitEditor();
	// load all packages in EditPackages
	debugf(TEXT("Loading edit packages..."));
	TArray<FString> editPkgs;
	TMultiMap<FString,FString> *section = GConfig->GetSectionPrivate(TEXT("Editor.EditorEngine"),0,1,GEngineIni);
	check(section != NULL && "Failed to find editpackages, did you specify the correct ini?");
	section->MultiFind(FString(TEXT("EditPackages")),editPkgs);
	for (INT idx = 0; idx < editPkgs.Num(); idx++)
	{
		LoadPackage(NULL,*editPkgs(idx),LOAD_NoWarn);
	}
	debugf(TEXT("Analyzing functions..."));
	// iterate through all functions
	TArray<UFunction*>	unrefFuncs;
	for (TObjectIterator<UFunction> funcIt; funcIt != NULL; ++funcIt)
	{
		// add to the unref'd
		UFunction *func = *funcIt;
		if (func != NULL &&
			!(func->FunctionFlags & FUNC_Final))
		{
			unrefFuncs.AddItem(func);
		}
	}
	// iterate through all unref'd and move to the ref'd
	TArray<UFunction*> refFuncs;
	for (INT idx = 0; idx < unrefFuncs.Num(); idx++)
	{
		UFunction *testFunc = unrefFuncs(idx)->GetSuperFunction();
		// try a manual lookup of the super function
		if (testFunc == NULL)
		{
			testFunc = FindSuperFunction(unrefFuncs(idx));
		}
		if (testFunc != NULL)
		{
			// remove the func that is ref'd
			INT findIdx;
			if (unrefFuncs.FindItem(testFunc,findIdx))
			{
				// add to the ref'd list
				refFuncs.AddUniqueItem(testFunc);
			}
			// and remove this function
			refFuncs.AddUniqueItem(unrefFuncs(idx));
		}
	}
	// remove all functions that are ref'd by other funcs
	INT removeCnt = 0;
	while (refFuncs.Num())
	{
		UFunction *func = refFuncs.Pop();
		INT idx;
		if (unrefFuncs.FindItem(func,idx))
		{
			unrefFuncs.Remove(idx,1);
			removeCnt++;
		}
	}
	debugf(TEXT("Removed %d functions"),
			removeCnt);
	// remove all functions that are only declared, not defined, except for native
	// and filter native replicated funcs
	INT filterCnt = 0;
	for (INT idx = 0; idx < unrefFuncs.Num(); idx++)
	{
		UFunction *func = unrefFuncs(idx);
		if ((!(func->FunctionFlags & FUNC_Native) &&
				!(func->FunctionFlags & FUNC_Defined)) ||
			(func->FunctionFlags & FUNC_Native) &&
			(func->FunctionFlags & FUNC_Net))
		{
			unrefFuncs.Remove(idx--,1);
			filterCnt++;
		}
	}
	debugf(TEXT("Filtered %d functions"),
			filterCnt);
	debugf(TEXT("Sorting by class..."));
	// build a map of funcs per class
	TMap<UClass*,TArray<UFunction*> > funcClassMap;
	for (INT idx = 0; idx < unrefFuncs.Num(); idx++)
	{
		UFunction *func = unrefFuncs(idx);
		UObject *testOuter = func->GetOuter();
		UClass *outerClass = NULL;
		while (testOuter != NULL &&
				outerClass == NULL)
		{
			outerClass = Cast<UClass>(testOuter);
			if (outerClass == NULL)
			{
				testOuter = testOuter->GetOuter();
			}
		}
		if (outerClass != NULL)
		{
			TArray<UFunction*> *funcList = funcClassMap.Find(outerClass);
			if (funcList == NULL)
			{
				funcList = new TArray<UFunction*>();
				funcClassMap.Set(outerClass,*funcList);
				delete funcList;
				// refind it since we just copied it for the array, blargh
				funcList = funcClassMap.Find(outerClass);
			}
			funcList->AddItem(func);
		}
		else
		{
			debugf(TEXT("Invalid outer for function %s"),
					func->GetName());
		}
	}
	// and dump them
	for (TMap<UClass*,TArray<UFunction*> >::TIterator It(funcClassMap); It; ++It)
	{
		TArray<UFunction*> *funcList = &(It.Value());
		debugf(TEXT("Class %s:"),
				It.Key()->GetName());
		for (INT idx = 0; idx < funcList->Num(); idx++)
		{
			UFunction *func = (*funcList)(idx);
			debugf(TEXT("- %s"),
					func->GetPathName());
		}
		debugf(TEXT(""));
	}
	debugf(TEXT("All done"));
	GIsRequestingExit = 1;
	return 0;
}
		
IMPLEMENT_CLASS(UAnalyzeScriptCommandlet);
