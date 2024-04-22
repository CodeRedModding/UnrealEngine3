// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#include "stdafx.h"
#pragma hdrstop

/*
 * CCodeLine 
 */

CCodeLine::CCodeLine( CCodeBlock *parent, int lineNumber, const string &code )
	: Parent( parent )
	, LineNumber( lineNumber )
	, Content( code )
{
	if ( Parent )
	{
		Parent->AddElement( this );
	}
}

const string &CCodeLine::GetContent() const
{
	return Content;
}

int CCodeLine::GetLineNumber() const
{
	return LineNumber;
}

CCodeBlock *CCodeLine::GetParentBlock() const
{
	return Parent;

}

CCodeBlock *CCodeLine::IsCodeBlock()
{
	return NULL;
}

void CCodeLine::Print( int level )
{
	CHAR lead[ 256 ];
	memset( lead, ' ', sizeof(lead) );
	lead[ level ] = 0;

	printf( "%sLine %03i: %s\n", lead, LineNumber, Content.c_str() );
}

void CCodeLine::Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output )
{
	if ( mask )
	{
		output.WriteLine( Content );
	}
	else
	{
		output.WriteLine( "// Removed" );
	}
}

/*
* CCodeBlock
*/

CCodeBlock::CCodeBlock( CCodeBlock *parent, int lineNumber, const string &code, EConditionalDirective conditonalType/*=EConditional_None*/ )
	: CCodeLine( parent, lineNumber, code )
	, ConditionalType( conditonalType )
{
}

CCodeBlock::~CCodeBlock()
{
	for ( size_t i=0; i<Lines.size(); i++ )
	{
		delete Lines[i];
	}
}

CCodeBlock *CCodeBlock::IsCodeBlock()
{
	return this;
}

CCodeRootBlock *CCodeBlock::IsRootBlock()
{
	return NULL;
}

CCodeConditionalBlock *CCodeBlock::IsConditionalBlock()
{
	return NULL;
}

void CCodeBlock::AddElement( CCodeLine *line )
{
	if ( line )
	{
		Lines.push_back( line );
	}
}

void CCodeBlock::Print( int level )
{
	CHAR lead[ 256 ];
	memset( lead, ' ', sizeof(lead) );
	lead[ level ] = 0;

	printf( "%sBlock (%s), line %i, %i subitems\n", lead, GetContent().c_str(), GetLineNumber(), Lines.size() );
	for ( size_t i=0; i<Lines.size(); i++ )
	{
		Lines[i]->Print( level+1 ); 
	}
}

void CCodeBlock::Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output )
{
	// Output contained lines
	for ( size_t i=0; i<Lines.size(); i++ )
	{
		Lines[i]->Filter( mask, exprCache, output );
	}
}

EConditionalDirective CCodeBlock::GetConditionalDirective() const
{
	return ConditionalType;
}


/*
* CCodeRootBlock
*/

CCodeRootBlock::CCodeRootBlock()
	: CCodeBlock( NULL, -1, "")
{
}

CCodeRootBlock::~CCodeRootBlock()
{
}

CCodeRootBlock *CCodeRootBlock::IsRootBlock()
{
	return this;
}

void CCodeRootBlock::Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output )
{
	for ( size_t i=0; i<Lines.size(); i++ )
	{
		Lines[i]->Filter( mask, exprCache, output );
	}
}

/*
* CCodeConditionalBlock
*/

CCodeConditionalBlock::CCodeConditionalBlock( CCodeBlock *parent, int lineNumber, const string &code )
	: CCodeBlock( parent, lineNumber, code )
{
}

CCodeConditionalBlock *CCodeConditionalBlock::IsConditionalBlock()
{
	return this;
}

void CCodeConditionalBlock::Print( int level )
{
	CHAR lead[ 256 ];
	memset( lead, ' ', sizeof(lead) );
	lead[ level ] = 0;

	printf( "%sConditional Block, %i cases\n", lead, Lines.size() );
	for ( size_t i=0; i<Lines.size(); i++ )
	{
		Lines[i]->Print( level+1 ); 
	}
}

void CCodeConditionalBlock::Filter( bool mask, CExpressionCache &exprCache, COutputDevice &output )
{
	// Filter sub blocks
	for ( size_t i=0; i<Lines.size(); i++ )
	{
		CLogicState condition;

		// Evaluate block condition
		CCodeBlock *block = Lines[i]->IsCodeBlock();
		if ( block->GetConditionalDirective() != EConditional_Else )
		{
			// Evaluate input conditions
			condition = EvaluateCondition( block->GetContent(), exprCache );
			printf( "Condition '%s' evaluated to '%s'\n", block->GetContent().c_str(), condition.ToString() );
		}
		else
		{
			// We don't give a fuck about else blocks
			condition = LOGIC_DontCare;
		}

		// Output block condition header (without filtering)
		Lines[i]->CCodeLine::Filter( mask, exprCache, output );

		// Output block if it's not canceled by rules
		BOOL filterFlag = condition != LOGIC_False;
		Lines[i]->Filter( filterFlag && mask, exprCache, output );
	}
}

CLogicState CCodeConditionalBlock::EvaluateCondition( const string &expr, CExpressionCache &exprCache )
{
	// Initialize parsing stream
	stringstream str;
	str.str( expr );

	// Parse preprocessor commands
	string command;
	CStringParser::ParseChar( str, '#' );
    CStringParser::ParseToken( str, command );

	// Ifdef
	if ( !Stricmp( command, "ifdef" ))
	{
		string ruleName;
		CStringParser::ParseToken( str, ruleName );
		return exprCache.GetRules().GetRuleState( ruleName );
	}

	// Ifndef
	if ( !Stricmp( command, "ifndef" ) )
	{
		string ruleName;
		CStringParser::ParseToken( str, ruleName );
		return ! exprCache.GetRules().GetRuleState( ruleName );
	}

	// Evaluate
	string expression;
	CStringParser::ParseRest( str, expression );
	return exprCache.Evaluate( expression );
}

/*
* CSourceFileDecomposer
*/

CSourceFileDecomposer::CSourceFileDecomposer()
{
}

CCodeRootBlock *CSourceFileDecomposer::DecomposeFile( CSourceFile *file )
{
	// Create root block
	CCodeRootBlock *root = new CCodeRootBlock;

	// Decompose
	DecomposeBlock( file, root );

	return root;
}

void CSourceFileDecomposer::DecomposeBlock( CSourceFile *file, CCodeBlock *block )
{
	// Parsing loop
	while ( !file->IsEOF() )
	{
		// Get line
		string line;
		line = file->ReadLine();

		// Initialize parsing stream
		stringstream str;
		str.str( line );

		// Parse preprocessor commands
		if ( ParseChar( str, '#' ))
		{
			// Get directive
			string command;
			if ( ParseToken( str, command ) )
			{
				
				// #endif
				if ( !Stricmp( command, "endif" ))
				{
					// End if, emit to parent block and exit parsing this one
					new CCodeLine( block->GetParentBlock()->GetParentBlock(), file->GetLineNumber(), line );

					// End parsing of this block
					break;
				}

				// #ifdef, #if
				if ( !Stricmp( command, "if" ) || !Stricmp( command, "ifdef") || !Stricmp( command, "ifndef") )
				{
					// Determine condition type
					EConditionalDirective type;
					if ( !Stricmp( command, "if" ) )
					{
						type = EConditional_If;
					}
					else if ( !Stricmp( command, "ifdef" ) )
					{
						type = EConditional_Ifdef;
					}
					else if ( !Stricmp( command, "ifndef" ) )
					{
						type = EConditional_Ifndef;
					}

					// Create conditional block that will enclose all options
					CCodeConditionalBlock *condBlock = new CCodeConditionalBlock( block, file->GetLineNumber(), "" );

					// Open new block (first conditional option) and recurse parsing
					CCodeBlock *newBlock = new CCodeBlock( condBlock, file->GetLineNumber(), line, type );
					DecomposeBlock( file, newBlock );

					// Line was parsed
					continue;
				}

				// #elif, #else
				if ( !Stricmp( command, "else" ) || !Stricmp( command, "elif") )
				{
					// Determine condition type
					EConditionalDirective type;
					if ( !Stricmp( command, "else" ) )
					{
						type = EConditional_Else;
					}
					else if ( !Stricmp( command, "elif" ) )
					{
						type = EConditional_Elif;
					}

					// End parsing of current block, add new block to parent and parse it
					CCodeBlock *newBlock = new CCodeBlock( block->GetParentBlock(), file->GetLineNumber(), line, type );
					DecomposeBlock( file, newBlock );

					// End parsing of this block
					break;
				}
			}
		}

		// Normal code line, add to this block
		new CCodeLine( block, file->GetLineNumber(), line );
	}
}
