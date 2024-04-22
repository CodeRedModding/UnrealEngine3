/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __UNSCRPRECOM_H__
#define __UNSCRPRECOM_H__

/**
 * Property value types for the `ifcondition macro
 */
enum EIfPropertyType
{
	IPT_Bool,
	IPT_Int,
	IPT_Float,
	IPT_String
};

/**
 * A simple struct for tracking macro parameter names and definitions
 *
 */
struct FScriptMacroDefinition
{
	/** actual definition of the macro */
	FString				MacroDefinition;

	/** @name FScriptMacroDefinition implementation */
	//@{
	const TArray<FString>& GetParameterNames() const
	{
		return MacroParameterNames;
	}

	const FString* GetParameterName( INT Index ) const
	{
		if ( MacroParameterNames.IsValidIndex(Index) )
			return &MacroParameterNames(Index);

		return NULL;
	}

	/**
	 * Adds a new parameter to the macro definition
	 *
	 * @param   NewParameter	the new parameter to add
	 * @param   Index			index to insert the new parameter; if not specified, inserted at the end of the list.
	 * 
	 * @return  position of the parameter in the MacroParameters array, if the parameter was added successfully.
	 *			INDEX_NONE if the parameter couldn't be added.
	 *
	 */
	INT AddParameterName( const TCHAR* NewParameter, INT Index = INDEX_NONE )
	{
		if ( NewParameter == NULL )
			return INDEX_NONE;

		if ( Index == INDEX_NONE )
			Index = MacroParameterNames.Num();

		MacroParameterNames.InsertZeroed(Index);
		MacroParameterNames(Index) = NewParameter;
		return Index;
	}
	//@}


	/** @name Constructors */
	//@{
	FScriptMacroDefinition()
	{ }

	FScriptMacroDefinition( const TCHAR* inValue )
	: MacroDefinition(inValue)
	{ }

	FScriptMacroDefinition( const TArray<FString>& inParameterNames, const TCHAR* inValue )
	: MacroParameterNames(inParameterNames), MacroDefinition(inValue)
	{ }

	/** Copy constructor */
	FScriptMacroDefinition( const FScriptMacroDefinition& Other )
	{
		MacroParameterNames = Other.MacroParameterNames;
		MacroDefinition = Other.MacroDefinition;
	}
	//@}

private:

	/** names for the parameters of the macro. */
	TArray<FString>		MacroParameterNames;
};

/**
* This class represents objects that contain text which will be emitted into the output buffer (post-processed uc file).
* Can be a .uc file, include file, macro or macro parameter.
*/
class FCharacterProvider
{
public:
	//@name Character interface
	//@{
	/** 
	* Return a copy of the next character without consuming it
	*
	* @return	the next character in the buffer
	*/
	TCHAR PeekChar(INT Offset = 0) const 
	{
		INT dLen = PriorityStack.Len();
		if (dLen > Offset)
		{
			return PriorityStack[dLen - (Offset + 1)];
		}
		else
		{
			return *BufferStart;
	}
	}

	/** 
	* Return and consume the next character in the input
	* 
	* @return	the next character in the buffer
	*/
	TCHAR GetChar() 
	{
		TCHAR retval;
		INT dLen = PriorityStack.Len();
		if (dLen) 
		{
			retval = PriorityStack[dLen - 1];
			PriorityStack = PriorityStack.Left(dLen-1);
		}
		else if (BufferStart != BufferEnd) 
		{
			retval = *BufferStart++;
		}
		else 
		{
			retval = '\0';
			bComplete = true;
		}

		++InputPos;
		if ( retval == TCHAR('\n')/* ||
			(retval == TCHAR('\r') && (PeekChar() != TCHAR('\n')))*/ )
		{
			++InputLine;
		}

		return retval;
	}

	/**
	* Puts a character back into the front of the input stream
	*
	* @param	ch	character to put back into the input stream
	*/
	void UngetChar(TCHAR ch) 
	{
		if (ch == 0)
			return;

		if ( ch == TCHAR('\n') )
		{
			--InputLine;
		}

		--InputPos;
		PriorityStack += ch;
	}

	/**
	* push the specified string onto the PriorityStack; requires reversing the incoming
	* string first so that the characters pop off in the right order
	*
	* @param	str		the string to inject into the input stream
	*/
	void PushString(const FString& str) 
	{
		PriorityStack += str.Reverse();
	}

	/**
	* Returns whether this FCharacterProvider has any characters left to process.
	*
	* @return	TRUE if we have more characters to process.
	*/
	operator UBOOL() const 
	{
		return (PriorityStack.Len() || (BufferStart != BufferEnd)) || !bComplete;
	}
	//@}

	//@name File interface
	//@{
	/**
	* Gets the filename for the character provider.  Only valid
	* if this FCharacterProvider represents an include file.
	*
	* @return  the filename for this character provider.
	*/
	FString GetFilename() const
	{
		return Filename;
	}

	/**
	* Sets the filename for this character provider
	*
	* @param	NewFilename		the new filename
	* 
	* @return  the filename for this character provider
	*/
	FString SetFilename(const FString& NewFilename) 
	{
		Filename = NewFilename;
		return Filename;
	}

	/**
	* Gets the current line number
	*
	* @return	the current line number
	*/
	INT GetLineNumber() const
	{
		return InputLine;
	}

	/**
	* Gets the current location.  
	*
	* @return  the current location in the FCharacterProvider's buffer; of the format <filename>:<line number>
	*/
	FString CurrentLocation() const
	{
		if (macroName_ != TEXT("")) 
		{
			return FString::Printf(TEXT("%s(%d)"), *macroName_.ToString(), InputLine+1);
		}
		else if (Filename != TEXT("")) 
		{
			return FString::Printf(TEXT("%s(%d)"), *Filename, InputLine);
		}
		else 
		{
			return FString();
		}
	}
	//@}

	//@name Macro interface
	//@{
	/**
	* Get the name of the macro represented by this character provider.  Only
	* valid if this FCharacterProvider represents a macro reference.
	*
	* @return the name of the macro for this character provider
	*/
	FName GetMacroName() const 
	{
		return macroName_;
	}

	/**
	* Set the macro name for this character provider
	*
	* @param	NewMacroName	name for the macro
	* 
	* @return  the name of the macro for this character provider
	*/
	FName SetMacroName(const FName& NewMacroName) 
	{
		macroName_ = NewMacroName;
		return macroName_;
	}

	/**
	 * Replaces macro parameter names in the macro literal with the actual values specified when the
	 * macro was invoked.  Macros should always have at least one name/value pair; in cases where the macro
	 * has no parameters (such as macro parameters themselves), it should have a key/value pair that gives
	 * the name of the macro and the definition of that macro, or in the case of a macro parameter, the value
	 * assigned to that macro parameter.
	 *
	 * @param	ParamNames	pointer to an array of parameter names for the macro which corresponds to this CharacterProvider
	 * @param	ParamValues	pointer to an array of parameter values for the macro which corresponds to this CharacterProvider
	 */
	void ParseMacroParameterValues(const TArray<FString>* ParamNames, TArray<FString>* ParamValues) 
	{
		check(SymbolMap == NULL);
		check(ParamNames);

		SymbolMap = new TMap<FName, FScriptMacroDefinition>;

		// here is where the parameter names are set
		for (INT i = 0; i < ParamNames->Num(); ++i)
		{
			const FString& curParamName = (*ParamNames)(i);
			SymbolMap->Set(*curParamName, ParamValues && i < ParamValues->Num() ? FScriptMacroDefinition(*(*ParamValues)(i)) : FScriptMacroDefinition());
		}

		// the number of parameters for this macro is stored as the last "parameter"
		SymbolMap->Set(TEXT("#"), FScriptMacroDefinition(ParamNames && ParamNames->Num() > 0 ? *appItoa(ParamNames->Num()) : TEXT("0")));
	}

	/**
	* Retrieves the definition for the specified macro.
	*
	* @param	macroName	the name of the macro to lookup
	* @param	definition	[out] the definition of the macro;  may be empty if the definition isn't found or if the macro is defined as empty
	* 
	* @return	TRUE if the macro has been defined in this CharacterProvider
	*/
	UBOOL Lookup(const FName& macroName, FScriptMacroDefinition*& definition) 
	{
		// .uc/include files don't have SymbolMaps (their defines are stored in the global symbol table)
		if ( SymbolMap == NULL )
			return false;

		FScriptMacroDefinition* MacroValue = SymbolMap->Find(macroName);
		if ( MacroValue == NULL )
			return false;

		definition = MacroValue;
		return true;
	}
	//@}

	FString GetProviderName()
	{
		if ( Filename.Len() )
			return Filename;

		else if ( macroName_ != NAME_None )
			return macroName_.ToString();

		return FString();
	}

	FCharacterProvider(const TCHAR* begin, const TCHAR* end, const FString& file = FString(), INT lineNo = 0)
	: Input(begin),  Filename(file), InputLine(lineNo)
	, InputPos(0), SymbolMap(0), bComplete(false) , macroName_(NAME_None)
	{
		BufferStart = *Input;
		BufferEnd = BufferStart + (end - begin);
	}

	~FCharacterProvider()
	{
		if ( SymbolMap != NULL )
			delete SymbolMap;

		SymbolMap = NULL;
	}

private:
	//@name File interface
	//@{
	/** the name of the file being expanded in this FCharacterProvider */
	FString Filename;

	/** current line number of the buffer */
	INT InputLine;
	//@}

	//@name Macro interface
	//@{
	/** macros defined in the file for this FCharacterProvider */
	TMap<FName, FScriptMacroDefinition>* SymbolMap;

	/** name of the macro being expanded in this FCharacterProvider */
	FName macroName_;
	//@}

	//@name Character interface
	//@{
	/** current position of the buffer */
	INT InputPos;

	/** the buffer for this character provider */
	FString Input;

	/** pointer to the start of the buffer */
	const TCHAR* BufferStart;

	/** pointer to the end of the buffer */
	const TCHAR* BufferEnd;

	/**
	* a stack of characters which is always processed before the main input buffer.  Typically
	* contains characters that have been returned via UngetChar().  Is also used to manually
	* inject text into the output stream.
	* (all insertions are reversed so insertion/deletion takes place at the END of the PriorityStack).
	*/
	FString PriorityStack;
	//@}

	/** FALSE if the current position is not at the end of the buffer */
	UBOOL bComplete;
};

/**
 * Maintains a stack of "character providers" (unrealscript files, include files, and macros)
 * which are used by the preprocessor text filter classes.
 */
class FCharacterSource
{
public:
	FCharacterSource()
	{}

	/**
	 * Returns the current location of the currently active CharacterProvider
	 *
	 * @return	the location of the currently active provider
	 */
	FString CurrentLocation( UBOOL bNoMacros=FALSE )
	{
		if ( ProviderStack.Num() == 0 )
			return TEXT("");

		if ( bNoMacros )
		{
			for ( INT ProviderIndex = ProviderStack.Num() - 1; ProviderIndex >= 0; ProviderIndex-- )
			{
				FCharacterProvider* Prov = ProviderStack(ProviderIndex);
				if ( Prov && Prov->GetMacroName() == NAME_None )
				{
					return Prov->CurrentLocation();
				}
			}
		}
		return ProviderStack.Top()->CurrentLocation();
	}

	/**
	 * Gets the name of the currently active character provider.
	 *
	 * @return	the name of the active provider
	 */
	FString GetCurrentProvider()
	{
		if ( ProviderStack.Num() == 0 )
			return TEXT("");

		return ProviderStack.Top()->GetProviderName();
	}

	/**
	* Gets the current line number
	*
	* @return	the current line number
	*/
	INT GetCurrentLineNumber()
	{
		if ( ProviderStack.Num() == 0 )
			return 1;

		return ProviderStack.Top()->GetLineNumber();
	}
	
	/**
	 * Add a raw buffer to the stack of character providers
	 *
	 * @param   File_BufferEnd	a pointer to the start of the buffer
	 * @param   File_BufferEnd	a pointer to the end of the buffer
	 */
	void Add(const TCHAR* File_BufferStart, const TCHAR* File_BufferEnd)
	{
		ProviderStack.Push(new FCharacterProvider(File_BufferStart, File_BufferEnd));
	}

	/**
	* Add a new file to the stack of character providers, changing the active
	* provider to the specified file (we'll now be spewing this file's contents
	* into the ouput stream)
	*
	* @param	File_BufferEnd		a pointer to the start of the buffer
	* @param	File_BufferEnd		a pointer to the end of the buffer
	* @param	Filename			the name of the file being added
	* @param	LineNumber			the line number that the provider will consider its first line
	*/
	void AddFile(const TCHAR* File_BufferStart, const TCHAR* File_BufferEnd, const FString& Filename, INT LineNumber=1)
	{
		ProviderStack.Push(new FCharacterProvider(File_BufferStart, File_BufferEnd, Filename, LineNumber));
	}

	/**
	 * Evaluate a macro and add it to the stack of CharacterProviders, changing the active provider
	 * to the specified macro (we'll now be spewing the expanded macro into the output stream).
	 *
	 * @param   Macro_BufferStart	a pointer to the beginning of the buffer containing the macro
	 * @param   Macro_BufferEnd		a pointer to the end of the buffer containing the macro
	 * @param   MacroName			the name of the macro to process
	 * @param	MacroDef			the information about the macro we're expanding
	 * @param   parameters			the values that will be used in evaluating this macro
	 */
	void AddMacro(const TCHAR* Macro_BufferStart, const TCHAR* Macro_BufferEnd, const FName& MacroName, FScriptMacroDefinition* MacroDef, TArray<FString>* Values) 
	{
		FCharacterProvider* NewProvider = new FCharacterProvider(Macro_BufferStart, Macro_BufferEnd);
		if ( NewProvider != NULL )
		{
			ProviderStack.Push(NewProvider);
			NewProvider->SetMacroName(MacroName);
			NewProvider->ParseMacroParameterValues( &MacroDef->GetParameterNames(), Values);
		}
	}

	/**
	 * Returns whether the active provider is a macro.
	 *
	 * @return	TRUE if the active provider is an expanded macro definition
	 */
	UBOOL IsMacro()
	{
		return ProviderStack.Num() && ProviderStack.Top()->GetMacroName() != NAME_None;
	}

	/**
	 * Retrieves the next character from the currently active provider without
	 * modifying the position of the input buffer.
	 *
	 * @return	the next character in the current provider's buffer.
	 */
	TCHAR PeekChar(INT Offset = 0) const
	{
		if (ProviderStack.Num() == 0)
	{
			return '\0';
		}

		FCharacterProvider* ActiveProvider = ProviderStack.Top();
		if ( ActiveProvider == NULL || !(*ActiveProvider) )
		{
			return '\0';
		}

		return ActiveProvider->PeekChar(Offset);
	}

	/**
	 * Retrieves the next character from the currently active provider, advancing
	 * the position of the input buffer.
	 *
	 * @return	the next character in the current character provider's buffer.
	 */
	TCHAR GetChar()
	{
		UBOOL FixupLineNumber = false;

		// if the active provider is out of characters,
		// remove it from stack and delete it.
		while ( ProviderStack.Num() != 0 && !(*ProviderStack.Top()) )
		{
			FCharacterProvider* activeProvider = ProviderStack.Pop();
			FixupLineNumber = FixupLineNumber || (activeProvider->GetFilename() != TEXT(""));
			delete activeProvider;
		}

		// if we don't any more character to process at all, return the null character
		if ( !UBOOL(*this) )
			return '\0';

		if (FixupLineNumber)
		{
			// if we just removed a file from the provider stack, we'll need to manually set the line number
			// to the last line that the previously active provider was at
			PushString( FString::Printf(TEXT("#linenumber %i"), ProviderStack.Top()->GetLineNumber()) );
		}

		return ProviderStack.Top()->GetChar();
	}

	/**
	 * Puts the specified character back into the input buffer, and backs up the position of the input buffer.
	 *
	 * @param	ch	the character to put back
	 */
	void UngetChar(TCHAR ch) 
	{
		if (ProviderStack.Num())
		{
			ProviderStack.Top()->UngetChar(ch);
		}
	}

	/**
	 * Injects the specified string into the input buffer.
	 *
	 * @param	str		the string to inject into the input stream
	 */
	void PushString( const FString& str ) 
	{
		if ( ProviderStack.Num() )
		{
			ProviderStack.Top()->PushString(str);
		}
	}

	/**
	 * Determines if we have any more characters left to process.
	 *
	 * @return	TRUE if we still have characters to process.
	 */
	operator UBOOL() const
	{
		// only thing that could be left on the stack is a provider
		// that is not empty. Thus if the stack is empty, we are done (false)
		// and if the stack is not empty, input remains.
		return (ProviderStack.Num() != 0);
	}

	/**
	 * Retrieves the definition for the specified macro.
	 *
	 * @param	macroName	the name of the macro to lookup
	 * @param	definition	[out] the definition of the macro;  may be empty if the definition isn't found or if the macro is defined as empty
	 * 
	 * @return	TRUE if the macro has been defined.
	 */
	UBOOL Lookup(const FName& macroName, FScriptMacroDefinition*& definition) 
	{
		if ( ProviderStack.Num() > 0 )
		{
			return ProviderStack.Top()->Lookup(macroName, definition);
		}

		return false;
	}

private:
	TArray<FCharacterProvider *> ProviderStack;
};




/**
 * The base class for text manipulation filters.
 */
class FTextFilter : public FContextSupplier

{
public:
	FTextFilter(const FString & fileName, FFeedbackContext* inWarn = GWarn)
	: Filename(fileName)
	, Warn(inWarn)
	, PreviousContext(NULL)
	, FContextSupplier()
	{
		if ( Warn )
		{
			PreviousContext = Warn->GetContext();
			Warn->SetContext(this);
		}
	}

	/**
	* Compiler generated copy constructor and assignment operator are
	* fine for the general case.
	*/
	virtual ~FTextFilter()
	{
		if ( Warn )
		{
			Warn->SetContext(PreviousContext);
		}
	}

	virtual FString GetContext()
	{
		return Filename;
	}

	/** 
	* The meat of the filter. Filters are provided with pointers to the beginning and
	* end of a character buffer. They return a result buffer built by "processing"
	* the contents of the input buffer. Processing has different meaning for different
	* filters. 
	*/
	virtual void Process(const TCHAR* Begin, const TCHAR* End, FString& Result) = 0;

protected:

	FString Filename;

	/** Where do warnings get sent? */
	FFeedbackContext* Warn;

	/** the supplier previously handling context */
	FContextSupplier* PreviousContext;
};



/**
 * Composes two text filters, uses the result of applying to first filter as the input
 * for the second filter and returns the post-processed text.
 */
class FSequencedTextFilter : public FTextFilter
{
public:
	FSequencedTextFilter(FTextFilter& first, FTextFilter& second, const FString& fileName, FFeedbackContext* Warn = GWarn)
	: FirstFilter(first), SecondFilter(second), FTextFilter(fileName, Warn) 
	{}

	virtual void Process(const TCHAR* Begin, const TCHAR* End, FString& Result) 
	{
		FString ProcessedText;
		const TCHAR* ProcessedBufferStart;
		const TCHAR* ProcessedBufferEnd;

		FirstFilter.Process(Begin, End, ProcessedText);

		ProcessedBufferStart = *ProcessedText;
		ProcessedBufferEnd = ProcessedBufferStart + ProcessedText.Len();
		SecondFilter.Process(ProcessedBufferStart, ProcessedBufferEnd, Result);
	}

private:
	FTextFilter& FirstFilter;
	FTextFilter& SecondFilter;

};

/**
 * Given a buffer of text, replaces all //-style comments with an end of line
 * character sequence (stripping the comment) and replaces all /* and * / comments
 * with a single space if they are on a single line or replacing them with  the
 * appropriate number of new lines if they span multiple lines.
 * Quotes are respected outside of comments, so "//" doesn't begin a comment.
 */
class FCommentStrippingFilter : public FTextFilter
{
public:
	FCommentStrippingFilter(const FString& fileName, FFeedbackContext* Warn = GWarn) 
	: FTextFilter(fileName, Warn)
	{}

	virtual void Process(const TCHAR* Begin, const TCHAR* End, FString& Result);
private:
	FCharacterSource SourceBuffer;
};


/**
 * This class processes a buffer of text, expanding macro expressions.
 * Constructed with the name of the package; this gives an initial search
 * path for included files.
 */
class FMacroProcessingFilter : public FTextFilter
{
	friend class UMakeCommandlet;

public:
	FMacroProcessingFilter(const TCHAR* pName, const FString& fileName, FFeedbackContext* Warn = GWarn);

	/** processes the global include file for the package the to-be-processed file is in
	 * @note: will throw exceptions for syntax errors, so this needs to be called manually as this object is set as the context
	 *			of the output device and thus the constructor must be outside any 'try' block while this function must be inside one
	 */
	void ProcessGlobalInclude();

	virtual void Process(const TCHAR* Begin, const TCHAR* End, FString& Result);

	virtual FString GetContext();

	/** the character that invokes a macro */
	static const TCHAR CALL_MACRO_CHAR = TEXT('`');

	/** the character that begins a delimited macro name region */
	static const TCHAR BEGIN_MACRO_BLOCK_CHAR = TEXT('{');

	/** the character that marks the end of a delimited macro name region */
	static const TCHAR END_MACRO_BLOCK_CHAR = TEXT('}');

	/** the special macro name used to represent the number of parameters a macro has */
	static const TCHAR MACRO_PARAMCOUNT_CHAR = TEXT('#');

private:

	/**
	 * Initializes the global macro table and adds any hard-coded macro names, such as "debug" and "final_release"
	 */
	void InitSymbolTable();

	/**
	 * Retrieves the definition of the specified macro.
	 *
	 * @param	macroName		the name of the macro to lookup
	 * @param	definition		[out] the definition for the macro; only valid if returns TRUE.
	 * @param	bIgnoreGlobals	specify TRUE to prevent searching the global symbol map
	 * 
	 * @return	TRUE if the specified macro exists in the macro table.
	 */
	UBOOL Lookup(const FName& macroName, FScriptMacroDefinition*& definition,UBOOL bIgnoreGlobals=FALSE) 
	{
		UBOOL bResult = FALSE;

		// see if the macro has been declared in the scope of the file/macro that is currently being parsed
		if ( SourceBuffer.Lookup(macroName, definition) )
		{
			bResult = TRUE;
		}
		else
		{
			// see if the macro exists in the global list
			FScriptMacroDefinition* Macro = CurrentSymbols.Find(macroName);
			if ( Macro != NULL )
			{
				definition = Macro;
				bResult = TRUE;
			}
			else if ( !bIgnoreGlobals )
			{
				check(GlobalSymbols);
				Macro = GlobalSymbols->Find(macroName);
				if ( Macro != NULL )
				{
					definition = Macro;
					bResult = TRUE;
				}
			}
		}

		return bResult;
	}

	/**
	 * Parses the name of the macro from the buffer.  SourceBuffer should be positioned at the first character
	 * of the name, or at a BEGIN_MACRO_BLOCK_CHAR.  Advances the position of SourceBuffer past the macro name.
	 *
	 * @return  the name of the macro
	 */
	FString ParseMacroName();

	/**
	 * Parses the macro definition from the Buffer.  SourceBuffer should be positioned at the beginning of the macro definition.
	 *
	 * @param	NewMacroName	the name of macro this definition will be associated with.  currently only used for error message purposes.
	 * @return  the definition for the macro
	 */
	FString ParseMacroDefinition( const TCHAR* NewMacroName);

	/**
	* Retrieves the next parameter from a macro expansion.  It is assumed that we are just past the opening
	* left parenthesis or comma. This function will skip leading whitespace and assemble the parameter until
	* it sees a  top-level (to it) comma or closing parenthesis. It will expand macros as they are encountered.
	*
	* @return  the next parameter for the current macro
	*/
	FString ParseNextParam();

	/**
	 * Retrieves the parameters for the macro currently being parsed.
	 *
	 * @param   Params   [out]	the parameters listed in the text for the call to the macro
	 * 
	 * @return  the number of parameters parsed
	 */
	INT GetParamList(TArray<FString>& Params);


	/**
	* Replaces a macro with the expanded definition for that macro.
	* 
	* @param	macroName	name of the macro to expand
	*/
	void ExpandMacro(const FName& macroName);

	/**
	 * Returns whether output is currently enabled.  For example, inside of `if(false) blocks, output is disabled.
	 *
	 * @return TRUE if output is enabled
	 */
	UBOOL IsOutputEnabled()
	{
		return !EnableOutputFlag.ContainsItem(0);
	}

	/**
	 * Determines whether the specified macro should be processed.
	 *
	 * @param macroName the name of the macro
	 *
	 * @return TRUE if the macro should be processed
	 */
	UBOOL ShouldProcessMacro(const FName& macroName);

	/**
	 * Parses the specified file and adds it to the stack of character providers.
	 *
	 * @param	IncludeFilename		the name of the file to process (i.e. Core\Globals.uci)
	 * @param	FileContent		receives a pointer to the processed version of the file.
	 */
	void ProcessIncludeFile( const FFilename& IncludeFilename, FString* FileContent=NULL );

	/**
	 * Takes an `ifcondition macro condition, and evalutes it to True/False
	 * NOTE: When parenthesis-enclosed values are encountered: "(True && False) && True"
	 *		This function is called recursively with the enclosed-value as input: "True && False"
	 *
	 * @param	Condition		The string representing an `ifcondition expression/condition
	 * @param	InParenNest		Internal use only, keeps track of the nest level of parenthesis-enclosed values
	 * @param	EndPos			Internal use only, returns the location where parsing has stopped
	 * @return				TRUE if the condition evalutes to True, FALSE otherwise
	 */
	UBOOL EvaluteIfCondition(TCHAR* Condition, INT InParenNest=0, TCHAR** EndPos=NULL);

	/** what package are we part of? Important for relative include file Lookup */
	FString PackageName;

	/** the name of the class currently being processed */
	FString ClassName;

	/** number of nested if/endif pairs processing is inside of. Permits nesting */
	INT NestLevel_If;

	/** flag to support conditional compilation (macros can turn emitted characters on and off) */
	TArray<UBOOL> EnableOutputFlag;

	/** the provider of characters: a stack of files, macros, etc. Keeps track of where the next char comes from */
	FCharacterSource SourceBuffer;

	/** Instance symbol table - this holds all the macro definitions available to the current uc file */
	TMap<FName, FScriptMacroDefinition> CurrentSymbols;

	/** Global symbol table - this holds all the macro definitions available to all uc files */
	static TMap<FName,FScriptMacroDefinition>* GlobalSymbols;
	static UBOOL bInitializingGlobalSymbols;

	/** TRUE if this is a shipping package, FALSE if it's a mod package. */
	UBOOL	bIsShippingPackage;

	/** The path for mod script source; either EditPackageInPath or ModPackagesInPath. */
	FString SourcePath;

	/** SourcePath * PackageName. */
	FString PackagePath;

	/** SourcePath * PackageName * "Classes". */
	FString ClassesPath;

	/** counter variable map to implement the Counter and SetCounter commands */
	TMap<FName,INT> ActiveCounters;

	/** Keeps track of last expanded macro */
	FName LastExpandedMacro;
};

#endif  // __UNSCRPRECOM_H__

