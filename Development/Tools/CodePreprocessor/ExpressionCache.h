// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HEXPRESSIONCACHEH
#define HEXPRESSIONCACHEH

/*
 * Cache of evaluated preprocessor directives 
 */

class CExpressionCache
{
private:
	map< string, CLogicState >	Cache;
	const CRuleTable&			Rules;

public:
	/* Constructor */
	CExpressionCache( const CRuleTable &rules );

	/* Get rules */
	const CRuleTable &GetRules() const;

	/* Reset expression cache */
	void ResetCache();

	/* Evaluate expression */
	CLogicState Evaluate( const string &expr );

protected:
	/* Get value from cache */
	bool GetCachedValue( const string &expr, CLogicState &value ) const;

	/* Cache expression value */
	void CacheValue( const string &expr, CLogicState value );
};

#endif