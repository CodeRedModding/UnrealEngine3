/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the various draw mesh macros that display draw calls
 * inside of PIX.
 */

// Colors that are defined for a particular mesh type
// Each event type will be displayed using the defined color
#define DEC_LIGHT			FColor(255,0,0,255)
#define DEC_SKEL_MESH		FColor(255,0,255,255)
#define DEC_STATIC_MESH		FColor(0,128,255,255)
#define DEC_CANVAS			FColor(128,255,255,255)
#define DEC_TERRAIN			FColor(0,128,0,255)
#define DEC_SHADOW			FColor(128,128,128,255)
#define DEC_BSP				FColor(255,128,0,255)
#define DEC_PARTICLE		FColor(128,0,128,255)
// general scene rendering events
#define DEC_SCENE_ITEMS		FColor(128,128,128,255)

/**
 * Platform specific function for adding a draw event that can be viewed in PIX
 */
void appBeginDrawEvent(const FColor& Color,const TCHAR* Text);
/**
 * Platform specific function for closing a draw event that can be viewed in PIX
 */
void appEndDrawEvent(void);
/**
 * Platform specific function for setting the value of a counter that can be
 * viewed in PIX.
 */
void appSetCounterValue(const TCHAR* CounterName, FLOAT Value);

// Disable draw mesh events for final builds
#if !FINAL_RELEASE
#define WANTS_DRAW_MESH_EVENTS 1
#else
#define WANTS_DRAW_MESH_EVENTS 0
#endif

#if defined(XBOX) || defined(_WINDOWS_) || PLATFORM_UNIX || PS3 || MOBILE
#define PLATFORM_SUPPORTS_DRAW_MESH_EVENTS 1
#else
#define PLATFORM_SUPPORTS_DRAW_MESH_EVENTS 0
#endif

#if WANTS_DRAW_MESH_EVENTS && PLATFORM_SUPPORTS_DRAW_MESH_EVENTS

	/**
	 * Class that logs draw events based upon class scope. Draw events can be seen
	 * in PIX
	 */
	struct FDrawEvent
	{
		/** Whether a draw event has been emitted or not. */
		UBOOL bDrawEventHasBeenEmitted;

		/** Default constructor, initializing all member variables. */
		FDrawEvent()
		:	bDrawEventHasBeenEmitted( FALSE )
		{}

		/**
		 * Terminate the event based upon scope
		 */
		~FDrawEvent()
		{
			if( bDrawEventHasBeenEmitted )
			{
				appEndDrawEvent();
			}
		}
		/**
		 * Operator for logging a PIX event with var args
		 */
		void CDECL operator()(const FColor& Color,const TCHAR* Fmt, ...)
		{
			check( IsInRenderingThread() );
			va_list ptr;
			va_start(ptr, Fmt);
			TCHAR TempStr[256];
			// Build the string in the temp buffer
			appGetVarArgs(TempStr,ARRAY_COUNT(TempStr),ARRAY_COUNT(TempStr)-1,Fmt,ptr);
			// Now trigger the PIX event with the custom string
			appBeginDrawEvent(Color,TempStr);
			bDrawEventHasBeenEmitted = TRUE;		
		}
	};

	// Macros to allow for scoping of draw events
	#define SCOPED_DRAW_EVENT(x) FDrawEvent Event_##x; if( GEmitDrawEvents ) Event_##x
	#define SCOPED_CONDITIONAL_DRAW_EVENT(x,Condition) FDrawEvent Event_##x; if( GEmitDrawEvents && (Condition) ) Event_##x

#else

#if COMPILER_SUPPORTS_NOOP

	// Eat all calls to these with __noop
	#define SCOPED_DRAW_EVENT(x) __noop
	#define SCOPED_CONDITIONAL_DRAW_EVENT(x,Condition) __noop

#elif SUPPORTS_VARIADIC_MACROS

	// Eat all calls to these
	#define SCOPED_DRAW_EVENT(...)
	#define SCOPED_CONDITIONAL_DRAW_EVENT(...)

#else

	/**
	 * This class eats the varargs. Needed for compilers that don't support __noop()
	 */
	struct FDrawEvent
	{
		/**
		 * Operator that meets the interface but does nothing.
		 */
		void CDECL operator()(const FColor& Color,const TCHAR* Fmt, ...) const
		{
		}
	};

	// Macros to allow for scoping of draw events
	#define SCOPED_DRAW_EVENT(x) FDrawEvent Event_##x; if( 0 ) Event_##x
	#define SCOPED_CONDITIONAL_DRAW_EVENT(x,Condition) FDrawEvent Event_##x; if( 0 ) Event_##x

#endif

#endif
