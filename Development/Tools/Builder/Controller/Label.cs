// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Text;

namespace Controller
{
    public class LabelInfo
    {
        // Hook back to main controller
        private Main Parent = null;

        // List of games that this build represents
        public List<GameConfig> Games = new List<GameConfig>();

		// List of games that had suppressed errors
		public List<GameConfig> FailedGames = new List<GameConfig>();

        // Name of the platform this label refers to - blank is all platforms
        public RevisionType RevisionType { get; set; }

        // The branch this revision lives in
        public string Branch { get; set; }

        // Name of the game this label refers to - blank is all games
        public string Game { get; set; }

		// Name of the game this label refers to - blank is all games
		public bool bIsTool { get; set; }

        // Name of the platform this label refers to - blank is all platforms
        public string Platform { get; set; }

		// Name of the subplatform this label refers to - used to disambiguate between devkits and testkits
		public string SubPlatform { get; set; }

        // Timestamp of this label
        public DateTime Timestamp { get; set; }

        // The changelist of this revision
        public int Changelist { get; set; }

        // The four digit version to apply to Windows binaries
        public Version BuildVersion { get; set; }
        
        // A list of defines used to compile this build
        public List<string> Defines { get; set; }

        // The language of this revision (if any)
        public string Language { get; set; }

        // The type of build being built
        public string BuildType { get; set; }

		// The perforce label description
		public string Description { get; set; }

        public LabelInfo( Main InParent, BuildState Builder )
        {
            RevisionType = RevisionType.Invalid;
			Branch = InParent.LabelRoot;
			Game = "";
			bIsTool = false;
			Platform = "";
			Changelist = 0;
			BuildVersion = new Version( 1, 0, 0, 131 );
			Defines = new List<string>();
			Language = "";
			BuildType = "";
			Description = "";

			Parent = InParent;
			Branch = Builder.BranchDef.Branch;
			Timestamp = Builder.CommandDetails.BuildStarted;
		}

        public List<GameConfig> GetGameConfigs()
        {
            return ( Games );
        }

        public int SafeStringToInt( string Number )
        {
            int Result = 0;

            try
            {
                Number = Number.Trim();
                Result = Int32.Parse( Number );
            }
            catch
            {
            }

            return ( Result );
        }

        public string GetRootLabelName()
        {
			DateTime LocalTimeStamp = Timestamp.ToLocalTime();
			string TimeStampString = LocalTimeStamp.Year + "-"
						+ LocalTimeStamp.Month.ToString( "00" ) + "-"
						+ LocalTimeStamp.Day.ToString( "00" ) + "_"
						+ LocalTimeStamp.Hour.ToString( "00" ) + "."
						+ LocalTimeStamp.Minute.ToString( "00" );

            string LabelName = Branch + "_[" + TimeStampString + "]";

            return ( LabelName );
        }

        // Gets a suitable label for using in Perforce
        public string GetLabelName()
        {
            string LabelName = GetRootLabelName();

			foreach( string Define in Defines )
			{
				LabelName += "_[" + Define + "]";
			}

            return ( LabelName );
        }

        public void ClearDefines()
        {
            Defines.Clear();
        }

        public void AddDefine( string Define )
        {
			if( Define.Length > 0 )
			{
				if( !Defines.Contains( Define ) )
				{
					Defines.Add( Define );
				}
			}
        }

		public string GetDefinesAsString()
		{
			string DefineString = "";
			foreach( string Define in Defines )
			{
				DefineString += Define + " ";
			}
			DefineString = DefineString.Trim();
			return ( DefineString );
		}

        // Extract a define (of potentially two) from a build label
        private string GetDefine( string Label )
        {
            int OpenBracketIndex, CloseBracketIndex;
            string Define = "";

            OpenBracketIndex = Label.IndexOf( '[' );
            if( OpenBracketIndex > 0 )
            {
                CloseBracketIndex = Label.IndexOf( ']', OpenBracketIndex );
                if( CloseBracketIndex > 0 )
                {
                    Define = Label.Substring( OpenBracketIndex + 1, CloseBracketIndex - OpenBracketIndex - 1 );
                }
            }

            return ( Define );
        }

		private List<string> GetDefines( string Label )
		{
			List<string> NewDefines = new List<string>();

			while( Label.Length > "_[]".Length )
			{
				string NewDefine = GetDefine( Label );
				NewDefines.Add( NewDefine );
				Label = Label.Substring( NewDefine.Length + "_[]".Length );
			}

			return ( NewDefines );
		}

        // Sets up the label data from an existing Perforce label
        private bool LabelFromLabel( string Label, bool bBranchMustMatch )
        {
            int OpenBracketIndex;

            OpenBracketIndex = Label.IndexOf( '[' );
            if( OpenBracketIndex >= 1 )
            {
                // Extract the branch
                string ExtractedBranch = Label.Substring( 0, OpenBracketIndex - 1 );
#if !DEBUG
				if( !bBranchMustMatch || ExtractedBranch == Branch )
#endif
				{
                    Label = Label.Substring( OpenBracketIndex );

                    if( Label.Length >= "[YYYY-MM-DD_HH.MM]".Length )
                    {
                        // Extract the timestamp
                        string TimeStampString = Label.Substring( 1, "YYYY-MM-DD_HH.MM".Length );

                        int Year = SafeStringToInt( TimeStampString.Substring( 0, 4 ) );
                        int Month = SafeStringToInt( TimeStampString.Substring( 5, 2 ) );
                        int Day = SafeStringToInt( TimeStampString.Substring( 8, 2 ) );
                        int Hour = SafeStringToInt( TimeStampString.Substring( 11, 2 ) );
                        int Minute = SafeStringToInt( TimeStampString.Substring( 14, 2 ) );

                        Timestamp = new DateTime( Year, Month, Day, Hour, Minute, 0 ).ToUniversalTime();

                        Label = Label.Substring( "[YYYY-MM-DD_HH.MM]".Length );

                        // Extract the defines (if any)
                        Defines = GetDefines( Label );
                        return ( true );
                    }
                }
            }
            return ( false );
        }

        // Gets a suitable folder name for publishing data to, or getting data from
		public string GetFolderName( string GameNameOverride, bool IncludePlatform, int BranchVersion )
        {
			DateTime LocalTimeStamp = Timestamp.ToLocalTime();
			string TimeStampString = LocalTimeStamp.Year + "-"
						+ LocalTimeStamp.Month.ToString( "00" ) + "-"
						+ LocalTimeStamp.Day.ToString( "00" ) + "_"
						+ LocalTimeStamp.Hour.ToString( "00" ) + "."
						+ LocalTimeStamp.Minute.ToString( "00" );

			string FolderName = Game + "_";
			if( GameNameOverride.Length > 0 )
			{
				FolderName = GameNameOverride + "_";
			}

			if( IncludePlatform )
			{
				GameConfig Config = new GameConfig( Game, Platform, "UnusedGameConfiguration", null, true, true );
				FolderName += Config.GetCookedFolderPlatform() + "_";
			}

			FolderName += "[" + TimeStampString + "]";

			foreach( string Define in Defines )
			{
				FolderName += "_[" + Define + "]";
			}

            return( FolderName );
        }

        // Sets up the label data from an existing folder name
        private bool LabelFromFolder( string Folder, bool ExtractPlatform )
        {
            int UnderscoreIndex;

            UnderscoreIndex = Folder.IndexOf( '_' );
            if( UnderscoreIndex > 0 )
            {
                // Extract the branch
                Game = Folder.Substring( 0, UnderscoreIndex );
                Folder = Folder.Substring( UnderscoreIndex + 1 );

                UnderscoreIndex = Folder.IndexOf( '_' );
                if( UnderscoreIndex > 0 )
                {
					if( ExtractPlatform )
					{
						Platform = Folder.Substring( 0, UnderscoreIndex );
						Folder = Folder.Substring( UnderscoreIndex + 1 );
					}

                    if( Folder.Length >= "[YYYY-MM-DD_HH.MM]".Length )
                    {
                        // Extract the timestamp
                        string TimeStampString = Folder.Substring( 1, "YYYY-MM-DD_HH.MM".Length );

                        int Year = SafeStringToInt( TimeStampString.Substring( 0, 4 ) );
                        int Month = SafeStringToInt( TimeStampString.Substring( 5, 2 ) );
                        int Day = SafeStringToInt( TimeStampString.Substring( 8, 2 ) );
                        int Hour = SafeStringToInt( TimeStampString.Substring( 11, 2 ) );
                        int Minute = SafeStringToInt( TimeStampString.Substring( 14, 2 ) );

                        Timestamp = new DateTime( Year, Month, Day, Hour, Minute, 0 ).ToUniversalTime();

                        Folder = Folder.Substring( "[YYYY-MM-DD_HH.MM]".Length );

                        // Extract the defines (if any)
                        Defines = GetDefines( Folder );
                        return ( true );
                    }
                }
            }

            return ( false );
        }

        private int CountGames( List<GameConfig> GameList, GameConfig InGame )
        {
            int Count = 0;
            foreach( GameConfig Game in GameList )
            {
                if( Game.Similar( InGame ) )
                {
                    Count++;
                }
            }

            return ( Count );
        }

        public List<GameConfig> ConsolidateGames()
        {
            List<GameConfig> NewGames = new List<GameConfig>();
            List<GameConfig> MiscGames = new List<GameConfig>();

            // Create a list of games that are compiled for all platforms
            for( int i = 0; i < Games.Count; i++ )
            {
                if( CountGames( NewGames, Games[i] ) == 0 )
                {
                    int GameCount = CountGames( Games, Games[i] );
                    if( GameCount >= 2 )
                    {
                        GameConfig NewGame = new GameConfig( GameCount, Games[i] );
                        NewGames.Add( NewGame );
                    }
                }
            }

            // Copy anything left over
            for( int i = 0; i < Games.Count; i++ )
            {
                if( CountGames( NewGames, Games[i] ) == 0 )
                {
                    MiscGames.Add( Games[i] );
                }
            }

            NewGames.AddRange( MiscGames );
            return ( NewGames );
        }

        // Creates the description string from the available data
        public void CreateLabelDescription()
        {
            // Otherwise, build a string of the configs built using what
            StringBuilder DescriptionBuilder = new StringBuilder();

            DescriptionBuilder.Append( "[BUILDER] '" + BuildType + "' built from changelist: " + Changelist.ToString() + Environment.NewLine + Environment.NewLine );

            DescriptionBuilder.Append( "Engine version: " + BuildVersion.Build.ToString() + Environment.NewLine + Environment.NewLine );

			// Output a consolidated version of the games/platforms/configurations built
            DescriptionBuilder.Append( "Configurations built:" + Environment.NewLine );
            if( Games.Count > 0 )
            {
                List<GameConfig> ConsolidatedGames = ConsolidateGames();

                foreach( GameConfig Config in ConsolidatedGames )
                {
                    DescriptionBuilder.Append( "\t" + Config.ToString() + Environment.NewLine );
                }
            }
            else
            {
                DescriptionBuilder.Append( "\tNone!" + Environment.NewLine );
            }

			// Output each failed job in sequence
			if( FailedGames.Count > 0 )
			{
				DescriptionBuilder.Append( Environment.NewLine + "Configurations that failed:" + Environment.NewLine );

				foreach( GameConfig Config in FailedGames )
				{
					DescriptionBuilder.Append( "\t" + Config.ToString() + Environment.NewLine );
				}
			}

			// Output the SDK versions used for this build
			DescriptionBuilder.Append( Environment.NewLine + "DirectX version: " + Parent.DXVersion + Environment.NewLine );
			// DescriptionBuilder.Append( "Android version: " + Parent.AndroidSDKVersion + Environment.NewLine );
			// DescriptionBuilder.Append( "iPhone version : " + Parent.iPhoneSDKVersion + Environment.NewLine );
			// DescriptionBuilder.Append( "Mac version    : " + Parent.MacSDKVersion + Environment.NewLine );
			DescriptionBuilder.Append( "NGP version    : " + Parent.NGPSDKVersion + Environment.NewLine );
			DescriptionBuilder.Append( "PS3 version    : " + Parent.PS3SDKVersion + Environment.NewLine );
			DescriptionBuilder.Append( "WiiU version   : " + Parent.WiiUSDKVersion + Environment.NewLine );
			DescriptionBuilder.Append( "Xbox360 version: " + Parent.Xbox360SDKVersion + Environment.NewLine );
			DescriptionBuilder.Append( "Flash version  : " + Parent.FlashSDKVersion + Environment.NewLine );

            Description = DescriptionBuilder.ToString();
        }

        // Extract integers from the label description
        private int GetInteger( string Key, string Info )
        {
            int LabelInteger = 0;

            if( Info != null )
            {
                int Index = Info.IndexOf( Key );
                if( Index >= 0 )
                {
                    string ListText = Info.Substring( Index + Key.Length ).Trim();
                    string[] Parms = ListText.Split( " \t\r\n".ToCharArray() );
                    if( Parms.Length > 0 )
                    {
                        LabelInteger = SafeStringToInt( Parms[0] );
                    }
                }
            }

            return ( LabelInteger );
        }

        // Callback from GetLabelInfo()
        public void HandleDescription( string InDescription )
        {
            // Get the changelist this was built from
            Changelist = GetInteger( "changelist:", InDescription );
            // Get the engine version of the label
            BuildVersion = new Version( BuildVersion.Major,
                                        BuildVersion.Minor,
                                        GetInteger( "Engine version:", InDescription ),
                                        BuildVersion.Revision );

            Description = InDescription;
        }

        // Create a new instance of a label, extract info from the name and then from Perforce
		public void Init( P4 SCC, BuildState Builder )
		{
			bool ValidLabel = false;

			// No cross branch operations
			Branch = Builder.BranchDef.Branch;

			// Work out what type of dependency and extract the relevant data
			string Dependency = Builder.Dependency;
			if( Dependency.Length > 0 )
			{
				// Dependency could be head, a changelist, a Perforce label, or a folder name derived from a Perforce label, or a generic named label
				if( Dependency == "#head" )
				{
					// Just sync to #head, no data extraction required
					RevisionType = RevisionType.Head;
					ValidLabel = true;
				}
				else if( Dependency.StartsWith( Branch ) )
				{
					Parent.Log( " ... using local label.", Color.DarkGreen );
					RevisionType = RevisionType.Label;
					ValidLabel = LabelFromLabel( Dependency, true );
				}
				else if( Dependency.StartsWith( "UnrealEngine3" ) || Dependency.StartsWith( "UE4" ) )
				{
					Parent.Log( " ... using label from alternate branch.", Color.DarkGreen );
					RevisionType = RevisionType.SimpleLabel;
					ValidLabel = LabelFromLabel( Dependency, false );
				}
				else
				{
					Parent.Log( " ... using label from folder.", Color.DarkGreen );
					RevisionType = RevisionType.Label;
					ValidLabel = LabelFromFolder( Dependency, Builder.IncludePlatform );
				}

				if( ValidLabel )
				{
					// Update the remaining info from perforce
					switch( RevisionType )
					{
					case RevisionType.Head:
					default:
						break;

					case RevisionType.Label:
						// Always use the root label name as everything is derived from that
						ValidLabel &= SCC.GetLabelInfo( Builder, GetRootLabelName(), this );
						if( ValidLabel )
						{
							Parent.Log( "Label '" + GetLabelName() + "' successfully interrogated.", Color.DarkGreen );
						}
						break;

					case RevisionType.SimpleLabel:
						ValidLabel &= SCC.GetLabelInfo( Builder, Dependency, this );
						if( ValidLabel )
						{
							Parent.Log( "Label '" + Dependency + "' successfully interrogated.", Color.DarkGreen );
							// Promote to a type that the builder knows has a valid changelist
							RevisionType = RevisionType.BuilderLabel;
						}
						break;
					}
				}
				else
				{
					// We have a label name, just not in the format of a build label or publish folder
					// Check for integer changelist here
					// No way to check to see if a label exists without creating it

					// Set default timestamp
					Timestamp = DateTime.UtcNow;

					Parent.Log( "Label '" + Dependency + "' is being used.", Color.DarkGreen );
					RevisionType = RevisionType.SimpleLabel;

					ValidLabel = true;
				}
			}

			if( !ValidLabel )
			{
				Parent.Log( "Label '" + GetLabelName() + "' could not be found.", Color.Red );
				RevisionType = RevisionType.Invalid;
			}
		}

        public void Touch()
        {
			Timestamp = DateTime.UtcNow;
        }

        public void Init( BuildState Builder, RevisionType InRevisionType )
        {
			BuildType = Builder.CommandDetails.Description;

            // No cross branch operations
			Branch = Builder.BranchDef.Branch;

            // Set the changelist
			Changelist = Builder.CommandDetails.LastAttemptedBuild;

			RevisionType = InRevisionType;
        }
    }
}
