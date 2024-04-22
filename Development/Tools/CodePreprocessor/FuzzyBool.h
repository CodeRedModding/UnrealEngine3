// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HFUZZYBOOLH
#define HFUZZYBOOLH

/*
 * Logic states
 */

enum ELogicState
{
	LOGIC_False=0,
	LOGIC_True=1,
	LOGIC_DontCare=2,
	LOGIC_Weak=3,
};

/*
 * Tri state logic 
 */

class CLogicState
{
private:
	ELogicState	State;	

public:
	/* Constructor */
	inline CLogicState( ELogicState state=LOGIC_DontCare )
		: State( state )
	{
	}

	/* Copy constructor */
	inline CLogicState( const CLogicState &state )
		: State( state.State )
	{
	}

	/* Assignment operator */
	inline CLogicState &operator=( const CLogicState &other )
	{
		State = other.State;
		return *this;
	}

	/* Logic comparison */
	inline bool operator==( const CLogicState &other ) const
	{
		return State == other.State;
	}

	/* Logic comparison */
	inline bool operator!=( const CLogicState &other ) const
	{
		return State != other.State;
	}

	/* And operator */
	inline CLogicState operator&&( const CLogicState &other ) const
	{
		static ELogicState results[4][4] =
		{
			{ LOGIC_False,		LOGIC_False,		LOGIC_DontCare,		LOGIC_False },
			{ LOGIC_False,		LOGIC_True,			LOGIC_DontCare,		LOGIC_True },
			{ LOGIC_DontCare,	LOGIC_DontCare,		LOGIC_DontCare,		LOGIC_DontCare },
			{ LOGIC_False,		LOGIC_True,			LOGIC_DontCare,		LOGIC_Weak }
		};

		return CLogicState( results[ State ][ other.State ] );
	}

	/* Or operator */
	inline CLogicState operator||( const CLogicState &other ) const
	{
		static ELogicState results[4][4] =
		{
			{ LOGIC_False,		LOGIC_True, 		LOGIC_DontCare,		LOGIC_False },
			{ LOGIC_True, 		LOGIC_True,			LOGIC_DontCare,		LOGIC_True },
			{ LOGIC_DontCare,	LOGIC_DontCare,		LOGIC_DontCare,		LOGIC_DontCare },
			{ LOGIC_False,		LOGIC_True,			LOGIC_DontCare,		LOGIC_Weak }
		};

		return CLogicState( results[ State ][ other.State ] );
	}

	/* Negation operator */
	inline CLogicState operator!( ) const
	{
		static ELogicState results[4] = { LOGIC_True, LOGIC_False, LOGIC_DontCare, LOGIC_Weak };
		return CLogicState( results[ State ] );
	}

	/* Conversion to string */
	inline const char *ToString() const
	{
		const char *names[4] = { "FALSE", "TRUE", "DONTCARE", "WEAK" };
		return names[ State ];
	}
};

#endif
