//=============================================================================
//	YesNoPrompt.xaml.cs: A modeless confirm / deny prompt.
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Linq;
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
using UnrealEd;
using System.Windows.Controls.Primitives;
using System.Diagnostics;

namespace CustomControls
{
	/// <summary>
	/// Interaction logic for YesNoPrompt.xaml
	/// </summary>
	public partial class YesNoPrompt : UserControl
	{
		public YesNoPrompt()
		{
			InitializeComponent();

			mConfirmButton.Click += new RoutedEventHandler( mConfirmButton_Click );
			mDenyButton.Click += new RoutedEventHandler( mDenyButton_Click );

			this.GotFocus += new RoutedEventHandler( FocusChangedHandler );
			mConfirmButton.LostFocus += new RoutedEventHandler( FocusChangedHandler );
			mDenyButton.LostFocus += new RoutedEventHandler( FocusChangedHandler );

			
			
			Keyboard.Focus( mDenyButton );
			
		}

		void FocusChangedHandler( object sender, RoutedEventArgs e )
		{
			Debug.WriteLine( Keyboard.FocusedElement.ToString() );
		}

		/// Delegate to be called when the prompt is accepted.
		public delegate void PromptAcceptedHandler( Object Parameters );

		/// This event is invoked when the prompt is accepted.
		public event PromptAcceptedHandler Accepted;		

		void mDenyButton_Click( object sender, RoutedEventArgs e )
		{
			Hide();
		}

		void mConfirmButton_Click( object sender, RoutedEventArgs e )
		{
			if ( Accepted != null )
			{
				Accepted( Parameters );
			}
			Hide();
		}

		/// <summary>
		/// Show the prompt
		/// </summary>
		/// <param name="InParameters">Optional (i.e. can be null) parameter that will be passed back to the user upon prompt acceptance</param>
		public void Show( Object InParameters )
		{
			this.mPopup.IsOpen = true;
			this.Parameters = InParameters;

			mConfirmButton.IsDefault = false;
			mDenyButton.IsDefault = false;
			Keyboard.Focus( mButtonContainer );
			if ( AffirmativeIsDefault )
			{
				mConfirmButton.IsDefault = true;
				Keyboard.Focus( this.mConfirmButton );
			}
			else
			{
				mDenyButton.IsDefault = true;
				Keyboard.Focus( this.mDenyButton );
			}
			this.mWarningTextBlock.Visibility = ( this.WarningText.Length > 0 ) ? Visibility.Visible : Visibility.Collapsed;
		}

		/// Hide the prompt
		void Hide()
		{
			this.mPopup.IsOpen = false;
		}

		/// An optional, user-provided parameter that will be passed back to the user upon prompt acceptance.
		Object Parameters { get; set; }

		#region PromptText Property
		
		/// The prompt text for the user; e.g. Do you want to destroy the selected collection?
		public String PromptText
		{
			get { return (String)GetValue( PromptTextProperty ); }
			set { SetValue( PromptTextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for PromptText.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty PromptTextProperty =
			DependencyProperty.Register( "PromptText", typeof( String ), typeof( YesNoPrompt ), new UIPropertyMetadata( "" ) );

		#endregion


		#region WarningText Property

		/// The warning text; e.g. Warning: This action cannot be undone.
		public String WarningText
		{
			get { return (String)GetValue( WarningTextProperty ); }
			set { SetValue( WarningTextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for WarningText.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty WarningTextProperty =
			DependencyProperty.Register( "WarningText", typeof( String ), typeof( YesNoPrompt ), new UIPropertyMetadata( "" ) );

		#endregion


		#region AffirmativeText Property

		/// Text on the confirm button: e.g. Destroy or Yes
		public String AffirmativeText
		{
			get { return (String)GetValue( AffirmativeTextProperty ); }
			set { SetValue( AffirmativeTextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for AffirmativeText.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty AffirmativeTextProperty =
			DependencyProperty.Register( "AffirmativeText", typeof( String ), typeof( YesNoPrompt ), new UIPropertyMetadata( Utils.Localize("ContentBrowser_YesNoPrompt_Yes") ) );

		#endregion


		#region NegativeText Property

		/// Text on the deny button: e.g. Cancel or No
		public String NegativeText
		{
			get { return (String)GetValue( NegativeTextProperty ); }
			set { SetValue( NegativeTextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for NegativeText.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty NegativeTextProperty =
			DependencyProperty.Register( "NegativeText", typeof( String ), typeof( YesNoPrompt ), new UIPropertyMetadata( Utils.Localize( "ContentBrowser_YesNoPrompt_No" ) ) );

		#endregion


		#region Placement Property

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
			DependencyProperty.Register( "Placement", typeof( PlacementMode ), typeof( YesNoPrompt ), new UIPropertyMetadata( PlacementMode.Bottom ) );

		#endregion


		#region PlacementTarget Property
		
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
			DependencyProperty.Register( "PlacementTarget", typeof( UIElement ), typeof( YesNoPrompt ), new UIPropertyMetadata( null ) );
		
		#endregion


		#region AffirmativeIsDefault Property
		
		/// Get or set whether the default button should be 'Yes' or 'No'. This is a dependency property.
		public bool AffirmativeIsDefault
		{
			get { return (bool)GetValue( AffirmativeIsDefaultProperty ); }
			set { SetValue( AffirmativeIsDefaultProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for AffirmativeIsDefault.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty AffirmativeIsDefaultProperty =
			DependencyProperty.Register( "AffirmativeIsDefault", typeof( bool ), typeof( YesNoPrompt ), new UIPropertyMetadata( true ) );

		#endregion


		#region ShowOptionToSuppressFuturePrompts

		/// Sets whether or not the "Don't show this prompt again this session" check box should be displayed.
		/// This setting should be configured before the dialog is shown.
		public bool ShowOptionToSuppressFuturePrompts
		{
			get { return (bool)GetValue( ShowOptionToSuppressFuturePromptsProperty ); }
			set { SetValue( ShowOptionToSuppressFuturePromptsProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for ShowOptionToSuppressFuturePrompts.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty ShowOptionToSuppressFuturePromptsProperty =
			DependencyProperty.Register( "ShowOptionToSuppressFuturePrompts", typeof( bool ), typeof( YesNoPrompt ), new UIPropertyMetadata( false ) );
		
		#endregion



		#region SuppressFuturePrompts

		/// Whether the user wants to suppress future prompts.  The initial value should be set before the
		/// dialog is displayed.  After the dialog is dismissed, you grab the result from this property.
		/// Remember that the result will be populated even if the user presses Cancel!
		public bool SuppressFuturePrompts
		{
			get { return (bool)GetValue( SuppressFuturePromptsProperty ); }
			set { SetValue( SuppressFuturePromptsProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for SuppressFuturePrompts.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty SuppressFuturePromptsProperty =
			DependencyProperty.Register( "SuppressFuturePrompts", typeof( bool ), typeof( YesNoPrompt ), new UIPropertyMetadata( false ) );

		#endregion
	}
}
