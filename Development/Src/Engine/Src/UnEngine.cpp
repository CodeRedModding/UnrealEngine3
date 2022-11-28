/*=============================================================================
	UnEngine.cpp: Unreal engine main.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnLinker.h"
#include "EngineMaterialClasses.h"
#include "UnTerrain.h"
#include "EngineAIClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "UnStatChart.h"
#ifndef XBOX
#include "..\XWindow\Inc\DebugToolExec.h"
#endif

//
//	GEngine - The global engine pointer.
//

UEngine*	GEngine = NULL;

/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEngine);
IMPLEMENT_CLASS(UDebugManager);

/*-----------------------------------------------------------------------------
	Engine init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the engine.
//
UEngine::UEngine()
{
}

//
// Init class.
//
void UEngine::StaticConstructor()
{
}

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineTerrainClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#include "EngineAIClasses.h"
#include "EngineMaterialClasses.h"
#include "EngineTerrainClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

// Register input keys.
#define DEFINE_KEY(Name) FName KEY_##Name;
#include "UnKeys.h"
#undef DEFINE_KEY

//
// Initialize the engine.
//
void UEngine::Init()
{
	// Add input key names.
	#define DEFINE_KEY(Name) KEY_##Name = FName(TEXT(#Name),FNAME_Intrinsic);
	#include "UnKeys.h"
	#undef DEFINE_KEY

	// Subsystems.
	FURL::StaticInit();
	GEngineMem.Init( 65536 );
	
	// Initialize random number generator.
	if( GIsBenchmarking )
		appRandInit( 0 );
	else
		appRandInit( appCycles() );

	// Create the global chart-drawing object.
	if(!GStatChart)
	{
		GStatChart = new FStatChart();
	}


	// Add to root.
	AddToRoot();

	if( !GIsUCCMake )
	{	
		// Ensure all native classes are loaded.
		for( TObjectIterator<UClass> It ; It ; ++It )
			if( !It->GetLinker() )
				LoadObject<UClass>( It->GetOuter(), It->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );

		// Create debug manager object
		DebugManager = ConstructObject<UDebugManager>( UDebugManager::StaticClass() );
#ifndef XBOX
		// Create debug exec.	
		GDebugToolExec = new FDebugToolExec;
#endif
	}

	debugf( NAME_Init, TEXT("Unreal engine initialized") );
}

//
// Exit the engine.
//
void UEngine::Destroy()
{
	// Remove from root.
	RemoveFromRoot();

	// Clean up debug tool.
	delete GDebugToolExec;
	GDebugToolExec		= NULL;

	// Shut down all subsystems.
	Client				= NULL;
	GEngine				= NULL;
	FURL::StaticExit();
	GEngineMem.Exit();

	if(GStatChart)
	{
		delete GStatChart;
		GStatChart = NULL;
	}

	Super::Destroy();
}

//
// Flush all caches.
//
void UEngine::Flush()
{
	GStreamingManager->Flush( FALSE );

	if( Client )
		Client->Flush();
}

void UEngine::PostEditChange(UProperty* PropertyThatChanged)
{
	GStreamingManager->Flush( FALSE );

	MaxStreamedInMips = Max<UINT>( 1, MaxStreamedInMips );
	MinStreamedInMips = Max<UINT>( 1, Min( MinStreamedInMips, MaxStreamedInMips ) );
}

//
// Progress indicator.
//
void UEngine::SetProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
UBOOL UEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// See if any other subsystems claim the command.
	if( GSys			&&	GSys->Exec			(Cmd,Ar) ) return 1;
	if(						UObject::StaticExec	(Cmd,Ar) ) return 1;
	if( Client			&&	Client->Exec		(Cmd,Ar) ) return 1;
	if( GDebugToolExec	&&	GDebugToolExec->Exec(Cmd,Ar) ) return 1;
	if( GStatChart		&&	GStatChart->Exec	(Cmd,Ar) ) return 1;

	// Handle engine command line.
	if( ParseCommand(&Cmd,TEXT("FLUSH")) )
	{
		Flush();
		Ar.Log( TEXT("Flushed engine caches") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWLOG")) )
	{
		// Toggle display of console log window.
		if( GLogConsole )
			GLogConsole->Show( !GLogConsole->IsShown() );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CRACKURL")) )
	{
		FURL URL(NULL,Cmd,TRAVEL_Absolute);
		if( URL.Valid )
		{
			Ar.Logf( TEXT("     Protocol: %s"), *URL.Protocol );
			Ar.Logf( TEXT("         Host: %s"), *URL.Host );
			Ar.Logf( TEXT("         Port: %i"), URL.Port );
			Ar.Logf( TEXT("          Map: %s"), *URL.Map );
			Ar.Logf( TEXT("   NumOptions: %i"), URL.Op.Num() );
			for( INT i=0; i<URL.Op.Num(); i++ )
				Ar.Logf( TEXT("     Option %i: %s"), i, *URL.Op(i) );
			Ar.Logf( TEXT("       Portal: %s"), *URL.Portal );
			Ar.Logf( TEXT("       String: '%s'"), *URL.String() );
		}
		else Ar.Logf( TEXT("BAD URL") );
		return 1;
	}
	else return 0;
}

INT UEngine::ChallengeResponse( INT Challenge )
{
	return 0;
}

/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

void UServerCommandlet::StaticConstructor()
{
	IsClient    	= 0;
	IsEditor   	 	= 0;
	LogToConsole	= 1;
	IsServer    	= 1;
	LazyLoad    	= 1;
}

INT UServerCommandlet::Main( const TCHAR* Parms )
{
	// Language.
	FString Temp;
	if( GConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), Temp, GEngineIni ) )
	UObject::SetLanguage( *Temp );

	// Create the editor class.
	UClass* EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = ConstructObject<UEngine>( EngineClass );
	GEngine->Init();

	// Main loop.
	GIsRunning = 1;
	DOUBLE OldTime = appSeconds();
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update the world.
		DOUBLE NewTime = appSeconds();
		GEngine->Tick( NewTime - OldTime );
		OldTime = NewTime;

		// Enforce optional maximum tick rate.
		FLOAT MaxTickRate = GEngine->GetMaxTickRate();
		if( MaxTickRate>0.f )
		{
			FLOAT Delta = (1.f/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
	}
	GIsRunning = 0;
	return 0;
}

IMPLEMENT_CLASS(UServerCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

