//=============================================================================
//	Assetcompete.xaml.cs: Custom control for auto completing text fields
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


namespace CustomControls
{
	/// <summary>
	/// Interaction logic for AutocompleteTextbox.xaml
	/// </summary>
	public partial class Autocomplete : UserControl
	{
		/// <summary>
		/// Reason why a suggestion box is being triggered.
		/// </summary>
		public enum Trigger
		{
			/// <summary>A character was typed; it is the first character in this textbox so there is no context.</summary>
			FirstCharTyped,
			/// <summary>A shortcut key was pressed by the user (e.g. ctrl+space in visual studio)</summary>
			ShortcutTriggered
		}

		/// <summary>
		/// A view of the dictionary; facilitates filtering of irrelevant suggestions
		/// </summary>
		private CollectionViewSource m_DictionaryView;

		private List<char> mWordDelimiters;
		/// <summary>
		/// List of characters that delimit words
		/// </summary>
		public List<char> WordDelimiters
		{
			get { return mWordDelimiters; }
			set { mWordDelimiters = value; }
		}

		#region AutocompleteScopeDelim

		private List<char> mScopeDelimiters;
		/// <summary>
		/// List of characters that delimit scope in names (e.g. dot/period char in "Material.MaterialName")
		/// </summary>
		public List<char> ScopeDelimiters
		{
			get { return mScopeDelimiters; }
			set { mScopeDelimiters = value; }
		}

		private int mScopeDepth;
		/// <summary>
		/// The current scope depth at the caret (how many ScopeDelimiters are between the start of the current word and the caret)
		/// </summary>
		public int ScopeDepth
		{
			get { return mScopeDepth; }
		}

		/// <summary>
		/// Raised when the ScopeDepth changes.
		/// </summary>
		public static readonly RoutedEvent ScopeDepthChangedEvent = EventManager.RegisterRoutedEvent(
			"ScopeDepthChanged",
			RoutingStrategy.Bubble,
			typeof(ScopeDepthChangedEventHandler),
			typeof(Autocomplete));

		/// <summary>
		/// Handler for ScopeDepthChanged Event
		/// </summary>
		/// <param name="sender">The Autocomplete control sending the event.</param>
		/// <param name="args">Why the suggestion box is being triggered.</param>
		public delegate void ScopeDepthChangedEventHandler(object sender, RoutedEventArgs args);

		/// <summary>
		/// Raised when the suggestion box is triggered by the user.
		/// </summary>
		public event ScopeDepthChangedEventHandler ScopeDepthChanged
		{
			add { AddHandler(ScopeDepthChangedEvent, value); }
			remove { RemoveHandler(ScopeDepthChangedEvent, value); }
		}
		#endregion

		/// <summary>
		/// List of keys that accept the current suggestion but do not get typed into the textbox
		/// </summary>
        private List<Key> m_AcceptAndEatInputKeylist;
        public List<Key> AcceptAndEatInputKeylist
        {
            get { return m_AcceptAndEatInputKeylist; }
            set { m_AcceptAndEatInputKeylist = value; }
        }
	
        /// <summary>
        /// List of keys that accept the current suggestion and are also typed into the textbox
        /// </summary>
        private List<Key> m_AcceptAndWriteThroughKeylist;
        public List<Key> AcceptAndWriteThroughKeylist
        {
            get { return m_AcceptAndWriteThroughKeylist; }
            set { m_AcceptAndWriteThroughKeylist = value; }
        }

        /// <summary>
        /// List of keys that dismiss the suggestion box without 
        /// </summary>
        private List<Key> m_EscapeAutocompleteKeylist;
        public List<Key> EscapeAutocompleteKeylist
        {
            get { return m_EscapeAutocompleteKeylist; }
            set { m_EscapeAutocompleteKeylist = value; }
        }
	

		#region AutocmpleteTriggered
		/// <summary>
		/// Raised when the suggestion box is triggered by the user.
		/// </summary>
		public static readonly RoutedEvent TriggeredEvent = EventManager.RegisterRoutedEvent(
			"Triggered",
			RoutingStrategy.Bubble,
			typeof( TriggeredEventHandler),
			typeof(Autocomplete));

		/// <summary>
		/// Handler for Triggered Event; triggered when the suggestion box becomes open due to user input
		/// </summary>
		/// <param name="sender">The Autocomplete control sending the event.</param>
		/// <param name="args">Why the suggestion box is being triggered.</param>
		public delegate void TriggeredEventHandler( object sender, TriggeredEventArgs args );

		/// <summary>
		/// Raised when the suggestion box is triggered by the user.
		/// </summary>
		public event TriggeredEventHandler Triggered
		{
			add { AddHandler(TriggeredEvent, value); }
			remove { RemoveHandler( TriggeredEvent, value ); }
        }
        #endregion


		#region Dictionary Property
		/// <summary>
		/// A list of possible suggestions
		/// <remarks>This is a DependencyProperty</remarks>
		/// </summary>
		public List<String> Dictionary
		{
			get { return ( List<String> )GetValue( DictionaryProperty ); }
			set { SetValue( DictionaryProperty, value ); }
		}


		/// <summary>
		/// Using a DependencyProperty as the backing store for Dictionary.  This enables animation, styling, binding, etc...
		/// </summary>
		public static readonly DependencyProperty DictionaryProperty =
			DependencyProperty.Register( "Dictionary",
										 typeof( List<String> ),
										 typeof( Autocomplete ),
										 new UIPropertyMetadata( new List<String>() ) );

		#endregion

		
		/// <summary>
		/// Event handler that triggers when the TextBoxControl text changes.
		/// </summary>
		private TextChangedEventHandler TextChangedHandler;

		
		/// <summary>
		/// Event handler that triggers when a TextBoxControl gets a tunneling KeyDown event.
		/// </summary>
		private KeyEventHandler KeyDownHandler;


		#region TextBoxControl Property
		/// <summary>
		/// The TextBox under Autocomplete; we will autocomplete words typed into this TextBox
		/// </summary>
		public TextBox TextBoxControl
		{
			get { return ( TextBox )GetValue( TextBoxControlProperty ); }
			set { SetValue( TextBoxControlProperty, value ); }
		}


		/// <summary>
		/// Using a DependencyProperty as the backing store for TextBoxControl.  This enables animation, styling, binding, etc...
		/// </summary>
		public static readonly DependencyProperty TextBoxControlProperty =
			DependencyProperty.Register( "TextBoxControl",
										 typeof( TextBox ),
										 typeof( Autocomplete ),
										 new UIPropertyMetadata( null, OnTextBoxControlInvalidated )
										 );


		/// <summary>
		/// Handler that occurs when the TextBoxControl Property is set.
		/// </summary>
		/// <param name="d">The dependencty object (our Autocomplete Control) for which the property is changed.</param>
		/// <param name="e">Information about how the property changed.</param>
		private static void OnTextBoxControlInvalidated( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			Autocomplete autocomplete = ( Autocomplete )d;
			
			TextBox OldTextBox = ( TextBox )e.OldValue;
			TextBox NewTextBox = ( TextBox )e.NewValue;

			if ( OldTextBox != null )
			{
				OldTextBox.TextChanged -= autocomplete.TextChangedHandler;
				OldTextBox.PreviewKeyDown -= autocomplete.KeyDownHandler;
			}

			if ( NewTextBox != null )
			{
				NewTextBox.TextChanged += autocomplete.TextChangedHandler;
				NewTextBox.PreviewKeyDown += autocomplete.KeyDownHandler;
			}

			autocomplete.m_AutocompletePopup.PlacementTarget = NewTextBox;
			
		}
		#endregion


		/// <summary>
		/// Default Contructor.
		/// </summary>
		public Autocomplete()
		{

			InitializeComponent();

			this.DataContext = this;
			WordToAutocomplete = "";

			m_DictionaryView = ( CollectionViewSource )this.Resources["dictionaryViewSource"];
			//dictionaryView.Filter += new FilterEventHandler( ViewSource_Filter ); // THIS IS SLOW

			m_AutocompletePopup.CustomPopupPlacementCallback = new CustomPopupPlacementCallback(AutocompletePopupPlacement);

			TextChangedHandler = new TextChangedEventHandler( TextBoxControl_TextChanged );
			KeyDownHandler = new KeyEventHandler( Autocomplete_KeyDown );

			mWordDelimiters = new List<char>( new char[] { ' ', ',' } );
			mScopeDelimiters = new List<char>();
			AcceptAndEatInputKeylist = new List<Key>( new Key[]{ Key.Enter, Key.Tab } );
			AcceptAndWriteThroughKeylist = new List<Key>( new Key[]{ Key.Space } );
			EscapeAutocompleteKeylist = new List<Key>( new Key[]{ Key.Escape } );

			SuggestionsFilter = new Predicate<object>( FilterSuggestionsMethod );
		}


		/// <summary>
		/// The word between the character that triggered the autocomplete and the cursor position.
		/// <example>For example, in "I like apples that are swee|" the WordToAutocomplete is "swee".</example>
		/// </summary>
        private String m_WordToAutocomplete;
        public String WordToAutocomplete
        {
            get { return m_WordToAutocomplete; }
            set
			{
				m_WordToAutocomplete = value;
				WordToAutocomplete_SubstringSearch = "." + WordToAutocomplete;
			}
        }
		/// WordToAutocomplete with a . pre-pended to it.
		/// This is useful in suggesting 'Category.Word' when the user is typing 'Word'
		private String WordToAutocomplete_SubstringSearch { get; set; }

	
        /// <summary>
		/// The position of the character at the start of the current word that we are autocompleting.
		/// <example>For example, in "Digital inter|" the WordStartPosition is the 'i' in "inter"</example>
		/// </summary>
		public int WordStartPosition { get { return m_WordStartPosition; } }
		private int m_WordStartPosition = 0;

		
		/// <summary>
		/// The position of the character before the cursor.
		/// <example>For example, in "Digital inter|" the CharBeforeCarretPosition is the 'r' in "inter"</example>
		/// </summary>
		public int CharBeforeCarretPosition { get { return charBeforeCarretPosition; } }
		private int charBeforeCarretPosition = 0;

		/// Determine if the text change is likely a result of a paste.
		private bool IsLikelyPaste( ICollection<TextChange> Changes )
		{
			foreach( TextChange Delta in Changes )
			{
				if (Delta.AddedLength > 1)
				{
					// We added more than one character in a single change, meaning that we likely pasted.
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Called when the text in the TextBoxControl changes.
		/// </summary>
		/// <param name="sender">The object that generated the event.</param>
		/// <param name="e">Arguments describing the text change.</param>
		private void TextBoxControl_TextChanged( object sender, TextChangedEventArgs e )
		{
			bool LikelyPasted = IsLikelyPaste(e.Changes);

			// Did the user type in a complete word that's in our dictionary?
            bool GotPerfectMatch = false;
            
			charBeforeCarretPosition = Math.Max( 0, TextBoxControl.CaretIndex - 1 );
            int charBeforeLastTyped = charBeforeCarretPosition - 1;

            if ( TextBoxControl.CaretIndex > 0 )
			{
				// Find the start of the string being autocompleted
				m_WordStartPosition = 0;
                if (m_WordStartPosition == 0)
                {
					foreach (char Delimiter in WordDelimiters)
					{
						m_WordStartPosition = Math.Max(m_WordStartPosition, TextBoxControl.Text.LastIndexOf(Delimiter, charBeforeCarretPosition) + 1);
					}
                }

				WordToAutocomplete = TextBoxControl.Text.Substring( m_WordStartPosition, charBeforeCarretPosition - m_WordStartPosition + 1 );

                GotPerfectMatch = this.Dictionary.Contains( WordToAutocomplete ); 

                if ( !GotPerfectMatch )
                {
                    // If we just typed the first character in the autocomplete box or the character before the one we just typed is a delimiter
					if (CharBeforeCarretPosition == 0 || WordDelimiters.Contains(TextBoxControl.Text[charBeforeLastTyped]))
				    {
                        RaiseTriggeredEvent( Trigger.FirstCharTyped );
				    }
                }
			}
			else
			{
				WordToAutocomplete = String.Empty;
			}

			// calculate scope depth based on WordToAutocomplete and raise the changed event if it's changed
			// must happen and update the dictionary before calling m_DictionaryView.View.Refresh() below
			int newScopeDepth = 0;
			foreach (char c in WordToAutocomplete)
			{
				if (ScopeDelimiters.Contains(c)) newScopeDepth++;
			}
			if (ScopeDepth != newScopeDepth)
			{
				mScopeDepth = newScopeDepth;
				RaiseEvent(new RoutedEventArgs(ScopeDepthChangedEvent));
			}

			// Show the suggestion box
			if ( WordToAutocomplete != String.Empty && !LikelyPasted )
			{
				m_DictionaryView.View.Filter = SuggestionsFilter;
				m_AutocompletePopup.IsOpen = !m_DictionaryView.View.IsEmpty && !GotPerfectMatch;
			}
			else
			{
				m_AutocompletePopup.IsOpen = false;
			}

			// Select the first element in the list (list guaranteed to be non-empty since popup is open)
			if (m_AutocompletePopup.IsOpen)
			{
				m_DictionaryView.View.MoveCurrentToFirst();
				m_AutocompletePopup.Focus();
			}
		}

		/// <summary>
		/// Raises a Triggered Event.
		/// </summary>
		/// <param name="LastTypedCharPosition">The position of CharThatTriggered in the TextBoxControl.</param>
		/// <param name="Cause">What caused the suggestion to be triggered.</param>
		private void RaiseTriggeredEvent( Autocomplete.Trigger Cause )
		{
			TriggeredEventArgs args = new TriggeredEventArgs( Cause );
			args.RoutedEvent = Autocomplete.TriggeredEvent;
			RaiseEvent( args );
		}


		/// <summary>
		/// Called when a key is pressed in either the TextBox or the Autocomplete suggestions box.
		/// </summary>
		/// <param name="sender">The object generating the event</param>
		/// <param name="e">The arguments describing the key press</param>
		private void Autocomplete_KeyDown( object sender, KeyEventArgs e )
		{
			if (m_AutocompletePopup.IsOpen)
			{
				if ( e.Key == Key.Up )
				{
					m_DictionaryView.View.MoveCurrentToPrevious();
					if ( m_DictionaryView.View.IsCurrentBeforeFirst )
					{
						m_DictionaryView.View.MoveCurrentToFirst();
					}
					e.Handled = true;
				}
				else if ( e.Key == Key.Down )
				{
					m_DictionaryView.View.MoveCurrentToNext();
					if ( m_DictionaryView.View.IsCurrentAfterLast )
					{
						m_DictionaryView.View.MoveCurrentToLast();
					}
					e.Handled = true;
				}
				else if ( e.Key == Key.End )
				{
					m_DictionaryView.View.MoveCurrentToLast();
					e.Handled = true;
				}
				else if ( e.Key == Key.Home )
				{
					m_DictionaryView.View.MoveCurrentToFirst();
					e.Handled = true;
				}
				else if ( e.Key == Key.PageDown )
				{
					// TODO: make this work, perhaps try a different approach
				}
				else if ( e.Key == Key.PageUp )
				{
					// TODO: make this work, perhaps try a different approach
				}
				else if ( AcceptAndEatInputKeylist.Contains( e.Key ) )
				{
					AcceptSuggestion();
					e.Handled = true;
				}
				else if ( AcceptAndWriteThroughKeylist.Contains( e.Key ) )
				{
					AcceptSuggestion();
					e.Handled = false;
				}
				else if ( EscapeAutocompleteKeylist.Contains( e.Key ) )
				{
					EscapeAutoComplete();
					e.Handled = true;
				}

				m_SuggestionListBox.ScrollIntoView( m_DictionaryView.View.CurrentItem );
			}
		}

		/// <summary>
		/// Close the autocomplete suggestion box; do not accept he suggestion.
		/// </summary>
		private void EscapeAutoComplete()
		{
			m_AutocompletePopup.IsOpen = false;
		}

		/// <summary>
		/// Accept the suggestion from the autocomplete box.
		/// (1) Replace the text between the character that triggered the autocomplete and the currenet carret position with the suggestion.
		/// (2) Close the suggestion box.
		/// </summary>
		private void AcceptSuggestion()
		{
			m_AutocompletePopup.IsOpen = false;

			String acceptedAutocompleteWord = ( String )m_DictionaryView.View.CurrentItem;
			TextBoxControl.Text = TextBoxControl.Text.Remove( m_WordStartPosition, charBeforeCarretPosition - m_WordStartPosition + 1 );
			TextBoxControl.Text = TextBoxControl.Text.Insert( m_WordStartPosition, acceptedAutocompleteWord );
			TextBoxControl.CaretIndex = m_WordStartPosition + acceptedAutocompleteWord.Length;
		}

		/// <summary>
		/// Event that filters out irrelevant suggestions.
		/// </summary>
		/// <param name="sender">Object that sent the event.</param>
		/// <param name="e">Filter parameters.</param>
		[Obsolete("Replaced by SuggestionsFilter predicate")]
		private void ViewSource_Filter( object sender, FilterEventArgs e )
		{
			String candidate = e.Item as String;
			if ( candidate != null )
			{
				if ( candidate.StartsWith( WordToAutocomplete, StringComparison.OrdinalIgnoreCase ) )
				{
					e.Accepted = true;
				}
				else
				{
					e.Accepted = false;
				}
			}
		}

		Predicate<object> SuggestionsFilter;
		private bool FilterSuggestionsMethod(object suggestion)
		{
			String PossibleSuggestion = (string)suggestion;
			bool bStartsWith = PossibleSuggestion.StartsWith( WordToAutocomplete, StringComparison.OrdinalIgnoreCase );
			return bStartsWith || -1 != PossibleSuggestion.IndexOf( WordToAutocomplete_SubstringSearch, StringComparison.OrdinalIgnoreCase );
		}

		/// <summary>
		/// Calculates the location of the pop up; places the pop up below the text being autocompleted.
		/// </summary>
		/// <param name="popupSize">Ignored.</param>
		/// <param name="targetSize">Ignored.</param>
		/// <param name="offset">Ignored.</param>
		/// <returns>An array containing a single desired pop up location.</returns>
		private CustomPopupPlacement[] AutocompletePopupPlacement( Size popupSize,
												 Size targetSize,
												 Point offset )
		{
			Rect charBoundingBox = TextBoxControl.GetRectFromCharacterIndex( m_WordStartPosition );

			CustomPopupPlacement placement =
				new CustomPopupPlacement( charBoundingBox.BottomLeft, PopupPrimaryAxis.Vertical );	

			return new CustomPopupPlacement[] { placement };
		}

		/// <summary>
		/// Handler for clicking on an item in the list box.
		/// Clicking on an items accepts the suggestion that is selected by the click.
		/// </summary>
		/// <param name="sender">The object sending the event.</param>
		/// <param name="e">Parameters describing the clicke event.</param>
		private void SuggestionBox_MouseUp( object sender, MouseButtonEventArgs e )
		{
			AcceptSuggestion();
		}
	}

	public class TriggeredEventArgs : RoutedEventArgs
	{
		public TriggeredEventArgs( Autocomplete.Trigger CauseIn )
		{
			Cause = CauseIn;
		}

		/// <summary>
		/// What caused the suggestion box to be triggered
		/// </summary>
        private Autocomplete.Trigger m_Cause;
		public Autocomplete.Trigger Cause
        {
            get { return m_Cause; }
            private set { m_Cause = value; }
        }
	}
}
