// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CSyntaxNode::CSyntaxNode( ESyntaxNodeType type, CSyntaxNode *a/*=NULL*/, CSyntaxNode *b/*=NULL*/ )
	: Type( type )
{
	Children[0] = a;
	Children[1] = b;
}

CSyntaxNode::CSyntaxNode( ESyntaxNodeType type, const string &value )
	: Type( type )
	, Value( value )
{
	Children[0] = NULL;
	Children[1] = NULL;
}

CSyntaxNode::~CSyntaxNode()
{
	for ( int i=0; i<2; i++ )
	{
		delete Children[i];
	}
}

ESyntaxNodeType CSyntaxNode::GetType() const
{
	return Type;
}

const string &CSyntaxNode::GetValue() const
{
	return Value;
}

void CSyntaxNode::SetValue( const string &value )
{
	Value = value;
}

void CSyntaxNode::Print( int level ) const
{
	char s[100];
	memset( s, ' ', 100 );
	s[ level ] = 0;

	char *c;
	switch ( Type )
	{
		case ESyntaxNode_Ident: c = "Ident"; break;
		case ESyntaxNode_And: c = "And"; break;
		case ESyntaxNode_Or: c = "Or"; break;
		case ESyntaxNode_Defined: c = "Defined"; break;
		case ESyntaxNode_Not: c = "Not"; break;
	};

	printf( "%s%s %s\n", s, c, Value.c_str() );

	for ( int i=0; i<2; i++ )
	{
		if ( Children[i] )
		{
			Children[i]->Print( level+1 );
		}
	}
}

CExpressionValue CSyntaxNode::Evaluate( const CRuleTable &rules )
{
	switch ( Type )
	{
		// Identifier, get value
		case ESyntaxNode_Ident:
		{
			// Get rule value
			CExpressionValue value = rules.GetRuleValue( Value );
			printf( "Ident '%s' evaluated to '%s'\n", Value.c_str(), value.ToString().c_str() );
			return value;
		}

		// Defined directive
		case ESyntaxNode_Defined:
		{
			// Get rule state
			return CExpressionValue( rules.GetRuleState( Value ) );
		}

		// Logical not
		case ESyntaxNode_Not:
		{
			CExpressionValue value = Children[0]->Evaluate( rules );
			return CExpressionValue( !value.ToLogic() );
		}

		// Logical and
		case ESyntaxNode_And:
		{
			CExpressionValue a = Children[0]->Evaluate( rules );
			CExpressionValue b = Children[1]->Evaluate( rules );
			return CExpressionValue( a.ToLogic() && b.ToLogic() );
		}

		// Logical or
		case ESyntaxNode_Or:
		{
			CExpressionValue a = Children[0]->Evaluate( rules );
			CExpressionValue b = Children[1]->Evaluate( rules );
			return CExpressionValue( a.ToLogic() || b.ToLogic() );
		}
	}

	return CExpressionValue( LOGIC_DontCare );
}