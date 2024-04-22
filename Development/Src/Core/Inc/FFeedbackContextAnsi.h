/*=============================================================================
	FFeedbackContextAnsi.h: Unreal Ansi user interface interaction.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FFEEDBACKCONTEXTANSI_H__
#define __FFEEDBACKCONTEXTANSI_H__

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
#if PS3 || PLATFORM_MACOSX
		printf("%s", TCHAR_TO_ANSI(Str));
#else
		wprintf(Str);
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
		TCHAR Temp[MAX_SPRINTF]=TEXT("");
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
			if( TreatWarningsAsErrors && Event==NAME_Warning )
			{
				Event = NAME_Error;
			}

			if( Context )
			{
				appSprintf( Temp, TEXT("%s : %s, %s"), *Context->GetContext(), *FName(Event).ToString(), (TCHAR*)V );
			}
			else
			{
				appSprintf( Temp, TEXT("%s, %s"), *FName(Event).ToString(), (TCHAR*)V );
			}
			V = Temp;

			// Only store off the message if running a commandlet.
			if ( GIsUCC )
			{
				if(Event == NAME_Error)
				{
					Errors.AddItem(FString(V));
				}
				else
				{
					Warnings.AddItem(FString(V));
				}
			}
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
#if PLATFORM_UNIX
		if (Event == NAME_Color)
		{
			if (appStricmp(V, TEXT("")) == 0)
			{
				LocalPrint(TEXT("\033[0m"));
			}
			else
			{
				LocalPrint(TEXT("\033[0;32m"));
			}
		}
		else
		{
			LocalPrint(V);
			LocalPrint( TEXT("\n") );
		}
#else
		LocalPrint( V );
		LocalPrint( TEXT("\n") );
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Event );
#endif
		if( AuxOut )
			AuxOut->Serialize( V, Event );
		fflush( stdout );
	}
	VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
		if( (GIsClient || GIsEditor) )
		{
			LocalPrint( TempStr );
			LocalPrint( TEXT(" (Y/N): ") );
			if( ( GIsSilent == TRUE ) || ( GIsUnattended == TRUE ) )
			{
				LocalPrint( TEXT("Y") );
				return TRUE;
			}
			else
			{
				char InputText[256];
				fgets( InputText, sizeof(InputText), stdin );
				return (InputText[0]=='Y' || InputText[0]=='y');
			}
		}
		return TRUE;
	}
	void BeginSlowTask( const TCHAR* Task, UBOOL ShowProgressDialog, UBOOL bShowCancelButton=FALSE )
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
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
		if( GIsSlowTask )
		{
			//!!
		}
		return TRUE;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		Context = InSupplier;
	}
};

#endif
