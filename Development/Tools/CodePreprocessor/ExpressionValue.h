// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HEXPRESSIONVALUEH
#define HEXPRESSIONVALUEH

/// Expression value
class CExpressionValue
{
public:
	CLogicState		State;		// Source rules state (we are interested in DontCare and Weak flags)
	int				Value;		// Integer value (valid only if State==LOGIC_True)

public:
	/* Default constructor */
	CExpressionValue()
		: State( LOGIC_DontCare )
		, Value( 0 )
	{
	}

	/* Constructor */
	CExpressionValue( const CLogicState &state )
		: State( state )
		, Value( state==LOGIC_True ? 1 : 0 )
	{
	}

	/* Constructor */
	CExpressionValue( const CLogicState &state, int value )
		: State( state )
		, Value( value )
	{
	}

	/* Copy constructor */
	CExpressionValue( const CExpressionValue &other )
		: State( other.State )
		, Value( other.Value )
	{
	}

	/* Convert to logic */
	CLogicState ToLogic() const
	{
		if ( State == LOGIC_True )
		{
			return Value ? LOGIC_True : LOGIC_False;
		}
		else
		{
			return State;
		}
	}

	/* Convert to integer */
	int ToInteger() const
	{
		if ( State == LOGIC_True )
		{
			return Value;
		}
		else
		{
			return 0;
		}
	}

	/* Convert to string */
	string ToString() const
	{
		return State.ToString() + ((string)" ") + ::ToString( Value );
	};
};

#endif