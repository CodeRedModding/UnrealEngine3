/*=============================================================================
	WebServer.cpp: Unreal Web Server
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

// To help ANSI out.
#undef clock
#undef unclock

#include <time.h>
/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

static TMap<FString,FString>& CachedFileContent() // Caching unparsed special files - sjs modified for static linking
{
    static TMap<FString, FString>	CachedFileContent;
    return CachedFileContent;
}
FString		WebRootRealPath;				// Full path on system of web root
/*-----------------------------------------------------------------------------
	UWebRequest functions.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UWebRequest);

//
// Decode a base64 encoded string - used for HTTP authentication
//
FString UWebRequest::DecodeBase64(const FString& Encoded)
{
	TCHAR *Decoded = (TCHAR *)appAlloca((Encoded.Len() / 4 * 3 + 1) * sizeof(TCHAR));
	check(Decoded);

	FString Base64Map(TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"));
	INT ch, i=0, j=0;
	TCHAR Junk[2] = {0, 0};
	TCHAR *Current = (TCHAR *)*Encoded;

    while((ch = (INT)(*Current++)) != '\0')
	{
		if (ch == '=')
			break;

		Junk[0] = ch;
		ch = Base64Map.InStr(Junk);
		if( ch == -1 )
		{
			return TEXT("");
		}

		switch(i % 4) {
		case 0:
			Decoded[j] = ch << 2;
			break;
		case 1:
			Decoded[j++] |= ch >> 4;
			Decoded[j] = (ch & 0x0f) << 4;
			break;
		case 2:
			Decoded[j++] |= ch >>2;
			Decoded[j] = (ch & 0x03) << 6;
			break;
		case 3:
			Decoded[j++] |= ch;
			break;
		}
		i++;
	}

    /* clean up if we ended on a boundary */
    if (ch == '=') 
	{
		switch(i % 4)
		{
		case 0:
		case 1:
			return TEXT("");
		case 2:
			j++;
		case 3:
			Decoded[j++] = 0;
		}
	}
	Decoded[j] = '\0';
	return FString(Decoded);
}

FString UWebRequest::EncodeBase64(const FString& Decoded)
{
	TCHAR *Encoded = (TCHAR *)appAlloca(((Decoded.Len() + 1) * 4 + 1) * sizeof(TCHAR));
	check(Encoded);

	FString Base64Map(TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"));

	INT i=0, j=0;
	INT x,y,z;

	while (i < Decoded.Len())
	{
		x = (INT) Decoded[i];
		y = (i < Decoded.Len()-1 ) ? (INT) Decoded[i+1] : 0;
		z = (i < Decoded.Len()-2 ) ? (INT) Decoded[i+2] : 0;
		Encoded[j++] = Base64Map[x >> 2];
		Encoded[j++] = Base64Map[((x & 3) << 4) | (y >> 4)];
		Encoded[j++] = Base64Map[((y & 15) << 2) | (z >> 6)];
		Encoded[j++] = Base64Map[(z & 63)];
		i+=3;
	}

	switch (Decoded.Len() % 3)
	{
	case 1:
		Encoded[j-2] = '=';
	case 2:
		Encoded[j-1] = '=';
	}

	Encoded[j] = '\0';
	return FString(Encoded);
}

void UWebRequest::AddVariable(const FString& VariableName, const FString& Value)
{
	VariableMap.Add(*(VariableName.ToUpper()), *Value);
}

FString UWebRequest::GetVariable(const FString& VariableName, const FString& DefaultValue)
{
	if ( VariableName == TEXT("") )
	{
		return TEXT("");
	}

	FString *S = VariableMap.Find(VariableName.ToUpper());
	if(S)
		return *S;
	else
		return DefaultValue;
}

INT UWebRequest::GetVariableCount(const FString& VariableName)
{
	if ( VariableName == TEXT("") )
	{
		return 0;
	}

	TArray<FString> List;
	VariableMap.MultiFind( VariableName.ToUpper(), List );
	return List.Num();
}

FString UWebRequest::GetVariableNumber(const FString& VariableName, INT Number, const FString& DefaultValue)
{
	if ( VariableName == TEXT("") )
	{
		return TEXT("");
	}

	TArray<FString> List;
	VariableMap.MultiFind( VariableName.ToUpper(), List );
	if( !List.IsValidIndex(Number) )
		return DefaultValue;
	else
		return List(Number);
}

void UWebRequest::GetVariables(TArray<FString>& varNames)
{
	VariableMap.GenerateKeyArray(varNames);
}

void UWebRequest::AddHeader(const FString& HeaderName, const FString& Value)
{
	HeaderMap.Add(*(HeaderName.ToUpper()), *Value);
}

FString UWebRequest::GetHeader(const FString& HeaderName, const FString& DefaultValue)
{
	if ( HeaderName == TEXT("") )
	{
		return TEXT("");
	}

	FString *S = HeaderMap.Find(HeaderName.ToUpper());
	if(S)
		return *S;
	else
		return DefaultValue;
}

void UWebRequest::GetHeaders(TArray<FString>& headers)
{
	HeaderMap.GenerateKeyArray(headers);
}

/*-----------------------------------------------------------------------------
	UWebResponse functions.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UWebResponse);

#define UHTMPACKETSIZE 512


FString UWebResponse::GetIncludePath()
{
	if ( IncludePath.InStr(".")>=0 || IncludePath.InStr(":")>=0 )
		return FString::Printf(TEXT("../Web") );
	else if ( IncludePath.Left(1) != TEXT("/") )
		return FString::Printf(TEXT("../%s"),*IncludePath );

	return FString::Printf(TEXT("..%s"),*IncludePath);
}

void UWebResponse::SendInParts( const FString &S )
{
	INT Pos = 0, L;
	L = S.Len();

	if(L <= UHTMPACKETSIZE)
	{
		if(L > 0)
			eventSendText(S, 1);
		return;
	}
	while( L - Pos > UHTMPACKETSIZE )
	{
		eventSendText(S.Mid(Pos, UHTMPACKETSIZE), 1);
		Pos += UHTMPACKETSIZE;
	}
	if(Pos > 0)
		eventSendText(S.Mid(Pos), 1);
}

// ValidWebFile takes a relative path and makes sure it is located under the WebRoot
// it makes use of / and .. legal.
bool UWebResponse::ValidWebFile(const FString &Filename)
{
	// Never, ever allow a ini file or a real abs path to be sent   
	if ( Filename.InStr(".ini", TRUE, TRUE)>=0 || Filename.InStr(":", FALSE, TRUE) >=0)
		return false;

	if( IncludePath == TEXT("") )
	{
		debugf( NAME_Log, TEXT("WebServer: Missing IncludePath: %s"), *IncludePath);//!!localize!!
		return false;
	}

	if ( WebRootRealPath == TEXT("") )
	{
		WebRootRealPath = GFileManager->ConvertToAbsolutePath(*GetIncludePath());
		if (WebRootRealPath == TEXT(""))
		{
			debugf( NAME_Log, TEXT("WebServer: Bad IncludePath: %s %s"), *GetIncludePath(),*IncludePath );
			return false;
		}

		//cleanup the webroot
		WebRootRealPath = appConvertRelativePathToFull(WebRootRealPath.Replace(TEXT("/"), PATH_SEPARATOR).Replace(TEXT("\\\\"), PATH_SEPARATOR) + PATH_SEPARATOR);
	}

	//cleanup the requested file name
	FFilename TempFilename(Filename.Replace(TEXT("/"), PATH_SEPARATOR).Replace(TEXT("\\\\"), PATH_SEPARATOR));
	const FString& FullPath = appConvertRelativePathToFull(TempFilename.GetPath() + PATH_SEPARATOR);

	if (FullPath.Len() < WebRootRealPath.Len() || FullPath.Left(WebRootRealPath.Len()) != WebRootRealPath)
	{
		debugf( NAME_Log, TEXT("WebServer: Filename not under web root: %s <-> %s"), *FullPath, *WebRootRealPath );
		return false;
	}

	return true;
}

UBOOL UWebResponse::FileExists(const FString& Filename)
{
	return ValidWebFile(*(GetIncludePath() * Filename))
		&& GFileManager->FileSize(*(GetIncludePath() * Filename)) > 0;
}

UBOOL UWebResponse::IncludeBinaryFile(const FString& Filename)
{
	if ( !ValidWebFile(*(GetIncludePath() * Filename)) )
		return false;

	TArray<BYTE> Data;
	if( !appLoadFileToArray( Data, *(GetIncludePath() * Filename)) )
	{
		debugf( NAME_Log, TEXT("WebServer: Unable to open include file %s%s%s"), *GetIncludePath(), PATH_SEPARATOR, *Filename );//!!localize!!
		return false;
	}
	for( INT i=0; i<Data.Num(); i += 255)
	{										
		SendBinary( Min<INT>(Data.Num()-i, 255), ((BYTE*)Data.GetData()) + i );
	}
	return true;
}

bool UWebResponse::IncludeTextFile(const FString &Root, const FString &Filename, bool bCache, FString *Result)
{
	// threw this check in here because this is where you'll probably
	//  crash out if the mirroring is wrong...sizes break if TMultiMap
	//  changes size.  --ryan.
	VERIFY_CLASS_SIZE(UWebRequest);
	VERIFY_CLASS_SIZE(UWebResponse);

	FString IncludeFile;

	if ( Result )
		*Result = TEXT("");

	if( Filename.Left(1) == TEXT("\\") || Filename.Left(1) == TEXT("/") )
		IncludeFile = *(GetIncludePath() + Filename);
	else
	{
		IncludeFile = GetIncludePath();
		if ( Root.Left(1) != TEXT("\\") && Root.Left(1) != TEXT("/") )
			IncludeFile += TEXT("/");
		IncludeFile += Root;

		if ( IncludeFile.Right(1) != TEXT("\\") && IncludeFile.Right(1) != TEXT("/") )
			IncludeFile += TEXT("/");

		IncludeFile += Filename;
	}

	if ( !ValidWebFile(IncludeFile) )
		return false;

	FString Text(TEXT("")), *PrevText = NULL;
	if (bCache)
	{
		PrevText = CachedFileContent().Find(IncludeFile);
		if (PrevText != NULL)
			Text = *PrevText;
	}

	if( PrevText == NULL && !appLoadFileToString( Text, *IncludeFile ) )
	{
		// Fallback to root webadmin directory if using a webskin
		if ( Root.InStr(TEXT("ServerAdmin")) == INDEX_NONE || Root == TEXT("/ServerAdmin") ||
			!appLoadFileToString(Text,*(FString::Printf(TEXT("%s/ServerAdmin/%s"), *GetIncludePath(), *Filename))) )
		{
			debugf( NAME_Log, TEXT("WebServer: Unable to open include file %s"), *IncludeFile );//!!localize!!
			return false;
		}
	}

	// Set character set based on language.
	ReplacementMap.Set(TEXT("charset"), *CharSet);

	// Add to Cache if it wasnt
	if (bCache && PrevText == NULL)
		CachedFileContent().Set( *IncludeFile, *Text);

	INT Pos = 0;
	TCHAR* T = const_cast<TCHAR*>( *Text );
	TCHAR* P(NULL), *I(NULL);
	while( true )
	{
		P = appStrstr(T, TEXT("<%"));
		I = appStrstr(T, TEXT("<!--"));

		if (P == NULL && I == NULL)
			break;

		if (I == NULL || (P != NULL && P<I))
		{
			if (Result == NULL)
				SendInParts( Text.Mid(Pos, (P - T)) );
			else
				(*Result) += Text.Mid(Pos, (P - T));

			Pos += (P - T);
			T = P;

			// Find the close percentage
			TCHAR *PEnd = appStrstr(P+2, TEXT("%>"));
			if(PEnd)
			{
				FString Key = Text.Mid(Pos + (P - T) + 2, (PEnd - P) - 2);
				FString *V = NULL, Value = TEXT("");
				if(Key.Len() > 0)
				{
					V = ReplacementMap.Find(Key);
					if(V)
						Value = *V;
					else
						Value = TEXT("");
				}
  				if (Result == NULL)
					SendInParts(Value);
				else
					(*Result) += Value;

				Pos += (PEnd - P) + 2;
				T = PEnd + 2;
			}
			else
			{
				Pos++;
				T++;
			}
		}
		else
		{
			if (Result == NULL)
				SendInParts( Text.Mid(Pos, (I - T)) );
			else
				(*Result) += Text.Mid(Pos, (I - T));

			Pos += (I - T);
			T = I;

			//debugf( NAME_Log, TEXT("Found Comment Marker"));//!!localize!!

			// Find the close tag
			TCHAR *IEnd = appStrstr(I+4, TEXT("-->"));
			if(IEnd)
			{
				bool bIncluded = false;
				//debugf( NAME_Log, TEXT("Found End Comment Marker"));//!!localize!!
				T += 4;
				// the code is <!-- #include file="filename.ext" -->
				// Skip any leading white space
				while (*T == ' ' || *T == '\t' || *T == '\r' || *T == '\n') T++;

				//debugf( NAME_Log, TEXT("Next Marker: '%s'"), *(Text.Mid((Pos), 9)));//!!localize!!

				if (Text.Mid((Pos + T - I), 9) == TEXT("#include "))
				{
					T += 9;

					//debugf( NAME_Log, TEXT("Found #include"));//!!localize!!

					// Skip any leading white space
					while (*T == ' ' || *T == '\t' || *T == '\r' || *T == '\n') T++;

					if (Text.Mid((Pos + T - I), 5) == TEXT("file="))
					{
						T += 5;
						//debugf( NAME_Log, TEXT("Found 'file='"));
						while (*T == ' ' || *T == '\t' || *T == '\r' || *T == '\n') T++;
						
						if (*T == '\'' || *T == '"')
						{
							TCHAR c = *T;
							TCHAR *U = appStrchr(T+1, c);
							if (U != NULL && (U-T-1) > 0)
							{
								//debugf( NAME_Log, TEXT("WebServer: Including File '%s'"), *(Root * Text.Mid((Pos + T - I + 1), U-T-1)) );//!!localize!!
								bIncluded = IncludeTextFile(Root, Text.Mid((Pos + T - I + 1), U-T-1));
							}
						}
					}
				}
				// Send the text if it the include file was not found
				if (!bIncluded)
				{
					if (Result == NULL)
						SendInParts(Text.Mid(Pos, (IEnd - I) + 3));
					else
						(*Result) += Text.Mid(Pos, (IEnd - I) + 3);
				}

				Pos += (IEnd - I) + 3;
				T = IEnd + 3;
			}
		}
	}
	if (Result == NULL)
		SendInParts(Text.Mid(Pos));
	else
		(*Result) += Text.Mid(Pos);

	return true;
}

UBOOL UWebResponse::IncludeUHTM(const FString& Filename)
{
	// Find the root path of filename
	//FString Root;
	//int p1, p2;

	FFilename File(Filename);
	return IncludeTextFile(File.GetPath(), File.GetCleanFilename());
#if 0
	p1 = Filename.InStr(TEXT("/"), true);
	p2 = Filename.InStr(TEXT("\\"), true);
	if (p1<p2)
		p1 = p2;

	if (p1 != -1)
	{
	  Root = Filename.Left(p1);
	  Filename = Filename.Mid(p1 + 1);
	}
	else
	  Root = TEXT("");

//	debugf( NAME_Log, TEXT("WebServer: Root of File is '%s'"), *Root );//!!localize!!

	IncludeTextFile(Root, Filename);
#endif
}

FString UWebResponse::LoadParsedUHTM(const FString& Filename)
{
	// Find the root path of filename
	//FString Root;
	//int p1, p2;

	FFilename File(Filename);
	FString Result(TEXT(""));
	IncludeTextFile(File.GetPath(), File.GetCleanFilename(), false, &Result);
	return Result;

#if 0
	p1 = Filename.InStr(TEXT("/"), true);
	p2 = Filename.InStr(TEXT("\\"), true);
	if (p1<p2)
		p1 = p2;

	if (p1 != -1)
	{
	  Root = Filename.Left(p1);
	  Filename = Filename.Mid(p1 + 1);
	}
	else
	  Root = TEXT("");

	FString Result(TEXT(""));	// Clear the result first
	IncludeTextFile(Root, Filename, false, &Result);
	return Result;
#endif
}

void UWebResponse::ClearSubst()
{
	ReplacementMap.Empty();
}

void UWebResponse::Subst(const FString& Variable,const FString& Value,UBOOL bClear)
{
	if(bClear)
		ReplacementMap.Empty();

	if ( Variable == TEXT("") )
		return;

	ReplacementMap.Set( *Variable, *Value );
}

FString UWebResponse::GetHTTPExpiration(INT OffsetSeconds)
{
	// Format GMT Time as "dd mmm yyyy hh:mm:ss GMT";
	time_t ltime;

	TCHAR GMTRef[100];
	const TCHAR *Months[12] = { TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"),
								TEXT("May"), TEXT("Jun"), TEXT("Jul"), TEXT("Aug"),
								TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec") };

	time( &ltime );
	ltime += OffsetSeconds;
#if PS3 || IPHONE || ANDROID || NGP || PLATFORM_MACOSX || WIIU || FLASH
	struct tm* newtime = gmtime(&ltime);
	if ( newtime == NULL )
	{
		debugf(TEXT("Error encountered while attempting to evaluate HTTP expiration: Invalid system time!!"));
		return TEXT("");
	}

	appSprintf(GMTRef, TEXT("%02d %3s %04d %02d:%02d:%02d GMT"),
		newtime->tm_mday, Months[newtime->tm_mon], newtime->tm_year + 1900,
		newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
#else
	struct tm newtime;
	int Err = gmtime_s(&newtime, &ltime );
	if ( Err )
	{
		debugf(TEXT("Error encountered while attempting to evaluate HTTP expiration: Invalid system time!!"));
		return TEXT("");
	}

	appSprintf(GMTRef, TEXT("%02d %3s %04d %02d:%02d:%02d GMT"),
		newtime.tm_mday, Months[newtime.tm_mon], newtime.tm_year + 1900,
		newtime.tm_hour, newtime.tm_min, newtime.tm_sec);
#endif

	return FString(GMTRef);
}

void UWebRequest::Dump()
{
}

void UWebResponse::Dump()
{
}

#endif
