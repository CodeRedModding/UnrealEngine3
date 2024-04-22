// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once
#ifndef HTOKENIZERH
#define HTOKENIZERH

/*
 * Token type
 */

enum ETokenType
{
	/* Raw token types */
	EToken_Invalid=0,
	EToken_Ident,
	EToken_Defined,
	EToken_Or,
	EToken_And,
	EToken_Not,
	EToken_LPar,
	EToken_RPar,
	EToken_Value,

	/* Special types, used in parser only */
	EToken_Expr,
};

/*
 * Token
 */

class CToken
{
private:
	ETokenType	Type;
	string		Text;
	int			Value;

public:
	/* Default constructor */
	inline CToken()
		: Type( EToken_Invalid )
		, Value( 0 )
	{
	}

	/* Constructor */
	inline CToken( ETokenType type, const string &text, int value=0 )
		: Type( type )
		, Text( text )
		, Value( value )
	{
	}

	/* Copy constructor */
	inline CToken( const CToken &token )
		: Type( token.Type )
		, Text( token.Text )
		, Value( token.Value )
	{
	}

	/* Get token type */
	inline ETokenType GetType() const
	{
		return Type;
	}

	/* Get token text */
	inline const string &GetText() const
	{
		return Text;
	}

	/* Get token value (for integer type) */
	inline int GetValue() const
	{
		return Value;
	}
};

/*
 * Token stream
 */

class CTokenStream
{
private:
	vector< CToken* >	 	Tokens;

public:
	/* Constructor */
	CTokenStream();

	/* Destructor */
	~CTokenStream();

	/* Add token to stack */
	void AddToken( CToken* token );

	/* Get size */
	int Size() const;

	/* Get token */
	CToken *GetToken( int index ) const;

	/* Get token */
	bool GetToken( CToken &token, int index ) const;
};

/*
 * Token stream reader
 */

class CTokenStreamReader
{
private:
	const CTokenStream&		Stream;
	int						Pos;

public:
	/* Constructor */
	CTokenStreamReader( const CTokenStream &stream );

	/* Rewind to beginning */
	void Rewind();

	/* EOF reached ? */
	bool IsEOF() const;

	/* Peek token */
	CToken *Peek( int offset=0 );
	
	/* Peek token */
	bool Peek( CToken &token, int offset=0 );

	/* Get token */
	CToken *Get();
};

/*
 * Tokenizer
 */

class CTokenizer : public CStringParser
{
public:
	/* Constructor */
	CTokenizer();

	/* Tokenize string */
	bool Tokenize( const string &text, CTokenStream &tokens );

protected:
	/* Try to match token */
	ETokenType MatchToken( const char *str, int n, bool fullMatch );
};


#endif
