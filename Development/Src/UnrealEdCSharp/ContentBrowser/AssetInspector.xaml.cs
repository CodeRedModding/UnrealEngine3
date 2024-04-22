//=============================================================================
//	AssetInspector.xaml.cs: Content browser asset inspection control
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Controls.Primitives;
using System.IO;
using System.ComponentModel;
using System.Collections.ObjectModel;
using System.Windows.Media.Animation;

using CustomControls;
using UnrealEd;
using System.Collections;
using System.Globalization;


namespace ContentBrowser
{

	/// <summary>
	/// Tag commands
	/// </summary>
	public static class TagCommands
	{
		/// Create
		public static RoutedUICommand Create { get { return m_Create; } }
		private static RoutedUICommand m_Create = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_TagCommand_Create" ), "TagCreate", typeof( TagCommands ) );

		/// Rename
		public static RoutedUICommand Rename { get { return m_Rename; } }
		private static RoutedUICommand m_Rename = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_TagCommand_Rename" ), "TagRename", typeof( TagCommands ) );

		/// Create Copy
		public static RoutedUICommand CreateCopy { get { return m_CreateCopy; } }
		private static RoutedUICommand m_CreateCopy = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_TagCommand_CreateCopy" ), "TagCreateCopy", typeof( TagCommands ) );

		/// Destroy
		public static RoutedUICommand Destroy { get { return m_Destroy; } }
		private static RoutedUICommand m_Destroy = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_TagCommand_Destroy" ), "TagDestroy", typeof( TagCommands ) );
	}

	/// <summary>
	/// Workaround for WPF's promiscuous DataTemplate binding when using System.String as datatype (leads to layout recursion)
	/// </summary>
	public class Tag : IComparable<Tag>
	{
		/// <summary>
		/// Construct a tag.
		/// </summary>
		/// <param name="InFullName"> Full name of the tag.</param>
		public Tag( String InFullName )
		{
			Name = TagUtils.GetTagNameFromFullName( InFullName );
			GroupName = TagUtils.GetGroupNameFromFullName( InFullName );
			FullName = InFullName;
		}

		/// Get the group to which this tag belongs. Returns String.Empty if there is no group.
		public String GroupName { get; protected set; }
		/// Get just the name of the tag without the Group.
		public String Name { get; protected set; }
		/// Get the Full Name of the tag: Group.Name. Tags with no group will return Name.
		public String FullName { get; protected set; }

		/// Names are formatted differently depending on whether there is a group: Name(Group) or Name.
		public String FormattedFullName
		{
			get
			{
				return TagUtils.MakeFormattedName( Name, GroupName );
			}
		}

		/// Support comparing to another tag. Tags without a group always get sorted below tags with a group.
		public int CompareTo( Tag other )
		{
			if ( ( this.GroupName == String.Empty ) == ( other.GroupName == String.Empty ) )
			{
				return String.Compare( this.FullName, other.FullName );
			}
			else
			{
				// Tags without a group sort to last.
				if ( this.GroupName == String.Empty )
				{
					return +1;
				}
				else
				{
					return -1;
				}
			}

		}
	}

	/// <summary>
	/// Converts an asset's "Loaded Status" to a string for the status in the tooltip
	/// </summary>
	[ValueConversion( typeof( Tag ), typeof( String ) )]
	public class TagToGroupConverter : IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			Tag InTag = (Tag)value;
			return InTag.GroupName;
		}

		/// Converts back to the source type from the target type
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;	// Not supported
		}
	}



    /// <summary>
    /// Asset info panel. Visualizes the currently selected assets and allows
	/// the user to modify tags on the selected assets.
    /// </summary>
    public partial class AssetInspector : UserControl
    {
		/// <summary>
		/// Construct an AssetInfoPanel
		/// </summary>
		public AssetInspector()
        {
            InitializeComponent();

			// Create handler for user clicking on "tiny x" on tags in order to remove them
			RemoveTagClickedHandler = new TagVisual.ClickEventHandler( RemoveTagClicked );

			// Grab a reference to the tag remove storyboard
			mTagVisualErase = (Storyboard)this.FindResource( "TagVisualErase" );
			// Add a handler for completion of the erase animation
			mTagVisualErase.Completed += new EventHandler( mTagVisualErase_Completed );


			// Add handler for user typing into the TagPalette's filter textbox
			mTagsPaletteFilter.TextChanged += new TextChangedEventHandler(mTagsPaletteFilter_TextChanged);

			// Set up the predicate for TagsPalette filtering
			FilterTagsPredicate = new Predicate<Object>( FilterTagsMethod );

			
			// Register handler for user clicking on (+) button to Create a Tag
			mCreateTagButton.Click += new RoutedEventHandler( mCreateTagButton_Click );
			mCreateTagPrompt.SetValidator( new NameEntryPrompt.Validator( TagPromptValidator ) );
			mCreateTagPrompt.Succeeded += new NameEntryPrompt.SucceededHandler( mCreateTagPrompt_Succeeded );

			// Register handler for user clicking on (-) button to Destroy a Tag
			mDestroyTagModeButton.Click += new RoutedEventHandler( mDestroyTagModeButton_Click );

			// Register handler for user accepting destroy tag prompt
			mDestroyTagPrompt.Accepted += new YesNoPrompt.PromptAcceptedHandler( mDestroyTagPrompt_Accepted );

			// Register handler for user accepting to tag a large number of assets.
			mProceedWithTaggingPrompt.Accepted += new YesNoPrompt.PromptAcceptedHandler( mProceedWithTaggingPrompt_Accepted );

			// Register handler for user accepting to untag a large number of assets.
			mProceedWithUntaggingPrompt.Accepted += new YesNoPrompt.PromptAcceptedHandler( mProceedWithUntaggingPrompt_Accepted );

			// Register handlers for Tag Management commands
			this.CommandBindings.Add( new CommandBinding( TagCommands.Create, CreateTagCommandHandler, CanManipulateTagsHandler ) );
			this.CommandBindings.Add( new CommandBinding( TagCommands.Rename, RenameTagCommandHandler, CanManipulateTagsHandler ) );
			this.CommandBindings.Add( new CommandBinding( TagCommands.CreateCopy, CreateCopyTagCommandHandler, CanManipulateTagsHandler ) );
			this.CommandBindings.Add( new CommandBinding( TagCommands.Destroy, DestroyTagCommandHandler, CanManipulateTagsHandler ) );


			UpdateNumSelectedAssetsLabel();
		}

		// Parameters passed to the dialog to indicate that a tag is being renamed or copies. (internal implementation detail.)
		struct TagCopyParams
		{
			public String CurrentTagName;
			public bool bDeleteOldTag;
		}

		/// Handler request to determine if we can execute tag management commands (we can do it if we are tag admin.)
		void CanManipulateTagsHandler(object sender, CanExecuteRoutedEventArgs e)
		{
			e.CanExecute = mMainControl != null && mMainControl.Backend.IsUserTagAdmin();
			e.Handled = true;
		}

		/// Handle the command to create a tag; pre-populate the group.
		void CreateTagCommandHandler( object sender, ExecutedRoutedEventArgs e )
		{
			// Get the visual and the actual tag item that this command is about
			ListBoxItem SourceVisual = (ListBoxItem)( e.OriginalSource );
			Tag TagOperand = (Tag)mTagsPalette.ItemContainerGenerator.ItemFromContainer( SourceVisual );

			// Set up the dialog prompt to reflect creating a new tag.
			mCreateTagPrompt.Message = Utils.Localize( "ContentBrowser_CreateTag_Message", TagOperand.FormattedFullName );
			mCreateTagPrompt.AcceptButtonLabel = Utils.Localize( "ContentBrowser_CreateTag_CreateButton" );
			mCreateTagPrompt.PlacementTarget = SourceVisual;
			mCreateTagPrompt.Parameters = null;

			// Prepopulate the Group box based on the tag that was clicked.
			int GroupNameIndex = mGroupOptions.FindIndex( GroupOptionName => GroupOptionName.Equals( TagOperand.GroupName, StringComparison.OrdinalIgnoreCase ) );
			mCreateTagPrompt.SelectedGroupIndexUponOpening = Math.Max( GroupNameIndex, 0 );

			mCreateTagPrompt.Show();

			e.Handled = true;
		}

		/// Handle the command to rename a tag.
		void RenameTagCommandHandler( object sender, ExecutedRoutedEventArgs e )
		{
			// Get the visual and the actual tag item that this command is about
			ListBoxItem SourceVisual = (ListBoxItem)( e.OriginalSource );
			Tag TagOperand = (Tag)mTagsPalette.ItemContainerGenerator.ItemFromContainer( SourceVisual );

			// Set up the dialog prompt to reflect renaming.
			mCreateTagPrompt.Message = Utils.Localize( "ContentBrowser_RenameTag_Message", TagOperand.FormattedFullName );
			mCreateTagPrompt.AcceptButtonLabel = Utils.Localize( "ContentBrowser_RenameTag_RenameButton" );
			mCreateTagPrompt.PlacementTarget = SourceVisual;
			mCreateTagPrompt.Parameters = new TagCopyParams { CurrentTagName = TagOperand.FullName, bDeleteOldTag = true };
			mCreateTagPrompt.Show();

			e.Handled = true;
		}

		/// Handle the command to create a copy of a tag (and apply copy to all assets tagged with the original tag).
		void CreateCopyTagCommandHandler( object sender, ExecutedRoutedEventArgs e )
		{
			// Get the visual and the actual tag item that this command is about
			ListBoxItem SourceVisual = (ListBoxItem)( e.OriginalSource );
			Tag TagOperand = (Tag)mTagsPalette.ItemContainerGenerator.ItemFromContainer( SourceVisual );

			// Set up the dialog prompt to reflect duplicating.
			mCreateTagPrompt.Message = Utils.Localize( "ContentBrowser_CopyTag_Message", TagOperand.FormattedFullName );
			mCreateTagPrompt.AcceptButtonLabel = Utils.Localize( "ContentBrowser_CopyTag_CreateCopyButton" );
			mCreateTagPrompt.PlacementTarget = SourceVisual;
			mCreateTagPrompt.Parameters = new TagCopyParams { CurrentTagName = TagOperand.FullName, bDeleteOldTag = false };
			mCreateTagPrompt.Show();

			e.Handled = true;
		}

		/// Destroy a tag.
		void DestroyTagCommandHandler( object sender, ExecutedRoutedEventArgs e )
		{
			// Get the visual and the actual tag item that this command is about
			ListBoxItem SourceVisual = (ListBoxItem)( e.OriginalSource );
			Tag TagOperand = (Tag)mTagsPalette.ItemContainerGenerator.ItemFromContainer( SourceVisual );

			// Set up the dialog prompt to reflect destroying a tag.
			mDestroyTagPrompt.PlacementTarget = SourceVisual;
			mDestroyTagPrompt.PromptText = Utils.Localize( "ContentBrowser_DestroyTagPrompt_PromptText", TagOperand.FullName );
			mDestroyTagPrompt.Show( TagOperand.FullName );

			e.Handled = true;
		}


		/// <summary>
		/// Make sure the proposed tag is valid.
		/// </summary>
		/// <param name="TagName">Proposed tag name to validate.</param>
		/// <param name="TagName">Proposed group name to validate.</param>
		/// 
		/// <returns></returns>
		String TagPromptValidator( String InTagName, String InGroupName, Object InTagCopyParams )
		{
			InGroupName = SanitizeGroupName( InGroupName );

			if ( InTagName == String.Empty )
			{
				return Utils.Localize("ContentBrowser_CreateTag_EmptyNameError");
			}
			else if ( !TagUtils.IsTagValid( InTagName ) || ( InGroupName!= null && InGroupName.Trim() != String.Empty && !TagUtils.IsTagValid( InGroupName )) )
			{
				return Utils.Localize( "ContentBrowser_CreateTag_DisallowedCharactersError" );
			}


			String ProposedTagName = TagUtils.CreateFullTagName( InGroupName, InTagName );

			if ( InTagCopyParams != null )
			{
				TagCopyParams CopyParams = (TagCopyParams)InTagCopyParams;
				
				// We are performing a rename or a copy
				if ( CopyParams.CurrentTagName.Equals( ProposedTagName, StringComparison.OrdinalIgnoreCase ) )
				{
					return Utils.Localize( "ContentBrowser_TagNamePrompt_NameCollision" );
				}
			}


			// Make sure that the tag doesn't already exist
			foreach( Tag CurTag in TagsCatalog )
			{
				if ( CurTag.FullName.Equals( ProposedTagName, StringComparison.OrdinalIgnoreCase ) )
				{
					// Name must not match an existing tag!
					return Utils.Localize( "ContentBrowser_CreateTag_NameCollision" );
				}
			}
			

			return null;
		}

		// Called when the user successfully entered a valid tag name to create a new tag
		void mCreateTagPrompt_Succeeded( String InAcceptedName, String InAcceptedGroupName, Object InTagCopyParams )
		{
			InAcceptedGroupName = SanitizeGroupName( InAcceptedGroupName );

			String ProposedFullName = TagUtils.CreateFullTagName( InAcceptedGroupName, InAcceptedName );

			if ( InTagCopyParams == null )
			{
				// There was no information present; create a tag.
				mMainControl.Backend.CreateTag( ProposedFullName );
			}
			else
			{
				// We are copying this tag or rename/moving it.
				TagCopyParams CopyParams = (TagCopyParams)InTagCopyParams;
				mMainControl.Backend.CopyTag( CopyParams.CurrentTagName, ProposedFullName, CopyParams.bDeleteOldTag );
				mMainControl.UpdateAssetsInView( RefreshFlags.KeepSelection );
			}

			mMainControl.Backend.UpdateTagsCatalogue();
			
		}

		/// Takes a group name and returns a cleaned up version (spaces removed); also takes into account special items selected in the prompt for tag creation/renaming, etc.
		String SanitizeGroupName( String GroupName )
		{
			if ( mCreateTagPrompt.SelectedGroupIndex == 0 )
			{
				// If the user selected the special -- None -- option then pretend they typed in nothing.
				return String.Empty;
			}
			else
			{
				return GroupName.Trim();
			}
		}


		#region InTagDestroyMode Property
		/// Gets/sets whether the AssetInspector is in TagDestroying mode. This is a dependency property.
		public bool InTagDestroyMode
		{
			get { return (bool)GetValue( InTagDestroyModeProperty ); }
			set { SetValue( InTagDestroyModeProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for InTagDestroyMode.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty InTagDestroyModeProperty =
			DependencyProperty.Register( "InTagDestroyMode", typeof( bool ), typeof( AssetInspector ), new UIPropertyMetadata( false, new PropertyChangedCallback(InTagDestroyModeChanged) ) );

		/// <summary>
		/// Called when the InTagDestroyMode property changes.
		/// </summary>
		/// <param name="sender">The dependency object on which the property changed (that is this AssetInspector)</param>
		/// <param name="args">The event arguments describing the change.</param>
		static void InTagDestroyModeChanged( DependencyObject sender, DependencyPropertyChangedEventArgs args )
		{
			AssetInspector ThisAssetInspector = sender as AssetInspector;
			if ( ThisAssetInspector.InTagDestroyMode )
			{
				ThisAssetInspector.mDestroyTagModeButton.Content = Utils.Localize( "ContentBrowser_AssetInspector_CancelDestroyTagMode" );
			}
			else
			{
				ThisAssetInspector.mDestroyTagModeButton.Content = Utils.Localize( "ContentBrowser_AssetInspector_ActivateDestroyTagMode" );
			}

			ThisAssetInspector.UpdateIsTaggingAllowed();
		}
		#endregion



		/// Called when the user clicks the (-) button to start destroying tags
		void mDestroyTagModeButton_Click( object sender, RoutedEventArgs e )
		{
			InTagDestroyMode = !InTagDestroyMode;			
		}

		/// Called when the user clicks the (+) button to start destroying tags
		void mCreateTagButton_Click( object sender, RoutedEventArgs e )
		{
			mCreateTagPrompt.Message = Utils.Localize( "ContentBrowser_CreateTag_Message" );
			mCreateTagPrompt.AcceptButtonLabel = Utils.Localize( "ContentBrowser_CreateTag_CreateButton" );
			mCreateTagPrompt.PlacementTarget = mTagsPaletteBorder;
			mCreateTagPrompt.Parameters = null;
			mCreateTagPrompt.Show();
		}


		#region IsTaggingAllowed Property
		// True when there are assets being inspected (the user has assets selected)
		private bool IsTaggingAllowed
		{
			get { return (bool)GetValue( IsTaggingAllowedProperty ); }
			set { SetValue( IsTaggingAllowedProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for IsTaggingAllowed.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsTaggingAllowedProperty =
			DependencyProperty.Register( "IsTaggingAllowed", typeof( bool ), typeof( AssetInspector ) );

		private void UpdateIsTaggingAllowed()
		{
			IsTaggingAllowed = m_AssetsUnderInspection.Count != 0 && !InTagDestroyMode;
		}

		#endregion



		// Update the label telling users how many assets they are inspecting.
		private void UpdateNumSelectedAssetsLabel()
		{
			//mNumAssets.Content = Utils.Localize( "ContentBrowser_AssetInspector_SelectionNote", m_AssetsUnderInspection.Count );
		}


		#region TagsPalette Filtering and Sorting

		/// Called when the user types in the Filter textbox
		void mTagsPaletteFilter_TextChanged( object sender, TextChangedEventArgs e )
		{
			TagFilterString = mTagsPaletteFilter.Text.Trim().ToLower();
			// Filter the tags palette
			CollectionViewSource.GetDefaultView( this.TagsCatalog ).Refresh();
		}

		/// Comparer used to sort tags
		class TagsComparer : IComparer
		{
			public static TagsComparer SharedInstance = new TagsComparer();
			public int Compare( object x, object y )
			{
				return (x as Tag).CompareTo(y as Tag);
			}
		}
		/// Predicate that filters tags
		Predicate<Object> FilterTagsPredicate;
		private String TagFilterString = "";
		/// Method for predicate that filters tag; shows tags that match the filter text in the mTagsPaletteFilter
		private bool FilterTagsMethod( Object CandidateTag )
		{
			String CandidateTagString = ( CandidateTag as Tag ).FullName.ToLower();
			bool PassesFilter = CandidateTagString.Contains( TagFilterString );
			return PassesFilter;
		}

		#endregion


		void mDestroyTagPrompt_Accepted( Object Parameters )
		{
			DestroyTag( (String)Parameters );
		}

		void DestroyTag( String TagToDestroy )
		{
			InTagDestroyMode = false;
			mMainControl.Backend.DestroyTag( TagToDestroy );
			mMainControl.Backend.UpdateTagsCatalogue();
			mMainControl.UpdateAssetsInView( RefreshFlags.KeepSelection );
		}

		/// Called when the user clicks on a ApplyTagButton in the TagsPalette (a button that tags any selected assets or deletes the tag depending on the mode)
		void TagPaletteTagButton_Click( object sender, RoutedEventArgs args )
		{
			TagVisual ClickedTagVisual = (TagVisual)sender;
			ContentBrowser.Tag ClickedTag = ClickedTagVisual.AssetTag;

			if ( !this.InTagDestroyMode )
			{
				bool ShouldShowConfirmationPrompt = mMainControl.Backend.ShouldShowConfirmationPrompt( ConfirmationPromptType.AddTagToAssets );
				if ( !ShouldShowConfirmationPrompt ||
					 m_AssetsUnderInspection.Count < ContentBrowser.ContentBrowserDefs.MaxNumAssetsForNoWarnGadOperation )
				{
					// Tag the assets with the tag we clicked
					mMainControl.Backend.AddTagToAssets( m_AssetsUnderInspection, ClickedTag.FullName );
					OnTagsModified();
				}
				else
				{
					// Prompt the user: did they really mean to tag this many assets
					mProceedWithTaggingPrompt.PromptText = Utils.Localize( "ContentBrowser_ApplyTagPrompt_Prompt", m_AssetsUnderInspection.Count );
					mProceedWithTaggingPrompt.PlacementTarget = (TagVisual)sender;
					mProceedWithTaggingPrompt.ShowOptionToSuppressFuturePrompts = true;
					mProceedWithTaggingPrompt.SuppressFuturePrompts = !ShouldShowConfirmationPrompt;
					mProceedWithTaggingPrompt.Show( ClickedTag.FullName );
				}
			}
			else
			{
				// Destroy the tag whose delete button we clicked on
				mDestroyTagPrompt.PlacementTarget = ClickedTagVisual;
				mDestroyTagPrompt.PromptText = Utils.Localize( "ContentBrowser_DestroyTagPrompt_PromptText", ClickedTag.FullName );
				mDestroyTagPrompt.Show( ClickedTag.FullName );
			}
		}

		/// The user accepted the prompt; wants to tag the assets.
		void mProceedWithTaggingPrompt_Accepted( object Parameters )
		{
			String TagToApply = (String)Parameters;
			mMainControl.Backend.AddTagToAssets( m_AssetsUnderInspection, TagToApply );
			OnTagsModified();

			if( mProceedWithTaggingPrompt.SuppressFuturePrompts )
			{
				mMainControl.Backend.DisableConfirmationPrompt( ConfirmationPromptType.AddTagToAssets );
			}
		}

		
		
		/// Hold on to a reference to the main ContentBrowser control
		private MainControl mMainControl;

		/// Storyboard that shows the TagVisual disappearing
		private Storyboard mTagVisualErase;

		#region TagsCatalog Property

		private ObservableCollection<Tag> TagsCatalog
		{
			get { return (ObservableCollection<Tag>)GetValue( TagsCatalogProperty ); }
			set { SetValue( TagsCatalogProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for TagsCatalog.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty TagsCatalogProperty =
			DependencyProperty.Register( "TagsCatalog", typeof( ObservableCollection<Tag> ), typeof( AssetInspector ), new UIPropertyMetadata( new ObservableCollection<Tag>() ) );

		#endregion

		private List<String> mGroupOptions;
		/// <summary>
		/// Sets the catalog of all tags in existence.
		/// </summary>
		/// <param name="InCatalog">The catalog of all known tags</param>
		/// <param name="InGroupNames">Set of group names from the incoming tags.</param>
		public void SetTagsCatalog( System.Collections.Generic.List<string> InCatalog, NameSet InGroupNames )
		{
			// Assign new tags to the tags list view
			ObservableCollection<Tag> TempTagsCatalog = new ObservableCollection<Tag>();
			foreach ( String TagName in InCatalog )
			{
			    TempTagsCatalog.Add( new Tag( TagName ) );
			}
			TagsCatalog = TempTagsCatalog;

			// Add sorting and filtering to TagsPalette
			ListCollectionView TagsCatalogView = (ListCollectionView)CollectionViewSource.GetDefaultView( TagsCatalog );
			TagsCatalogView.CustomSort = TagsComparer.SharedInstance;
			TagsCatalogView.Filter = FilterTagsPredicate;

			// Populate the existing Tag Groups into the combo box
			mGroupOptions = new List<String>( InGroupNames );
			mGroupOptions.Sort();
			mGroupOptions.Insert( 0, Utils.Localize( "ContentBrowser_NameEntryPrompt_NoGroup" ) );
			mCreateTagPrompt.SetGroupOptions( mGroupOptions );

			// Make the list show its items in groups
			TagsCatalogView.GroupDescriptions.Clear();
			TagsCatalogView.GroupDescriptions.Add( new PropertyGroupDescription( null, new TagToGroupConverter() ) );
			bool CanGroup = TagsCatalogView.CanGroup;			
		}
	
		/// Tag about to be removed from the AssetInspector
		private TagVisual mTagPendingRemoval = null;

		/// Called when the Animation for erasing the TagVisual is done playing
		private void mTagVisualErase_Completed( object sender, EventArgs e )
		{
			String Tag = ( mTagPendingRemoval ).AssetTag.FullName;
			mTagPendingRemoval = null;
			this.mMainControl.Backend.RemoveTagFromAssets( m_AssetsUnderInspection, Tag );
			OnTagsModified();
		}

		/// Handler that is called when user clicks on "tiny x" on a tag to remove it
		private TagVisual.ClickEventHandler RemoveTagClickedHandler;

		/// Method that is called when user clicks on the minus icon on a tag to remove it
		private void RemoveTagClicked( object sender, RoutedEventArgs args )
		{
			if ( mTagPendingRemoval == null )
			{
				bool ShouldShowConfirmationPrompt = mMainControl.Backend.ShouldShowConfirmationPrompt( ConfirmationPromptType.RemoveTagFromAssets );

				TagVisual TagToRemove = (TagVisual)sender;
				if ( !ShouldShowConfirmationPrompt ||
					 m_AssetsUnderInspection.Count < ContentBrowser.ContentBrowserDefs.MaxNumAssetsForNoWarnGadOperation )
				{
					mTagPendingRemoval = TagToRemove;
					mTagVisualErase.Begin( mTagPendingRemoval );
				}
				else
				{
					mProceedWithUntaggingPrompt.PromptText = Utils.Localize( "ContentBrowser_UntagPrompt_Prompt", m_AssetsUnderInspection.Count );
					mProceedWithUntaggingPrompt.PlacementTarget = (TagVisual)sender;					
					mProceedWithUntaggingPrompt.ShowOptionToSuppressFuturePrompts = true;
					mProceedWithUntaggingPrompt.SuppressFuturePrompts = !ShouldShowConfirmationPrompt;
					mProceedWithUntaggingPrompt.Show( TagToRemove );
				}
			}			
		}

		/// User accepted the prompt to untag a large number of assets.
		void mProceedWithUntaggingPrompt_Accepted( object Parameters )
		{
			mTagPendingRemoval = (TagVisual)Parameters;
			mTagVisualErase.Begin( mTagPendingRemoval );

			if( mProceedWithUntaggingPrompt.SuppressFuturePrompts )
			{
				mMainControl.Backend.DisableConfirmationPrompt( ConfirmationPromptType.RemoveTagFromAssets );
			}
		}
		
		/// Initialize the AssetInspector
		public void Init( MainControl InMainControl )
		{
			mMainControl = InMainControl;
			bool IsTagAdmin = mMainControl.Backend.IsUserTagAdmin();
			this.mCreateTagButton.IsEnabled = IsTagAdmin;
			this.mDestroyTagModeButton.IsEnabled = IsTagAdmin;
		}

		/// Method called when tags on the selected assets have been altered from within the AssetInspector
		private void OnTagsModified()
		{
			// Update all the selected assets; their tags changed
			foreach ( AssetItem Asset in m_AssetsUnderInspection )
			{
				mMainControl.MyAssets.UpdateAssetStatus( Asset, AssetStatusUpdateFlags.Tags );
			}
			mMainControl.RequestFilterRefresh( RefreshFlags.KeepSelection );

			Refresh();
		}

		/// Updates the visualization of the tags on Assets under inspection
		private void Refresh()
		{
			UpdateNumSelectedAssetsLabel();
			UpdateIsTaggingAllowed();

			TagUtils.GatherTagsFromAssets( m_AssetsUnderInspection, out mTagsOnAllAssets, out mTagsOnSomeAssets );

			if ( m_AssetsUnderInspection.Count == 0 )
			{
				mTagsWrapPanel.Children.Clear();
			}
			else
			{
				mTagsWrapPanel.Children.Clear();
				foreach ( String Tag in mTagsOnAllAssets )
				{
					TagVisual NewTagVisual = new TagVisual();
					NewTagVisual.AssetTag = new Tag(Tag);
					NewTagVisual.Click += RemoveTagClicked;
					mTagsWrapPanel.Children.Add( NewTagVisual );
				}

				foreach ( String Tag in mTagsOnSomeAssets )
				{
					TagVisual NewTagVisual = new TagVisual();
					NewTagVisual.AssetTag = new Tag(Tag);
					NewTagVisual.IsSemiPresent = true;
					NewTagVisual.Click += RemoveTagClicked;
					mTagsWrapPanel.Children.Add( NewTagVisual );
				}
			}

			bool bTagsPresent = mTagsOnAllAssets.Count > 0 || mTagsOnSomeAssets.Count > 0;
			mNoTagsLabel.Visibility = ( bTagsPresent ) ? Visibility.Collapsed : Visibility.Visible;
		}

        #region AssetsUnderInspection

		/// Tags present on all the assets in the selection.
        private List<String> mTagsOnAllAssets = new List<String>();
		
		/// Tags on only some of the assets in the selection
		private List<String> mTagsOnSomeAssets = new List<String>();

		/// <summary>
		/// Assets being viewed/modified by this AssetInspector.
		/// </summary>
        public ReadOnlyCollection<AssetItem> AssetsUnderInspection
        {
            set
            {
                m_AssetsUnderInspection = value;
				
				// selected assets changed; respond
				Refresh();				
			}
        }

		/// Assets currently being inspected by the AssetInspector
        private ReadOnlyCollection<AssetItem> m_AssetsUnderInspection = new List<AssetItem>().AsReadOnly();


        #endregion


    }
}
