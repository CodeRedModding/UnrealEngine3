/*=============================================================================
	MacWideChar.mm: Functions that allow using 2-byte TCHAR on Mac.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <CoreFoundation/CoreFoundation.h>
#include "Mac.h"

#define CP_UTF8 65001

void appMacUTF32ToUTF16(const unsigned int *Source, unsigned short *Dest, int Length)
{
	CFStringRef String = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)Source, Length * sizeof(UInt32), kCFStringEncodingUTF32LE, false);
	if (String)
	{
		CFStringGetBytes(String, CFRangeMake(0, Length), kCFStringEncodingUnicode, '?', false, (UInt8 *)Dest, Length * 2, NULL);
		Dest[Length] = 0;
		CFRelease(String);
	}
	else
	{
		Dest[0] = 0;
	}
}

void appMacWideCharToMultiByte(DWORD CodePage, const TCHAR *Source, DWORD LengthW, ANSICHAR *Dest, DWORD LengthA)
{
/*
	CFStringRef UTF16String = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)Source, LengthW);
	CFStringGetBytes(
		UTF16String,
		CFRangeMake(0, LengthW),
		(CodePage == CP_UTF8) ? kCFStringEncodingUTF8 : kCFStringEncodingASCII,
		'?',
		FALSE,
		(UInt8 *)Dest,
		LengthW,
		NULL);
	Dest[LengthW] = 0;
	CFRelease(UTF16String);
*/
	for (INT C = 0; C < LengthW+1; C++)
	{
		Dest[C] = Source[C] & 0xFF;
	}
}

void appMacMultiByteToWideChar(DWORD CodePage, const ANSICHAR *Source, TCHAR *Dest, DWORD Length)
{/*
	CFStringRef AnsiString = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)Source, Length, (CodePage == CP_UTF8) ? kCFStringEncodingUTF8 : kCFStringEncodingASCII, FALSE);
	CFStringGetBytes(
		AnsiString,
		CFRangeMake(0, Length),
		kCFStringEncodingUnicode,
		'?',
		FALSE,
		(UInt8 *)Dest,
		Length * 2,
		NULL);
	Dest[Length] = 0;
	CFRelease(AnsiString);*/

	for (INT C = 0; C < Length+1; C++)
	{
		Dest[C] = (BYTE)Source[C];
	}
}

#if !TCHAR_IS_4_BYTES

#include <wctype.h>

size_t _tcslen(const TCHAR *String)
{
	size_t Length = -1;

	do
	{
		Length++;
	}
	while (*String++);

	return Length;
}

// ===============================================
/** Class that handles the TCHAR to ANSI conversion */
class FTCHARToANSIDigitsOnly_Convert
{
public:
//	FORCEINLINE FTCHARToANSIDigitsOnly_Convert( DWORD InCodePage = CP_ACP ) {}

	FORCEINLINE ANSICHAR* Convert( const TCHAR* Source, ANSICHAR* Dest, DWORD Size )
	{
		// Determine whether we need to allocate memory or not
		DWORD LengthW = (DWORD)_tcslen( Source ) + 1;

		// Needs to be 2x the wide in case each converted char is multibyte
		DWORD LengthA = LengthW * 2;
		if( LengthA > Size )
		{
			// Need to allocate memory because the string is too big
			Dest = new char[LengthA * sizeof(ANSICHAR)];
		}

		// Now do the conversion
		for( INT C = 0; C < LengthW; C++ )
		{
			if( iswspace( Source[C] ) )
				Dest[C] = ' ';
			else if( iswascii( Source[C] ) )
			{
				check( ( Source[C] & 0xFF ) == Source[C] );
				Dest[C] = Source[C];
			}
			else
			{
				check( !iswxdigit( Source[C] ) );
				Dest[C] = 'y';	// neutral char for number reading
			}
		}

		return Dest;
	}

	// return the string length without the null terminator
	FORCEINLINE UINT Length( ANSICHAR* Dest )
	{
		return (UINT)strlen(Dest);
	}
};

typedef TStringConversion<ANSICHAR,TCHAR,FTCHARToANSIDigitsOnly_Convert> FTCHARToANSIDigitsOnly;

#define TCHAR_TO_ANSI_DIGITS_ONLY(str) (ANSICHAR*)FTCHARToANSIDigitsOnly((const TCHAR*)str)
// ===============================================


TCHAR *_tcscpy(TCHAR *StrDestination, const TCHAR *StrSource)
{
	TCHAR *BufPtr = StrDestination;

	while (*StrSource)
	{
		*BufPtr++ = *StrSource++;
	}

	*BufPtr = 0;

	return StrDestination;
}

TCHAR *_tcsstr(const TCHAR *String1, const TCHAR *String2)
{
	TCHAR Char1, Char2;
	if ((Char1 = *String2++) != 0)
	{
		size_t Length = _tcslen(String2);

		do
		{
			do
			{
				if ((Char2 = *String1++) == L'\0')
				{
					return NULL;
				}
			}
			while (Char1 != Char2);
		}
		while (_tcsncmp(String1, String2, Length) != 0);

		String1--;
	}

	return (TCHAR *)String1;
}

TCHAR *_tcschr(const TCHAR *String, TCHAR Char)
{
	while (*String != Char && *String != L'\0')
	{
		String++;
	}

	return (*String == Char) ? (TCHAR *)String : NULL;
}

TCHAR *_tcsrchr(const TCHAR *String, TCHAR Char)
{
	const TCHAR *Last = NULL;

	while (true)
	{
		if (*String == Char)
		{
			Last = String;
		}
		
		if (*String == L'\0')
		{
			break;
		}

		String++;
	}

	return (TCHAR *)Last;
}

TCHAR *_tcscat(TCHAR *String1, const TCHAR *String2)
{
	TCHAR *String = String1;

	while (*String != L'\0')
	{
		String++;
	}

	while ((*String++ = *String2++) != L'\0');

	return String1;
}

int _tcscmp(const TCHAR *String1, const TCHAR *String2)
{
	// walk the strings, comparing them case sensitively
	for (; *String1 || *String2; String1++, String2++)
	{
		TCHAR A = *String1, B = *String2;
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

int _tcsicmp(const TCHAR *String1, const TCHAR *String2)
{
	// walk the strings, comparing them case insensitively
	for (; *String1 || *String2; String1++, String2++)
	{
		TCHAR A = towupper(*String1), B = towupper(*String2);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

int _tcsncmp(const TCHAR *String1, const TCHAR *String2, size_t Count)
{
	// walk the strings, comparing them case sensitively, up to a max size
	for (; (*String1 || *String2) && Count; String1++, String2++, Count--)
	{
		TCHAR A = *String1, B = *String2;
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

unsigned long _tcstoul(const TCHAR *Ptr, TCHAR **EndPtr, INT Base)
{
	if( ( ( Base < 1 ) && ( Base != 0 ) ) || ( Base > 36 ) )
		return 0;	// eliminate unsupported/nonsensical values

	// Pass leading white spaces
	const TCHAR* Str = Ptr;
	while( *Str && iswspace( *Str ) )
		++Str;

	if( !*Str )
	{
		// no number to convert, exit
		if( EndPtr )
			*EndPtr = (TCHAR*)Ptr;
		return 0;
	}

	// Detect minus sign, or pass plus sign
	bool Minus = false;
	{
		TCHAR Sign = *Str;
		switch( Sign )
		{
		case '-':	Minus = true;	// pass-through
		case '+':	++Str;			// pass-through
		default:	;
		}
	}

	// Detect/pass/determine base
	bool Passed0x = false;
	if( !Base )
	{
		if( *Str == '0' )
		{
			if( ( *Str == 'X' ) || ( *Str == 'x' ) )
			{
				Str += 2;
				Base = 16;
				Passed0x = true;
			}
			else
				Base = 8;
		}
		else
			Base = 10;
	}
	else if( ( Base == 16 ) && ( *Str == '0' ) && ( ( *(Str+1) == 'X' ) || ( *(Str+1) == 'x' ) ) )
	{
		Str += 2;	// pass '0X' or '0x'
		Passed0x = true;
	}

	// Decipher the value
	unsigned long long Value = 0;
	bool ValueOverflown = false;
	bool NoCharacterAccepted = true;
	do
	{
		if( Value >= (unsigned long long)ULONG_MAX )
		{
			ValueOverflown = true;	// we'll return ULONG_MAX, no matter what, but we need to decode to end of decodable text for EndPtr.
			Value = 0;
		}

		// Get next character
		TCHAR Character = *Str;
		unsigned long CharValue;

		if( Character == 0 )
			break;
		if( ( Character >= '0' ) && ( Character <= '9' ) )
			CharValue = Character - '0';		// a digit
		else if( iswupper( Character ) )
			CharValue = Character - 'A' + 10;	// capital letter
		else if( iswlower( Character ) )
			CharValue = Character - 'a' + 10;	// small letter
		else
			break;	// something else, end here

		if( CharValue >= Base )
			break;	// unacceptable in current base, end here

		Value *= Base;
		Value += CharValue;
		++Str;
		NoCharacterAccepted = false;
	}
	while( true );

	if( NoCharacterAccepted && Passed0x )
	{
		// Special case, intricacy of Windows wcstoul() - we have to return 0, and EndPtr points to x.
		if( EndPtr )
		{
			check( *(Str-1) == 'x' || *(Str-1) == 'X' );
			*EndPtr = (TCHAR*)Str-1;
		}
		return 0;
	}

	if( EndPtr )
		*EndPtr = (TCHAR*)Str;

	// Temp tests of _stsscanf()
//	{
//		TCHAR Buffer[] = { 'S', 't', 'r', ':', ' ', ' ', 'x', '1', 0 };
//		TCHAR Format[] = { 'S', 't', 'r', ':', ' ', '%', 'c', '%', 'd', 0 };
//		char Output[64]; 
//		unsigned long Output2;
//		char Output;
//		int result = _stscanf( Buffer, Format, &Output, &Output2 );
//		Buffer[0] = 0;
//	}

	if( ValueOverflown )
		return ULONG_MAX;
	else if( Minus )
		return (unsigned long)-Value;
	else
		return (unsigned long)Value;
}

unsigned long long _tcstoui64(const TCHAR *Ptr, TCHAR **EndPtr, INT Base)
{
	if( ( ( Base < 1 ) && ( Base != 0 ) ) || ( Base > 36 ) )
		return 0;	// eliminate unsupported/nonsensical values

	// Pass leading white spaces
	const TCHAR* Str = Ptr;
	while( *Str && iswspace( *Str ) )
		++Str;

	if( !*Str )
	{
		// no number to convert, exit
		if( EndPtr )
			*EndPtr = (TCHAR*)Ptr;
		return 0;
	}

	// Detect minus sign, or pass plus sign
	bool Minus = false;
	{
		TCHAR Sign = *Str;
		switch( Sign )
		{
		case '-':	Minus = true;	// pass-through
		case '+':	++Str;			// pass-through
		default:	;
		}
	}

	// Detect/pass/determine base
	if( !Base )
	{
		if( *Str == '0' )
		{
			if( ( *Str == 'X' ) || ( *Str == 'x' ) )
			{
				Str += 2;
				Base = 16;
			}
			else
				Base = 8;
		}
		else
			Base = 10;
	}
	else if( ( Base == 16 ) && ( *Str == '0' ) && ( ( *(Str+1) == 'X' ) || ( *(Str+1) == 'x' ) ) )
		Str += 2;	// pass '0X' or '0x'

	// Decipher the value
	unsigned long long Value = 0;
	bool ValueOverflown = false;
	unsigned long long MaxValueDividedByBase = ULLONG_MAX/Base;
	do
	{
		// Get next character
		TCHAR Character = *Str;
		unsigned long CharValue;

		if( Character == 0 )
			break;
		if( ( Character >= '0' ) && ( Character <= '9' ) )
			CharValue = Character - '0';		// a digit
		else if( iswupper( Character ) )
			CharValue = Character - 'A' + 10;	// capital letter
		else if( iswlower( Character ) )
			CharValue = Character - 'a' + 10;	// small letter
		else
			break;	// something else, end here

		if( CharValue >= Base )
			break;	// unacceptable in current base, end here

		if( Value > MaxValueDividedByBase )
		{
			ValueOverflown = true;	// we'll return ULLONG_MAX, no matter what, but we need to decode to end of decodable text for EndPtr.
			Value = 0;
		}

		Value *= Base;

		if( Value > ULLONG_MAX - CharValue )
		{
			ValueOverflown = true;	// we'll return ULLONG_MAX, no matter what, but we need to decode to end of decodable text for EndPtr.
			Value = 0;
		}

		Value += CharValue;
		++Str;
	}
	while( true );

	if( EndPtr )
		*EndPtr = (TCHAR*)Str;

	if( ValueOverflown )
		return ULLONG_MAX;
	else if( Minus )
		return (unsigned long)-Value;
	else
		return (unsigned long)Value;
}

int _tcsnicmp(const TCHAR *String1, const TCHAR *String2, size_t Count)
{
	// walk the strings, comparing them case insensitively, up to a max size
	for (; (*String1 || *String2) && Count; String1++, String2++, Count--)
	{
		TCHAR A = towupper(*String1), B = towupper(*String2);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

long _tstoi(const TCHAR *String)
{
	if( String == NULL )
	{
		// NULL argument should call handler set by _set_invalid_parameter_handler. It does this:
		printf( "SECURE CRT: Invalid parameter detected." );
		strtol( "o", NULL, 10 );	// this dummy call will set errno to EINVAL, as required
		return 0;
	}

	return strtol( TCHAR_TO_ANSI_DIGITS_ONLY( String ), NULL, 10 );	// using this one, because it correctly sets errno to ERANGE

/*
	// Pass leading white spaces
	while( iswspace( *String ) )
		++String;

	// Detect minus sign, or pass plus sign
	bool Minus = false;
	{
		TCHAR Sign = *Str;
		switch( Sign )
		{
		case '-':	Minus = true;	// pass-through
		case '+':	++Str;			// pass-through
		default:	;
		}
	}

	// Decipher value
	unsigned long Value = 0;
	while( ( *String >= '0' ) && ( *String <= '9' ) )
	{
		if( Value > LONG_MAX/10 )
			return Minus ? LONG_MIN : LONG_MAX;	// should set errno to ERANGE

		Value *= 10;

		unsigned long digit = ( *String - '0' );

		if( Value > LONG_MAX - digit )
			return Minus ? LONG_MIN : LONG_MAX;	// should set errno to ERANGE

		Value += digit;
		++String;
	}

	return Minus ? (long)-Value : (long)Value;
*/
}

long long _tstoi64(const TCHAR *String)
{

	if( String == NULL )
	{
		// NULL argument should call handler set by _set_invalid_parameter_handler. It does this:
		printf( "SECURE CRT: Invalid parameter detected." );
		strtol( "o", NULL, 10 );	// this dummy call will set errno to EINVAL, as required
		return 0;
	}

	return strtoll( TCHAR_TO_ANSI_DIGITS_ONLY( String ), NULL, 10 );	// using this one, because it correctly sets errno to ERANGE
/*
	// Pass leading white spaces
	while( iswspace( *String ) )
		++String;

	// Detect minus sign, or pass plus sign
	bool Minus = false;
	{
		TCHAR Sign = *Str;
		switch( Sign )
		{
		case '-':	Minus = true;	// pass-through
		case '+':	++Str;			// pass-through
		default:	;
		}
	}

	// Decipher value
	unsigned long long Value = 0;
	while( ( *String >= '0' ) && ( *String <= '9' ) )
	{
		if( Value > LLONG_MAX/10 )
			return Minus ? LLONG_MIN : LLONG_MAX;	// should set errno to ERANGE

		Value *= 10;

		unsigned long long digit = ( *String - '0' );

		if( Value > LLONG_MAX - digit )
			return Minus ? LLONG_MIN : LLONG_MAX;	// should set errno to ERANGE

		Value += digit;
		++String;
	}

	return Minus ? (long long)-Value : (long long)Value;
*/
}

float _tstof(const TCHAR *String)
{
	return strtof( TCHAR_TO_ANSI_DIGITS_ONLY( String ), NULL );
}

double _tcstod( const TCHAR *Ptr, TCHAR **EndPtr )
{
	char SmallAnsiBuffer[256];
	char* AnsiString;

	// Convert TCHAR string to important Ansi characters and trash
	int Len = _tcslen( Ptr );
	AnsiString = ( Len < 256 ) ? SmallAnsiBuffer : (char*)malloc( Len+1 );

	const TCHAR* TempPtr = Ptr;
	char* AnsiPtr = AnsiString;
	while( *TempPtr )
	{
		TCHAR C = *TempPtr++;
		if( iswspace( C ) )
			*AnsiPtr++ = ' ';
		else if( iswascii( C ) )
		{
			check( ( C & 0xFF ) == C );
			*AnsiPtr++ = C;
		}
		else
		{
			check( !iswxdigit( C ) );
			*AnsiPtr++ = 'y';	// neutral char for number reading
		}
	}
	*AnsiPtr = 0;

	// Read the double and EndPtr
	double Result;

	if( EndPtr )
	{
		char* AnsiEndPtr = 0;
		Result = strtod( AnsiString, &AnsiEndPtr );
		if( AnsiEndPtr )
		{
			int EndCharIndex = AnsiEndPtr - AnsiString;
			*EndPtr = (TCHAR*)Ptr + EndCharIndex;
		}
	}
	else
	{
		Result = strtod( AnsiString, NULL );
	}

	// Clean up, if necessary
	if( AnsiString != SmallAnsiBuffer )
	{
		free( AnsiString );
	}

	return Result;
}

TCHAR *_tcsncpy(TCHAR *StrDestination, const TCHAR *StrSource, size_t Count)
{
	TCHAR *BufPtr = StrDestination;

	while (*StrSource && Count--)
	{
		*BufPtr++ = *StrSource++;
	}

	*BufPtr = 0;

	return StrDestination;
}

static int WideCharToDigit( const TCHAR C, int Base )
{
	if( ( C >= '0' ) && ( C <= '9' ) && ( C < '0' + Base ) )
		return C - '0';
	if( Base <= 10 )
		return -1;
	if( ( C >= 'a' ) && ( C <= 'a' ) && ( C < ( 'a' + Base - 10 ) ) )
		return C - 'a' + 10;
	if( ( C >= 'A' ) && ( C <= 'Z' ) && ( C < ( 'A' + Base - 10 ) ) )
		return C - 'A' + 10;
	return -1;
}

static int InternalSwscanf( const TCHAR *String, const TCHAR *Format, va_list Values )
{
	if( !*Format )
		return 0;

	const TCHAR* StringStart = String;
	int NextChar = *String++;
	int VariablesRead = 0;

	if( !NextChar )
		return -1;

	while( *Format )
	{
		if( iswspace( *Format ) )
		{
			while( NextChar && iswspace( NextChar ) )
				NextChar = *String++;
		}
		else if( *Format == '%' )
		{
			bool Suppress = false;
			bool SomethingRead = false;

			int Width = 0;
			int Base;

			bool ContainsSmallH	= false;
			bool ContainsSmallL	= false;
			bool ContainsBigL	= false;
			bool ContainsSmallW	= false;
			bool ContainsI64	= false;

			// At most one of those will be true
			bool IsNumber = false;
			bool IsFloatingNumber = false;
			bool IsString = false;
			bool IsWideString = false;
			bool IsChar = false;
			bool IsWideChar = false;

			// Decode suppress
			if( *Format == '*' )
			{
				++Format;
				Suppress = true;
			}

			// Decode width
			while( iswdigit( *Format ) )
			{
				Width = ( Width * 10 ) + ( *Format - '0' );
				++Format;
			}

			if( !Width )
				Width = -1;	// no width specified

			// Decode prefix
			for( bool PrefixDone = false; !PrefixDone; ++Format )
			{
				switch( *Format )
				{
				case 'h':	ContainsSmallH = true;	break;
				case 'l':	ContainsSmallL = true;	break;
				case 'L':	ContainsBigL = true;	break;
				case 'w':	ContainsSmallW = true;	break;
				case 'I':
					if( ( *(Format+1) == '6' ) && ( *(Format+2) == '4' ) )
					{
						ContainsI64 = true;
						Format += 2;
					}
					break;
				default:
					PrefixDone = true;
					break;
				}
			}

			// Decode type
			switch( *Format )
			{
			case 'p':
			case 'P':	if( sizeof(void*) == sizeof(long long) ) ContainsI64 = true;	// pass-through
			case 'x':
			case 'X':	Base = 16; IsNumber = true; break;
			case 'u':
			case 'd':	Base = 10; IsNumber = true; break;
			case 'i':	Base = 0; IsNumber = true; break;
			case 'o':	Base = 8; IsNumber = true; break;
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':	IsFloatingNumber = true; break;
			case 's':
			case 'S':
				if( ContainsSmallW || ContainsSmallL )
					IsWideString = true;
				else if( ContainsSmallH )
					IsString = true;
				else if( *Format == 's' )
					IsWideString = true;	// yes, because is swscanf, so we expect wide by default
				else
					IsString = true;
				break;
			case 'c':
			case 'C':
				if( ContainsSmallW || ContainsSmallL )
					IsWideChar = true;
				else if( ContainsSmallH )
					IsChar = true;
				else if( *Format == 'c' )
					IsWideChar = true;	// yes, because is swscanf, so we expect wide by default
				else
					IsChar = true;
				break;
			case 'n':
				if( !Suppress )
					*va_arg( Values, int* ) = ( String - StringStart - 1 );
				Suppress = true;
				SomethingRead = true;
				break;
			case '[':
				check( 0 );	// Add this if we ever need it...
				break;
			default:
				// Pass leading whitespaces
				while( NextChar && iswspace( NextChar ) )
					NextChar = *String++;
				if( NextChar == *Format )
				{
					Suppress = true;
					SomethingRead = true;
					NextChar = *String++;
				}
				break;
			}

			// Now transfer values to variables, if there's something more to transfer
			if( IsNumber )
			{
				bool Minus = false;
				bool NumberStarted = false;

				// Pass whitespaces
				while( NextChar && iswspace( NextChar ) )
					NextChar = *String++;

				// Decode sign
				if( ( NextChar == '-' ) || ( NextChar == '+' ) )
				{
					Minus = ( NextChar == '-' );
					NextChar = *String++;
					if( Width > 0 )
						--Width;
				}

				// Decode number base
				if( Width && ( NextChar == '0' ) && ( *Format != 'p' ) && ( *Format != 'P' ) )
				{
					NextChar = *String++;
					if( Width > 0 )
						--Width;
					NumberStarted = true;
					if( Width && ( ( NextChar == 'x' ) || ( NextChar == 'X' ) ) )
					{
						if( !Base )
							Base = 16;
						if( Base == 16 )
						{
							NextChar = *String++;
							if( Width > 0 )
								--Width;
							NumberStarted = false;	// just passed 0x
						}
					}
					else if( !Base )
						Base = 8;
				}

				if( !Base )	// it could be zero only if it was %i, and we already detected other bases and not found them
					Base = 10;

				// Decode the value
				unsigned long long Value = 0ULL;
				while( Width && NextChar )
				{
					int Digit = WideCharToDigit( NextChar, Base );
					if( Digit == -1 )
						break;
					Value = Value*Base + Digit;
					NextChar = *String++;
					if( Width > 0 )
						--Width;
					NumberStarted = true;
				}

				if( NumberStarted )
				{
					SomethingRead = true;
					if( !Suppress )
					{
						if( ContainsI64 )
							*va_arg( Values, long long* ) = Minus ? -Value : Value;
						else if( ContainsSmallL )
							*va_arg( Values, long* ) = Minus ? -Value : Value;
						else if( ContainsSmallH )
							*va_arg( Values, short int* ) = Minus ? -Value : Value;
						else
							*va_arg( Values, int* ) = Minus ? -Value : Value;
					}
				}
			}
			else if( IsFloatingNumber )
			{
				bool Minus = false;
				bool BadNumber = false;
				long double Value = 0;

				// Pass whitespaces
				while( NextChar && iswspace( NextChar ) )
					NextChar = *String++;

				// Decode sign
				if( ( NextChar == '-' ) || ( NextChar == '+' ) )
				{
					Minus = ( NextChar == '-' );
					NextChar = *String++;
					if( Width > 0 )
						--Width;
				}

				// Decode part before dot
				if( NextChar != '.' )
				{
					bool NoDigitsFound = true;
					while( Width && NextChar && iswdigit( NextChar ) )
					{
						Value = Value * 10 + ( NextChar - '0' );
						NextChar = *String++;
						if( Width > 0 )
							--Width;
						NoDigitsFound = false;
					}
					if( NoDigitsFound )
						BadNumber = true;
				}

				if( !BadNumber )
				{
					if( Width && ( NextChar == '.' ) )
					{
						// Decode digits in decimal places
						long double DecimalPlace = 1;
						NextChar = *String++;
						if( Width > 0 )
							--Width;
						while( Width && NextChar && iswdigit( NextChar ) )
						{
							DecimalPlace /= 10;
							Value += DecimalPlace * ( NextChar - '0' );
							NextChar = *String++;
							if( Width > 0 )
								--Width;
						}
					}

					// Decode exponent
					if( Width && ( ( NextChar == 'e' ) || ( NextChar == 'E' ) ) )
					{
						bool ExponentMinus = false;
						int Exponent = 0;

						NextChar = *String++;
						if( Width > 0 )
							--Width;

						// Decode exponent sign
						if( Width && ( ( NextChar == '-' ) || ( NextChar == '+' ) ) )
						{
							ExponentMinus = ( NextChar == '-' );
							NextChar = *String++;
							if( Width > 0 )
								--Width;
						}

						// Decode exponent digits
						while( Width && NextChar && iswdigit( NextChar ) )
						{
							Exponent = Exponent * 10 + ( NextChar - '0' );
							NextChar = *String++;
							if( Width > 0 )
								--Width;
						}

						// Apply exponent to value
						{
							// It's a tricky piece that decomposes exponent into powers of two to minimize the number of multiplications before arriving at final figure.
							long double ExponentMultiplier = ExponentMinus ? 0.1 : 10;
							while( Exponent )
							{
								if( Exponent & 1 )	// this power-of-two is non-zero
									Value *= ExponentMultiplier;
								Exponent >>= 1;		// shift to next power-of-two
								ExponentMultiplier = ExponentMultiplier * ExponentMultiplier;
							}
						}
					}

					SomethingRead = true;
					
					if( !Suppress )
					{
						if( ContainsBigL )
							*va_arg( Values, long double* ) = Minus ? -Value : Value;
						else if( ContainsSmallL )
							*va_arg( Values, double* ) = Minus ? -Value : Value;
						else
							*va_arg( Values, float* ) = Minus ? -Value : Value;
					}
				}
			}
			else if( IsString )
			{
				char* ValuePtr = Suppress ? NULL : va_arg( Values, char* );

				// Skip leading whitespaces
				while( NextChar && iswspace( NextChar ) )
					NextChar = *String++;

				// Read until whitespace
				while( Width && NextChar && !iswspace( NextChar ) )
				{
					if( !Suppress )
						*ValuePtr++ = NextChar & 0xFF;	// assumming that wide char just converts to the same char; else it would be hard to keep it in Ascii string (multi-byte? have to check)
					SomethingRead = true;
					NextChar = *String++;
					if( Width > 0 )
						--Width;
				}
				
				// End with 0, even when nothing read
				if( !Suppress )
					*ValuePtr = 0;
			}
			else if( IsWideString )
			{
				TCHAR* ValuePtr = Suppress ? NULL : va_arg( Values, TCHAR* );

				// Skip leading whitespaces
				while( NextChar && iswspace( NextChar ) )
					NextChar = *String++;

				// Read until whitespace
				while( Width && NextChar && !iswspace( NextChar ) )
				{
					if( !Suppress )
						*ValuePtr++ = NextChar;	// wide to wide, no conversion needed
					SomethingRead = true;
					NextChar = *String++;
					if( Width > 0 )
						--Width;
				}

				// End with 0, even when nothing read
				if( !Suppress )
					*ValuePtr = 0;
			}
			else if( IsChar )
			{
				char* ValuePtr = Suppress ? NULL : va_arg( Values, char* );

				if( Width == -1 )
					Width = 1;

				// Read until whitespace
				while( Width && NextChar )
				{
					if( !Suppress )
						*ValuePtr++ = NextChar & 0xFF;	// assumming that wide char just converts to the same char; else it would be hard to keep it in Ascii string (multi-byte? have to check)
					SomethingRead = true;
					NextChar = *String++;
					--Width;
				}
			}
			else if( IsWideChar )
			{
				TCHAR* ValuePtr = Suppress ? NULL : va_arg( Values, TCHAR* );

				if( Width == -1 )
					Width = 1;

				// Read until whitespace
				while( Width && NextChar )
				{
					if( !Suppress )
						*ValuePtr++ = NextChar;	// wide to wide, no conversion needed
					SomethingRead = true;
					NextChar = *String++;
					--Width;
				}
			}

			if( SomethingRead )
			{
				if( !Suppress )
					++VariablesRead;
			}
			else
				break;	// fail
		}
		else	// *Format is not space and not % - it must match the pattern
		{
			if( NextChar == *Format )
				NextChar = *String++;
			else
				break;	// fail
		}

		++Format;
	}

	return VariablesRead;
}

int _stscanf( const TCHAR *String, const TCHAR *Format, ... )
{
	va_list Values;
	int Result;
	va_start( Values, Format );
	Result = InternalSwscanf( String, Format, Values );
	va_end( Values );
	return Result;
}

TCHAR towupper(TCHAR Char)
{
	return __toupper(Char);
}

int iswspace(TCHAR Char)
{
	return __istype(Char, _CTYPE_S);
}

int iswpunct(TCHAR Char)
{
	return __istype(Char, _CTYPE_P);
}

#endif // !TCHAR_IS_4_BYTES
