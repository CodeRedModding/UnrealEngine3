// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HRULETABLEH
#define HRULETABLEH

/*
 * Rule 
 */

class CRule
{
private:
	string			Name;			// Rule name
	CLogicState		State;			// Rule state
	string			Value;			// Defined value

public:
	/* Constructor for value rules */
	CRule( const string &name, CLogicState state, const string &value="" )
		: Name( name )
		, Value( value )
		, State( state )
	{
	}

	/* Copy constructor */
	CRule( const CRule &other )
		: Name( other.Name )
		, Value( other.Value )
		, State( other.State )
	{
	}

	/* Get rule name */
	const string &GetName() const
	{
		return Name;
	}

	/* Get rule value */
	const string &GetValue() const
	{
		return Value;
	}
	
	/* Is rule macro defined ? */
	const CLogicState &GetState() const
	{
		return State;
	}

	/* Redefine rule */
	void Redefine( const string &name, CLogicState state, const string &value="" )
	{
		Name = name;
		Value = value;
		State = state;
	}
};


/* 
 * Rule table
 */

class CRuleTable
{
private:
	vector< CRule* >	Rules;		// Filtering rules

public:
	/* Constructor */
	CRuleTable();

	/* Copy constructor */
	CRuleTable( const CRuleTable &other );

	/* Destructor */
	~CRuleTable();

	/* Load rules from file */
	bool LoadFromFile( const string &fileName );

	/* Clear rules */
	void Clear();

	/* Find existing rule */
	CRule *FindRule( const string &name ) const;

	/* Add value based rule */
	void AddRule( const string &name, CLogicState state, const string &value );

	/* Get rule logic state, returns LOGIC_True for defined, LOGIC_False for undefined rules and LOGIC_DontCare if rule was not found */
	CLogicState GetRuleState( const string &name ) const;

	/* Get expression value for rule */
	CExpressionValue GetRuleValue( const string &name ) const;
};

#endif