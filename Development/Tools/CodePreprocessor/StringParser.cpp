// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

bool CStringParser::EatWhiteSpaces( stringstream &str )
{
	while ( !str.eof() && str.peek() <= ' ' )
	{
		str.get();
	}

	return !str.eof();
}

bool CStringParser::EatStream( stringstream &str, char c )
{
	while ( !str.eof() && str.peek() != c )
	{
		str.get();
	}

	return !str.eof();
}

bool CStringParser::ParseChar( stringstream &str, char c )
{
	if ( EatWhiteSpaces( str ) )
	{
		if ( !str.eof() && str.peek() == c )
		{
			str.get();
			return true;
		}
	}

	return false;
}

bool CStringParser::ParseToken( stringstream &str, string &token )
{
	locale loc( locale::empty(), locale::classic(), locale::numeric );

	if ( EatWhiteSpaces( str ) )
	{
		while ( !str.eof() && (isalnum( str.peek(), loc ) || str.peek() == '_' ) )
		{
			char c = str.get();
			char charStr[2] = { c, 0 };
			token += charStr;
		}
	}

	return token != "";
}

bool CStringParser::ParseRest( stringstream &str, string &s )
{
	if ( EatWhiteSpaces( str ) )
	{
		while ( !str.eof() )
		{
			char c = str.get();
			if ( c > 0 )
			{
				char charStr[2] = { c, 0 };
				s += charStr;
			}
		}
	}

	return s != "";
}



bool CStringParser::TestToken( const char *str, const char *match, int n, bool fullMatch )  
{
	int matchLen = (int) strlen( match );	
	return n<=matchLen && (_strnicmp( str, match, n )==0) && (!fullMatch || n==matchLen);
}

bool CStringParser::TestIdent( const char *str, int n )
{
	// Test char by char
	for ( int i=0; i<n; i++ )
	{
		char c = str[i];
		bool alpha = (c>='A' && c<='Z') || (c>='a' && c<='z') || c=='_';
		bool num = (c>='0' && c<='9');

		// Number is not allowed as a first char
		if ( num && i==0 )
		{
			return FALSE;
		}

		// Not alphanumeric char
		if ( !alpha && !num )
		{
			return FALSE;
		}
	}

	// Valid
	return TRUE;
}

bool CStringParser::TestInteger( const char *str, int n )
{
	// Test char by char
	for ( int i=0; i<n; i++ )
	{
		char c = str[i];
		bool num = (c>='0' && c<='9');

		// Not a number
		if ( !num )
		{
			return FALSE;
		}
	}

	// Number
	return TRUE;
}
