/*=============================================================================
	UnMisc.cpp: Various core platform-independent functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/** For FConfigFile in appInit							*/
#include "FConfigCacheIni.h"
#include "../../Engine/Inc/Localization.h"  // !!! FIXME: Core referencing Engine.

#include "StackTracker.h"
#include "ProfilingHelpers.h"

#if XBOX
	#include "Engine.h"
	#include "XeD3DTextureAllocator.h"
#endif

#if PS3
	#include "FFileManagerPS3.h"
#endif

#if IPHONE
	#include <signal.h>
#endif

#include "IConsoleManager.h"
#if WITH_STEAMWORKS
#include "OnlineSubsystemSteamworks.h"
#endif

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

FString PerfMemRunResultStrings[4] = { TEXT("Unknown"), TEXT("OOM"), TEXT("Passed"), TEXT("Crashed") };

/** Number of top function calls to hide when dumping the callstack as text. */
#define CALLSTACK_IGNOREDEPTH 2

/**
 * Serializes a string as ANSI char array.
 *
 * @param	String			String to serialize
 * @param	Ar				Archive to serialize with
 * @param	MinCharacters	Minimum number of characters to serialize.
 */
void SerializeStringAsANSICharArray( const FString& String, FArchive& Ar, INT MinCharacters )
{
	INT	Length = Max( String.Len(), MinCharacters );
	Ar << Length;
	for( INT CharIndex=0; CharIndex<String.Len(); CharIndex++ )
	{
		ANSICHAR AnsiChar = ToAnsi( String[CharIndex] );
		Ar << AnsiChar;
	}
	// Zero pad till minimum number of characters are written.
	for( INT i=String.Len(); i<Length; i++ )
	{
		ANSICHAR NullChar = 0;
		Ar << NullChar;
	}
}

/**
 * PERF_ISSUE_FINDER
 *
 * Once a level is loaded you should not have LazyLoaded Array data being loaded. 
 *
 * Turn his on to have the engine log out when an LazyLoaded array's data is being loaded
 * Right now this will log even when you should be loading LazyArrays.  So you will get a bunch of
 * false positive log messages on Level load.  Once that has occured any logs will be true perf issues.
 * 
 */
//#define PERF_LOG_LAZY_ARRAY_LOADS 1

/*-----------------------------------------------------------------------------
	UObjectSerializer.
-----------------------------------------------------------------------------*/

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UObjectSerializer::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );
	AddReferencedObjectsViaSerialization( ObjectArray );
}

/**
 * Adds an object to the serialize list
 *
 * @param Object The object to add to the list
 */
void UObjectSerializer::AddObject(FSerializableObject* Object)
{
	check(Object);
	// Make sure there are no duplicates. Should be impossible...
	SerializableObjects.AddUniqueItem(Object);
}

/**
 * Removes a window from the list so it won't receive serialization events
 *
 * @param Object The object to remove from the list
 */
void UObjectSerializer::RemoveObject(FSerializableObject* Object)
{
	check(Object);
	SerializableObjects.RemoveItem(Object);
}

/**
 * Forwards this call to all registered objects so they can serialize
 * any UObjects they depend upon
 *
 * @param Ar The archive to serialize with
 */
void UObjectSerializer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// Let each registered object handle its serialization
	for (INT i = 0; i < SerializableObjects.Num(); i++)
	{
		FSerializableObject* Object = SerializableObjects(i);
		check(Object);
		Object->Serialize(Ar);
	}
}

/**
 * Destroy function that gets called before the object is freed. This might
 * be as late as from the destructor.
 */
void UObjectSerializer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Make sure FSerializableObjects that are around after exit purge don't
		// reference this object.
		check( FSerializableObject::GObjectSerializer == this );
		FSerializableObject::GObjectSerializer = NULL;
	}

	Super::FinishDestroy();
}

IMPLEMENT_CLASS(UObjectSerializer);

/** Static used for serializing non- UObject objects */
UObjectSerializer* FSerializableObject::GObjectSerializer = NULL;


/*-----------------------------------------------------------------------------
	FOutputDevice implementation.
-----------------------------------------------------------------------------*/

void FOutputDevice::Log( EName Event, const TCHAR* Str )
{
	if (!bAllowSuppression || !FName::SafeSuppressed(Event))
	{
		Serialize( Str, Event );
	}
}
void FOutputDevice::Log( const TCHAR* Str )
{
	if (!bAllowSuppression || !FName::SafeSuppressed(NAME_Log))
	{	
		Serialize( Str, NAME_Log );
	}
}
void FOutputDevice::Log( const FString& S )
{
	if (!bAllowSuppression || !FName::SafeSuppressed(NAME_Log))
	{	
		Serialize( *S, NAME_Log );
	}
}
void FOutputDevice::Log( enum EName Type, const FString& S )
{
	if (!bAllowSuppression || !FName::SafeSuppressed(Type))
	{
		Serialize( *S, Type );
	}
}



// Pulled the two FOutputDevice::Logf functions into shared code. Needs to be a #define
// since it uses GET_VARARGS_RESULT which uses the va_list stuff which operates on the
// current function, so we can't easily call a function
#define GROWABLE_LOGF(SerializeFunc) \
	INT		BufferSize	= 1024; \
	TCHAR*	Buffer		= NULL; \
	INT		Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[256]; \
	TCHAR*	AllocatedBuffer = NULL; \
\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_VARARGS_RESULT( Buffer, ARRAY_COUNT(StackBuffer), ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
\
	/* if that fails, then use heap allocation to make enough space */ \
	while(Result == -1) \
	{ \
		appSystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) appSystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
	}; \
	Buffer[Result] = 0; \
	; \
\
	SerializeFunc; \
	appSystemFree(AllocatedBuffer);

VARARG_BODY( void, FOutputDevice::Logf, const TCHAR*, VARARG_EXTRA(enum EName Event) )
{
	/* do we want to log this type of message? */ \
	if (!bAllowSuppression || !FName::SafeSuppressed(Event))
	{ 
		// call serialize with the final buffer
		GROWABLE_LOGF(Serialize(Buffer, Event))
	}
}
VARARG_BODY( void, FOutputDevice::Logf, const TCHAR*, VARARG_NONE )
{
	if (!bAllowSuppression || !FName::SafeSuppressed(NAME_Log))
	{ 
		// call serialize with the final buffer
		GROWABLE_LOGF(Serialize(Buffer, NAME_Log))
	}
}


/*-----------------------------------------------------------------------------
	FString implementation.
-----------------------------------------------------------------------------*/

FString FString::Chr( TCHAR Ch )
{
	TCHAR Temp[2]={Ch,0};
	return FString(Temp);
}

FString FString::LeftPad( INT ChCount ) const
{
	INT Pad = ChCount - Len();
	if( Pad > 0 )
	{
		TCHAR* Ch = (TCHAR*)appAlloca((Pad+1)*sizeof(TCHAR));
		INT i;
		for( i=0; i<Pad; i++ )
			Ch[i] = ' ';
		Ch[i] = 0;
		return FString(Ch) + *this;
	}
	else return *this;
}
FString FString::RightPad( INT ChCount ) const
{
	INT Pad = ChCount - Len();
	if( Pad > 0 )
	{
		TCHAR* Ch = (TCHAR*)appAlloca((Pad+1)*sizeof(TCHAR));
		INT i;
		for( i=0; i<Pad; i++ )
			Ch[i] = ' ';
		Ch[i] = 0;
		return *this + FString(Ch);
	}
	else return *this;
}

UBOOL FString::IsNumeric() const
{
	if ( Len() == 0 )
		return 0;

	TCHAR C = (*this)(0);
	
	if( C == '-' || C =='.' || appIsDigit( C ) )
	{
		UBOOL HasDot = (C == '.');

		for( INT i=1; i<Len(); i++ )
		{
			C = (*this)(i);

			if( C == '.' )
			{
				if( HasDot )
				{
					return 0;
				}
				else
				{
					HasDot = 1;
				}
			}
			else if( !appIsDigit(C) )
			{
				return 0;
			}
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Breaks up a delimited string into elements of a string array.
 *
 * @param	InArray		The array to fill with the string pieces
 * @param	pchDelim	The string to delimit on
 * @param	InCullEmpty	If 1, empty strings are not added to the array
 *
 * @return	The number of elements in InArray
 */
INT FString::ParseIntoArray( TArray<FString>* InArray, const TCHAR* pchDelim, UBOOL InCullEmpty ) const
{
	check(InArray);
	InArray->Empty();
	const TCHAR *Start = GetTypedData();
	INT DelimLength = appStrlen(pchDelim);
	if (Start && DelimLength)
	{
		while( const TCHAR *At = appStrstr(Start,pchDelim) )
		{
			if (!InCullEmpty || At-Start)
			{
				new (*InArray) FString(At-Start,Start);
			}
			Start += DelimLength + (At-Start);
		}
		if (!InCullEmpty || *Start)
		{
			new(*InArray) FString(Start);
		}

	}
	return InArray->Num();
}

/**
 * Takes a string, and skips over all instances of white space and returns the new string
 *
 * @param	WhiteSpace		An array of white space strings
 * @param	NumWhiteSpaces	The length of the WhiteSpace array
 * @param	S				The input and output string
 */
static void SkipOver(const TCHAR** WhiteSpace, INT NumWhiteSpaces, FString& S)
{
	UBOOL bStop = false;

	// keep going until we hit non-white space
	while (!bStop)
	{
		// we stop it we don't find any white space
		bStop = true;
		// loop over all possible white spaces to search for
		for (INT iWS = 0; iWS < NumWhiteSpaces; iWS++)
		{
			// get the length (tiny optimization)
			INT WSLen = appStrlen(WhiteSpace[iWS]);

			// if we start with this bit of whitespace, chop it off, and keep looking for more whitespace
			if (appStrnicmp(*S, WhiteSpace[iWS], WSLen) == 0)
			{
				// chop it off
				S = S.Mid(WSLen);
				// keep looking!
				bStop = false;
				break;
			}
		}
	}
}

/**
 * Splits the input string on the first bit of white space, and returns the initial token
 * (to the left of the white space), and the rest (white space and to the right)
 *
 * @param	WhiteSpace		An array of white space strings
 * @param	NumWhiteSpaces	The length of the WhiteSpace array
 * @param	Token			The first token before any white space
 * @param	S				The input and outputted remainder string
 *
 * @return	Was there a valid token before the end of the string?
 */
static UBOOL SplitOn( const TCHAR** WhiteSpace, INT NumWhiteSpaces, FString& Token, FString& S, TCHAR& InCh )
{
	// this is the index of the first instance of whitespace
	INT SmallestToken = MAXINT;
	InCh = TEXT(' ');

	// Keeps track if we are inside quotations or not (if we are, we don't stop at whitespace until we see the ending quote)
	UBOOL bInQuotes = false;

	// loop through all possible white spaces
	for (INT iWS = 0; iWS < NumWhiteSpaces; iWS++)
	{
		// look for the first instance of it
		INT NextWS = S.InStr(WhiteSpace[iWS]);

		// if shouldn't be at the start of the string, because SkipOver should have been called
		check(NextWS != 0);

		// if we found this white space, and it is before any other white spaces, remember it
		if (NextWS > 0 && NextWS < SmallestToken)
		{
			SmallestToken = NextWS;
			InCh = *WhiteSpace[iWS];
		}
	}

	// if we found some white space, SmallestToken is pointing to the the first one
	if (SmallestToken != MAXINT)
	{
		// get the token before the white space
		Token = S.Left(SmallestToken);
		// update out string with the remainder
		S = S.Mid(SmallestToken);
		// we found a token
		return true;
	}

	// we failed to find a token
	return false;
}

/** Caution!! this routine is O(N^2) allocations...use it for parsing very short text or not at all */
INT FString::ParseIntoArrayWS( TArray<FString>* InArray, const TCHAR* pchExtraDelim ) const
{
	// default array of White Spaces, the last entry can be replaced with the optional pchExtraDelim string
	// (if you want to split on white space and another character)
	static const TCHAR* WhiteSpace[] = 
	{
		TEXT(" "),
		TEXT("\t"),
		TEXT("\r"),
		TEXT("\n"),
		TEXT(""),
	};

	// start with just the standard whitespaces
	INT NumWhiteSpaces = ARRAY_COUNT(WhiteSpace) - 1;
	// if we got one passed in, use that in addition
	if (pchExtraDelim && *pchExtraDelim)
	{
		WhiteSpace[NumWhiteSpaces++] = pchExtraDelim;
	}

	check(InArray);
	InArray->Empty();

	// this is our temporary workhorse string
	FString S = *this;

	UBOOL bStop = false;
	// keep going until we run out of tokens
	while (!bStop)
	{
		// skip over any white space at the beginning of the string
		SkipOver(WhiteSpace, NumWhiteSpaces, S);
		
		// find the first token in the string, and if we get one, add it to the output array of tokens
		FString Token;
		TCHAR ch;
		if (SplitOn(WhiteSpace, NumWhiteSpaces, Token, S, ch))
		{
			if( Token[0] == TEXT('"') )
			{
				INT SaveSz = Token.Len();

				FString Wk = FString::Printf( TEXT("%s%c"), *Token, ch );
				for( INT x = 1 ; x < S.Len() ; ++x )
				{
					if( S(x) == TEXT('"') )
					{
						Wk += TEXT("\"");
						break;
					}
					else
					{
						Wk = Wk + S.Mid(x,1);
					}
				}

				Token = Wk;

				INT DiffSz = Token.Len() - SaveSz;
				S = S.Mid( DiffSz );
			}

			// stick it on the end
			new(*InArray)FString(Token);
		}
		else
		{
			// if the remaining string is not empty, then we need to add the last token
			if (S.Len())
			{
				new(*InArray)FString(S);
			}

			// and, we're done this crazy ride
			bStop = true;
		}
	}

	// simply return the number of elements in the output array
	return InArray->Num();
}

FString FString::Replace(const TCHAR* From, const TCHAR* To, UBOOL bIgnoreCase) const
{
	if (Len() == 0)
	{
		return *this;
	}

	FString Result;

	// get a pointer into the character data
	TCHAR* Travel = (TCHAR*)GetData();

	// precalc the length of the From string
	INT FromLength = appStrlen(From);

	// appStrstr will not behave like we want on empty From string
	if (FromLength == 0)
 	{
 		return *this;
 	}

	while (TRUE)
	{
		// look for From in the remaining string
		TCHAR* FromLocation = bIgnoreCase ? appStristr(Travel, From) : appStrstr(Travel, From);
		if (FromLocation)
		{
			// replace the first letter of the From with 0 so we can do a strcpy (FString +=)
			TCHAR C = *FromLocation;
			*FromLocation = 0;
			
			// copy everything up to the From
			Result += Travel;

			// copy over the To
			Result += To;

			// retore the letter, just so we don't have 0's in the string
			*FromLocation = *From;

			Travel = FromLocation + FromLength;
		}
		else
		{
			break;
		}
	}

	// copy anything left over
	Result += Travel;

	return Result;
}

/**
 * Replace all occurrences of SearchText with ReplacementText in this string.
 *
 * @param	SearchText	the text that should be removed from this string
 * @param	ReplacementText		the text to insert in its place
 *
 * @return	the number of occurrences of SearchText that were replaced.
 */
INT FString::ReplaceInline( const TCHAR* SearchText, const TCHAR* ReplacementText )
{
	INT ReplacementCount = 0;

	if (Len() > 0
	&&	SearchText != NULL && *SearchText != 0
	&&	ReplacementText != NULL && appStrcmp(SearchText, ReplacementText) != 0 )
	{
		const INT NumCharsToReplace=appStrlen(SearchText);
		const INT NumCharsToInsert=appStrlen(ReplacementText);

		if ( NumCharsToInsert == NumCharsToReplace )
		{
			TCHAR* Pos = appStristr(&(*this)(0), SearchText);
			while ( Pos != NULL )
			{
				ReplacementCount++;

				// appStrcpy now inserts a terminating zero so can't use that
				for ( INT i = 0; i < NumCharsToInsert; i++ )
				{
					Pos[i] = ReplacementText[i];
				}

				if ( Pos + NumCharsToReplace - **this < Len() )
				{
					Pos = appStristr(Pos + NumCharsToReplace, SearchText);
				}
				else
				{
					break;
				}
			}
		}
		else if ( InStr(SearchText) != INDEX_NONE )
		{
			FString Copy(*this);
			Empty(Len());

			// get a pointer into the character data
			TCHAR* WritePosition = (TCHAR*)Copy.GetData();
			// look for From in the remaining string
			TCHAR* SearchPosition = appStristr(WritePosition, SearchText);
			while ( SearchPosition != NULL )
			{
				ReplacementCount++;

				// replace the first letter of the From with 0 so we can do a strcpy (FString +=)
				*SearchPosition = 0;

				// copy everything up to the SearchPosition
				(*this) += WritePosition;

				// copy over the ReplacementText
				(*this) += ReplacementText;

				// restore the letter, just so we don't have 0's in the string
				*SearchPosition = *SearchText;

				WritePosition = SearchPosition + NumCharsToReplace;
				SearchPosition = appStristr(WritePosition, SearchText);
			}

			// copy anything left over
			(*this) += WritePosition;
		}
	}

	return ReplacementCount;
}


/**
 * Returns a copy of this string with all quote marks escaped (unless the quote is already escaped)
 */
FString FString::ReplaceQuotesWithEscapedQuotes() const
{
	if ( InStr(TEXT("\"")) != INDEX_NONE )
	{
		FString Result;

		const TCHAR* pChar = **this;

		UBOOL bEscaped = FALSE;
		while ( *pChar != 0 )
		{
			if ( bEscaped )
			{
				bEscaped = FALSE;
			}
			else if ( *pChar == TCHAR('\\') )
			{
				bEscaped = TRUE;
			}
			else if ( *pChar == TCHAR('"') )
			{
				Result += TCHAR('\\');
			}

			Result += *pChar++;
		}
		
		return Result;
	}

	return *this;
}

#define MAX_SUPPORTED_ESCAPE_CHARS 3

static const TCHAR* CharToEscapeSeqMap[MAX_SUPPORTED_ESCAPE_CHARS][2] =
{
	{ TEXT("\n"), TEXT("\\n")  },
	{ TEXT("\r"), TEXT("\\r")  },
	{ TEXT("\t"), TEXT("\\t")  }

	// these are currently disabled as the escaped backslash causes problems, the escaped ' and " cause too much churn in the inis
// 	{ TEXT("\'"), TEXT("\\'")  },
//	{ TEXT("\\"), TEXT("\\\\") },
// 	{ TEXT("\""), TEXT("\\\"") }
};

/**
 * Replaces certain characters with the "escaped" version of that character (i.e. replaces "\n" with "\\n").
 * The characters supported are: { \n, \r, \t, \', \", \\ }.
 *
 * @param	Chars	by default, replaces all supported characters; this parameter allows you to limit the replacement to a subset.
 *
 * @return	a string with all control characters replaced by the escaped version.
 */
FString FString::ReplaceCharWithEscapedChar( const TArray<TCHAR>* Chars/*=NULL*/ ) const
{
	if ( Len() > 0 && (Chars == NULL || Chars->Num() > 0) )
	{
		FString Result(*this);
		for ( INT ChIdx = 0; ChIdx < MAX_SUPPORTED_ESCAPE_CHARS; ChIdx++ )
		{
			if ( Chars == NULL || Chars->ContainsItem(*(CharToEscapeSeqMap[ChIdx][0])) )
			{
				// use ReplaceInline as that won't create a copy of the string if the character isn't found
				Result.ReplaceInline(CharToEscapeSeqMap[ChIdx][0], CharToEscapeSeqMap[ChIdx][1]);
			}
		}
		return Result;
	}

	return *this;
}
/**
 * Removes the escape backslash for all supported characters, replacing the escape and character with the non-escaped version.  (i.e.
 * replaces "\\n" with "\n".  Counterpart to ReplaceCharWithEscapedChar().
 */
FString FString::ReplaceEscapedCharWithChar( const TArray<TCHAR>* Chars/*=NULL*/ ) const
{
	if ( Len() > 0 && (Chars == NULL || Chars->Num() > 0) )
	{
		FString Result(*this);
		for ( INT ChIdx = 0; ChIdx < MAX_SUPPORTED_ESCAPE_CHARS; ChIdx++ )
		{
			if ( Chars == NULL || Chars->ContainsItem(*(CharToEscapeSeqMap[ChIdx][0])) )
			{
				// use ReplaceInline as that won't create a copy of the string if the character isn't found
				Result.ReplaceInline( CharToEscapeSeqMap[ChIdx][1], CharToEscapeSeqMap[ChIdx][0] );
			}
		}
		return Result;
	}

	return *this;
}

/** 
 * Replaces all instances of '\t' with TabWidth number of spaces
 * @param InSpacesPerTab - Number of spaces that a tab represents
 */
FString FString::ConvertTabsToSpaces (const INT InSpacesPerTab)
{
	//must call this with at least 1 space so the modulus operation works
	check(InSpacesPerTab > 0);

	FString FinalString = *this;
	INT TabIndex;
	while ((TabIndex = FinalString.InStr(TEXT("\t"))) != INDEX_NONE )
	{
		FString LeftSide = FinalString.Left(TabIndex);
		FString RightSide = FinalString.Mid(TabIndex+1);

		FinalString = LeftSide;
		//for a tab size of 4, 
		UBOOL bSearchFromEnd = TRUE;
		UBOOL bIgnoreCase = TRUE;
		INT LineBegin = LeftSide.InStr(TEXT("\n"), bSearchFromEnd, bIgnoreCase, TabIndex);
		if (LineBegin == INDEX_NONE)
		{
			LineBegin = 0;
		}
		INT CharactersOnLine = (LeftSide.Len()-LineBegin);

		INT NumSpacesForTab = InSpacesPerTab - (CharactersOnLine % InSpacesPerTab);
		for (INT i = 0; i < NumSpacesForTab; ++i)
		{
			FinalString.AppendChar(' ');
		}
		FinalString += RightSide;
	}

	return FinalString;
}

// This starting size catches 99.97% of printf calls - there are about 700k printf calls per level
#define STARTING_BUFFER_SIZE		128

VARARG_BODY( FString, FString::Printf, const TCHAR*, VARARG_NONE )
{
	INT		BufferSize	= STARTING_BUFFER_SIZE;
	TCHAR	StartingBuffer[STARTING_BUFFER_SIZE];
	TCHAR*	Buffer		= StartingBuffer;
	INT		Result		= -1;

	// First try to print to a stack allocated location 
	GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );

	// If that fails, start allocating regular memory
	if( Result == -1 )
	{
		Buffer = NULL;
		while(Result == -1)
		{
			BufferSize *= 2;
			Buffer = (TCHAR*) appRealloc( Buffer, BufferSize * sizeof(TCHAR) );
			GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		};
	}

	Buffer[Result] = 0;

	FString ResultString(Buffer);

	if( BufferSize != STARTING_BUFFER_SIZE )
	{
		appFree( Buffer );
	}

	return ResultString;
}

FArchive& operator<<( FArchive& Ar, FString& A )
{
	INT SaveNum;
	const UBOOL bIsLoading = Ar.IsLoading();
	if( !bIsLoading )
	{
		// > 0 for ANSICHAR, < 0 for UNICHAR serialization
		SaveNum = -A.Num();
		if( !Ar.IsForcingUnicode() && appIsPureAnsi(*A) )
		{
			SaveNum	= A.Num();
		}
	}
	Ar << SaveNum;
	if( bIsLoading )
	{
		// Protect against network packets allocating too much memory
		if( Ar.GetMaxSerializeSize() > 0 && Abs(SaveNum) > Ar.GetMaxSerializeSize() )
		{
			Ar.ArIsError = 1;
			Ar.ArIsCriticalError = 1;
			return Ar;
		}

		// Resize the array only if it passes the above tests to prevent rogue packets from crashing
		A.ArrayMax = A.ArrayNum = Abs(SaveNum);
		A.AllocatorInstance.ResizeAllocation(0,A.ArrayMax,sizeof(TCHAR));

		if( SaveNum>=0 )
		{
			// String was serialized as a series of ANSICHARs.
			ANSICHAR* AnsiString = (ANSICHAR*) appMalloc( A.Num() * sizeof(ANSICHAR) );
			Ar.Serialize( AnsiString, A.Num() * sizeof(ANSICHAR) );
			for( INT i=0; i<A.Num(); i++ )
			{
				A(i) = FromAnsi(AnsiString[i]);
			}
			appFree(AnsiString);
		}
		else
		{
			// read in the unicode string and byteswap it, etc
			appSerializeUnicodeString(Ar, A.GetTypedData(), A.Num());
		}

		// Throw away empty string.
		if( A.Num()==1 )
		{
			A.Empty();
		}
	}
	else
	{
		A.CountBytes( Ar );
		if( SaveNum>=0 )
		{
			// String is being serialized as a series of ANSICHARs.
			if(A.Num() > 0)
			{
				ANSICHAR* AnsiString = (ANSICHAR*) appAlloca( A.Num() );
				for( INT i=0; i<A.Num(); i++ )
				{
					AnsiString[i] = ToAnsi(A(i));
				}
				Ar.Serialize( AnsiString, sizeof(ANSICHAR) * A.Num() );
			}
		}
		else
		{
			// String is serialized as a series of UNICHARs.	
#if TCHAR_IS_4_BYTES  // "Unicode" is 2 bytes in Unreal, but on platforms where TCHAR is UCS-4, we need to convert here.
			if(A.Num() > 0)
			{
				UNICHAR* UniString = (UNICHAR*) appAlloca( A.Num() * sizeof (UNICHAR) );
				for( INT i=0; i<A.Num(); i++ )
				{
					const WORD Word = (WORD) (A(i));
					UniString[i] = (UNICHAR) (INTEL_ORDER16(Word));
				}
				Ar.Serialize( UniString, sizeof(UNICHAR) * A.Num() );
			}
#elif TCHAR_IS_1_BYTE  // "Unicode" is 2 bytes in Unreal, but on platforms where TCHAR is char, we need to convert here.
			if( A.Num() > 0 )
			{
				UNICHAR* UniString = (UNICHAR*) appAlloca( A.Num() * sizeof(UNICHAR) );

				// copy from 1 bytes to 2
				for( INT i = 0; i < A.Num(); i++ )
				{
					const WORD Word = (WORD) (A(i));
					UniString[i] = (UNICHAR) Word;
				}
				Ar.Serialize( UniString, sizeof(UNICHAR) * A.Num() );
			}
#else  // is UNICODE, and TCHAR is 2 bytes.
#if CONSOLE && !__INTEL_BYTE_ORDER__
			// the loading code in this case assumes byteswapping is required so we must do so here if it actually wouldn't be
			for( INT i=0; i<A.Num(); i++ )
			{
				UNICHAR TheChar = INTEL_ORDER16((WORD)A(i));
				Ar.Serialize(&TheChar, sizeof(UNICHAR));
			}
#else
			Ar.Serialize( A.GetData(), sizeof(UNICHAR) * A.Num() );
#endif // CONSOLE && !__INTEL_BYTE_ORDER__
#endif // TCHAR_IS_4_BYTES
		}
	}
	return Ar;
}

/*-----------------------------------------------------------------------------
	FFilename implementation
-----------------------------------------------------------------------------*/

/**
 * Gets the extension for this filename.
 *
 * @param	bIncludeDot		if TRUE, includes the leading dot in the result
 *
 * @return	the extension of this filename, or an empty string if the filename doesn't have an extension.
 */
FString FFilename::GetExtension( UBOOL bIncludeDot/*=FALSE*/ ) const
{
	const FString Filename = GetCleanFilename();
	INT DotPos = Filename.InStr(TEXT("."), TRUE);
	if (DotPos != INDEX_NONE)
	{
		return Filename.Mid(DotPos + (bIncludeDot ? 0 : 1));
	}

	return TEXT("");
}

// Returns the filename (with extension), minus any path information.
FString FFilename::GetCleanFilename() const
{
	INT Pos = InStr(PATH_SEPARATOR, TRUE);

	// in case we are using slashes on a platform that uses backslashes
	Pos = Max(Pos, InStr(TEXT("/"), TRUE));

	// in case we are using backslashes on a platform that doesn't use backslashes
	Pos = Max(Pos, InStr(TEXT("\\"), TRUE));

	if ( Pos != INDEX_NONE )
	{
		return Mid(Pos + 1);
	}

	return *this;
}

// Returns the same thing as GetCleanFilename, but without the extension
FString FFilename::GetBaseFilename( UBOOL bRemovePath ) const
{
	FString Wk = bRemovePath ? GetCleanFilename() : FString(*this);

	// remove the extension
	INT Pos = Wk.InStr(TEXT("."), TRUE);
	if ( Pos != INDEX_NONE )
	{
		return Wk.Left(Pos);
	}

	return Wk;
}

// Returns the path in front of the filename
FString FFilename::GetPath() const
{
	INT Pos = InStr(PATH_SEPARATOR, TRUE);

	// in case we are using slashes on a platform that uses backslashes
	Pos = Max(Pos, InStr(TEXT("/"), TRUE));

	// in case we are using backslashes on a platform that doesn't use backslashes
	Pos = Max(Pos, InStr(TEXT("\\"), TRUE));
	if ( Pos != INDEX_NONE )
	{
		return Left(Pos);
	}

	return TEXT("");
}

/** @return TRUE if this file was found, FALSE otherwise */
UBOOL FFilename::FileExists() const
{
	return INDEX_NONE != GFileManager->FileSize( *(*this) );
}

/** @return TRUE if this file was found, FALSE otherwise, but does not open the file to determine this */
UBOOL FFilename::NonLockingFileExists() const
{
#if CONSOLE
	// consoles use a TOC, so don't lock files to figure this out anyway
	return INDEX_NONE != GFileManager->FileSize( *(*this) );
#else
	TArray<FString> Files;
	GFileManager->FindFiles(Files,*(*this),TRUE,FALSE);
	return !!Files.Num();
#endif
}

/**
 * Returns the localized package name by appending the language suffix before the extension.
 *
 * @param	Language	Language to use.
 * @return	Localized package name
 */
FString FFilename::GetLocalizedFilename( const TCHAR* Language ) const
{
	// Use default language if none specified.
	if( !Language )
	{
		Language = UObject::GetLanguage();
	}

	// Prepend path and path separator.
	FFilename LocalizedFilename = GetPath();
	if( LocalizedFilename.Len() )
	{
		LocalizedFilename += PATH_SEPARATOR;
	}

	FString BaseName = GetBaseFilename();

	// If the name already has a language appended, remove it
	INT Offset = BaseName.InStr( TEXT( "_LOC_" ), FALSE, TRUE );
	if( Offset >= 0 )
	{
		BaseName = BaseName.Left( Offset + 4 );
	}

	// Append _LANG to filename.
	LocalizedFilename += BaseName + TEXT( "_" ) + Language;
	
	// Append extension if used.
	if( GetExtension().Len() )
	{
		LocalizedFilename += FString( TEXT( "." ) ) + GetExtension();
	}
	
	return LocalizedFilename;
}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

//
// Returns whether the string is pure ANSI.
//
UBOOL appIsPureAnsi( const TCHAR* Str )
{
// @todo flash Make this TCHAR_IS_ONE_BYTE thing
#if FLASH
	return TRUE;
#else
	for( ; *Str; Str++ )
	{
		if( *Str>0xff )
		{
			return 0;
		}
	}
	return 1;
#endif
}


//
// Formats the text for appOutputDebugString.
//
void VARARGS appOutputDebugStringf( const TCHAR *Fmt, ... )
{
	// allow unlimited size strings
	GROWABLE_LOGF(appOutputDebugString(Buffer))
}


/** Sends a formatted message to a remote tool. */
void VARARGS appSendNotificationStringf( const ANSICHAR *Format, ... )
{
	ANSICHAR TempStr[4096];
	GET_VARARGS_ANSI( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );
	appSendNotificationString( TempStr );
}


//
// Failed assertion handler.
//warning: May be called at library startup time.
//
void VARARGS appFailAssertFunc( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Format/*=TEXT("")*/, ... )
{
	// Ignore this assert if we're already forcibly shutting down because of a critical error.
	// Note that appFailAssertFuncDebug() is still called.
	if ( !GIsCriticalError )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );
#if !CONSOLE && !PLATFORM_MACOSX // @todo Mac
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*) appSystemMalloc( StackTraceSize );
		StackTrace[0] = 0;
		// Walk the stack and dump it to the allocated memory.
		appStackWalkAndDump( StackTrace, StackTraceSize, CALLSTACK_IGNOREDEPTH );
		GError->Logf( TEXT("Assertion failed: %s [File:%s] [Line: %i]\n%s\nStack: %s"), ANSI_TO_TCHAR(Expr), ANSI_TO_TCHAR(File), Line, TempStr, ANSI_TO_TCHAR(StackTrace) );
		appSystemFree( StackTrace );
#else
		GError->Logf( TEXT("Assertion failed: %s [File:%s] [Line: %i]\n%s\nStack: Not avail yet"), ANSI_TO_TCHAR(Expr), ANSI_TO_TCHAR(File), Line, TempStr );
#endif
	}
}


//
// Failed assertion handler.  This version only calls appOutputDebugString.
//
void VARARGS appFailAssertFuncDebug( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Format/*=TEXT("")*/, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );
	appOutputDebugStringf( TEXT("%s(%i): Assertion failed: %s\n%s\n"), ANSI_TO_TCHAR(File), Line, ANSI_TO_TCHAR(Expr), TempStr );

#if _XBOX
	extern void appStopLoggingThread(void);
	appStopLoggingThread();
#endif

#if PS3
	DOUBLE CrashTime	= appSeconds() - GStartTime;
	INT CrashHour		= appTrunc(CrashTime/3600.0);
	INT CrashMinute		= appTrunc(CrashTime/60.0 - CrashHour*60.0);
	INT CrashSecond		= appTrunc(CrashTime - CrashHour*3600.0 - CrashMinute*60.0);
	appOutputDebugStringf( TEXT("Time is %d:%02d:%02d since startup.\n"), CrashHour, CrashMinute, CrashSecond );

	// exception handler will dump the callstack
	extern UBOOL GPS3ExceptionHandlerInstalled;
	if (!GPS3ExceptionHandlerInstalled)
	{
		PS3Callstack();
	}
	appSleep(1.0f);
#endif

#if IPHONE
	FString AssertionString = FString::Printf(TEXT("%s(%i): Assertion failed: %s\n%s\n"), ANSI_TO_TCHAR(File), Line, ANSI_TO_TCHAR(Expr), TempStr);

	// Try to flush the log before appCaptureCrashCallStack aborts
	GLog->Flush();

	appCaptureCrashCallStack(*AssertionString, 1.5f, -1);
#endif
}

void VARARGS appFailAssertFuncDebug( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, enum EName Type, const TCHAR* Format/*=TEXT("")*/, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Format, Format );
	appOutputDebugStringf( TEXT("%s(%i): Assertion failed: %s\n%s\n"), ANSI_TO_TCHAR(File), Line, ANSI_TO_TCHAR(Expr), TempStr );

#if _XBOX
	extern void appStopLoggingThread(void);
	appStopLoggingThread();
#endif

#if PS3
	DOUBLE CrashTime	= appSeconds() - GStartTime;
	INT CrashHour		= appTrunc(CrashTime/3600.0);
	INT CrashMinute		= appTrunc(CrashTime/60.0 - CrashHour*60.0);
	INT CrashSecond		= appTrunc(CrashTime - CrashHour*3600.0 - CrashMinute*60.0);
	appOutputDebugStringf( TEXT("Time is %d:%02d:%02d since startup.\n"), CrashHour, CrashMinute, CrashSecond );

	// exception handler will dump the callstack
	extern UBOOL GPS3ExceptionHandlerInstalled;
	if (!GPS3ExceptionHandlerInstalled)
	{
		PS3Callstack();
	}
	appSleep(1.0f);
#endif

#if IPHONE
	FString AssertionString = FString::Printf(TEXT("%s(%i): Assertion failed: %s\n%s\n"), ANSI_TO_TCHAR(File), Line, ANSI_TO_TCHAR(Expr), TempStr);
	appCaptureCrashCallStack(*AssertionString, 1.5f, -1);
#endif
}



/**
 * Called when an 'ensure' assertion fails; gathers stack data and generates and error report.
 *
 * @param	Expr	Code expression ANSI string (#code)
 * @param	File	File name ANSI string (__FILE__)
 * @param	Line	Line number (__LINE__)
 * @param	Msg		Informative error message text
 */
void appFailEnsure( const ANSICHAR* Expr, const ANSICHAR* File, INT Line, const TCHAR* Msg )
{
	// Should we crash here?
	//	- Always crash for asserts on consoles so we can get a crash dump, no free pass there.
	//	- Always crash in debug builds as presumably a programmer wants to fix these ASAP.
	//	- Final release and "shipping" builds always crash for these asserts
	//	- In non-Epic builds always crash as a QA error report server may not be available
#if CONSOLE || _DEBUG || SHIPPING_PC_GAME || FINAL_RELEASE || !EPIC_INTERNAL
	const UBOOL bShouldCrash = TRUE;
#else
	const UBOOL bShouldCrash = FALSE;
#endif
	if( bShouldCrash )
	{
		// Just trigger a regular assertion which will crash via GError->Logf()
		appFailAssert( Expr, File, Line, Msg );
	}
	else
	{
		// Log/report the error and continue application execution

		// Print initial debug message for this error
		appFailAssertFuncDebug( Expr, File, Line, Msg );


		// Is there a debugger attached?  If so we'll just break, otherwise we'll submit an error report.
		if( appIsDebuggerPresent() )
		{
			// ensure() assertion failed.  Break into the attached debugger.  Presumedly, this assertion
			// is handled and you can safely "set next statement" and resume execution if you want.
			// NOTE: In Release builds appDebugBreak currently initiates a crash
			appDebugBreak();
		}
		else
		{
			// No debugger attached, so generate a call stack and submit a crash report
#if !CONSOLE && !PLATFORM_MACOSX // @todo Mac
			// Walk the stack and dump it to the allocated memory.
			const SIZE_T StackTraceSize = 65535;
			ANSICHAR* StackTrace = (ANSICHAR*) appSystemMalloc( StackTraceSize );
			StackTrace[0] = 0;
			appStackWalkAndDump( StackTrace, StackTraceSize, CALLSTACK_IGNOREDEPTH );

			// Create a final string that we'll output to the log (and error history buffer)
			TCHAR ErrorMsg[16384];
			appSprintf( ErrorMsg, TEXT("Assertion failed: %s [File:%s] [Line: %i]") LINE_TERMINATOR TEXT("%s") LINE_TERMINATOR TEXT("Stack: "), ANSI_TO_TCHAR(Expr), ANSI_TO_TCHAR(File), Line, Msg );

			// Also append the stack trace
			appStrncat( ErrorMsg, ANSI_TO_TCHAR(StackTrace), ARRAY_COUNT(ErrorMsg) - 1 );
			appSystemFree( StackTrace );

			// Dump the error and flush the log.
			debugf(TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s"), ErrorMsg);
			GLog->Flush();

			// Submit the error report to the server! (and display a balloon in the system tray)
			if( GIsEpicInternal )
			{
				// Check to see if we've already reported this error.  No point in blasting the server with
				// the same error over and over again in a single application session.
				UBOOL bHasErrorAlreadyBeenReported = FALSE;
				{
					// How many unique previous errors we should keep track of
					const UINT MaxPreviousErrorsToTrack = 4;

					// Static: Array of previous unique error message CRCs
					static DWORD StaticPreviousErrorCRCs[ MaxPreviousErrorsToTrack ];
					static UINT StaticPreviousErrorCount = 0;

					// Compute CRC of error string.  Note that along with the call stack, this includes the message
					// string passed to the macro, so only truly redundant errors will go unreported.  Though it also
					// means you shouldn't pass loop counters to ensureMsgf(), otherwise failures may spam the server!
					DWORD ErrorStrCRC = appStrCrc( ErrorMsg );

					for( UINT CurErrorIndex = 0; CurErrorIndex < StaticPreviousErrorCount; ++CurErrorIndex )
					{
						if( StaticPreviousErrorCRCs[ CurErrorIndex ] == ErrorStrCRC )
						{
							// Found it!  This is a redundant error message.
							bHasErrorAlreadyBeenReported = TRUE;
							break;
						}
					}

					// Not redundant, so add it to our cyclic list
					{
						if( StaticPreviousErrorCount >= MaxPreviousErrorsToTrack )
						{
							// Array is full, so cycle all of the elements down an index
							for( UINT CurErrorIndex = 0; CurErrorIndex < StaticPreviousErrorCount - 1; ++CurErrorIndex )
							{
								StaticPreviousErrorCRCs[ CurErrorIndex ] = StaticPreviousErrorCRCs[ CurErrorIndex + 1 ];
							}

							// Reduce the array count
							--StaticPreviousErrorCount;
						}

						// Add the element to the list and bump the count
						StaticPreviousErrorCRCs[ StaticPreviousErrorCount++ ] = ErrorStrCRC;
					}
				}

				if( !bHasErrorAlreadyBeenReported )
				{
					appSubmitErrorReport( ErrorMsg, EErrorReportMode::Balloon );
				}
			}
#endif
		}
	}
}



//
// Gets the extension of a file, such as "PCX".  Returns NULL if none.
// string if there's no extension.
//
const TCHAR* appFExt( const TCHAR* fname )
{
	if( appStrstr(fname,TEXT(":")) )
	{
		fname = appStrstr(fname,TEXT(":"))+1;
	}

	while( appStrstr(fname,TEXT("/")) )
	{
		fname = appStrstr(fname,TEXT("/"))+1;
	}

	while( appStrstr(fname,TEXT(".")) )
	{
		fname = appStrstr(fname,TEXT("."))+1;
	}

	return fname;
}

//
// Convert an integer to a string.
//
// Faster Itoa that also appends to a string
void appItoaAppend( INT InNum,FString &NumberString )
{
	SQWORD	Num					= InNum; // This avoids having to deal with negating -MAXINT-1
	const TCHAR*	NumberChar[11]		= { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("-") };
	UBOOL	bIsNumberNegative	= FALSE;
	TCHAR	TempNum[16];		// 16 is big enough
	INT		TempAt				= 16; // fill the temp string from the top down.

	// Correctly handle negative numbers and convert to positive integer.
	if( Num < 0 )
	{
		bIsNumberNegative = TRUE;
		Num = -Num;
	}

	TempNum[--TempAt] = 0; // NULL terminator

	// Convert to string assuming base ten and a positive integer.
	do 
	{
		TempNum[--TempAt] = *NumberChar[Num % 10];
		Num /= 10;
	} while( Num );

	// Append sign as we're going to reverse string afterwards.
	if( bIsNumberNegative )
	{
		TempNum[--TempAt] = *NumberChar[10];
	}

	NumberString += TempNum + TempAt;
}

FString appItoa( INT InNum )
{
	FString NumberString;
	appItoaAppend(InNum,NumberString );
	return NumberString;
}


//
// Find string in string, case insensitive, requires non-alphanumeric lead-in.
//
const TCHAR* appStrfind( const TCHAR* Str, const TCHAR* Find )
{
	if( Find == NULL || Str == NULL )
	{
		return NULL;
	}
	UBOOL Alnum  = 0;
	TCHAR f      = (*Find<'a' || *Find>'z') ? (*Find) : (*Find+'A'-'a');
	INT   Length = appStrlen(Find++)-1;
	TCHAR c      = *Str++;
	while( c )
	{
		if( c>='a' && c<='z' )
		{
			c += 'A'-'a';
		}
		if( !Alnum && c==f && !appStrnicmp(Str,Find,Length) )
		{
			return Str-1;
		}
		Alnum = (c>='A' && c<='Z') || (c>='0' && c<='9');
		c = *Str++;
	}
	return NULL;
}

/** 
 * Finds string in string, case insensitive 
 * @param Str The string to look through
 * @param Find The string to find inside Str
 * @return Position in Str if Find was found, otherwise, NULL
 */
const TCHAR* appStristr(const TCHAR* Str, const TCHAR* Find)
{
	// both strings must be valid
	if( Find == NULL || Str == NULL )
	{
		return NULL;
	}
	// get upper-case first letter of the find string (to reduce the number of full strnicmps)
	TCHAR FindInitial = appToUpper(*Find);
	// get length of find string, and increment past first letter
	INT   Length = appStrlen(Find++) - 1;
	// get the first letter of the search string, and increment past it
	TCHAR StrChar = *Str++;
	// while we aren't at end of string...
	while (StrChar)
	{
		// make sure it's upper-case
		StrChar = appToUpper(StrChar);
		// if it matches the first letter of the find string, do a case-insensitive string compare for the length of the find string
		if (StrChar == FindInitial && !appStrnicmp(Str, Find, Length))
		{
			// if we found the string, then return a pointer to the beginning of it in the search string
			return Str-1;
		}
		// go to next letter
		StrChar = *Str++;
	}

	// if nothing was found, return NULL
	return NULL;
}
TCHAR* appStristr(TCHAR* Str, const TCHAR* Find)
{
	return (TCHAR*)appStristr((const TCHAR*)Str, Find);
}

/**
 * Returns a static string that is full of a variable number of characters
 * Since it is static, only one return value from a call is valid at a time.
 *
 * @param NumCharacters Number of characters to put into the string, max of 255
 * @param Char Character to put into the string
 * 
 * @return The string of NumCharacters characters.
 */
const TCHAR* appSpc( INT NumCharacters, BYTE Char )
{
	static const INT MAX_CHARACTERS = 255;

	// static string storage
	static TCHAR StaticString[ MAX_CHARACTERS + 1 ];
	// previous number of chars, used to avoid duplicate work if it didn't change
	static INT OldNum=-1;
	// previous character filling the string, used to avoid duplicate work if it didn't change
	static BYTE OldChar=255;
	
	check( NumCharacters >= 0 );
	check( NumCharacters <= MAX_CHARACTERS );

	// if the character changed, we need to start this string over from scratch
	if (OldChar != Char)
	{
		OldNum = -1;
		OldChar = Char;
	}

	// if the number changed, fill in the array
	if( NumCharacters != OldNum )
	{
		// fill out the array with the characer
		for( OldNum=0; OldNum<NumCharacters; OldNum++ )
		{
			StaticString[OldNum] = Char;
		}
		// null terminate it
		StaticString[NumCharacters] = 0;
	}

	// return the one string
	return StaticString;
}

/**
 * Returns a static string that is full of a variable number of spaces
 * that can be used to space things out, or calculate string widths
 *
 * @param NumSpaces Number of spaces to put into the string, max of 255
 * 
 * @return The string of NumSpaces spaces
 */
const TCHAR* appSpc( INT NumSpaces )
{
	static const INT MAX_SPACES = 255;
	static TCHAR StaticSpaces[ MAX_SPACES + 1 ];
	static UBOOL bFirst = TRUE;

	check( NumSpaces >= 0 );
	check( NumSpaces <= MAX_SPACES );

	if( bFirst )
	{
		for( INT Index = 0; Index < MAX_SPACES; Index++ )
		{
			StaticSpaces[ Index ] = ' ';
		}
		StaticSpaces[ MAX_SPACES ] = 0;
		bFirst = FALSE;
	}
	return &StaticSpaces[ MAX_SPACES - NumSpaces ];
}

// 
// Trim spaces from an ascii string by zeroing them.
//
void appTrimSpaces( ANSICHAR* String )
{		
	// Find 0 terminator.
	INT t=0;
	while( (String[t]!=0 ) && (t< 1024) ) t++;
	if (t>0) t--;
	// Zero trailing spaces.
	while( (String[t]==32) && (t>0) )
	{
		String[t]=0;
		t--;
	}
}

/**
 * Returns a pretty-string for a time given in seconds. (I.e. "4:31 min", "2:16:30 hours", etc)
 * @param Seconds	Time in seconds
 * @return			Time in a pretty formatted string
 */
FString appPrettyTime( DOUBLE Seconds )
{
	if ( Seconds < 1.0 )
	{
		return FString::Printf( TEXT("%d ms"), appTrunc(Seconds*1000) );
	}
	else if ( Seconds < 10.0 )
	{
		INT Sec = appTrunc(Seconds);
		INT Ms = appTrunc(Seconds*1000) - Sec*1000;
		return FString::Printf( TEXT("%d.%02d sec"), Sec, Ms/10 );
	}
	else if ( Seconds < 60.0 )
	{
		INT Sec = appTrunc(Seconds);
		INT Ms = appTrunc(Seconds*1000) - Sec*1000;
		return FString::Printf( TEXT("%d.%d sec"), Sec, Ms/100 );
	}
	else if ( Seconds < 60.0*60.0 )
	{
		INT Min = appTrunc(Seconds/60.0);
		INT Sec = appTrunc(Seconds) - Min*60;
		return FString::Printf( TEXT("%d:%02d min"), Min, Sec );
	}
	else
	{
		INT Hr = appTrunc(Seconds/3600.0);
		INT Min = appTrunc((Seconds - Hr*3600)/60.0);
		INT Sec = appTrunc(Seconds - Hr*3600 - Min*60);
		return FString::Printf( TEXT("%d:%02d:%02d hours"), Hr, Min, Sec );
	}
}


/*-----------------------------------------------------------------------------
	CRC functions.
-----------------------------------------------------------------------------*/

//
// CRC32 computer based on CRC32_POLY.
//
DWORD appMemCrc( const void* InData, INT Length, DWORD CRC )
{
	BYTE* Data = (BYTE*)InData;
	CRC = ~CRC;
	for( INT i=0; i<Length; i++ )
		CRC = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ Data[i]];
	return ~CRC;
}

//
// String CRC.
//
DWORD appStrCrc( const TCHAR* Data )
{
	INT Length = appStrlen( Data );
	DWORD CRC = 0xFFFFFFFF;
	for( INT i=0; i<Length; i++ )
	{
		TCHAR C   = Data[i];
		INT   CL  = (C&255);
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CL];;
		INT   CH  = (C>>8)&255;
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CH];;
	}
	return ~CRC;
}

//
// String CRC, case insensitive.
//
DWORD appStrCrcCaps( const TCHAR* Data )
{
	INT Length = appStrlen( Data );
	DWORD CRC = 0xFFFFFFFF;
	for( INT i=0; i<Length; i++ )
	{
		TCHAR C   = appToUpper(Data[i]);
		INT   CL  = (C&255);
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CL];
		INT   CH  = (C>>8)&255;
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CH];
	}
	return ~CRC;
}

// Ansi String CRC.
//
DWORD appAnsiStrCrc( const char* Data )
{
	INT Length = strlen( Data );
	DWORD CRC = 0xFFFFFFFF;
	for( INT i=0; i<Length; i++ )
	{
		char C   = Data[i];
		INT   CL  = (C&255);
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CL];;
		INT   CH  = (C>>8)&255;
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CH];;
	}
	return ~CRC;
}

//
// Ansi String CRC, case insensitive.
//
DWORD appAnsiStrCrcCaps( const char* Data )
{
	INT Length = strlen( Data );
	DWORD CRC = 0xFFFFFFFF;
	for( INT i=0; i<Length; i++ )
	{
		char C   = toupper(Data[i]);
		INT   CL  = (C&255);
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CL];
		INT   CH  = (C>>8)&255;
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CH];
	}
	return ~CRC;
}

/*-----------------------------------------------------------------------------
	MD5 functions, adapted from MD5 RFC by Brandon Reinhart
-----------------------------------------------------------------------------*/

//
// Constants for MD5 Transform.
//

enum {S11=7};
enum {S12=12};
enum {S13=17};
enum {S14=22};
enum {S21=5};
enum {S22=9};
enum {S23=14};
enum {S24=20};
enum {S31=4};
enum {S32=11};
enum {S33=16};
enum {S34=23};
enum {S41=6};
enum {S42=10};
enum {S43=15};
enum {S44=21};

static BYTE PADDING[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

//
// Basic MD5 transformations.
//
#define MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | (~z)))

//
// Rotates X left N bits.
//
#define ROTLEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

//
// Rounds 1, 2, 3, and 4 MD5 transformations.
// Rotation is separate from addition to prevent recomputation.
//
#define MD5_FF(a, b, c, d, x, s, ac) { \
	(a) += MD5_F ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

#define MD5_GG(a, b, c, d, x, s, ac) { \
	(a) += MD5_G ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

#define MD5_HH(a, b, c, d, x, s, ac) { \
	(a) += MD5_H ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

#define MD5_II(a, b, c, d, x, s, ac) { \
	(a) += MD5_I ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

//
// MD5 initialization.  Begins an MD5 operation, writing a new context.
//
void appMD5Init( FMD5Context* context )
{
	context->count[0] = context->count[1] = 0;
	// Load magic initialization constants.
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

//
// MD5 block update operation.  Continues an MD5 message-digest operation,
// processing another message block, and updating the context.
//
void appMD5Update( FMD5Context* context, const BYTE* input, INT inputLen )
{
	INT i, index, partLen;

	// Compute number of bytes mod 64.
	index = (INT)((context->count[0] >> 3) & 0x3F);

	// Update number of bits.
	if ((context->count[0] += ((DWORD)inputLen << 3)) < ((DWORD)inputLen << 3))
	{
		context->count[1]++;
	}
	context->count[1] += ((DWORD)inputLen >> 29);

	partLen = 64 - index;

	// Transform as many times as possible.
	if (inputLen >= partLen) 
	{
		appMemcpy( &context->buffer[index], input, partLen );
		appMD5Transform( context->state, context->buffer );
		for (i = partLen; i + 63 < inputLen; i += 64)
		{
			appMD5Transform( context->state, &input[i] );
		}
		index = 0;
	}
	else
	{
		i = 0;
	}

	// Buffer remaining input.
	appMemcpy( &context->buffer[index], &input[i], inputLen-i );
}

//
// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
// Digest is 16 BYTEs.
//
void appMD5Final ( BYTE* digest, FMD5Context* context )
{
	BYTE bits[8];
	INT index, padLen;

	// Save number of bits.
	appMD5Encode( bits, context->count, 8 );

	// Pad out to 56 mod 64.
	index = (INT)((context->count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	appMD5Update( context, PADDING, padLen );

	// Append length (before padding).
	appMD5Update( context, bits, 8 );

	// Store state in digest
	appMD5Encode( digest, context->state, 16 );

	// Zeroize sensitive information.
	appMemset( context, 0, sizeof(*context) );
}

//
// MD5 basic transformation. Transforms state based on block.
//
void appMD5Transform( DWORD* state, const BYTE* block )
{
	DWORD a = state[0], b = state[1], c = state[2], d = state[3], x[16];

	appMD5Decode( x, block, 64 );

	// Round 1
	MD5_FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
	MD5_FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
	MD5_FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
	MD5_FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
	MD5_FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
	MD5_FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
	MD5_FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
	MD5_FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
	MD5_FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
	MD5_FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
	MD5_FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
	MD5_FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
	MD5_FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
	MD5_FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
	MD5_FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
	MD5_FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

	// Round 2
	MD5_GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
	MD5_GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
	MD5_GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
	MD5_GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
	MD5_GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
	MD5_GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
	MD5_GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
	MD5_GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
	MD5_GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
	MD5_GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
	MD5_GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
	MD5_GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
	MD5_GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
	MD5_GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
	MD5_GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
	MD5_GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

	// Round 3
	MD5_HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
	MD5_HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
	MD5_HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
	MD5_HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
	MD5_HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
	MD5_HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
	MD5_HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
	MD5_HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
	MD5_HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
	MD5_HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
	MD5_HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
	MD5_HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
	MD5_HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
	MD5_HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
	MD5_HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
	MD5_HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

	// Round 4
	MD5_II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
	MD5_II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
	MD5_II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
	MD5_II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
	MD5_II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
	MD5_II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
	MD5_II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
	MD5_II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
	MD5_II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
	MD5_II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
	MD5_II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
	MD5_II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
	MD5_II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
	MD5_II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
	MD5_II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
	MD5_II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	// Zeroize sensitive information.
	appMemset( x, 0, sizeof(x) );
}

//
// Encodes input (DWORD) into output (BYTE).
// Assumes len is a multiple of 4.
//
void appMD5Encode( BYTE* output, const DWORD* input, INT len )
{
	INT i, j;

	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (BYTE)(input[i] & 0xff);
		output[j+1] = (BYTE)((input[i] >> 8) & 0xff);
		output[j+2] = (BYTE)((input[i] >> 16) & 0xff);
		output[j+3] = (BYTE)((input[i] >> 24) & 0xff);
	}
}

//
// Decodes input (BYTE) into output (DWORD).
// Assumes len is a multiple of 4.
//
void appMD5Decode( DWORD* output, const BYTE* input, INT len )
{
	INT i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
	{
		output[i] = 
			((DWORD)input[j]) | 
			(((DWORD)input[j+1]) << 8) |
			(((DWORD)input[j+2]) << 16) | 
			(((DWORD)input[j+3]) << 24);
	}
}

/*-----------------------------------------------------------------------------
	SHA-1
-----------------------------------------------------------------------------*/

/** Global maps of filename to hash value */
TMap<FString, BYTE*> FSHA1::FullFileSHAHashMap;
TMap<FString, BYTE*> FSHA1::ScriptSHAHashMap;

// Rotate x bits to the left
#ifndef ROL32
#ifdef _MSC_VER
#define ROL32(_val32, _nBits) _rotl(_val32, _nBits)
#else
#define ROL32(_val32, _nBits) (((_val32)<<(_nBits))|((_val32)>>(32-(_nBits))))
#endif
#endif

#if __INTEL_BYTE_ORDER__
	#define SHABLK0(i) (m_block->l[i] = (ROL32(m_block->l[i],24) & 0xFF00FF00) | (ROL32(m_block->l[i],8) & 0x00FF00FF))
#else
	#define SHABLK0(i) (m_block->l[i])
#endif

#define SHABLK(i) (m_block->l[i&15] = ROL32(m_block->l[(i+13)&15] ^ m_block->l[(i+8)&15] \
	^ m_block->l[(i+2)&15] ^ m_block->l[i&15],1))

// SHA-1 rounds
#define _R0(v,w,x,y,z,i) { z+=((w&(x^y))^y)+SHABLK0(i)+0x5A827999+ROL32(v,5); w=ROL32(w,30); }
#define _R1(v,w,x,y,z,i) { z+=((w&(x^y))^y)+SHABLK(i)+0x5A827999+ROL32(v,5); w=ROL32(w,30); }
#define _R2(v,w,x,y,z,i) { z+=(w^x^y)+SHABLK(i)+0x6ED9EBA1+ROL32(v,5); w=ROL32(w,30); }
#define _R3(v,w,x,y,z,i) { z+=(((w|x)&y)|(w&x))+SHABLK(i)+0x8F1BBCDC+ROL32(v,5); w=ROL32(w,30); }
#define _R4(v,w,x,y,z,i) { z+=(w^x^y)+SHABLK(i)+0xCA62C1D6+ROL32(v,5); w=ROL32(w,30); }

FArchive& operator<<( FArchive& Ar, FSHAHash& G )
{
	Ar.Serialize(&G.Hash, sizeof(G.Hash));
	return Ar;
}

FSHA1::FSHA1()
{
	m_block = (SHA1_WORKSPACE_BLOCK *)m_workspace;

	Reset();
}

FSHA1::~FSHA1()
{
	Reset();
}

void FSHA1::Reset()
{
	// SHA1 initialization constants
	m_state[0] = 0x67452301;
	m_state[1] = 0xEFCDAB89;
	m_state[2] = 0x98BADCFE;
	m_state[3] = 0x10325476;
	m_state[4] = 0xC3D2E1F0;

	m_count[0] = 0;
	m_count[1] = 0;
}

void FSHA1::Transform(DWORD *state, const BYTE *buffer)
{
	// Copy state[] to working vars
	DWORD a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

	appMemcpy(m_block, buffer, 64);

	// 4 rounds of 20 operations each. Loop unrolled.
	_R0(a,b,c,d,e, 0); _R0(e,a,b,c,d, 1); _R0(d,e,a,b,c, 2); _R0(c,d,e,a,b, 3);
	_R0(b,c,d,e,a, 4); _R0(a,b,c,d,e, 5); _R0(e,a,b,c,d, 6); _R0(d,e,a,b,c, 7);
	_R0(c,d,e,a,b, 8); _R0(b,c,d,e,a, 9); _R0(a,b,c,d,e,10); _R0(e,a,b,c,d,11);
	_R0(d,e,a,b,c,12); _R0(c,d,e,a,b,13); _R0(b,c,d,e,a,14); _R0(a,b,c,d,e,15);
	_R1(e,a,b,c,d,16); _R1(d,e,a,b,c,17); _R1(c,d,e,a,b,18); _R1(b,c,d,e,a,19);
	_R2(a,b,c,d,e,20); _R2(e,a,b,c,d,21); _R2(d,e,a,b,c,22); _R2(c,d,e,a,b,23);
	_R2(b,c,d,e,a,24); _R2(a,b,c,d,e,25); _R2(e,a,b,c,d,26); _R2(d,e,a,b,c,27);
	_R2(c,d,e,a,b,28); _R2(b,c,d,e,a,29); _R2(a,b,c,d,e,30); _R2(e,a,b,c,d,31);
	_R2(d,e,a,b,c,32); _R2(c,d,e,a,b,33); _R2(b,c,d,e,a,34); _R2(a,b,c,d,e,35);
	_R2(e,a,b,c,d,36); _R2(d,e,a,b,c,37); _R2(c,d,e,a,b,38); _R2(b,c,d,e,a,39);
	_R3(a,b,c,d,e,40); _R3(e,a,b,c,d,41); _R3(d,e,a,b,c,42); _R3(c,d,e,a,b,43);
	_R3(b,c,d,e,a,44); _R3(a,b,c,d,e,45); _R3(e,a,b,c,d,46); _R3(d,e,a,b,c,47);
	_R3(c,d,e,a,b,48); _R3(b,c,d,e,a,49); _R3(a,b,c,d,e,50); _R3(e,a,b,c,d,51);
	_R3(d,e,a,b,c,52); _R3(c,d,e,a,b,53); _R3(b,c,d,e,a,54); _R3(a,b,c,d,e,55);
	_R3(e,a,b,c,d,56); _R3(d,e,a,b,c,57); _R3(c,d,e,a,b,58); _R3(b,c,d,e,a,59);
	_R4(a,b,c,d,e,60); _R4(e,a,b,c,d,61); _R4(d,e,a,b,c,62); _R4(c,d,e,a,b,63);
	_R4(b,c,d,e,a,64); _R4(a,b,c,d,e,65); _R4(e,a,b,c,d,66); _R4(d,e,a,b,c,67);
	_R4(c,d,e,a,b,68); _R4(b,c,d,e,a,69); _R4(a,b,c,d,e,70); _R4(e,a,b,c,d,71);
	_R4(d,e,a,b,c,72); _R4(c,d,e,a,b,73); _R4(b,c,d,e,a,74); _R4(a,b,c,d,e,75);
	_R4(e,a,b,c,d,76); _R4(d,e,a,b,c,77); _R4(c,d,e,a,b,78); _R4(b,c,d,e,a,79);

	// Add the working vars back into state
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

// Use this function to hash in binary data and strings
void FSHA1::Update(const BYTE *data, DWORD len)
{
	DWORD i, j;

	j = (m_count[0] >> 3) & 63;

	if((m_count[0] += len << 3) < (len << 3)) m_count[1]++;

	m_count[1] += (len >> 29);

	if((j + len) > 63)
	{
		i = 64 - j;
		appMemcpy(&m_buffer[j], data, i);
		Transform(m_state, m_buffer);

		for( ; i + 63 < len; i += 64) Transform(m_state, &data[i]);

		j = 0;
	}
	else i = 0;

	appMemcpy(&m_buffer[j], &data[i], len - i);
}

void FSHA1::Final()
{
	DWORD i;
	BYTE finalcount[8];

	for(i = 0; i < 8; i++)
	{
		finalcount[i] = (BYTE)((m_count[((i >= 4) ? 0 : 1)] >> ((3 - (i & 3)) * 8) ) & 255); // Endian independent
	}

	Update((BYTE*)"\200", 1);

	while ((m_count[0] & 504) != 448)
	{
		Update((BYTE*)"\0", 1);
	}

	Update(finalcount, 8); // Cause a SHA1Transform()

	for(i = 0; i < 20; i++)
	{
		m_digest[i] = (BYTE)((m_state[i >> 2] >> ((3 - (i & 3)) * 8) ) & 255);
	}
}

// Get the raw message digest
void FSHA1::GetHash(BYTE *puDest)
{
	appMemcpy(puDest, m_digest, 20);
}

/**
* Calculate the hash on a single block and return it
*
* @param Data Input data to hash
* @param DataSize Size of the Data block
* @param OutHash Resulting hash value (20 byte buffer)
*/
void FSHA1::HashBuffer(const void* Data, DWORD DataSize, BYTE* OutHash)
{
	// do an atomic hash operation
	FSHA1 Sha;
	Sha.Update((const BYTE*)Data, DataSize);
	Sha.Final();
	Sha.GetHash(OutHash);
}


/**
 * Shared hashes.sha reading code (each platform gets a buffer to the data,
 * then passes it to this function for processing)
 */
void FSHA1::InitializeFileHashesFromBuffer(BYTE* Buffer, INT BufferSize, UBOOL bDuplicateKeyMemory)
{
	// the start of the file is full file hashes
	UBOOL bIsDoingFullFileHashes = TRUE;
	// if it exists, parse it
	INT Offset = 0;
	while (Offset < BufferSize)
	{
		// format is null terminated string followed by hash
		ANSICHAR* Filename = (ANSICHAR*)Buffer + Offset;

		// make sure it's not an empty string (this could happen with an empty hash file)
		if (Filename[0])
		{
			// skip over the file
			Offset += strlen(Filename) + 1;

			// if we hit the magic separator between sections
			if (strcmp(Filename, HASHES_SHA_DIVIDER) == 0)
			{
				// switch to script sha
				bIsDoingFullFileHashes = FALSE;

				// don't process a hash for this special case
				continue;
			}

			// duplicate the memory if needed (some hash sources are always loaded, ie in the executable,
			// so no need to duplicate that memory, we can just point into the middle of it)
			BYTE* Hash;
			if (bDuplicateKeyMemory)
			{
				Hash = (BYTE*)appMalloc(20);
				appMemcpy(Hash, Buffer + Offset, 20);
			}
			else
			{
				Hash = Buffer + Offset;
			}

			// offset now points to the hash data, so save a pointer to it
			(bIsDoingFullFileHashes ? FullFileSHAHashMap : ScriptSHAHashMap).Set(ANSI_TO_TCHAR(Filename), Hash);

			// move the offset over the hash (always 20 bytes)
			Offset += 20;
		}
	}

	// we should be exactly at the end
	check(Offset == BufferSize);

}


/**
 * Gets the stored SHA hash from the platform, if it exists. This function
 * must be able to be called from any thread.
 *
 * @param Pathname Pathname to the file to get the SHA for
 * @param Hash 20 byte array that receives the hash
 * @param bIsFullPackageHash TRUE if we are looking for a full package hash, instead of a script code only hash
 *
 * @return TRUE if the hash was found, FALSE otherwise
 */
UBOOL FSHA1::GetFileSHAHash(const TCHAR* Pathname, BYTE Hash[20], UBOOL bIsFullPackageHash)
{
	// look for this file in the hash
	BYTE** HashData = (bIsFullPackageHash ? FullFileSHAHashMap : ScriptSHAHashMap).Find(FFilename(Pathname).GetCleanFilename().ToLower());

	// do we want a copy?
	if (HashData && Hash)
	{
		// return it
		appMemcpy(Hash, *HashData, 20);
	}

	// return TRUE if we found the hash
	return HashData != NULL;
}
/*-----------------------------------------------------------------------------
	Exceptions.
-----------------------------------------------------------------------------*/

//
// Throw a string exception with a message.
//
VARARG_BODY( void VARARGS, appThrowf, const TCHAR*, VARARG_NONE )
{
	static TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
#if EXCEPTIONS_DISABLED // @todo hack
	debugf(TEXT("THROW: %s"), TempStr);
	#if PS3
		PS3Callstack();
	#endif
	appDebugBreak();
#else
	throw( TempStr );
#endif
}

/**
 * Raises an OS exception. Normally used for critical errors that forces UE3 to shutdown.
 * These can be caught by __try/__except in MS unmanaged C++ environments, and
 * try/catch(Exception) in MS managed C++ environments.
 *
 * @ExceptionCode	Application-specific code that is passed to the exception handler.
 */
void appRaiseException( DWORD ExceptionCode )
{
#if _WINDOWS || XBOX
	RaiseException( ExceptionCode, 0, 0, NULL );
#elif EXCEPTIONS_DISABLED // @todo hack
	debugf(TEXT("EXCEPTION: %d"), ExceptionCode);
	#if PS3
		PS3Callstack();
	#endif
	appDebugBreak();
#else
	throw( ExceptionCode );
#endif
}

/*-----------------------------------------------------------------------------
	Parameter parsing.
-----------------------------------------------------------------------------*/

//
// Get a string from a text string.
//
UBOOL Parse
(
	const TCHAR* Stream, 
	const TCHAR* Match,
	TCHAR*		 Value,
	INT			 MaxLen,
	UBOOL		 bShouldStopOnComma
)
{
	const TCHAR* Found = appStrfind(Stream,Match);
	const TCHAR* Start;

	if( Found )
	{
		Start = Found + appStrlen(Match);
		if( *Start == '\x22' )
		{
			// Quoted string with spaces.
			appStrncpy( Value, Start+1, MaxLen );
			Value[MaxLen-1]=0;
			TCHAR* Temp = appStrstr( Value, TEXT("\x22") );
			if( Temp != NULL )
				*Temp=0;
		}
		else
		{
			// Non-quoted string without spaces.
			appStrncpy( Value, Start, MaxLen );
			Value[MaxLen-1]=0;
			TCHAR* Temp;
			Temp = appStrstr( Value, TEXT(" ")  ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT("\r") ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT("\n") ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT("\t") ); if( Temp ) *Temp=0;
			if (bShouldStopOnComma)
			{
				Temp = appStrstr( Value, TEXT(",")  ); if( Temp ) *Temp=0;
			}
		}
		return 1;
	}
	else return 0;
}

//
// Checks if a command-line parameter exists in the stream.
//
UBOOL ParseParam( const TCHAR* Stream, const TCHAR* Param, UBOOL bAllowQuoted )
{
	const TCHAR* Start = Stream;
	if( *Stream )
	{
		while( (Start = appStrfind(Start + 1,Param)) != NULL )
		{
			if( Start > Stream && (Start[-1] == '-' || Start[-1] == '/') )
			{
				const TCHAR* End = Start + appStrlen(Param);
				if ( End == NULL || *End == 0 || appIsWhitespace(*End) )
				{
                    return TRUE;
				}

				if( bAllowQuoted )
				{
					if( Start[-2] == '\"' && *End == '\"' )
					{
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

// 
// Parse a string.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FString& Value, UBOOL bShouldStopOnComma )
{
	TCHAR Temp[4096]=TEXT("");
	if( ::Parse( Stream, Match, Temp, ARRAY_COUNT(Temp), bShouldStopOnComma ) )
	{
		Value = Temp;
		return 1;
	}
	else return 0;
}

//
// Parse a quadword.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, QWORD& Value )
{
	return Parse( Stream, Match, *(SQWORD*)&Value );
}

//
// Parse a signed quadword.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SQWORD& Value )
{
	TCHAR Temp[4096]=TEXT(""), *Ptr=Temp;
	if( ::Parse( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
	{
		Value = 0;
		UBOOL Negative = (*Ptr=='-');
		Ptr += Negative;
		while( *Ptr>='0' && *Ptr<='9' )
			Value = Value*10 + *Ptr++ - '0';
		if( Negative )
			Value = -Value;
		return 1;
	}
	else return 0;
}

//
// Get an object from a text stream.
//
UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent )
{
	TCHAR TempStr[1024];
	if( !Parse( Stream, Match, TempStr, ARRAY_COUNT(TempStr) ) )
	{
		return 0;
	}
	else if( appStricmp(TempStr,TEXT("NONE"))==0 )
	{
		DestRes = NULL;
		return 1;
	}
	else
	{
		// Look this object up.
		UObject* Res;
		Res = UObject::StaticFindObject( Class, InParent, TempStr );
		if( !Res )
			return 0;
		DestRes = Res;
		return 1;
	}
}

//
// Get a name.
//
UBOOL Parse
(
	const TCHAR* Stream, 
	const TCHAR* Match, 
	FName& Name
)
{
	TCHAR TempStr[NAME_SIZE];

	if( !Parse(Stream,Match,TempStr,NAME_SIZE) )
		return 0;

	Name = FName(TempStr);

	return 1;
}

//
// Get a DWORD.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, DWORD& Value )
{
	const TCHAR* Temp = appStrfind(Stream,Match);
	TCHAR* End;
	if( Temp==NULL )
		return 0;
	Value = appStrtoi( Temp + appStrlen(Match), &End, 10 );

	return 1;
}

//
// Get a byte.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, BYTE& Value )
{
	const TCHAR* Temp = appStrfind(Stream,Match);
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = (BYTE)appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
}

//
// Get a signed byte.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SBYTE& Value )
{
	const TCHAR* Temp = appStrfind(Stream,Match);
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
}

//
// Get a word.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, WORD& Value )
{
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = (WORD)appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
}

//
// Get a signed word.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SWORD& Value )
{
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = (SWORD)appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
}

//
// Get a floating-point number.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FLOAT& Value )
{
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Value = appAtof( Temp+appStrlen(Match) );
	return 1;
}

//
// Get a signed double word.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, INT& Value )
{
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Value = appAtoi( Temp + appStrlen(Match) );
	return 1;
}

//
// Get a boolean value.
//
UBOOL ParseUBOOL( const TCHAR* Stream, const TCHAR* Match, UBOOL& OnOff )
{
	TCHAR TempStr[16];
	if( Parse( Stream, Match, TempStr, 16 ) )
	{
		OnOff
		=	!appStricmp(TempStr,TEXT("On"))
		||	!appStricmp(TempStr,TEXT("True"))
		||	!appStricmp(TempStr,GTrue)
		||	!appStricmp(TempStr,TEXT("1"));
		return 1;
	}
	else return 0;
}

//
// Get a globally unique identifier.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FGuid& Guid )
{
	TCHAR Temp[256];
	if( !Parse( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
		return 0;

	Guid.A = Guid.B = Guid.C = Guid.D = 0;
	if( appStrlen(Temp)==32 )
	{
		TCHAR* End;
		Guid.D = appStrtoi( Temp+24, &End, 16 ); Temp[24]=0;
		Guid.C = appStrtoi( Temp+16, &End, 16 ); Temp[16]=0;
		Guid.B = appStrtoi( Temp+8,  &End, 16 ); Temp[8 ]=0;
		Guid.A = appStrtoi( Temp+0,  &End, 16 ); Temp[0 ]=0;
	}
	return 1;
}


#if !FINAL_RELEASE
/**
 * Needed for the console command "DumpConsoleCommands"
 * How it works:
 *   - GConsoleCommandLibrary is set to point at a local instance of ConsoleCommandLibrary
 *   - a dummy command search is triggered which gathers all commands in a hashed set
 *   - sort all gathered commands in human friendly way
 *   - log all commands
 *   - GConsoleCommandLibrary is set 0
 */
class ConsoleCommandLibrary
{
public:
	ConsoleCommandLibrary(const FString& InPattern);

	~ConsoleCommandLibrary();

	void OnParseCommand(const TCHAR* Match)
	{
		// -1 to not take the "*" after the pattern into account
		if(appStrnicmp(Match, *Pattern, Pattern.Len() - 1) == 0)
		{
			KnownNames.Add(Match);
		}
	}

private:

	TSet<FString>		KnownNames;
	const FString&		Pattern;

	friend void ConsoleCommandLibrary_DumpLibrary(FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar);
};

// 0 if gathering of names is deactivated
ConsoleCommandLibrary* GConsoleCommandLibrary;

ConsoleCommandLibrary::ConsoleCommandLibrary(const FString& InPattern) :Pattern(InPattern)
{
	// activate name gathering
	GConsoleCommandLibrary = this;
}

ConsoleCommandLibrary::~ConsoleCommandLibrary()
{
	// deactivate name gathering
	GConsoleCommandLibrary = 0;
}

/**
 * Helper struct to sort console command names
 */
struct FriendlyNameStringSorter
{
	static INT Compare(const FString& A, const FString& B)
	{
		return appStricmp(*A, *B);
	}
};

/** Needed for the console command "DumpConsoleCommands" */
void ConsoleCommandLibrary_DumpLibrary(FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar)
{
	ConsoleCommandLibrary LocalConsoleCommandLibrary(Pattern);

	FOutputDeviceNull Null;

	UBOOL bExecuted = SubSystem.Exec(*Pattern, Null);

	LocalConsoleCommandLibrary.KnownNames.Sort<FriendlyNameStringSorter>();

	for(TSet<FString>::TConstIterator It(LocalConsoleCommandLibrary.KnownNames); It; ++It)
	{
		const FString Name = *It;

		Ar.Logf(TEXT("%s"), *Name);
	}
	Ar.Logf(TEXT(""));

	// the pattern (e.g. Motion*) should not really trigger the execution
	if(bExecuted)
	{
		Ar.Logf(TEXT("ERROR: The function was supposed to only find matching commands but not have any side effect."));
		Ar.Logf(TEXT("However Exec() returned TRUE which means we either executed a command or the command parsing returned TRUE where it shouldn't."));
	}
}
#endif // !FINAL_RELEASE


//
// Sees if Stream starts with the named command.  If it does,
// skips through the command and blanks past it.  Returns 1 of match,
// 0 if not.
//
UBOOL ParseCommand
(
	const TCHAR** Stream, 
	const TCHAR*  Match,
	UBOOL bParseMightTriggerExecution
)
{
#if !FINAL_RELEASE
	if(GConsoleCommandLibrary)
	{
		GConsoleCommandLibrary->OnParseCommand(Match);
		
		if(bParseMightTriggerExecution)
		{
			// Better we fail the test - we only wanted to find all commands.
			return FALSE;
		}
	}
#endif // !FINAL_RELEASE

	while( (**Stream==' ')||(**Stream==9) )
		(*Stream)++;

	if( appStrnicmp(*Stream,Match,appStrlen(Match))==0 )
	{
		*Stream += appStrlen(Match);
		if( !appIsAlnum(**Stream))
		{
			while ((**Stream==' ')||(**Stream==9)) (*Stream)++;
			return 1; // Success.
		}
		else
		{
			*Stream -= appStrlen(Match);
			return 0; // Only found partial match.
		}
	}
	else return 0; // No match.
}

//
// Get next command.  Skips past comments and cr's.
//
void ParseNext( const TCHAR** Stream )
{
	// Skip over spaces, tabs, cr's, and linefeeds.
	SkipJunk:
	while( **Stream==' ' || **Stream==9 || **Stream==13 || **Stream==10 )
		++*Stream;

	if( **Stream==';' )
	{
		// Skip past comments.
		while( **Stream!=0 && **Stream!=10 && **Stream!=13 )
			++*Stream;
		goto SkipJunk;
	}

	// Upon exit, *Stream either points to valid Stream or a nul.
}

//
// Grab the next space-delimited string from the input stream.
// If quoted, gets entire quoted string.
//
UBOOL ParseToken( const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape )
{
	INT Len=0;

	// Skip preceeding spaces and tabs.
	while( appIsWhitespace(*Str) )
	{
		Str++;
	}

	if( *Str == TEXT('"') )
	{
		// Get quoted string.
		Str++;
		while( *Str && *Str!=TEXT('"') && (Len+1)<MaxLen )
		{
			TCHAR c = *Str++;
			if( c=='\\' && UseEscape )
			{
				// Get escape.
				c = *Str++;
				if( !c )
				{
					break;
				}
			}
			if( (Len+1)<MaxLen )
			{
				Result[Len++] = c;
			}
		}
		if( *Str==TEXT('"') )
		{
			Str++;
		}
	}
	else
	{
		// Get unquoted string (that might contain a quoted part, which will be left intact).
		// For example, -ARG="foo bar baz", will be treated as one token, with quotes intact
		UBOOL bInQuote = FALSE;

		while (1)
		{
			TCHAR Character = *Str;
			if ((Character == 0) || (appIsWhitespace(Character) && !bInQuote))
			{
				break;
			}
			Str++;

			// Preserve escapes if they're in a quoted string (the check for " is in the else to let \" work as expected)
			if (Character == TEXT('\\') && UseEscape && bInQuote)
			{
				if ((Len+1) < MaxLen)
				{
					Result[Len++] = Character;
				}

				Character = *Str;
				if (!Character)
				{
					break;
				}
				Str++;
			}
			else if (Character == TEXT('"'))
			{
				bInQuote = !bInQuote;
			}

			if( (Len+1)<MaxLen )
			{
				Result[Len++] = Character;
			}
		}
	}
	Result[Len]=0;
	return Len!=0;
}

UBOOL ParseToken( const TCHAR*& Str, FString& Arg, UBOOL UseEscape )
{
	Arg.Empty();

	// Skip preceeding spaces and tabs.
	while( appIsWhitespace(*Str) )
	{
		Str++;
	}

	if ( *Str == TEXT('"') )
	{
		// Get quoted string.
		Str++;
		while( *Str && *Str != TCHAR('"') )
		{
			TCHAR c = *Str++;
			if( c==TEXT('\\') && UseEscape )
			{
				// Get escape.
				c = *Str++;
				if( !c )
				{
					break;
				}
			}

			Arg += c;
		}

		if ( *Str == TEXT('"') )
		{
			Str++;
		}
	}
	else
	{
		// Get unquoted string (that might contain a quoted part, which will be left intact).
		// For example, -ARG="foo bar baz", will be treated as one token, with quotes intact
		UBOOL bInQuote = FALSE;

		while (1)
		{
			TCHAR Character = *Str;
			if ((Character == 0) || (appIsWhitespace(Character) && !bInQuote))
			{
				break;
			}
			Str++;

			// Preserve escapes if they're in a quoted string (the check for " is in the else to let \" work as expected)
			if (Character == TEXT('\\') && UseEscape && bInQuote)
			{
				Arg += Character;

				Character = *Str;
				if (!Character)
				{
					break;
				}
				Str++;
			}
			else if (Character == TEXT('"'))
			{
				bInQuote = !bInQuote;
			}

			Arg += Character;
		}
	}

	return Arg.Len() > 0;
}
FString ParseToken( const TCHAR*& Str, UBOOL UseEscape )
{
	TCHAR Buffer[1024];
	if( ParseToken( Str, Buffer, ARRAY_COUNT(Buffer), UseEscape ) )
		return Buffer;
	else
		return TEXT("");
}

//
// Get a line of Stream (everything up to, but not including, CR/LF.
// Returns 0 if ok, nonzero if at end of stream and returned 0-length string.
//
UBOOL ParseLine
(
	const TCHAR**	Stream,
	TCHAR*			Result,
	INT				MaxLen,
	UBOOL			Exact
)
{
	UBOOL GotStream=0;
	UBOOL IsQuoted=0;
	UBOOL Ignore=0;

	*Result=0;
	while( **Stream!=0 && **Stream!=10 && **Stream!=13 && --MaxLen>0 )
	{
		// Start of comments.
		if( !IsQuoted && !Exact && (*Stream)[0]=='/' && (*Stream)[1]=='/' )
			Ignore = 1;
		
		// Command chaining.
		if( !IsQuoted && !Exact && **Stream=='|' )
			break;

		// Check quoting.
		IsQuoted = IsQuoted ^ (**Stream==34);
		GotStream=1;

		// Got stuff.
		if( !Ignore )
			*(Result++) = *((*Stream)++);
		else
			(*Stream)++;
	}
	if( Exact )
	{
		// Eat up exactly one CR/LF.
		if( **Stream == 13 )
			(*Stream)++;
		if( **Stream == 10 )
			(*Stream)++;
	}
	else
	{
		// Eat up all CR/LF's.
		while( **Stream==10 || **Stream==13 || **Stream=='|' )
			(*Stream)++;
	}
	*Result=0;
	return **Stream!=0 || GotStream;
}
UBOOL ParseLine
(
	const TCHAR**	Stream,
	FString&		Result,
	UBOOL			Exact
)
{
	UBOOL GotStream=0;
	UBOOL IsQuoted=0;
	UBOOL Ignore=0;

	Result = TEXT("");

	while( **Stream!=0 && **Stream!=10 && **Stream!=13 )
	{
		// Start of comments.
		if( !IsQuoted && !Exact && (*Stream)[0]=='/' && (*Stream)[1]=='/' )
			Ignore = 1;

		// Command chaining.
		if( !IsQuoted && !Exact && **Stream=='|' )
			break;

		// Check quoting.
		IsQuoted = IsQuoted ^ (**Stream==34);
		GotStream=1;

		// Got stuff.
		if( !Ignore )
		{
			Result.AppendChar( *((*Stream)++) );
		}
		else
		{
			(*Stream)++;
		}
	}
	if( Exact )
	{
		// Eat up exactly one CR/LF.
		if( **Stream == 13 )
			(*Stream)++;
		if( **Stream == 10 )
			(*Stream)++;
	}
	else
	{
		// Eat up all CR/LF's.
		while( **Stream==10 || **Stream==13 || **Stream=='|' )
			(*Stream)++;
	}

	return **Stream!=0 || GotStream;
}

/*----------------------------------------------------------------------------
	Localization.
----------------------------------------------------------------------------*/

/**
 *	Retrieve the index for the given language extension.
 *	
 *	@param	Ext		The extension to search for
 *
 *	@return	INT		The index of the language; -1 if not found.
 */
INT Localization_GetLanguageExtensionIndex(const TCHAR* Ext)
{
	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	check(Ext);
    for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
	{
		const FString& CheckExt = KnownLanguageExtensions(LangIndex);
		if ( CheckExt == Ext )
		{
			return LangIndex;
		}
	}
	return -1;
}

FString LocalizeLabel( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL Optional )
{
	return Localize(Section,Key,Package,LangExt,Optional)+TEXT(":");
}

FString Localize( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL bOptional )
{
	// Errors during early loading can cause Localize to be called before GConfig is initialized.
	if( !GIsStarted || !GConfig || !GSys )
	{
		return Key;
	}

	// The default behaviour for Localize is to use the configured language which is indicated by passing in NULL.
	if( LangExt == NULL )
	{
		LangExt = UObject::GetLanguage();
	}

	FString Result;
	UBOOL	bFoundMatch = FALSE;

	// We allow various .inis to contribute multiple paths for localization files.
	for( INT PathIndex=GSys->LocalizationPaths.Num()-1; PathIndex>=0; PathIndex-- )
	{
		// Try specified language first
		FFilename FilenameLang	= FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s") PATH_SEPARATOR TEXT("%s.%s"), *GSys->LocalizationPaths(PathIndex), LangExt	  , Package, LangExt	 );
		if ( GConfig->GetString( Section, Key, Result, *FilenameLang ) )
		{
			// found it in the localized language file
			bFoundMatch = TRUE;
			break;
		}
	}

	if ( !bFoundMatch && appStricmp(LangExt, TEXT("INT")) )
	{
		// if we haven't found it yet, fall back to default (int) and see if it exists there.
		for( INT PathIndex=GSys->LocalizationPaths.Num()-1; PathIndex>=0; PathIndex-- )
		{
			FFilename FilenameInt	= FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s") PATH_SEPARATOR TEXT("%s.%s"), *GSys->LocalizationPaths(PathIndex), TEXT("INT"), Package, TEXT("INT") );
			if ( GConfig->GetString( Section, Key, Result, *FilenameInt ) )
			{
				bFoundMatch = TRUE;
				break;
			}
		}

		if ( bFoundMatch )
		{
			static UBOOL bShowMissingLoc = ParseParam(appCmdLine(), TEXT("SHOWMISSINGLOC"));
			if ( bShowMissingLoc )
			{
				// the value was not found in the loc file for the current language, but was found in the english files
				// if we want to see the location for missing localized text, return the error string instead of the english text
				bFoundMatch = FALSE;
				bOptional = FALSE;
			}
		}
	}

	// Complain about missing localization for non optional queries.
	if( !bFoundMatch && !bOptional )
	{
		warnf( NAME_LocalizationWarning, TEXT("No localization: %s.%s.%s (%s)"), Package, Section, Key, LangExt );
		Result = FString::Printf( TEXT("?%s?%s.%s.%s?"), LangExt, Package, Section, Key );
	}

	// if we have any \n's in the text file, replace them with real line feeds
	// (when read in from a .int file, the \n's are converted to \\n)

	// this is now handled by the FConfigCacheIni when reading in files
// 	if (Result.InStr(TEXT("\\n")) != -1)
// 	{
// 		Result= Result.Replace(TEXT("\\n"), TEXT("\n"));
// 	}
// 
// 	// repeat for \t's
// 	if (Result.InStr(TEXT("\\t")) != -1)
// 	{
// 		Result= Result.Replace(TEXT("\\t"), TEXT("\t"));
// 	}

	// Use "###" as a comment token.
	Result.Split( TEXT("###"), &Result, NULL );

	return Result;
}
FString LocalizeError( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("Errors"), Key, Package, LangExt );
}
FString LocalizeProgress( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("Progress"), Key, Package, LangExt );
}
FString LocalizeQuery( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("Query"), Key, Package, LangExt );
}
FString LocalizeGeneral( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("General"), Key, Package, LangExt );
}
FString LocalizeUnrealEd( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("UnrealEd"), Key, Package, LangExt );
}
FString LocalizeProperty( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( Section, Key, Package, LangExt, TRUE );
}
FString LocalizePropertyPath( const TCHAR* PackageSectionKey, const TCHAR* LangExt )
{
	FString Result = PackageSectionKey;
	if (Result != TEXT(""))
	{
		TArray<FString> Pieces;
		Result.ParseIntoArray(&Pieces, TEXT("."), TRUE);
		if (Pieces.Num() >= 3)
		{
			Result = Localize(*Pieces(1), *Pieces(2), *Pieces(0), NULL, TRUE);
		}
	}
	return Result;
}

#if !TCHAR_IS_1_BYTE
FString LocalizeLabel( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL Optional )
{
	return LocalizeLabel( ANSI_TO_TCHAR(Section), ANSI_TO_TCHAR(Key), Package, LangExt, Optional );
}
FString Localize( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL Optional )
{
	return Localize( ANSI_TO_TCHAR(Section), ANSI_TO_TCHAR(Key), Package, LangExt, Optional );
}
FString LocalizeError( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeError( ANSI_TO_TCHAR(Key), Package, LangExt );
}
FString LocalizeProgress( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeProgress( ANSI_TO_TCHAR(Key), Package, LangExt );
}
FString LocalizeQuery( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeQuery( ANSI_TO_TCHAR(Key), Package, LangExt );
}
FString LocalizeGeneral( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeGeneral( ANSI_TO_TCHAR(Key), Package, LangExt );
}
FString LocalizeUnrealEd( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeUnrealEd( ANSI_TO_TCHAR(Key), Package, LangExt );
}
FString LocalizeProperty( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeProperty( ANSI_TO_TCHAR(Section), ANSI_TO_TCHAR(Key), Package, LangExt );
}
FString LocalizePropertyPath( const ANSICHAR* PackageSectionKey, const TCHAR* LangExt )
{
	return LocalizePropertyPath( ANSI_TO_TCHAR(PackageSectionKey), LangExt );
}
#endif

/*-----------------------------------------------------------------------------
	High level file functions.
-----------------------------------------------------------------------------*/

//
// Load a binary file to a dynamic array.
//
UBOOL appLoadFileToArray( TArray<BYTE>& Result, const TCHAR* Filename, FFileManager* FileManager, DWORD Flags )
{
	FStringOutputDevice ErrorString;
	FArchive* Reader = FileManager->CreateFileReader( Filename, Flags, &ErrorString );
	if( !Reader )
	{
		if (!(Flags & FILEREAD_Silent))
		{
			debugf(NAME_Error,TEXT("Failed to read file '%s' error (%s)"),Filename,*ErrorString);
		}
		return 0;
	}
	Result.Reset();
	Result.Add( Reader->TotalSize() );
	Reader->Serialize( &Result(0), Result.Num() );
	UBOOL Success = Reader->Close();
	delete Reader;
	return Success;
}

//
// Converts an arbitrary text buffer to an FString.
// Supports all combination of ANSI/Unicode files and platforms.
//
void appBufferToString( FString& Result, const BYTE* Buffer, INT Size )
{
	TArray<TCHAR>& ResultArray = Result.GetCharArray();
	ResultArray.Empty();

	if( Size >= 2 && !( Size & 1 ) && Buffer[0] == 0xff && Buffer[1] == 0xfe )
	{
		// Unicode Intel byte order. Less 1 for the FFFE header, additional 1 for null terminator.
		ResultArray.Add( Size / 2 );
		for( INT i = 0; i < ( Size / 2 ) - 1; i++ )
		{
			ResultArray( i ) = FromUnicode( ( WORD )Buffer[i * 2 + 2] + ( WORD )Buffer[i * 2 + 3] * 256 );
		}
		// Ensure null terminator is present
		ResultArray.Last() = 0;
	}
	else if( Size >= 2 && !( Size & 1 ) && Buffer[0] == 0xfe && Buffer[1] == 0xff )
	{
		// Unicode non-Intel byte order. Less 1 for the FFFE header, additional 1 for null terminator.
		ResultArray.Add( Size / 2 );
		for( INT i = 0; i < ( Size / 2 ) - 1; i++ )
		{
			ResultArray( i ) = FromUnicode( ( WORD )Buffer[i * 2 + 3] + ( WORD )Buffer[i * 2 + 2] * 256 );
		}
		// Ensure null terminator is present
		ResultArray.Last() = 0;
	}
	else
	{
		// ANSI. Additional 1 for null terminator.
		ResultArray.Add( Size + 1 );

#if !CONSOLE && defined(_MSC_VER)
		// We cannot use FANSIToTCHAR_Convert in this case because our source data is not null terminated.
		// Convert and add null terminator.
		INT res = MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,(LPCSTR)Buffer,Size,&ResultArray(0),ResultArray.Num());

		if( res > 0 )
		{
			ResultArray(res) = 0;

			// Reallocate the FString to match the translated data size.
			if( Size != res )
			{
				ResultArray.Remove( res + 1, Size - res );
			}
		}
		else
		// Our buffer is always big enough, so GetLastError() is expected to always be ERROR_NO_UNICODE_TRANSLATION
		// If the conversion failed, we will just use the raw data (old behavior)
#endif
		{
			for( INT i = 0; i < Size; i++ )
			{
				ResultArray( i ) = FromAnsi( Buffer[i] );
			}
			// Ensure null terminator is present
			ResultArray.Last() = 0;
		}
	}
}

//
// Load a text file to an FString.
// Supports all combination of ANSI/Unicode files and platforms.
//
UBOOL appLoadFileToString( FString& Result, const TCHAR* Filename, FFileManager* FileManager, DWORD VerifyFlags, DWORD ReadFlags )
{
	FArchive* Reader = FileManager->CreateFileReader( Filename, ReadFlags );
	if( !Reader )
	{
		return 0;
	}
	
	INT Size = Reader->TotalSize();
	BYTE* Ch = (BYTE*)appMalloc(Size);
	Reader->Serialize( Ch, Size );
	UBOOL Success = Reader->Close();
	delete Reader;
	appBufferToString( Result, Ch, Size );

	// handle SHA verify of the file
	if( VerifyFlags & LoadFileHash_EnableVerify )
	{
		if( (VerifyFlags & LoadFileHash_ErrorMissingHash) || FSHA1::GetFileSHAHash(Filename, NULL) )
		{
			// kick off SHA verify task. this frees the buffer on close
			FBufferReaderWithSHA Ar( Ch, Size, TRUE, Filename, FALSE, TRUE );
		}
	}
	else
	{
		// free manually since not running SHA task
		appFree(Ch);
	}

	return Success;
}

#if WITH_EDITOR
/**
 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
 *	Intended for use in simple text parsing actions
 *
 *	@param	InFilename			The text file to read, full path
 *	@param	InFileManager		The filemanager to use - NULL will use GFileManager
 *	@param	OutStrings			The array of FStrings to fill in
 *
 *	@return	UBOOL				TRUE if successful, FALSE if not
 */
UBOOL appLoadANSITextFileToStrings(const TCHAR* InFilename, FFileManager* InFileManager, TArray<FString>& OutStrings)
{
	FFileManager* FileManager = (InFileManager != NULL) ? InFileManager : GFileManager;
	// Read and parse the file, adding the pawns and their sounds to the list
	FArchive* TextFile = FileManager->CreateFileReader(InFilename, 0, GLog);
	if (TextFile != NULL)
	{
		// get the size of the file
		INT Size = TextFile->TotalSize();
		// read the file
		TArray<BYTE> Buffer;
		Buffer.Empty(Size);
		Buffer.Add(Size);
		TextFile->Serialize(Buffer.GetData(), Size);
		// zero terminate it
		Buffer.AddItem(0);
		// Release the file
		delete TextFile;

		// Now read it
		// init traveling pointer
		ANSICHAR* Ptr = (ANSICHAR*)Buffer.GetData();

		// iterate over the lines until complete
		UBOOL bIsDone = FALSE;
		while (!bIsDone)
		{
			// Advance past new line characters
			while (*Ptr=='\r' || *Ptr=='\n')
			{
				Ptr++;
			}

			// Store the location of the first character of this line
			ANSICHAR* Start = Ptr;

			// Advance the char pointer until we hit a newline character
			while (*Ptr && *Ptr!='\r' && *Ptr!='\n')
			{
				Ptr++;
			}

			// If this is the end of the file, we're done
			if (*Ptr == 0)
			{
				bIsDone = 1;
			}

			// Terminate the current line, and advance the pointer to the next character in the stream
			*Ptr++ = 0;

			FString CurrLine = ANSI_TO_TCHAR(Start);
			OutStrings.AddItem(CurrLine);
		}

		return TRUE;
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to open ANSI text file %s"), InFilename);
		return FALSE;
	}
}
#endif

//
// Save a binary array to a file.
//
UBOOL appSaveArrayToFile( const TArray<BYTE>& Array, const TCHAR* Filename, FFileManager* FileManager )
{
	FArchive* Ar = FileManager->CreateFileWriter( Filename, 0, GNull, Array.Num() );
	if( !Ar )
	{
		return 0;
	}
	Ar->Serialize( const_cast<BYTE*>(&Array(0)), Array.Num() );
	delete Ar;
	return 1;
}

UBOOL appWriteStringToFileInternal (const FString& String, const TCHAR* Filename, UBOOL bAlwaysSaveAsAnsi, FFileManager* FileManager, UBOOL Append)
{
	if( !String.Len() )
	{
		return 0;
	}

	DWORD WriteFlags = 0;
	if (Append)
	{
		WriteFlags = FILEWRITE_Append;
	}

	// max size of the string is a TCHAR for each character and some UNICODE magic
	FArchive* Ar = FileManager->CreateFileWriter( Filename, WriteFlags, GNull, String.Len() * sizeof(TCHAR) + sizeof(UNICHAR) );
	if( !Ar )
	{
		return 0;
	}
	UBOOL SaveAsUnicode=0, ConvertToAnsi=0, Success=1;
	for( INT i=0; i<String.Len(); i++ )
	{
		if( (*String)[i] != FromAnsi(ToAnsi((*String)[i])) )
		{
			if( bAlwaysSaveAsAnsi )
			{
				ConvertToAnsi = 1;
			}
			else
			{
				UNICHAR BOM = UNICODE_BOM;
				Ar->Serialize( &BOM, sizeof(BOM) );
				SaveAsUnicode = 1;
			}
			break;
		}
	}
	if( SaveAsUnicode || sizeof(TCHAR)==1 )
	{
		Ar->Serialize( const_cast<TCHAR*>(*String), String.Len()*sizeof(TCHAR) );
	}
	else
		if( ConvertToAnsi )
		{
			FTCHARToANSI Convert(*String);
			Ar->Serialize( (ANSICHAR*)Convert, Convert.Length() );
		}
		else
		{
			TArray<ANSICHAR> AnsiBuffer(String.Len());
			for( INT i=0; i<String.Len(); i++ )
				AnsiBuffer(i) = ToAnsi((*String)[i]);
			Ar->Serialize( const_cast<ANSICHAR*>(&AnsiBuffer(0)), String.Len() );
		}
		delete Ar;
		if( !Success )
			GFileManager->Delete( Filename );
		return Success;

}
//
// Write the FString to a file.
// Supports all combination of ANSI/Unicode files and platforms.
//
UBOOL appSaveStringToFile( const FString& String, const TCHAR* Filename, UBOOL bAlwaysSaveAsAnsi, FFileManager* FileManager )
{
	return appWriteStringToFileInternal(String, Filename, bAlwaysSaveAsAnsi, FileManager, FALSE);
}

UBOOL appAppendStringToFile( const FString& String, const TCHAR* Filename, UBOOL bAlwaysSaveAsAnsi, FFileManager* FileManager )
{
	return appWriteStringToFileInternal(String, Filename, bAlwaysSaveAsAnsi, FileManager, TRUE);
}

UBOOL appCreateBitmap( const TCHAR* Pattern, INT Width, INT Height, FColor* Data, FFileManager* FileManager )
{
#if ALLOW_DEBUG_FILES
	TCHAR File[MAX_SPRINTF]=TEXT("");
	// if the Pattern already has a .bmp extension, then use that the file to write to
	if (FFilename(Pattern).GetExtension() == TEXT("bmp"))
	{
		appStrcpy(File, Pattern);
	}
	else
	{
		for( INT TestBitmapIndex=GScreenshotBitmapIndex+1; TestBitmapIndex<65536; TestBitmapIndex++ )
		{
			appSprintf( File, TEXT("%s%05i.bmp"), Pattern, TestBitmapIndex );
			if( FileManager->FileSize(File) < 0 )
			{
				GScreenshotBitmapIndex = TestBitmapIndex;
				break;
			}
		}
		if (GScreenshotBitmapIndex == 65536)
		{
			return FALSE;
		}
	}

#if DWTRIOVIZSDK && _WINDOWS
	// 3D dumps need alpha for depth, use a PNG and bypass normal saving
	extern UBOOL DwTriovizImpl_IsTriovizCapturingDepth();
	if (DwTriovizImpl_IsTriovizCapturingDepth())
	{
		extern void DwTriovizImpl_SaveTGA(TCHAR * File, INT Width, INT Height, FColor* Data);
		DwTriovizImpl_SaveTGA(File, Width, Height, Data);
		return TRUE;
	}
#endif

	FArchive* Ar = FileManager->CreateDebugFileWriter( File );
	if( Ar )
	{
		// Types.
		#if SUPPORTS_PRAGMA_PACK
			#pragma pack (push,1)
		#endif
		struct BITMAPFILEHEADER
		{
			WORD   bfType GCC_PACK(1);
			DWORD   bfSize GCC_PACK(1);
			WORD   bfReserved1 GCC_PACK(1); 
			WORD   bfReserved2 GCC_PACK(1);
			DWORD   bfOffBits GCC_PACK(1);
		} FH; 
		struct BITMAPINFOHEADER
		{
			DWORD  biSize GCC_PACK(1); 
			INT    biWidth GCC_PACK(1);
			INT    biHeight GCC_PACK(1);
			WORD  biPlanes GCC_PACK(1);
			WORD  biBitCount GCC_PACK(1);
			DWORD  biCompression GCC_PACK(1);
			DWORD  biSizeImage GCC_PACK(1);
			INT    biXPelsPerMeter GCC_PACK(1); 
			INT    biYPelsPerMeter GCC_PACK(1);
			DWORD  biClrUsed GCC_PACK(1);
			DWORD  biClrImportant GCC_PACK(1); 
		} IH;
		#if SUPPORTS_PRAGMA_PACK
			#pragma pack (pop)
		#endif

		UINT	BytesPerLine = Align(Width * 3,4);

		// File header.
		FH.bfType       		= INTEL_ORDER16((WORD) ('B' + 256*'M'));
		FH.bfSize       		= INTEL_ORDER32((DWORD) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + BytesPerLine * Height));
		FH.bfReserved1  		= INTEL_ORDER16((WORD) 0);
		FH.bfReserved2  		= INTEL_ORDER16((WORD) 0);
		FH.bfOffBits    		= INTEL_ORDER32((DWORD) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)));
		Ar->Serialize( &FH, sizeof(FH) );

		// Info header.
		IH.biSize               = INTEL_ORDER32((DWORD) sizeof(BITMAPINFOHEADER));
		IH.biWidth              = INTEL_ORDER32((DWORD) Width);
		IH.biHeight             = INTEL_ORDER32((DWORD) Height);
		IH.biPlanes             = INTEL_ORDER16((WORD) 1);
		IH.biBitCount           = INTEL_ORDER16((WORD) 24);
		IH.biCompression        = INTEL_ORDER32((DWORD) 0); //BI_RGB
		IH.biSizeImage          = INTEL_ORDER32((DWORD) BytesPerLine * Height);
		IH.biXPelsPerMeter      = INTEL_ORDER32((DWORD) 0);
		IH.biYPelsPerMeter      = INTEL_ORDER32((DWORD) 0);
		IH.biClrUsed            = INTEL_ORDER32((DWORD) 0);
		IH.biClrImportant       = INTEL_ORDER32((DWORD) 0);
		Ar->Serialize( &IH, sizeof(IH) );

		// Colors.
		for( INT i=Height-1; i>=0; i-- )
		{
			for( INT j=0; j<Width; j++ )
			{
				Ar->Serialize( &Data[i*Width+j].B, 1 );
				Ar->Serialize( &Data[i*Width+j].G, 1 );
				Ar->Serialize( &Data[i*Width+j].R, 1 );
			}

			// Pad each row's length to be a multiple of 4 bytes.

			for(UINT PadIndex = Width * 3;PadIndex < BytesPerLine;PadIndex++)
			{
				BYTE	B = 0;
				Ar->Serialize(&B,1);
			}
		}

		// Success.
		delete Ar;
		if (!GIsEditor)
		{
			SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), File );
		}
	}
	else 
	{
		return 0;
	}
#endif
	// Success.
	return 1;
}

/**
 * Finds a usable splash pathname for the given filename
 * 
 * @param SplashFilename Name of the desired splash name ("Splash.bmp")
 * @param OutPath String containing the path to the file, if this function returns TRUE
 *
 * @return TRUE if a splash screen was found
 */
UBOOL appGetSplashPath(const TCHAR* SplashFilename, FString& OutPath)
{
	// we need a file manager!
	if (GFileManager == NULL)
	{
		return FALSE;
	}

	// first look in game's splash directory
	OutPath = appGameDir() + "Splash\\" + SplashFilename;
	
	// if this was found, then we're done
	if (GFileManager->FileSize(*OutPath) != -1)
	{
		return TRUE;
	}

	// next look in Engine\\Splash
	OutPath = appEngineDir() + "Splash\\" + SplashFilename;

	// if this was found, then we're done
	if (GFileManager->FileSize(*OutPath) != -1)
	{
		return TRUE;
	}

	// if not found yet, then return failure
	return FALSE;
}

/*-----------------------------------------------------------------------------
	User created content.
-----------------------------------------------------------------------------*/

/** Are there currently any user crated packages loaded? (since boot/appResetUserCreatedContentLoaded) */
UBOOL GWasUserCreatedContentLoaded = FALSE;

/** 
 * @return TRUE if any user created content was loaded since boot or since a call to appResetUserCreatedContentLoaded()
 */
UBOOL appHasAnyUserCreatedContentLoaded()
{
	return GWasUserCreatedContentLoaded;
}

/**
 * Marks that some user created content was loaded
 */
void appSetUserCreatedContentLoaded()
{
	GWasUserCreatedContentLoaded = TRUE;
}

/**
 * Resets the flag that tracks if any user created content was loaded (say, on return to main menu, etc)
 */
void appResetUserCreatedContentLoaded()
{
	GWasUserCreatedContentLoaded = FALSE;
}

/*-----------------------------------------------------------------------------
	Files.
-----------------------------------------------------------------------------*/

/**
 * This will recurse over a directory structure looking for files.
 * 
 * @param Result The output array that is filled out with a file paths
 * @param RootDirectory The root of the directory structure to recurse through
 * @param bFindPackages Should this function add package files to the Results
 * @param bFindNonPackages Should this function add non-package files to the Results
 */
void appFindFilesInDirectory(TArray<FString>& Results, const TCHAR* RootDirectory, UBOOL bFindPackages, UBOOL bFindNonPackages)
{
	// cache a temp FString of the root directory
	FString Root(RootDirectory);

	// make a wild card to look for directories in the current directory
	FString Wildcard = FString(RootDirectory) * TEXT("*.*");
	TArray<FString> SubDirs; 
	// find the directories, not files
	GFileManager->FindFiles(SubDirs, *Wildcard, FALSE, TRUE);
	// recurse through the directories looking for more files/directories
	for (INT SubDirIndex = 0; SubDirIndex < SubDirs.Num(); SubDirIndex++)
	{
		appFindFilesInDirectory(Results, *(Root * SubDirs(SubDirIndex)), bFindPackages, bFindNonPackages);
	}

	// look for any files in this directory
	TArray<FString> Files;
	// look for files, not directories
	GFileManager->FindFiles(Files, *Wildcard, TRUE, FALSE);
	// go over the file list
	for (INT FileIndex = 0; FileIndex < Files.Num(); FileIndex++)
	{
		// create a filename out of the found file
		FFilename Filename(Files(FileIndex));

		// this file is a package if its extension is registered as a package extension
		// (if we are doing this before GSys is setup, then we can't tell if it's a valid 
		// extension, which comes from GSys (ini files), so assume it is NOT a package)
		UBOOL bIsPackage = GSys && GSys->Extensions.FindItemIndex(*Filename.GetExtension()) != INDEX_NONE;

		// add this file if its a package and we want packages, or vice versa
		if ((bFindPackages && bIsPackage) || (bFindNonPackages && !bIsPackage))
		{
			Results.AddItem(Root * Files(FileIndex));
		}
	}
}

/**
 * Replaces all slashes and backslashes in the string with the correct path separator character for the current platform.
 *
 * @param	InFilename	a string representing a filename.
 */
void FPackageFileCache::NormalizePathSeparators( FString& InFilename )
{
	for( TCHAR* p=(TCHAR*)*InFilename; *p; p++ )
	{
		if( *p == '\\' || *p == '/' )
		{
			*p = PATH_SEPARATOR[0];
		}
	}
}


/**
 * Strips all path and extension information from a relative or fully qualified file name.
 *
 * @param	InPathName	a relative or fully qualified file name
 *
 * @return	the passed in string, stripped of path and extensions
 */
FString FPackageFileCache::PackageFromPath( const TCHAR* InPathName )
{
	FString PackageName = InPathName;
	INT i = PackageName.InStr( PATH_SEPARATOR, 1 );
	if( i != -1 )
		PackageName = PackageName.Mid(i+1);

	i = PackageName.InStr( TEXT("/"), 1 );
	if( i != -1 )
		PackageName = PackageName.Mid(i+1);

	i = PackageName.InStr( TEXT("\\"), 1 );
	if( i != -1 )
		PackageName = PackageName.Mid(i+1);

	i = PackageName.InStr( TEXT(".") );
	if( i != -1 )
		PackageName = PackageName.Left(i);

#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive && PackageName != TEXT("Core") && GScriptPatchPackageSuffix.Len() > 0 && appStrstr(InPathName, *GScriptPatchPackageSuffix) == NULL )
	{
		PackageName += GScriptPatchPackageSuffix;
	}
#endif

	return PackageName.ToLower();
}

/**
 * Parses a fully qualified or relative filename into its components (filename, path, extension).
 *
 * @param	InPathName	the filename to parse
 * @param	Path		[out] receives the value of the path portion of the input string
 * @param	Filename	[out] receives the value of the filename portion of the input string
 * @param	Extension	[out] receives the value of the extension portion of the input string
 */
void FPackageFileCache::SplitPath( const TCHAR* InPathName, FString& Path, FString& Filename, FString& Extension )
{
	Filename = InPathName;

	// first, make sure all path separators are the correct type for this platform
	NormalizePathSeparators(Filename);

	INT i = Filename.InStr( PATH_SEPARATOR, TRUE );
	if( i != INDEX_NONE )
	{
		Path = Filename.Left(i);
		Filename = Filename.Mid(i+1);
	}
	else
	{
		Path = TEXT("");
	}

	i = Filename.InStr( TEXT("."), TRUE );
	if( i != -1 )
	{
		Extension = Filename.Mid(i+1);
		Filename = Filename.Left(i);
	}
	else
	{
		Extension = TEXT("");
	}
}

/** 
 * Cache all packages in this path and any directories under it
 * 
 * @param InPath The path to look in for packages
 */
void FMapPackageFileCache::CachePath( const TCHAR* InPath)
{
	// find all packages in, and under the path
	TArray<FString> Packages;
	FString FixedPath = InPath;

#if FLASH 
	// @todo flash: remove this once the vfs layer is fixed, or at least move it into FFileManagerFlash!
	FixedPath = appConvertRelativePathToFull(FixedPath); 
#endif

	if (appGetPlatformType() & UE3::PLATFORM_WindowsConsole)
	{
		FixedPath = FixedPath.Replace(TEXT("\\CookedPC"),TEXT("\\CookedPCConsole"),TRUE);
	}
	else if (appGetPlatformType() & UE3::PLATFORM_WindowsServer)
	{
		FixedPath = FixedPath.Replace(TEXT("\\CookedPC"),TEXT("\\CookedPCServer"),TRUE);
	}

	appFindFilesInDirectory(Packages, *FixedPath, TRUE, FALSE);

	for (INT PackageIndex = 0; PackageIndex < Packages.Num(); PackageIndex++)
	{
		CachePackage(*Packages(PackageIndex));
	}
}

/**
 * Takes a fully pathed string and eliminates relative pathing (eg: annihilates ".." with the adjacent directory).
 * Assumes all slashes have been converted to PATH_SEPARATOR[0].
 * For example, takes the string:
 *	BaseDirectory/SomeDirectory/../SomeOtherDirectory/Filename.ext
 * and converts it to:
 *	BaseDirectory/SomeOtherDirectory/Filename.ext
 *
 * @param	InString	a pathname potentially containing relative pathing
 */
FString appCollapseRelativeDirectories(const FString& InString)
{
	// For each occurrance of "..PATH_SEPARATOR" eat a directory to the left.

	FString ReturnString = InString;
	FString LeftString, RightString;

	FPackageFileCache::NormalizePathSeparators(ReturnString);

	const FString SearchString = FString::Printf(TEXT("..") PATH_SEPARATOR);
	while( ReturnString.Split( SearchString, &LeftString, &RightString ) )
	{
		//debugf(TEXT("== %s == %s =="), *LeftString, *RightString );

		// Strip the first directory off LeftString.
		INT Index = LeftString.Len()-1;

		// Eat the slash on the end of left if there is one.
		if ( Index >= 0 && LeftString[Index] == PATH_SEPARATOR[0] )
		{
			--Index;
		}

		// Eat leftwards until a slash is hit.
		while ( Index >= 0 && LeftString[Index] != PATH_SEPARATOR[0] )
		{
			LeftString[Index--] = 0;
		}

		ReturnString = FString( *LeftString ) + FString( *RightString );
		//debugf(TEXT("after split: %s"), *ReturnString);
	}

	return ReturnString;
}

// Converts a Takes a potentially relative path and fills it out.
FString appConvertRelativePathToFull(const FString& InString)
{
	FString FullyPathed;
	if ( InString.StartsWith( TEXT("../") ) || InString.StartsWith( TEXT("..\\") ) )
	{
		FullyPathed = FString( appBaseDir() );
	}
	FullyPathed *= InString;

	return appCollapseRelativeDirectories( FullyPathed );
}

/**
 * Adds the package name specified to the runtime lookup map.  The stripped package name (minus extension or path info) will be mapped
 * to the fully qualified or relative filename specified.
 *
 * @param	InPathName		a fully qualified or relative [to Binaries] path name for an Unreal package file.
 * @param	InOverrideDupe	specify TRUE to replace existing mapping with the new path info
 * @param	WarnIfExists	specify TRUE to write a warning to the log if there is an existing entry for this package name
 *
 * @return	TRUE if the specified path name was successfully added to the lookup table; FALSE if an entry already existed for this package
 */
UBOOL FMapPackageFileCache::CachePackage( const TCHAR* InPathName, UBOOL InOverrideDupe, UBOOL WarnIfExists )
{
	// strip all path and extension info from the file
	FString PackageName = PackageFromPath( InPathName );

	// replace / and \ with PATH_SEPARATOR
	FFilename FixedPathName = InPathName;
	NormalizePathSeparators(FixedPathName);

	FString* ExistingEntry = FileLookup.Find(*PackageName);
	if( !InOverrideDupe && ExistingEntry )
	{
		// Expand relative paths to make sure that we don't get invalid ambiguous name warnings
		// e.g. flesh out c:\dir1\dir2\..\somePath2\file.war and ..\somePath2\file.war to see if they truly are the same.

		const FFilename FullExistingEntry = appConvertRelativePathToFull( *ExistingEntry );
		// debugf(TEXT(" ... %s --> %s"), **ExistingEntry, *FullExistingEntry);

		const FFilename FullFixedPathName = appConvertRelativePathToFull( FixedPathName );
		// debugf(TEXT(" ... %s --> %s"), *FixedPathName, *FullFixedPathName );

		// If the expanded existing entry is the same as the old, ignore the Cache request.
		if( FullFixedPathName.GetBaseFilename(FALSE) == FullExistingEntry.GetBaseFilename(FALSE) )
		{
			return TRUE;
		}


		if( WarnIfExists == TRUE )
		{
			SET_WARN_COLOR(COLOR_RED);
			warnf( NAME_Error, TEXT("Ambiguous package name: Using \'%s\', not \'%s\'"), *FullExistingEntry, *FullFixedPathName);
			CLEAR_WARN_COLOR();

			if( GIsUnattended == FALSE && !ParseParam(appCmdLine(),TEXT("DEMOMODE")) )
			{
				appMsgf(AMT_OK,TEXT("Ambiguous package name: Using \'%s\', not \'%s\'"), *FullExistingEntry, *FullFixedPathName);
			}
		}

		return FALSE;
	}
	else
	{
#if FLASH
		// @todo flash: remove this once the vfs layer is fixed, or at least move it into FFileManagerFlash!
		FixedPathName = appConvertRelativePathToFull( FixedPathName );
#endif
		FileLookup.Set( *PackageName, *FixedPathName );
		SourceControlStateLookup.Set(*PackageName, 0);
		return TRUE;
	}
}

/** 
 * Cache all packages found in the engine's configured paths directories, recursively.
 */
void FMapPackageFileCache::CachePaths()
{
	check(GSys);
	FileLookup.Empty();
	SourceControlStateLookup.Empty();

#if SHIPPING_PC_GAME
	TArray<FString>& Paths = GSys->Paths;
#else
	// decide which paths to use by commandline parameter
	// Used only for testing wrangled content -- not for ship!
	FString PathSet(TEXT("Normal"));
	Parse(appCmdLine(), TEXT("PATHS="), PathSet);

	TArray<FString>& Paths = (PathSet == TEXT("Cutdown")) ? GSys->CutdownPaths : GSys->Paths;
#endif

	// get the list of script package directories
	appGetScriptPackageDirectories(Paths);

	// loop through the specified paths and cache all packages found therein
	for (INT PathIndex = 0; PathIndex < Paths.Num(); PathIndex++)
	{
		CachePath(*Paths(PathIndex));
	}
}

/**
 * Finds the fully qualified or relative pathname for the package specified.
 *
 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
 * @param	Guid			if specified, searches the directory containing files downloaded from other servers (cache directory) for the specified package.
 * @param	OutFilename		receives the full [or relative] path that was originally registered for the package specified.
 * @param	Language		Language version to retrieve if overridden for that particular language
 *
 * @return	TRUE if the package was successfully found.
 */
UBOOL FMapPackageFileCache::FindPackageFile( const TCHAR* InName, const FGuid* Guid, FString& OutFileName, const TCHAR* Language)
{
	// Use current language if none specified.
	if( Language == NULL )
	{
		Language = UObject::GetLanguage();
	}

	UBOOL bNonEnglish = (appStricmp(Language, TEXT("INT")) != 0) ? TRUE : FALSE;

	// Don't return it if it's a library.
	if( appStrlen(InName)>appStrlen(DLLEXT) && appStricmp( InName + appStrlen(InName)-appStrlen(DLLEXT), DLLEXT )==0 )
	{
		return FALSE;
	}

	// Get the base name (engine, not ..\MyGame\Script\Engine.u)
	FFilename BasePackageName = PackageFromPath(InName);

	// We should first look up to see if the package name has a filename mapping
	const FName PackageFName = FName::FName(*BasePackageName);
	const FName *MappedName = UObject::GetPackageNameToFileMapping()->Find(PackageFName);
	if (MappedName != NULL)
	{
		BasePackageName = MappedName->ToString();
	}

	// track if the file was found
	UBOOL bFoundFile = FALSE;
	FString PackageFileName;

#if XBOX
	// on xbox, we don't support package downloading, so no need to waste any extra cycles/disk io dealing with it
	Guid = NULL;
#endif

#if !CONSOLE
	// First, check to see if the file exists.  This doesn't work for case sensitive filenames but it'll work for commandlets with hardcoded paths or
	// when calling this with the result of FindPackageFile:
	//		[pseudo code here]
	//		PackagePath = FindPackageFile(PackageName);
	//		LoadPackage(PackagePath);
	//			GetPackageLinker(PackagePath);
	//				PackagePath2 = FindPackageFile(PackagePath);
	if (GFileManager->FileSize(InName)>0)
	{
		// Make sure we have it in the cache.
		CachePackage( InName, 1 );

		// remember the path as the given path
		PackageFileName = InName;
		bFoundFile = TRUE;
		//warnf(TEXT("FindPackage: %s found directly"), InName);
	}
#endif

	// if we haven't found it yet, search by package name
	if (!bFoundFile)
	{
		for (INT LocPass = 0; LocPass < 3 && !bFoundFile; LocPass++)
		{
			FFilename PackageName = BasePackageName;

			// First pass we look at Package_LANG.ext
			if( LocPass == 0 )
			{
				PackageName = PackageName.GetLocalizedFilename( Language );
			}
			// Second pass we look at Package_INT.ext for partial localization.
			else if( LocPass == 1 )
			{
				PackageName = PackageName.GetLocalizedFilename( TEXT("INT") );
			}
			// Third pass just Package.ext
			
			// first, look in downloaded packages (so DLC can override normal packages)
			FDLCInfo* ExistingDLCEntry = DownloadedFileLookup.Find(*PackageName);
			// if we found some downloaded content, return it
			if (ExistingDLCEntry)
			{
				PackageFileName = ExistingDLCEntry->Path;
				bFoundFile = TRUE;

#if !CONSOLE
				// Now that the file has been found, verify that it actually still exists
				// on disk and wasn't deleted out from underneath us by an external source.
				// If it doesn't, remove it from the downloaded packages map.
				if ( GFileManager->FileSize( *PackageFileName ) == INDEX_NONE )
				{
					bFoundFile = FALSE;
					DownloadedFileLookup.Remove( *PackageName );
				}
#endif // #if !CONSOLE
			}
			else
			{
				FString* ExistingEntry = FileLookup.Find(*PackageName);
				if( ExistingEntry )
				{
					PackageFileName = *ExistingEntry;
					bFoundFile = TRUE;

#if !CONSOLE
					// Now that the file has been found, verify that it actually still exists
					// on disk and wasn't deleted out from underneath us by an external source.
					// If it doesn't, remove it from the packages and source control status maps.
					if ( GFileManager->FileSize( *PackageFileName ) == INDEX_NONE )
					{
						bFoundFile = FALSE;
						FileLookup.Remove( *PackageName );
						SourceControlStateLookup.Remove( *PackageName );
					}
#endif // #if !CONSOLE

					//if ((LocPass == 1) && (bNonEnglish == TRUE))
// 					if (bNonEnglish == TRUE)
// 					{
// 						debugf(TEXT("*><* FindPackageFile> Localized package on Pass %d: Found %s"), LocPass, *PackageFileName);
// 					}
				}
			}
		}
	}

	// if we successfully found the package by name, then we have to check the Guid to make sure it matches
	if (bFoundFile && Guid)
	{
		// @todo: If we could get to list of linkers here, it would be faster to check
		// then to open the file and read it
		FArchive* PackageReader = GFileManager->CreateFileReader(*PackageFileName);
		// this had better open
		check(PackageReader != NULL);

		// read in the package summary
		FPackageFileSummary Summary;
		*PackageReader << Summary;

		// compare Guids
		if (Summary.Guid != *Guid)
		{
			bFoundFile = FALSE;
		}

		// close package file
		delete PackageReader;
	}

	// if matching by filename and guid succeeded, then we are good to go, otherwise, look in the cache for a guid match (if a Guid was specified)
	if (bFoundFile)
	{
		OutFileName = PackageFileName;
	}
	else if (Guid)
	{
		// look in the download cache
		bFoundFile = GSys->CheckCacheForPackage(*Guid, InName, OutFileName);
	}

	return bFoundFile;
}

/**
 * Sets the source control status for a package
 *
 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
 * @param	InState			the new source control state
 */
UBOOL FMapPackageFileCache::SetSourceControlState ( const TCHAR* InName, INT InNewState)
{
	SourceControlStateLookup.Set(InName, InNewState);
	return TRUE;
}

/**
 * Gets the source control status for a package
 *
 * @param	InName			a string representing the filename of an Unreal package; may include path and/or extension.
 */
INT FMapPackageFileCache::GetSourceControlState ( const TCHAR* InName)
{
	const INT* State = SourceControlStateLookup.Find(InName);
	if (State)
	{
		return *State;
	}
	return 0;
}

/**
 * Returns the list of fully qualified or relative pathnames for all registered packages.
 */
TArray<FString> FMapPackageFileCache::GetPackageFileList()
{
	TArray<FString>	Result;
	for(TMap<FString, FDLCInfo>::TIterator It(DownloadedFileLookup);It;++It)
	{
		::new(Result) FString(It.Value().Path);
	}
	for(TMap<FString,FString>::TIterator It(FileLookup);It;++It)
	{
		::new(Result) FString(It.Value());
	}
	return Result;
}

/**
 * Add a downloaded content package to the list of known packages.
 * Can be assigned to a particular ID for removing with ClearDownloadadPackages.
 *
 * @param InPlatformPathName The platform-specific full path name to a package (will be passed directly to CreateFileReader)
 * @param UserIndex Optional user to associate with the package so that it can be flushed later
 *
 * @return TRUE if successful
 */
UBOOL FMapPackageFileCache::CacheDownloadedPackage(const TCHAR* InPlatformPathName, INT UserIndex)
{
	// make a new DLC info structure
	FDLCInfo NewInfo(FString(InPlatformPathName), UserIndex);

	// get the package name from the full path
	FString PackageName = PackageFromPath(InPlatformPathName);

	// add this to our list of downloaded package
	DownloadedFileLookup.Set(*PackageName, NewInfo);

	return TRUE;
}

//@script patcher
/**
 * Clears all entries from the package file cache.
 */
void FMapPackageFileCache::ClearPackageCache()
{
	FileLookup.Empty();
	SourceControlStateLookup.Empty();
}

/**
 * Remove all downloaded packages from the package file cache.
 */
void FMapPackageFileCache::ClearDownloadedPackages()
{
	DownloadedFileLookup.Empty();
}

FPackageFileCache* GPackageFileCache;

/** 
 * Creates a temporary filename with the specified prefix 
 *
 * @param Path - file pathname
 * @param Prefix - file prefix
 * @param Extension - file extension ('.' required!)
 * @param Result1024 - destination buffer to store results of unique path (@warning must be >= MAX_SPRINTF size)
 */
void appCreateTempFilename( const TCHAR* Path, const TCHAR* Prefix, const TCHAR* Extension, TCHAR* Result, SIZE_T ResultSize )
{
	check( ResultSize >= MAX_SPRINTF );
	static INT i=0;
	do
	{
		const INT PathLen = appStrlen( Path );
		if( PathLen	> 0 && Path[ PathLen - 1 ] != PATH_SEPARATOR[0] )
		{
			appSprintf( Result, TEXT("%s\\%s%04X%s"), Path, Prefix, i++, Extension );
		}
		else
		{
			appSprintf( Result, TEXT("%s%s%04X%s"), Path, Prefix, i++, Extension );
		}
	}
	while( GFileManager->FileSize(Result)>0 );
}


/** 
 * Creates a temporary filename (extension = '.tmp')
 *
 * @param Path - file pathname
 * @param Result1024 - destination buffer to store results of unique path (@warning must be >= MAX_SPRINTF size)
 */
void appCreateTempFilename( const TCHAR* Path, TCHAR* Result, SIZE_T ResultSize )
{
	appCreateTempFilename( Path, TEXT( "" ), TEXT( ".tmp" ), Result, ResultSize );
}

/** 
 * Removes the executable name from a commandline, denoted by parentheses.
 */
const TCHAR* RemoveExeName(const TCHAR* CmdLine)
{
	// Skip over executable that is in the commandline
	if( *CmdLine=='\"' )
	{
		CmdLine++;
		while( *CmdLine && *CmdLine!='\"' )
		{
			CmdLine++;
		}
		if( *CmdLine )
		{
			CmdLine++;
		}
	}
	while( *CmdLine && *CmdLine!=' ' )
	{
		CmdLine++;
	}
	// skip over any spaces at the start, which Vista likes to toss in multiple
	while (*CmdLine == ' ')
	{
		CmdLine++;
	}
	return CmdLine;
}

/*-----------------------------------------------------------------------------
	Game/ mode specific directories.
-----------------------------------------------------------------------------*/

/**
 * Returns the base directory of the current game by looking at the global
 * GGameName variable. This is usually a subdirectory of the installation
 * root directory and can be overridden on the command line to allow self
 * contained mod support.
 *
 * @return base directory
 */
FString appGameDir()
{
	return FString::Printf( TEXT("..") PATH_SEPARATOR TEXT("..") PATH_SEPARATOR TEXT("%sGame") PATH_SEPARATOR, GGameName );
}

/**
 * Returns the directory the engine uses to look for the leaf ini files. This
 * can't be an .ini variable for obvious reasons.
 *
 * @return config directory
 */
FString appGameConfigDir()
{
	return FString::Printf( TEXT( "%s%s%s%s" ), *appGameDir(), TEXT( "Config" ) PATH_SEPARATOR,
							( GConfigSubDirectory[0] != 0 ) ? GConfigSubDirectory : TEXT( "" ),
							( GConfigSubDirectory[0] != 0 ) ? PATH_SEPARATOR : TEXT( "" ) );
}

/**
 * Returns the directory the engine uses to output profiling files.
 *
 * @return log directory
 */
FString appProfilingDir()
{
#if XBOX
	GFileManager->EnableLogging();
	const FString PathName = *( FString(TEXT("DEVKIT:\\")) + FString::Printf( TEXT("%sGame\\Profiling\\"), GGameName ) );
	return PathName;
#else
	return appGameDir() + TEXT("Profiling") PATH_SEPARATOR;
#endif
}

/**
 * Returns the directory the engine uses to output screenshot files.
 *
 * @return screenshot directory
 */
FString appScreenShotDir()
{
	return appGameDir() + TEXT("Screenshots") PATH_SEPARATOR + appGetPlatformStringEx() + PATH_SEPARATOR;
}


/**
 * Returns the directory the engine uses to output logs. This currently can't 
 * be an .ini setting as the game starts logging before it can read from .ini
 * files.
 *
 * @return log directory
 */
FString appGameLogDir()
{
	return appGameDir() + TEXT("Logs") PATH_SEPARATOR;
}

/** 
 * Returns the base directory of the "core" engine that can be shared across
 * several games or across games & mods. Shaders and base localization files
 * e.g. reside in the engine directory.
 *
 * @return engine directory
 */
FString appEngineDir()
{
	return TEXT("..") PATH_SEPARATOR TEXT("..") PATH_SEPARATOR TEXT("Engine") PATH_SEPARATOR;
}

/**
 * Returns the directory the root configuration files are located.
 *
 * @return root config directory
 */
FString appEngineConfigDir()
{
	return appEngineDir() + TEXT("Config") + PATH_SEPARATOR;
}


// This is implemented differently on Mac, due to Mac having an irregular game directory structure (can't rely on having a 'Binaries' directory)
#if !PLATFORM_MACOSX
/** @return the root directory of the engine directory tree */
FString appRootDir()
{
	static FString Path;
	if ( Path.Len() == 0 )
	{
		Path = appBaseDir();

		// if the path ends in a separator, remove it
		if( Path.Right(1)==PATH_SEPARATOR )
		{
			Path = Path.LeftChop( 1 );
		}

		// keep going until we've removed Binaries
		INT pos = Path.InStr(PATH_SEPARATOR TEXT("Binaries"),FALSE,TRUE);
		if ( pos != INDEX_NONE )
		{
			Path = Path.Left(pos + 1);
		}
		else
		{
			while( Path.Len() && Path.Right(1)!=PATH_SEPARATOR )
			{
				Path = Path.LeftChop( 1 );
			}
		}
	}

	return Path;
}
#endif

/**
 * Returns the file used for the script class manifest
 */
FString appScriptManifestFile()
{
	FString ManifestFileName = appScriptOutputDir() * TEXT("Manifest.txt");
	return ManifestFileName;
}

/**
 * Returns the directory the engine should save compiled script packages to.
 */
FString appScriptOutputDir()
{
	check(GConfig);

	FString ScriptOutputDir;

	if ( ParseParam(appCmdLine(), TEXT("FINAL_RELEASE")) || ParseParam(appCmdLine(), TEXT("FINAL_RELEASE_DC")))
	{
		verify(GConfig->GetString( TEXT("UnrealEd.EditorEngine"), TEXT("FRScriptOutputPath"), ScriptOutputDir, GEngineIni ));
	}
	else
	{
		verify(GConfig->GetString( TEXT("UnrealEd.EditorEngine"), TEXT("EditPackagesOutPath"), ScriptOutputDir, GEngineIni ));
	}

	return ScriptOutputDir;
}

/**
 * Returns the pathnames for the directories which contain script packages.
 *
 * @param	ScriptPackagePaths	receives the list of directory paths to use for loading script packages 
 */
void appGetScriptPackageDirectories( TArray<FString>& ScriptPackagePaths )
{
	check(GSys);

	if ( ParseParam(appCmdLine(), TEXT("FINAL_RELEASE")) || ParseParam(appCmdLine(), TEXT("FINAL_RELEASE_DC")))
	{
		ScriptPackagePaths += GSys->FRScriptPaths;
	}
	else
	{
		ScriptPackagePaths += GSys->ScriptPaths;
	}
}

/**
 * @return The directory for local files used in cloud emulation or support
 */
FString appCloudDir()
{
#if IPHONE
	return TEXT("SAVE\\");
#else
	return appGameDir() + TEXT("Cloud") + PATH_SEPARATOR;
#endif
}

/**
 * @return The directory for local files that are cached
 */
FString appCacheDir()
{
	return appGameDir() + TEXT("Cache") PATH_SEPARATOR;
}


/*-----------------------------------------------------------------------------
	Init and Exit.
-----------------------------------------------------------------------------*/

//
// General initialization.
//
TCHAR GCmdLine[16384]=TEXT("");

const TCHAR* appCmdLine()
{
	return GCmdLine;
}

/**
 * This will completely load an .ini file into the passed in FConfigFile.  This means that it will 
 * recurse up the BasedOn hierarchy loading each of those .ini.  The passed in FConfigFile will then
 * have the data after combining all of those .ini 
 *
 * @param FilenameToLoad - this is the path to the file to 
 * @param ConfigFile - This is the FConfigFile which will have the contents of the .ini loaded into and Combined()
 * @param bUpdateIniTimeStamps - whether to update the timestamps array.  Only for Default___.ini should this be set to TRUE.  The generated .inis already have the timestamps.
 *
 **/
void LoadAnIniFile( const TCHAR* FilenameToLoad, FConfigFile& ConfigFile, UBOOL bUpdateIniTimeStamps )
{
	// This shouldn't be getting called if seekfree is enabled on console.
	check(!GUseSeekFreeLoading || !CONSOLE);

	// if the file does not exist then return
	if( GFileManager->FileSize( FilenameToLoad ) <= 0 )
	{
		//warnf( TEXT( "LoadAnIniFile was unable to find FilenameToLoad: %s "), FilenameToLoad );
		return;
	}

	TArray<DOUBLE> TimestampsOfInis;

	// Keep a list of ini's, starting with Source and ending with the root configuration file.
	TArray<FString> IniList;
	INT IniIndex = IniList.AddItem( FilenameToLoad );

	FConfigFile TmpConfigFile;
	UBOOL bFoundBasedOnText = FALSE; 

	// Recurse inis till we found a root ini (aka one without BasedOn being set).
	do
	{
		// Spit out friendly error if there was a problem locating .inis (e.g. bad command line parameter or missing folder, ...).
		if( GFileManager->FileSize( *IniList(IniIndex) ) < 0 )
		{
			GConfig = NULL;
			appErrorf( NAME_FriendlyError, TEXT("Couldn't locate '%s' which is required to run '%s'"), *IniList(IniIndex), GGameName );
		}

		// read in the based on .ini file
		TmpConfigFile.Read( *IniList(IniIndex) );
		//debugf( TEXT( "Just read in: %s" ), *IniList(IniIndex) );

		IniIndex = IniList.AddZeroed(); // so the get can replace it
		bFoundBasedOnText = TmpConfigFile.GetString( TEXT("Configuration"), TEXT("BasedOn"), IniList(IniIndex) );
		if( bFoundBasedOnText )
		{
			// Prepend an extra '..' now that the application binary is stored in a deeper subdirectory
			// @todo: Ideally paths specified in .ini files would be relative to the UE3 root
			IniList(IniIndex) = FString( TEXT( ".." ) PATH_SEPARATOR ) + IniList(IniIndex);
		}

	} while( bFoundBasedOnText == TRUE );

	// Discard empty entry.
	IniIndex--;
	//debugf( TEXT( "Discard empty entry" ) );

	// Read root ini.
	//debugf( TEXT( "Combining configFile: %s" ), *IniList(IniIndex) );
	ConfigFile.Read( *IniList(IniIndex) );
	DOUBLE AFiletimestamp = GFileManager->GetFileTimestamp(*IniList(IniIndex));
	TimestampsOfInis.AddItem( AFiletimestamp );

	// Traverse ini list back to front, merging along the way.
	for( IniIndex--; IniIndex >= 0; IniIndex-- )
	{
		//debugf( TEXT( "Combining configFile: %s" ), *IniList(IniIndex) );
		ConfigFile.Combine( *IniList(IniIndex) );
		AFiletimestamp = GFileManager->GetFileTimestamp( *IniList(IniIndex) );
		TimestampsOfInis.AddItem( AFiletimestamp );
	}

	// now strip out the BasedOn line so that if this function is used to load the generated file directly in the future,
	// we don't attempt to combine the files a second time
	FConfigSection* Sec = ConfigFile.Find(TEXT("Configuration"));
	if (Sec != NULL)
	{
		Sec->Remove(TEXT("BasedOn"));
	}

	if( bUpdateIniTimeStamps == TRUE )
	{
		// for loop of the number of files
		for( INT Idx = 0; Idx < TimestampsOfInis.Num(); ++Idx )
		{
			DOUBLE ATimestamp = TimestampsOfInis(Idx);

			TCHAR TimestampIdx[MAX_SPRINTF]=TEXT("");
			appSprintf( TimestampIdx, TEXT("%d"), Idx );
			ConfigFile.SetDouble( TEXT("IniVersion"), TimestampIdx, ATimestamp );
		}

	}
}

/**
 * This will load up two .ini files and then determine if the Generated one is outdated.
 * Outdatedness is determined by the following mechanic:
 *
 * When a generated .ini is written out it will store the timestamps of the files it was generated
 * from.  This way whenever the Default__.inis are modified the Generated .ini will view itself as
 * outdated and regenerate it self.
 *
 * Outdatedness also can be affected by commandline params which allow one to delete all .ini, have
 * automated build system etc.
 *
 * Additionally, this function will save the previous generation of the .ini to the Config dir with
 * a datestamp.
 *
 * Finally, this function will load the Generated .ini into the global ConfigManager.
 *
 * @param DefaultIniFile		The Default .ini file (e.g. DefaultEngine.ini )
 * @param GeneratedIniFile		The Generated .ini file (e.g. FooGameEngine.ini )
 * @param bTryToPreserveContents	If set, only properties that don't exist in the generatedn file will be added
 *										from the default file.  Used for editor user preferences that we don't want
 *										ever want to blow away.
 * @param YesNoToAll			[out] Receives the user's selection if an .ini was out of date.
 * @param bForceReload			Forces a reload of the resulting .ini file.
 *
 **/
void appCheckIniForOutdatedness( const TCHAR* GeneratedIniFile, const TCHAR* DefaultIniFile, const UBOOL bTryToPreserveContents, UINT& YesNoToAll, UBOOL bForceReload/*=FALSE*/ )
{
	// Not needed for seekfree loading on console
	if( !GUseSeekFreeLoading || !CONSOLE )
	{
		// need to check to see if the file already exists in the GConfigManager's cache
		// if it does exist then go ahead and return as we don't need to load it up again
		if( bForceReload == FALSE && GConfig->FindConfigFile( GeneratedIniFile ) != NULL )
		{
			//debugf( TEXT( "Request to load a config file that was already loaded: %s" ), GeneratedIniFile );
			return;
		}

		// recursively load up the config file
		FConfigFile DefaultConfigFile;
		LoadAnIniFile( DefaultIniFile, DefaultConfigFile, TRUE );

		FConfigFile GeneratedConfigFile; 
		LoadAnIniFile( GeneratedIniFile, GeneratedConfigFile, FALSE );

		// now that we have both of the files loaded we need to check the various dates
		// here we look to see if both the number of timestamp entries matches AND the contents
		// of those entries match

		UBOOL bFilesWereDifferent = FALSE;		
		INT Count = 0;
		{
			UBOOL bFoundIniEntry = TRUE;
			while( bFoundIniEntry == TRUE )
			{
				TCHAR TimestampIdx[MAX_SPRINTF]=TEXT("");
				appSprintf( TimestampIdx, TEXT("%d"), Count );

				DOUBLE DefaultIniTimestamp = 0;
				DOUBLE GeneratedIniTimestamp = 0;

				bFoundIniEntry = DefaultConfigFile.GetDouble( TEXT("IniVersion"), TimestampIdx, DefaultIniTimestamp );
				GeneratedConfigFile.GetDouble( TEXT("IniVersion"), TimestampIdx, GeneratedIniTimestamp );

				if( DefaultIniTimestamp != GeneratedIniTimestamp )
				{
					INT breakme = 0;
					bFilesWereDifferent = TRUE;
					break;
				}

				++Count;
			}
		}

		// Regenerate the ini file?
		UBOOL bForceRegenerate = FALSE;
		UBOOL bShouldUpdate = FALSE;
		if( ParseParam(appCmdLine(),TEXT("REGENERATEINIS")) == TRUE )
		{
			// NOTE: Always regenerates whether or not bTryToPreserveContents was set
			bForceRegenerate = TRUE;
		}
		else if( bFilesWereDifferent == TRUE )
		{
			// Check to see if the GeneratedIniFile exists.  If it does then we might need to prompt to 
			// overwrite it.  Otherwise, since it does not exist we will just create it
			const UBOOL bGeneratedFileExists = (GFileManager->FileSize(GeneratedIniFile) > 0 );
			if( bGeneratedFileExists == FALSE )
			{
				bForceRegenerate = TRUE;
			}
			else
			{
				if( ParseParam(appCmdLine(),TEXT("NOAUTOINIUPDATE")) )
				{
					// Flag indicating whether the user has requested 'Yes/No To All'.
					static INT GIniYesNoToAll = -1;
					// Make sure GIniYesNoToAll's 'uninitialized' value is kosher.
					checkAtCompileTime( ART_YesAll != -1, ART_YesAll_MustNotBeNegOne );
					checkAtCompileTime( ART_NoAll != -1, ART_NoAll_MustNotBeNegOne );

					// The file exists but is different.
					// Prompt the user if they haven't already responded with a 'Yes/No To All' answer.
					if( GIniYesNoToAll != ART_YesAll && GIniYesNoToAll != ART_NoAll )
					{
						YesNoToAll = appMsgf( AMT_YesNoYesAllNoAll, TEXT("Your ini (%s) file is outdated. Do you want to automatically update it saving the previous version? Not doing so might cause crashes!"), GeneratedIniFile );
						// Record whether the user responded with a 'Yes/No To All' answer.
						if ( YesNoToAll == ART_YesAll || YesNoToAll == ART_NoAll )
						{
							GIniYesNoToAll = YesNoToAll;
						}
					}
					else
					{
						// The user has already responded with a 'Yes/No To All' answer, so note it 
						// in the output arg so that calling code can operate on its value.
						YesNoToAll = GIniYesNoToAll;
					}
					// Regenerate the file if approved by the user.
					bShouldUpdate = (YesNoToAll == ART_Yes) || (YesNoToAll == ART_YesAll);
				}
				else
				{
					bShouldUpdate = TRUE;
				}
			}
		}

		if( bShouldUpdate && !bTryToPreserveContents )
		{
			// We were not asked to preserve the existing .ini file, so go ahead and regenerate it from scratch
			bForceRegenerate = TRUE;
		}

		// Regenerate the file.
		if( bForceRegenerate )
		{
			// overwrite the GeneratedFile with the Default data PLUS the default data's timestamps which it has
			DefaultConfigFile.Dirty = TRUE;
			DefaultConfigFile.Write( GeneratedIniFile );
		}
		else if( bShouldUpdate )
		{
			// Merge the .ini files by copying over properties that exist in the default .ini but are
			// missing from the generated .ini
			// NOTE: Most of the time there won't be any properties to add here, since LoadAnIniFile will
			//		 combine properties in the Default .ini with those in the Project .ini
			GeneratedConfigFile.AddMissingProperties( DefaultConfigFile );

			// Update the timestamps in the generated config file to match the source
			{
				UBOOL bFoundIniEntry = TRUE;
				while( bFoundIniEntry == TRUE )
				{
					TCHAR TimestampIdx[MAX_SPRINTF]=TEXT("");
					appSprintf( TimestampIdx, TEXT("%d"), Count );

					DOUBLE DefaultIniTimestamp = 0;
					DOUBLE GeneratedIniTimestamp = 0;

					bFoundIniEntry = DefaultConfigFile.GetDouble( TEXT("IniVersion"), TimestampIdx, DefaultIniTimestamp );
					if( bFoundIniEntry )
					{
						GeneratedConfigFile.SetDouble( TEXT("IniVersion"), TimestampIdx, DefaultIniTimestamp );
					}
					++Count;
				}
			}

			GeneratedConfigFile.Dirty = TRUE;
			GeneratedConfigFile.Write( GeneratedIniFile );
		}

		// after this has run we are guaranteed to have the correct generated .ini files 
		// so we need to load them up into the GlobalConfigManager
		GConfig->LoadFile( GeneratedIniFile, &DefaultConfigFile );
	}
}


/**
* This will create the .ini filenames for the Default and the Game based off the passed in values.
* (e.g. DefaultEditor.ini, MyLeetGameEditor.ini  and the respective relative paths to those files )
*
* @param GeneratedIniName				[Out] The Global TCHAR[MAX_SPRINTF] that unreal uses ( e.g. GEngineIni )
* @param GeneratedDefaultIniName		[Out] The Global TCHAR[MAX_SPRINTF] that unreal uses for the default ini ( e.g. GDefaultEngineIni )
* @param CommandLineDefaultIniToken	The token to look for on the command line to parse the passed in Default<Type>Ini
* @param CommandLineIniToken			The token to look for on the command line to parse the passed in <Type>Ini
* @param IniFileName					The IniFile's name  (e.g. Engine.ini Editor.ini )
* @param DefaultIniPrefix				What the prefix for the Default .inis should be  ( e.g. Default )
* @param IniPrefix						What the prefix for the Game's .inis should be  ( generally empty )
*/
void appCreateIniNames( TCHAR* GeneratedIniName, TCHAR* GeneratedDefaultIniName, const TCHAR* CommandLineDefaultIniName, const TCHAR* CommandLineIniName, const TCHAR* IniFileName, const TCHAR* DefaultIniPrefix, const TCHAR* IniPrefix )
{
	// This shouldn't be getting called for seekfree on console.
	check(!GUseSeekFreeLoading || !CONSOLE);

	// if the command line doesn't have the default INI NAME on it it calculate it
	if( Parse( appCmdLine(), CommandLineDefaultIniName, GeneratedDefaultIniName, MAX_SPRINTF ) != TRUE )
	{
		appSprintf( GeneratedDefaultIniName, TEXT("%s%s%s"), *appGameConfigDir(), DefaultIniPrefix, IniFileName );
	}

	// if the command line doesn't have the INI NAME on it it calculate it
	if( Parse( appCmdLine(), CommandLineIniName, GeneratedIniName, MAX_SPRINTF ) != TRUE )
	{
		appSprintf( GeneratedIniName, TEXT("%s%s%s%s"), *appGameConfigDir(), IniPrefix, GGameName, IniFileName );
	}
}

void appDeleteOldLogs()
{
	INT PurgeLogsDays = 0;
	GConfig->GetInt(TEXT("LogFiles"), TEXT("PurgeLogsDays"), PurgeLogsDays, GEngineIni);
	if (PurgeLogsDays >= 0)
	{
		// get a list of files in the log dir
		TArray<FString> Files;
		GFileManager->FindFiles(Files, *FString::Printf(TEXT("%s*.*"), *appGameLogDir()), TRUE, FALSE);

		// delete all those with the backup text in their name and that are older than the specified number of days
		DOUBLE MaxFileAgeSeconds = 60.0 * 60.0 * 24.0 * DOUBLE(PurgeLogsDays);
		for (INT i = 0; i < Files.Num(); i++)
		{
			FString FullFileName = appGameLogDir() + Files(i);
			if (FullFileName.InStr(BACKUP_LOG_FILENAME_POSTFIX) != INDEX_NONE && GFileManager->GetFileAgeSeconds(*FullFileName) > MaxFileAgeSeconds)
			{
				debugf(TEXT("Deleting old log file %s"), *Files(i));
				GFileManager->Delete(*FullFileName);
			}
		}
	}
}

/** 
 * Returns a list of known language extensions.
 *
 * @return	The array of known language extensions found in the Engine.ini config file
 */
const TArray<FString>& appGetKnownLanguageExtensions()
{
	static TArray<FString> KnownLanguageExtensions;

	if( KnownLanguageExtensions.Num() == 0 )
	{
#if CONSOLE
		// Look for Coalesced_*.bin files in the toc to see what is available
		KnownLanguageExtensions.AddItem( TEXT("INT") );

		FString LocalisationFiles = FString::Printf( TEXT( "..\\%sGame\\*.txt" ), appGetGameName() );
		TArray<FString> Languages;
		GFileManager->FindFiles( Languages, *LocalisationFiles, TRUE, FALSE );

		// Make sure each language found is valid.
		FString TOCPrefix = appGetPlatformString() + TEXT( "TOC_" );
		for( INT LanguageIndex = 0; LanguageIndex < Languages.Num(); ++LanguageIndex )
		{
			FString FileName = Languages( LanguageIndex );
			if( FileName.InStr( TOCPrefix, FALSE, TRUE ) == 0 )
			{
				FString Language = FileName.Mid( TOCPrefix.Len(), 3 );
				KnownLanguageExtensions.AddUniqueItem( Language.ToUpper() );
			}
		}
#else
		// INT is required and required to be first
		KnownLanguageExtensions.AddItem( TEXT("INT") );
		// Scan each localization path for localized directories.  All these languages will be "known" at runtime.
		// This lets UDK users to simply add languages by adding localized directories to the localized path
		for( INT PathIndex=0; PathIndex<GSys->LocalizationPaths.Num(); PathIndex++ )
		{
			TArray<FString> Languages;
			GFileManager->FindFiles( Languages, *(GSys->LocalizationPaths( PathIndex )*FString(TEXT("*.*"))), FALSE, TRUE );
		
			// Make sure each language found is valid.
			for( INT LanguageIndex = 0; LanguageIndex < Languages.Num(); ++LanguageIndex )
			{
				if( Languages( LanguageIndex ).Len() == 3 )
				{
					// If the language is a valid 3 letter extension add it to the list of known languages
					KnownLanguageExtensions.AddUniqueItem( Languages( LanguageIndex ).ToUpper() );
				}
			}
		}
#endif
	}

	return KnownLanguageExtensions;
}


/** 
 *	Returns whether the given language setting is known or not
 *
 *	@param	InLangExt		The language extension to check for validity
 *
 *	@return	UBOOL			TRUE if valid, FALSE if not
 */
UBOOL appIsKnownLanguageExt(const FString& InLangExt)
{
	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	UBOOL bIsKnownLanguage = FALSE;
	for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
	{
		if (InLangExt == KnownLanguageExtensions(LangIndex) )
		{
			bIsKnownLanguage = TRUE;
			break;
		}
	}

	return bIsKnownLanguage;
}

/*-----------------------------------------------------------------------------
	appInit
-----------------------------------------------------------------------------*/

/**
 * Sets GCmdLine to the string given
 */
void appSetCommandline(TCHAR* NewCommandLine)
{
	appStrncpy( GCmdLine, NewCommandLine, ARRAY_COUNT(GCmdLine) );
}

/**
 * Needs to be called after GConfig is set and LoadCoalescedFile was called.
 * Loads the state of console variables.
 * Works even if the variable is registered after the ini file was loaded.
 */
static void LoadConsoleVariablesFromINI()
{
	// e.g. "..\\..\\Engine\\Config\\ConsoleVariables.ini"
	// If the file doesn't exist simply no variable is set (no error).
	FString ConfigPath = appEngineDir() + TEXT("Config\\ConsoleVariables.ini");

	FConfigSection* Section = GConfig->GetSectionPrivate(TEXT("Startup"), FALSE, TRUE, *ConfigPath);

	if(Section)
	{
		for(FConfigSectionMap::TConstIterator It(*Section); It; ++It)
		{
			const FString& KeyString = It.Key().GetNameString(); 
			const FString& ValueString = It.Value();

			IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(*KeyString, FALSE);

			if(CVar)
			{
				// Set if the variable exists.
				CVar->Set(*ValueString);
			}
			else
			{
				// Create a dummy that is used when someone registers the variable later on.
				GConsoleManager->RegisterConsoleVariable(*KeyString, *ValueString, TEXT("IAmNoRealVariable"), (UINT)ECVF_Unregistered | (UINT)ECVF_CreatedFromIni);
			}
		}
	}
}

/**
* Main initialization function for app
*/
void appInit( const TCHAR* InCmdLine, FOutputDevice* InLog, FOutputDeviceConsole* InLogConsole, FOutputDeviceError* InError, FFeedbackContext* InWarn, FFileManager* InFileManager, FCallbackEventObserver* InCallbackEventDevice, FCallbackQueryDevice* InCallbackQueryDevice, FConfigCacheIni*(*ConfigFactory)() )
{
#if FORCELOWGORE
	GForceLowGore = TRUE;  // force this right at the start (we re-force it in the main loop, too, just in case).
#endif

	// Output devices.
	GLogConsole	= InLogConsole;
	GError		= InError;
	GWarn		= InWarn;

	GCallbackEvent = InCallbackEventDevice;
	check(GCallbackEvent);
	GCallbackQuery = InCallbackQueryDevice;
	check(GCallbackQuery);

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
	if ( GObjectSerializationPerfTracker == NULL )
	{
		GObjectSerializationPerfTracker = new FStructEventMap;
	}

	if ( GClassSerializationPerfTracker == NULL )
	{
		GClassSerializationPerfTracker = new FStructEventMap;
	}
#endif

	appStrncpy( GCmdLine, InCmdLine, ARRAY_COUNT(GCmdLine) );

	// Avoiding potential exploits by not exposing command line overrides in the shipping games.
#if !SHIPPING_PC_GAME && !CONSOLE
	// 8192 is the maximum length of the command line on Windows XP.
	TCHAR CmdLineEnv[8192];

	// Retrieve additional command line arguments from environment variable.
	appGetEnvironmentVariable(TEXT("UE3-CmdLineArgs"),CmdLineEnv,ARRAY_COUNT(CmdLineEnv));
	
	// Manually NULL terminate just in case. The NULL string is returned above in the error case so
	// we don't have to worry about that.
	CmdLineEnv[ARRAY_COUNT(CmdLineEnv)-1]=0;

	// Append the command line environment after inserting a space as we can't set it in the 
	// environment. Note that any code accessing GCmdLine before appInit obviously won't 
	// respect the command line environment additions.
	appStrncat( GCmdLine, TEXT(" "), ARRAY_COUNT(GCmdLine) );
	appStrncat( GCmdLine, CmdLineEnv, ARRAY_COUNT(GCmdLine) );
#endif

#if WITH_UE3_NETWORKING
	// Init sockets layer (may be needed for file management, so do it early)
	appSocketInit(TRUE);
#endif	//#if WITH_UE3_NETWORKING

	// Initialize file manager before making it default.
	GFileManager = InFileManager;
	GFileManager->PreInit();

	UINT YesNoToAll = ART_No;
	
#if !CONSOLE || IPHONE || ANDROID
	if (!Parse(appCmdLine(), TEXT("CONFIGSUBDIR="), GConfigSubDirectory, MAX_SPRINTF ))
	{
		GConfigSubDirectory[0] = 0;
	}
#endif

#if !CONSOLE
	// Error history.
	appStrcpy( GErrorHist, TEXT("Fatal error!\r\n\r\n") );
#endif

	// Platform specific pre-init.
	appPlatformPreInit();

	// Keep track of start time.
	GSystemStartTime = appSystemTimeString();

	// determine the name of all the required engine inis.
	// Done here because the FileManager needs to load the file when starting up
#if CONSOLE
	// We only supported munged inis on console.
	// hardcode the global ini names (not allowing for command line overrides)
	appSprintf(GEngineIni, TEXT("%s%s%sEngine.ini"), *appGameConfigDir(), INI_PREFIX, GGameName);
	appSprintf(GSystemSettingsIni, TEXT("%s%s%sSystemSettings.ini"), *appGameConfigDir(), INI_PREFIX, GGameName);
	appSprintf(GGameIni, TEXT("%s%s%sGame.ini"), *appGameConfigDir(), INI_PREFIX, GGameName);
	appSprintf(GInputIni, TEXT("%s%s%sInput.ini"), *appGameConfigDir(), INI_PREFIX, GGameName);
	appSprintf(GUIIni, TEXT("%s%s%sUI.ini"), *appGameConfigDir(), INI_PREFIX, GGameName);
	// make the default ini match the regular one.
	appStrcpy(GDefaultEngineIni, GEngineIni);
	appStrcpy(GDefaultSystemSettingsIni, GSystemSettingsIni);
	appStrcpy(GDefaultGameIni, GGameIni);
	appStrcpy(GDefaultInputIni, GInputIni);
	appStrcpy(GDefaultUIIni, GUIIni);
#else

#if WITH_EDITOR
	appCreateIniNames( GEditorIni, GDefaultEditorIni, TEXT("DEFEDITORINI="), TEXT("EDITORINI="), TEXT("Editor.ini"), DEFAULT_INI_PREFIX, INI_PREFIX );
	appCreateIniNames( GEditorUserSettingsIni, GDefaultEditorUserSettingsIni, TEXT("DEFEDITORUSERSETTINGSINI="), TEXT("EDITORUSERSETTINGSINI="), TEXT("EditorUserSettings.ini"), DEFAULT_INI_PREFIX, INI_PREFIX );
#endif
	appCreateIniNames( GSystemSettingsIni, GDefaultSystemSettingsIni, TEXT("DEFSYSTEMSETTINGSINI="), TEXT("SYSTEMSETTINGSINI="), TEXT("SystemSettings.ini"), DEFAULT_INI_PREFIX, INI_PREFIX );
#if _WINDOWS
	appCreateIniNames( GLightmassIni, GDefaultLightmassIni, TEXT("DEFLIGHTMASSINI="), TEXT("LIGHTMASSINI="), TEXT("Lightmass.ini"), DEFAULT_INI_PREFIX, PC_INI_PREFIX );
#endif

	const UBOOL bMobileEmulation = ParseParam( appCmdLine(), TEXT("simmobile") );

	if( bMobileEmulation )
	{
		extern UBOOL GAllowFullRHIReset;
		GAllowFullRHIReset = ParseParam( appCmdLine(), TEXT("rhireset") );
	}

	FString IniPrefx = bMobileEmulation ? SIMMOBILE_INI_PREFIX : INI_PREFIX;
	FString DefaultIniPrefix = bMobileEmulation ? SIMMOBILE_DEFAULT_INI_PREFIX : DEFAULT_INI_PREFIX; 
	appCreateIniNames( GEngineIni, GDefaultEngineIni, TEXT("DEFENGINEINI="), TEXT("ENGINEINI="), TEXT("Engine.ini"), *DefaultIniPrefix, *IniPrefx  );
	appCreateIniNames( GGameIni, GDefaultGameIni, TEXT("DEFGAMEINI="), TEXT("GAMEINI="), TEXT("Game.ini"), *DefaultIniPrefix, *IniPrefx  );
	appCreateIniNames( GInputIni, GDefaultInputIni, TEXT("DEFINPUTINI="), TEXT("INPUTINI="), TEXT("Input.ini"), *DefaultIniPrefix, *IniPrefx  );

	appCreateIniNames( GUIIni, GDefaultUIIni, TEXT("DEFUIINI="), TEXT("UIINI="), TEXT("UI.ini"), *DefaultIniPrefix, *IniPrefx  );
#endif

	// Now finish initializing the file manager after the commandline is set up
	GFileManager->Init(TRUE);
	// Switch into executable's directory.
	GFileManager->SetDefaultDirectory();

#if _WINDOWS
	extern TCHAR MiniDumpFilenameW[1024];

	// update the minidump filename now that we have enough info to point it to the log folder even in installed builds
	appStrcpy( MiniDumpFilenameW, *GFileManager->ConvertAbsolutePathToUserPath( *GFileManager->ConvertToAbsolutePath( *FString::Printf( TEXT("%sunreal-v%i-%s.dmp"), *appGameLogDir(), GEngineVersion, *appSystemTimeString() )) ) );
#endif

	if( ParseParam(appCmdLine(),TEXT("BUILDMACHINE")) == TRUE )
	{
		GIsBuildMachine = TRUE;
	}

#if !PERF_TRACK_FILEIO_STATS
	// Don't write to log file if we're tracking I/O perf so we don't deadlock while dumping stats.
	GLog->AddOutputDevice( InLog );
#endif

	if( ParseParam(appCmdLine(),TEXT("NOCONSOLE")) == FALSE )
	{
	  	GLog->AddOutputDevice( InLogConsole );
	}

#if DEDICATED_SERVER
	GLog->AddOutputDevice( new FOutputDeviceDebug() );
#else
	if( CONSOLE || appIsDebuggerPresent() || GIsBuildMachine )
	{
		GLog->AddOutputDevice( new FOutputDeviceDebug() );
	}
#endif

#if WANTS_WINDOWS_EVENT_LOGGING
	GLog->AddOutputDevice( new FOutputDeviceEventLog() );
#endif

	// if the config directory doesn't exist or is empty, create it and copy the .ini's from the default directory
	if (GConfigSubDirectory[0] != 0)
	{
		FString ConfigDir = appGameConfigDir();
		TArray<FString> SubDirIniList;
		GFileManager->FindFiles(SubDirIniList, *(ConfigDir + TEXT("*.ini")), TRUE, FALSE);
		if (SubDirIniList.Num() == 0)
		{
			debugf(TEXT("Specified config subdirectory \"%s\" doesn't exist or has no .ini files, copying .ini files from base directory"), GConfigSubDirectory);
			GFileManager->MakeDirectory(*ConfigDir, TRUE);
			// get the default config directory
			TCHAR BackupFirstChar = GConfigSubDirectory[0];
			GConfigSubDirectory[0] = 0;
			FString DefaultConfigDir = appGameConfigDir();
			GConfigSubDirectory[0] = BackupFirstChar;
			// copy *.ini from the default directory to the new directory
			TArray<FString> IniList;
			GFileManager->FindFiles(IniList, *(DefaultConfigDir + TEXT("*.ini")), TRUE, FALSE);
			for (INT i = 0; i < IniList.Num(); i++)
			{
				FString CleanName = FFilename(IniList(i)).GetCleanFilename();
				GFileManager->Copy(*(ConfigDir + CleanName), *(DefaultConfigDir + CleanName), FALSE, FALSE, TRUE);
			}
		}
	}

	//// Init config.
	GConfig = ConfigFactory();

#if CONSOLE
	// We only supported munged inis on console.
	UObject::SetLanguage(*(appGetLanguageExt()));
	// preload all needed ini files so that we don't seek around for them later
	GConfig->LoadCoalescedFile(NULL);
#else
#if WITH_EDITOR
	appCheckIniForOutdatedness( GEditorIni, GDefaultEditorIni, FALSE, YesNoToAll );
	appCheckIniForOutdatedness( GEditorUserSettingsIni, GDefaultEditorUserSettingsIni, TRUE, YesNoToAll );
#endif
	appCheckIniForOutdatedness( GSystemSettingsIni, GDefaultSystemSettingsIni, FALSE, YesNoToAll );
#if _WINDOWS
	appCheckIniForOutdatedness( GLightmassIni, GDefaultLightmassIni, FALSE, YesNoToAll );
#endif
	appCheckIniForOutdatedness( GEngineIni, GDefaultEngineIni, FALSE, YesNoToAll );
	appCheckIniForOutdatedness( GGameIni, GDefaultGameIni, FALSE, YesNoToAll );
	appCheckIniForOutdatedness( GInputIni, GDefaultInputIni, FALSE, YesNoToAll );
	appCheckIniForOutdatedness( GUIIni, GDefaultUIIni, FALSE, YesNoToAll );
#endif

#if XBOX
	// update config cache from any patch ini/loc files
	extern void appXenonPatchConfigs();
	appXenonPatchConfigs();
#endif

	LoadConsoleVariablesFromINI();

	// okie so after the above has run we now have the REQUIRED set of engine .inis  (all of the other .inis)
	// that are gotten from .uc files' config() are not requires and are dynamically loaded when the .u files are loaded

	// Feedback context.
	if( ParseParam(appCmdLine(),TEXT("WARNINGSASERRORS")) == TRUE )
	{
		GWarn->TreatWarningsAsErrors = TRUE;
	}

	if( ParseParam(appCmdLine(),TEXT("UNATTENDED")) == TRUE )
	{
		GIsUnattended = TRUE;
	}

	if( ParseParam(appCmdLine(),TEXT("SILENT")) == TRUE )
	{
		GIsSilent = TRUE;
	}

#if XBOX
	if(ParseParam(appCmdLine(), TEXT("SETTHREADNAMES")) == TRUE)
	{
		GSetThreadNames = TRUE;
	}
#endif

#if ENABLE_SCRIPT_TRACING
	if ( ParseParam(appCmdLine(), TEXT("UTRACE")) )
	{
		GIsUTracing = TRUE;
		debugf(NAME_Init, TEXT("Script tracing features enabled."));
	}
#endif

	// Show log if wanted.
	if( GLogConsole && ParseParam(appCmdLine(),TEXT("LOG")) )
	{
		GLogConsole->Show( TRUE );
	}

#if !CONSOLE && WITH_EDITOR
	// If specified, Lightmass has to be launched manually with -debug (e.g. through a debugger).
	// This creates a job with a hard-coded GUID, and allows Lightmass to be executed multiple times (even stand-alone).
	if ( ParseParam(appCmdLine(), TEXT("LIGHTMASSDEBUG")) )
	{
		extern UBOOL GLightmassDebugMode;
		GLightmassDebugMode = TRUE;
		debugf(TEXT("Running UE3 with Lightmass Debug Mode ENABLED"));
	}

	// If specified, all participating Lightmass agents will report back detailed stats to the log.
	if ( ParseParam(appCmdLine(), TEXT("LIGHTMASSSTATS")) )
	{
		extern UBOOL GLightmassStatsMode;
		GLightmassStatsMode = TRUE;
		debugf(TEXT("Running UE3 with Lightmass Stats Mode ENABLED"));
	}

#endif	//#if !CONSOLE && WITH_EDITOR

	// Query whether this is an epic internal build or not.
	if( GFileManager->FileSize( TEXT("..") PATH_SEPARATOR TEXT("..") PATH_SEPARATOR TEXT("Binaries") PATH_SEPARATOR TEXT("EpicInternal.txt") ) >= 0 )
	{
		GIsEpicInternal = TRUE;
	}

	//// Command line.
	debugf( NAME_Init, TEXT("Version: %i"), GEngineVersion );
	debugf( NAME_Init, TEXT("Epic Internal: %i"), GIsEpicInternal );
#if PLATFORM_64BITS
	debugf( NAME_Init, TEXT("Compiled (64-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__) );
#else
	debugf( NAME_Init, TEXT("Compiled (32-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__) );
#endif
	debugf( NAME_Init, TEXT("Changelist: %i"), GBuiltFromChangeList );
	debugf( NAME_Init, TEXT("Command line: %s"), appCmdLine() );
	debugf( NAME_Init, TEXT("Base directory: %s"), appBaseDir() );
	//debugf( NAME_Init, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );

	DWORD PreferredProcessor=0;
	if ( Parse(appCmdLine(), TEXT("PREFERPROCESSOR="), PreferredProcessor) )
	{
		DWORD Result = appSetThreadAffinity(appGetCurrentThread(), PreferredProcessor);
		debugf(NAME_Init, TEXT("Preferred Processor: %d (%d)"), PreferredProcessor, Result);
	}

	// Determine whether to override the default setting for including timestamps in the log.
	GConfig->GetBool(TEXT("LogFiles"), TEXT("LogTimes"), GPrintLogTimes, GEngineIni);
	if( ParseParam(appCmdLine(), TEXT("LOGTIMES")) )
	{
		GPrintLogTimes = TRUE;
	}
	else if( ParseParam(appCmdLine(), TEXT("NOLOGTIMES")) )
	{
		GPrintLogTimes = FALSE;
	}

	// if a logging build, clear out old log files
#if !NO_LOGGING && !FINAL_RELEASE && !MOBILE
	appDeleteOldLogs();
#endif

	// Platform specific init.
	appPlatformInit();

#if WITH_UE3_NETWORKING
#if WITH_STEAMWORKS
	appSteamInit();
#endif
	// now initialize sockets for any platform that couldn't initialize previously
	appSocketInit(FALSE);
#endif	//#if WITH_UE3_NETWORKING

#if STATS
	// Initialize stats before objects because there are object stats
	GStatManager.Init();
#endif

	// Object initialization.
	UObject::StaticInit();

	// System initialization.
	USystem* DefaultSystemObject = USystem::StaticClass()->GetDefaultObject<USystem>();
	FArchive DummyAr;
	USystem::StaticClass()->Link(DummyAr,FALSE);
	DefaultSystemObject->LoadConfig(NULL,NULL,UE3::LCPF_ReadParentSections);

	UBOOL bUseSeekFreePCPaths = FALSE;

#if SHIPPING_PC_GAME && !UDK
	bUseSeekFreePCPaths = TRUE;
	if (ParseParam(appCmdLine(), TEXT("NOSEEKFREELOADING")))
	{
		bUseSeekFreePCPaths = FALSE;
	}
#else
	const UBOOL bIsPCSeekFree = GUseSeekFreeLoading && !CONSOLE;
	const UBOOL bIsCookedEditor = ParseParam(appCmdLine(), TEXT("cookededitor"));
	const UBOOL bIsCookingAsUser = (appStristr(appCmdLine(), TEXT("CookPackages")) != NULL) && 
		(ParseParam(appCmdLine(), TEXT("user")) && ParseParam(appCmdLine(), TEXT("installed")));
	bUseSeekFreePCPaths = bIsPCSeekFree || bIsCookedEditor || bIsCookingAsUser;
#endif // SHIPPING_PC_GAME

	// check for the cases where we want to use the seekfree PC paths
	if( bUseSeekFreePCPaths )
	{
		DefaultSystemObject->Paths		= DefaultSystemObject->SeekFreePCPaths;
		// clear out the script paths, as we won't be using them
		DefaultSystemObject->ScriptPaths.Empty();
		DefaultSystemObject->FRScriptPaths.Empty();
	}

	GSys = new USystem;
	GSys->AddToRoot();
#if SUPPORT_NAME_FLAGS
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
	{
		GSys->Suppress(i).SetFlags( RF_Suppress );
	}
#endif

#if PS3
	// some additional processing now that GSys is created
	GFileManagerPS3->PostGSysCreation();
#endif

	// Language.
	TCHAR CookerLanguage[8];
	if( Parse( appCmdLine(), TEXT("LANGUAGEFORCOOKING="), CookerLanguage, ARRAY_COUNT(CookerLanguage) ) == TRUE )
	{
		UObject::SetLanguage( CookerLanguage );

		// Write the language passed in if firstinstall...
		if (ParseParam(appCmdLine(), TEXT("firstinstall")) == TRUE)
		{
			GConfig->SetString(TEXT("Engine.Engine"), TEXT("Language"), CookerLanguage, GEngineIni);
		}
	}
	else
	{
		UObject::SetLanguage(*appGetLanguageExt());
	}

	// Perform the hardware survey if desired
	extern void UploadHardwareSurveyIfNecessary();
	UploadHardwareSurveyIfNecessary();

#if CONSOLE
	// make sure no more .ini or .int files are read/written after this point, since everything has been read in
	GConfig->DisableFileOperations();
#endif

#if DEMOVERSION
	debugf(TEXT("DEMOVERSION is defined."));
#endif

	// Cache file paths
	GPackageFileCache = new FMapPackageFileCache;
	GPackageFileCache->CachePaths();

#if !XBOX
	// Init list of common colors. For Xbox @see appXenonInit.
	GColorList.CreateColorMap();
#endif // XBOX
}

//
// Pre-shutdown.
// Called from within guarded exit code, only during non-error exits.
//
void appPreExit()
{
#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
	if ( GObjectSerializationPerfTracker != NULL )
	{
		delete GObjectSerializationPerfTracker;
		GObjectSerializationPerfTracker = NULL;
	}

	if ( GClassSerializationPerfTracker != NULL )
	{
		delete GClassSerializationPerfTracker;
		GClassSerializationPerfTracker = NULL;
	}
#endif

#if XBOX
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	debugf( NAME_Exit, TEXT( "Rebooting console ...") );
	DmReboot( DMBOOT_COLD );
#endif
#endif

	debugf( NAME_Exit, TEXT("Preparing to exit.") );
	UObject::StaticExit();
	// Clean up the thread pool
	if (GThreadPool != NULL)
	{
		GThreadPool->Destroy();
	}

#if WITH_STEAMWORKS
	appSteamShutdown();
#endif
}

//
// Shutdown.
// Called outside guarded exit code, during all exits (including error exits).
//
void appExit()
{
	// Reset GCallbackEvent to prevent the pointer from being dereferenced after it has been destructed.  It's assumed that appExit is called
	// just before static destruction, and the GCallbackEvent points to a static instance that's about to be destructed.
	GCallbackEvent = NULL;

	debugf( NAME_Exit, TEXT("Exiting.") );
	if( GConfig )
	{
		GConfig->Exit();
		delete GConfig;
		GConfig = NULL;
	}
	GLog->TearDown();
	GLog = NULL;
}

/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

IMPLEMENT_COMPARE_CONSTREF( FString, UnMisc, { return appStricmp(*A,*B); } );

#pragma warning (push)
#pragma warning (disable : 4717)
static void InfiniteRecursionFunction(UBOOL B)
{
	if(B)
		InfiniteRecursionFunction(B);
}
#pragma warning (pop)

USystem::USystem()
:	SavePath			( E_NoInit )
,	CachePath			( E_NoInit )
,	CacheExt			( E_NoInit )
,	Paths				( E_NoInit )
,	ScriptPaths			( E_NoInit )
,	FRScriptPaths		( E_NoInit )
,	CutdownPaths		( E_NoInit )
,	Suppress			( E_NoInit )
,	Extensions			( E_NoInit )
,	LocalizationPaths	( E_NoInit )
,	TextureFileCacheExtension ( E_NoInit )
{}

void USystem::StaticConstructor()
{
	new(GetClass(),TEXT("StaleCacheDays"),			RF_Public)UIntProperty   (CPP_PROPERTY(StaleCacheDays		), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MaxStaleCacheSize"),		RF_Public)UIntProperty   (CPP_PROPERTY(MaxStaleCacheSize	), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MaxOverallCacheSize"),		RF_Public)UIntProperty   (CPP_PROPERTY(MaxOverallCacheSize	), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("PackageSizeSoftLimit"),	RF_Public)UIntProperty   (CPP_PROPERTY(PackageSizeSoftLimit	), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("AsyncIOBandwidthLimit"),	RF_Public)UFloatProperty (CPP_PROPERTY(AsyncIOBandwidthLimit), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SavePath"),				RF_Public)UStrProperty   (CPP_PROPERTY(SavePath				), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CachePath"),				RF_Public)UStrProperty   (CPP_PROPERTY(CachePath			), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CacheExt"),				RF_Public)UStrProperty   (CPP_PROPERTY(CacheExt				), TEXT("Options"), CPF_Config );

	UArrayProperty* A = new(GetClass(),TEXT("Paths"),RF_Public)UArrayProperty( CPP_PROPERTY(Paths), TEXT("Options"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* ScriptPathArray = new(GetClass(),TEXT("ScriptPaths"),RF_Public)UArrayProperty( CPP_PROPERTY(ScriptPaths), TEXT("Options"), CPF_Config );
	ScriptPathArray->Inner = new(ScriptPathArray,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* FinalReleaseScriptPathArray = new(GetClass(),TEXT("FRScriptPaths"),RF_Public)UArrayProperty( CPP_PROPERTY(FRScriptPaths), TEXT("Options"), CPF_Config );
	FinalReleaseScriptPathArray->Inner = new(FinalReleaseScriptPathArray,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* D = new(GetClass(),TEXT("Suppress"),RF_Public)UArrayProperty( CPP_PROPERTY(Suppress), TEXT("Options"), CPF_Config );
	D->Inner = new(D,TEXT("NameProperty0"),RF_Public)UNameProperty;

	UArrayProperty* E = new(GetClass(),TEXT("Extensions"),RF_Public)UArrayProperty( CPP_PROPERTY(Extensions), TEXT("Options"), CPF_Config );
	E->Inner = new(E,TEXT("StrProperty0"),RF_Public)UStrProperty;

	new(GetClass(),TEXT("TextureFileCacheExtension"),RF_Public)UStrProperty(CPP_PROPERTY(TextureFileCacheExtension),TEXT("Options"),CPF_Config);

	UArrayProperty* F = new(GetClass(),TEXT("LocalizationPaths"),RF_Public)UArrayProperty( CPP_PROPERTY(LocalizationPaths), TEXT("Options"), CPF_Config );
	F->Inner = new(F,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* G = new(GetClass(),TEXT("CutdownPaths"),RF_Public)UArrayProperty( CPP_PROPERTY(CutdownPaths), TEXT("Options"), CPF_Config );
	G->Inner = new(G,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* H = new(GetClass(),TEXT("SeekFreePCExtensions"),RF_Public)UArrayProperty( CPP_PROPERTY(SeekFreePCExtensions), TEXT("Options"), CPF_Config );
	H->Inner = new(H,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* I = new(GetClass(),TEXT("SeekFreePCPaths"),RF_Public)UArrayProperty( CPP_PROPERTY(SeekFreePCPaths), TEXT("Options"), CPF_Config );
	I->Inner = new(I,TEXT("StrProperty0"),RF_Public)UStrProperty;
}

///////////////////////////////////////////////////////////////////////////////

/** DEBUG used for exe "DEBUG BUFFEROVERFLOW" */
static void BufferOverflowFunction(SIZE_T BufferSize, const ANSICHAR* Buffer) 
{
	ANSICHAR LocalBuffer[32];
	LocalBuffer[0] = LocalBuffer[31] = 0; //if BufferSize is 0 then there's nothing to print out!

	BufferSize = Min<SIZE_T>(BufferSize, ARRAY_COUNT(LocalBuffer)-1);

	for( UINT i = 0; i < BufferSize; i++ ) 
	{
		LocalBuffer[i] = Buffer[i];
	}
	debugf(TEXT("BufferOverflowFunction BufferSize=%d LocalBuffer=%s"),BufferSize, ANSI_TO_TCHAR(LocalBuffer));
}

UBOOL USystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("CONFIGHASH")) )
	{
		FString ConfigFilename;
		if ( ParseToken(Cmd, ConfigFilename, TRUE) )
		{
			if ( ConfigFilename == TEXT("NAMESONLY") )
			{
				Ar.Log( TEXT("Files map:") );
				for ( FConfigCacheIni::TIterator It(*GConfig); It; ++It )
				{
					Ar.Logf(TEXT("FileName: %s"), *It.Key());
				}
			}
			else
			{
				const FString QualifiedFilename = ConfigFilename.InStr(TEXT(".")) == INDEX_NONE ? appGameConfigDir() + ConfigFilename + TEXT(".ini") : ConfigFilename;
				Ar.Logf(TEXT("Attempting to dump data for config file: %s"), *ConfigFilename);
				FConfigFile* ConfigFile = GConfig->FindConfigFile(*QualifiedFilename);
				if ( ConfigFile != NULL )
				{
					ConfigFile->Dump(Ar);
				}
				else
				{
					Ar.Logf(TEXT("No config file found using the path '%s'"), *QualifiedFilename);
				}
			}
		}
		else
		{
			GConfig->Dump( Ar );
		}
		return TRUE;
	}
	else if ( ParseCommand(&Cmd,TEXT("CONFIGMEM")) )
	{
		GConfig->ShowMemoryUsage(Ar);
		return TRUE;
	}
	else if ( ParseCommand(&Cmd,TEXT("DUMPALLOCS")) )
	{
		GMalloc->DumpAllocations(Ar);
		return TRUE;
	}
	else if ( ParseCommand(&Cmd,TEXT("HEAPCHECK")) )
	{
		GMalloc->ValidateHeap();
		return TRUE;
	}
	else if ( ParseCommand(&Cmd,TEXT("FLUSHLOG")) )
	{
		GLog->FlushThreadedLogs();
		GLog->Flush();
		return TRUE;
	}
	else
	if( ParseCommand(&Cmd,TEXT("EXIT")) || ParseCommand(&Cmd,TEXT("QUIT")))
	{
		// Ignore these commands when running the editor
		if( !GIsEditor )
		{
			Ar.Log( TEXT("Closing by request") );
			appRequestExit( 0 );
		}
		return TRUE;
	}
#if !SHIPPING_PC_GAME || UDK
	else if( ParseCommand( &Cmd, TEXT("DEBUG") ) )
	{
		if( ParseCommand(&Cmd,TEXT("CRASH")) )
		{
			appErrorf( TEXT("%s"), TEXT("Crashing the gamethread at your request") );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT("GPF") ) )
		{
			Ar.Log( TEXT("Crashing with voluntary GPF") );
			// changed to 3 from NULL because clang noticed writing to NULL and warned about it
			*(INT *)3 = 123;
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT("ASSERT") ) )
		{
			check(0);
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT("ENSURE") ) )
		{
			if( !ensure( 0 ) )
			{
				return TRUE;
			}
		}
#if IPHONE
		else if( ParseCommand( &Cmd, TEXT("SIGNAL") ) )
		{
			Ar.Log( TEXT("Crashing for the signal handler") );
			raise(SIGABRT);
			return TRUE;
		}
		else if (ParseCommand(&Cmd, TEXT("EXCEPTION")))
		{
			Ar.Log(TEXT("Crashing w/ unhandled exception!"));
			appRaiseException(12);
			return TRUE;
		}
#endif
		else if( ParseCommand( &Cmd, TEXT("RESETLOADERS") ) )
		{
			UObject::ResetLoaders( NULL );
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT("BUFFEROVERRUN") ) )
		{
 			// stack overflow test - this case should be caught by /GS (Buffer Overflow Check) compile option
 			ANSICHAR SrcBuffer[] = "12345678901234567890123456789012345678901234567890";
			BufferOverflowFunction(ARRAY_COUNT(SrcBuffer),SrcBuffer);
			return TRUE;
		}
		else if( ParseCommand(&Cmd, TEXT("CRTINVALID")) )
		{
			FString::Printf(NULL);
			return TRUE;
		}
		else if( ParseCommand(&Cmd, TEXT("HITCH")) )
		{
			appSleep(1.0f);
			return TRUE;
		}
		else if ( ParseCommand(&Cmd,TEXT("LONGLOG")) )
		{
			debugf(TEXT("This is going to be a really long log message to test the code to resize the buffer used to log with. %02048s"), TEXT("HAHA, this isn't really a long string, but it sure has a lot of zeros!"));
		}
#if 0
		else if( ParseCommand( &Cmd, TEXT("RECURSE") ) )
		{
			Ar.Logf( TEXT("Recursing") );
			InfiniteRecursionFunction(1);
			return TRUE;
		}
		else if( ParseCommand( &Cmd, TEXT("EATMEM") ) )
		{
			Ar.Log( TEXT("Eating up all available memory") );
			while( 1 )
			{
				void* Eat = appMalloc(65536);
				memset( Eat, 0, 65536 );
			}
			return TRUE;
		}
#endif
		return FALSE;
	}
	else if( ParseCommand(&Cmd,TEXT("DIR")) )		// DIR [path\pattern]
	{
		TArray<FString> Files;
		TArray<FString> Directories;

		GFileManager->FindFiles( Files, Cmd, 1, 0 );
		GFileManager->FindFiles( Directories, Cmd, 0, 1 );

		// Directories
		Sort<USE_COMPARE_CONSTREF(FString,UnMisc)>( &Directories(0), Directories.Num() );
		for( INT x = 0 ; x < Directories.Num() ; x++ )
		{
			Ar.Logf( TEXT("[%s]"), *Directories(x) );
		}

		// Files
		Sort<USE_COMPARE_CONSTREF(FString,UnMisc)>( &Files(0), Files.Num() );
		for( INT x = 0 ; x < Files.Num() ; x++ )
		{
			Ar.Logf( TEXT("[%s]"), *Files(x) );
		}

		return TRUE;
	}
	// Display information about the name table only rather than using HASH
	else if( ParseCommand( &Cmd, TEXT("NAMEHASH") ) )
	{
		FName::DisplayHash(Ar);
		return TRUE;
	}
#endif
#if TRACK_ARRAY_SLACK
	else if( ParseCommand( &Cmd, TEXT("DUMPSLACKTRACES") ) )
	{
		if( GSlackTracker )
		{
			GSlackTracker->DumpStackTraces( 100, Ar );
		}
		return TRUE;
	}
	else if( ParseCommand( &Cmd, TEXT("RESETSLACKTRACKING") ) )
	{
		if( GSlackTracker )
		{
			GSlackTracker->ResetTracking();
		}
		return TRUE;
	}
	else if( ParseCommand( &Cmd, TEXT("TOGGLESLACKTRACKING") ) )
	{
		if( GSlackTracker )
		{
			GSlackTracker->ToggleTracking();
		}
		return TRUE;
	}
#endif // TRACK_ARRAY_SLACK
	// View the last N number of names added to the name table. Useful for
	// tracking down name table bloat
	else if( ParseCommand( &Cmd, TEXT("VIEWNAMES") ) )
	{
		INT NumNames = 0;
		if (Parse(Cmd,TEXT("NUM="),NumNames))
		{
			for (INT NameIndex = Max<INT>(FName::GetMaxNames() - NumNames, 0); NameIndex < FName::GetMaxNames(); NameIndex++)
			{
				Ar.Logf(TEXT("%d->%s"), NameIndex, *FName::SafeString(EName(NameIndex)));
			}
		}
		return TRUE;
	}
	return FALSE;
}


/**
 * Performs periodic cleaning of the cache
 * @param bForceDeleteDownloadCache If TRUE, the entire autodownload cache will be cleared
 */
void USystem::PerformPeriodicCacheCleanup(UBOOL bForceDeleteDownloadCache)
{
	TArray<FString> TempFiles; 
	// if we desired, delete ALL files in the cache
	if (bForceDeleteDownloadCache)
	{
		GFileManager->FindFiles(TempFiles, *(CachePath * TEXT("*.*")), TRUE, FALSE);
	}
	// otherwise, just find any .tmp files in the cache directory
	else
	{
	GFileManager->FindFiles(TempFiles, *(CachePath * TEXT("*.tmp")), TRUE, FALSE);
	}

	// and delete them
	for (INT FileIndex = 0; FileIndex < TempFiles.Num(); FileIndex++)
	{
		GFileManager->Delete(*(CachePath * TempFiles(FileIndex)));
	}

	// now clean old cache files until we get down to MaxStaleCacheSize (converting days to seconds)
	CleanCache(MaxStaleCacheSize * 1024 * 1024, StaleCacheDays * 60 * 60 * 24);
}

/**
 * Cleans out the cache as necessary to free the space needed for an incoming
 * incoming downloaded package 
 *
 * @param SpaceNeeded Amount of space needed to download a package
 */
void USystem::CleanCacheForNeededSpace(INT SpaceNeeded)
{
	// clean the cache until we get down the overall max size - SpaceNeeded
	// use fairly small nonzero value (30 minutes) for expiration time so that we don't delete stuff we just downloaded
	CleanCache(Max(0, MaxOverallCacheSize * 1024 * 1024 - SpaceNeeded), 1800);
}

/**
 * Check to see if the cache contains a package with the given Guid and optional name
 *
 * @param Guid Guid to look for
 * @param PackageName Optional name of the package to check against
 * @param Filename [out] Path to the file if found
 *
 * @return TRUE if the package was found
 */
UBOOL USystem::CheckCacheForPackage(const FGuid& Guid, const TCHAR* PackageName, FString& Filename)
{
	UBOOL bFoundFile = FALSE;

	// figure out the name it would have been saved as (CachePath\Guid.CacheExt)
	FString GuidPackageName = GSys->CachePath * Guid.String() + GSys->CacheExt;

	// does it exist?
	if (GFileManager->FileSize(*GuidPackageName) != -1)
	{
		// if no name was specified, we've done all the checking we can
		if (PackageName != NULL)
		{
			// temporarily allow the GConfigCache to perform file operations if they were off
			UBOOL bWereFileOpsDisabled = GConfig->AreFileOperationsDisabled();
			GConfig->EnableFileOperations();

			FString IniName = GSys->CachePath * TEXT("Cache.ini");
			FString IniPackageName;
			// yes, now make sure the cache.ini entry matches names
			FConfigCacheIni CacheIni;
			// compare names if it's found in the ini
			if (CacheIni.GetString(TEXT("Cache"), *Guid.String(), IniPackageName, *IniName))
			{
				// if the guid matched and the name, then mark that we found the file
				// @todo: if it didn't match, we probably should delete it, as something is fishy
				if (FPackageFileCache::PackageFromPath(PackageName) == IniPackageName)
				{
					// set the package path to the cache path
					Filename = GuidPackageName;
					bFoundFile = TRUE;

					// if we found the cache package, and will therefore use it, then touch it 
					// so that it won't expire after N days
					GFileManager->TouchFile(*Filename);
				}
			}

			// re-disable file ops if they were before
			if (bWereFileOpsDisabled)
			{
				GConfig->DisableFileOperations();
			}
		}
	}

	return bFoundFile;
}

// helper struct to contain cache information
struct FCacheInfo
{
	static INT Compare(const FCacheInfo& A, const FCacheInfo& B)
	{
		// we want oldest to newest, so return < 0 if A is older (bigger age)
		return (INT)(B.Age - A.Age);
	}

	/** File name, age and size */
	FString Name;
	DOUBLE Age;
	INT Size;
};

/**
 * Internal helper function for cleaning the cache
 *
 * @param MaxSize Max size the total of the cache can be (for files older than ExpirationSeconds)
 * @param ExpirationSeconds Only delete files older than this down to MaxSize
 */
void USystem::CleanCache(INT MaxSize, DOUBLE ExpirationSeconds)
{
	// first, find all of the cache files
	TArray<FString> CacheFiles;
	GFileManager->FindFiles(CacheFiles, *(GSys->CachePath * TEXT("*") + GSys->CacheExt), TRUE, FALSE);

	// build a full view of the cache
	TArray<FCacheInfo> Cache;
	INT TotalCacheSize = 0;
	for (INT FileIndex = 0; FileIndex < CacheFiles.Num(); FileIndex++)
	{
		// get how old the file is
		FString Filename = GSys->CachePath * CacheFiles(FileIndex);
		DOUBLE Age = GFileManager->GetFileAgeSeconds(*Filename);

		// we only care about files older than ExpirationSeconds
		if (Age > ExpirationSeconds)
		{
			// fill out a cacheinfo object
			FCacheInfo* CacheInfo = new(Cache) FCacheInfo;
			CacheInfo->Name = Filename;
			CacheInfo->Age = Age;
			CacheInfo->Size = GFileManager->FileSize(*CacheInfo->Name);

			// remember total size of cache
			TotalCacheSize += CacheInfo->Size;
		}
	}

	// sort by age, so older ones are first
	Sort<FCacheInfo, FCacheInfo>((FCacheInfo*)Cache.GetData(), Cache.Num());

	// now delete files until we get <= MaxSize
	INT DeleteIndex = 0;
	while (TotalCacheSize > MaxSize)
	{
		// delete the next file to be deleted
		FCacheInfo& CacheInfo = Cache(DeleteIndex++);
		GFileManager->Delete(*CacheInfo.Name);

		// update the total cache size
		TotalCacheSize -= CacheInfo.Size;

		debugf( TEXT("Purged file from cache: %s (Age: %.0f > %.0f) [Cache is now %d / %d]"), *CacheInfo.Name, CacheInfo.Age, ExpirationSeconds, TotalCacheSize, MaxSize);
	}
}


IMPLEMENT_CLASS(USystem);

/*-----------------------------------------------------------------------------
	Game Name.
-----------------------------------------------------------------------------*/

const TCHAR* appGetGameName()
{
	return GGameName;
}

/**
 * This will log out that an LazyArray::Load has occurred.  
 *
 * Recommended usage: 
 *
 * PC: The stack trace will SPAM SPAM SPAM out the callstack of what caused the load to occur.
 *
 * Consoles:  If you see lots of log entries then you should probably run the game in a debugger
 *            and set a breakpoint in this function or in UnTemplate.h  LazyArray::Load() so you can track 
 *            down the LazyArray::Load() calls.
 *
 * @todo:  make it so this does not spam when a level is being associated / loaded
 * NOTE:  right now this is VERY VERY spammy and a bit slow as the new stack walk code is slower in SP 2
 **/
void LogLazyArrayPerfIssue()
{
#if defined(PERF_LOG_LAZY_ARRAY_LOADS) || LOOKING_FOR_PERF_ISSUES

	debugf( NAME_PerfWarning, TEXT("A LazyArray::Load has been called") );

#if _MSC_VER && !CONSOLE
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*) appSystemMalloc( StackTraceSize );
	StackTrace[0] = 0;
	// Walk the stack and dump it to the allocated memory.
	appStackWalkAndDump( StackTrace, StackTraceSize, CALLSTACK_IGNOREDEPTH );
	debugf( NAME_Warning, ANSI_TO_TCHAR(StackTrace) );
	appSystemFree( StackTrace );

#endif // #fi stacktrace capability

#endif

}

/*-----------------------------------------------------------------------------
	FIOManager implementation
-----------------------------------------------------------------------------*/

/**
 * Constructor, associating self with GIOManager.
 */
FIOManager::FIOManager()
{
	check( GIOManager==NULL );
	GIOManager = this;
}

/**
 * Destructor, removing association with GIOManager and deleting IO systems.
 */
FIOManager::~FIOManager()
{
	for( INT i=0; i<IOSystems.Num(); i++ )
	{
		delete IOSystems(i);
	}
	IOSystems.Empty();
	check( GIOManager==this );
	GIOManager = NULL;
}

/**
 * Flushes the IO manager. This means all outstanding IO requests will be fulfilled and
 * file handles will be closed. This is mainly to ensure that certain operations like 
 * saving a package can rely on there not being any open file handles.
 */
void FIOManager::Flush()
{
	// Block till all IO sub systems are done and also flush their handles.
	for( INT i=0; i<IOSystems.Num(); i++ )
	{
		FIOSystem* IO = IOSystems(i);
		IO->BlockTillAllRequestsFinishedAndFlushHandles();
	}
}

/**
 * Returns the IO system matching the passed in tag.
 *
 * @return	FIOSystem matching the passed in tag.
 */
FIOSystem* FIOManager::GetIOSystem( DWORD IOSystemTag )
{
	for( INT i=0; i<IOSystems.Num(); i++ )
	{
		FIOSystem* IO = IOSystems(i);
		// Return this system if tag matches.	
		if( IOSystemTag == IO->GetTag() )
		{
			return IO;
		}
	}
	return NULL;
}

/*-----------------------------------------------------------------------------
	Compression.
-----------------------------------------------------------------------------*/

/** Base compression method to use. Fixed on console but cooker needs to change this depending on target. */
ECompressionFlags GBaseCompressionMethod = COMPRESS_Default;

#include "../../../External/zlib/Inc/zlib.h"

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
static UBOOL appCompressMemoryZLIB( void* CompressedBuffer, INT& CompressedSize, const void* UncompressedBuffer, INT UncompressedSize )
{
	// Zlib wants to use unsigned long.
	unsigned long ZCompressedSize	= CompressedSize;
	unsigned long ZUncompressedSize	= UncompressedSize;
	// Compress data
	UBOOL bOperationSucceeded = compress( (BYTE*) CompressedBuffer, &ZCompressedSize, (const BYTE*) UncompressedBuffer, ZUncompressedSize ) == Z_OK ? TRUE : FALSE;
	// Propagate compressed size from intermediate variable back into out variable.
	CompressedSize = ZCompressedSize;
	return bOperationSucceeded;
}

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
UBOOL appUncompressMemoryZLIB( void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize )
{
	// Zlib wants to use unsigned long.
	unsigned long ZCompressedSize	= CompressedSize;
	unsigned long ZUncompressedSize	= UncompressedSize;
	
	// Uncompress data.
	UBOOL bOperationSucceeded = uncompress( (BYTE*) UncompressedBuffer, &ZUncompressedSize, (const BYTE*) CompressedBuffer, ZCompressedSize ) == Z_OK ? TRUE : FALSE;

	// Sanity check to make sure we uncompressed as much data as we expected to.
	check( UncompressedSize == ZUncompressedSize );
	return bOperationSucceeded;
}


#if WITH_XDK

#if !XBOX
#pragma comment(lib,"XCompress.lib")
#endif
// Undefine WITH_XDK in UnBuild.h if you don't have Xbox 360 XDK access or don't want to install it.
#include <Xcompress.h>

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
static UBOOL appCompressMemoryLZX( void* CompressedBuffer, INT& CompressedSize, const void* UncompressedBuffer, INT UncompressedSize )
{
	XMEMCOMPRESSION_CONTEXT CompressionContext = NULL;
	HRESULT hr = E_FAIL;

	// Check the arguments.
	if(UncompressedBuffer == NULL)
	{
		return FALSE;
	}

	// Create a compression context.
	if (FAILED(hr = XMemCreateCompressionContext(XMEMCODEC_LZX, NULL, 0, &CompressionContext)))
	{
		return FALSE;
	}

	// Reduce size by 4 as LZX requires 4 byte padding to avoid reading past the end during decompression. We reduce 
	// the in-size so the compressor can fail if the buffer is not big enough to include the padding.
	SIZE_T CompressedSizeOut = CompressedSize - 4;

	// Compress the data from the UncompressedBuffer buffer 
	// and copy it to the CompressedBuffer buffer.
	hr = XMemCompress(CompressionContext, CompressedBuffer, &CompressedSizeOut, UncompressedBuffer, UncompressedSize);

	// Increase reported compressed size by 4 to pad as LZX reads past the end of compressed data when decompressing.
	CompressedSize = CompressedSizeOut + 4;

	// Destroy the decompression context.
	XMemDestroyCompressionContext( CompressionContext );

	return SUCCEEDED(hr);
}

/**
 * Thread-safe abstract decompression routine. Decompresses memory from a compressed buffer and writes it to 
 * uncompressed buffer. 
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @return TRUE if decompression succeeds, FALSE if it fails
 */
UBOOL appUncompressMemoryLZX( void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize )
{
	XMEMDECOMPRESSION_CONTEXT CompressionContext = NULL;
	HRESULT hr = E_FAIL;

	// Check the arguments.
	if(CompressedBuffer == NULL || UncompressedSize == 0)
	{
		return FALSE;
	}

	// Create a decompression context.
	if (FAILED(hr = XMemCreateDecompressionContext(XMEMCODEC_LZX, NULL, 0, &CompressionContext)))
	{
		return FALSE;
	}

	// Compressed size was padded in the compressor so we have an extra 4 bytes of data at the end. The LZX decompressor
	// reads past the end of memory so this is required to avoid crashes.
	CompressedSize -= 4;

	SIZE_T UncompressedSizeOut = UncompressedSize;

	// Decompress the data from the CompressedBuffer buffer and copy it to the UncompressedBuffer buffer.
	hr = XMemDecompress(CompressionContext, UncompressedBuffer, &UncompressedSizeOut, CompressedBuffer, CompressedSize);

	// Destroy the decompression context.
	XMemDestroyDecompressionContext( CompressionContext );

	// Make sure we uncompressed the correct number of bytes if decompression succeeded as XMemDecompress
	// doesn't do a very good job of detecting bad input data.
	return SUCCEEDED(hr) && (UncompressedSize == UncompressedSizeOut);
}
#endif


#if WITH_LZO
#include "../../../External/lzopro/include/lzo/lzoconf.h"
#include "../../../External/lzopro/include/lzo/lzopro/lzo1x.h"
#include "../../../External/lzopro/include/lzo/lzopro/lzo1y.h"
#include "../../../External/lzopro/include/lzo/lzo1f.h"

/**
 * Callback memory allocation function for the LZO*99* compressor to use
 * 
 * @param UserData	Points to the GLZOCallbacks structure
 * @param Items		Number of "items" to allocate
 * @param Size		Size of each "item" to allocate
 * 
 * @return A pointer to a block of memory Items * Size big
 */
static lzo_voidp __LZO_CDECL LZOMalloc(lzo_callback_p UserData, lzo_uint Items, lzo_uint Size)
{
    return appMalloc(Items * Size);
}

/**
 * Callback memory deallocation function for the LZO*99* compressor to use
 * 
 * @param UserData	Points to the GLZOCallbacks structure
 * @param Ptr		Pointer to memory to free
 */
static void __LZO_CDECL LZOFree(lzo_callback_p UserData, lzo_voidp Ptr)
{
    appFree(Ptr);
}

lzo_callback_t GLZOCallbacks = 
{
	LZOMalloc, // allocation routine
	LZOFree, // deallocation routine
	0, // progress callback
	NULL, // user pointer
	0, // user data
	0 // user data
};

// NOTE: The following will be all cleaned up when we decide on the final choice of compressors to use!
// each compression method needs a different amount of work mem
#define LZO_STANDARD_MEM		LZOPRO_LZO1X_1_14_MEM_COMPRESS
#define LZO_SPEED_MEM			LZOPRO_LZO1X_1_08_MEM_COMPRESS
#define LZO_SIZE_MEM			0

// allocate to fit the biggest one
#define LZO_WORK_MEM_SIZE		(Max<INT>(Max<INT>(LZO_STANDARD_MEM, LZO_SPEED_MEM), LZO_SIZE_MEM))

// each compression function must be in the same family (lzo1x, etc) for decompression
#define LZO_STANDARD_COMPRESS	lzopro_lzo1x_1_14_compress
#define LZO_SPEED_COMPRESS		lzopro_lzo1x_1_08_compress
#define LZO_SIZE_COMPRESS(in, in_len, out, out_len) lzopro_lzo1x_99_compress(in, in_len, out, out_len, &GLZOCallbacks, 10);
#if __WIN32__
#define LZO_DECOMPRESS			lzopro_lzo1x_decompress_safe
#else
#define LZO_DECOMPRESS			lzopro_lzo1x_decompress
#endif

/** Critical section used to serialize access to working memory. */
static FCriticalSection LZOCriticalSection;

/**
 * Initializes LZO library, safe to call multiple times.
 */
static void InitializeLZO()
{
	static UBOOL bIsInitialized = FALSE;
	if( !bIsInitialized )
	{
		// Initialized lzo library and verify it succeeds.
		verify( lzo_init() == LZO_E_OK );
	}
}

// VS 2005 SP1 seems to generate bad code calling appUncompressMemoryLZO if optmizations are enabled.
#if _MSC_VER == 1400
PRAGMA_DISABLE_OPTIMIZATION
#endif

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	Flags						Flags to optionally control memory vs speed
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
static UBOOL appCompressMemoryLZO( ECompressionFlags Flags, void* CompressedBuffer, INT& CompressedSize, const void* UncompressedBuffer, INT UncompressedSize )
{
	void* LZOWorkMemoryBase = NULL;

	check( UncompressedSize <= MaxUncompressedSize );

	// Serialize access to working/ scratch memory.
	FScopeLock ScopeLock( &LZOCriticalSection );

	// we use a temporary buffer here that is bigger than the uncompressed size. this
	// is because LZO expects CompressedSize to be bigger than UncompressedSize, in
	// case the data is incompressible. Since there is no guarantee that CompressedSize
	// is big enough, we can't use it, because LZO will happily overwrite past the end
	// of CompressedBuffer (since it doesn't take as input the size of CompressedBuffer)

	const PTRINT LZOAlignment = 0x10000;  // LZO uses the pointer low bits as a seed, we want to avoid that

	BYTE* CompressScratchBufferBase = ( BYTE* )appMalloc( UncompressedSize + LZO_WORK_MEM_SIZE + LZOAlignment);
	BYTE* CompressScratchBuffer = Align(CompressScratchBufferBase,LZOAlignment);

	// LZO reads past the end of the source data, so we need to ensure that data is always the same.
	BYTE* SourceDataBase = ( BYTE * )appMalloc( UncompressedSize + LZO_WORK_MEM_SIZE + LZOAlignment);
	BYTE* SourceData = Align(SourceDataBase,LZOAlignment);
	appMemzero( SourceData, UncompressedSize + LZO_WORK_MEM_SIZE );
	appMemcpy( SourceData, UncompressedBuffer, UncompressedSize );

	// out variable for how big the compressed data actually is
	lzo_uint FinalCompressedSize;
	// attempt to compress the data 
	INT Result = LZO_E_OK;
	
	// Make sure LZO is initialized before calling it.
	InitializeLZO();

	if( Flags & COMPRESS_BiasSpeed )
	{
		// Zero initialize scratch memory. The LZO compressor makes decisions based on values of uninitialized data.
		LZOWorkMemoryBase = appMalloc( LZO_SPEED_MEM + LZOAlignment );
		BYTE* LZOWorkMemory = (BYTE *)Align(LZOWorkMemoryBase,LZOAlignment);
		appMemzero( LZOWorkMemory, LZO_SPEED_MEM );

		Result = LZO_SPEED_COMPRESS( SourceData, UncompressedSize, CompressScratchBuffer, &FinalCompressedSize, LZOWorkMemory );
	}
	else if( Flags & COMPRESS_BiasMemory )
	{
		Result = LZO_SIZE_COMPRESS( SourceData, UncompressedSize, CompressScratchBuffer, &FinalCompressedSize );
	}
	else
	{
		// Zero initialize scratch memory. The LZO compressor makes decisions based on values of uninitialized data.
		LZOWorkMemoryBase = appMalloc( LZO_STANDARD_MEM + LZOAlignment );
		BYTE* LZOWorkMemory = (BYTE *)Align(LZOWorkMemoryBase,LZOAlignment);
		appMemzero( LZOWorkMemory, LZO_STANDARD_MEM );

		Result = LZO_STANDARD_COMPRESS( SourceData, UncompressedSize, CompressScratchBuffer, &FinalCompressedSize, LZOWorkMemory );
	}

	// this shouldn't ever fail, apparently unless something catastrophic happened
	// but the docs are really not clear, because there are no docs
	check(Result == LZO_E_OK);

	// by default we succeeded (ie fit into available memory)
	UBOOL Return = TRUE;

	// if the compressed size will fit in the CompressedBuffer, copy it in
	if( FinalCompressedSize <= ( DWORD )CompressedSize )
	{
		// copy the data
		appMemcpy( CompressedBuffer, CompressScratchBuffer, FinalCompressedSize );
	}
	else
	{
		// if it doesn't fit, then this function has failed
		Return = FALSE;
	}

	// Free up the work memory
	if( LZOWorkMemoryBase )
	{
		appFree( LZOWorkMemoryBase );
	}

	appFree( SourceDataBase );
	appFree( CompressScratchBufferBase );

	// if this compression succeeded or failed, return how big it compressed it to
	// this way, on a failure, it can be called again with a big enough buffer
	CompressedSize = FinalCompressedSize;

	// return our success/failure
	return( Return );
}

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
static UBOOL appUncompressMemoryLZO( void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize )
{
	// Make sure LZO is initialized before calling it.
	InitializeLZO();

	// LZO wants unsigned. Initialized to uncompressed size as safe version uses this for bounds checking.
	lzo_uint FinalUncompressedSize = UncompressedSize;
	INT Result = LZO_DECOMPRESS((const BYTE*)CompressedBuffer, CompressedSize, (BYTE*)UncompressedBuffer, &FinalUncompressedSize, NULL);
	
	// if the call failed, return FALSE
	if (Result != LZO_E_OK)
	{
		return FALSE;
	}

	// success
	return TRUE;
}
#endif	//#if WITH_LZO

// VS 2005 SP1 seems to generate bad code calling appUncompressMemoryLZO if optmizations are enabled.
#if _MSC_VER == 1400
PRAGMA_ENABLE_OPTIMIZATION
#endif

/** Time spent compressing data in seconds. */
DOUBLE GCompressorTime		= 0;
/** Number of bytes before compression.		*/
QWORD GCompressorSrcBytes	= 0;
/** Nubmer of bytes after compression.		*/
QWORD GCompressorDstBytes	= 0;

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data. Compression controlled by the passed in flags.
 *
 * @param	Flags						Flags to control what method to use and optionally control memory vs speed
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
UBOOL appCompressMemory( ECompressionFlags Flags, void* CompressedBuffer, INT& CompressedSize, const void* UncompressedBuffer, INT UncompressedSize )
{
	DOUBLE CompressorStartTime = appSeconds();

	// make sure a valid compression scheme was provided
	check(Flags & (COMPRESS_ZLIB | COMPRESS_LZO | COMPRESS_LZX));

	UBOOL bCompressSucceeded = FALSE;

	// Always bias for speed if option is set.
	if( GAlwaysBiasCompressionForSize )
	{
		INT NewFlags = Flags;
		NewFlags &= ~COMPRESS_BiasSpeed;
		NewFlags |= COMPRESS_BiasMemory;
		Flags = (ECompressionFlags) NewFlags;
	}

	switch(Flags & COMPRESSION_FLAGS_TYPE_MASK)
	{
		case COMPRESS_ZLIB:
			bCompressSucceeded = appCompressMemoryZLIB(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
			break;
		case COMPRESS_LZO:
#if WITH_LZO
			bCompressSucceeded = appCompressMemoryLZO(Flags, CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
			break;
#endif
		case COMPRESS_LZX:
#if WITH_XDK
			bCompressSucceeded = appCompressMemoryLZX(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
			break;
#endif
		default:
			warnf(TEXT("appCompressMemory - This compression type not supported"));
			bCompressSucceeded =  FALSE;
	}

	// Keep track of compression time and stats.
	GCompressorTime += appSeconds() - CompressorStartTime;
	if( bCompressSucceeded )
	{
		GCompressorSrcBytes += UncompressedSize;
		GCompressorDstBytes += CompressedSize;
	}

	return bCompressSucceeded;
}

/**
 * Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
 * buffer. UncompressedSize is expected to be the exact size of the data after decompression.
 *
 * @param	Flags						Flags to control what method to use to decompress
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @param	bIsSourcePadded				Whether the source memory is padded with a full cache line at the end
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
UBOOL appUncompressMemory( ECompressionFlags Flags, void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize, UBOOL bIsSourcePadded /*= FALSE*/ )
{
	// Keep track of time spent uncompressing memory.
	STAT(DOUBLE UncompressorStartTime = appSeconds();)
	
	// make sure a valid compression scheme was provided
	check(Flags & (COMPRESS_ZLIB | COMPRESS_LZO | COMPRESS_LZX));

	UBOOL bUncompressSucceeded = FALSE;

	switch(Flags & COMPRESSION_FLAGS_TYPE_MASK)
	{
		case COMPRESS_ZLIB:
#if PS3
			if (Flags & COMPRESS_ForcePPUDecompressZLib)
			{
				bUncompressSucceeded = appUncompressMemoryZLIB(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
			}
			else
			{
				bUncompressSucceeded = appUncompressMemoryZLIBPS3(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, bIsSourcePadded);
			}
#else
			bUncompressSucceeded = appUncompressMemoryZLIB(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
#endif
			break;
		case COMPRESS_LZO:
#if WITH_LZO
			bUncompressSucceeded = appUncompressMemoryLZO(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
			break;
#endif
		case COMPRESS_LZX:
#if WITH_XDK
			bUncompressSucceeded = appUncompressMemoryLZX(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
			break;
#endif
		default:
			warnf(TEXT("appUncompressMemory - This compression type not supported"));
			bUncompressSucceeded = FALSE;
	}
	INC_FLOAT_STAT_BY(STAT_UncompressorTime,(FLOAT)(appSeconds()-UncompressorStartTime));
	
	return bUncompressSucceeded;
}

// !!! FIXME: move the Xbox code from LaunchEngineLoop.cpp here, too.
#if (!WITH_PANORAMA && !_XBOX)
DWORD appGetTitleId(void)
{
	static UBOOL Done = FALSE;
	static DWORD TitleId = 0;

	if (!Done)
	{
#if WITH_GAMESPY
		TitleId = appMemCrc(appGetGameSpyGameName(),appStrlen(appGetGameSpyGameName()) * sizeof(TCHAR));
#elif WITH_STEAMWORKS
		INT AppId = appGetSteamworksAppId();

		// If the appid could not be retrieved, try to provide a reasonably unique value
		if (AppId == INDEX_NONE)
		{
			TitleId = appMemCrc(appGetGameName(), appStrlen(appGetGameName()) * sizeof(TCHAR));
		}
		else
		{
			TitleId = AppId;
		}
#else
		TitleId = appMemCrc(appGetGameName(),appStrlen(appGetGameName()) * sizeof(TCHAR));
#endif
		Done = TRUE;
	}

	return TitleId;
}
#endif

/*-----------------------------------------------------------------------------
	FCompressedGrowableBuffer.
-----------------------------------------------------------------------------*/

/**
 * Constructor
 *
 * @param	InMaxPendingBufferSize	Max chunk size to compress in uncompressed bytes
 * @param	InCompressionFlags		Compression flags to compress memory with
 */
FCompressedGrowableBuffer::FCompressedGrowableBuffer( INT InMaxPendingBufferSize, ECompressionFlags InCompressionFlags )
:	MaxPendingBufferSize( InMaxPendingBufferSize )
,	CompressionFlags( InCompressionFlags )
,	CurrentOffset( 0 )
,	NumEntries( 0 )
,	DecompressedBufferBookKeepingInfoIndex( INDEX_NONE )
{
	PendingCompressionBuffer.Empty( MaxPendingBufferSize );
}

/**
 * Locks the buffer for reading. Needs to be called before calls to Access and needs
 * to be matched up with Unlock call.
 */
void FCompressedGrowableBuffer::Lock()
{
	check( DecompressedBuffer.Num() == 0 );
}

/**
 * Unlocks the buffer and frees temporary resources used for accessing.
 */
void FCompressedGrowableBuffer::Unlock()
{
	DecompressedBuffer.Empty();
	DecompressedBufferBookKeepingInfoIndex = INDEX_NONE;
}

/**
 * Appends passed in data to the buffer. The data needs to be less than the max
 * pending buffer size. The code will assert on this assumption.
 *
 * @param	Data	Data to append
 * @param	Size	Size of data in bytes.
 * @return	Offset of data, used for retrieval later on
 */
INT FCompressedGrowableBuffer::Append( void* Data, INT Size )
{
	check( DecompressedBuffer.Num() == 0 );
	check( Size <= MaxPendingBufferSize );
	NumEntries++;

	// Data does NOT fit into pending compression buffer. Compress existing data 
	// and purge buffer.
	if( MaxPendingBufferSize - PendingCompressionBuffer.Num() < Size )
	{
		// Allocate temporary buffer to hold compressed data. It is bigger than the uncompressed size as
		// compression is not guaranteed to create smaller data and we don't want to handle that case so 
		// we simply assert if it doesn't fit. For all practical purposes this works out fine and is what
		// other code in the engine does as well.
		INT CompressedSize = MaxPendingBufferSize * 4 / 3;
		void* TempBuffer = appMalloc( CompressedSize );

		// Compress the memory. CompressedSize is [in/out]
		verify( appCompressMemory( CompressionFlags, TempBuffer, CompressedSize, PendingCompressionBuffer.GetData(), PendingCompressionBuffer.Num() ) );

		// Append the compressed data to the compressed buffer and delete temporary data.
		INT StartIndex = CompressedBuffer.Add( CompressedSize );
		appMemcpy( &CompressedBuffer(StartIndex), TempBuffer, CompressedSize );
		appFree( TempBuffer );

		// Keep track of book keeping info for later access to data.
		FBufferBookKeeping Info;
		Info.CompressedOffset = StartIndex;
		Info.CompressedSize = CompressedSize;
		Info.UncompressedOffset = CurrentOffset - PendingCompressionBuffer.Num();
		Info.UncompressedSize = PendingCompressionBuffer.Num();
		BookKeepingInfo.AddItem( Info ); 

		// Resize & empty the pending buffer to the default state.
		PendingCompressionBuffer.Empty( MaxPendingBufferSize );
	}

	// Appends the data to the pending buffer. The pending buffer is compressed
	// as needed above.
	INT StartIndex = PendingCompressionBuffer.Add( Size );
	appMemcpy( &PendingCompressionBuffer(StartIndex), Data, Size );

	// Return start offset in uncompressed memory.
	INT StartOffset = CurrentOffset;
	CurrentOffset += Size;
	return StartOffset;
}

/**
 * Accesses the data at passed in offset and returns it. The memory is read-only and
 * memory will be freed in call to unlock. The lifetime of the data is till the next
 * call to Unlock, Append or Access
 *
 * @param	Offset	Offset to return corresponding data for
 */
void* FCompressedGrowableBuffer::Access( INT Offset )
{
	void* UncompressedData = NULL;

	// Check whether the decompressed data is already cached.
	if( DecompressedBufferBookKeepingInfoIndex != INDEX_NONE )
	{
		const FBufferBookKeeping& Info = BookKeepingInfo(DecompressedBufferBookKeepingInfoIndex);
		// Cache HIT.
		if( (Info.UncompressedOffset <= Offset) && (Info.UncompressedOffset + Info.UncompressedSize > Offset) )
		{
			// Figure out index into uncompressed data and set it. DecompressionBuffer (return value) is going 
			// to be valid till the next call to Access or Unlock.
			INT InternalOffset = Offset - Info.UncompressedOffset;
			UncompressedData = &DecompressedBuffer(InternalOffset);
		}
		// Cache MISS.
		else
		{
			DecompressedBufferBookKeepingInfoIndex = INDEX_NONE;
		}
	}

	// Traverse book keeping info till we find the matching block.
	if( UncompressedData == NULL )
	{
		for( INT InfoIndex=0; InfoIndex<BookKeepingInfo.Num(); InfoIndex++ )
		{
			const FBufferBookKeeping& Info = BookKeepingInfo(InfoIndex);
			if( (Info.UncompressedOffset <= Offset) && (Info.UncompressedOffset + Info.UncompressedSize > Offset) )
			{
				// Found the right buffer, now decompress it.
				DecompressedBuffer.Empty( Info.UncompressedSize );
				DecompressedBuffer.Add( Info.UncompressedSize );
				verify( appUncompressMemory( CompressionFlags, DecompressedBuffer.GetData(), Info.UncompressedSize, &CompressedBuffer(Info.CompressedOffset), Info.CompressedSize ) );

				// Figure out index into uncompressed data and set it. DecompressionBuffer (return value) is going 
				// to be valid till the next call to Access or Unlock.
				INT InternalOffset = Offset - Info.UncompressedOffset;
				UncompressedData = &DecompressedBuffer(InternalOffset);	

				// Keep track of buffer index for the next call to this function.
				DecompressedBufferBookKeepingInfoIndex = InfoIndex;
				break;
			}
		}
	}

	// If we still haven't found the data it might be in the pending compression buffer.
	if( UncompressedData == NULL )
	{
		INT UncompressedStartOffset = CurrentOffset - PendingCompressionBuffer.Num();
		if( (UncompressedStartOffset <= Offset) && (CurrentOffset > Offset) )
		{
			// Figure out index into uncompressed data and set it. PendingCompressionBuffer (return value) 
			// is going to be valid till the next call to Access, Unlock or Append.
			INT InternalOffset = Offset - UncompressedStartOffset;
			UncompressedData = &PendingCompressionBuffer(InternalOffset);
		}
	}

	// Return value is only valid till next call to Access, Unlock or Append!
	check( UncompressedData );
	return UncompressedData;
}


/*-----------------------------------------------------------------------------
	Blob functionality.
-----------------------------------------------------------------------------*/

/**
 * Converts a buffer to a string by hex-ifying the elements
 *
 * @param SrcBuffer the buffer to stringify
 * @param SrcSize the number of bytes to convert
 *
 * @return the blob in string form
 */
FString appBlobToString(const BYTE* SrcBuffer,const DWORD SrcSize)
{
	FString Result;
	// Convert and append each byte in the buffer
	for (DWORD Count = 0; Count < SrcSize; Count++)
	{
		Result += FString::Printf(TEXT("%03d"),(DWORD)SrcBuffer[Count]);
	}
	return Result;
}

/**
 * Converts a string into a buffer
 *
 * @param DestBuffer the buffer to fill with the string data
 * @param DestSize the size of the buffer in bytes (must be at least string len / 2)
 *
 * @return TRUE if the conversion happened, FALSE otherwise
 */
UBOOL appStringToBlob(const FString& Source,BYTE* DestBuffer,const DWORD DestSize)
{
	// Make sure the buffer is at least half the size and that the string is an
	// even number of characters long
	if (DestSize >= (DWORD)(Source.Len() / 3) &&
		(Source.Len() % 3) == 0)
	{
		TCHAR ConvBuffer[4];
		ConvBuffer[3] = TEXT('\0');
		INT WriteIndex = 0;
		// Walk the string 2 chars at a time
		for (INT Index = 0; Index < Source.Len(); Index += 3, WriteIndex++)
		{
			ConvBuffer[0] = Source[Index];
			ConvBuffer[1] = Source[Index + 1];
			ConvBuffer[2] = Source[Index + 2];
			DestBuffer[WriteIndex] = appAtoi(ConvBuffer);
		}
		return TRUE;
	}
	return FALSE;
}

/** 
 * Returns the string name of the given platform 
 *
 * @param InPlatform The platform of interest (UE3::PlatformType)
 *
 * @return The name of the platform, "" if not found
 */
FString appPlatformTypeToString(UE3::EPlatformType Platform)
{
	switch (Platform)
	{
		case UE3::PLATFORM_Windows:
			return TEXT("PC");

		case UE3::PLATFORM_WindowsConsole:
			return TEXT("PCConsole");

		case UE3::PLATFORM_WindowsServer:
			return TEXT("PCServer");

		case UE3::PLATFORM_Xbox360:
			return TEXT("Xbox360");

		case UE3::PLATFORM_PS3:
			return TEXT("PS3");

		case UE3::PLATFORM_Linux:
			return TEXT("Linux");

		case UE3::PLATFORM_MacOSX:
			return TEXT("Mac");

		case UE3::PLATFORM_IPhone:
			return TEXT("IPhone");

		case UE3::PLATFORM_Android:
			return TEXT("Android");

		case UE3::PLATFORM_NGP:
			return TEXT("NGP");

		case UE3::PLATFORM_WiiU:
			return TEXT("WiiU");

		case UE3::PLATFORM_Flash:
			return TEXT("Flash");
		default:
			return TEXT("");
	}	
}

/** 
 * Returns the string name of the given platform 
 *
 * @param InPlatform The platform of interest (UE3::PlatformType)
 *
 * @return The name of the platform, "" if not found
 */
FString appPlatformTypeToStringEx(UE3::EPlatformType Platform)
{
	switch (Platform)
	{
		case UE3::PLATFORM_Windows:
#if _WIN64
			return TEXT("Win64");
#else
			return TEXT("Win32");
#endif

		case UE3::PLATFORM_WindowsConsole:
#if _WIN64
			return TEXT("Win64Console");
#else
			return TEXT("Win32Console");
#endif

		case UE3::PLATFORM_WindowsServer:
#if _WIN64
			return TEXT("Win64Server");
#else
			return TEXT("Win32Server");
#endif

		case UE3::PLATFORM_Xbox360:
			return TEXT("Xbox360");

		case UE3::PLATFORM_PS3:
			return TEXT("PS3");

		case UE3::PLATFORM_Linux:
			return TEXT("Linux");

		case UE3::PLATFORM_MacOSX:
			return TEXT("Mac");

		case UE3::PLATFORM_IPhone:
			return TEXT("IPhone");

		case UE3::PLATFORM_Android:
			return TEXT("Android");

		case UE3::PLATFORM_NGP:
			return TEXT("NGP");

		case UE3::PLATFORM_WiiU:
			return TEXT("WiiU");

		default:
			return TEXT("");
	}	
}

/** 
 * Returns the list of valid platforms, in <platform 1>|<platform 2>|...|<platform N> style.
 *
 * @return The list of valid platforms
 */
FString appValidPlatformsString()
{
	return TEXT("xbox360|pc|pcserver|pcconsole|ps3|linux|macosx|iphone|android|ngp|wiiu");
}

/** 
 * Returns the enumeration value for the given platform 
 *
 * @param InPlatform The platform of interest
 *
 * @return The platform type, or PLATFORM_Unknown if bad input
 */
UE3::EPlatformType appPlatformStringToType(const FString& PlatformStr)
{
	if (PlatformStr == TEXT("ps3"))
	{
		return UE3::PLATFORM_PS3;
	}
	else if (PlatformStr == TEXT("xbox360"))
	{	
		return UE3::PLATFORM_Xbox360;
	}
	else if (PlatformStr == TEXT("pc") || PlatformStr == TEXT("win32") || PlatformStr == TEXT("win64"))
	{
		return UE3::PLATFORM_Windows;
	}
	else if (PlatformStr == TEXT("pcconsole") || PlatformStr == TEXT("win32console") || PlatformStr == TEXT("win64console"))
	{
		return UE3::PLATFORM_WindowsConsole;
	}
	else if (PlatformStr == TEXT("pcserver") || PlatformStr == TEXT("win32server") || PlatformStr == TEXT("win64server"))
	{
		return UE3::PLATFORM_WindowsServer;
	}
	else if (PlatformStr == TEXT("iphone"))
	{
		return UE3::PLATFORM_IPhone;
	}
	else if (PlatformStr == TEXT("android"))
	{
		return UE3::PLATFORM_Android;
	}
	else if (PlatformStr == TEXT("ngp"))
	{
		return UE3::PLATFORM_NGP;
	}
	else if (PlatformStr == TEXT("linux"))
	{
		return UE3::PLATFORM_Linux;
	}
	else if (PlatformStr == TEXT("mac") || PlatformStr == TEXT("macosx"))
	{
		return UE3::PLATFORM_MacOSX;
	}
	else if (PlatformStr == TEXT("wiiu"))
	{
		return UE3::PLATFORM_WiiU;
	}
	else if (PlatformStr == TEXT("flash"))
	{
		return UE3::PLATFORM_Flash;
	}
	else
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(NAME_Error, TEXT("Unknown platform (%s) specified"), *PlatformStr);
		CLEAR_WARN_COLOR();

		return UE3::PLATFORM_Unknown;
	}
}

/** 
 * @return Enumerated type for the current, compiled platform 
 */
UE3::EPlatformType appGetPlatformType()
{
#if _MSC_VER && !XBOX
	#if DEDICATED_SERVER
		return UE3::PLATFORM_WindowsServer;
	#else
		if (GIsSeekFreePCConsole)
		{
			return UE3::PLATFORM_WindowsConsole;
		}
		else if (GIsSeekFreePCServer)
		{
			return UE3::PLATFORM_WindowsServer;
		}
		return UE3::PLATFORM_Windows;
	#endif //DEDICATED_SERVER
#elif XBOX
	return UE3::PLATFORM_Xbox360;
#elif PS3
	return UE3::PLATFORM_PS3;
#elif IPHONE
	return UE3::PLATFORM_IPhone;
#elif ANDROID
	return UE3::PLATFORM_Android;
#elif NGP
	return UE3::PLATFORM_NGP;
#elif PLATFORM_MACOSX
	return UE3::PLATFORM_MacOSX;
#elif WIIU
	return UE3::PLATFORM_WiiU;
#elif FLASH
	return UE3::PLATFORM_Flash;
// we put this last, because many platforms define it to be true. 
// @todo: Use a better #define that is for Linux only, not unix clones
#elif PLATFORM_LINUX
	return UE3::PLATFORM_Linux;
#else
	#error Please define your platform.
#endif
}

FString appGetPlatformString()
{
	// get the platform enum, then convert that to a string
	return appPlatformTypeToString(appGetPlatformType());
}

FString appGetPlatformStringEx()
{
	// get the platform enum, then convert that to a string
	return appPlatformTypeToStringEx(appGetPlatformType());
}


/** 
 * Return the current configuration of this binary (Debug, Release, Shipping or Test)
 */
FString appGetConfigurationString()
{
#if _DEBUG
	return FString( TEXT( "Debug" ) );
#elif NDEBUG
	return FString( TEXT( "Release" ) );
#elif FINAL_RELEASE
	return FString( TEXT( "Shipping" ) );
#elif FINAL_RELEASE_DEBUGCONSOLE
	return FString( TEXT( "Test" ) );
#else
	return FString( TEXT( "Unknown" ) );
#endif
}

/**
 * Detects whether we're running in a 64-bit operating system.
 *
 * @return	TRUE if we're running in a 64-bit operating system
 */
UBOOL appIs64bitOperatingSystem()
{
#if defined(_WIN64)
	return TRUE;
#elif defined(_WINDOWS)
	#pragma warning( push )
	#pragma warning( disable: 4191 )	// unsafe conversion from 'type of expression' to 'type required'
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress( GetModuleHandle(TEXT("kernel32")), "IsWow64Process" );
	BOOL bIsWoW64Process = FALSE;
	if ( fnIsWow64Process != NULL )
	{
		if ( fnIsWow64Process(GetCurrentProcess(), &bIsWoW64Process) == 0 )
		{
			bIsWoW64Process = FALSE;
		}
	}
	#pragma warning( pop )
	return bIsWoW64Process;
#else
	return sizeof(void*) == 8;
#endif
}

UBOOL appGetCookedContentPath(UE3::EPlatformType InPlatform, FString& OutPath)
{
	FString CookedDirName;

	CookedDirName = TEXT("Cooked");
	CookedDirName += appPlatformTypeToString(InPlatform);

	OutPath = appGameDir() + CookedDirName + PATH_SEPARATOR;
	return TRUE;
}

/**
 *	Get the cooked content path for the given platform.
 *
 *	@param	InPlatform		The platform of interest
 *	@param	InDLCName		The name of the DLC being cooked; empty if none
 *	@param	OutPath			Will be filled in with the cooked content path 
 *
 *	@return	UBOOL			TRUE if successful, FALSE if not.
 */
UBOOL appCookedContentPath(UE3::EPlatformType InPlatform, FString& InDLCName, FString& OutPath)
{
	FString CookedDir;
	FString PlatformName;

	// user mode cooking requires special handling
	if (InDLCName.Len())
	{
		CookedDir = appGameDir();
		PlatformName = appPlatformTypeToString(InPlatform);
		switch (InPlatform)
		{
		// cooking unpublished data goes to published
		case UE3::PLATFORM_Windows:
		case UE3::PLATFORM_WindowsConsole:
		case UE3::PLATFORM_WindowsServer:
				CookedDir += FString::Printf(TEXT("Published\\Cooked%s\\"),*PlatformName);
				break;
		// Console go into DLC directory
		case UE3::PLATFORM_PS3:
		case UE3::PLATFORM_Xbox360:
			CookedDir += FString::Printf(TEXT("DLC\\%s\\%s\\Content\\%sGame\\Cooked%s\\"),*PlatformName,*InDLCName,GGameName,*PlatformName);
			break;
		}
	}
	else
	{
		appGetCookedContentPath(InPlatform, CookedDir);
	}
	
	OutPath = CookedDir;
	return (CookedDir.Len() > 0);
}

/** 
 * Determines which, if any, platform was specified in a string (usually command line) 
 * 
 * @param String String to look in for platform
 *
 * @return Platform specified, or PLATFORM_Unknown if not specified
 */
UE3::EPlatformType ParsePlatformType(const TCHAR* String)
{
	FString PlatformStr;
	if (Parse(String, TEXT("PLATFORM="), PlatformStr))
	{
		return appPlatformStringToType(PlatformStr);
	}

	// return unknown on failure
	return UE3::PLATFORM_Unknown;
}

/**
 * Parses a string into tokens, separating switches (beginning with - or /) from
 * other parameters
 *
 * @param	CmdLine		the string to parse
 * @param	Tokens		[out] filled with all parameters found in the string
 * @param	Switches	[out] filled with all switches found in the string
 */
void appParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while (ParseToken(CmdLine, NextToken, FALSE))
	{
		if ((**NextToken == TCHAR('-')) || (**NextToken == TCHAR('/')))
		{
			new(Switches) FString(NextToken.Mid(1));
		}
		else
		{
			new(Tokens) FString(NextToken);
		}
	}
}

#define SBO_KEY_LENGTH (761)

#if 0
/**
 * Helper function to generate a key
 */
void GenerateSecurityByObscurityKey()
{
	TArray<BYTE> File;
	appLoadFileToArray(File,TEXT("..\\..\\UDKGame\\CookedXenon\\Textures.tfc"));
	check(File.Num() > SBO_KEY_LENGTH * 100);

	TArray<BYTE> Result;
	Result.Add(SBO_KEY_LENGTH); // don't care if this is uninitialized...that would be better

	INT ReadOffset = SBO_KEY_LENGTH * 3 + 1019;
	INT WriteOffset = 631;

	while (ReadOffset < File.Num() - (SBO_KEY_LENGTH * 5 + 929))
	{
		BYTE In=File(ReadOffset++);
		WriteOffset = WriteOffset % SBO_KEY_LENGTH;
		Result(WriteOffset) = Result(WriteOffset) ^ In; 
		WriteOffset++;
	}

	INT Index = 0;
	while (Index < SBO_KEY_LENGTH)
	{
		FString Line = TEXT("\t");
		for (INT SubIndex = 0; SubIndex < 50 && Index < SBO_KEY_LENGTH;SubIndex++,Index++)
		{
			Line+= FString::Printf(TEXT("0x%02X,"),(INT)Result(Index));
		}
		debugf(TEXT("%s"),*Line);
	}
}

/**
 * Function to test SBO encryption
 * IMPORTANT: Must match ShaderCompileWorker.cpp
 */
void SecurityByObscurityEncryptAndDecryptTest()
{
	TArray<BYTE> File;
	appLoadFileToArray(File,TEXT("..\\..\\UDKGame\\CookedXenon\\Lighting.tfc"));
	TArray<BYTE> EncryptedFile = File;
	check(EncryptedFile.Num());
	SecurityByObscurityEncryptAndDecrypt(EncryptedFile);
	TArray<BYTE> DecryptedFile = EncryptedFile;
	SecurityByObscurityEncryptAndDecrypt(DecryptedFile);
	check(appMemcmp(&EncryptedFile(0),&DecryptedFile(0),EncryptedFile.Num()));
	check(!appMemcmp(&DecryptedFile(0),&DecryptedFile(0),EncryptedFile.Num()));
	check(EncryptedFile.Num()==DecryptedFile.Num());
	debugf(TEXT("SecurityByObscurityEncryptAndDecryptTest completed."));
}

#endif

static BYTE SecurityByObscurityKey[SBO_KEY_LENGTH] = 
{
 	0xAD,0x50,0x3B,0x59,0x5A,0x5D,0x33,0x5C,0xEA,0xA6,0x58,0xA7,0xED,0x28,0xF4,0x90,0x9B,0x6F,0x60,0x70,0x50,0x30,0x3E,0x5E,0x0F,0x3A,0xB6,0xC4,0xEE,0x91,0xF7,0x2F,0x6D,0xEE,0x6F,0xAE,0x2D,0x9E,0x6F,0x20,0x94,0x6D,0x7A,0x6B,0x21,0x88,0xD4,0xF8,0xAE,0x74,
 	0xDF,0x08,0x31,0x2A,0xEE,0x9D,0xA3,0x33,0xB7,0x57,0x60,0xB0,0x60,0xD5,0x88,0x4B,0x07,0xC4,0xFB,0x02,0x42,0x84,0x02,0x74,0xA6,0x6A,0x45,0x70,0x65,0x7D,0xA5,0xDB,0x4F,0x8B,0xF3,0xF4,0x95,0x8B,0xFF,0xF8,0x75,0x86,0xB0,0x8F,0xFF,0x4D,0xDC,0xE7,0xC2,0xF1,
 	0xF2,0x53,0xCA,0xDF,0x0B,0x6B,0x80,0x5C,0xF8,0x97,0x6F,0x67,0x33,0x98,0xD5,0xE3,0x06,0x50,0x95,0x53,0xF1,0x16,0x4A,0xAA,0xC7,0x3D,0x94,0x48,0x7F,0xAE,0xAE,0x40,0x98,0x5D,0x22,0x57,0xB7,0xB7,0xE6,0x83,0x21,0x3F,0xD5,0xF3,0xDD,0x8C,0x28,0x01,0xBC,0xFC,
 	0x62,0xDF,0x93,0x14,0x86,0x6C,0xF2,0xC4,0x29,0xEC,0x4C,0x9A,0x24,0x7B,0x2F,0x19,0xAD,0x0C,0x62,0x9B,0x0F,0x36,0xC5,0x0E,0x4C,0xDF,0xA1,0xA8,0x55,0xAB,0x81,0x06,0x07,0x43,0x69,0xD7,0x2D,0x9F,0x7C,0x83,0xD0,0x7B,0xB5,0x92,0xE9,0xB2,0x3E,0x40,0xED,0xB5,
 	0x72,0xB4,0x16,0x4E,0x7C,0x6F,0xD7,0x3F,0x00,0xD1,0x4F,0x65,0x63,0x4A,0x9F,0xB9,0x2B,0x50,0x99,0x5F,0x65,0x66,0xFF,0x1B,0x8B,0x07,0x4A,0x9E,0xEC,0x66,0x4B,0xEB,0xCB,0x25,0x14,0xDD,0xC7,0x79,0xAE,0x5B,0xC8,0xA0,0x85,0x8E,0x1E,0x62,0x17,0xD6,0x19,0x5C,
 	0xC0,0x1B,0x0E,0xD8,0x3A,0xDC,0x57,0xB8,0xC0,0x8E,0xB8,0x97,0x9E,0x21,0x14,0x19,0x9C,0x27,0xC5,0x30,0x54,0x87,0x5B,0xB3,0x3C,0x86,0xB8,0x4F,0x94,0xD1,0xAE,0x16,0x03,0x7B,0xC1,0x34,0x9F,0x91,0xC6,0x3B,0xA4,0x1B,0x1C,0x17,0x3F,0x98,0xBD,0xC8,0x17,0xE9,
 	0x2A,0x3D,0x96,0x54,0x53,0x40,0xBE,0x26,0x2D,0xE2,0x59,0x2A,0x1A,0x80,0xD0,0x5C,0xDE,0xBC,0xBC,0x0C,0x62,0x08,0x5A,0xB5,0xE8,0xC3,0x11,0xF5,0xA3,0xFA,0xE3,0x7E,0xB9,0x80,0x00,0xA1,0xC7,0x26,0x0C,0x1B,0xF7,0xB5,0x13,0xE4,0xDD,0x2A,0x43,0x81,0x23,0xE8,
 	0x85,0x5D,0x53,0xE1,0x30,0xEB,0x61,0x86,0x4F,0xB7,0x72,0x46,0xA6,0x84,0x57,0x63,0x8E,0xAE,0x07,0xD2,0xE6,0x3E,0xA9,0x83,0x3C,0x9E,0x81,0x3B,0x52,0x45,0x6A,0xC4,0x63,0x0A,0x04,0xAB,0xEB,0xD5,0x50,0x13,0x85,0x95,0x64,0x3E,0xFA,0x2D,0x52,0x5F,0x8B,0xB4,
 	0x3B,0x88,0xD3,0xD4,0x5B,0x7B,0x52,0x77,0xD1,0x04,0x7B,0xBD,0x41,0x1B,0xE9,0x8D,0x70,0x22,0x60,0x80,0x47,0xD3,0x92,0x8C,0x55,0xBB,0xFD,0x7C,0x1B,0x29,0x1D,0x6D,0x15,0x43,0x3B,0xE5,0x69,0x7E,0x1E,0x3B,0x80,0x7D,0xEC,0x33,0x28,0x3D,0x5B,0x6F,0xD6,0xF2,
 	0x06,0xC2,0xF3,0x52,0xC2,0x88,0x90,0x25,0x86,0x84,0xCF,0x72,0xCF,0xB7,0xAB,0xEC,0x9E,0xE4,0xD9,0x21,0x70,0x6D,0x53,0xCF,0xDC,0xB9,0xA1,0x38,0x92,0x75,0x94,0xFA,0x08,0x70,0xA6,0xC4,0x85,0x6C,0xCE,0x3D,0xFD,0x7F,0x4C,0x5B,0xD6,0x47,0x53,0xA5,0xF8,0x49,
 	0xE8,0x79,0xF3,0xA8,0xD2,0x45,0xEF,0x1C,0x78,0xA7,0xE2,0x6E,0x26,0x48,0xCF,0x2A,0xBD,0xBE,0x2F,0xDA,0xDE,0xE4,0x5E,0x4F,0x5E,0x8F,0xB2,0x02,0xD7,0xEB,0xF0,0x28,0xDB,0xB9,0x69,0xA2,0xD2,0xA1,0x7A,0x08,0x08,0x48,0xB5,0x36,0x02,0x67,0x31,0x9F,0xCD,0xCB,
 	0xF3,0xE5,0x0B,0xFF,0xAE,0xFB,0xB5,0xC4,0x69,0xB1,0xC8,0xFE,0xE3,0xE6,0x42,0x9F,0xF4,0x86,0xDD,0xDF,0x43,0xC2,0xF7,0x67,0xCB,0x08,0xB1,0xD6,0x59,0x08,0xE1,0xB2,0x28,0xC3,0x93,0xE7,0x5E,0x06,0x5F,0xF7,0x2F,0x29,0xBA,0x3A,0xAD,0x54,0x15,0x03,0x86,0x9F,
 	0xC8,0xC7,0xE3,0x82,0xA7,0xFB,0xFD,0x5D,0xA7,0xE6,0xA1,0xBC,0x3A,0x79,0xFD,0x8B,0x29,0x2D,0x1C,0x46,0x2F,0x22,0x07,0xC3,0xEA,0x39,0xAC,0x5C,0x3D,0xDA,0xE5,0x10,0x6A,0xF0,0x22,0x76,0xC5,0x04,0x0F,0x4A,0x68,0xE9,0x95,0x37,0xE6,0x17,0x0D,0x53,0x25,0x46,
 	0x50,0x4C,0xD9,0x98,0x16,0xC6,0x8A,0xE1,0x34,0x69,0xD8,0xD7,0x73,0xAA,0x9C,0x53,0x64,0xE8,0x77,0x2B,0xD5,0xF8,0x3E,0xAB,0x1C,0x97,0x49,0x97,0x39,0x9A,0xE3,0xF3,0x3E,0x8C,0xEB,0xF2,0x05,0x2C,0x8F,0x7B,0x3E,0xCF,0xD1,0xA1,0xDE,0xA9,0x8C,0x2F,0x6E,0x2F,
 	0x4A,0x26,0xDF,0x31,0xD2,0xAD,0xBB,0xA4,0x25,0x90,0x3D,0xC6,0x16,0x90,0x2C,0xA8,0x67,0xC6,0xC5,0x10,0x87,0x1B,0x20,0xCB,0x04,0x4A,0x6B,0x07,0x75,0xC0,0x68,0x5D,0xE6,0x5B,0x4D,0x98,0xF4,0x43,0x37,0x00,0xC2,0x10,0x0C,0xAD,0x7D,0xEF,0xEF,0xA2,0xA0,0x25,
 	0xD8,0xFB,0xDC,0xA9,0x8D,0x01,0x83,0x52,0x2C,0x5B,0xAC
};

/**
 * Function to encrypt and decrypt an array of bytes using obscurity
 * 
 * @param InAndOutData data to encrypt or decrypt, and also the result
 * @param Offset byte-offset to start encrypting/decrypting
 */
void SecurityByObscurityEncryptAndDecrypt(TArray<BYTE>& InAndOutData, INT Offset/*=0*/)
{
	INT Size = InAndOutData.Num() - Offset;
	if ( Size > 0 )
	{
		DWORD KeyOffset = 240169 + (244109 * Size);
		for (INT Index = Offset; Index < InAndOutData.Num(); Index++, KeyOffset++)
		{
			KeyOffset = KeyOffset % SBO_KEY_LENGTH;
			InAndOutData(Index) = InAndOutData(Index) ^ SecurityByObscurityKey[KeyOffset]; 
		}
	}
}

/**
 * Takes the property name and breaks it down into a human readable string.
 * For example - "bCreateSomeStuff" becomes "Create Some Stuff?" and "DrawScale3D" becomes "Draw Scale 3D".
 * 
 * @param	InOutPropertyDisplayName	[In, Out] The property name to sanitize
 * @param	bIsBoolProperty				True if the property is a bool property
 */
void SanitizePropertyDisplayName( FString& InOutPropertyDisplayName, const UBOOL bIsBoolProperty )
{
	TArray<TCHAR> Chars;
	Chars = InOutPropertyDisplayName.GetCharArray();

	InOutPropertyDisplayName.Empty();

	// This is used to indicate that we are in a run of uppercase letter and/or digits.  The code attempts to keep
	// these characters together as breaking them up often looks silly (i.e. "Draw Scale 3 D" as opposed to "Draw Scale 3D"
	UBOOL bInARun = FALSE;

	for( INT CharIndex = 0 ; CharIndex < Chars.Num() ; ++CharIndex )
	{
		TCHAR ch = Chars(CharIndex);

		UBOOL bLowerCase = appIsLower( ch );
		UBOOL bUpperCase = appIsUpper( ch );
		UBOOL bIsDigit = appIsDigit( ch );
		UBOOL bIsUnderscore = appIsUnderscore( ch );

		// Skip the first character if the property is a bool (they should all start with a lowercase 'b', which we don't want to keep)
		if( CharIndex == 0 && bIsBoolProperty && ch == 'b' )
		{
			continue;
		}

		// If the current character is upper case or a digit, and the previous character wasn't, then we need to insert a space
		if( (bUpperCase || bIsDigit) && !bInARun )
		{
			if( InOutPropertyDisplayName.Len() > 0 )
			{
				InOutPropertyDisplayName += TEXT(" ");
			}
			bInARun = TRUE;
		}
		
		// A lower case character will break a run of upper case letters and/or digits
		if( bLowerCase )
		{
			bInARun = FALSE;
		}

		if( bIsUnderscore )
		{
			ch = TEXT( ' ' );
			bInARun = TRUE;
		}

		InOutPropertyDisplayName  += ch;
	}
}

/** 
 *  HELPER FUNCTIONS - Shared between Cooker / PatchCommandlet
 */

/**
 * @return The name of the directory where cooked ini files go
 */
FString GetConfigOutputDirectory(UE3::EPlatformType Platform)
{
	return appPlatformTypeToString(Platform) * TEXT("Cooked");
}

/**
 * @return The prefix to pass to appCreateIniNamesAndThenCheckForOutdatedness for non-platform specific inis
 */
FString GetConfigOutputPrefix(UE3::EPlatformType Platform)
{
	return GetConfigOutputDirectory(Platform) + PATH_SEPARATOR;
}

/**
 * @return The prefix to pass to appCreateIniNamesAndThenCheckForOutdatedness for platform specific inis
 */
FString GetPlatformConfigOutputPrefix(UE3::EPlatformType Platform)
{
	return GetConfigOutputPrefix(Platform) + appPlatformTypeToString(Platform) + TEXT("-");
}

/**
 * @return The default ini prefix to pass to appCreateIniNamesAndThenCheckForOutdatedness for 
 */
FString GetPlatformDefaultIniPrefix(UE3::EPlatformType Platform)
{
	return appPlatformTypeToString(Platform) * appPlatformTypeToString(Platform);
}

/*
 * Make sure all the cooked platform inis are up to date for a given platform
 *
 * @param Platform - the platform being cooked
 * @param PlatformEngineConfigFilename - [out] the generated "engine.ini" file that results for the platform
 * @param PlatformSystemSettingsConfigName - [out] the generated "systemsettings.ini" file that results for the platform
 */
void UpdateCookedPlatformIniFilesFromDefaults(UE3::EPlatformType Platform, TCHAR PlatformEngineConfigFilename[], TCHAR PlatformSystemSettingsConfigName[])
{
	// make sure the output config directory exists
	GFileManager->MakeDirectory(*(appGameConfigDir() + GetConfigOutputDirectory(Platform)));

	UINT YesNoToAll = ART_No;
	TCHAR PlatformIniName[MAX_SPRINTF] = TEXT("");
	TCHAR PlatformDefaultIniName[MAX_SPRINTF] = TEXT("");

	const UBOOL bTryToPreserveContents = FALSE;

	// assemble standard ini files
	appCreateIniNames(
		PlatformEngineConfigFilename, 
		PlatformDefaultIniName,
		NULL, NULL, 
		TEXT("Engine.ini"), 
		*GetPlatformDefaultIniPrefix(Platform),
		*GetPlatformConfigOutputPrefix(Platform));

	appCheckIniForOutdatedness(
		PlatformEngineConfigFilename, 
		PlatformDefaultIniName,
		bTryToPreserveContents,
		YesNoToAll);

	appCreateIniNames(
		PlatformIniName, 
		PlatformDefaultIniName,
		NULL, NULL, 
		TEXT("Game.ini"), 
		*GetPlatformDefaultIniPrefix(Platform),
		*GetPlatformConfigOutputPrefix(Platform));	

	appCheckIniForOutdatedness(
		PlatformIniName, 
		PlatformDefaultIniName,
		bTryToPreserveContents,
		YesNoToAll);

	appCreateIniNames(
		PlatformIniName, 
		PlatformDefaultIniName,
		NULL, NULL, 
		TEXT("Input.ini"), 
		*GetPlatformDefaultIniPrefix(Platform),
		*GetPlatformConfigOutputPrefix(Platform));

	appCheckIniForOutdatedness(
		PlatformIniName, 
		PlatformDefaultIniName,
		bTryToPreserveContents,
		YesNoToAll);

	appCreateIniNames(
		PlatformIniName, 
		PlatformDefaultIniName,
		NULL, NULL, 
		TEXT("UI.ini"), 
		*GetPlatformDefaultIniPrefix(Platform),
		*GetPlatformConfigOutputPrefix(Platform));

	appCheckIniForOutdatedness(
		PlatformIniName, 
		PlatformDefaultIniName,
		bTryToPreserveContents,
		YesNoToAll);

	appCreateIniNames(
		PlatformSystemSettingsConfigName, 
		PlatformDefaultIniName,
		NULL, NULL, 
		TEXT("SystemSettings.ini"), 
		*GetPlatformDefaultIniPrefix(Platform),
		*GetPlatformConfigOutputPrefix(Platform));

	appCheckIniForOutdatedness(
		PlatformSystemSettingsConfigName, 
		PlatformDefaultIniName,
		bTryToPreserveContents,
		YesNoToAll);
}


struct FProfNodeData
{
	enum 				{ MAX_DEPTH = 32 };
	enum  				{ FLAG_FORCE_PRINT = 0x1,
						  FLAG_EVENT = 0x2 };
	FString			    TimerName[MAX_DEPTH];
	DOUBLE			    TimerStartTime[MAX_DEPTH];
	DWORD 				TimerFlags[MAX_DEPTH];
	INT				    TimerIndex;
	INT  			    DepthThreshold;
	FLOAT 			    TimeThreshold;
	DWORD 			    ThreadId;
};


/**
 * Gets the TLS Slot for FProfNodeData
 *
 * @return The TLS Slot used for FProfNodeData
 */

static DWORD ProfNodeGetTlsIndex()
{
	static DWORD TlsIndex = appAllocTlsSlot();
	return TlsIndex;
}


/**
 * Gets the copy of FProfNodeData stored in TLS
 *
 * @return The thread local PFrofNodeData
 */

static FProfNodeData * ProfNodeGetThreadData()
{
	FProfNodeData * ThreadProfData = (FProfNodeData*)appGetTlsValue(ProfNodeGetTlsIndex());

	if (!ThreadProfData)
	{
		ThreadProfData                 = new FProfNodeData();
		appMemset(ThreadProfData, 0, sizeof(FProfNodeData));
		ThreadProfData->ThreadId       = appGetCurrentThreadId();
		ThreadProfData->DepthThreshold = 2;
		ThreadProfData->TimeThreshold  = 0.1f;
		appSetTlsValue(ProfNodeGetTlsIndex(), ThreadProfData);
	}

	return ThreadProfData;
}


/**
 * Creates the entry point for a simple profiling node
 *
 * @param TimerName - The name of the section
 * @return The TimerIndex this node was inserted at
 */
int ProfNodeStart(const TCHAR * TimerName)
{
	FProfNodeData * ThreadProfData = ProfNodeGetThreadData();
	INT TimerIndex = ThreadProfData->TimerIndex++;
	// check(TimerIndex < FProfNodeData::MAX_DEPTH);
	ThreadProfData->TimerName[TimerIndex] = TimerName;
	ThreadProfData->TimerStartTime[TimerIndex] = appSeconds();
	ThreadProfData->TimerFlags[TimerIndex] = 0;
	return TimerIndex;
}


static FOutputDeviceRedirector       GProfNodeLogRedirector;
static FOutputDeviceRedirectorBase * GProfNodeLog= &GProfNodeLogRedirector;
static FOutputDeviceFile *           GProfNodeOutputDevice = NULL;

/**
 * Stops the current scope and logs out the timing information
 * @param AssumedTimerIndex - The TimerIndex that was returned from ProfNodeStart.
 *                          - This can be used to help track down mismatched nodes.
 */
void ProfNodeStop(int AssumedTimerIndex)
{
	FProfNodeData * ThreadProfData = ProfNodeGetThreadData();

	INT TimerIndex = --ThreadProfData->TimerIndex;
	DOUBLE StopTime = appSeconds();
	// These checks can be enabled to help debug ProfNode usage (such as mismatched START/STOP pairs)
	// check(TimerIndex >= 0);
	// check((AssumedTimerIndex != -1) ? (AssumedTimerIndex == TimerIndex) : true);

	FLOAT ElapsedTime = StopTime - ThreadProfData->TimerStartTime[TimerIndex];
	DWORD Flags = ThreadProfData->TimerFlags[TimerIndex];

	UBOOL bMustPrint = (Flags & FProfNodeData::FLAG_EVENT) || (Flags & FProfNodeData::FLAG_FORCE_PRINT);

	if ((TimerIndex < ThreadProfData->DepthThreshold) || (ElapsedTime > ThreadProfData->TimeThreshold) || bMustPrint)
	{
		if (GFileManager && !GProfNodeOutputDevice)
		{
			GProfNodeOutputDevice = new FOutputDeviceFile(*FString::Printf(TEXT("%s%s%s"), appBaseDir(), *appGameLogDir(), TEXT("ProfNode.log")), FALSE, FALSE);
			GProfNodeLog->AddOutputDevice( GProfNodeOutputDevice );
		}

		GProfNodeLog->Logf(TEXT("ProfNode:0x%08x, %d, %s, %f"), ThreadProfData->ThreadId, TimerIndex, *ThreadProfData->TimerName[TimerIndex], ElapsedTime);
		

		if (bMustPrint && ((TimerIndex - 1) >= 0))
		{
			 ThreadProfData->TimerFlags[TimerIndex-1] = FProfNodeData::FLAG_FORCE_PRINT;
		}
	}
}


/**
 * Inserts an event into the ProfNode output
 *
 * @param TimerName - The name of the event
 */
void ProfNodeEvent(const TCHAR * TimerName)
{
	INT Index = ProfNodeStart(TimerName);

	FProfNodeData * ThreadProfData = ProfNodeGetThreadData();
	ThreadProfData->TimerFlags[Index] = FProfNodeData::FLAG_EVENT;

	ProfNodeStop(Index);
}


/**
 * @param Threshold - The threshold in seconds below which timing information will not be printed out. Default is 0.1f
 * @return The old time treshold
 */
FLOAT ProfNodeSetTimeThresholdSeconds(FLOAT Threshold)
{
	FProfNodeData * ThreadProfData = ProfNodeGetThreadData();
	FLOAT PreviousThreshold        = ThreadProfData->TimeThreshold;
	ThreadProfData->TimeThreshold  = Threshold;
	return PreviousThreshold;
}


/**
 * @param Depth The depth level above shich timings will always be printed out.  Default is 2.
 * @return The old depth threshold
 */
INT ProfNodeSetDepthThreshold(INT Depth)
{
	FProfNodeData * ThreadProfData = ProfNodeGetThreadData();
	INT PreviousDepth              = ThreadProfData->DepthThreshold;
	ThreadProfData->DepthThreshold = Depth;
	return PreviousDepth;
}

/**
 * This sets the current thread to be the master thread for the ProfNode.log file
 */
void ProfNodeSetCurrentThreadAsMasterThread()
{
	GProfNodeLog->SetCurrentThreadAsMasterThread();
}

