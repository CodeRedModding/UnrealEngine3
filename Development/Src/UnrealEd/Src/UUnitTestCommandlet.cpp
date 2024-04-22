// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

IMPLEMENT_CLASS(UUnitTestCommandlet)

/** 
 * Commandlet designed to allow for the running of unit tests 
 *
 * @param	Params	Parameters sent to the commandlet
 */
INT UUnitTestCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine( *Params, Tokens, Switches );

	TMap<FString, FUnitTestExecutionInfo> OutputMap;
	
	// Check to see if the user provided the "-ALL" switch. If so, attempt to run all valid unit tests
	if ( Switches.Num() > 0 )
	{
		if ( Switches(0).ToUpper() == TEXT("ALL") )
		{
			FUnitTestFramework::GetInstance().RunAllValidTests( OutputMap );
		}
		else
		{
			SET_WARN_COLOR(COLOR_RED);
			GWarn->Logf( NAME_Error, TEXT("Invalid switch passed to commandlet.") );
			CLEAR_WARN_COLOR();
		}
	}
	// Otherwise, make sure the user specified at least one unit test name to run
	else if ( Tokens.Num() > 0 )
	{
		for ( TArray<FString>::TConstIterator TestIter( Tokens ); TestIter; ++TestIter )
		{
			if ( FUnitTestFramework::GetInstance().ContainsTest( *TestIter ) )
			{
				FUnitTestExecutionInfo& CurExecutionInfo = OutputMap.Set( *TestIter, FUnitTestExecutionInfo() );
				FUnitTestFramework::GetInstance().RunTestByName( *TestIter, CurExecutionInfo );
			}
			else
			{
				SET_WARN_COLOR(COLOR_RED);
				GWarn->Logf( NAME_Error, *FString::Printf( LocalizeSecure( LocalizeUnrealEd("UnitTest_TestNotFound"), **TestIter ) ) );
				CLEAR_WARN_COLOR();
			}
		}
	}
	else
	{
		SET_WARN_COLOR(COLOR_RED);
		GWarn->Logf( NAME_Error, TEXT("No parameters passed to commandlet.") );
		CLEAR_WARN_COLOR();
	}

	// Output the results of running the unit tests to the screen
	FUnitTestFramework::DumpUnitTestExecutionInfoToContext( GWarn, OutputMap );
	
	return 0;
}
