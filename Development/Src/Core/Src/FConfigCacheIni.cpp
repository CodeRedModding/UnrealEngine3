/*=============================================================================
	FConfigCacheIni.cpp: Unreal config file reading/writing.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "FConfigCacheIni.h"
#include "AES.h"

/*-----------------------------------------------------------------------------
	FConfigSection
-----------------------------------------------------------------------------*/

UBOOL FConfigSection::HasQuotes( const FString& Test ) const
{
	return Test.Left(1) == TEXT("\"") && Test.Right(1) == TEXT("\"");
}

UBOOL FConfigSection::operator==( const FConfigSection& Other ) const
{
	if ( Pairs.Num() != Other.Pairs.Num() )
		return 0;

	FConfigSectionMap::TConstIterator My(*this), Their(Other);
	while ( My && Their )
	{
		if (My.Key() != Their.Key())
			return 0;

		const FString& MyValue = My.Value(), &TheirValue = Their.Value();
		if ( appStrcmp(*MyValue,*TheirValue) &&
			(!HasQuotes(MyValue) || appStrcmp(*TheirValue,*MyValue.Mid(1,MyValue.Len()-2))) &&
			(!HasQuotes(TheirValue) || appStrcmp(*MyValue,*TheirValue.Mid(1,TheirValue.Len()-2))) )
			return 0;

		++My, ++Their;
	}
	return 1;
}

UBOOL FConfigSection::operator!=( const FConfigSection& Other ) const
{
	return ! (FConfigSection::operator==(Other));
}

/*-----------------------------------------------------------------------------
	FConfigFile
-----------------------------------------------------------------------------*/
FConfigFile::FConfigFile()
: Dirty( FALSE )
, NoSave( FALSE )
, Quotes( FALSE )
{}
	

UBOOL FConfigFile::operator==( const FConfigFile& Other ) const
{
	if ( Pairs.Num() != Other.Pairs.Num() )
		return 0;

	for ( TMap<FString,FConfigSection>::TConstIterator It(*this), OtherIt(Other); It && OtherIt; ++It, ++OtherIt)
	{
		if ( It.Key() != OtherIt.Key() )
			return 0;

		if ( It.Value() != OtherIt.Value() )
			return 0;
	}

	return 1;
}

UBOOL FConfigFile::operator!=( const FConfigFile& Other ) const
{
	return ! (FConfigFile::operator==(Other));
}

UBOOL FConfigFile::Combine(const TCHAR* Filename)
{
	FString Text;
	// note: we don't check if FileOperations are disabled because downloadable content calls this directly (which
	// needs file ops), and the other caller of this is already checking for disabled file ops
	if( appLoadFileToString( Text, Filename ) )
	{
		CombineFromBuffer(Filename,Text);
		return TRUE;
	}

	return FALSE;
}

/**
 * Looks for any overrides on the commandline for this file
 * 
 * @param Filename Name of the .ini file to look for overrides
 */
void FConfigFile::OverrideFromCommandline(const FFilename& Filename)
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	FString Settings;
	// look for this filename on the commandline in the format:
	//		-ini:IniName:Section1.Key1=Value1,Section2.Key2=Value2
	// for example:
	//		-ini:PS3-UDKEngine:Engine.Engine.bSmoothFrameRate=False,TextureStreaming.PoolSize=100
	//
	// This has only been tested setting the generated .ini file (UDKEngine, not DefaultEngine).
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// NOTE: If you use this on PC, it will update your generated .ini file !!!!!
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if (Parse(appCmdLine(), *FString::Printf(TEXT("-ini:%s:"), *Filename.GetBaseFilename()), Settings, FALSE))
	{
		// break apart on the commas
		TArray<FString> SettingPairs;
		Settings.ParseIntoArray(&SettingPairs, TEXT(","), TRUE);
		for (INT Index = 0; Index < SettingPairs.Num(); Index++)
		{
			// set each one, by splitting on the =
			FString SectionAndKey, Value;
			if (SettingPairs(Index).Split(TEXT("="), &SectionAndKey, &Value))
			{
				// now we need to split off the key from the rest of the section name
				INT LastDot = SectionAndKey.InStr(TEXT("."), TRUE);
				// check for malformed string
				if (LastDot == INDEX_NONE || LastDot == 0)
				{
					continue;
				}

				// break it apart
				FString Section = SectionAndKey.Left(LastDot);
				FString Key = SectionAndKey.Mid(LastDot + 1);

				// now put it into this .ini
				SetString(*Section, *Key, *Value);
			}
		}
	}
#endif
}

void FConfigFile::CombineFromBuffer(const TCHAR* Filename,const FString& Buffer)
{
	// Replace %GAME% with game name.
	FString Text = Buffer.Replace( TEXT("%GAME%"), GGameName );

	const FFilename FilenameString(Filename);
	const UBOOL bLocalizationFile = FilenameString.GetExtension() == UObject::GetLanguage() || FilenameString.GetExtension() == TEXT("INT");

	TCHAR* Ptr = const_cast<TCHAR*>( *Text );
	FConfigSection* CurrentSection = NULL;
	UBOOL Done = 0;
	while( !Done )
	{
		// do similar multiline handling to ProcessInputFileContents
		TCHAR* Start = NULL;
		TCHAR* BackSlashAppendPtr = NULL;
		UBOOL bBackSlashAppend = FALSE;
		do 
		{
			// Advance past new line characters
			while( *Ptr=='\r' || *Ptr=='\n' )
				Ptr++;

			// If not appending the current line to the last line
			if( !bBackSlashAppend )
			{
				// Store the location of the first character of this new line
				Start = Ptr;
			}			

			// Advance the char pointer until we hit a newline character
			while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
				Ptr++;

			// If this is the end of the file, we're done
			if( *Ptr==0 )
				Done = 1;

			// Terminate the current line, and advance the pointer to the next character in the stream
			TCHAR* EndOfLineChar = Ptr++;
			*EndOfLineChar = 0;

			// Search backwards in the string to find a backslash
			BackSlashAppendPtr = appStrrchr(Start,'\\');
			bBackSlashAppend = FALSE;

			// If a double backslash \\ exists at the end of the current line
			if( BackSlashAppendPtr && *(BackSlashAppendPtr-1) == '\\' && BackSlashAppendPtr == (Ptr-2) )
			{
				// Set flag that we are appending the next line to current line
				bBackSlashAppend = TRUE;
				// Clear all backslashes, new lines and white space between 1st backslash and next valid text
				TCHAR* ClearPtr = (EndOfLineChar - 2);
				while( *ClearPtr == '\\' || *ClearPtr == '\r' || *ClearPtr == '\n' || *ClearPtr == '\t' || *ClearPtr == 0 )
				{
					*ClearPtr = ' ';
					ClearPtr++;
				}
			}
		} while( bBackSlashAppend );

		// Strip trailing spaces from the current line
		while( *Start && appIsWhitespace(Start[appStrlen(Start)-1]) )
		{
			Start[appStrlen(Start)-1] = 0;
		}

		// If the first character in the line is [ and last char is ], this line indicates a section name
		if( *Start=='[' && Start[appStrlen(Start)-1]==']' )
		{
			// Remove the brackets
			Start++;
			Start[appStrlen(Start)-1] = 0;

			// If we don't have an existing section by this name, add one
			CurrentSection = Find( Start );
			if( !CurrentSection )
			{
				CurrentSection = &Set( Start, FConfigSection() );
			}
		}

		// Otherwise, if we're currently inside a section, and we haven't reached the end of the stream
		else if( CurrentSection && *Start )
		{
			TCHAR* Value = 0;

			// ignore [comment] lines that start with ;
			if(*Start != (TCHAR)';')
			{
				Value = appStrstr(Start,TEXT("="));
			}

			// Ignore any lines that don't contain a key-value pair
			if( Value )
			{
				// Terminate the property name, advancing past the =
				*Value++ = 0;

				// strip leading whitespace from the property name
				while ( *Start && appIsWhitespace(*Start) )
				{						
					Start++;
				}

				// determine how this line will be merged
				TCHAR Cmd = Start[0];
				if ( Cmd=='+' || Cmd=='-' || Cmd=='.' || Cmd=='!' )
				{
					Start++;
				}
				else
				{
					Cmd=' ';
				}

				// Strip trailing spaces from the property name.
				while( *Start && appIsWhitespace(Start[appStrlen(Start)-1]) )
				{
					Start[appStrlen(Start)-1] = 0;
				}

				FString ProcessedValue;

				// Strip leading whitespace from the property value
				while ( *Value && appIsWhitespace(*Value) )
				{
					Value++;
				}

				// strip trailing whitespace from the property value
				while( *Value && appIsWhitespace(Value[appStrlen(Value)-1]) )
				{
					Value[appStrlen(Value)-1] = 0;
				}

				// If this line is delimited by quotes
				if( *Value=='\"' )
				{
					Value++;
					//epic moelfke: fixed handling of escaped characters in quoted string
					while (*Value && *Value != '\"')
					{
						if (*Value != '\\') // unescaped character
						{
							ProcessedValue += *Value++;
						}
						else if (*++Value == '\\') // escaped forward slash "\\"
						{
							ProcessedValue += '\\';
							Value++;
						}
						else if (*Value == '\"') // escaped double quote "\""
						{
							ProcessedValue += '\"';
							Value++;
						}
						else if ( *Value == TEXT('n') )
						{
							ProcessedValue += TEXT('\n');
							Value++;
						}
						else // some other escape sequence, assume it's a hex character value
						{
							ProcessedValue += ParseHexDigit(Value[0])*16 + ParseHexDigit(Value[1]);
							Value += 2;
						}
					}
				}
				else
				{
					if ( bLocalizationFile )
					{
						ProcessedValue = FString(Value).ReplaceEscapedCharWithChar();
					}
					else
					{
						ProcessedValue = Value;
					}
				}

				if( Cmd=='+' ) 
				{
					// Add if not already present.
					CurrentSection->AddUnique( Start, *ProcessedValue );
				}
				else if( Cmd=='-' )	
				{
					// Remove if present.
					CurrentSection->RemovePair( Start, *ProcessedValue );
					CurrentSection->Compact();
				}
				else if ( Cmd=='.' )
				{
					CurrentSection->Add( Start, *ProcessedValue );
				}
				else if( Cmd=='!' )
				{
					CurrentSection->Remove( Start );
				}
				else
				{
					// Add if not present and replace if present.
					FString* Str = CurrentSection->Find( Start );
					if( !Str )
					{
						CurrentSection->Add( Start, *ProcessedValue );
					}
					else
					{
						*Str = ProcessedValue;
					}
				}

				// Mark as dirty so "Write" will actually save the changes.
				Dirty = 1;
			}
		}
	}

	// look for overrides
	OverrideFromCommandline(FilenameString);

	// Avoid memory wasted in array slack.
	Shrink();
	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		It.Value().Shrink();
	}
}

/**
 * Process the contents of an .ini file that has been read into an FString
 * 
 * @param Filename Name of the .ini file the contents came from
 * @param Contents Contents of the .ini file
 */
void FConfigFile::ProcessInputFileContents(const TCHAR* Filename, FString& Contents)
{
	// Replace %GAME% with game name.
	FString Text = Contents.Replace(TEXT("%GAME%"), GGameName);

	const FFilename FilenameString(Filename);
	const UBOOL bLocalizationFile = FilenameString.GetExtension() == UObject::GetLanguage() || FilenameString.GetExtension() == TEXT("INT");
	if ( bLocalizationFile )
	{
		Contents.ReplaceInline(TEXT("%"), TEXT("`"));
	}

	TCHAR* Ptr = Text.Len() > 0 ? const_cast<TCHAR*>( *Text ) : NULL;
	FConfigSection* CurrentSection = NULL;
	UBOOL Done = 0;
	while( !Done && Ptr != NULL )
	{
		TCHAR* Start = NULL;
		TCHAR* BackSlashAppendPtr = NULL;
		UBOOL bBackSlashAppend = FALSE;
		do {
			// Advance past new line characters
			while( *Ptr=='\r' || *Ptr=='\n' )
				Ptr++;

			// If not appending the current line to the last line
			if( !bBackSlashAppend )
			{
				// Store the location of the first character of this new line
				Start = Ptr;
			}			

			// Advance the char pointer until we hit a newline character
			while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
				Ptr++;

			// If this is the end of the file, we're done
			if( *Ptr==0 )
				Done = 1;

			// Terminate the current line, and advance the pointer to the next character in the stream
			TCHAR* EndOfLineChar = Ptr++;
			*EndOfLineChar = 0;

			// Search backwards in the string to find a backslash
			BackSlashAppendPtr = appStrrchr(Start,'\\');
			bBackSlashAppend = FALSE;
			// If a double backslash \\ exists at the end of the current line
			if( BackSlashAppendPtr && *(BackSlashAppendPtr-1) == '\\' && BackSlashAppendPtr == (Ptr-2) )
			{
				// Set flag that we are appending the next line to current line
				bBackSlashAppend = TRUE;
				// Clear all backslashes, new lines and white space between 1st backslash and next valid text
				TCHAR* ClearPtr = (EndOfLineChar - 2);
				while( *ClearPtr == '\\' || *ClearPtr == '\r' || *ClearPtr == '\n' || *ClearPtr == '\t' || *ClearPtr == 0 )
				{
					*ClearPtr = ' ';
					ClearPtr++;
				}
			}
		} while( bBackSlashAppend );

		// Strip trailing spaces from the current line
		while( *Start && appIsWhitespace(Start[appStrlen(Start)-1]) )
		{
			Start[appStrlen(Start)-1] = 0;
		}

		// If the first character in the line is [ and last char is ], this line indicates a section name
		if( *Start=='[' && Start[appStrlen(Start)-1]==']' )
		{
			// Remove the brackets
			Start++;
			Start[appStrlen(Start)-1] = 0;

			// If we don't have an existing section by this name, add one
			CurrentSection = Find( Start );
			if( !CurrentSection )
			{
				CurrentSection = &Set( Start, FConfigSection() );
			}
		}

		// Otherwise, if we're currently inside a section, and we haven't reached the end of the stream
		else if( CurrentSection && *Start )
		{
			TCHAR* Value = 0;

			// ignore [comment] lines that start with ;
			if(*Start != (TCHAR)';')
			{
				Value = appStrstr(Start,TEXT("="));
			}

			// Ignore any lines that don't contain a key-value pair
			if( Value )
			{
				// Terminate the propertyname, advancing past the =
				*Value++ = 0;

				// strip leading whitespace from the property name
				while ( *Start && appIsWhitespace(*Start) )
					Start++;

				// Strip trailing spaces from the property name.
				while( *Start && appIsWhitespace(Start[appStrlen(Start)-1]) )
					Start[appStrlen(Start)-1] = 0;

				// Strip leading whitespace from the property value
				while ( *Value && appIsWhitespace(*Value) )
					Value++;

				// strip trailing whitespace from the property value
				while( *Value && appIsWhitespace(Value[appStrlen(Value)-1]) )
					Value[appStrlen(Value)-1] = 0;

				// If this line is delimited by quotes
				if( *Value=='\"' )
				{
					FString PreprocessedValue = FString(Value).TrimQuotes().ReplaceQuotesWithEscapedQuotes();
					const TCHAR* NewValue = *PreprocessedValue;

					FString ProcessedValue;
					//epic moelfke: fixed handling of escaped characters in quoted string
					while (*NewValue && *NewValue != '\"')
					{
						if (*NewValue != '\\') // unescaped character
						{
							ProcessedValue += *NewValue++;
						}
						else if (*++NewValue == '\\') // escaped backslash "\\"
						{
							ProcessedValue += '\\';
							NewValue++;
						}
						else if (*NewValue == '\"') // escaped double quote "\""
						{
							ProcessedValue += '\"';
							NewValue++;
						}
						else if ( *NewValue == TEXT('n') )
						{
							ProcessedValue += TEXT('\n');
							NewValue++;
						}
						else // some other escape sequence, assume it's a hex character value
						{
							ProcessedValue += ParseHexDigit(NewValue[0])*16 + ParseHexDigit(NewValue[1]);
							NewValue += 2;
						}
					}

					if (!appStricmp(UObject::GetLanguage(), TEXT("XXX"))
					&&	FilenameString.GetExtension() == TEXT("INT"))
					{
						if ( ProcessedValue.Len() > 0 )
						{
							UBOOL bCharEscaped = FALSE;
							for ( INT i = 0; i < ProcessedValue.Len(); i++ )
							{
								if ( bCharEscaped || ProcessedValue[i] == TEXT('\\') )
								{
									bCharEscaped = !bCharEscaped;
								}
								else
								{
									ProcessedValue[i] = TEXT('X');
								}
							}
						}
					}

					// Add this pair to the current FConfigSection
					CurrentSection->Add(Start, *ProcessedValue);
				}
				else
				{
					if (!appStricmp(UObject::GetLanguage(), TEXT("XXX"))
					&&	FilenameString.GetExtension() == TEXT("INT"))
					{
						const INT ValueLength = appStrlen(Value);
						if ( ValueLength > 0 )
						{
							if ( Value[0] == TEXT('(') && Value[ValueLength - 1] == TEXT(')') )
							{
								TCHAR prev=Value[0];
								UBOOL bCharEscaped=FALSE, bInValue=FALSE, bInString=FALSE;
								for ( INT i = 1; i < ValueLength; i++ )
								{
									TCHAR ch = Value[i];
									if ( bCharEscaped || ch == TEXT('\\') )
									{
										bCharEscaped = !bCharEscaped;
									}
									else if ( ch == TEXT('\"') )
									{
										bInString = !bInString;
										if ( !bInString )
										{
											bInValue = FALSE;
										}
										else if ( prev == TEXT('(') || prev == TEXT(',') )
										{
											bInValue = true;
										}
									}
									else if ( !bInString )
									{
										if ( ch == TEXT('(') || ch == TEXT(')') || ch == TEXT(',') )
										{
											bInValue = FALSE;
										}
										else if ( ch == TEXT('=') )
										{
											bInValue = TRUE;
										}
										else if ( bInValue )
										{
											Value[i] = TEXT('X');
										}
									}
									else if ( bInValue )
									{
										Value[i] = TEXT('X');
									}

									prev = ch;
								}
							}
							else
							{
								for ( INT i = 0; i < ValueLength; i++ )
								{
									Value[i] = TEXT('X');
								}
							}
						}
					}

					// Add this pair to the current FConfigSection
					if ( bLocalizationFile )
					{
						CurrentSection->Add(Start, *FString(Value).ReplaceEscapedCharWithChar());
					}
					else
					{
						CurrentSection->Add(Start, Value);
					}
				}
			}
		}
	}

	// look for overrides
	OverrideFromCommandline(FilenameString);

	// Avoid memory wasted in array slack.
	Shrink();
	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		It.Value().Shrink();
	}
}

void FConfigFile::Read( const TCHAR* Filename )
{
	// we can't read in a file if file IO is disabled
	if (!GConfig->AreFileOperationsDisabled())
	{
		Empty();
		FString Text;

		if( appLoadFileToString( Text, Filename, GFileManager, 0 /* LoadFileHash_EnableVerify */ ) )
		{
			// process the contents of the string
			ProcessInputFileContents(Filename, Text);
		}
	}
}

UBOOL FConfigFile::Write( const TCHAR* Filename )
{
	if( !Dirty || NoSave || (ParseParam( appCmdLine(), TEXT("nowrite"))))
		return TRUE;

	FString Text;

	const FFilename FilenameString(Filename);
	const UBOOL bLocalizationFile = FilenameString.GetExtension() == UObject::GetLanguage() || FilenameString.GetExtension() == TEXT("INT");

	if (bLocalizationFile)
	{
		Dirty = false;
		return TRUE;
	}

	for( TIterator It(*this); It; ++It )
	{
		/* The Quotes member is only set when exporting .int from the editor, and the whole concept doesn't work
		very well (when you have a localized INT var, you don't want quotes, even if the FConfigFile's Quotes == 1

		Where we really run into a problem is when a string value begins with a space...since quotes are stripped from the file
		when it's read, these quotes are lost.  String values that begin with a space will be ignored the next time they are imported
		from the game. 
		
		The easiest fix for this is to check if this value begins with a space, and if so, add the quotes so that the value will
		be imported correctly in the future.
		*/


		const FString& SectionName = It.Key();
		const FConfigSection& Section = It.Value();

		{
			Text += FString::Printf( TEXT("[%s]") LINE_TERMINATOR, *SectionName);
			for( FConfigSection::TConstIterator It2(Section); It2; ++It2 )
			{
				const FName PropertyName = It2.Key();
				const FString& PropertyValue = It2.Value();

				TCHAR QuoteString[2] = {0,0};
				if ( Quotes || (**PropertyValue == TEXT(' ')) )
				{
					QuoteString[0] = TEXT('\"');
				}
				Text += FString::Printf( TEXT("%s=%s%s%s") LINE_TERMINATOR, 
					*PropertyName.ToString(), QuoteString, bLocalizationFile ? *PropertyValue.ReplaceCharWithEscapedChar() : *PropertyValue, QuoteString);
			}
			Text += LINE_TERMINATOR;
		}
	}
	// Don't write a file if there's nothing to write (all sections were downloaded).
	UBOOL bResult = Text.Len() > 0 ? appSaveStringToFile( Text, Filename ) : TRUE;
	Dirty = !bResult;

	// only clear dirty flag if write was successful
	return bResult;
}



/** Adds any properties that exist in InSourceFile that this config file is missing */
void FConfigFile::AddMissingProperties( const FConfigFile& InSourceFile )
{
	for( TConstIterator SourceSectionIt( InSourceFile ); SourceSectionIt; ++SourceSectionIt )
	{
		const FString& SourceSectionName = SourceSectionIt.Key();
		const FConfigSection& SourceSection = SourceSectionIt.Value();

		{
			// If we don't already have this section, go ahead and add it now
			FConfigSection* DestSection = Find( SourceSectionName );
			if( DestSection == NULL )
			{
				DestSection = &Set( SourceSectionName, FConfigSection() );
				Dirty = TRUE;
			}

			for( FConfigSection::TConstIterator SourcePropertyIt( SourceSection ); SourcePropertyIt; ++SourcePropertyIt )
			{
				const FName SourcePropertyName = SourcePropertyIt.Key();
				const FString& SourcePropertyValue = SourcePropertyIt.Value();

				// If we don't already have this property, go ahead and add it now
				if( DestSection->Find( SourcePropertyName ) == NULL )
				{
					DestSection->Add( SourcePropertyName, SourcePropertyValue );
					Dirty = TRUE;
				}
			}
		}
	}
}



void FConfigFile::Dump(FOutputDevice& Ar)
{
	Ar.Logf( TEXT("FConfigFile::Dump") );

	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		Ar.Logf( TEXT("[%s]"), *It.Key() );
		TLookupMap<FName> KeyNames;

		FConfigSection& Section = It.Value();
		Section.GetKeys(KeyNames);
		for(TLookupMap<FName>::TConstIterator KeyNameIt(KeyNames);KeyNameIt;++KeyNameIt)
		{
			const FName KeyName = KeyNameIt.Key();

			TArray<FString> Values;
			Section.MultiFind(KeyName,Values,TRUE);

			if ( Values.Num() > 1 )
			{
				for ( INT ValueIndex = 0; ValueIndex < Values.Num(); ValueIndex++ )
				{
					Ar.Logf(TEXT("	%s[%i]=%s"), *KeyName.ToString(), ValueIndex, *Values(ValueIndex).ReplaceCharWithEscapedChar());
				}
			}
			else
			{
				Ar.Logf(TEXT("	%s=%s"), *KeyName.ToString(), *Values(0).ReplaceCharWithEscapedChar());
			}
		}

		Ar.Log( LINE_TERMINATOR );
	}
}

UBOOL FConfigFile::GetString( const TCHAR* Section, const TCHAR* Key, FString& Value )
{
	FConfigSection* Sec = Find( Section );
	if( Sec == NULL )
	{
		return FALSE;
	}
	FString* PairString = Sec->Find( Key );
	if( PairString == NULL )
	{
		return FALSE;
	}
	Value = **PairString;
	return TRUE;
}


UBOOL FConfigFile::GetDouble( const TCHAR* Section, const TCHAR* Key, DOUBLE& Value )
{
	FString Text; 
	if( GetString( Section, Key, Text ) )
	{
		Value = appAtod(*Text);
		return TRUE;
	}
	return FALSE;
}



void FConfigFile::SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value )
{
	FConfigSection* Sec  = Find( Section );
	if( Sec == NULL )
	{
		Sec = &Set( Section, FConfigSection() );
	}

	FString* Str = Sec->Find( Key );
	if( Str == NULL )
	{
		Sec->Add( Key, Value );
		Dirty = 1;
	}
	else if( appStrcmp(**Str,Value)!=0 )
	{
		Dirty = TRUE;
		*Str = Value;
	}
}


void FConfigFile::SetDouble( const TCHAR* Section, const TCHAR* Key, DOUBLE Value )
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	appSprintf( Text, TEXT("%f"), Value );
	SetString( Section, Key, Text );
}

/*-----------------------------------------------------------------------------
	FConfigCacheIni
-----------------------------------------------------------------------------*/

FConfigCacheIni::FConfigCacheIni()
	: bAreFileOperationsDisabled(FALSE)
{
}

FConfigCacheIni::~FConfigCacheIni()
{
	Flush( 1 );
}

FConfigFile* FConfigCacheIni::FindConfigFile( const TCHAR* Filename )
{
	return TMap<FFilename,FConfigFile>::Find( Filename );
}

FConfigFile* FConfigCacheIni::Find( const TCHAR* Filename, UBOOL CreateIfNotFound )
{	
	// Get file.
	FConfigFile* Result = TMap<FFilename,FConfigFile>::Find( Filename );
	// this is || filesize so we load up .int files if file IO is allowed
	if( !Result && !bAreFileOperationsDisabled && (CreateIfNotFound || ( GFileManager->FileSize(Filename) >= 0 ) ) )
	{
		Result = &Set( Filename, FConfigFile() );
		Result->Read( Filename );
		debugf( NAME_DevConfig, TEXT( "GConfig::Find has loaded file:  %s" ), Filename );
	}
	return Result;
}

void FConfigCacheIni::Flush( UBOOL Read, const TCHAR* Filename )
{
	// write out the files if we can
	if (!bAreFileOperationsDisabled)
	{
		for( TIterator It(*this); It; ++It )
			if( !Filename || It.Key()==Filename )
				It.Value().Write( *It.Key() );
	}
	if( Read )
	{
		// we can't read it back in if file operations are disabled
		if (bAreFileOperationsDisabled)
		{
			warnf(NAME_Warning, TEXT("Tried to flush the config cache and read it back in, but File Operations are disabled!!"));
			return;
		}

		if( Filename )
			Remove(Filename);
		else
			Empty();
	}
}

/**
 * Disables any file IO by the config cache system
 */
void FConfigCacheIni::DisableFileOperations()
{
	bAreFileOperationsDisabled = TRUE;
}

/**
 * Re-enables file IO by the config cache system
 */
void FConfigCacheIni::EnableFileOperations()
{
	bAreFileOperationsDisabled = FALSE;
}

/**
 * Returns whether or not file operations are disabled
 */
UBOOL FConfigCacheIni::AreFileOperationsDisabled()
{
	return bAreFileOperationsDisabled;
}

// consoles will never coalesce ini files
#if !CONSOLE

/**
 * Trim the editoronly lines out of the string
 * 
 * @param InputLines Lines of a text file
 * 
 * @todo: Remove after Gears ships!!!
 * This is ugly, but after discussing with a scripting expert (SteveP) it seems like the
 * right thing to do with the current deadline.
 * The proper solution would be to lookup the actor's class, then ask if each field is marked
 * as "editoronly".
 */
static void TrimEditorOnly( FString & InputLines )
{
	INT					Start, End;
	TArray<TCHAR>		StringData;
	TArray<FString>		Lines;

	// Remove linefeeds to make eol detection easier and the text smaller
	InputLines = InputLines.Replace( TEXT( "\r" ), TEXT( "" ) );
	
	// Make sure we aren't trying to access any NULL strings
	if( !InputLines.Len() )
	{
		return;
	}

	// Make an array of lines for easier processing
	Lines.Empty( 2048 );
	StringData = InputLines.GetCharArray();
	Start = 0;
	End = Start;
	while( StringData( End ) )
	{
		End++;
		if( StringData( End - 1 ) == '\n' )
		{
			if( Start != End - 1 )
			{
				Lines.AddItem( InputLines.Mid( Start, End - Start ) );
			}
			Start = End;
		}
	}

	if( Start != End )
	{
		Lines.AddItem( InputLines.Mid( Start, End - Start + 1 ) );
	}

	// Iterate over the array looking for editor only fields
	for( TArray<FString>::TIterator It( Lines ); It; ++It )
	{
		FString & Line = *It;

		if( Line.Len() < 4 )
		{
			continue;
		}

		TCHAR LastChar = Line[Line.Len() - 2];
		if( LastChar == '\"' )
		{
			// Check for a field of "SpokenText" and ending with a "
			if ( Line.InStr( TEXT( "SpokenText=\"" ) ) != INDEX_NONE )
			{
				Line = "";
				continue;
			}

			// Check for a field of "Comment" and ending with a "
			if ( Line.InStr( TEXT( "Comment=\"" ) ) != INDEX_NONE )
			{
				Line = "";
				continue;
			}
		}
		else
		{
			// Check for a field of ".bMature=False".
			if ( Line.InStr( TEXT( "bMature=False" ) ) != INDEX_NONE )
			{
				Line = "";
				continue;
			}

			// Check for a field of ".bManualWordWrap=False".
			if ( Line.InStr( TEXT( "bManualWordWrap=False" ) ) != INDEX_NONE )
			{
				Line = "";
				continue;
			}
		}
	}

	// Recreate one huge string
	InputLines.Empty();
	FString::CullArray( &Lines );

	for( TArray<FString>::TIterator It( Lines ); It; ++It )
	{
		InputLines += *It;
	}
}

/**
 *	Trim the soundnodewave sections out of the string
 *	Since all SubTitle information will be stored in the SoundNodeWave itself
 *	when it is cooked, there is no need to leave it in the coalesced file.
 * 
 *	@param InputLines Lines of a text file
 * 
 */
static void TrimSoundNodeWaveSections( FString & InputLines )
{
	INT					Start, End;
	TArray<TCHAR>		StringData;
	TArray<FString>		Lines;

	// If a line ends in ' SoundNodeWave]' or starts with 'Subtitles[',
	// remove it from the input lines...
	
	// Remove linefeeds to make eol detection easier and the text smaller
	InputLines = InputLines.Replace( TEXT( "\r" ), TEXT( "" ) );
	
	// Make sure we aren't trying to access any NULL strings
	if( !InputLines.Len() )
	{
		return;
	}

	// Make an array of lines for easier processing
	Lines.Empty( 2048 );
	StringData = InputLines.GetCharArray();
	Start = 0;
	End = Start;
	while( StringData( End ) )
	{
		End++;
		if( StringData( End - 1 ) == '\n' )
		{
			if( Start != End - 1 )
			{
				Lines.AddItem( InputLines.Mid( Start, End - Start ) );
			}
			Start = End;
		}
	}

	if( Start != End )
	{
		Lines.AddItem( InputLines.Mid( Start, End - Start + 1 ) );
	}

	// Iterate over the array looking for editor only fields
	for( TArray<FString>::TIterator It( Lines ); It; ++It )
	{
		FString & Line = *It;

		if( Line.Len() < 4 )
		{
			continue;
		}

		//@todo.SAS. We may need a more error-proof way of finding these...
		// From what I have seen, this should be safe w.r.t. our content.
		if (Line.InStr(TEXT(" SoundNodeWave]")) != INDEX_NONE)
		{
			Line = "";
			continue;
		}

		if (Line.InStr(TEXT("Subtitles[")) != INDEX_NONE)
		{
			Line = "";
			continue;
		}
	}

	// Recreate one huge string
	InputLines.Empty();
	FString::CullArray( &Lines );

	for( TArray<FString>::TIterator It( Lines ); It; ++It )
	{
		InputLines += *It;
	}
}

/**
 *	Trim the soundnodewave sections out of the ConfigCache
 *	Since all SubTitle information will be stored in the SoundNodeWave itself
 *	when it is cooked, there is no need to leave it in the coalesced file.
 * 
 *	@param InConfig		The config cache...
 * 
 */
static void TrimSoundNodeWaveSections(FConfigCacheIni& InConfig)
{
	TArray<FFilename> ConfigFilenames;

	InConfig.GetConfigFilenames(ConfigFilenames);
	for (INT FileIndex = 0; FileIndex < ConfigFilenames.Num(); FileIndex++)
	{
		FFilename ConfigFilename = ConfigFilenames(FileIndex);
		InConfig.EmptySectionsMatchingString(TEXT(" SoundNodeWave"), *ConfigFilename);
	}
}

/**
 * Coalesce all files with a certain extension into one file, to reduce the number
 * of file reads at runtime (and no need to worry about properly ordering dozens
 * of files on disc)
 *
 * @param PossibleFiles List of files to search through for the given extension
 * @param Extension Extension to filter what files to coalesce (ie "ini")
 * @param CoalescedFilename Name of the output coalesced file
 * @param bNeedsByteSwapping TRUE if the output file is destined for a platform that expects byte swapped data
 */
static void CoalesceFiles(const TArray<FString>& PossibleFiles, const TCHAR* Extension, const TCHAR* CoalescedFilename, UBOOL bNeedsByteSwapping, UBOOL bTrimEditorOnly,
						  FConfigCacheIni& InConfigCache, UBOOL bSerializeConfigCache, const TCHAR* PlatformString)
{
	TArray<FString> FilenamesAndContents;

	FConfigCacheIni FullConfigCache;
	if (bSerializeConfigCache == TRUE)
	{
		FullConfigCache = InConfigCache;
	}

	// go over all files given
	for (INT FileIndex = 0; FileIndex < PossibleFiles.Num(); FileIndex++)
	{
		FFilename Filename = PossibleFiles(FileIndex);

		// make sure extension matches
		if (Filename.GetExtension() == Extension &&
			Filename.GetBaseFilename() != TEXT("Coalesced"))
		{
			// read in file
			InConfigCache.LoadFile(*Filename, NULL, PlatformString);
			if (bSerializeConfigCache == TRUE)
			{
				FullConfigCache.LoadFile(*Filename, NULL, PlatformString);
			}
		}
	}

	// Only trim out the soundnodewave sections in the shipping cache
	TrimSoundNodeWaveSections(InConfigCache);
	// Serialize the ConfigCache...
	if (bSerializeConfigCache == TRUE)
	{
		// Write the coalesced data to a memory buffer
		TArray<BYTE> IniFileBlob;
		FMemoryWriter* IniFileData = new FMemoryWriter( IniFileBlob );
		IniFileData->SetByteSwapping( bNeedsByteSwapping );
		IniFileData->SetForceUnicode( TRUE );
		FNameAsStringProxyArchive Ar( *IniFileData );
		Ar << InConfigCache;


		// we allow this to not have the AES key, but will print a warning.
		if (appStrcmpANSI(AES_KEY, INVALID_AES_KEY) != 0)
		{
			// Encrypt the memory buffer
			INT PaddedSize = ( IniFileBlob.Num() + AES_BLOCK_SIZE - 1 ) & -AES_BLOCK_SIZE;
			IniFileBlob.AddZeroed( PaddedSize - IniFileBlob.Num() );

			appEncryptData( IniFileBlob.GetData(), IniFileBlob.Num() );
		}
		else
		{
			warnf(TEXT("Saving coalesced .ini file without encryption. It's suggested you set AES_KEY in code to enable encryption."));
		}


		// Write the encrypted data to disk
		FString IniDumpPath = FString( CoalescedFilename ) + TEXT( ".bin" );
		GFileManager->Delete( *IniDumpPath );
		FArchive* CfgFile = GFileManager->CreateFileWriter( *IniDumpPath );
		if( CfgFile )
		{
			CfgFile->Serialize( IniFileBlob.GetData(), IniFileBlob.Num() );
			delete CfgFile;
		}
	}
}
#endif

/**
 * Coalesces .ini and localization files into single files.
 * DOES NOT use the config cache in memory, rather it reads all files from disk,
 * so it would be a static function if it wasn't virtual
 *
 * @param ConfigDir The base directory to search for .ini files
 * @param OutputDirectory The directory to save the output file in
 * @param bNeedsByteSwapping TRUE if the output file is destined for a platform that expects byte swapped data
 * @param IniFileWithFilters Name of ini file to look in for the list of files to filter out
 * @param bCurrentLanguageOnly	If TRUE, only generate the coalesced files for the current INI
 * @param LanguageMask			if non-zero only languages with corresponding bits set will be generated
*/
void FConfigCacheIni::CoalesceFilesFromDisk(const TCHAR* ConfigDir, const TCHAR* OutputDirectory, UBOOL bNeedsByteSwapping, 
	const TCHAR* IniFileWithFilters, const TCHAR* PlatformString, UBOOL bCurrentLanguageOnly,DWORD LanguageMask)
{
// consoles don't need to coalesce
#if !CONSOLE

	// get a list of file names to not coalesce
	FConfigSection* FilteredFilesConfig = GConfig->GetSectionPrivate(TEXT("ConfigCoalesceFilter"), FALSE, TRUE, IniFileWithFilters);

	// get list of generated ini files
	TArray<FString> FilesToCoalesce;
	appFindFilesInDirectory(FilesToCoalesce, ConfigDir, FALSE, TRUE);

	// manually add the ConsoleVariables.ini file
	FilesToCoalesce.AddItem(appEngineDir() + "Config\\ConsoleVariables.ini");

	FConfigCacheIni RootConfigCache;

	// perform any filtering
	if (FilteredFilesConfig)
	{
		for (INT FileIndex = 0; FileIndex < FilesToCoalesce.Num(); FileIndex++)
		{
			// put them into an array (what the function wants)
			for (FConfigSectionMap::TIterator It(*FilteredFilesConfig); It; ++It)
			{
				// if the file was filtered out, then remove it from the array, and fix up the index
				if (It.Value() == FFilename(FilesToCoalesce(FileIndex)).GetCleanFilename())
				{
					FilesToCoalesce.Remove(FileIndex, 1);
					FileIndex--;
					break;
				}
			}
		}
	}

	// coalesce the .ini files
	CoalesceFiles(FilesToCoalesce, TEXT("ini"), *(FString(ConfigDir) * TEXT("Coalesced_ini")), bNeedsByteSwapping, FALSE, RootConfigCache, FALSE, PlatformString);

	// get a list of all language's int files
	FilesToCoalesce.Empty();
	for (INT PathIndex = 0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++)
	{
		appFindFilesInDirectory(FilesToCoalesce, *GSys->LocalizationPaths(PathIndex), FALSE, TRUE);
	}

	// perform any filtering
	if (FilteredFilesConfig)
	{
		for (INT FileIndex = 0; FileIndex < FilesToCoalesce.Num(); FileIndex++)
		{
			// put them into an array (what the function wants)
			for (FConfigSectionMap::TIterator It(*FilteredFilesConfig); It; ++It)
			{
				FFilename FilterFilename(It.Value());

				// if the file was filtered out, then remove it from the array, and fix up the index
				// (also handle swapping language extensions)
				if (FilterFilename.GetExtension() == TEXT("int") && 
					FilterFilename.GetBaseFilename() == FFilename(FilesToCoalesce(FileIndex)).GetBaseFilename())
				{
					FilesToCoalesce.Remove(FileIndex, 1);
					FileIndex--;
					break;
				}
			}
		}
	}

	// coalesce each language separately
	FString CheckExt = UObject::GetLanguage();

	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	for (INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++)
	{
		const FString& Ext = KnownLanguageExtensions(LangIndex);
		if (bCurrentLanguageOnly)
		{
			if ( CheckExt.Len() == 0 || CheckExt != Ext )
			{
				continue;
			}
		}
		else if (LanguageMask && !((1 << LangIndex) & LanguageMask))
		{
			continue;
		}

		// Only include coalesced files for languages that we know are supported by the game.  We'll determine this
		// by checking for an appropriately named language sub-folder in the Localization directory
		FString LanguageFolder = FString::Printf(TEXT("%sLocalization%s%s"), *appGameDir(), PATH_SEPARATOR, *Ext);
		TArray< FString > FoundFiles;
		GFileManager->FindFiles( FoundFiles, *LanguageFolder, FALSE, TRUE );		// Search for directories only
		if( FoundFiles.Num() > 0 )
		{
			FConfigCacheIni LocConfigCache = RootConfigCache;
			FString OutputFile = FString::Printf(TEXT("%s%sCoalesced_%s"), 
				OutputDirectory, PATH_SEPARATOR, *Ext);

			// make sure that the engine's current language matches the language being cooked so that loc files are processed
			// correctly by ProcessInputFileContents()
			UObject::SetLanguage(*Ext, FALSE);

			// it's okay to hardcode the localization directory, because the CookerFrontend assumes it's called Localization
			CoalesceFiles(FilesToCoalesce, *Ext, *OutputFile, bNeedsByteSwapping, TRUE, LocConfigCache, TRUE, PlatformString);
		}
	}
	// restore the engine's language to the original language
	UObject::SetLanguage(*CheckExt, FALSE);
#endif
}

/**
* Reads a coalesced file, breaks it up, and adds the contents to the config cache. Can
* load .ini or locailzation file (see ConfigDir description)
*
* @param ConfigDir If loading ini a file, then this is the path to load from, otherwise if loading a localizaton file, 
*                  this MUST be NULL, and the current language is loaded
*/
void FConfigCacheIni::LoadCoalescedFile(const TCHAR* ConfigDir)
{
	FString CoalescedFilename;
	FString LangExt = appGetLanguageExt();

	debugf( NAME_Init, TEXT( "Language extension: %s" ), *LangExt );

	// if the config dir was specified, load the Coalesced.ini file from there
	if (ConfigDir != NULL)
	{
#if CONSOLE
		if (GUseSeekFreeLoading)
		{
			CoalescedFilename = FString(ConfigDir) + TEXT("Coalesced_") + LangExt + TEXT(".bin");
		}
		else
#endif
		{
			CoalescedFilename = FString(ConfigDir) + "Coalesced.ini";
		}
	}
	else
	{
#if CONSOLE
		// consoles load coalesced files from the cooked directory
		FString	CookedDirName;
		appGetCookedContentPath(appGetPlatformType(), CookedDirName);

		CoalescedFilename = FString::Printf(TEXT("%sCoalesced_%s.bin"), *CookedDirName, *LangExt);
#else
		// otherwise, load the coalesced localization file from the base game's localization directory
		{
			CoalescedFilename = FString::Printf(TEXT("%sLocalization%sCoalesced.%s"), *appGameDir(), PATH_SEPARATOR, UObject::GetLanguage());
		}
#endif
		// Does the requested coalesced ini file exist?
		// This is to workaround the fact that FileSize() does not handle EDAT files. Ideally, it should. CreateFileReader() relies on the fact that FileSize() does not to handle it itself.
		const UBOOL bCoalescedFileExists = ( GFileManager->CreateFileReader( *CoalescedFilename ) != NULL );

		// Revert to the default (English) coalesced ini if the language-localized version cannot be found.
		if ( !bCoalescedFileExists || ParseParam( appCmdLine(), TEXT("ENGLISHCOALESCED") ) )
		{
#if CONSOLE
			CoalescedFilename = FString::Printf(TEXT("%sCoalesced_%s.bin"), *CookedDirName, TEXT("INT"));
#else
			CoalescedFilename = FString::Printf( TEXT("%sLocalization%sCoalesced.%s"), *appGameDir(), PATH_SEPARATOR, TEXT("INT") );
#endif
		}
	}

	// read in all of the contents of the file
	FArchive* InputFile = GFileManager->CreateFileReader(*CoalescedFilename);
	checkf(InputFile != NULL, TEXT("Failed to find required coalesced ini file %s"), *CoalescedFilename);

	INT FileSize = InputFile->TotalSize();
	// this buffer will be passed to the SHA verify task, so we don't free it here
	void* Contents = appMalloc(FileSize);
	// read the whole file into a buffer
	InputFile->Serialize(Contents, FileSize);
	// close file reader
	delete InputFile;
	InputFile = NULL;

	// we allow this to not have the AES key - it's expected that saving and loading has the same AES_KEY
	if (appStrcmpANSI(AES_KEY, INVALID_AES_KEY) != 0)
	{
		// Decrypt the data in place
		appDecryptData( ( BYTE* )Contents, FileSize );
	}

	// read the strings from the memory block to an array
	FBufferReaderWithSHA InnerAr(Contents, FileSize, TRUE, *CoalescedFilename);
	FNameAsStringProxyArchive Ar(InnerAr);
#if CONSOLE
	Ar << *this;
	//Dump(*GLog);
#else
	TArray<FString> FilenamesAndContents;
	Ar << FilenamesAndContents;

	// process the coalesced contents
	for (INT FileIndex = 0; FileIndex < FilenamesAndContents.Num(); FileIndex += 2)
	{
		// get the two strings
		FFilename Filename = FilenamesAndContents(FileIndex + 0);
		FString& CoalescedContents = FilenamesAndContents(FileIndex + 1);

		// remember the config file as being in the game config dir
		if (ConfigDir)
		{
			Filename = appGameConfigDir() + Filename.GetCleanFilename();
		}

		// make a new config file for this input file
		FConfigFile* Result = &Set(*Filename, FConfigFile());

		// process it
		Result->ProcessInputFileContents(*Filename, CoalescedContents);
	}
#endif

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// loop over all loaded files, looking for overrides
	for (TMap<FFilename,FConfigFile>::TIterator It(*this); It; ++It)
	{
		It.Value().OverrideFromCommandline(It.Key());
	}
#endif
}

/**
 * Prases apart an ini section that contains a list of 1-to-N mappings of names in the following format
 *	 [PerMapPackages]
 *	 MapName=Map1
 *	 Package=PackageA
 *	 Package=PackageB
 *	 MapName=Map2
 *	 Package=PackageC
 *	 Package=PackageD
 * 
 * @param Section Name of section to look in
 * @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
 * @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
 * @param OutMap Map containing parsed results
 * @param Filename Filename to use to find the section
 *
 * NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
 */
void FConfigCacheIni::Parse1ToNSectionOfNames(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FName, TArray<FName> >& OutMap, const TCHAR* Filename)
{
	// find the config file object
	FConfigFile* ConfigFile = Find(Filename, 0);
	if (!ConfigFile)
	{
		return;
	}

	// find the section in the file
	FConfigSectionMap* ConfigSection = ConfigFile->Find(Section);
	if (!ConfigSection)
	{
		return;
	}

	TArray<FName>* WorkingList = NULL;
	for( FConfigSectionMap::TIterator It(*ConfigSection); It; ++It )
	{
		// is the current key the 1 key?
		if (It.Key() == KeyOne)
		{
			FName KeyName(*It.Value());

			// look for existing set in the map
			WorkingList = OutMap.Find(KeyName);

			// make a new one if it wasn't there
			if (WorkingList == NULL)
			{
				WorkingList = &OutMap.Set(KeyName, TArray<FName>());
			}
		}
		// is the current key the N key?
		else if (It.Key() == KeyN && WorkingList != NULL)
		{
			// if so, add it to the N list for the current 1 key
			WorkingList->AddItem(FName(*It.Value()));
		}
		// if it's neither, then reset
		else
		{
			WorkingList = NULL;
		}
	}
}

/**
 * Prases apart an ini section that contains a list of 1-to-N mappings of strings in the following format
 *	 [PerMapPackages]
 *	 MapName=Map1
 *	 Package=PackageA
 *	 Package=PackageB
 *	 MapName=Map2
 *	 Package=PackageC
 *	 Package=PackageD
 * 
 * @param Section Name of section to look in
 * @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
 * @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
 * @param OutMap Map containing parsed results
 * @param Filename Filename to use to find the section
 *
 * NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
 */
void FConfigCacheIni::Parse1ToNSectionOfStrings(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FString, TArray<FString> >& OutMap, const TCHAR* Filename)
{
	// find the config file object
	FConfigFile* ConfigFile = Find(Filename, 0);
	if (!ConfigFile)
	{
		return;
	}

	// find the section in the file
	FConfigSectionMap* ConfigSection = ConfigFile->Find(Section);
	if (!ConfigSection)
	{
		return;
	}

	TArray<FString>* WorkingList = NULL;
	for( FConfigSectionMap::TIterator It(*ConfigSection); It; ++It )
	{
		// is the current key the 1 key?
		if (It.Key() == KeyOne)
		{
			// look for existing set in the map
			WorkingList = OutMap.Find(*It.Value());

			// make a new one if it wasn't there
			if (WorkingList == NULL)
			{
				WorkingList = &OutMap.Set(*It.Value(), TArray<FString>());
			}
		}
		// is the current key the N key?
		else if (It.Key() == KeyN && WorkingList != NULL)
		{
			// if so, add it to the N list for the current 1 key
			WorkingList->AddItem(It.Value());
		}
		// if it's neither, then reset
		else
		{
			WorkingList = NULL;
		}
	}
}

void FConfigCacheIni::LoadFile( const TCHAR* InFilename, const FConfigFile* Fallback, const TCHAR* PlatformString )
{
	FFilename Filename( InFilename );

	// if the file has some data in it, read it in
	if( GFileManager->FileSize(*Filename) >= 0 )
	{
		FString StoreFilename(InFilename);
		if (GIsCooking && (PlatformString != NULL))
		{
			FString RemoveString = FString(PlatformString) + PATH_SEPARATOR + FString(TEXT("Cooked")) + PATH_SEPARATOR;
			INT Index = StoreFilename.InStr(RemoveString);
			if (Index != -1)
			{
				StoreFilename.ReplaceInline(*RemoveString, TEXT(""));
			}
		}
		FConfigFile* Result = &Set( *StoreFilename, FConfigFile() );
		Result->Read( *Filename );
		debugfSuppressed( NAME_DevConfig, TEXT( "GConfig::LoadFile has loaded file:  %s" ), InFilename );
	}
	else if( Fallback )
	{
		Set( *Filename, *Fallback );
		debugf( NAME_DevConfig, TEXT( "GConfig::LoadFile associated file:  %s" ), InFilename );
	}
	else
	{
		warnf( NAME_DevConfig, TEXT( "FConfigCacheIni::LoadFile failed loading file as it was 0 size.  Filename was:  %s" ), InFilename );
	}

	// Avoid memory wasted in array slack.
	Shrink();
}


void FConfigCacheIni::SetFile( const TCHAR* InFilename, const FConfigFile* NewConfigFile )
{
	FFilename Filename( InFilename );

	Set( *Filename, *NewConfigFile );
}


void FConfigCacheIni::UnloadFile( const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, 0 );
	if( File )
		Remove( Filename );
}

void FConfigCacheIni::Detach( const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, 1 );
	if( File )
		File->NoSave = 1;
}

UBOOL FConfigCacheIni::GetString( const TCHAR* Section, const TCHAR* Key, FString& Value, const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, 0 );
	if( !File )
	{
		return FALSE;
	}
	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
	{
		return FALSE;
	}
	FString* PairString = Sec->Find( Key );
	if( !PairString )
	{
		return FALSE;
	}
	Value = **PairString;
	return TRUE;
}

UBOOL FConfigCacheIni::GetSection( const TCHAR* Section, TArray<FString>& Result, const TCHAR* Filename )
{
	Result.Empty();
	FConfigFile* File = Find( Filename, 0 );
	if( !File )
		return 0;
	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
		return 0;
	for( FConfigSection::TIterator It(*Sec); It; ++It )
		new(Result) FString(FString::Printf( TEXT("%s=%s"), *It.Key().ToString(), *It.Value() ));
	return 1;
}

FConfigSection* FConfigCacheIni::GetSectionPrivate( const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, Force );
	if( !File )
		return NULL;
	FConfigSection* Sec = File->Find( Section );
	if( !Sec && Force )
		Sec = &File->Set( Section, FConfigSection() );
	if( Sec && (Force || !Const) )
		File->Dirty = 1;
	return Sec;
}

void FConfigCacheIni::SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, 1 );

	if ( !File )
	{
		return;
	}

	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
		Sec = &File->Set( Section, FConfigSection() );
	FString* Str = Sec->Find( Key );
	if( !Str )
	{
		Sec->Add( Key, Value );
		File->Dirty = 1;
	}
	else if( appStricmp(**Str,Value)!=0 )
	{
		File->Dirty = (appStrcmp(**Str,Value)!=0);
		*Str = Value;
	}
}

void FConfigCacheIni::EmptySection( const TCHAR* Section, const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, 0 );
	if( File )
	{
		FConfigSection* Sec = File->Find( Section );
		// remove the section name if there are no more properties for this section
		if( Sec )
		{
			if ( FConfigSection::TIterator(*Sec) )
				Sec->Empty();

			File->Remove(Section);
			if (bAreFileOperationsDisabled == FALSE)
			{
				if (File->Num())
				{
					File->Dirty = 1;
					Flush(0, Filename);
				}
				else
				{
					GFileManager->Delete(Filename);	
				}
			}
		}
	}
}

void FConfigCacheIni::EmptySectionsMatchingString( const TCHAR* SectionString, const TCHAR* Filename )
{
	FConfigFile* File = Find( Filename, 0 );
	if (File)
	{
		UBOOL bSaveOpsDisabled = bAreFileOperationsDisabled;
		bAreFileOperationsDisabled = TRUE;
		for (FConfigFile::TIterator It(*File); It; ++It)
		{
			if (It.Key().InStr(SectionString) != INDEX_NONE)
			{
				EmptySection(*(It.Key()), Filename);
			}
		}
		bAreFileOperationsDisabled = bSaveOpsDisabled;
	}
}

/**
 * Retrieve a list of all of the config files stored in the cache
 *
 * @param ConfigFilenames Out array to receive the list of filenames
 */
void FConfigCacheIni::GetConfigFilenames(TArray<FFilename>& ConfigFilenames)
{
	// copy from our map to the array
	for (FConfigCacheIni::TIterator It(*this); It; ++It)
	{
		ConfigFilenames.AddItem(*(It.Key()));
	}
}

/**
 * Retrieve the names for all sections contained in the file specified by Filename
 *
 * @param	Filename			the file to retrieve section names from
 * @param	out_SectionNames	will receive the list of section names
 *
 * @return	TRUE if the file specified was successfully found;
 */
UBOOL FConfigCacheIni::GetSectionNames( const TCHAR* Filename, TArray<FString>& out_SectionNames )
{
	UBOOL bResult = FALSE;

	FConfigFile* File = Find(Filename, FALSE);
	if ( File != NULL )
	{
		out_SectionNames.Empty(Num());
		for ( FConfigFile::TIterator It(*File); It; ++It )
		{
			// insert each item at the beginning of the array because TIterators return results in reverse order from which they were added
			out_SectionNames.InsertItem(It.Key(),0);
		}
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Retrieve the names of sections which contain data for the specified PerObjectConfig class.
 *
 * @param	Filename			the file to retrieve section names from
 * @param	SearchClass			the name of the PerObjectConfig class to retrieve sections for.
 * @param	out_SectionNames	will receive the list of section names that correspond to PerObjectConfig sections of the specified class
 * @param	MaxResults			the maximum number of section names to retrieve
 *
 * @return	TRUE if the file specified was found and it contained at least 1 section for the specified class
 */
UBOOL FConfigCacheIni::GetPerObjectConfigSections( const TCHAR* Filename, const FString& SearchClass, TArray<FString>& out_SectionNames, INT MaxResults )
{
	UBOOL bResult = FALSE;

	MaxResults = Max(0, MaxResults);
	FConfigFile* File = Find(Filename, FALSE);
	if ( File != NULL )
	{
		out_SectionNames.Empty();
		for ( FConfigFile::TIterator It(*File); It && out_SectionNames.Num() < MaxResults; ++It )
		{
			const FString& SectionName = It.Key();
			
			// determine whether this section corresponds to a PerObjectConfig section
			INT POCClassDelimiter = SectionName.InStr(TEXT(" "));
			if ( POCClassDelimiter != INDEX_NONE )
			{
				// the section name contained a space, which for now we'll assume means that we've found a PerObjectConfig section
				// see if the remainder of the section name matches the class name we're searching for
				if ( SectionName.Mid(POCClassDelimiter + 1) == SearchClass )
				{
					// found a PerObjectConfig section for the class specified - add it to the list
					out_SectionNames.InsertItem(SectionName,0);
					bResult = TRUE;
				}
			}
		}
	}

	return bResult;
}

void FConfigCacheIni::Exit()
{
	Flush( 1 );
}
void FConfigCacheIni::Dump( FOutputDevice& Ar )
{
	Ar.Log( TEXT("Files map:") );
	TMap<FFilename,FConfigFile>::Dump( Ar );

	for ( TIterator It(*this); It; ++It )
	{
		Ar.Logf(TEXT("FileName: %s"), *It.Key());
		FConfigFile& File = It.Value();
		for ( FConfigFile::TIterator FileIt(File); FileIt; ++FileIt )
		{
			FConfigSection& Sec = FileIt.Value();
			Ar.Logf(TEXT("   [%s]"), *FileIt.Key());
			for ( FConfigSectionMap::TConstIterator SecIt(Sec); SecIt; ++SecIt )
				Ar.Logf(TEXT("   %s=%s"), *SecIt.Key().ToString(), *SecIt.Value());

			Ar.Log(LINE_TERMINATOR);
		}
	}
}

// Derived functions.
FString FConfigCacheIni::GetStr( const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename )
{
	FString Result;
	GetString( Section, Key, Result, Filename );
	return Result;
}
UBOOL FConfigCacheIni::GetInt
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	INT&			Value,
	const TCHAR*	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = appAtoi(*Text);
		return 1;
	}
	return 0;
}
UBOOL FConfigCacheIni::GetFloat
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	FLOAT&			Value,
	const TCHAR*	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = appAtof(*Text);
		return 1;
	}
	return 0;
}
UBOOL FConfigCacheIni::GetDouble
	(
	const TCHAR*	Section,
	const TCHAR*	Key,
	DOUBLE&			Value,
	const TCHAR*	Filename
	)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = appAtod(*Text);
		return 1;
	}
	return 0;
}


UBOOL FConfigCacheIni::GetBool
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	UBOOL&			Value,
	const TCHAR*	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value
		=		!appStricmp(*Text,TEXT("On"))
			||	!appStricmp(*Text,TEXT("True"))
			||	!appStricmp(*Text,TEXT("Yes"))
			||	!appStricmp(*Text,GYes)
			||	!appStricmp(*Text,GTrue)
			||	!appStricmp(*Text,TEXT("1"));
		return 1;
	}
	return 0;
}
INT FConfigCacheIni::GetArray
(
	const TCHAR* Section,
	const TCHAR* Key,
	TArray<FString>& out_Arr,
	const TCHAR* Filename/* =NULL  */)
{
	out_Arr.Empty();
	FConfigFile* File = Find( Filename, 0 );
	if ( File != NULL )
	{
		FConfigSection* Sec = File->Find( Section );
		if ( Sec != NULL )
		{
			TArray<FString> RemapArray;
			Sec->MultiFind(Key, RemapArray);

			// TMultiMap::MultiFind will return the results in reverse order
			out_Arr.AddZeroed(RemapArray.Num());
			for ( INT RemapIndex = RemapArray.Num() - 1, Index = 0; RemapIndex >= 0; RemapIndex--, Index++ )
			{
				out_Arr(Index) = RemapArray(RemapIndex);
			}
		}
	}

	return out_Arr.Num();
}
/** Loads a "delimited" list of string
 * @param Section - Section of the ini file to load from
 * @param Key - The key in the section of the ini file to load
 * @param out_Arr - Array to load into
 * @param Delimiter - Break in the strings
 * @param Filename - Ini file to load from
 */
INT FConfigCacheIni::GetSingleLineArray
(
	const TCHAR* Section,
	const TCHAR* Key,
	TArray<FString>& out_Arr,
	const TCHAR* Filename
)
{
	FString FullString;
	UBOOL bValueExisted = GetString(Section, Key, FullString, Filename);
	const TCHAR* RawString = *FullString;

	//tokenize the string into out_Arr
	FString NextToken;
	while ( ParseToken(RawString, NextToken, FALSE) )
	{
		new(out_Arr) FString(NextToken);
	}
	return bValueExisted;
}

UBOOL FConfigCacheIni::GetColor
(
 const TCHAR*	Section,
 const TCHAR*	Key,
 FColor&		Value,
 const TCHAR*	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return FALSE;
}

UBOOL FConfigCacheIni::GetVector
(
 const TCHAR*	Section,
 const TCHAR*	Key,
 FVector&		Value,
 const TCHAR*	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return FALSE;
}

UBOOL FConfigCacheIni::GetRotator
(
 const TCHAR*	Section,
 const TCHAR*	Key,
 FRotator&		Value,
 const TCHAR*	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return FALSE;
}

void FConfigCacheIni::SetInt
(
	const TCHAR* Section,
	const TCHAR* Key,
	INT			 Value,
	const TCHAR* Filename
)
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	appSprintf( Text, TEXT("%i"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetFloat
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	FLOAT			Value,
	const TCHAR*	Filename
)
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	appSprintf( Text, TEXT("%f"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetDouble
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	DOUBLE			Value,
	const TCHAR*	Filename
)
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	appSprintf( Text, TEXT("%f"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetBool
(
	const TCHAR* Section,
	const TCHAR* Key,
	UBOOL		 Value,
	const TCHAR* Filename
)
{
	SetString( Section, Key, Value ? TEXT("True") : TEXT("False"), Filename );
}

void FConfigCacheIni::SetArray
(
	const TCHAR* Section,
	const TCHAR* Key,
	const TArray<FString>& Value,
	const TCHAR* Filename /* = NULL  */
)
{
	FConfigFile* File = Find( Filename, 1 );
	FConfigSection* Sec  = File->Find( Section );
	if( !Sec )
		Sec = &File->Set( Section, FConfigSection() );

	if ( Sec->Remove(Key) > 0 )
		File->Dirty = 1;

	for ( INT i = 0; i < Value.Num(); i++ )
	{
		Sec->Add(Key, *Value(i));
		File->Dirty = 1;
	}
}
/** Saves a "delimited" list of strings
 * @param Section - Section of the ini file to save to
 * @param Key - The key in the section of the ini file to save
 * @param In_Arr - Array to save from
 * @param Filename - Ini file to save to
 */
void FConfigCacheIni::SetSingleLineArray
(
	const TCHAR* Section,
	const TCHAR* Key,
	const TArray<FString>& In_Arr,
	const TCHAR* Filename
)
{
	FString FullString;

	//append all strings to single string
	for (INT i = 0; i < In_Arr.Num(); ++i)
	{
		FullString += In_Arr(i);
		FullString += TEXT(" ");
	}

	//save to ini file
	SetString(Section, Key, *FullString, Filename);
}

void FConfigCacheIni::SetColor
(
 const TCHAR* Section,
 const TCHAR* Key,
 const FColor Value,
 const TCHAR* Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

void FConfigCacheIni::SetVector
(
 const TCHAR* Section,
 const TCHAR* Key,
 const FVector Value,
 const TCHAR* Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

void FConfigCacheIni::SetRotator
(
 const TCHAR* Section,
 const TCHAR* Key,
 const FRotator Value,
 const TCHAR* Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

// Static allocator.
FConfigCacheIni* FConfigCacheIni::Factory()
{
	return ::new FConfigCacheIni();
}


/**
 * Archive for counting config file memory usage.
 */
class FArchiveCountConfigMem : public FArchive
{
public:
	FArchiveCountConfigMem()
	:	Num(0)
	,	Max(0)
	{
		ArIsCountingMemory = TRUE;
	}
	SIZE_T GetNum()
	{
		return Num;
	}
	SIZE_T GetMax()
	{
		return Max;
	}
	void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		Num += InNum;
		Max += InMax;
	}
protected:
	SIZE_T Num, Max;
};


/**
 * Tracks the amount of memory used by a single config or loc file
 */
struct FConfigFileMemoryData
{
	FFilename	ConfigFilename;
	SIZE_T		CurrentSize;
	SIZE_T		MaxSize;

	FConfigFileMemoryData( const FFilename& InFilename, SIZE_T InSize, SIZE_T InMax )
	: ConfigFilename(InFilename), CurrentSize(InSize), MaxSize(InMax)
	{}
};

IMPLEMENT_COMPARE_CONSTREF(FConfigFileMemoryData,FConfigCacheIni,
{
	INT Result = B.CurrentSize - A.CurrentSize;
	if ( Result == 0 )
	{
		Result = B.MaxSize - A.MaxSize;
	}

	return Result;
})

/**
 * Tracks the memory data recorded for all loaded config files.
 */
struct FConfigMemoryData
{
	INT NameIndent;
	INT SizeIndent;
	INT MaxSizeIndent;

	TArray<FConfigFileMemoryData> MemoryData;

	FConfigMemoryData()
	: NameIndent(0), SizeIndent(0), MaxSizeIndent(0)
	{}

	void AddConfigFile( const FFilename& ConfigFilename, FArchiveCountConfigMem& MemAr )
	{
		SIZE_T TotalMem = MemAr.GetNum();
		SIZE_T MaxMem = MemAr.GetMax();

		NameIndent = Max(NameIndent, ConfigFilename.Len());
		SizeIndent = Max(SizeIndent, appItoa(TotalMem).Len());
		MaxSizeIndent = Max(MaxSizeIndent, appItoa(MaxMem).Len());
		
		new(MemoryData) FConfigFileMemoryData( ConfigFilename, TotalMem, MaxMem );
	}

	void SortBySize()
	{
		Sort<USE_COMPARE_CONSTREF(FConfigFileMemoryData,FConfigCacheIni)>( &MemoryData(0), MemoryData.Num() );
	}
};

/**
 * Dumps memory stats for each file in the config cache to the specified archive.
 *
 * @param	Ar	the output device to dump the results to
 */
void FConfigCacheIni::ShowMemoryUsage( FOutputDevice& Ar )
{
	FConfigMemoryData ConfigCacheMemoryData;

	for ( TIterator FileIt(*this); FileIt; ++FileIt )
	{
		FFilename Filename = FileIt.Key();
		FConfigFile& ConfigFile = FileIt.Value();

		FArchiveCountConfigMem MemAr;

		// count the bytes used for storing the filename
		MemAr << Filename;

		// count the bytes used for storing the array of SectionName->Section pairs
		MemAr << ConfigFile;
		
		ConfigCacheMemoryData.AddConfigFile(Filename, MemAr);
	}

	// add a little extra spacing between the columns
	ConfigCacheMemoryData.SizeIndent += 10;
	ConfigCacheMemoryData.MaxSizeIndent += 10;

	// record the memory used by the FConfigCacheIni's TMap
	FArchiveCountConfigMem MemAr;
	CountBytes(MemAr);

	SIZE_T TotalMemoryUsage=MemAr.GetNum();
	SIZE_T MaxMemoryUsage=MemAr.GetMax();

	Ar.Log(TEXT("Config cache memory usage:"));
	// print out the header
#if USE_LS_SPEC_FOR_UNICODE
	Ar.Logf(TEXT("%*ls %*ls %*ls"), ConfigCacheMemoryData.NameIndent, TEXT("FileName"), ConfigCacheMemoryData.SizeIndent, TEXT("NumBytes"), ConfigCacheMemoryData.MaxSizeIndent, TEXT("MaxBytes"));
#else
	Ar.Logf(TEXT("%*s %*s %*s"), ConfigCacheMemoryData.NameIndent, TEXT("FileName"), ConfigCacheMemoryData.SizeIndent, TEXT("NumBytes"), ConfigCacheMemoryData.MaxSizeIndent, TEXT("MaxBytes"));
#endif

	ConfigCacheMemoryData.SortBySize();
	for ( INT Index = 0; Index < ConfigCacheMemoryData.MemoryData.Num(); Index++ )
	{
		FConfigFileMemoryData& ConfigFileMemoryData = ConfigCacheMemoryData.MemoryData(Index);
#if USE_LS_SPEC_FOR_UNICODE
		Ar.Logf(TEXT("%*ls %*u %*u"),
#else
		Ar.Logf(TEXT("%*s %*u %*u"), 
#endif
			ConfigCacheMemoryData.NameIndent, *ConfigFileMemoryData.ConfigFilename,
			ConfigCacheMemoryData.SizeIndent, ConfigFileMemoryData.CurrentSize,
			ConfigCacheMemoryData.MaxSizeIndent, ConfigFileMemoryData.MaxSize);

		TotalMemoryUsage += ConfigFileMemoryData.CurrentSize;
		MaxMemoryUsage += ConfigFileMemoryData.MaxSize;
	}

#if USE_LS_SPEC_FOR_UNICODE
	Ar.Logf(TEXT("%*ls %*u %*u"),
#else
	Ar.Logf(TEXT("%*s %*u %*u"), 
#endif
		ConfigCacheMemoryData.NameIndent, TEXT("Total"),
		ConfigCacheMemoryData.SizeIndent, TotalMemoryUsage,
		ConfigCacheMemoryData.MaxSizeIndent, MaxMemoryUsage);
}



SIZE_T FConfigCacheIni::GetMaxMemoryUsage()
{
	// record the memory used by the FConfigCacheIni's TMap
	FArchiveCountConfigMem MemAr;
	CountBytes(MemAr);

	SIZE_T TotalMemoryUsage=MemAr.GetNum();
	SIZE_T MaxMemoryUsage=MemAr.GetMax();


	FConfigMemoryData ConfigCacheMemoryData;

	for ( TIterator FileIt(*this); FileIt; ++FileIt )
	{
		FFilename Filename = FileIt.Key();
		FConfigFile& ConfigFile = FileIt.Value();

		FArchiveCountConfigMem MemAr;

		// count the bytes used for storing the filename
		MemAr << Filename;

		// count the bytes used for storing the array of SectionName->Section pairs
		MemAr << ConfigFile;

		ConfigCacheMemoryData.AddConfigFile(Filename, MemAr);
	}

	for ( INT Index = 0; Index < ConfigCacheMemoryData.MemoryData.Num(); Index++ )
	{
		FConfigFileMemoryData& ConfigFileMemoryData = ConfigCacheMemoryData.MemoryData(Index);

		TotalMemoryUsage += ConfigFileMemoryData.CurrentSize;
		MaxMemoryUsage += ConfigFileMemoryData.MaxSize;
	}

	return MaxMemoryUsage;
}

