/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;

namespace UnrealLoc
{
    public class LocEntry
    {
        private UnrealLoc Main = null;
        public LocEntryHandler Owner = null;

        public LocEntry DefaultLE { get; set; }
        public string Key { get; set; }
        public string Value { get; set; }
		public int LocEntryLineNumber { get; set; }
		public bool DirtyEntry { get; set; }
		public bool DefaultExists { get; set; }

        private List<LocEntry> LocalSubKeys = new List<LocEntry>();
        public List<LocEntry> SubKeys
        {
            get { return ( LocalSubKeys ); }
            set { LocalSubKeys = value; }
        }

        private string[] ParseTokens( string TokenList )
        {
            List<string> Tokens = new List<string>();
            string Token = "";
            bool bInQuotes = false;
			int InParens = 0;
            char LastLetter = '\\';

            foreach( char Letter in TokenList )
            {
                if( Letter == '(' )
                {
                    InParens++;
                    Token += Letter;
                }
                else if( Letter == ')' )
                {
                    InParens--;
                    Token += Letter;
                }
                else if( ( InParens == 0 ) && Letter == '\"' && LastLetter != '\\' )
                {
                    Token += Letter;
                    bInQuotes = !bInQuotes;
                }
                else if( ( InParens == 0 ) && ( Letter == ',' || Letter == '=' ) )
                {
                    if( bInQuotes )
                    {
                        Token += Letter;
                    }
                    else
                    {
                        Tokens.Add( Token );
                        Token = "";
                    }
                }
                else
                {
                    Token += Letter;
                }

                LastLetter = Letter;
            }

            // Should add even if blank for the 'Key=' case
            Tokens.Add( Token );

            return ( Tokens.ToArray() );
        }

        public void ExpandSubKeys( LanguageInfo Lang )
        {
            // Check for sub keys
            if( Value.StartsWith( "(" ) && Value.Contains( "=" ) )
            {
                // Fix up any missing ")"
                if( !Value.Contains( ")" ) )
                {
                    Value += ")";
                    Main.Warning( Lang, "Adding missing ')' to '" + Value + "' at line " + Owner.Owner.Owner.LineCount + " in '" + Owner.Owner.Owner.Owner.RelativeName + "'" );
                }

                // Trim the outer parens to get the value
                string SubValueString = Value.Substring( 1, Value.LastIndexOf( ')' ) - 1 );
                // Clear out the original value if it's made up of substrings
                Value = "";

                // Value could be several comma delimited - need to handle quotes properly
                string[] SubKeyValues = ParseTokens( SubValueString );

                for( int Index = 0; Index < SubKeyValues.Length; )
                {
                    // For 'ProfileMappings[0]=(ValueMappings=((Name="Off"),(Name="On")))'
                    if( SubKeyValues[Index].StartsWith( "(" ) )
                    {
                        string SubKey = "";
                        string SubValue = SubKeyValues[Index];

                        Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ............... added sub ('" + SubValue + "')", Color.Black );

                        LocEntry SubEntry = new LocEntry( Main, Owner, Lang, SubKey, SubValue );
                        SubKeys.Add( SubEntry );

                        Index++;
                    }
                    else
                    {
                        if( Index + 1 < SubKeyValues.Length )
                        {
                            // For 'NavigationItems[0]=(ItemTag="Collectables",DisplayName="Collectables",ItemHelpText="")'
                            string SubKey = SubKeyValues[Index];
                            string SubValue = SubKeyValues[Index + 1];

                            Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ............ added sub '" + SubKey + "' = '" + SubValue + "'", Color.Black );

                            LocEntry SubEntry = new LocEntry( Main, Owner, Lang, SubKey, SubValue );
                            SubKeys.Add( SubEntry );
                        }
                        else
                        {
                            Main.Warning( Lang, "Failed to parse subkeys in line " + Owner.Owner.Owner.LineCount + " in '" + Owner.Owner.Owner.Owner.RelativeName + "'" );
                        }

                        Index += 2;
                    }
                }
            }
        }

        private string GetLine()
        {
            string Line = "";

            if( Key.Length > 0 )
            {
                Line += Key + "=";
            }

            if( SubKeys.Count > 0 )
            {
                Line += "(";
                int Index = 0;
                foreach( LocEntry LE in SubKeys )
                {
                    if( Index > 0 )
                    {
                        Line += ",";
                    }
                    Line += LE.GetLine();
                    Index++;
                }
                Line += ")";
            }
            else
            {
                Line += Value;
            }

            return ( Line );
        }

        public bool WriteLocFiles( StreamWriter File )
        {
            string Line = GetLine();
            Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, "Writing '" + Line + "'", Color.Black );
            File.WriteLine( Line );
            return ( true );
        }

        // Make sure the subkeys match up
        public void Validate( LanguageInfo Lang, LocEntry DefLE )
        {
            if( DefLE != null && SubKeys.Count != DefLE.SubKeys.Count )
            {
                Main.Error( Lang, "Mismatched subkey count - " + SubKeys.Count + " vs. " + DefLE.SubKeys.Count + " for '" + Key + "' at line " + Owner.Owner.Owner.LineCount + " in '" + Owner.Owner.Owner.Owner.RelativeName + "'" );

                SubKeys.Clear();
                foreach( LocEntry LE in DefLE.SubKeys )
                {
                    SubKeys.Add( LE );
                }
            }
            else if( DefLE != null && Key != DefLE.Key )
            {
				bool bValueQuoted = ( Value.StartsWith( "\"" ) && Value.EndsWith(  "\"" ) );
				bool bDefValueQuoted = ( DefLE.Value.StartsWith( "\"" ) && DefLE.Value.EndsWith( "\"" ) );

				if( bValueQuoted == bDefValueQuoted )
				{
					Main.Warning( Lang, "Mismatched key name! Changing '" + Key + "' to '" + DefLE.Key + "'" );
					Key = DefLE.Key;
				}
				else
				{
					Main.Error( Lang, "Mismatched key name! '" + Key + "' is not '" + DefLE.Key + "' at line " + Owner.Owner.Owner.LineCount + " in '" + Owner.Owner.Owner.Owner.RelativeName + "'" );
				}
            }
            else
            {
                for( int Index = 0; Index < SubKeys.Count; Index++ )
                {
                    LocEntry SubEntry = SubKeys[Index];
                    if( DefLE != null )
                    {
                        SubEntry.Validate( Lang, DefLE.SubKeys[Index] );
                    }
                    Lang.Validate( ref SubEntry, true );
                    SubKeys[Index] = SubEntry;
                }
            }
        }

        public void Update( LanguageInfo Lang, string InValue )
        {
            Value = InValue;

            SubKeys.Clear();
            ExpandSubKeys( Lang );
        }

        public LocEntry( UnrealLoc InMain, LocEntryHandler InOwner, LocEntry LE )
        {
            Main = InMain;
            Owner = InOwner;
            DefaultLE = LE;
            Key = DefaultLE.Key;
            Value = DefaultLE.Value;
			SubKeys = new List<LocEntry>( DefaultLE.SubKeys );
			LocEntryLineNumber = 0;
            DirtyEntry = true;
        }

        public LocEntry( UnrealLoc InMain, LocEntryHandler InOwner, LanguageInfo Lang, string InKey, string InValue )
        {
            Main = InMain;
            Owner = InOwner;
            Key = InKey;
            Value = InValue;
            DirtyEntry = false;
			LocEntryLineNumber = Owner.Owner.Owner.LineCount;

            ExpandSubKeys( Lang );
        }
    }

    public class LocEntryHandler
    {
        private UnrealLoc Main = null;
        private LanguageInfo Lang = null;
        private List<LocEntry> LocEntries;

        public ObjectEntry Owner = null;

        public LocEntryHandler( UnrealLoc InMain, LanguageInfo InLang, ObjectEntry InOwner )
        {
            Main = InMain;
            Lang = InLang;
            Owner = InOwner;
            LocEntries = new List<LocEntry>();
        }

        public List<LocEntry> GetLocEntries()
        {
            return ( LocEntries );
        }

        public int GetCount()
        {
            return ( LocEntries.Count );
        }

        private LocEntry CreateLoc( LocEntry DefaultLE )
        {
            LocEntry LocElement = new LocEntry( Main, this, DefaultLE );
            LocEntries.Add( LocElement );

            Lang.LocCreated++;
            Owner.HasNewLocEntries = true;
            Owner.Owner.Owner.HasNewLocEntries = true;

            Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ... created loc '" + LocElement.Key + "'", Color.Blue );

            return ( LocElement );
        }

        private LocEntry LocExists( string DefaultKeyName )
        {
			// FIXME: Linear search
            foreach( LocEntry LE in LocEntries )
            {
                if( LE.Key == DefaultKeyName )
                {
                    return ( LE );
                }
            }

            return ( null );
        }

        public bool GenerateLocEntries( ObjectEntry DefaultObjectEntry )
        {
            List<LocEntry> DefaultLocEntries = DefaultObjectEntry.GetLocEntries();

            foreach( LocEntry DefaultLE in DefaultLocEntries )
            {
                LocEntry LocLE = LocExists( DefaultLE.Key );
                if( LocLE == null )
                {
                    LocLE = CreateLoc( DefaultLE );
                }
                else if( Lang != Main.DefaultLanguageInfo )
                {
                    // Set the reference LocEntry
                    LocLE.DefaultLE = DefaultLE;

                    // If same as the INT (default) version, value is unlocalised
                    if( LocLE.Value == DefaultLE.Value )
                    {
                        LocLE.DirtyEntry = true;
                    }
                }

                LocLE.DefaultExists = true;

                Lang.Validate( ref LocLE, false );
            }

            return ( true );
        }

        public void RemoveOrphans()
        {
            List<LocEntry> LEs = LocEntries;
            LocEntries = new List<LocEntry>();

            foreach( LocEntry LE in LEs )
            {
                if( LE.DefaultExists || !Main.Options.bRemoveOrphans )
                {
                    LocEntries.Add( LE );
					Main.Log( UnrealLoc.VerbosityLevel.Complex, " ... retaining orphan: '" + LE.Key + "' from object '[" + Owner.ObjectName + "]'", Color.Black );
				}
                else
                {
                    Main.Warning( Lang, "Removed orphan: '" + LE.Key + "' from object '[" + Owner.ObjectName + "]' for file '" + Owner.Owner.Owner.RelativeName + "' at line: " + Owner.Owner.LineCount );
                }
            }

            Lang.NumOrphansRemoved += LEs.Count - LocEntries.Count;
        }

        public bool WriteLocFiles( StreamWriter File )
        {
            foreach ( LocEntry LE in LocEntries )
            {
                Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ... creating loc entry: " + LE.Key, Color.Black );
                LE.WriteLocFiles( File );
            }

            return ( true );
        }

        public bool WriteDiffLocFiles( StreamWriter File )
        {
            foreach( LocEntry LE in LocEntries )
            {
                if( LE.DirtyEntry )
                {
                    Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ... creating loc entry: " + LE.Key, Color.Black );
                    LE.WriteLocFiles( File );
                }
            }

            return ( true );
        }

        public bool AddLocEntry( string Line )
        {
            int Index = Line.IndexOf( '=' );
            if( Index > 0 )
            {
                string Key = Line.Substring( 0, Index );
                string Value = Line.Substring( Index + 1, Line.Length - Index - 1 );

                LocEntry LE = LocExists( Key );
                if( LE == null )
                {
                    Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ......... added '" + Key + "' = '" + Value + "'", Color.Black );

                    LocEntry LocElement = new LocEntry( Main, this, Lang, Key, Value );
                    LocEntries.Add( LocElement );
                    return ( true );
                }
                else
                {
                    Main.Warning( Lang, "Duplicate key '" + Key + "' in '[" + Owner.ObjectName + "]' for file '" + Owner.Owner.Owner.RelativeName + "' at line: " + Owner.Owner.LineCount );
                }
            }
            else
            {
                Main.Error( Lang, "No '=' found in line '" + Line + "' in '[" + Owner.ObjectName + "]' for file '" + Owner.Owner.Owner.RelativeName + "' at line: " + Owner.Owner.LineCount );
            }

            return ( false );
        }

        public bool ReplaceLocEntry( string Line, bool AddNew )
        {
            int Index = Line.IndexOf( '=' );
            if( Index > 0 )
            {
                string Key = Line.Substring( 0, Index );
                string Value = Line.Substring( Index + 1, Line.Length - Index - 1 );

                LocEntry LE = LocExists( Key );
                if( LE != null )
                {
                    LE.Update( Lang, Value );
                    Lang.Validate( ref LE, false );

                    Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ......... updated '" + Key + "' = '" + LE.Value + "'", Color.Black );

                    return ( true );
                }
                else if( AddNew )
                {
                    LocEntry LocElement = new LocEntry( Main, this, Lang, Key, Value );
                    LocEntries.Add( LocElement );

                    Main.Log( UnrealLoc.VerbosityLevel.ExtraVerbose, " ......... added '" + Key + "' = '" + Value + "'", Color.Black );

                    return ( true );
                }

                Main.Warning( Lang, "Key '" + Key + "' in '[" + Owner.ObjectName + "]' for file '" + Owner.Owner.Owner.RelativeName + "' at line " + Owner.Owner.LineCount + " does not exist in INT." );
            }
            else
            {
                Main.Error( Lang, "No '=' found in line '" + Line + "' in '[" + Owner.ObjectName + "]' for file '" + Owner.Owner.Owner.RelativeName + "' at line: " + Owner.Owner.LineCount );
            }

            return ( false );
        }
    }
}
