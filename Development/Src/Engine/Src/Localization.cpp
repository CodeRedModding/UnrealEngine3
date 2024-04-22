/*=============================================================================
	Localization.cpp: Declarations for classes which deal with localized text; word-wrapping, sorting, etc.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "Localization.h"

/**
 * Returns the position of the first boundary character [non-whitespace, etc.] in the specified text.
 *
 * @param	pText				a pointer to the string to operate on
 *
 * @return	the position of the first character in the text being scanned, or INDEX_NONE if there isn't any boundary
 *			characters in the text
 */
INT FLocalizedWordWrapHelper::GetStartingPosition( const TCHAR* pText )
{
	INT Result = INDEX_NONE;

	if ( pText != NULL && *pText )
	{
		TCHAR Prev=0;
		for ( INT CurrentPosition = 0; pText[CurrentPosition]; CurrentPosition++ )
		{
			//@todo ronp - should we be using appCanBreakLineAt instead?
			if ( !appIsWhitespace(pText[CurrentPosition]) )
			{
				Result = CurrentPosition;
				break;
			}
		}
	}

	return Result;
}

/**
 * Returns the position of the last boundary character in the text being scanned; e.g. the beginning of the last word in the text.
 *
 * @param	pText				a pointer to the string to operate on
 *	
 * @return	the position of the first character of the last word in the text being scanned, or INDEX_NONE if there isn't any boundary
 *			characters in the text
 */
INT FLocalizedWordWrapHelper::GetLastBreakPosition( const TCHAR* pText )
{
	INT Result = INDEX_NONE;

	if ( pText != NULL )
	{
		const INT StringLen = appStrlen(pText);
		if ( StringLen > 0 )
		{
			INT CurrentPosition = StringLen - 1;
			TCHAR Prev=CurrentPosition > 0 ? pText[CurrentPosition-1] : 0;

			// first, skip over all trailing whitespace or linebreak characters to find end of the last word in the text
			while ( CurrentPosition >= 0 && appCanBreakLineAt(Prev, pText[CurrentPosition]) )
			{
				CurrentPosition--;
				Prev = CurrentPosition > 0 ? pText[CurrentPosition-1] : 0;
			}

			if ( CurrentPosition >= 0 )
			{
				// +1 because the last boundary position should be the first breakable character following the last complete word in the text,
				// or if the last character of the text is part of the last word, the last boundary position is the beyond the end of the text
				Result = CurrentPosition + 1;
			}
		}
	}

	return Result;
}

/**
 * Returns the position of the next boundary character in the text being scanned.
 *
 * @param	pText				a pointer to the string to operate on
 * @param	CurrentPosition		the position to use as the starting point when searching for the next boundary character.
 *
 * @return	the position of the next boundary character in the text being scanned, or INDEX_NONE if there aren't any more boundary
 *			characters in the text
 */
INT FLocalizedWordWrapHelper::GetNextBreakPosition( const TCHAR* pText, INT CurrentPosition )
{
	INT Result = INDEX_NONE;

	if ( pText && CurrentPosition >= 0 && pText[CurrentPosition] )
	{
		TCHAR Prev = pText[CurrentPosition++];
		for ( ; pText[CurrentPosition]; CurrentPosition++ )
		{
			TCHAR Char = pText[CurrentPosition];
			if ( appCanBreakLineAt(Prev, Char) )
			{
				break;
			}

			Prev = Char;
		}

		// include all connecting punctuation so neither single marks like ,.?! nor things like !!, ..., and !? don't wrap
		while ( pText[CurrentPosition] && appIsPunct(pText[CurrentPosition]) )
		{
			CurrentPosition++;
		}

		// all whitespace must be trailing, so advance past all whitespace encountered after the breakable character
		while ( pText[CurrentPosition] && appIsWhitespace(pText[CurrentPosition]) )
		{
			CurrentPosition++;
		}

		// so now CurrentPosition is either pointing to the first character of the next word, or [if no more boundary characters were found] is equal
		// to the length of the string
		Result = CurrentPosition;
	}

	return Result;
}


/**
 * Returns the position of the previous boundary character in the specified text; e.g. the beginning of the previous word.
 *
 * @param	pText				a pointer to the string to operate on
 * @param	CurrentPosition		the position to use as the starting point when searching for the previous boundary character.
 *
 * @return	the position of the previous boundary character in the text being scanned, or INDEX_NONE if there aren't any more boundary
 *			characters in the text
 */
INT FLocalizedWordWrapHelper::GetPreviousBreakPosition( const TCHAR* pText, INT CurrentPosition )
{
	INT Result = INDEX_NONE;

	if ( pText && CurrentPosition > 0 && pText[CurrentPosition] )
	{

		// if the current character is a whitespace character, we'll need to backup until we find the end of the previous word
		while ( CurrentPosition > 0 && appCanBreakLineAt(pText[CurrentPosition - 1], pText[CurrentPosition]) )
		{
			CurrentPosition--;
		}

		// at this point, CurrentPosition is pointing to a non-breakable character inside a word or the first character of the text
		// if we're pointing to a character inside a word, find the first character of the word
		for ( ; CurrentPosition > 0; CurrentPosition-- )
		{
			const INT PrevPosition = CurrentPosition - 1;
			TCHAR Prev = PrevPosition > 0 ? pText[PrevPosition-1] : 0;
			TCHAR Char = pText[PrevPosition];

			if ( appCanBreakLineAt(Prev, Char) )
			{
				Result = CurrentPosition;
				break;
			}
		}

		if ( CurrentPosition == 0 )
		{
			Result = CurrentPosition;
		}
	}

	return Result;
}

/**
 * Determines whether the specified character is the line-break character.
 *
 * @param	pText			a pointer to the string to operate on
 * @param	CurrentPosition	the position of the character to test (into pText)
 * @param	ManualEOL		if specified, return TRUE if Char matches this character.
 *
 * @return	TRUE if Char matches ManualEOL or is a valid line-break character according to the line-break rules of the current language.
 */
UBOOL FLocalizedWordWrapHelper::IsLineBreak( const TCHAR* pText, INT CurrentPosition, const TCHAR* ManualEOL/*=NULL*/ )
{
	UBOOL bResult = FALSE;

	if ( CurrentPosition >= 0 )
	{
		bResult = (ManualEOL != NULL && pText[CurrentPosition] == *ManualEOL) || appIsLinebreak(pText[CurrentPosition]);
	}

	return bResult;
}

/**
 * Determines whether the specified character is a valid line-break character.
 *
 * @param	pText			a pointer to the string to operate on
 * @param	CurrentPosition	the position of the character to test (into pText)
 * @param	ManualEOL		if specified, return TRUE if Char matches this character.
 *
 * @return	TRUE if Char matches ManualEOL or is a valid line-break character according to the line-break rules of the current language.
 */
UBOOL FLocalizedWordWrapHelper::CanBreakLineAtChar( const TCHAR* pText, INT CurrentPosition, const TCHAR* ManualEOL/*=NULL*/ )
{
	UBOOL bResult = FALSE;

	if ( CurrentPosition >= 0 )
	{
		TCHAR Prev = CurrentPosition > 0 ? pText[CurrentPosition - 1] : 0;
		TCHAR Char = pText[CurrentPosition];

		bResult = (ManualEOL != NULL && Char == *ManualEOL) || appCanBreakLineAt(Prev, Char);
	}

	return bResult;
}

// EOL







