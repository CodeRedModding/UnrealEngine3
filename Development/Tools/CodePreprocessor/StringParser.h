// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HSTRINGPARSERH
#define HSTRINGPARSERH

/*
 * Simple string stream parser 
 */

class CStringParser
{
public:
	/* Skip white spaces in stream */
	static bool EatWhiteSpaces( stringstream &str );

	/* Eat stream until given char is reached */
	static bool EatStream( stringstream &str, char c );

	/* Get single token */
	static bool ParseToken( stringstream &str, string &token );

	/* Parse single char */
	static bool ParseChar( stringstream &str, char c );

	/* Parse rest of the line */
	static bool ParseRest( stringstream &str, string &s );

	/* Try to match given token */
	static bool TestToken( const char *str, const char *match, int n, bool fullMatch );

	/* Try to match identifier */
	static bool TestIdent( const char *str, int n );

	/* Try to match integer value */
	static bool TestInteger( const char *str, int n );
};

#endif