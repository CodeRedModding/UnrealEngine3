/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealLoc
{
    class Validator
    {
        private UnrealLoc Main = null;
        private char BadChar = ' ';
        private int Offset = 0;
        private bool DoValidateTags = false;

		enum ValidationScheme
		{
			WesternEuro,
			EasternEuro,
			Russian,
			SimplifiedChinese,
			Japanese,
			Korean
		};

        enum ValidationError
        {
            NoError,
            IllegalCharacter,
            LocalisedTags,
            MismatchedSquareBrackets,
            MismatchedParentheses,
            MismatchedCurlyBrackets,
            MismatchedTags,
            MismatchedTagCount,
            MismatchedQuotes
        };

        public Validator( UnrealLoc InMain )
        {
            Main = InMain;
        }

        private List<string> ExtractTags( string Value )
        {
            List<string> Tags = new List<string>();

            int OpenIndex = Value.IndexOf( '<' );
            int CloseIndex = Value.IndexOf( '>' );
            while( OpenIndex >= 0 && CloseIndex >= 0 && CloseIndex > OpenIndex )
            {
                string Tag = Value.Substring( OpenIndex, CloseIndex - OpenIndex + 1 );
                Tags.Add( Tag );

                Value = Value.Substring( CloseIndex + 1 );
                OpenIndex = Value.IndexOf( '<' );
                CloseIndex = Value.IndexOf( '>' );
            }

            return ( Tags );
        }

        private ValidationError ValidateTags( LocEntry LE )
        {
            // Can't check to see if the INT versions have over localised
            if( LE.DefaultLE == null )
            {
                return ( ValidationError.NoError );
            }

            List<string> DefaultTags = ExtractTags( LE.DefaultLE.Value );
            List<string> LocTags = ExtractTags( LE.Value );

            if( LocTags.Count != DefaultTags.Count )
            {
                return ( ValidationError.MismatchedTagCount );
            }

            for( int TagIndex = 0; TagIndex < DefaultTags.Count; TagIndex++ )
            {
                if( DefaultTags[TagIndex] != LocTags[TagIndex] )
                {
                    return ( ValidationError.LocalisedTags );
                }
            }

            return ( ValidationError.NoError );
        }

        private bool CheckAlphaNumeric( char Character )
        {
            if( Character >= 'a' && Character <= 'z' )
            {
                return ( true );
            }
            if( Character >= 'A' && Character <= 'Z' )
            {
                return ( true );
            }
            if( Character >= '0' && Character <= '9' )
            {
                return ( true );
            }

            return ( false );
        }

        private bool CheckAdditional( char Character, string AllowedAdditionalChars )
        {
            // Standard punctuation available for all languages - 32 <= c < 128
            const string Punct = " `~!@#$%^&*()-_=+[]{}\\|;:'\",.<>/?";
            // High ASCII characters for Western European - 192 <= c < 256 (except multiply and divide)
			string HiAscii = "";
			for( int HiAsciiChar = 192; HiAsciiChar < 256; HiAsciiChar++ )
			{
				if( HiAsciiChar != 0xd7 && HiAsciiChar != 0xf7 )
				{
					HiAscii += ( char )HiAsciiChar;
				}
			}

            if( Punct.IndexOf( Character ) >= 0 )
            {
                return ( true );
            }

            if( HiAscii.IndexOf( Character ) >= 0 )
            {
                return ( true );
            }

			if( AllowedAdditionalChars.IndexOf( Character ) >= 0 )
            {
                return ( true );
            }

            if( Character == ( char )0x00a0 )
            {
                return ( true );
            }

            return ( false );
        }

        private void FixupTabs( LanguageInfo Lang, ref string Value )
        {
            if( Value.IndexOf( ( char )0x0009 ) >= 0 )
            {
                Lang.AddWarning( "Replacing tab with four spaces in '" + Value + "'" );
                Value = Value.Replace( "\t", "    " );
            }
        }

        private void FixupSmartQuotes( LanguageInfo Lang, ref string Value )
        {
            char OpenSmartQuote = ( char )0x201c;
            char CloseSmartQuote = ( char )0x201d;
            if( Value.IndexOf( OpenSmartQuote ) >= 0 || Value.IndexOf( CloseSmartQuote ) >= 0 )
            {
                Lang.AddWarning( "Replacing smart quotes with normal quotes in '" + Value + "'" );
                Value = Value.Replace( OpenSmartQuote, '\"' );
                Value = Value.Replace( CloseSmartQuote, '\"' );
            }
        }

        private void FixupSmartSingleQuotes( LanguageInfo Lang, ref string Value )
        {
            char OpenSmartSingleQuote = ( char )0x2018;
            char CloseSmartSingleQuote = ( char )0x2019;
            if( Value.IndexOf( OpenSmartSingleQuote ) >= 0 || Value.IndexOf( CloseSmartSingleQuote ) >= 0 )
            {
                Lang.AddWarning( "Replacing smart single quotes with normal quotes in '" + Value + "'" );
                Value = Value.Replace( OpenSmartSingleQuote, '\'' );
                Value = Value.Replace( CloseSmartSingleQuote, '\'' );
            }
        }

        private void FixupDoubleSingleQuotes( LanguageInfo Lang, ref string Value )
        {
            string DoubleSingleQuote = "''";
            if( Value.IndexOf( DoubleSingleQuote ) >= 0 )
            {
                Lang.AddWarning( "Replacing double single quotes with single double quote in '" + Value + "'" );
                Value = Value.Replace( DoubleSingleQuote, "\"" );
            }
        }

		private ValidationError FixupQuotes( LanguageInfo Lang, ref string Value, bool bOuterQuotesRequired )
		{
			char EastEuroOpenQuote = ( char )0x201e;
			int Offset = 0;
			bool bInSquareBrackets = false;
			List<int> DoubleQuoteLocations = new List<int>();

			// Make sure the quotes are matched
			foreach( char C in Value )
			{
				if( C == '[' )
				{
					bInSquareBrackets = true;
				}
				else if( C == ']' )
				{
					bInSquareBrackets = false;
				}
				else if( !bInSquareBrackets && ( C == '\"' || C == EastEuroOpenQuote ) )
				{
					DoubleQuoteLocations.Add( Offset );
				}

				Offset++;
			}

			// Early out if the quotes aren't paired
			if( DoubleQuoteLocations.Count % 2 != 0 )
			{
				return ValidationError.MismatchedQuotes;
			}

			// If outer parens, no escaping required
			if( Value[0] == '(' && Value[Value.Length - 1] == ')' )
			{
				return ValidationError.NoError;
			}

			// Early out if there are no quotes to check, or outer quotes are not required (-> no escaping is required)
			if( bOuterQuotesRequired && ( DoubleQuoteLocations.Count > 0 ) )
			{
				if( DoubleQuoteLocations[0] != 0 || DoubleQuoteLocations[DoubleQuoteLocations.Count - 1] != Value.Length - 1 )
				{
					return ( ValidationError.MismatchedQuotes );
				}

				// Remove the quotes that define the entire property
				DoubleQuoteLocations.RemoveAt( DoubleQuoteLocations.Count - 1 );
				DoubleQuoteLocations.RemoveAt( 0 );

				// Quotes at the beginning and end are required for escaping to work
				bool bInQuotes = false;
				int LastLocation = -1;
				Offset = 0;
				foreach( int Location in DoubleQuoteLocations )
				{
					bInQuotes = !bInQuotes;
					// Make sure the closing quotes are followed by a ')' or a ',' or 0 or /r or /n
					if( !bInQuotes )
					{
						// Check for end of string
						if( Location + Offset + 1 == Value.Length )
						{
							continue;
						}

						// Check for already being escaped
						if( Location > 0 && LastLocation > 0 )
						{
							if( Value[Location + Offset - 1] == '\\' && Value[LastLocation + Offset - 1] == '\\' )
							{
								continue;
							}
						}

						// Escape both opening and closing quotes
						Value = Value.Remove( Location + Offset, 1 );
						Value = Value.Insert( Location + Offset, "\\\"" );

						Value = Value.Remove( LastLocation + Offset, 1 );
						Value = Value.Insert( LastLocation + Offset, "\\\"" );

						// We've added 2 characters
						Offset += 2;
					}

					LastLocation = Location;
				}
			}

			return ValidationError.NoError;
		}

        private void FixupEllipses( LanguageInfo Lang, ref string Value, bool Asian )
        {
            string FivePeriods = ".....";
            string FourPeriods = "....";
            string ThreePeriods = "...";
            char Ellipsis = ( char )0x2026;

            if( Main.Options.bAddEllipses && !Asian )
            {
                if( Value.IndexOf( FivePeriods ) >= 0 )
                {
                    Value = Value.Replace( FivePeriods, Ellipsis.ToString() );
                }

                if( Value.IndexOf( FourPeriods ) >= 0 )
                {
                    Value = Value.Replace( FourPeriods, Ellipsis.ToString() );
                }
                
                if( Value.IndexOf( ThreePeriods ) >= 0 )
                {
                    Lang.AddWarning( "Replacing '...' with ellipses in '" + Value + "'" );
                    Value = Value.Replace( ThreePeriods, Ellipsis.ToString() );
                }
            }
            else if( Main.Options.bRemoveEllipses )
            {
                // Ellipses don't work on XP =(
                if( Value.IndexOf( Ellipsis ) >= 0 )
                {
                    Value = Value.Replace( Ellipsis.ToString(), ThreePeriods );
                    Lang.AddWarning( "Replacing ellipsis with '...' in '" + Value + "'" );
                }
            }
        }

        private void FixupDashes( LanguageInfo Lang, ref string Value, bool Asian )
        {
            char EMDash = ( char )0x2014;

            char ENDash = ( char )0x2013;
            if( Value.IndexOf( ENDash ) >= 0 )
            {
                Lang.AddWarning( "Replacing ENDash with hyphen in '" + Value + "'" );
                Value = Value.Replace( ENDash, '-' );
            }

            if( Asian )
            {
                if( Value.IndexOf( EMDash ) >= 0 )
                {
                    Lang.AddWarning( "Replacing EMDash with hyphen in '" + Value + "'" );
                    Value = Value.Replace( EMDash, '-' );
                }
            }

            if( !Asian )
            {
                if( Value.IndexOf( "--" ) >= 0 )
                {
                    Lang.AddWarning( "Replacing -- with EMDash in '" + Value + "'" );
                    Value = Value.Replace( "--", EMDash.ToString() );
                }
            }
        }

        private ValidationError MatchPairs( ref string Value, ValidationScheme Scheme )
        {
            int ParensCount = 0;
			int SquareBracketCount = 0;
			int CurlyBracketCount = 0;
            int TagCount = 0;
            char LastChar = ' ';

            Offset = 0;

            foreach( char C in Value )
            {
                Offset++;

                if( C == '(' )
                {
                    ParensCount++;
                }
				// Emoticons do not count as closing parentheses
				else if( C == ')' && LastChar != ':' && LastChar != ';' && LastChar != '=' )
                {
                    ParensCount--;
                }
                else if( C == '{' )
                {
                    CurlyBracketCount++;
                }
                else if( C == '}' )
                {
                    CurlyBracketCount--;
                }
                else if( C == '<' )
                {
                    DoValidateTags = true;
                    TagCount++;
                }
                else if( C == '>' && LastChar != '\\' )
                {
                    TagCount--;
                }
				else if( C == '[' )
				{
					SquareBracketCount++;
				}
				else if( C == ']' )
				{
					SquareBracketCount--;
				}

				switch( Scheme )
				{
				case ValidationScheme.WesternEuro:
					if( !CheckAlphaNumeric( C ) && !CheckAdditional( C, Main.Options.AllowedWesternEuroCharactersAlt ) )
					{
						BadChar = C;
						return ( ValidationError.IllegalCharacter );
					}
					break;

				case ValidationScheme.EasternEuro:
				case ValidationScheme.Russian:
				case ValidationScheme.Japanese:
				case ValidationScheme.Korean:
				case ValidationScheme.SimplifiedChinese:
				default:
					break;
				}

				LastChar = C;
            }

			if( Value.Length > 1 )
			{
				if( ParensCount != 0 )
				{
					return ( ValidationError.MismatchedParentheses );
				}

				if( CurlyBracketCount != 0 )
				{
					return ( ValidationError.MismatchedCurlyBrackets );
				}

				if( SquareBracketCount != 0 )
				{
					return ( ValidationError.MismatchedSquareBrackets );
				}

				if( TagCount != 0 )
				{
					return ( ValidationError.MismatchedTags );
				}
			}

            return ( ValidationError.NoError );
        }

        private ValidationError ValidateWesternEuro( LanguageInfo Lang, ref string Value )
        {
            FixupEllipses( Lang, ref Value, false );
            FixupDashes( Lang, ref Value, false );

            return ( MatchPairs( ref Value, ValidationScheme.WesternEuro ) );
        }

        private ValidationError ValidateRussian( LanguageInfo Lang, ref string Value )
        {
            FixupEllipses( Lang, ref Value, false );
            FixupDashes( Lang, ref Value, false );

			return ( MatchPairs( ref Value, ValidationScheme.Russian ) );
        }

        private ValidationError ValidateEasternEuro( LanguageInfo Lang, ref string Value )
        {
            FixupEllipses( Lang, ref Value, false );
            FixupDashes( Lang, ref Value, false );

			return ( MatchPairs( ref Value, ValidationScheme.EasternEuro ) );
        }

        private ValidationError ValidateChinese( LanguageInfo Lang, ref string Value )
        {
            FixupEllipses( Lang, ref Value, true );
            FixupDashes( Lang, ref Value, true );

			return ( MatchPairs( ref Value, ValidationScheme.SimplifiedChinese ) );
        }

        private ValidationError ValidateKorean( LanguageInfo Lang, ref string Value )
        {
            FixupEllipses( Lang, ref Value, true );
            FixupDashes( Lang, ref Value, true );

			return ( MatchPairs( ref Value, ValidationScheme.Korean ) );
        }

		public string Validate( LanguageInfo Lang, ref LocEntry LE, bool bOuterQuotesRequired )
        {
            ValidationError ReturnCode = ValidationError.NoError;
            string Value = LE.Value;

			if( Value.Length > 0 )
			{
				Offset = 0;
				BadChar = '0';
				DoValidateTags = false;

				FixupTabs( Lang, ref Value );
				FixupSmartQuotes( Lang, ref Value );
				FixupSmartSingleQuotes( Lang, ref Value );
				FixupDoubleSingleQuotes( Lang, ref Value );

				switch( Lang.LangID )
				{
				case "INT":
				case "FRA":
				case "ITA":
				case "DEU":
				case "ESN":
				case "ESM":
				case "PTB":
					ReturnCode = ValidateWesternEuro( Lang, ref Value );
					break;

				case "RUS":
					ReturnCode = ValidateRussian( Lang, ref Value );
					break;

				case "HUN":
				case "POL":
				case "CZE":
				case "SLO":
					ReturnCode = ValidateEasternEuro( Lang, ref Value );
					break;

				case "CHN":
					ReturnCode = ValidateChinese( Lang, ref Value );
					break;

				case "JPN":
					break;

				case "KOR":
					ReturnCode = ValidateKorean( Lang, ref Value );
					break;

				default:
					break;
				}

				if( ReturnCode == ValidationError.NoError )
				{
					ReturnCode = FixupQuotes( Lang, ref Value, bOuterQuotesRequired );
				}

				LE.Value = Value;

				// Return if we have found an error so far
				if( ReturnCode != ValidationError.NoError )
				{
					// Check for special exceptions
					foreach( string Allowed in Main.Options.AllowedStrings )
					{
						if( Value.Contains( Allowed ) )
						{
							return ( ValidationError.NoError.ToString() );
						}
					}

					string ReturnString = ReturnCode.ToString();
					switch( ReturnCode )
					{
					case ValidationError.IllegalCharacter:
						ReturnString += " '" + BadChar + "' (0x" + ( ( int )BadChar ).ToString( "X" ) + ") at offset " + Offset.ToString() + " in '" + Value + "' in '" + LE.Owner.Owner.Owner.Owner.RelativeName + "' at line " + LE.LocEntryLineNumber.ToString();
						break;

					case ValidationError.MismatchedCurlyBrackets:
					case ValidationError.MismatchedParentheses:
					case ValidationError.MismatchedQuotes:
					case ValidationError.MismatchedSquareBrackets:
					case ValidationError.MismatchedTagCount:
					case ValidationError.MismatchedTags:
					case ValidationError.LocalisedTags:
						ReturnString += " in '" + Value + "' in '" + LE.Owner.Owner.Owner.Owner.RelativeName + "' at line " + LE.LocEntryLineNumber.ToString();
						break;
					}

					Main.Error( Lang, ReturnString );
					return ( ReturnString );
				}

				if( DoValidateTags )
				{
					// Make sure the tags have not been localised
					ReturnCode = ValidateTags( LE );
				}
			}

            return ( ReturnCode.ToString() );
        }
    }
}
