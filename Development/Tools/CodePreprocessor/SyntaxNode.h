// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HSYNTAXNODEH
#define HSYNTAXNODEH

/* 
 * Syntax node type
 */

enum ESyntaxNodeType
{
	ESyntaxNode_Ident,
	ESyntaxNode_And,
	ESyntaxNode_Or,
	ESyntaxNode_Defined,
	ESyntaxNode_Not,
};

/*
 * Syntax node
 */

class CSyntaxNode
{
private:
	ESyntaxNodeType		Type;			// Type of syntax node
	CSyntaxNode*		Children[2];	// Child nodes
	string				Value;			// Data string

public:
	/* Constructor */
	CSyntaxNode( ESyntaxNodeType type, CSyntaxNode *a=NULL, CSyntaxNode *b=NULL );

	/* Constructor */
	CSyntaxNode( ESyntaxNodeType type, const string &value );

	/* Destructor */
	~CSyntaxNode();

	/* Get node type */
	ESyntaxNodeType GetType() const;

	/* Get node value */
	const string &GetValue() const;

	/* Set node value */
	void SetValue( const string &value );

	/* Print (Debug only) */
	void Print( int level ) const;

	/* Evaluate node value */
	CExpressionValue Evaluate( const CRuleTable &rules );
};

#endif