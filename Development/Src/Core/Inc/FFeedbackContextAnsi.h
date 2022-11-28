/*=============================================================================
	FFeedbackContextAnsi.h: Unreal Ansi user interface interaction.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	FFeedbackContextAnsi.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextAnsi : public FFeedbackContext
{
public:
	// Variables.
	INT					SlowTaskCount;
	FContextSupplier*	Context;
	FOutputDevice*		AuxOut;

	// Local functions.
	void LocalPrint( const TCHAR* Str )
	{
#if UNICODE
		wprintf(TEXT("%s"),Str);
#else
		printf(TEXT("%s"),Str);
#endif
	}

	// Constructor.
	FFeedbackContextAnsi()
	: FFeedbackContext()
	, SlowTaskCount( 0 )
	, Context( NULL )
	, AuxOut( NULL )
	{}
	void Serialize( const TCHAR* V, EName Event )
	{
		TCHAR Temp[1024]=TEXT("");
		if( Event==NAME_Title )
		{
			return;
		}
		else if( Event==NAME_Heading )
		{
			appSprintf( Temp, TEXT("--------------------%s--------------------"), (TCHAR*)V );
			V = Temp;
		}
		else if( Event==NAME_SubHeading )
		{
			appSprintf( Temp, TEXT("%s..."), (TCHAR*)V );
			V = Temp;
		}
		else if( Event==NAME_Error || Event==NAME_Warning || Event==NAME_ExecWarning || Event==NAME_ScriptWarning )
		{
			if( Context )
			{
				appSprintf( Temp, TEXT("%s : %s, %s"), *Context->GetContext(), *FName(Event), (TCHAR*)V );
				V = Temp;
			}
			if(Event == NAME_Error || TreatWarningsAsErrors)
				ErrorCount++;
			else
				WarningCount++;
		}
		else if( Event==NAME_Progress )
		{
			appSprintf( Temp, TEXT("%s"), (TCHAR*)V );
			V = Temp;
			LocalPrint( V );
			LocalPrint( TEXT("\r") );
			fflush( stdout );
			return;
		}
		LocalPrint( V );
		LocalPrint( TEXT("\n") );
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Event );
		if( AuxOut )
			AuxOut->Serialize( V, Event );
		fflush( stdout );
	}
	VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		if( (GIsClient || GIsEditor) )
		{
			LocalPrint( TempStr );
			LocalPrint( TEXT(" (Y/N): ") );
			if( ParseParam(appCmdLine(),TEXT("Silent")) )
			{
				LocalPrint( TEXT("Y") );
				return 1;
			}
			else
			{
				char InputText[256];
				fgets( InputText, sizeof(InputText), stdin );
				return (InputText[0]=='Y' || InputText[0]=='y');
			}
		}
		return 1;
	}
	void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )
	{
		GIsSlowTask = ++SlowTaskCount>0;
	}
	void EndSlowTask()
	{
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
	}
	VARARG_BODY( UBOOL VARARGS, StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		if( GIsSlowTask )
		{
			//!!
		}
		return 1;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		Context = InSupplier;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

