/*=============================================================================
	FFeedbackContextWindows.h: Unreal Windows user interface interaction.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FFEEDBACKCONTEXTWINDOWS_H__
#define __FFEEDBACKCONTEXTWINDOWS_H__

/**
 * Feedback context implementation for windows.
 */
class FFeedbackContextWindows : public FFeedbackContext
{
	/** Context information for warning and error messages */
	FContextSupplier*	Context;

public:
	// Variables.
	INT					SlowTaskCount;

	// Constructor.
	FFeedbackContextWindows()
	: FFeedbackContext()
	, SlowTaskCount( 0 )
	, Context( NULL )
	{}
	void Serialize( const TCHAR* V, EName Event )
	{
		TCHAR Temp[MAX_SPRINTF]=TEXT("");
		// if we set the color for warnings or errors, then reset at the end of the function
		// note, we have to set the colors directly without using the standard SET_WARN_COLOR macro
		UBOOL bNeedToResetColor = FALSE;

		if( Event==NAME_UserPrompt && (GIsClient || GIsEditor) )
		{
			::MessageBox( NULL, V, (TCHAR *)*LocalizeError("Warning",TEXT("Core")), MB_OK|MB_TASKMODAL );
		}
		else if( Event==NAME_Title )
		{
			return;
		}
		else if( Event==NAME_Heading )
		{
			Serialize(COLOR_WHITE, NAME_Color); bNeedToResetColor = TRUE;
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

			if(Event == NAME_Error)
			{
				Serialize(COLOR_RED, NAME_Color); bNeedToResetColor = TRUE;
				// Only store off the message if running a commandlet.
				if ( GIsUCC )
				{
					Errors.AddItem(FString(V));
				}
			}
			else
			{
				Serialize(COLOR_YELLOW, NAME_Color); bNeedToResetColor = TRUE;
				// Only store off the message if running a commandlet.
				if ( GIsUCC )
				{
					Warnings.AddItem(FString(V));
				}
			}
		}

		if( GLogConsole && GIsUCC )
			GLogConsole->Serialize( V, Event == NAME_Color ? NAME_Color : NAME_None );
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Event );

		if (bNeedToResetColor)
		{
			Serialize(COLOR_NONE, NAME_Color);
		}
	}
	VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
		if( ( GIsClient || GIsEditor ) && ( ( GIsSilent != TRUE ) && ( GIsUnattended != TRUE ) ) )
		{
			return( ::MessageBox( NULL, TempStr, (TCHAR *)*LocalizeError("Question",TEXT("Core")), MB_YESNO|MB_TASKMODAL ) == IDYES);
		}
		else
		{
			return FALSE;
		}
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
		return TRUE;
	}
	FContextSupplier* GetContext() const
	{
		return Context;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		Context = InSupplier;
	}
};

#endif // __FFEEDBACKCONTEXTWINDOWS_H__
