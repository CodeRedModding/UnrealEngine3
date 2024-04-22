/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "TranslatorTags.h"

IMPLEMENT_CLASS(UTranslationContext);
IMPLEMENT_CLASS(UTranslatorTag);
IMPLEMENT_CLASS(UStringsTag);

/** 
 * Tokenize the string. Given "Sometext <With:Tag/> MoreText", the resulting
 * tokens would be "Sometext", "<With:Tag/>", and "MoreText". Any escape chacaters
 * are removed.
 * 
 * @param outTokens  An array of resulting tokens.
 * @param InString   The text to tokenize
 */
void TTranslator::EscapeAndTokenize( TArray<FString> *outTokens, const FString InString )
{
	outTokens->Empty();

	FString CurToken;
	UBOOL IsEscaped = FALSE;

	for( INT CharIndex=0; CharIndex < InString.Len(); ++CharIndex )
	{
		TCHAR ThisChar = InString[CharIndex];
		if ( ThisChar == '\\' && !IsEscaped )
		{
			if ( IsEscaped )
			{
				CurToken.AppendChar(ThisChar);
				IsEscaped = FALSE;
			}
			else
			{
				IsEscaped = TRUE;
			}
		}
		else if (ThisChar == '<')
		{
			if (IsEscaped)
			{
				CurToken.AppendChar(ThisChar);
				IsEscaped = FALSE;
			}
			else
			{
				if (CurToken.Len() > 0)
				{
					outTokens->AddItem(CurToken);
				}
				CurToken = TEXT("<");
			}			
		}
		else if (ThisChar == '>')
		{
			if (IsEscaped)
			{
				CurToken.AppendChar('>');
				IsEscaped = FALSE;
			}
			else
			{
				CurToken.AppendChar('>');
				outTokens->AddItem(CurToken);
				CurToken = TEXT("");
			}
		}
		else if (ThisChar == 'n')
		{
			if (IsEscaped)
			{
				if (CurToken.Len() > 0)
				{
					outTokens->AddItem(CurToken);
				}				
				outTokens->AddItem(TEXT("\n"));
				CurToken = TEXT("");
				IsEscaped = FALSE;
			}
			else
			{
				CurToken.AppendChar(ThisChar);
			}
		}
		else
		{
			if (IsEscaped)
			{
				CurToken.AppendChar('\\');
				IsEscaped = FALSE;
			}
			CurToken.AppendChar(ThisChar);
		}
	}

	if (CurToken.Len() > 0)
	{
		outTokens->AddItem(CurToken);
	}
}


/**
 * Given a tag, return the appropriate translator if possible.
 * e.g. Given <MyTag:Option />  get the Appropriate translator for MyTag.
 */
UTranslatorTag* UTranslationContext::TranslatorTagFromName( FName InName ) const
{
	UTranslatorTag* MatchingHandler = NULL;
	for ( TArray<UTranslatorTag*>::TConstIterator TranslatorTagIt(TranslatorTags); MatchingHandler==NULL && TranslatorTagIt; ++TranslatorTagIt)
	{
		if ( InName == (*TranslatorTagIt)->Tag )
		{
			MatchingHandler = *TranslatorTagIt;
		}
	}
	
	return MatchingHandler;
}

/** Add a Tag to this Context */
UBOOL UTranslationContext::RegisterTranslatorTag( UTranslatorTag* InTagHandler )
{
	UTranslatorTag* MatchingHandler = TranslatorTagFromName(InTagHandler->Tag);
	
	if (MatchingHandler == NULL)
	{
		TranslatorTags.AddItem( InTagHandler );
		MatchingHandler = InTagHandler;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/** 
 * Translator the given string by attempting to pass it to each of the registered translators.
 *
 * @param InString       Text to translate.
 * @param OutTranslated  Translated text.
 *
 * @return True if the translation succeeded.
 */
UBOOL UTranslationContext::Translate( const FString & InString, FString *OutTranslated )
{
	FString TranslatedString;
	UBOOL bTranslateSuccess = FALSE;
	if (InString.StartsWith(TEXT("<")))
	{
		INT ColonIndex = InString.InStr(TEXT(":"));
		INT TerminatorIndex = InString.InStr(TEXT("/>"));
		if (TerminatorIndex < 0)
		{
			TerminatorIndex = InString.Len()-1;
		}

		if ( ColonIndex > 1 && (ColonIndex + 1) < InString.Len() )
		{
			FName CurTagName( *InString.Mid(1,ColonIndex-1) );
			FString CurOptName( *(InString.Mid(ColonIndex+1, TerminatorIndex-ColonIndex-1).TrimTrailing()) );
			UTranslatorTag* TranslatorTag = TranslatorTagFromName(CurTagName);
			if (TranslatorTag != NULL)
			{
				TranslatedString = TranslatorTag->Translate( CurOptName );
				bTranslateSuccess = TRUE;
			}
			else
			{
				warnf( NAME_Warning, TEXT("TranslationContext could not find handler for '%s' while translating %s"), *(CurTagName.ToString()), *InString );
			}
		}		
	}	

	if (bTranslateSuccess)
	{
		*OutTranslated = TranslatedString;
	}
	else
	{
		// We failed to translate this; so just return it untouched
		*OutTranslated = InString;
	}

	return bTranslateSuccess;
}


FString UTranslatorTag::Translate(const FString& InOption)
{
	return FString::Printf( TEXT("<%s:%s Unimplemented />"), *Tag.ToString(), *InOption );
}

FString UStringsTag::Translate( const FString& InOption )
{
	TArray<FString> Parts;
	InOption.ParseIntoArray(&Parts, TEXT("."), TRUE);
	if ( Parts.Num() >= 3 )
	{
		return Localize(*Parts(1), *Parts(2), *Parts(0), NULL);
	}
	else
	{
		return FString::Printf( TEXT("StringsTag failed to translate %s"), *InOption );
	}

}



