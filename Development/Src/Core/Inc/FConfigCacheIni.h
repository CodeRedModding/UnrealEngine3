/*=============================================================================
	FConfigCacheIni.h: Unreal config file reading/writing.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	Config cache.
-----------------------------------------------------------------------------*/

// One section in a config file.
class FConfigSection : public TMultiMap<FString,FString>
{};

// One config file.
class FConfigFile : public TMap<FString,FConfigSection>
{
public:
	UBOOL Dirty, NoSave, Quotes;
	
	FConfigFile()
	: Dirty( 0 )
	, NoSave( 0 )
	, Quotes( 0 )
	{}
	
	void Combine( const TCHAR* Filename)
	{
		FString Text;
		if( appLoadFileToString( Text, Filename ) )
		{
			// Replace %GAME% with game name.
			Text = Text.Replace( TEXT("%GAME%"), GGameName );

			TCHAR* Ptr = const_cast<TCHAR*>( *Text );
			FConfigSection* CurrentSection = NULL;
			UBOOL Done = 0;
			while( !Done )
			{
				while( *Ptr=='\r' || *Ptr=='\n' )
					Ptr++;
				TCHAR* Start = Ptr;
				while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
					Ptr++;				
				if( *Ptr==0 )
					Done = 1;
				*Ptr++ = 0;
				if( *Start=='[' && Start[appStrlen(Start)-1]==']' )
				{
					Start++;
					Start[appStrlen(Start)-1] = 0;
					CurrentSection = Find( Start );
					if( !CurrentSection )
						CurrentSection = &Set( Start, FConfigSection() );
				}
				else if( CurrentSection && *Start )
				{
					TCHAR* Value = appStrstr(Start,TEXT("="));
					if( Value )
					{
						TCHAR Cmd = Start[0];
						if ( Cmd=='+' || Cmd=='-' || Cmd=='.' )
							Start++;
						else
							Cmd=' ';

						*Value++ = 0;

						// Strip trailing spaces.
						while( *Value && Value[appStrlen(Value)-1]==' ' )
							Value[appStrlen(Value)-1] = 0;

						// Decode quotes if they're present.
						if( *Value=='\"' && Value[appStrlen(Value)-1]=='\"' )
						{
							Value++;
							Value[appStrlen(Value)-1]=0;
						}

						if( Cmd=='+' ) 
						{
							// Add if not already present.
							CurrentSection->AddUnique( Start, Value );
						}
						else if( Cmd=='-' )	
						{
							// Remove if present.
							CurrentSection->RemovePair( Start, Value );
						}
						else if ( Cmd=='.' )
						{
							CurrentSection->Add( Start, Value );
						}
						else
						{
							// Add if not present and replace if present.
							FString* Str = CurrentSection->Find( Start );
							if( !Str )
								CurrentSection->Add( Start, Value );
							else
								*Str = Value;
						}

						// Mark as dirty so "Write" will actually save the changes.
						Dirty = 1;
					}
				}
			}
		}
	}
	void Read( const TCHAR* Filename )
	{
		Empty();
		FString Text;
		if( appLoadFileToString( Text, Filename ) )
		{
			// Replace %GAME% with game name.
			Text = Text.Replace( TEXT("%GAME%"), GGameName );

			TCHAR* Ptr = const_cast<TCHAR*>( *Text );
			FConfigSection* CurrentSection = NULL;
			UBOOL Done = 0;
			while( !Done )
			{
				while( *Ptr=='\r' || *Ptr=='\n' )
					Ptr++;
				TCHAR* Start = Ptr;
				while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
					Ptr++;				
				if( *Ptr==0 )
					Done = 1;
				*Ptr++ = 0;
				if( *Start=='[' && Start[appStrlen(Start)-1]==']' )
				{
					Start++;
					Start[appStrlen(Start)-1] = 0;
					CurrentSection = Find( Start );
					if( !CurrentSection )
						CurrentSection = &Set( Start, FConfigSection() );
				}
				else if( CurrentSection && *Start )
				{
					TCHAR* Value = appStrstr(Start,TEXT("="));
					if( Value )
					{
						*Value++ = 0;
						if( *Value=='\"' && Value[appStrlen(Value)-1]=='\"' )
						{
							Value++;
							Value[appStrlen(Value)-1]=0;
						}
						CurrentSection->Add( Start, Value );
					}
				}
			}
		}
	}
	UBOOL Write( const TCHAR* Filename )
	{
		if( !Dirty || NoSave )
			return 1;
		Dirty = 0;
		FString Text;
		for( TIterator It(*this); It; ++It )
		{
			Text += FString::Printf( TEXT("[%s]%s"), *It.Key(), LINE_TERMINATOR );
			for( FConfigSection::TIterator It2(It.Value()); It2; ++It2 )
				Text += FString::Printf( TEXT("%s=%s%s%s%s"), *It2.Key(), Quotes ? TEXT("\"") : TEXT(""), *It2.Value(), Quotes ? TEXT("\"") : TEXT(""), LINE_TERMINATOR );
			Text += FString::Printf( LINE_TERMINATOR );
		}
		return appSaveStringToFile( Text, Filename );
	}
};

// Set of all cached config files.
class FConfigCacheIni : public FConfigCache, public TMap<FString,FConfigFile>
{
public:
	// Basic functions.
	FConfigCacheIni()
	{}
	~FConfigCacheIni()
	{
		Flush( 1 );
	}
	FConfigFile* Find( const TCHAR* InFilename, UBOOL CreateIfNotFound )
	{
		FFilename Filename( InFilename  );
			
		// Get file.
		FConfigFile* Result = TMap<FString,FConfigFile>::Find( *Filename );
		if( !Result && (CreateIfNotFound || GFileManager->FileSize(*Filename)>=0)  )
		{
			Result = &Set( *Filename, FConfigFile() );
			Result->Read( *Filename );
		}
		return Result;
	}
	void Flush( UBOOL Read, const TCHAR* Filename=NULL )
	{
		for( TIterator It(*this); It; ++It )
			if( !Filename || It.Key()==Filename )
				It.Value().Write( *It.Key() );
		if( Read )
		{
			if( Filename )
				Remove(Filename);
			else
				Empty();
		}
	}
	void UnloadFile( const TCHAR* Filename )
	{
		FConfigFile* File = Find( Filename, 1 );
		if( File )
			Remove( Filename );
	}
	void Detach( const TCHAR* Filename )
	{
		FConfigFile* File = Find( Filename, 1 );
		if( File )
			File->NoSave = 1;
	}
	UBOOL GetString( const TCHAR* Section, const TCHAR* Key, FString& Value, const TCHAR* Filename )
	{
		Value = TEXT("");
		FConfigFile* File = Find( Filename, 0 );
		if( !File )
			return 0;
		FConfigSection* Sec = File->Find( Section );
		if( !Sec )
			return 0;
		FString* PairString = Sec->Find( Key );
		if( !PairString )
			return 0;
		Value = **PairString;
		return 1;
	}
	UBOOL GetSection( const TCHAR* Section, TArray<FString>& Result, const TCHAR* Filename )
	{
		Result.Empty();
		FConfigFile* File = Find( Filename, 0 );
		if( !File )
			return 0;
		FConfigSection* Sec = File->Find( Section );
		if( !Sec )
			return 0;
		for( FConfigSection::TIterator It(*Sec); It; ++It )
			new(Result) FString(FString::Printf( TEXT("%s=%s"), *It.Key(), *It.Value() ));
		return 1;
	}
	TMultiMap<FString,FString>* GetSectionPrivate( const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename )
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
	void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename )
	{
		FConfigFile* File = Find( Filename, 1 );
		FConfigSection* Sec  = File->Find( Section );
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
	void EmptySection( const TCHAR* Section, const TCHAR* Filename )
	{
		FConfigFile* File = Find( Filename, 0 );
		if( File )
		{
			FConfigSection* Sec = File->Find( Section );
			if( Sec && FConfigSection::TIterator(*Sec) )
			{
				Sec->Empty();
				File->Dirty = 1;
			}
		}
	}
	void Exit()
	{
		Flush( 1 );
	}
	void Dump( FOutputDevice& Ar )
	{
		Ar.Log( TEXT("Files map:") );
		TMap<FString,FConfigFile>::Dump( Ar );
	}

	// Derived functions.
	FString GetStr( const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename )
	{
		FString Result;
		GetString( Section, Key, Result, Filename );
		return Result;
	}
	UBOOL GetInt
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
	UBOOL GetFloat
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
	UBOOL GetBool
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
			if( appStricmp(*Text,TEXT("True"))==0 )
			{
				Value = 1;
			}
			else
			{
				Value = appAtoi(*Text)==1;
			}
			return 1;
		}
		return 0;
	}
	void SetInt
	(
		const TCHAR* Section,
		const TCHAR* Key,
		INT			 Value,
		const TCHAR* Filename
	)
	{
		TCHAR Text[30];
		appSprintf( Text, TEXT("%i"), Value );
		SetString( Section, Key, Text, Filename );
	}
	void SetFloat
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FLOAT			Value,
		const TCHAR*	Filename
	)
	{
		TCHAR Text[30];
		appSprintf( Text, TEXT("%f"), Value );
		SetString( Section, Key, Text, Filename );
	}
	void SetBool
	(
		const TCHAR* Section,
		const TCHAR* Key,
		UBOOL		 Value,
		const TCHAR* Filename
	)
	{
		SetString( Section, Key, Value ? TEXT("True") : TEXT("False"), Filename );
	}

	// Static allocator.
	static FConfigCache* Factory()
	{
		return new FConfigCacheIni();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

