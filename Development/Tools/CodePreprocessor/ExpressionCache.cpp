// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CExpressionCache::CExpressionCache( const CRuleTable &rules )
	: Rules( rules )
{
}

const CRuleTable &CExpressionCache::GetRules() const
{
	return Rules;
}

void CExpressionCache::ResetCache()
{
	Cache.clear();
}

CLogicState CExpressionCache::Evaluate( const string &expr )
{
	// Try to use cached value
	CLogicState value = LOGIC_DontCare;
	if ( GetCachedValue( expr, value ))
	{
		return value;
	}

	printf( "Evaluating '%s'\n", expr.c_str() );

	// Tokenize
	CTokenStream tokens;
	CTokenizer tokenizer;
	if ( !tokenizer.Tokenize( expr, tokens ))
	{
		// We were unable to parse rule, assume don't care value
		return LOGIC_DontCare;
	}

	// Parse expression
	CSimpleParser parser;
	CSyntaxNode *rootNode = parser.BuildSyntaxTree( tokens );

	// Evaluate syntax tree
	if ( rootNode )
	{
		// Show it
		rootNode->Print( 1 );

		// Evaluate expression
		value = rootNode->Evaluate( Rules ).ToLogic();

		// Delete syntax tree
		delete rootNode;
	}

	// Cache value
	CacheValue( expr, value );
	return value;
}

bool CExpressionCache::GetCachedValue( const string &expr, CLogicState &value ) const
{
	map< string, CLogicState >::const_iterator i = Cache.find( expr );

	if ( i != Cache.end() )
	{
		// Found cached value
		value = i->second;
		return true;
	}
	else
	{
		// Value not found
		return false;
	}
}

void CExpressionCache::CacheValue( const string &expr, CLogicState value )
{
	Cache[ expr ] = value;
}