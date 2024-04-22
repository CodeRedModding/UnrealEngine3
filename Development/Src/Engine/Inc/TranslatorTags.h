/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "Engine.h"

/**
 * A utility class that facilitates translation.
 * A Translator is used to transform a string that contains markup and translates
 * it for consumption via a display method. Multiple display targets are supported
 * via an OutputPolicy. FOutputToGfxPolicy is a policy that simply converts from
 * UE3's markup to Scaleform GFx markup. One could also implement a CanvasPolicy;
 * such a policy would render the output to an FCanvas.
 */
class TTranslator
{
public:
	
	/** 
	 * Tokenize the string. Given "Sometext <With:Tag/> MoreText", the resulting
	 * tokens would be "Sometext", "<With:Tag/>", and "MoreText". Any escape chacaters
	 * are removed.
	 * 
	 * @param outTokens  An array of resulting tokens.
	 * @param InString   The text to tokenize
	 */
	static void EscapeAndTokenize( TArray<FString> *outTokens, const FString InString );

	/**
	 * Translate markup given a translation context and a translator policy.
	 *
	 * @param InTranslationContext  A collection of TranslatorTags that comprise a context for translation.
	 *                              e.g. Two different contexts could translate controls for player one vs. controls for player two. 
	 * @param InString              The text to be translated.
	 * @param OutputPolicy          The output policy (e.g. FOutputToGfxPolicy or FOutputToFCanvasPolicy)
	 */
	template<class OutputPolicyType>
	static void TranslateMarkup( UTranslationContext* InTranslationContext,  const FString& InString, OutputPolicyType& OutputPolicy)
	{
		Translate_Helper( InTranslationContext, InString, OutputPolicy );
	}

private:
	/** Helper to TranslateMarkup that does all the actual work. */
	template<class OutputPolicyType>
	static void Translate_Helper( UTranslationContext* InTagContext, const FString& InString, OutputPolicyType& OutputPolicy )
	{
		UBOOL HasErrors = FALSE;

		TArray<FString> Tokens;
		EscapeAndTokenize(&Tokens, InString);

		for ( TArray<FString>::TConstIterator TokenIt(Tokens); TokenIt; ++TokenIt )
		{
			const FString& CurToken = *TokenIt;

			UBOOL bIsTag = (CurToken[0]=='<');

			if ( bIsTag )
			{
				if ( CurToken.StartsWith( TEXT("<Font:") ) )
				{
					const INT StrlenFontTag = 6; // "<Font:".Len();
					FString FontName = CurToken.Mid(StrlenFontTag, CurToken.Len()-StrlenFontTag-1);
					OutputPolicy.OnBeginFont( FontName );
				}
				else if (CurToken.StartsWith( TEXT("<Color:") ))
				{
					const INT StrlenColorTag = 7; // "<Color:".Len();
					FString ColorArgs=CurToken.Right( CurToken.Len()-StrlenColorTag );

					UBOOL bSuccess = TRUE;			

					float R=1;
					float G=1;
					float B=1;
					float A=1;

					if ((!Parse(*ColorArgs,TEXT("R="),R)) || 
						(!Parse(*ColorArgs,TEXT("G="),G)) ||
						(!Parse(*ColorArgs,TEXT("B="),B)) )
					{
						bSuccess = FALSE;
					}

					if ( ! Parse(*ColorArgs,TEXT("A="),A) )
					{
						A = 1;
					}			

					if (bSuccess)
					{
						OutputPolicy.OnBeginColor( FLinearColor( R, G, B, A ) );
					}
					else
					{
						OutputPolicy.OnError(CurToken);
					}
				}
				else if (CurToken.StartsWith( TEXT("</Font") ))
				{
					OutputPolicy.OnEndFont();
				}
				else if (CurToken.StartsWith( TEXT("</Color") ))
				{
					OutputPolicy.OnEndColor();
				}
				else
				{
					FString TranslationResult;
					UBOOL bTranslationSuccessful = FALSE;

					if ( InTagContext != NULL && InTagContext->Translate(CurToken, &TranslationResult) )
					{
						// Dynamic tags could themselves be marked up or contain other dynamic tags!
						Translate_Helper( InTagContext, TranslationResult, OutputPolicy );
					}
					else if ( GEngine->GlobalTranslationContext->Translate(CurToken, &TranslationResult) )
					{
						// Dynamic tags could themselves be marked up or contain other dynamic tags!
						Translate_Helper( InTagContext, TranslationResult, OutputPolicy );
					}
					else
					{
						OutputPolicy.OnError( TranslationResult );
					}
				}
			}
			else
			{
				if (CurToken == TEXT("\n") )
				{
					OutputPolicy.OnNewLine();
				}
				else
				{
					OutputPolicy.OnPlainText( CurToken );
				}
			}
		}	
	}
};
