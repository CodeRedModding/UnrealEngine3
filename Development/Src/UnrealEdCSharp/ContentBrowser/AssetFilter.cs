//=============================================================================
//	AssetFilter.cs: Content browser asset filter state
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Text;
using System.Collections.ObjectModel;
using System.Text.RegularExpressions;


namespace ContentBrowser
{

	/// Describes preparsed information about each token in the text-based query
	[FlagsAttribute]
	enum TokenStatus : int
	{
		None = 0,
		KnownType = 1 << 0,
		KnownTag = 1 << 1,
		Negated = 1 << 2,
	}

	/// Syntax sugar for TokenStatus
	static class TokenStatusUtil
	{
		public static bool IsKnownType( this TokenStatus Status ) { return ( Status & TokenStatus.KnownType ) != TokenStatus.None; }
		public static bool IsKnownTag( this TokenStatus Status ) { return ( Status & TokenStatus.KnownTag ) != TokenStatus.None; }
		public static bool IsNegated( this TokenStatus Status ) { return ( Status & TokenStatus.Negated ) != TokenStatus.None; }
	}


	/// <summary>
	/// An AssetFilter describes how the assets should be refined.
	/// </summary>
	public class AssetFilter
		: ICloneable
	{
		private readonly static char NegatingToken = '-';
		private readonly static String NegatingTokenStr = "-";
		public enum AllVsAny : int
		{
			All = 0,
			Any = 1,
		}

		public enum FilterField : int
		{
			Tags = 0,
			Types = 1,
			Path = 2,
			Name = 3,
		}

		public enum TaggedState : int
		{			
			TaggedAndUntagged = 0,
			TaggedOnly = 1,
			UntaggedOnly = 2,
		}

		public enum LoadedState : int
		{
			LoadedAndUnloaded = 0,
			LoadedOnly = 1,
			UnloadedOnly = 2,
		}

		/// <summary>
		/// Construct an AssetFilter
		/// </summary>
		/// <param name="Backend">The Backend for which filter will happen</param>
		public AssetFilter( MainControl InMainControl)
		{
			mMainControl = InMainControl;
			mBackend = InMainControl.Backend;
			

			ObjectTypeDescriptions = new List<String>(0);
			SetTagTiers( new List<List<String>>(0) ) ;
		}

		/// Reference to a backend whose content the ContentBrowser is browsing.
		IContentBrowserBackendInterface mBackend;

		/// Reference to the top-level control of the content browser.
		MainControl mMainControl;

		/// The search string entered by a user
		private String mSearchString = String.Empty;
		/// Tokenized substring search terms
		private List<String> mSearchTokens = new List<String>();
		/// A list of flags signifying if each token is a Type name.
		private List<TokenStatus> mTokenStatus = new List<TokenStatus>();

		/// Tokenized exact match search terms.
		private List<String> mExactSearchAtoms = new List<String>();

		/// List of types which are known to the asset filter (used to determine if a token is a type)
		public List<String> KnownTypes { private get; set; }

		/// List of types which are known to the asset filter (used to determine if a token is a type)
		public List<String> KnownTags { private get; set; }

		/// The search string entered by a user
		public String SearchString
		{
			get { return mSearchString; }
			set
			{
				mSearchString = value;
				
				// Lowcase all queries for easier case-insensitive searches
				mSearchString = mSearchString.ToLower();
				ParseSearchString( mSearchString, mSearchTokens, mExactSearchAtoms );
				DiscoverTokenStatus( );
			}
		}

		/// <summary>
		/// List of assets that are the input into a given tier.
		/// </summary>
		/// <param name="TierIndex"></param>
		/// <returns>List of assets input into a given tier.</returns>
		public List<AssetItem> GetInputToTier( int TierIndex )
		{			
			return TierInputs[TierIndex];	
		}

		/// Reset the cached assets that pass various filter tiers.
		private void ResetAssetsInTiers()
		{
			TierInputs = new List<List<AssetItem>>(TagTiers.Count);
			for ( int TierIndex = 0; TierIndex < TagTiers.Count; ++TierIndex )
			{
				TierInputs.Add( new List<AssetItem>() );
			};
		}

		// Mark any tokens that are complete names of types, so we can treat them specially during the search. (see MasterFilterMethod)
		private void DiscoverTokenStatus( )
		{
			mTokenStatus.Clear();			
			for ( int TokenIndex = 0; TokenIndex < mSearchTokens.Count; ++TokenIndex)
			{
				String CurToken = mSearchTokens[TokenIndex];

				TokenStatus Status = TokenStatus.None;
				// Test if this toekn is Negated. If so update it to not include the negation character
				if ( CurToken.Length > 0 && CurToken[0] == NegatingToken)
				{
					Status |= TokenStatus.Negated;
					mSearchTokens[TokenIndex] = CurToken = CurToken.Substring( 1 );
				}

				// Test if this token is a KnownType.
				if ( UnrealEd.Utils.ListContainsString( CurToken, KnownTypes, true ) )
				{
					Status |= TokenStatus.KnownType;
				}
				
				// Test if this token is a KnownTag.
				if ( UnrealEd.Utils.ListContainsString( CurToken, KnownTags, true ) )
				{
					Status |= TokenStatus.KnownTag;
				}
				mTokenStatus.Add( Status );
			}
		}

		private bool MasterFilterMethod(object InCandidate)
		{
			AssetItem CandidateAsset = (AssetItem)InCandidate;
			String LowcaseFullName = CandidateAsset.FullyQualifiedPath.ToLower();

			// If there is no text entered we automatically pass text search.
			bool PassedTextSearch = (mExactSearchAtoms.Count == 0 && mSearchTokens.Count == 0);

			// Test against exact queries (these will always be ORed because ANDing them will result in an empty set)
			// They also do not support negation.
			if ( !PassedTextSearch && mExactSearchAtoms.Count > 0 ) 
			{				
				foreach ( String ExactSearchAtom in mExactSearchAtoms )
				{
					if (LowcaseFullName == ExactSearchAtom) 
					{
						PassedTextSearch = true;
						break;						
					}
				}
			}

			// Test against partial (a.k.a "contains" queries). These are ORed or ANDed depending on the settings
			// The default is ANDed.
			if ( !PassedTextSearch && mSearchTokens.Count > 0 )
			{
				// - - We have not passed yet - - 

				// In "Match All" mode we assume that we will pass and escape early if we fail.
				// In "Match Any" we assume that we fail and escape early if anything matches.
				PassedTextSearch = ( this.RequireTokens == AllVsAny.All );

				// Test all the tokens for whether they matched this asset's type.
				// We plan on ignoring any token that is a known type if this asset already matches in the type field (even partially).
				bool AssetMatchedAType = false;
				foreach ( String CurToken in mSearchTokens )
				{
					if ( CandidateAsset.AssetType.ToLower().Contains( CurToken ) )
					{
						AssetMatchedAType = true;
						break;
					}
				}

				// For every token...
				for ( int TokenIndex = 0 ; TokenIndex < mSearchTokens.Count; ++TokenIndex )
				{
					String CurToken = mSearchTokens[TokenIndex];
					bool TokenFoundOnAsset = false;
					
					// Look in TYPE
					if ( this.ShouldSearchField_Type )
					{
						// If CurToken is a TypeName (e.g. Texture2D, Material, etc...) and this Asset
						// already matches a type, then we can safely ignore this token.
						// Why?
						// Since no asset can be of two types, queries like "Material StaticMesh Marcus"
						// should do the intuitive thing and return both Materials and StaticMeshes.
						bool CurTokenIsTypeName = mTokenStatus[TokenIndex].IsKnownType();
						if ( AssetMatchedAType && CurTokenIsTypeName )
						{
							TokenFoundOnAsset = true;
						}
						else
						{
							TokenFoundOnAsset = CandidateAsset.AssetType.ToLower().Contains( CurToken );
						}
					}

					// Look in TAGS
					if (!TokenFoundOnAsset && this.ShouldSearchField_Tags)
					{
						//Split the full tag into components up so we can search by subcategory
						//(ex: "Architecture.Deco" tags can be found with either "Achitecture" or "Deco")
						List<String> SearchableTagList = new List<string>();
						string Delim = ".";
						char[] DelimArray = Delim.ToCharArray();
						foreach (String TagToSplit in CandidateAsset.Tags)
						{
							SearchableTagList.AddRange(TagToSplit.Split(DelimArray, StringSplitOptions.RemoveEmptyEntries));
						}

                        TokenFoundOnAsset = UnrealEd.Utils.ListContainsPartialString(CurToken, SearchableTagList, true);
					}

					// Look in PATH
					if ( !TokenFoundOnAsset && this.ShouldSearchField_Path )
					{
						TokenFoundOnAsset = LowcaseFullName.Contains( CurToken );
					}

					// Look in NAME
					if ( !TokenFoundOnAsset && this.ShouldSearchField_Name )
					{
						TokenFoundOnAsset = CandidateAsset.Name.ToLower().Contains( CurToken );
					}


					// EARLY OUT with success or fail depending on search parameters :
					if ( this.RequireTokens == AllVsAny.Any )
					{
						if ( TokenFoundOnAsset )
						{
							// Asset matched one of the tokens;
							PassedTextSearch = true;
							break;
						}
					}
					else // ( this.RequireTokens == AllVsAny.All )
					{
						bool ShouldNotShow = !( TokenFoundOnAsset ^ mTokenStatus[TokenIndex].IsNegated() ); // TokenFound and IsNegated OR TokenNotFound and IsNotNegated
						if ( ShouldNotShow )
						{
							// Found a negated token (we don't want any assets with these)
							//  OR
							// Could not find a required token! Asset disqualified.
							// Early out!
							return false;
						}
					}
				}
			}

			return PassedTextSearch & PassesMetadataFilter( CandidateAsset );
		}

		/// <summary>
		/// Test if the CandidateAsset passes the metadata filter (currently object type and tags)
		/// </summary>
		/// <param name="CandidateAsset">AssetItem to be tested</param>
		/// <returns>true if asset passes this metadata filter, false otherwise</returns>
		private bool PassesMetadataFilter(AssetItem CandidateAsset)
		{
			// Does the asset pass the parts of the filter that are not the multi-tier "drill down" criteria.
			bool PassesNonTieredFilters =
				// Check that we pass the quarantined-only
				(!ShowOnlyQuarantined || CandidateAsset.IsQuarantined) &&
				// Check 'tagged only' / 'untagged only' / 'tagged and untagged'
				PassesLoadedStateFilter(CandidateAsset) &&
				// Check 'loaded only' / 'unloaded only' / 'loaded and unloaded'
				PassesTaggedStateFilter(CandidateAsset) &&
				// Check that we pass the object type filter
				PassesObjectTypeFilter(CandidateAsset) &&
				// Check that we pass filter for recent access
				PassesRecentFilter(CandidateAsset) &&
				// Check that we pass the in use filter 
				PassesInUseFilter(CandidateAsset) &&
				// Check if we're showing flattened textures
				PassesShowFlattenedTextureFilter(CandidateAsset);

			// Check to see if we pass the multi-tier filter.
			if ( PassesNonTieredFilters )
			{
				for ( int TierIndex = 0; TierIndex < TagTiers.Count; ++TierIndex )
				{
					// If we are candidates for this tier; remember this asset so we can later update the tag tiers based on it.
					TierInputs[TierIndex].Add( CandidateAsset );

					if ( !AssetPassesTagTier( CandidateAsset, TagTiers[TierIndex] ) )
					{
						// We have passed the non-tiered filter but failed one of the tag tiers.
						return false;
					}
				}
			}

			return PassesNonTieredFilters;
		}

		/// <summary>
		///  Tests the asset against a tag tier (list of tag).
		/// </summary>
		/// <param name="CandidateAsset">The asset to test</param>
		/// <param name="InTagTier">The list of string representing a selection of a TagsColumn in the Filter Panel.</param>
		/// <returns>True if the asset passes, false otherwise.</returns>
		bool AssetPassesTagTier( AssetItem CandidateAsset, List<String> InTagTier )
		{
			return InTagTier.Count == 0 || TagUtils.AnyElementsInCommon( CandidateAsset.Tags, InTagTier );
		}

		/// <summary>
		/// Tests the given asset against the list of object types in the Object Type filter option.
		/// </summary>
		/// <param name="CandidateAsset">The asset to check against the list of filtered object types.</param>
		/// <returns>true if the asset will pass the object type filter.</returns>
		private bool PassesObjectTypeFilter( AssetItem CandidateAsset )
		{
			return PassesObjectTypeFilter( CandidateAsset.AssetType, CandidateAsset.IsArchetype );
		}

		/// <summary>
		/// Tests the type of asset against the list of object types in the Object Type filter option.
		/// </summary>
		/// <param name="AssetType">The type of asset to check against the list of filtered object types.</param>
		/// <param name="IsArchetype">Is the asset an archetype?</param>
		/// <returns>true if the asset type will pass the object type filter.</returns>
		public bool PassesObjectTypeFilter( String AssetType, bool IsArchetype )
		{
			return this.ObjectTypeDescriptions.Count == 0 || mBackend.IsAssetAnyOfBrowsableTypes( AssetType, IsArchetype, this.ObjectTypeDescriptions );
		}

		/// <summary>
		/// Test if AssetItem passes this filter's requirements for recent access.
		/// </summary>
		/// <param name="CandidateAsset">AssetItem to test</param>
		/// <returns>True if AssetItem was recently accessed or we don't need to check, false otherwise.</returns>
		public bool PassesRecentFilter(AssetItem CandidateAsset)
		{
			return ShowRecentItemsOnly ? mMainControl.IsObjectInRecents(CandidateAsset.FullyQualifiedPath) : true;
		}

		/// <summary>
		/// Test if AssetItem passes this filters requirements being a flattened texture
		/// </summary>
		/// <param name="CandidateAsset"></param>
		/// <returns></returns>
		public bool PassesShowFlattenedTextureFilter(AssetItem CandidateAsset)
		{
			return PassesShowFlattenedTextureFilter(CandidateAsset.AssetType, CandidateAsset.Name);
		}

		/// <summary>
		/// Test if Asset with Name and Type passes this filters requirements being a flattened texture
		/// </summary>
		/// <param name="AssetType"></param>
		/// <param name="AssetName"></param>
		/// <returns></returns>
		public bool PassesShowFlattenedTextureFilter(String AssetType, String AssetName)
		{
			if (!ShowFlattenedTextures)
			{
				if (AssetName.Contains("_Flattened") && AssetType.StartsWith("Texture"))
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Test if AssetItem passes this filter's requirements for being in use.
		/// </summary>
		/// <param name="CandidateAsset">AssetItem to test</param>
		/// <returns>True if AssetItem is in use, false otherwise.</returns>
		public bool PassesInUseFilter(AssetItem CandidateAsset )
		{
			return InUseFilterEnabled ? mMainControl.Backend.IsObjectInUse(CandidateAsset) : true;
		}

		/// <summary>
		/// Test if AssetItem passes this filter's requirements for presence of tags.
		/// </summary>
		/// <param name="CandidateAsset">AssetItem to test</param>
		/// <returns>True if AssetItem passes this filter's requirements for presence of tags, false otherwise.</returns>
		private bool PassesTaggedStateFilter( AssetItem CandidateAsset )
		{
			return
				( TaggedFilterOption == TaggedState.TaggedAndUntagged ) ||
				( TaggedFilterOption == TaggedState.TaggedOnly && CandidateAsset.Tags.Count > 0 ) ||
				( TaggedFilterOption == TaggedState.UntaggedOnly && CandidateAsset.Tags.Count == 0 );
		}

		/// <summary>
		/// Test if AssetItem passes this filter's requirements for being loaded.
		/// </summary>
		/// <param name="CandidateAsset">AssetItem to test</param>
		/// <returns>True if AssetItem passes this filter's requirements for being loaded, false otherwise.</returns>
		private bool PassesLoadedStateFilter( AssetItem CandidateAsset )
		{
			return
				( LoadedFilterOption == LoadedState.LoadedAndUnloaded ) ||
				( LoadedFilterOption == LoadedState.LoadedOnly && CandidateAsset.LoadedStatus != AssetItem.LoadedStatusType.NotLoaded ) ||
				( LoadedFilterOption == LoadedState.UnloadedOnly && CandidateAsset.LoadedStatus == AssetItem.LoadedStatusType.NotLoaded );
		}

		/// Get the filter predicate that tests AssetItems on whether they pass this filter.
		public Predicate<object> GetFilterPredicate()
		{
			if ( this.IsNullFilter() )
			{		   
				return null;
			}
			else
			{
				ResetAssetsInTiers();
				return new Predicate<object>( MasterFilterMethod );
			}
		}

		/// <summary>
		/// Break the search string up into exact search queries (queries in quotes) and regular search queries (find anywhere in name)
		/// Also sanitizes the search string.
		/// </summary>
		/// <param name="InSearchString">Search string entered by the user</param>
		/// <param name="OutNonQuotedStrings">Search string segments not in quotes</param>
		/// <param name="OutQuotedStrings">Search string segments in quotes</param>
		private void ParseSearchString (String InSearchString, List< String > OutNonQuotedStrings, List< String > OutQuotedStrings)
		{
			// - is used to negate a token. Make sure any whitespace after - is removed.
			Regex NegatedSyntaxFixer = new Regex("-\\s+");
			InSearchString = NegatedSyntaxFixer.Replace(InSearchString, NegatingTokenStr);


			OutNonQuotedStrings.Clear();
			OutQuotedStrings.Clear();

			// Extract anything in quotes and add to the quoted tokens list.
						
			// Regex: a quote followed by one or more non-quotes followed by a quote. The () create an extraction group.
			Regex QuotedStringMatcher = new Regex("[\\w]*'([^\"]+?)' *");
			MatchCollection QuotedAtoms = QuotedStringMatcher.Matches( InSearchString );
			foreach ( Match QuotedAtom in QuotedAtoms ) 
			{
				// Groups[0] is the whole match; Groups[1] is the captured value.
				OutQuotedStrings.Add( QuotedAtom.Groups[1].Value.Trim() );
			}

			// Remove anything in quotes from the query;
			InSearchString = QuotedStringMatcher.Replace( InSearchString, String.Empty );

			// Split on spaces and add to the non-quoted tokens list.
			char[] AtomSeparator = new char[]{' '};			
			String[] NonQuotedAtoms = InSearchString.Split(AtomSeparator, StringSplitOptions.RemoveEmptyEntries);
			foreach ( String NonQuotedAtom in NonQuotedAtoms )
			{
				if ( NonQuotedAtom != NegatingTokenStr )
				{
					OutNonQuotedStrings.Add( NonQuotedAtom );
				}				
			}
		}

		/// Specifies whether or not to only show recent assets
		public bool ShowRecentItemsOnly { get; set; }

		/// Specifices if flattend textures are to be displayed with the other materials
		public bool ShowFlattenedTextures { get; set; }
		
		/// Specifies whether or not to search for in use assets referenced by the current level.
		public bool ShowCurrentLevelInUse { get; set; }
		/// Specifies whether or not to search for in use assets referenced by visible levels.
		public bool ShowVisibleLevelsInUse { get; set; }
		/// Specifies whether or not to search for in use assets referenced by loaded levels.
		public bool ShowLoadedLevelsInUse { get; set; }
		/// Specifies whether or not to filter assets by their usage.
		public bool InUseOff { get; set; }

		/// Returns true if anything makes the in use filter enabled.
		public bool InUseFilterEnabled
		{
			get
			{
				return ( ShowCurrentLevelInUse || ShowVisibleLevelsInUse || ShowLoadedLevelsInUse ) && !InUseOff;
			}
		}

		/// Specifies whether to show / hide / ignore tagged assets
		public TaggedState TaggedFilterOption { get; set; }

		/// Specifies whether to show / hide / ignore loaded assets
		public LoadedState LoadedFilterOption { get; set; }

		/// Should we should filter out non-quarantined assets
		public bool ShowOnlyQuarantined { get; set; }

		/// <summary>
		/// Determines if this filter will cause any items to be filtered. Cache this value for best performance.
		/// </summary>
		/// <returns>True if the filter does nothing; false if it would actually remove items from the list.</returns>
		public bool IsNullFilter()
		{
			foreach ( List<String> Tier in TagTiers)
			{
				if (Tier.Count > 0)
				{
					return false;
				}
			}
			
			return IsNonTierFilterNull();
		}

		/// <summary>
		/// True if the non-tiered part of the filter does nothing.
		/// </summary>
		/// <returns>True if null, false otherwise</returns>
		public bool IsNonTierFilterNull()
		{
			// Are the text-based name and tag filters null (the stuff in the search panel)
			bool IsTextFilterNull = mSearchString.Trim() == String.Empty;

			return
				ShowOnlyQuarantined == false &&
				IsTextFilterNull &&
				TaggedFilterOption == TaggedState.TaggedAndUntagged &&
				LoadedFilterOption == LoadedState.LoadedAndUnloaded &&
				ShowRecentItemsOnly == false &&
				ShowFlattenedTextures == true &&
				!InUseFilterEnabled &&
 				ObjectTypeDescriptions.Count == 0;

		}

		/// True when we should not filter out any assets based on the Object Type
		public bool HasNullObjectTypeFiler
		{
			get { return ObjectTypeDescriptions.Count == 0; }
		}

		/// How we should interpret the query from the SearchPanel. e.g. match all tags vs. mach any tags.
		public AllVsAny RequireTokens { get; set; }

		public bool ShouldSearchField_Tags { get; set; }
		public bool ShouldSearchField_Type { get; set; }
		public bool ShouldSearchField_Path { get; set; }
		public bool ShouldSearchField_Name { get; set; }

		/// List of string names of ObjectTypes that the user wants to see.
		public List<String> ObjectTypeDescriptions { get; set; }


		private ReadOnlyCollection<String> m_Tags = new List<String>().AsReadOnly();
		/// List of tags in query. The user wants to see objects with all or any of these tags on them. (See SearchMode, Mode)
		public ReadOnlyCollection<String> Tags
		{
			get { return m_Tags; }
			set { m_Tags = value; }
		}

		/// List of tags from the filter list. The user wants to see objects with any of these tags.
		private List< List<String> > TagTiers { get; set; }
		
		/// How many tag tiers will be applied by this filter.
		public int GetTagTierCount() { return TagTiers.Count; }

		/// Set the tag tiers to be applied by this filter.
		/// Each tier is a list of strings representing the tags selected in that tag column of the FilterPanel.
		public void SetTagTiers( List< List<String> > InTagTiers  )
		{
			this.TagTiers = InTagTiers;
			ResetAssetsInTiers();
		}

		/// List of assets that pass the various tag tiers
		private List< List<AssetItem> > TierInputs { get; set; }

		/// Creates a shallow copy of this object
		public Object Clone()
		{
			return MemberwiseClone();
		}
	}
}
