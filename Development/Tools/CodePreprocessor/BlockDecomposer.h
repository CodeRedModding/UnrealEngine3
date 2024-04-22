// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HBLOCKDECOMPOSERH
#define HBLOCKDECOMPOSERH

class CCodeBlock;
class CCodeRootBlock;
class CCodeConditionalBlock;

/* 
 * Preprocesor conditinal directives 
 */

enum EConditionalDirective
{
	EConditional_None,
	EConditional_Ifdef,
	EConditional_Ifndef,
	EConditional_If,
	EConditional_Elif,
	EConditional_Else,
};

/*
 * Source code line
 */

class CCodeLine
{
private:
	CCodeBlock*	Parent;			// Parent block
	int			LineNumber;		// Line number in source file
	string		Content;		// Source code in that line

public:
	/* Constructor */
	CCodeLine( CCodeBlock *parent, int lineNumber, const string &code );

	/* Get source code */
	const string &GetContent() const;

	/* Get line number */
	int GetLineNumber() const;

	/* Get parent block */
	CCodeBlock *GetParentBlock() const;

public:
	/* Is a code block ? */
	virtual CCodeBlock *IsCodeBlock();

	/* Debug print */
	virtual void Print( int level );

	/* Code filtering */
	virtual void Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output );
};

/* 
 * Block of source code 
 */

class CCodeBlock : public CCodeLine
{
protected:
	vector<CCodeLine *>		Lines;				/* Sub lines */
	EConditionalDirective	ConditionalType;	/* Type of conditional rule */

public:
	/* Constructor */
	CCodeBlock( CCodeBlock *parent, int lineNumber, const string &code, EConditionalDirective conditonalType=EConditional_None );

	/* Destructor */
	~CCodeBlock();

	/* Add element */
	void AddElement( CCodeLine *line );

	/* Get conditional type */
	EConditionalDirective GetConditionalDirective() const;

public:
	/* Is a code block ? */
	virtual CCodeBlock *IsCodeBlock();

	/* Is this a root block ? */
	virtual CCodeRootBlock *IsRootBlock();

	/* Is this a conditional block ? */
	virtual CCodeConditionalBlock *IsConditionalBlock();

	/* Debug print */
	virtual void Print( int level );

	/* Code filtering */
	virtual void Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output );
};

/* 
 * Code root node (whole file)
 */

class CCodeRootBlock : public CCodeBlock
{
public:
	/* Constructor */
	CCodeRootBlock();

	/* Destructor */
	~CCodeRootBlock();
	
	/* Code filtering */
	virtual void Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output );

public:
	/* Is this a root block ? */
	virtual CCodeRootBlock *IsRootBlock();
};

/*
 * Code conditional block (#if/#else/#endif etc)
 */

class CCodeConditionalBlock : public CCodeBlock
{
public:
	/* Constructor */
	CCodeConditionalBlock( CCodeBlock *parent, int lineNumber, const string &code );

	/* Debug print */
	virtual void Print( int level );

public:
	/* Is this a conditional block ? */
	virtual CCodeConditionalBlock *IsConditionalBlock();

	/* Code filtering */
	virtual void Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output );

protected:
	/* Evaluate condition */
	static CLogicState EvaluateCondition( const string &expr, CExpressionCache &exprCache );
};

/* File decomposer */

class CSourceFileDecomposer : public CStringParser
{
public:
	/* Constructor */
	CSourceFileDecomposer();

	/* Decompose file */
	CCodeRootBlock *DecomposeFile( CSourceFile *file );

private:
	/* Decompose block */
	void DecomposeBlock( CSourceFile *file, CCodeBlock *block );
};

#endif
