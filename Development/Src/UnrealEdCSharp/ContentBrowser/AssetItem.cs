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
using System.ComponentModel;
using UnrealEd;



namespace ContentBrowser
{

	///
	/// Asset Item class
	/// 
	/// Note: This class is instantiated frequently (e.g. 100k+ times per full refresh of Gears).
	/// For this reason we opt to use INotifyPropertyChanged instead
	/// of DependencyProperties to cut down to ctor overhead.
	/// 

	public class AssetItem : INotifyPropertyChanged
    {

		/// <summary>
		/// Construct an AssetItem for displaying in the ContentBrowser
		/// </summary>
		/// <param name="InName">The Name of the item to display</param>
		/// <param name="InPathOnly">The package path to this asset</param>
        public AssetItem( MainControl InMainControl, String InName, String InPathOnly )
        {
			mMainControl = InMainControl;

            Name = InName;
			PathOnly = InPathOnly;
			mTags = new List<String> { "--" };
			AssetType = "--";
			mMemoryUsage = 0;
			mCustomLabels = new List<String> { "", "", "", "", "", "", "", "", "", "" };	// ContentBrowserDefs::MaxAssetListCustomLabels + 1 (the last item reserved for warnings)
			mCustomDataColumns = new List<String> { "", "", "", "", "", "", "", "", "", "" }; // ContentBrowserDefs::MaxAssetListCustomDataColumns
		}


		/// Content Browser object that (indirectly) owns us
		MainControl mMainControl;


		/// We override "ToString" so that we can report the name of the asset.  WPF's ListView will use
		/// "ToString" when performing a text-based search (typing characters while the list view is focused.)
		public override string ToString()
		{
			return Name;
		}


		/// True if the asset is currently selected.  You should never set this property unless you know
		/// what you're doing.  If you want to change whether the asset is selected, you should modify
		/// the SelectedItems array on the asset view instead!
		private bool mSelected;
		public bool Selected
		{
			get { return mSelected; }
			set
			{
				if( value != Selected )
				{
					mSelected = value;

					// Also keep our associated asset visual up to date
					if( Visual != null )
					{
						// Update the visual state of the asset visual
						if( Selected )
						{
							if( Visual.VisualState != AssetVisual.VisualStateType.Selected )
							{
								Visual.VisualState = AssetVisual.VisualStateType.SelectedInteractively;
							}
						}
						else
						{
							if( Visual.VisualState != AssetVisual.VisualStateType.Default )
							{
								Visual.VisualState = AssetVisual.VisualStateType.DeselectedInteractively;
							}
						}
					}

					NotifyPropertyChanged( "Selected" );
				}
			}
		}



		/// The name of the asset.
		public String Name{get; set;}

		/// "Formatted" name for this asset.  It will display a star next to modified asset.
		public String FormattedName
		{
			get
			{
				if( LoadedStatus == LoadedStatusType.LoadedAndModified )
				{
					return Name + "*";
				}

				return Name;
			}
		}

		
		
		/// The path of the Unreal object (package and groups), excluding the name of the object
		public String PathOnly { get; set; }
		
		
		/// A string representing the type of asset
		public String AssetType { get; set; }


		/// True if this asset is an Archetype
		public bool IsArchetype { get; set; }
		 

		#region Tags Property
		
		/// Get or set Tags currently assigned to this object. This is a dependency property.
		private List<String> mTags;
		public List<String> Tags
		{
			get { return mTags; }
			set
			{ 
				if ( mTags != value )
				{
					mTags = value;
					mTagsAsString = TagUtils.TagSetToFormattedString( mTags );
					
					NotifyPropertyChanged( "TagsAsString" );
					NotifyPropertyChanged( "Tags" );
				}
			}
		}

		/// Backing store for the cached Tags as a string.
		String mTagsAsString;
		/// Get the tags as a single, coma-delimited string.
		public String TagsAsString { get { return mTagsAsString; } }

		#endregion



		/// The object's fully qualified path in Unreal syntax: package.[group.]name
		public String FullyQualifiedPath
		{
			get
			{
				// Assemble a fully qualified path from the object name and it's path
				return PathOnly + "." + Name; 
			}
		}


		/// The 'full name' for this asset in Unreal syntax: type'package.[group.]name'
		public String FullName
		{
			get
			{
				// Assemble the full name for this object
				return Utils.MakeFullName( AssetType, FullyQualifiedPath );
			}
		}


		/// Computes and returns only the package part of the object's path
		public String PackageName
		{
			get
			{
				// Grab the package name string from the object's path
				String PackageNameOnly = PathOnly;
				int FirstDotIndex = PathOnly.IndexOf( '.' );
				if( FirstDotIndex != -1 )
				{
					PackageNameOnly = PathOnly.Substring( 0, FirstDotIndex );
				}
				return PackageNameOnly;
			}
		}


		/// Asset loaded/unloaded state
		public enum LoadedStatusType
		{
			/// Asset is not currently loaded
			NotLoaded,

			/// Asset is loaded
			Loaded,

			/// Asset is loaded and has been modified since the last save
			LoadedAndModified,
		}


		/// Whether the object is currently loaded, as far as the UI knows at least
		private LoadedStatusType mLoadedStatus;
		public LoadedStatusType LoadedStatus
		{
			get { return mLoadedStatus; }
			set
			{
				if ( mLoadedStatus != value )
				{
					mLoadedStatus = value;
					NotifyPropertyChanged( "LoadedStatus" );
					NotifyPropertyChanged( "FormattedName" );
				}
			}
		}


		/// True if the object is 'verified', meaning the checkpoint commandlet has blessed it yet (or we're
		/// pretty sure for some other reason that the asset really exists)
		private bool mIsVerified;
		public bool IsVerified
		{
			get { return mIsVerified; }
			set
			{
				if ( mIsVerified != value )
				{
					mIsVerified = value;
					NotifyPropertyChanged( "IsVerified" );
				}
			}
		}


		/// True if custom labels are up to date for this asset
		private bool mCustomLabelsAreUpToDate;
	
		/// True if memory usage for this asset is up to date
		private bool mMemoryUsageUpToDate;

		/// Marks custom labels as dirty so they'll be refreshed on demand next time
		public void MarkCustomLabelsAsDirty()
		{
			mCustomLabelsAreUpToDate = false;
		}

		/// Marks memory usage for this asset as dirty so it will be refreshed on demand when needed
		public void MarkMemoryUsageAsDirty()
		{
			mMemoryUsageUpToDate = false;
		}

		/// Updates the custom labels for this asset if they're dirty
		public void UpdateCustomLabelsIfNeeded()
		{
			// Refresh custom data columns if we need to
			if( !mCustomLabelsAreUpToDate )
			{
				mCustomLabels = mMainControl.Backend.GenerateCustomLabelsForAsset( this );
				mCustomLabelsAreUpToDate = true;

				this.NotifyPropertyChanged( "CustomLabels" );
			}
		}

		private int mMemoryUsage;

        /// Unformatted memory usage.  Just returns bytes so we can sort correctly.
		public int RawMemoryUsage
        {
            get
            {
                return mMemoryUsage;
            }
        }

        /// Memory usage text to display
		public String MemoryUsageText
		{
			get
			{
				/// Make sure memory usage is up to date
				UpdateMemoryUsageIfNeeded();
                String MemoryStr = "";
                // Memory usage is -1 if the memory for the asset could not be determined.
                if( mMemoryUsage != -1 )
                {
                    float ResourceSize = mMemoryUsage;

                    // Default to using KByte.
                    String SizeDescription = "KByte";
                    ResourceSize /= 1024;
                    // Use MByte if the size is greater than 1 meg.
                    if (ResourceSize > 1024)
                    {
                        SizeDescription = "MByte";
                        ResourceSize /= 1024;
                    }
                    MemoryStr = String.Format("{0:0.00} {1}", ResourceSize, SizeDescription);
                }
                return MemoryStr;
			}
		}

		/// Custom label strings for this asset (may be null)
		private List<String> mCustomLabels;
		public List<String> CustomLabels
		{
			get
			{
				UpdateCustomLabelsIfNeeded();
				return mCustomLabels;
			}
			set
			{
				if( mCustomLabels != value )
				{
					mCustomLabels = value;
					this.NotifyPropertyChanged( "CustomLabels" );
				}
			}
		}



		/// True if custom data columns are up to date for this asset
		private bool mCustomDataColumnsAreUpToDate;

		/// Marks custom data columns as dirty so they'll be refreshed on demand next time
		public void MarkCustomDataColumnsAsDirty()
		{
			mCustomDataColumnsAreUpToDate = false;
		}


		/// Fills in the custom data columns for this asset if they're dirty
		public void UpdateCustomDataColumnsIfNeeded()
		{
			// Refresh custom data columns if we need to
			if( !mCustomDataColumnsAreUpToDate )
			{
				mCustomDataColumns = mMainControl.Backend.GenerateCustomDataColumnsForAsset( this );
				mCustomDataColumnsAreUpToDate = true;

				this.NotifyPropertyChanged( "CustomDataColumns" );
			}
		}

	
		/// Custom data strings for this asset (may be null)
		private List<String> mCustomDataColumns;
		public List<String> CustomDataColumns
		{
			get
			{
				UpdateCustomDataColumnsIfNeeded();			
				return mCustomDataColumns;
			}
			set
			{
				if ( mCustomDataColumns != value )
				{
					mCustomDataColumns = value;
					this.NotifyPropertyChanged( "CustomDataColumns" );
				}
			}
		}


		/// Fills in the 'Date Added' for this asset if we haven't done that yet
		public void UpdateDateAddedIfNeeded()
		{
			// Refresh the 'date added' if we need to
			if( !mDateAddedIsUpToDate )
			{
				mDateAdded = mMainControl.Backend.GenerateDateAddedForAsset( this );
				mDateAddedIsUpToDate = true;

				this.NotifyPropertyChanged( "DateAdded" );
			}
		}

		/// Calculates memory usage for the asset list view memory column and tool tip if it isn't already up to date.
		public void UpdateMemoryUsageIfNeeded()
		{
			if ( !mMemoryUsageUpToDate )
			{
				mMemoryUsage = mMainControl.Backend.CalculateMemoryUsageForAsset( this );
				mMemoryUsageUpToDate = true;

				this.NotifyPropertyChanged("MemoryUsageText");
			}
		}

		/// True if date added is up to date for this asset
		private bool mDateAddedIsUpToDate;
		
		/// Date Added
		private DateTime mDateAdded;
		public DateTime DateAdded
		{
			get
			{
				UpdateDateAddedIfNeeded();
				return mDateAdded;
			}
			set
			{
				if( mDateAdded != value )
				{
					mDateAdded = value;
					this.NotifyPropertyChanged( "DateAdded" );
				}
			}
		}

		/// Date Added formatted text
		public string DateAddedText
		{
			get
			{
				return DateAdded.ToShortDateString();
			}
		}

		private bool mIsQuarantined;
		public bool IsQuarantined
		{
			get
			{
				return mIsQuarantined;
			}

			set
			{
				if ( mIsQuarantined != value )
				{
					mIsQuarantined = value;
					this.NotifyPropertyChanged( "IsQuarantined" );
				}
			}
		}


		/// Visual that represents this asset's thumbnail on the asset canvas
		public AssetVisual Visual;

		#region INotifyPropertyChanged Members

		public event PropertyChangedEventHandler PropertyChanged;
		private void NotifyPropertyChanged( String PropertyName )
		{
			if ( PropertyChanged != null )
			{
				this.PropertyChanged( this, new PropertyChangedEventArgs( PropertyName ) );
			}
		}

		#endregion

		#region Static Helper Functions
		public static void SortByAssetType(List<AssetItem> ListToSort)
		{
			ListToSort.Sort(
				delegate(AssetItem AssetItem0, AssetItem AssetItem1) 
				{
					return AssetItem0.AssetType.CompareTo(AssetItem1.AssetType); 
				}
			);
		}
		#endregion
	}
}
