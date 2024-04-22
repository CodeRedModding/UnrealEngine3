// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

CSimpleParser::CStackElem::CStackElem( const CToken &token )
	: Node( NULL )
	, Token( NULL )
{
	Token = new CToken( token );
}

CSimpleParser::CStackElem::CStackElem( CSyntaxNode *node )
	: Node( node )
	, Token( NULL )
{
}

bool CSimpleParser::CStackElem::Match( ETokenType type )
{
	if ( Node )
	{
		return type == EToken_Expr;
	}
	else if ( Token )
	{
		return type == Token->GetType();
	}
	else
	{
		return false;
	}
}

CToken *CSimpleParser::CStackElem::GetToken() const
{
	return Token;
}

CSyntaxNode *CSimpleParser::CStackElem::GetNode() const
{
	return Node;
}

CSimpleParser::CStackElem::~CStackElem()
{
	delete Token;
}

CSimpleParser::CStack::CStack()
{
}

CSimpleParser::CStack::~CStack()
{
	for ( size_t i=0; i<Stack.size(); i++ )
	{
		delete Stack[i];
	}
}

void CSimpleParser::CStack::Shift( const CToken &token )
{
	// Push new, token based node onto stack
	Stack.push_back( new CStackElem( token ) );
}

void CSimpleParser::CStack::Reduce( int elemCount, CSyntaxNode *newNode )
{
	// Cleanup shit
	for ( int i=0; i<elemCount; i++ )
	{
		delete Stack[ (int)Stack.size() - (1+i) ];
	}

	// Delete elements from the top of the stack
	Stack.resize( Stack.size() - elemCount );

	// Push new node onto stack
	if ( newNode )
	{
		Stack.push_back( new CStackElem( newNode ) );
	}
}

CSimpleParser::CStackElem *CSimpleParser::CStack::Get( int index )
{
	if ( index < (int)Stack.size() )
	{
		return Stack[ ((int)Stack.size()-1) + index ];
	}
	else
	{
		return NULL;
	}
}

bool CSimpleParser::CStack::Test( ETokenType a, ETokenType b/*=EToken_Invalid*/, ETokenType c/*=EToken_Invalid*/, ETokenType d/*=EToken_Invalid*/ )
{
	// Assemble more compact list of tokens to match
	ETokenType matchList[ 4 ] = {a,b,c,d};

	// Count tokens to match
	int matchCount = 0;
	for ( matchCount=0; matchCount<4; matchCount++ )
	{
		if ( matchList[matchCount] == EToken_Invalid )
		{
			break;
		}
	}

	// Make sure stack is big enough
	if ( matchCount <= (int)Stack.size() )
	{
		// Match tokens
		for ( int i=0; i<matchCount; i++ )
		{
			INT index = -matchCount + (i+1);
			if ( !Get( index )->Match( matchList[ i ] ) )
			{
				// Pattern not mached
				return false;
			}
		}

		// Pattern matched
		return true;
	}

	// Not matched
	return false;
}

CSyntaxNode *CSimpleParser::CStack::GetRoot() const
{
	// We should have only one node on stack
	if ( Stack.size() == 1 )
	{
		return Stack[ 0 ]->GetNode();
	}
	else
	{
		return NULL;
	}
}

void CSimpleParser::CStack::Clear()
{
	// Delete all stack elements
	for ( size_t i=0; i<Stack.size(); i++ )
	{
		delete Stack[ i ]->GetNode();
		delete Stack[ i ];
	}

	Stack.clear();
}

CSimpleParser::CSimpleParser()
{
}

CSyntaxNode *CSimpleParser::BuildSyntaxTree( const CTokenStream &inputTokens )
{
	// Create stream reader
	CTokenStreamReader reader( inputTokens );
	reader.Rewind();

	// Parser stack
	CStack stack;

	// Parsing loop
	for ( ;; )
	{
		// Get lookahead token
		CToken lookAheadToken;
		reader.Peek( lookAheadToken );

		// Try to reduce 
		if ( !Reduce( stack, lookAheadToken ) )
		{
			// Not reduced, shift
			if ( !Shift( stack, reader ) )
			{
				// End of data
				break;
			}
		}
	}

	// We should end up with single node on the stack
	CSyntaxNode *root = stack.GetRoot();

	// No valid root node, cleanup
	if ( !root )
	{
		stack.Clear();
	}

	return root;
}

bool CSimpleParser::Reduce( CStack &stack, const CToken &lookAheadToken )
{
	// defined ( id )
	if ( stack.Test( EToken_Defined, EToken_LPar, EToken_Ident, EToken_RPar ) )
	{
		string identName = stack.Get( -1 )->GetToken()->GetText();
		stack.Reduce( 4, new CSyntaxNode( ESyntaxNode_Defined, identName ) );
		return true;
	}

	// ( expr )
	if ( stack.Test( EToken_LPar, EToken_Expr, EToken_RPar ) )
	{
		CSyntaxNode *innerExpr = stack.Get( -1 )->GetNode();
		stack.Reduce( 3, innerExpr );
		return true;
	}

	// Logical or
	if ( stack.Test( EToken_Expr, EToken_Or, EToken_Expr ) )
	{
		CSyntaxNode *a = stack.Get( -2 )->GetNode();
		CSyntaxNode *b = stack.Get( 0 )->GetNode();
		stack.Reduce( 3, new CSyntaxNode( ESyntaxNode_Or, a, b ) );
		return true;
	}

	// Logical and
	if ( stack.Test( EToken_Expr, EToken_And, EToken_Expr ) )
	{
		CSyntaxNode *a = stack.Get( -2 )->GetNode();
		CSyntaxNode *b = stack.Get( 0 )->GetNode();
		stack.Reduce( 3, new CSyntaxNode( ESyntaxNode_And, a, b ) );
		return true;
	}

	// Not expression
	if ( stack.Test( EToken_Not, EToken_Expr ) )
	{
		CSyntaxNode *a = stack.Get( 0 )->GetNode();
		stack.Reduce( 2, new CSyntaxNode( ESyntaxNode_Not, a ) );
		return true;
	}

	// Ident
	if ( stack.Test( EToken_Ident ) )
	{
		// Don't reduce if we are parsing "defined" directive
		if ( stack.Test( EToken_Defined, EToken_LPar, EToken_Ident ) )
			return false;

		// Reduce
		string identName = stack.Get( 0 )->GetToken()->GetText();
		stack.Reduce( 1, new CSyntaxNode( ESyntaxNode_Ident, identName ) );
		return true;
	}

	// No known rules
	return false;
}

bool CSimpleParser::Shift( CStack &stack, CTokenStreamReader &reader )
{
	// Stream is not empty, push token
	if ( !reader.IsEOF() )
	{
		stack.Shift( *reader.Get() );
		return true;
	}

	// Out of tokens
	return false;
}
