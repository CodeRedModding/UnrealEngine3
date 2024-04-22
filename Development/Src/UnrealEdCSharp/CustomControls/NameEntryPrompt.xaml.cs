//=============================================================================
//	NameEntryPrompt.xaml.cs: User control that prompts the user to enter a string.
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
using ContentBrowser;
using System.Text.RegularExpressions;
using System.Windows.Media.Animation;
using System.Collections;


namespace CustomControls
{
	/// <summary>
	/// Interaction logic for NameEntryPrompt.xaml
	/// </summary>
	public partial class NameEntryPrompt : UserControl
	{
		/// Construct a SortableColumnHeader
		public NameEntryPrompt()
		{
			InitializeComponent();

			// Register handler for OK button in Add Collection prompt being clicked
			this.mOkButton.Click += new RoutedEventHandler( mOkButton_Click );
			// Register handler for user typing into the AddCollection name textbox
			this.mNameTextBox.TextChanged += new TextChangedEventHandler( mNameTextBox_TextChanged );
			// Register handler for user hitting enter/esc while adding Collections
			this.mPopup.KeyDown += new KeyEventHandler( mPopup_KeyUp );
			// Register handler for user clicking on x to close prompt
			this.mCloseButton.Click += new RoutedEventHandler( mCloseButton_Click );
			// Register handler for popup opening
			mPopup.Opened += new EventHandler( mPopup_Opened );

			// Set a default validator in case the user does not set one
			this.SetValidator( new Validator( DefaultValidatorMethod ) );
		}

		/// Workaround to get a TextChanged event from a Editable Combo Box.
		private String ComboBoxText
		{
			get { return (String)GetValue( ComboBoxTextProperty ); }
			set { SetValue( ComboBoxTextProperty, value ); }
		}
		public static readonly DependencyProperty ComboBoxTextProperty =
			DependencyProperty.Register( "ComboBoxText", typeof( String ), typeof( NameEntryPrompt ), new UIPropertyMetadata( String.Empty, ComboBoxTextPropertyChanged) );

		private static void ComboBoxTextPropertyChanged( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			NameEntryPrompt This = (NameEntryPrompt)d;
			This.Validate();
		}



		/// Called when the popup is opened
		void mPopup_Opened( object sender, EventArgs e )
		{

			mGroupComboBox.SelectedIndex = SelectedGroupIndexUponOpening;
			SelectedGroupIndexUponOpening = 0;

			mNameTextBox.Text = String.Empty;
			Validate();

			// Focus the most appropriate element
			if ( SupportsGroupName )
			{
				Keyboard.Focus( mGroupComboBox );
			}
			else
			{
				Keyboard.Focus( mNameTextBox );
			}
		}

		/// <summary>
		/// Check the input for validity, display error message.
		/// </summary>
		/// <returns>True if input is valid, false otherwise</returns>
		bool Validate()
		{
			String ErrorText = mValidator( mNameTextBox.Text, ComboBoxText, Parameters );
			bool bValid = ErrorText == null || ErrorText == String.Empty;

			mErrorLabel.Text = ErrorText;
			mErrorLabel.Visibility = ( bValid ) ? Visibility.Collapsed : Visibility.Visible;

			mOkButton.IsEnabled = bValid;

			return bValid;
		}
		
		/// <summary>
		/// Delegate validation to the user. Should test NameIn for validity and return the error text as a string. Empty of null return means no errors.
		/// </summary>
		/// <param name="InName">The user input to validate</param>
		/// <param name="InGroupName">Optional group name to validate; ignored unless SupportsGroupName is true.</param>
		/// <returns>The error text; null or String.Empty for no error.</returns>
		public delegate String Validator( String InName, String InGroupname, Object Parameters  );
		/// Used to validate user input
		private Validator mValidator;
		/// <summary>
		/// Set a custom user input validator.
		/// </summary>
		/// <param name="CustomValidator">The Validator used to test input</param>
		/// <see cref="CustomControls.NameEntryPrompt.Validator"/>
		public void SetValidator( Validator CustomValidator )
		{
			mValidator = CustomValidator;
		}

		/// <summary>
		/// Set the list of options that will appear in the editable Group Combo Box.
		/// </summary>
		/// <param name="InGroupOptions"> List of options from which the user can choose. </param>
		public void SetGroupOptions( List<String> InGroupOptions )
		{
			mGroupComboBox.ItemsSource = InGroupOptions;
		}

		public int SelectedGroupIndex { get { return mGroupComboBox.SelectedIndex; } }
		public int SelectedGroupIndexUponOpening { get; set; }

		/// <summary>
		/// Default validator implementation in case the user does not set one
		/// </summary>
		/// <param name="NewName">User-input name to test.</param>
		/// <param name="NewGroupName">User-input group name to test.</param>
		/// <returns>Error string; null if valid</returns>
		String DefaultValidatorMethod( String NewName, String NewGroupName, Object Parameters )
		{
			if ( NewName.Length <= 0 || NewGroupName.Length <= 0 )
			{
				//Names must not be empty
				return UnrealEd.Utils.Localize( "ContentBrowser_NameEntryPrompt_EmptyNameNotAllowed" );
			}

			return null;
		}

		/// <summary>
		/// Handler type for notifying the user that their input succeeded
		/// </summary>
		/// <param name="AcceptedName">The input name that was accepted.</param>
		public delegate void SucceededHandler( String AcceptedName, String AcceptedGroupName, Object Parameters );
		/// Event fired when 
		public event SucceededHandler Succeeded;

		/// Show the prompt
		public void Show()
		{
			mPopup.IsOpen = true;
		}

		/// Hide the prompt
		public void Hide()
		{
			mPopup.IsOpen = false;
		}


		/// Handle user clicking on OK button in Add Collection
		void mOkButton_Click( object sender, RoutedEventArgs e )
		{
			if ( Validate() && Succeeded != null )
			{
				Succeeded( mNameTextBox.Text, ComboBoxText, Parameters );
				this.Hide();
				e.Handled = true;
			}			
		}
		
		/// Handle the text changing in the add collection textbox
		void mNameTextBox_TextChanged( object sender, TextChangedEventArgs e )
		{
			Validate();
		}

		/// Handle keypresses such as Enter and Esc. during Collection Adding
		void mPopup_KeyUp( object sender, KeyEventArgs e )
		{
			if ( e.Key == Key.Enter )
			{
				if ( Validate() && Succeeded != null )
				{
					Succeeded( mNameTextBox.Text, ComboBoxText, Parameters );
					this.Hide();
					e.Handled = true;
				}
			}
			else if ( e.Key == Key.Escape )
			{
				this.Hide();
				e.Handled = true;
			}
		}

		/// Handler user clicking X to close the name entry prompt.
		void mCloseButton_Click( object sender, RoutedEventArgs e )
		{
			this.Hide();
			e.Handled = true;
		}
		

		/// <summary>
		/// The placement of this prompt poppup. This is a dependency property.
		/// </summary>
		public PlacementMode Placement
		{
			get { return (PlacementMode)GetValue( PlacementProperty ); }
			set { SetValue( PlacementProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for Placement.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty PlacementProperty =
			DependencyProperty.Register( "Placement", typeof( PlacementMode ), typeof( NameEntryPrompt ), new UIPropertyMetadata( PlacementMode.Bottom ) );




		/// <summary>
		/// The element relative to which we are placing. This is a dependency property.
		/// </summary>
		public UIElement PlacementTarget
		{
			get { return (UIElement)GetValue( PlacementTargetProperty ); } 
			set { SetValue( PlacementTargetProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for PlacementTarget.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty PlacementTargetProperty =
			DependencyProperty.Register( "PlacementTarget", typeof( UIElement ), typeof( NameEntryPrompt ), new UIPropertyMetadata( null ) );



		/// <summary>
		/// The message that prompts the user. This is a dependency property.
		/// </summary>
		public String Message
		{
			get { return (String)GetValue( MessageProperty ); }
			set { SetValue( MessageProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for Message.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty MessageProperty =
			DependencyProperty.Register( "Message", typeof( String ), typeof( NameEntryPrompt ), new UIPropertyMetadata( UnrealEd.Utils.Localize( "ContentBrowser_NameEntryPrompt_EnterNamePrompt" ) ) );


		/// <summary>
		/// The label on the "OK" button. This is a dependency property.
		/// </summary>
		public String AcceptButtonLabel
		{
			get { return (String)GetValue( AcceptButtonLabelProperty ); }
			set { SetValue( AcceptButtonLabelProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for AcceptButtonLabel.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty AcceptButtonLabelProperty =
			DependencyProperty.Register( "AcceptButtonLabel", typeof( String ), typeof( NameEntryPrompt ), new UIPropertyMetadata( UnrealEd.Utils.Localize( "ContentBrowser_NameEntryPrompt_Ok" ) ) );



		/// Whether this dialog supports entering a group name. This is a dependency property.
		public bool SupportsGroupName
		{
			get { return (bool)GetValue( SupportsGroupNameProperty ); }
			set { SetValue( SupportsGroupNameProperty, value ); }
		}
		public static readonly DependencyProperty SupportsGroupNameProperty =
			DependencyProperty.Register( "SupportsGroupName", typeof( bool ), typeof( NameEntryPrompt ), new UIPropertyMetadata( false ) );


		/// An optional, user-provided parameter that will be passed back to the user upon prompt acceptance.
		public Object Parameters { get; set; }



	}
}
