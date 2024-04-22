//=============================================================================
//	AssetViewModel.cs: Content browser asset view model
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using UnrealEd;


namespace ContentBrowser
{
	/// Provides a visual model for Assets returned by queries to the GameAssetDatabase and the Engine.
	public class AssetViewModel : ObservableCollection<AssetItem>
	{
		/// Cached content browser reference
		private MainControl m_ContentBrowser;

		/// Construct an AssetViewModel
		public AssetViewModel()
        {
        }

		/// <summary>
		/// Initialize the asset view model
		/// </summary>
		/// <param name="InContentBrowser">Content browser that the asset view model is associated with</param>
		public void Init( MainControl InContentBrowser )
		{
			m_ContentBrowser = InContentBrowser;
		}



		#region Asset Status

		/// <summary>
		/// Updates the cached status of the specified asset item
		/// </summary>
		/// <param name="Asset">The asset item to update</param>
		/// <param name="UpdateFlags">Flags for limiting which parts of the asset are refreshed.  This is simply to improve performance.</param>
		public void UpdateAssetStatus( AssetItem Asset, AssetStatusUpdateFlags UpdateFlags )
		{
			m_ContentBrowser.Backend.UpdateAssetStatus( Asset, UpdateFlags );
		}



		/// Updates the cached status for all assets in the view model
		public void UpdateStatusForAllAssetsInView( AssetStatusUpdateFlags UpdateFlags )
		{
			if( m_ContentBrowser != null )
			{
				foreach( AssetItem CurAssetItem in this )
				{
					UpdateAssetStatus( CurAssetItem, UpdateFlags );
				}
			}
		}


		#endregion



		/// <summary>
		/// Searches for and returns the index of the asset item of the specified path, or -1 if not found.  O(N).
		/// </summary>
		public int FindAssetIndex( String InAssetFullName )
		{
			String AssetClassName = Utils.GetClassNameFromFullName( InAssetFullName );
			String FullyQualifiedPath = Utils.GetPathFromFullName( InAssetFullName );
			
			// Grab the object's name and path by splitting the full path string
			int DotIndex = FullyQualifiedPath.LastIndexOf( '.' );
			if ( DotIndex != -1 )
			{
				String ObjectPathOnly = FullyQualifiedPath.Substring( 0, DotIndex );	// Everything before the last dot
				String ObjectName = FullyQualifiedPath.Substring( DotIndex + 1 );	// Everything after the last dot

                // Remove any sub-object type, if this is the case we can't compare asset types
                bool bSkipType = false;
                int ColonIndex = ObjectName.IndexOf(':');
                if (ColonIndex != -1)
                {
                    bSkipType = true;
                    ObjectName = ObjectName.Substring(0, ColonIndex);	// Everything before the first colon
                }

				// @todo CB: Ideally this would not be O(N), we should probably pregenerate a dictionary
				for ( int AssetIndex = 0; AssetIndex < this.Count; ++AssetIndex )
				{
					AssetItem CurAssetItem = this[AssetIndex];
                    if (bSkipType || CurAssetItem.AssetType.Equals(AssetClassName, StringComparison.OrdinalIgnoreCase))
                    {
                        if (CurAssetItem.Name.Equals(ObjectName, StringComparison.OrdinalIgnoreCase))
                        {
                            if (CurAssetItem.PathOnly.Equals(ObjectPathOnly, StringComparison.OrdinalIgnoreCase))
                            {
                                // Found it!
                                return AssetIndex;
                            }
                        }
					}
				}
			}

			// Not found
			return -1;
		}

	}
}
