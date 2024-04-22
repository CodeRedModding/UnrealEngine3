// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CRuleTable::CRuleTable()
{
}

CRuleTable::CRuleTable( const CRuleTable &other )
{
	// Clear current list
	Clear();

	// Copy rules
	for ( size_t i=0; i<other.Rules.size(); i++ )
	{
		Rules.push_back( new CRule( *Rules[i] ) );
	}
}

CRuleTable::~CRuleTable()
{
	Clear();
}

void CRuleTable::Clear()
{
	for ( size_t i=0; i<Rules.size(); i++ )
	{
		delete Rules[i];
	}

	Rules.clear();
}

bool CRuleTable::LoadFromFile( const string &fileName )
{
	// Open file
	ifstream file;
	file.open( fileName.c_str(), ios::binary | ios::in );

	// If file is valid, read content
	if ( file.good() )
	{
		// Clear current rules
		Clear();

		// Parse rules
		while ( !file.eof() )
		{
			// Extract line
			string line;
			getline( file, line );
			stringstream str;
			str.str( line );

			// Parse command
			if ( CStringParser::ParseChar( str, '#' ))
			{
				// Get directive
				string cmd;
				CStringParser::ParseToken( str, cmd );

				if ( !Stricmp( cmd, "define" )) 
				{
					// Parse name and value
					string name, value;
					CStringParser::ParseToken( str, name );
					CStringParser::ParseToken( str, value );

					// Define rule
					AddRule( name, LOGIC_True, value );
				}
				else if ( !Stricmp( cmd, "undefine" ) )
				{
					// Parse name and value
					string name;
					CStringParser::ParseToken( str, name );

					// Define rule
					AddRule( name, LOGIC_False, "" );
				}
				else if ( !Stricmp( cmd, "weak") )
				{
					// Parse name and value
					string name;
					CStringParser::ParseToken( str, name );

					// Define rule
					AddRule( name, LOGIC_Weak, "" );
				}
			}
		}

		// Close file
		file.close();
	}

	// Loaded
	return true;
}

CRule *CRuleTable::FindRule( const string &name ) const
{
	for ( size_t i=0; i<Rules.size(); i++ )
	{
		CRule *rule = Rules[i];
		if ( rule->GetName() == name )
		{
			return rule;
		}
	}

	return NULL;
}

void CRuleTable::AddRule( const string &name, CLogicState state, const string &value )
{
	// Modify existing rule
	CRule *rule = FindRule( name );
	if ( rule )
	{
		// Redefine existing rule
		rule->Redefine( name, state, value );
	}
	else
	{
		// Add new rule
		Rules.push_back( new CRule( name, state, value ) );
	}
}


/* Get expression value for rule */
CExpressionValue CRuleTable::GetRuleValue( const string &name ) const
{
	CRule *rule = FindRule( name );

	if ( rule )
	{
		// Rules was defined, get value
		return CExpressionValue( rule->GetState(), atoi( rule->GetValue().c_str() ) );
	}
	else
	{
		// Rule was not defined, use DontCare logic
		return CExpressionValue( LOGIC_DontCare );
	}
}

CLogicState CRuleTable::GetRuleState( const string &name ) const
{
	CRule *rule = FindRule( name );
	return rule ? rule->GetState() : LOGIC_DontCare;
}