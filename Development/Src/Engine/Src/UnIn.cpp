/*=============================================================================
	UnIn.cpp: Unreal input system.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UInput);
IMPLEMENT_CLASS(UPlayerInput);

//
//	UInput::FindButtonName - Find a button.
//
BYTE* UInput::FindButtonName( const TCHAR* ButtonName )
{
	FName Button( ButtonName, FNAME_Find );
	if( Button == NAME_None )
		return NULL;

	BYTE* Ptr = (BYTE*) NameToPtr.FindRef( Button );
	
	if( Ptr == NULL )
		for(const UObject* Object = this;Object;Object = Object->GetOuter())
			for( TFieldIterator<UByteProperty> It(Object->GetClass()); It; ++It )
				if( (It->PropertyFlags & CPF_Input) && ((*It)->GetFName()==Button) )
				{
					Ptr = (BYTE*) Object + (*It)->Offset;
					NameToPtr.Set( Button, Ptr );
					return Ptr;
				}

	return Ptr;
}

//
//	UInput::FindAxisName - Find an axis.
//
FLOAT* UInput::FindAxisName( const TCHAR* ButtonName )
{
	FName Button( ButtonName, FNAME_Find );
	if( Button == NAME_None )
		return NULL;

	FLOAT* Ptr = (FLOAT*) NameToPtr.FindRef( Button );
	
	if( Ptr == NULL )
		for(const UObject* Object = this;Object;Object = Object->GetOuter())
			for( TFieldIterator<UFloatProperty> It(Object->GetClass()); It; ++It )
				if( (It->PropertyFlags & CPF_Input) && ((*It)->GetFName()==Button) )
				{
					Ptr = (FLOAT*) ((BYTE*) Object + (*It)->Offset);
					NameToPtr.Set( Button, Ptr );
					return Ptr;
				}

	return Ptr;
}

//
//	UInput::GetBind
//
FString UInput::GetBind(FName Key)
{
	UBOOL	Control = PressedKeys.FindItemIndex(KEY_LeftControl) != INDEX_NONE || PressedKeys.FindItemIndex(KEY_RightControl) != INDEX_NONE,
			Shift = PressedKeys.FindItemIndex(KEY_LeftShift) != INDEX_NONE || PressedKeys.FindItemIndex(KEY_RightShift) != INDEX_NONE,
			Alt = PressedKeys.FindItemIndex(KEY_LeftAlt) != INDEX_NONE || PressedKeys.FindItemIndex(KEY_RightAlt) != INDEX_NONE;

	for(INT BindIndex = Bindings.Num()-1; BindIndex >= 0; BindIndex--)
	{
		FKeyBind&	Bind = Bindings(BindIndex);
		if(Bind.Name == Key && (!Bind.Control || Control) && (!Bind.Shift || Shift) && (!Bind.Alt || Alt))
			return Bindings(BindIndex).Command;
	}

	return TEXT("");
}

// Checks to see if InKey is pressed down

UBOOL UInput::IsPressed( FName InKey )
{
	return ( PressedKeys.FindItemIndex( InKey ) != INDEX_NONE );
}

UBOOL UInput::IsCtrlPressed()
{
	return ( IsPressed( KEY_LeftControl ) || IsPressed( KEY_RightControl ) );
}

UBOOL UInput::IsShiftPressed()
{
	return ( IsPressed( KEY_LeftShift ) || IsPressed( KEY_RightShift ) );
}

UBOOL UInput::IsAltPressed()
{
	return ( IsPressed( KEY_LeftAlt ) || IsPressed( KEY_RightAlt ) );
}

//
//	UInput::ExecInputCommands - Execute input commands.
//

void UInput::ExecInputCommands( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TCHAR Line[256];
	while( ParseLine( &Cmd, Line, ARRAY_COUNT(Line)) )
	{
		const TCHAR* Str = Line;
		if(CurrentEvent == IE_Pressed || (CurrentEvent == IE_Released && ParseCommand(&Str,TEXT("OnRelease"))))
		{
			APlayerController*	Actor = Cast<APlayerController>(GetOuter());

			if(ScriptConsoleExec(Str,Ar,this))
				continue;
			else if(Exec(Str,Ar))
				continue;
			else if(Actor && Actor->Player && Actor->Player->Exec(Str,Ar))
				continue;
		}
		else
			Exec(Str,Ar);
	}
}

//
//	UInput::Exec - Execute a command.
//
UBOOL UInput::Exec(const TCHAR* Str,FOutputDevice& Ar)
{
	TCHAR Temp[256];
	static UBOOL InAlias=0;

	if( ParseCommand( &Str, TEXT("BUTTON") ) )
	{
		// Normal button.
		BYTE* Button;
		if( ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 ) )
		{
			if	( (Button=FindButtonName(Temp))!=NULL )
			{
				if( CurrentEvent == IE_Pressed )
					*Button = 1;
				else if( CurrentEvent == IE_Released && *Button )
					*Button = 0;
			}
			else Ar.Log( TEXT("Bad Button command") );
		}
		else Ar.Log( TEXT("Bad Button command") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("PULSE") ) )
	{
		// Normal button.
		BYTE* Button;
		if( ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 ) )
		{
			if	( (Button=FindButtonName(Temp))!=NULL )
			{
				if( CurrentEvent == IE_Pressed )
					*Button = 1;
			}
			else Ar.Log( TEXT("Bad Button command") );
		}
		else Ar.Log( TEXT("Bad Button command") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("TOGGLE") ) )
	{
		// Toggle button.
		BYTE* Button;
		if
		(	ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 )
		&&	((Button=FindButtonName(Temp))!=NULL) )
		{
			if( CurrentEvent == IE_Pressed )
				*Button ^= 0x80;
		}
		else Ar.Log( TEXT("Bad Toggle command") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("AXIS") ) )
	{
		// Axis movement.
		FLOAT* Axis;
		if(	
			ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 ) 
		&& (Axis=FindAxisName(Temp))!=NULL )
		{
			if( CurrentEvent == IE_Axis )
			{
				FLOAT	Speed			= 1.f, 
						DeadZone		= 0.f,
						AbsoluteAxis	= 0.f;
				UBOOL	Invert			= 1;

				Parse( Str, TEXT("SPEED=")			, Speed			);
				Parse( Str, TEXT("INVERT=")			, Invert		);
				Parse( Str, TEXT("DEADZONE=")		, DeadZone		);
				Parse( Str, TEXT("ABSOLUTEAXIS=")	, AbsoluteAxis	);

				// Axis is expected to be in -1 .. 1 range if dead zone is used.
				if( DeadZone > 0.f && DeadZone < 1.f )
				{
					// We need to translate and scale the input to the +/- 1 range after removing the dead zone.
					if( CurrentDelta > 0 )
					{
						CurrentDelta = Max( 0.f, CurrentDelta - DeadZone ) / (1.f - DeadZone);
					}
					else
					{
						CurrentDelta = -Max( 0.f, -CurrentDelta - DeadZone ) / (1.f - DeadZone);
					}
				}
				
				// Absolute axis like joysticks need to be scaled by delta time in order to be framerate independent.
				if( AbsoluteAxis )
				{
					Speed *= CurrentDeltaTime * AbsoluteAxis;
				}
				*Axis += Speed * Invert * CurrentDelta;
			}
		}
		else Ar.Logf( TEXT("%s Bad Axis command"),Str );
		return 1;
	}
	else if ( ParseCommand( &Str, TEXT("COUNT") ) )
	{
		BYTE *Count;
		if
		(	ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 ) 
		&& (Count=FindButtonName(Temp))!=NULL )
		{
			*Count += 1;
		}
		else Ar.Logf( TEXT("%s Bad Count command"),Str );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("KEYBINDING") ) && ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 ) )
	{
		FName	KeyName(Temp,FNAME_Find);

		if(KeyName != NAME_None)
		{
			for(UINT BindIndex = 0;BindIndex < (UINT)Bindings.Num();BindIndex++)
			{
				if(Bindings(BindIndex).Name == KeyName)
				{
					Ar.Logf(TEXT("%s"),*Bindings(BindIndex).Command);
					break;
				}
			}
		}

		return 1;
	}
	else if( !InAlias && ParseToken( Str, Temp, ARRAY_COUNT(Temp), 0 ) )
	{
		FName	KeyName(Temp,FNAME_Find);

		if(KeyName != NAME_None)
		{
			for(INT BindIndex = Bindings.Num() - 1; BindIndex >= 0; BindIndex--)
			{
				if(Bindings(BindIndex).Name == KeyName)
				{
					InAlias = 1;
					ExecInputCommands(*Bindings(BindIndex).Command,Ar);
					InAlias = 0;
					return 1;
				}
			}
		}
	}

	return 0;
}

//
//	UInput::InputKey
//
UBOOL UInput::InputKey(FName Key,EInputEvent Event,FLOAT AmountDepressed)
{
	switch(Event)
	{
	case IE_Pressed:
		if(PressedKeys.FindItemIndex(Key) != INDEX_NONE)
			return 0;
		PressedKeys.AddUniqueItem(Key);
		break;
	case IE_Released:
		if(!PressedKeys.RemoveItem(Key))
			return 0;
		break;
	};

	CurrentEvent		= Event;
	CurrentDelta		= 0.0f;
	CurrentDeltaTime	= 0.0f;

	FString	Command = GetBind(Key);

	if(Command.Len())
	{
		ExecInputCommands(*Command,*GLog);
		return 1;
	}
	else
		return Super::InputKey(Key,Event,AmountDepressed);
}

//
//	UInput::InputAxis
//
UBOOL UInput::InputAxis(FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	CurrentEvent		= IE_Axis;
	CurrentDelta		= Delta;
	CurrentDeltaTime	= DeltaTime;

	FString	Command	= GetBind(Key);

	if(Command.Len())
	{
		ExecInputCommands(*Command,*GLog);
		return 1;
	}
	else
		return Super::InputAxis(Key,Delta,DeltaTime);
}

//
//	UInput::Tick - Read input for the viewport.
//
void UInput::Tick(FLOAT DeltaTime)
{
	if(DeltaTime != -1.f)
	{
		// Update held keys with IE_Repeat.

		for(UINT PressedIndex = 0;PressedIndex < (UINT)PressedKeys.Num();PressedIndex++)
			InputAxis(PressedKeys(PressedIndex),1,DeltaTime);	
	}
	else
	{
		// Initialize axis array if needed.

		if( !AxisArray.Num() )
			for( TFieldIterator<UFloatProperty> It(GetClass()); It; ++It )
				if( (It->PropertyFlags & CPF_Input) )
					AxisArray.AddUniqueItem( (FLOAT*) ((BYTE*) this + (*It)->Offset) );

		// Reset axis.

		for( INT i=0; i<AxisArray.Num(); i++ )
			*AxisArray(i) = 0;
	}

	Super::Tick(DeltaTime);
}

//
//	UInput::ResetInput - Reset the input system's state.
//
void UInput::ResetInput()
{
	PressedKeys.Empty();

	// Reset all input bytes.
	for( TFieldIterator<UByteProperty> ItB(GetClass()); ItB; ++ItB )
		if( ItB->PropertyFlags & CPF_Input )
			*(BYTE *)((BYTE*)this + ItB->Offset) = 0;

	// Reset all input floats.
	for( TFieldIterator<UFloatProperty> ItF(GetClass()); ItF; ++ItF )
		if( ItF->PropertyFlags & CPF_Input )
			*(FLOAT *)((BYTE*)this + ItF->Offset) = 0;
}