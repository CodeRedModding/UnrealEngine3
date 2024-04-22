// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Code Preprocessor
=============================================================================*/

#pragma once

// Windows file system system
#ifdef _MSC_VER
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif						
	#include <windows.h>
#endif

#include <vector>
#include <cstring>
#include <string>
#include <map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <locale>

using namespace std;

template<class T>
string ToString( const T& val )
{
	stringstream strm;
	strm << val;
	return strm.str();
}

inline int Stricmp( const string& a, const string &b )
{
	return _stricmp( a.c_str(), b.c_str() );
}

#include "SourceFile.h"
#include "FileSystem.h"
#include "OutputDevice.h"
#include "FuzzyBool.h"
#include "ExpressionValue.h"
#include "RuleTable.h"
#include "StringParser.h"
#include "ExpressionCache.h"
#include "BlockDecomposer.h"
#include "Tokenizer.h"
#include "SyntaxNode.h"
#include "SimpleParser.h"

