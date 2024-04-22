/*=============================================================================
	Localization.h: Declarations for classes which deal with localized text; word-wrapping, sorting, etc.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LOCALIZATION_H__
#define __LOCALIZATION_H__

/**
 * This class encapsulates and abstracts multi-lingual word break rules.  It is used by word wrapping code to determine where to break lines for wrapping to screen.
 */
class FLocalizedWordWrapHelper
{
public:
	/**
	 * Returns the position of the first boundary character [non-whitespace, etc.] in the specified text.
	 *
	 * @param	pText				a pointer to the string to operate on
	 *
	 * @return	the position of the first character in the text being scanned, or INDEX_NONE if there isn't any boundary
	 *			characters in the text
	 */
	static INT GetStartingPosition( const TCHAR* pText );

	/**
	 * Returns the position of the next boundary character in the text being scanned.
	 *
	 * @param	pText				a pointer to the string to operate on
	 * @param	CurrentPosition		the position to use as the starting point when searching for the next boundary character.
	 *
	 * @return	the position of the next boundary character in the text being scanned, or INDEX_NONE if there aren't any more boundary
	 *			characters in the text
	 */
	static INT GetNextBreakPosition( const TCHAR* pText, INT CurrentPosition );

	/**
	 * Returns the position of the previous boundary character in the specified text; e.g. the beginning of the previous word.
	 *
	 * @param	pText				a pointer to the string to operate on
	 * @param	CurrentPosition		the position to use as the starting point when searching for the previous boundary character.
	 *
	 * @return	the position of the previous boundary character in the specified text, or INDEX_NONE if there aren't any more boundary
	 *			characters in the text
	 */
 	static INT GetPreviousBreakPosition( const TCHAR* pText, INT CurrentPosition );

	/**
	 * Returns the position of the last boundary character in the specified text; e.g. the position of the character immediately following
	 * the last character of the last word in the text.
	 *
	 * @param	pText				a pointer to the string to operate on
	 *	
	 * @return	the position of the character immediately following the last character of the last word in the specified text.
	 *			INDEX_NONE if there isn't any boundary characters in the text
	 */
	static INT GetLastBreakPosition( const TCHAR* pText );

	/**
	 * Determines whether the specified character is the line-break character.
	 *
	 * @param	pText			a pointer to the string to operate on
	 * @param	CurrentPosition	the position of the character to test (into pText)
	 * @param	ManualEOL		if specified, return TRUE if Char matches this character.
	 *
	 * @return	TRUE if Char matches ManualEOL or is a valid line-break character according to the line-break rules of the current language.
	 */
	static UBOOL IsLineBreak( const TCHAR* pText, INT CurrentPosition, const TCHAR* ManualEOL=NULL );

	/**
	 * Determines whether the specified character is a valid line-break character.
	 *
	 * @param	pText			a pointer to the string to operate on
	 * @param	CurrentPosition	the position of the character to test (into pText)
	 * @param	ManualEOL		if specified, return TRUE if Char matches this character.
	 *
	 * @return	TRUE if Char matches ManualEOL or is a valid line-break character according to the line-break rules of the current language.
	 */
	static UBOOL CanBreakLineAtChar( const TCHAR* pText, INT CurrentPosition, const TCHAR* ManualEOL=NULL );
};

#endif
