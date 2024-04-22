// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CTokenStream::CTokenStream()
{

}

CTokenStream::~CTokenStream()
{
	for ( size_t i=0; i<Tokens.size(); i++ )
	{
		delete Tokens[i];
	}
}

void CTokenStream::AddToken( CToken* token )
{
	Tokens.push_back( token );
}

int CTokenStream::Size() const
{
	return (int)Tokens.size();
}

CToken *CTokenStream::GetToken( int index ) const
{
	if ( index >= 0 && index < (int)Tokens.size() )
	{
		return Tokens[ index ];
	}
	else
	{
		return NULL;
	}
}

bool CTokenStream::GetToken( CToken &token, int index ) const
{
	if ( index >= 0 && index < (int)Tokens.size() )
	{
		token = *Tokens[ index ];
		return true;
	}
	else
	{
		return false;
	}
}

CTokenStreamReader::CTokenStreamReader( const CTokenStream &stream )
	: Stream( stream )
	, Pos( 0 )
{
}

void CTokenStreamReader::Rewind()
{
	Pos = 0;
}

bool CTokenStreamReader::IsEOF() const
{
	return Pos >= Stream.Size();
}

CToken *CTokenStreamReader::Peek( int offset/*=0*/ )
{
	return Stream.GetToken( Pos + offset );
}

bool CTokenStreamReader::Peek( CToken &token, int offset/*=0*/ )
{
	return Stream.GetToken( token, Pos + offset );
}

CToken *CTokenStreamReader::Get()
{
	return Stream.GetToken( Pos++ );
}

CTokenizer::CTokenizer()
{
}

ETokenType CTokenizer::MatchToken( const char *str, int n, bool fullMatch )
{
	if ( TestToken( str, "defined", n, fullMatch )) return EToken_Defined;
	if ( TestToken( str, "&&", n, fullMatch )) return EToken_And;
	if ( TestToken( str, "||", n, fullMatch )) return EToken_Or;
	if ( TestToken( str, "(", n, fullMatch )) return EToken_LPar;
	if ( TestToken( str, ")", n, fullMatch )) return EToken_RPar;
	if ( TestToken( str, "!", n, fullMatch )) return EToken_Not;
	if ( TestIdent( str, n )) return EToken_Ident;
	if ( TestInteger( str, n )) return EToken_Value;
	return EToken_Invalid;
}

bool CTokenizer::Tokenize( const string &text, CTokenStream &tokens )
{
	const char *str = text.c_str();
	const char *start = str;
	int total = (int) strlen( str );
	int len = 1;

	// Tokenize expression
	while ( *start )
	{
		// White space, skip it
		if ( len == 1 && *start <= ' ' )
		{
			start++;
			continue;
		}

		// Try to match valid token
		ETokenType matchedToken = MatchToken( start, len, false );

		// No valid token matched
		if ( matchedToken == EToken_Invalid )
		{
			// Perform full match
			matchedToken = MatchToken( start, len-1, true );

			// Error in stream
			if ( matchedToken == EToken_Invalid || len==1 )
			{
				printf( "Error in tokenizer: '%s' %i\n", start, len );
				return false;
			}

			// Valid token was parsed from the stream, insert it into token stream
			string tokenString( start, len-1 );
			tokens.AddToken( new CToken( matchedToken, tokenString, 0 ));

			// Continue parsing after this token
			start += len-1;
			len = 1;
		}
		else
		{
			// Something valid was parsed, keep parsing
			len++;
		}	
	}

	// Tokenized without errors
	return true;
}
