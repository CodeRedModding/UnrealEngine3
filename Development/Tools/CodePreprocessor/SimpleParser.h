// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HSIMPLEPARSERH
#define HSIMPLEPARSERH

/*
 * Simple expression parser
 */

class CSimpleParser
{
public:
	/* Stack element */
	class CStackElem
	{		
	private:
		CToken*				Token;		// Raw token data
		CSyntaxNode*		Node;		// Created syntax node

	public:
		/* Construct from token */
		CStackElem( const CToken &token );

		/* Construct from syntax node */
		CStackElem( CSyntaxNode *node );

		/* Destructor */
		~CStackElem();

		/* Match type */
		bool Match( ETokenType type );

		/* Get token */
		CToken *GetToken() const;

		/* Get syntax node */
		CSyntaxNode *GetNode() const;
	};

	/* Parser stack */
	class CStack
	{
	private:
		vector< CStackElem* >	Stack;

	public:
		/* Constructor */
		CStack();

		/* Destructor */
		~CStack();

		/* Shift token onto stack */
		void Shift( const CToken &token);

		/* Reduce expression on stack */
		void Reduce( int elemCount, CSyntaxNode *newNode );		

		/* Get element from the stack, counting from the top */
		CStackElem *Get( int index );

		/* Try to match expression */
		bool Test( ETokenType a, ETokenType b=EToken_Invalid, ETokenType c=EToken_Invalid, ETokenType d=EToken_Invalid );

		/* Get root syntax node, returns NULL is element count on stack is different than 1 */
		CSyntaxNode *GetRoot() const;

		/* Cleanup, deletes all nodes */
		void Clear();
	};

public:
	/* Constructor */
	CSimpleParser();

	/* Build syntax tree from token stream */
	CSyntaxNode *BuildSyntaxTree( const CTokenStream &inputTokens );

protected:
	/* Try to reduce expression on stack, returns true if reduced */
	bool Reduce( CStack &stack, const CToken &lookAheadToken );

	/* Try to shift token on stack */
	bool Shift( CStack &stack, CTokenStreamReader &reader );
};


#endif