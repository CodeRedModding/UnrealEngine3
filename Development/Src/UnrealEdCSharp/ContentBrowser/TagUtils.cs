//=============================================================================
//	TagUtils.cs: Content browser tag utilities
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;


namespace ContentBrowser
{
    public static class TagUtils
    {
		/// The preferred default tag delimiter; used for joining tags
		public static string TagDelimiter = ", ";

		/// Characters considered to be valid tag delimiters
		public static char[] TagDelimiterArray = TagDelimiter.ToCharArray();

		/// <summary>
		/// Is this character considered to be a tag delimiter?
		/// </summary>
		/// <param name="Candidate">Character to test</param>
		/// <returns>True if the character is a valid tag separator</returns>
		public static bool IsDelimiter( char Candidate )
		{
			return TagDelimiter.IndexOf(Candidate) >= 0;
		}

		/// <summary>
		/// Check validity of the tag
		/// </summary>
		/// <param name="Tag">Tag to test</param>
		/// <returns>true if tag is valid; false otherwise.</returns>
		public static bool IsTagValid(String Tag)
		{
			return Tag.Length > 0 && Tag.Length < TagDefs.MaxTagLength && m_TagValidator.IsMatch( Tag );
		}

		/// Describes what a valid string of tags looks like. ^ and $ needed to force match entire string.
		private static Regex m_TagValidator = new Regex(@"^[\w_]+$", RegexOptions.IgnoreCase);

		#region Tag Naming
		
		/// Given a tag group and a tag name create the full tag name: Group.Name
		public static String CreateFullTagName(String GroupName, String TagName)
		{
			if ( GroupName == null || GroupName.Trim() == String.Empty )
			{
				return TagName;
			}
			else
			{
				return GroupName + "." + TagName;
			}
			
		}

		/// Given a full tag name extract just the group.
		public static String GetGroupNameFromFullName( String FullTagName )
		{
			int DelimiterIndex = FullTagName.IndexOf( '.' );
			if ( DelimiterIndex > 0 )
			{
				return FullTagName.Substring( 0, DelimiterIndex );
			}
			else
			{
				return String.Empty;
			}
		}

		/// Given a full tag name extract just the Name.
		public static String GetTagNameFromFullName( String FullTagName )
		{
			int DelimiterIndex = FullTagName.IndexOf( '.' );
			if ( DelimiterIndex + 1 < FullTagName.Length )
			{
				return FullTagName.Substring( DelimiterIndex + 1 );
			}
			else
			{
				return FullTagName;
			}
		}

		/// Given a full tag name return a pretty version of it: Name(Group) or Name depending on whether this tag has a group.
		public static String MakeFormattedName( String Name, String GroupName )
		{
			if ( GroupName == String.Empty )
			{
				return Name;
			}
			else
			{
				return Name + " (" + GroupName + ")";
			}
		}

		/// Given a full tag name return a pretty version of it: Name(Group) or Name depending on whether this tag has a group.
		public static String MakeFormattedName( String FullName )
		{
			return MakeFormattedName( GetTagNameFromFullName(FullName), GetGroupNameFromFullName( FullName ) );
		}

		#endregion


		/// <summary>
        /// Split a string into a tag set
        /// </summary>
        /// <param name="TagsToSplit">The string to split</param>
        /// <returns>A list strings, each representing a tag</returns>
        public static List<String> TagsStringToTagSet(String TagsToSplit)
        {
            return new List<String>( TagsToSplit.Split(TagUtils.TagDelimiterArray, StringSplitOptions.RemoveEmptyEntries) );
        }

		/// <summary>
		/// Checks if the two lists have any tags in common. Empty lists cannot have tags in common.
		/// </summary>
		/// <param name="TagList1">One of the lists to compare</param>
		/// <param name="TagList2">Other list to compare</param>
		/// <returns>true if the two sets have any tags in common; false otherwise.</returns>
		public static bool AnyElementsInCommon( ICollection<String> TagList1, ICollection<String> TagList2 )
		{
			foreach ( String Tag in TagList1)
			{
				if ( TagList2.Contains(Tag) )
				{
					return true;
				}
			}

			return false;
		}

        /// <summary>
        /// Compute a set of tags present in set Minuend that is not present in set Subtrahend.
        /// The original sets are not modified
        /// </summary>
        /// <param name="Minuend">The set from which to subtract tags</param>
        /// <param name="Subtrahend">The set of tags to subtract</param>
        /// <returns>The set difference: Minuend - Subtrahend.</returns>
        public static List<String> SubtractTagLists(ICollection<String> Minuend, ICollection<String> Subtrahend)
        {
            // Make a copy of the subtrahend; we need to modify it
            Subtrahend = new List<String>( Subtrahend );

            List<String> Difference = new List<String>();
            foreach (String Tag in Minuend)
            {
                if ( !Subtrahend.Contains( Tag ) )
                {
                    Difference.Add( Tag );
                }
                else
                {
                    Subtrahend.Remove( Tag );
                }
            }

            return Difference;
        }

		/// <summary>
		/// Test if SubsetCandidate is contained within ContainerCandidate.
		/// </summary>
		/// <param name="ContainerCandidate">Test if set of tags contains SubsetCandidate</param>
		/// <param name="SubsetCandidate">Test if this set of tags is containted in ContainerCandidate</param>
		/// <returns>True if SubsetCandidate is a subset of ContainerCandidate</returns>
		public static bool DoesSetContainSet( ICollection<String> ContainerCandidate, ICollection<String> SubsetCandidate )
		{
			foreach ( String Elt in SubsetCandidate )
			{
				if (!ContainerCandidate.Contains(Elt))
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
        /// Checks to see if the two sets of tags are equivalent.
		/// Despite parameters being ICollection they are treated as sets; comparison is order independent. Multiple instances are ignored.
        /// </summary>
        /// <param name="SetOne">First set of tags</param>
        /// <param name="SetTwo">Second set of tags</param>
        /// <returns>True if the two sets of tags are equivalent</returns>
        public static bool AreTagSetsEquivalent( ICollection<String> SetOne, ICollection<String> SetTwo )
        {
			return DoesSetContainSet( SetOne, SetTwo ) && DoesSetContainSet( SetTwo, SetOne );
        }

        /// <summary>
        /// Turn a collection of tag strings into a single string.
        /// </summary>
        /// <param name="TagsToStringify">A string collection that represents a set of tags</param>
        /// <returns>A single space separated string</returns>
        public static String TagSetToFormattedString(IList<String> TagsToStringify)
        {
            if (TagsToStringify.Count > 0)
            {
                StringBuilder MyStringBuilder = new StringBuilder();
				MyStringBuilder.Append( MakeFormattedName( TagsToStringify[0] ) );

				for (int TagIdx = 1; TagIdx < TagsToStringify.Count; ++TagIdx)
                {
                    MyStringBuilder.Append(TagUtils.TagDelimiter);
					MyStringBuilder.Append( MakeFormattedName( TagsToStringify[TagIdx] ) );
                }

                return MyStringBuilder.ToString();
            }
            else
            {
                return "";
            }

        }


		/// <summary>
		/// Given a list of assets, compute the union of all tags present on those asset.
		/// </summary>
		/// <param name="InAssetItems"></param>
		/// <return>A hash set union of all tags assets.</return>
		public static HashSet<String> GatherTagsFromAssets( ICollection<AssetItem> InAssetItems )
		{
			// A set of tags present on some assets (but not on all assets)
			HashSet<String> TagsOnAssets = new HashSet<String>();

			// Build up a union of all tags on given assets.
			foreach ( AssetItem Asset in InAssetItems )
			{
				List<String> TagsOnThisAsset = Asset.Tags;
				foreach ( String Tag in TagsOnThisAsset )
				{
					TagsOnAssets.Add( Tag );
				}
			}

			return TagsOnAssets;
		}

		/// <summary>
		/// Given a set of assets determines which tags are present on all assets and which are present on some only (these are exclusive sets)
		/// </summary>
		/// <param name="InAssetItems">Set of assets</param>
		/// <param name="OutTagsOnAllAssets">Output list of tags present on all assets</param>
		/// <param name="OutTagsOnSomeAssets">Output lost of tags present on some assets only</param>
		public static void GatherTagsFromAssets( ICollection<AssetItem> InAssetItems, out List<String> OutTagsOnAllAssets, out List<String> OutTagsOnSomeAssets )
		{
			// A set of tags present on some assets (but not on all assets)
			HashSet<String> OnSomeAssets = new HashSet<String>();

			// A set of tags present on all assets (but not on some assets)
			HashSet<String> OnAllAssets = new HashSet<String>();

			// Build up a union of all tags on given assets.
			foreach ( AssetItem Asset in InAssetItems )
			{
				List<String> TagsOnThisAsset = Asset.Tags;
				foreach(String Tag in TagsOnThisAsset)
				{
					OnAllAssets.Add(Tag);
				}				
			}

			// -- Determine which tags are on some assets only. --

			foreach ( AssetItem Asset in InAssetItems )
			{
				List<String> TagsOnThisAsset = Asset.Tags;
				// For every tag in the union of all tags
				foreach ( String TagCandidate in OnAllAssets )
				{					
					// If the tag is not on this sepecifc asset, then it cannot be "on all assets"
					if ( !TagsOnThisAsset.Contains( TagCandidate ) )
					{
						OnSomeAssets.Add( TagCandidate );
					}
				}
			}

			// None of the tags on some assets can be on all assets
			foreach ( String Tag in OnSomeAssets )
			{
				OnAllAssets.Remove( Tag );
			}

			OutTagsOnAllAssets = new List<String>( OnAllAssets );
			OutTagsOnSomeAssets = new List<String>( OnSomeAssets );
		}

		#region EngineTagDefs
		/// Various Tag-related attributes that we get from the engine; initialized to some default values
		private static EngineTagDefs mTagDefs = new EngineTagDefs(0, '[', ']');

		/// <summary>
		/// Set what the TagUtils think a Tag is. This method should only be called upon initialization.
		/// It necessary to grab definitions of what a tag is from the engine.
		/// </summary>
		/// <param name="TagDefsIn"></param>
		public static void SetTagDefs(EngineTagDefs TagDefsIn)
		{
			mTagDefs = TagDefsIn;
		}
		
		/// Gets definitions of some tag property definitions.
		public static EngineTagDefs TagDefs { get{ return mTagDefs; } }


		/// Describes attributes of tags
		public class EngineTagDefs
		{
			public EngineTagDefs(
				int MaxTagLegnthIn,
				char SystemTagPreDelimiterIn,
				char SystemTagPostDelimiterIn)
			{
				MaxTagLength = MaxTagLegnthIn;
				SystemTagPreDelimiter = SystemTagPreDelimiterIn;
				SystemTagPostDelimiter = SystemTagPostDelimiterIn;
			}

			public readonly int MaxTagLength;
			public readonly char SystemTagPreDelimiter;
			public readonly char SystemTagPostDelimiter;
		}

		#endregion
    }
}
