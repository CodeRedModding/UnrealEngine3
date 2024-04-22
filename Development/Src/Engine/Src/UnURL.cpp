/*=============================================================================
	UnURL.cpp: Various file-management functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	FURL Statics.
-----------------------------------------------------------------------------*/

// Variables.
FString FURL::DefaultProtocol;
FString FURL::DefaultName;
FString FURL::DefaultMap;
FString FURL::DefaultLocalMap;
FString FURL::DefaultLocalOptions;
FString FURL::DefaultTransitionMap;
FString FURL::DefaultHost;
FString FURL::DefaultPortal;
FString FURL::DefaultMapExt;
FString FURL::DefaultSaveExt;
FString FURL::AdditionalMapExt;
INT		FURL::DefaultPort=0;
INT		FURL::DefaultPeerPort=0;
UBOOL	FURL::bDefaultsInitialized=FALSE;

// Static init.
void FURL::StaticInit()
{
	if (!GIsUCCMake)
	{
		DefaultProtocol				= GConfig->GetStr( TEXT("URL"), TEXT("Protocol"),	GEngineIni );
		DefaultName					= GConfig->GetStr( TEXT("URL"), TEXT("Name"),		GEngineIni );
		// strip off any file extensions from map names
		DefaultMap					= FFilename(GConfig->GetStr( TEXT("URL"), TEXT("Map"),		GEngineIni )).GetBaseFilename();
		DefaultLocalMap				= FFilename(GConfig->GetStr( TEXT("URL"), TEXT("LocalMap"),	GEngineIni )).GetBaseFilename();
		DefaultLocalOptions			= FFilename(GConfig->GetStr( TEXT("URL"), TEXT("LocalOptions"),	GEngineIni ));
		DefaultTransitionMap		= FFilename(GConfig->GetStr( TEXT("URL"), TEXT("TransitionMap"), GEngineIni )).GetBaseFilename();
		DefaultHost					= GConfig->GetStr( TEXT("URL"), TEXT("Host"),		GEngineIni );
		DefaultPortal				= GConfig->GetStr( TEXT("URL"), TEXT("Portal"),		GEngineIni );
		DefaultMapExt				= GConfig->GetStr( TEXT("URL"), TEXT("MapExt"),		GEngineIni );
		DefaultSaveExt				= GConfig->GetStr( TEXT("URL"), TEXT("SaveExt"),	GEngineIni );
		AdditionalMapExt			= GConfig->GetStr( TEXT("URL"), TEXT("AdditionalMapExt"),	GEngineIni );

		FString Port;
		// Allow the command line to override the default port
		if (Parse(appCmdLine(),TEXT("Port="),Port) == FALSE)
		{
			Port = GConfig->GetStr( TEXT("URL"), TEXT("Port"), GEngineIni );
		}
		DefaultPort					= appAtoi( *Port );
		// Allow the command line to override the default peer port
		FString PeerPort;
		if (Parse(appCmdLine(),TEXT("PeerPort="),PeerPort) == FALSE)
		{
			PeerPort = GConfig->GetStr( TEXT("URL"), TEXT("PeerPort"), GEngineIni );
		}
		DefaultPeerPort				= appAtoi( *PeerPort );
		bDefaultsInitialized		= TRUE;
	}
}
void FURL::StaticExit()
{
	DefaultProtocol				= TEXT("");
	DefaultName					= TEXT("");
	DefaultMap					= TEXT("");
	DefaultLocalMap				= TEXT("");
	DefaultLocalOptions			= TEXT("");
	DefaultTransitionMap		= TEXT("");
	DefaultHost					= TEXT("");
	DefaultPortal				= TEXT("");
	DefaultMapExt				= TEXT("");
	DefaultSaveExt				= TEXT("");
	bDefaultsInitialized		= FALSE;
}


FArchive& operator<<( FArchive& Ar, FURL& U )
{
	Ar << U.Protocol << U.Host << U.Map << U.Portal << U.Op << U.Port << U.Valid;
	return Ar;
}

/*-----------------------------------------------------------------------------
	Internal.
-----------------------------------------------------------------------------*/

static UBOOL ValidNetChar( const TCHAR* c )
{
	// NOTE: We purposely allow for SPACE characters inside URL strings, since we need to support player aliases
	//   on the URL that potentially have spaces in them.

	// @todo: Support true URL character encode/decode (e.g. %20 for spaces), so that we can be compliant with
	//   URL protocol specifications

	// NOTE: EQUALS characters (=) are not checked here because they're valid within fragments, but incoming
	//   option data should always be filtered of equals signs

	if( appStrchr( c, ':' ) || appStrchr( c, '/' ) ||		// : and / are for protocol and user/password info
		appStrchr( c, '?' ) || appStrchr( c, '#' ) )		// ? and # delimit fragments
	{		
		return FALSE;
	}

	return TRUE;
}

/*-----------------------------------------------------------------------------
	Constructors.
-----------------------------------------------------------------------------*/

//
// Constuct a purely default, local URL from an optional filename.
//
FURL::FURL( const TCHAR* LocalFilename )
:	Protocol	( DefaultProtocol )
,	Host		( DefaultHost )
,	Port		( DefaultPort )
,	Op			()
,	Portal		( DefaultPortal )
,	Valid		( 1 )
{
	// strip off any extension from map name
	if (LocalFilename)
	{
		Map = FFilename(LocalFilename).GetBaseFilename();
	}
	else
	{
		Map = DefaultMap;
	}
}

//
// Helper function.
//
static inline TCHAR* HelperStrchr( TCHAR* Src, TCHAR A, TCHAR B )
{
	TCHAR* AA = appStrchr( Src, A );
	TCHAR* BB = appStrchr( Src, B );
	return (AA && (!BB || AA<BB)) ? AA : BB;
}


/**
 * Static: Removes any special URL characters from the specified string
 *
 * @param Str String to be filtered
 */
void FURL::FilterURLString( FString& Str )
{
	FString NewString;
	for( INT CurCharIndex = 0; CurCharIndex < Str.Len(); ++CurCharIndex )
	{
		const TCHAR CurChar = Str[ CurCharIndex ];

		if( CurChar != ':' && CurChar != '?' && CurChar != '/' && CurChar != '#' && CurChar != '=' )
		{
			NewString.AppendChar( Str[ CurCharIndex ] );
		}
	}

	Str = NewString;
}




//
// Construct a URL from text and an optional relative base.
//
FURL::FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type )
:	Protocol	( DefaultProtocol )
,	Host		( DefaultHost )
,	Port		( DefaultPort )
,	Map			( DefaultMap )
,	Op			()
,	Portal		( DefaultPortal )
,	Valid		( 1 )
{
	check(TextURL);

	if( !bDefaultsInitialized )
	{
		FURL::StaticInit();
		Protocol = DefaultProtocol;
		Host = DefaultHost;
		Port = DefaultPort;
		Map = DefaultMap;
		Portal = DefaultPortal;
	}

	// Make a copy.
	const INT URLLength = appStrlen(TextURL);
	TCHAR* TempURL = (TCHAR*)appAlloca((URLLength+1)*sizeof(TCHAR));
	TCHAR* URL = TempURL;

	appStrcpy( TempURL, URLLength + 1, TextURL );

	// Copy Base.
	if( Type==TRAVEL_Relative )
	{
		check(Base);
		Protocol = Base->Protocol;
		Host     = Base->Host;
		Map      = Base->Map;
		Portal   = Base->Portal;
		Port     = Base->Port;
	}
	if( Type==TRAVEL_Relative || Type==TRAVEL_Partial )
	{
		check(Base);
		for( INT i=0; i<Base->Op.Num(); i++ )
		{
			if
			(	appStricmp(*Base->Op(i),TEXT("PUSH"))!=0
			&&	appStricmp(*Base->Op(i),TEXT("POP" ))!=0
			&&	appStricmp(*Base->Op(i),TEXT("PEER"))!=0
			&&	appStricmp(*Base->Op(i),TEXT("LOAD"))!=0
			&&	appStricmp(*Base->Op(i),TEXT("QUIET"))!=0 )
				new(Op)FString(Base->Op(i));
		}
	}

	// Skip leading blanks.
	while( *URL == ' ' )
		URL++;

	// Options.
	TCHAR* s = HelperStrchr(URL,'?','#');
	if( s )
	{
		TCHAR OptionChar=*s, NextOptionChar=0;
		*s++ = 0;
		do
		{
			TCHAR* t = HelperStrchr(s,'?','#');
			if( t )
			{
				NextOptionChar = *t;
				*t++ = 0;
			}
			if( !ValidNetChar( s ) )
			{
				*this = FURL();
				Valid = 0;
				return;
			}
			if( OptionChar=='?' )
				AddOption( s );
			else
				Portal = s;
			s = t;
			OptionChar = NextOptionChar;
		} while( s );
	}

	// Handle pure filenames.
	UBOOL FarHost=0;
	UBOOL FarMap=0;
	if( appStrlen(URL)>2 && URL[1]==':' )
	{
		// Pure filename.
		Protocol = DefaultProtocol;
		Host = DefaultHost;
		Map = URL;
		Portal = DefaultPortal;
		URL = NULL;
		FarHost = 1;
		FarMap = 1;
		Host = TEXT("");
	}
	else
	{
		// Parse protocol.
		if
		(	(appStrchr(URL,':')!=NULL)
		&&	(appStrchr(URL,':')>URL+1)
		&&	(appStrchr(URL,'.')==NULL || appStrchr(URL,':')<appStrchr(URL,'.')) )
		{
			TCHAR* ss = URL;
			URL      = appStrchr(URL,':');
			*URL++   = 0;
			Protocol = ss;
		}

		// Parse optional leading slashes.
		if( *URL=='/' )
		{
			URL++;
			if( *URL++ != '/' )
			{
				*this = FURL();
				Valid = 0;
				return;
			}
			FarHost = 1;
			Host = TEXT("");
		}

		// Parse optional host name and port.
		const TCHAR* Dot = appStrchr(URL,'.');
		if
		(	(Dot)
		&&	(Dot-URL>0)
		&&	(appStrnicmp( Dot+1,*DefaultMapExt,  DefaultMapExt .Len() )!=0 || appIsAlnum(Dot[DefaultMapExt .Len()+1]) )
		&&	(appStrnicmp( Dot+1,*AdditionalMapExt, AdditionalMapExt.Len() )!=0 || appIsAlnum(Dot[AdditionalMapExt.Len()+1]) )
		&&	(appStrnicmp( Dot+1,*DefaultSaveExt, DefaultSaveExt.Len() )!=0 || appIsAlnum(Dot[DefaultSaveExt.Len()+1]) )
		&&	(appStrnicmp( Dot+1,TEXT("demo"), 4 ) != 0 || appIsAlnum(Dot[5])) )
		{
			TCHAR* ss = URL;
			URL     = appStrchr(URL,'/');
			if( URL )
				*URL++ = 0;
			TCHAR* t = appStrchr(ss,':');
			if( t )
			{
				// Port.
				*t++ = 0;
				Port = appAtoi( t );
			}
			Host = ss;
			if( appStricmp(*Protocol,*DefaultProtocol)==0 )
				Map = DefaultMap;
			else
				Map = TEXT("");
			FarHost = 1;
		}
	}

	// Copy existing options which aren't in current URL	
	if( Type==TRAVEL_Absolute && Base && IsInternal())
	{
		for( INT i=0; i<Base->Op.Num(); i++ )
		{
			if
			(	appStrnicmp(*Base->Op(i),TEXT("Name="),5)==0
			||	appStrnicmp(*Base->Op(i),TEXT("Team=" ),5)==0
			||	appStrnicmp(*Base->Op(i),TEXT("Class="),6)==0
			||	appStrnicmp(*Base->Op(i),TEXT("Skin="),5)==0 
			||	appStrnicmp(*Base->Op(i),TEXT("Face="),5)==0 
			||	appStrnicmp(*Base->Op(i),TEXT("Voice="),6 )==0 
			||	appStrnicmp(*Base->Op(i),TEXT("OverrideClass="),14)==0 )
			{
				const FString& BaseOption = Base->Op(i);
				FString OptionName;

				INT Pos = BaseOption.InStr(TEXT("="));
				if ( Pos != INDEX_NONE )
				{
					OptionName = BaseOption.Left(Pos);
				}
				else
				{
					OptionName = BaseOption;
				}

				if ( !appStrcmp(GetOption(*OptionName, TEXT("")), TEXT("")) )
				{
					debugf(NAME_DevNet, TEXT("URL: Adding default option %s"), *BaseOption );
					Op.AddItem(BaseOption);
				}
			}
		}
	}

	// Parse optional map and teleporter.
	if( URL && *URL )
	{
		if(IsInternal())
		{
			// Portal.
			FarMap = 1;
			TCHAR* t = appStrchr(URL,'/');
			if( t )
			{
				// Trailing slash.
				*t++ = 0;
				TCHAR* u = appStrchr(t,'/');
				if( u )
				{
					*u++ = 0;
					if( *u != 0 )
					{
						*this = FURL();
						Valid = 0;
						return;
					}
				}

				// Portal name.
				Portal = t;
			}
		}

		// Map.
		Map = URL;
	}
	
	// Validate everything.
	if
	(	!ValidNetChar(*Protocol  )
	||	!ValidNetChar(*Host      )
	//||	!ValidNetChar(*Map       )
	||	!ValidNetChar(*Portal    )
	||	(!FarHost && !FarMap && !Op.Num()) )
	{
		*this = FURL();
		Valid = 0;
		return;
	}

	// Success.
}

/*-----------------------------------------------------------------------------
	Conversion to text.
-----------------------------------------------------------------------------*/

//
// Convert this URL to text.
//
FString FURL::String( UBOOL FullyQualified ) const
{
	FString Result;

	// Emit protocol.
	if( Protocol!=DefaultProtocol || FullyQualified )
	{
		Result += Protocol;
		Result += TEXT(":");
		if( Host!=DefaultHost )
			Result += TEXT("//");
	}

	// Emit host.
	if( Host!=DefaultHost || Port!=DefaultPort )
	{
		Result += Host;
		if( Port!=DefaultPort )
		{
			Result += TEXT(":");
			Result += FString::Printf( TEXT("%i"), Port );
		}
		Result += TEXT("/");
	}

	// Emit map.
	if( Map.Len() > 0 )
		Result += Map;

	// Emit options.
	for( INT i=0; i<Op.Num(); i++ )
	{
		Result += TEXT("?");
		Result += Op(i);
	}

	// Emit portal.
	if( Portal.Len() > 0 )
	{
		Result += TEXT("#");
		Result += Portal;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	Informational.
-----------------------------------------------------------------------------*/

//
// Return whether this URL corrsponds to an internal object, i.e. an Unreal
// level which this app can try to connect to locally or on the net. If this
// is fals, the URL refers to an object that a remote application like Internet
// Explorer can execute.
//
UBOOL FURL::IsInternal() const
{
	return Protocol==DefaultProtocol;
}

//
// Return whether this URL corresponds to an internal object on this local 
// process. In this case, no Internet use is necessary.
//
UBOOL FURL::IsLocalInternal() const
{
	return IsInternal() && Host.Len()==0;
}

//
// Add a unique option to the URL, replacing any existing one.
//
void FURL::AddOption( const TCHAR* Str )
{
	INT Match = appStrchr(Str, '=') ? (appStrchr(Str, '=') - Str) : appStrlen(Str);
	INT i;
	for (i = 0; i < Op.Num(); i++)
	{
		if (appStrnicmp(*Op(i), Str, Match) == 0 && ((*Op(i))[Match] == '=' || (*Op(i))[Match] == '\0'))
		{
			break;
		}
	}
	if (i == Op.Num())
	{
		new(Op) FString(Str);
	}
	else
	{
		Op(i) = Str;
	}
}


//
// Remove an option from the URL
//
void FURL::RemoveOption( const TCHAR* Key, const TCHAR* Section, const TCHAR* Filename )
{
	if ( !Key )
		return;

	if ( !Filename )
		Filename = GGameIni;

	for ( INT i = Op.Num() - 1; i >= 0; i-- )
	{
		if ( Op(i).Left(appStrlen(Key)) == Key )
		{
			FConfigSection* Sec = GConfig->GetSectionPrivate( Section ? Section : TEXT("DefaultPlayer"), 0, 0, Filename );
			if ( Sec )
			{
				if (Sec->Remove( Key ) > 0)
					GConfig->Flush( 0, Filename );
			}

			Op.Remove(i);
		}
	}
}

//
// Load URL from config.
//
void FURL::LoadURLConfig( const TCHAR* Section, const TCHAR* Filename )
{
	TArray<FString> Options;
	GConfig->GetSection( Section, Options, Filename );
	for( INT i=0;i<Options.Num();i++ )
		AddOption( *Options(i) );
}

//
// Save URL to config.
//
void FURL::SaveURLConfig( const TCHAR* Section, const TCHAR* Item, const TCHAR* Filename ) const
{
	for( INT i=0; i<Op.Num(); i++ )
	{
		TCHAR Temp[1024];
		appStrcpy( Temp, *Op(i) );
		TCHAR* Value = appStrchr(Temp,'=');
		if( Value )
		{
			*Value++ = 0;
			if( appStricmp(Temp,Item)==0 )
				GConfig->SetString( Section, Temp, Value, Filename );
		}
	}
}

//
// See if the URL contains an option string.
//
UBOOL FURL::HasOption( const TCHAR* Test ) const
{
	return GetOption( Test, NULL ) != NULL;
}

const TCHAR* FURL::GetOption( const TCHAR* Match, const TCHAR* Default ) const
{
	const INT Len = appStrlen(Match);
	
	if( Len > 0 )
	{
		for( INT i = 0; i < Op.Num(); i++ ) 
		{
			const TCHAR* s = *Op(i);
			if( appStrnicmp( s, Match, Len ) == 0 ) 
			{
				if (s[Len-1] == '=' || s[Len] == '=' || s[Len] == '\0') 
				{
					return s + Len;
				}
			}
		}
	}

	return Default;
}

/*-----------------------------------------------------------------------------
	Comparing.
-----------------------------------------------------------------------------*/

//
// Compare two URL's to see if they refer to the same exact thing.
//
UBOOL FURL::operator==( const FURL& Other ) const
{
	if
	(	Protocol	!= Other.Protocol
	||	Host		!= Other.Host
	||	Map			!= Other.Map
	||	Port		!= Other.Port
	||  Op.Num()    != Other.Op.Num() )
		return 0;

	for( INT i=0; i<Op.Num(); i++ )
		if( Op(i) != Other.Op(i) )
			return 0;

	return 1;
}

