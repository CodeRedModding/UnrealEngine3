//=============================================================================
//	TagBox.xaml.cs: User control for a tag editing box
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


namespace CustomControls
{
	/// <summary>
	/// Interaction logic for AutocompleteTextbox.xaml
	/// </summary>
    public partial class TagBox : UserControl
    {
        /// <summary>
        /// Construct a TagBox
        /// </summary>
        public TagBox()
		{
			InitializeComponent();

			// Set up an Adorner that highlights the unrecognized and invalid tags
			{
				AdornerLayer MyAdornerLayer = AdornerLayer.GetAdornerLayer(m_TextBox);
				m_TagBoxAdorner = new TagBoxAdorner(m_TextBox);
				m_TagBoxAdorner.IsHitTestVisible = false;
				MyAdornerLayer.Add(m_TagBoxAdorner);
			}

			// Register changer for text changing in the textbox
			m_TextBox.TextChanged += new TextChangedEventHandler(m_TextBox_TextChanged);
			
			// Register handler for textbox losing focus
			m_TextBox.LostKeyboardFocus += new KeyboardFocusChangedEventHandler(m_TextBox_LostKeyboardFocus);

			// Register handler for the TextBox size changing
			this.m_TextBox.SizeChanged += new SizeChangedEventHandler(m_TextBox_SizeChanged);

			// Register handler for the TextBox selection changing
			this.m_TextBox.SelectionChanged += new RoutedEventHandler(m_TextBox_SelectionChanged);

			// Register handler for text changing in the textbox
			this.m_TextBox.TextChanged += new TextChangedEventHandler(m_TextBox_TextChanged);

			// Register handler for key being pushed down for the textbox
			this.m_TextBox.KeyDown += new KeyEventHandler(m_TextBox_KeyDown);

			// Change the key bindings such that "Enter" accepts and applies changes
			m_Autocomplete.AcceptAndEatInputKeylist = new List<Key>(new Key[] { Key.Tab });
			m_Autocomplete.AcceptAndWriteThroughKeylist = new List<Key>(new Key[] { Key.Enter, Key.Space });
			m_Autocomplete.WordDelimiters = new List<char>(TagUtils.TagDelimiterArray);


			// Get reference to the TagsModified animation
			m_TagsModifiedPulse = (Storyboard)this.Resources["TagsModifiedPulse"];

			// Get reference to the ErrorsPresent animation (when attempting to tag modify and there are errors)
			m_ErrorsPresentPulse = (Storyboard)this.Resources["ErrorsPresentPulse"];
		}

		/// Animation to play when tags have been modified
		private Storyboard m_TagsModifiedPulse;

		/// Animation to play when errors present during tag modification attempt
		private Storyboard m_ErrorsPresentPulse;

		/// <summary>
		/// Focus the Tag box
		/// </summary>
		public void FocusSelf()
		{
			m_TextBox.Focus();
		}

		/// Pulse the tagbox background with blue for "your changes have been applied"
		public void PulseTagsModified()
		{
			m_TagsModifiedPulse.Begin(m_TextBox);
		}

		/// Pulse the tagbox background with red for "there are errors present"
		public void PulseErrorsPresent()
		{
			m_ErrorsPresentPulse.Begin(m_TextBox);
		}


        #region Events

        /// <summary>
        /// Method signature for delegates that are invoked when tags change or need to be applied
        /// </summary>
        /// <param name="sender">The TagBox which has had its tags changed.</param>
        /// <param name="IsEphemeralChange">Ephemeral - this change is a result of adding/removing/altering one or more valid tags. Permanent - enter was pressed or focus was lost; we should apply the changes.</param>
        /// <param name="RecognizedTags">After the changes have been applied these are the recognized tags in the tag box.</param>
        /// <param name="UnrecognizedTags">After the changes have been applied these are the "unrecognized tags" (just words).</param>
        public delegate void TagsChangedEventHandler(TagBox sender, bool IsEphemeralChange, ReadOnlyCollection<String> RecognizedTags, ReadOnlyCollection<String> UnrecognizedTags);

        /// <summary>
        /// TagsChanged is triggered when some tags in the TagBox change (ephemeral change).
        /// </summary>
        public event TagsChangedEventHandler TagsChanged;

        /// <summary>
        /// ApplyTagChanges is triggered when the tagbox loses focus or the user presses enter.
        /// </summary>
        public event TagsChangedEventHandler ApplyTagChanges;

		
		/// Method signature for handlers invoked when a user wants to cancel any un-applied changes.
		public delegate void CancelChangesEventHandler();

		/// Event triggered when the user wants to cancel changes; e.g. the user presses Escape.
		public event CancelChangesEventHandler CancelChanges;

		

        #endregion


        /// <summary>
        /// Clear all the tags/text in the box
        /// </summary>
        public void Clear()
        {
            this.m_TextBox.Clear();
        }


        /// <summary>
        /// Set the tags in the tagbox.
        /// </summary>
        /// <param name="TagIn">A list of space-delimited tags</param>
        public void SetTags(String TagsIn)
        {
            this.m_TextBox.Text = TagsIn;
			this.m_TextBox.CaretIndex = this.m_TextBox.Text.Length;
        }


        /// <summary>
        /// Exposes the textbox's text wrapping property.
        /// @todo Is there an elegant way to expose this as a dependency property?
        /// </summary>
        public TextWrapping TextWrapping
        {
            get { return m_TextBox.TextWrapping; }
            set { m_TextBox.TextWrapping = value; }
        }


        /// <summary>
        /// A catalog of all the known tags
        /// </summary>
        private Dictionary<String, String> m_ValidTags = new Dictionary<String, String>();


        /// <summary>
        /// Set the list of all the known tags
        /// </summary>
        /// <param name="DictionaryIn">The list of all the known tags to use.</param>
        public void SetTagsCatalog(List<String> DictionaryIn)
        {
            m_ValidTags.Clear();
            foreach (String Word in DictionaryIn)
            {
                m_ValidTags.Add(Word, Word);
            }

            m_Autocomplete.Dictionary = DictionaryIn;
        }


        /// <summary>
        /// The valid tags in this tagbox (tags that are recognized by the system)
        /// </summary>
        private List<String> m_RecognizedTags = new List<String>();
        public ReadOnlyCollection<String> RecognizedTags { get { return m_RecognizedTags.AsReadOnly(); } }


        /// <summary>
        /// The tags typed into this tagbox that are not recognized by the system.
        /// </summary>
        private List<String> m_UnrecognizedTags = new List<String>();
        public ReadOnlyCollection<String> UnrecognizedTags { get { return m_UnrecognizedTags.AsReadOnly(); } }


        /// <summary>
        /// Adorner that renders the unrecognized tag overlay.
        /// </summary>
        private TagBoxAdorner m_TagBoxAdorner;


        /// <summary>
        /// All the tags in this tagbox
        /// </summary>
        private List<String> m_Tags = new List<String>();
        public ReadOnlyCollection<String> AllTags { get { return m_Tags.AsReadOnly(); } }


		/// What kind of error?
        public enum ErrorCause : int
        {
			/// Bad characters
            SyntaxError,
			/// Unrecognized tag
            SemanticError
        }
		/// An error
        public struct TagBoxError
        {
			/// What kind of error; syntax or semantic
            public ErrorCause Cause;
			/// Where in the tagbox this error is.
            public Rect Location;
        }
		
		/// List of errors
        private List<TagBoxError> m_TagErrors = new List<TagBoxError>();

        /// Iterate over tags in the tagbox and discover any errors.
        private void UpdateTagErrors()
        {
            m_TagErrors.Clear();

            String Text = m_TextBox.Text;


            // Find any words that are not in the dictionary or have invalid characters and add them to the list of errors
            int StartIdx = 0;
            while (StartIdx < m_TextBox.Text.Length)
            {
				// Skip all tag delimeters
                if ( !TagUtils.IsDelimiter(Text[StartIdx]) )
                {
					// Find the end of the word that we are on
					int EndOfWord = Text.IndexOfAny(TagUtils.TagDelimiterArray, StartIdx);
                    if (EndOfWord < 0)
                    {
                        EndOfWord = Text.Length;
                    }

					// Extract the word that we are testing (wish I had string slices)
                    String WordToTest = Text.Substring(StartIdx, EndOfWord - StartIdx);
					// Do we not recognize this word as a tag?
                    bool bIsTagUnknown = !m_ValidTags.ContainsKey(WordToTest);
					// Is this a syntactically invalid tag?
					bool bIsTagInvalid = !TagUtils.IsTagValid(WordToTest);

					// If it's an unknown or invalid tag then make an add an error to the list to reflect this
                    if ( bIsTagUnknown || bIsTagInvalid )
                    {
                        ErrorCause ThisErrorCause = bIsTagInvalid ? ErrorCause.SyntaxError : ErrorCause.SemanticError;

                        int StartLine = m_TextBox.GetLineIndexFromCharacterIndex(StartIdx);
                        int EndLine = m_TextBox.GetLineIndexFromCharacterIndex(EndOfWord);

                        if (StartLine != EndLine)
                        {
                            // From the start of word to the end of that line
                            AddError( StartIdx,  m_TextBox.GetCharacterIndexFromLineIndex(StartLine+1) - 1, ThisErrorCause);

                            // From the start of the end line to the last character
                            AddError( m_TextBox.GetCharacterIndexFromLineIndex(EndLine), EndOfWord, ThisErrorCause);

                            // All the lines in between are errors
                            for (int ErrorLine = StartLine+1; ErrorLine < EndLine; ++ErrorLine)
                            {
                                // Entire line is an error
                                AddError(
                                    m_TextBox.GetCharacterIndexFromLineIndex(ErrorLine),
                                    m_TextBox.GetCharacterIndexFromLineIndex(ErrorLine+1)-1,
                                    ThisErrorCause);
                            }
                        }
                        else
                        {
                            // The simplest case; the error is all on one line
                            AddError(StartIdx, EndOfWord, ThisErrorCause);
                        }
                        

                        

                    }

                    StartIdx = EndOfWord;
                }

                ++StartIdx;

            }
            
        }

        /// <summary>
        /// Helper method for adding errors visualization
        /// </summary>
        /// <param name="StartIndex">The index of the character on which the error starts</param>
        /// <param name="EndIndex">The index of the character on which the error ends</param>
        /// <param name="Cause">What causes this error: syntax or semantics</param>
        private void AddError(int StartIndex, int EndIndex, ErrorCause Cause)
        {
            // Draw a rectangle signifying that this word is valid
            Rect StartRect = m_TextBox.GetRectFromCharacterIndex(StartIndex);
            Rect EndRect = m_TextBox.GetRectFromCharacterIndex(EndIndex);

            // Add to a list of errors
            TagBoxError NewError = new TagBoxError();
            NewError.Location = new Rect(StartRect.TopLeft, EndRect.BottomRight);
            NewError.Cause = Cause;
            m_TagErrors.Add(NewError);

        }


        /// <summary>
        /// Returns true when there are syntax errors present in the TagBox
        /// </summary>
        public bool HasSyntaxErrors
        {
            get
            {
                foreach (TagBoxError TagError in m_TagErrors)
                {
                    if (TagError.Cause == ErrorCause.SyntaxError)
                    {
                        return true;
                    }
                }

                return false;
            }
		}

		#region Textbox Handlers

		/// Handle textbox size changing
		private void m_TextBox_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            UpdateTagErrors();
        }

		/// Handle textbox selection changing
		private void m_TextBox_SelectionChanged(object sender, RoutedEventArgs e)
        {
            UpdateTagErrors();
            m_TagBoxAdorner.TriggerUpdate(m_TagErrors);
        }

		/// Handle textbox text changing
        private void m_TextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            UpdateTagErrors();
            m_TagBoxAdorner.TriggerUpdate(m_TagErrors);
            RaiseTagsChangedEventIfTagsChanged();
        }

		/// Handle textbox losing keyboard focus
        private void m_TextBox_LostKeyboardFocus( object sender, KeyboardFocusChangedEventArgs e )
        {
            RaiseTagsChangedEventIfTagsChanged();
            RaiseApplyTagChangesEvent();
        }

		/// Handle textbox key being pressed        
        private void m_TextBox_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                RaiseApplyTagChangesEvent();
            }
			else if (e.Key == Key.Escape)
			{
				if (CancelChanges != null)
				{
					this.CancelChanges();
				}
			}
		}

		#endregion


		/// Raises a RagsChanged event if the list of known tags in the tagbox have changed.
		private void RaiseTagsChangedEventIfTagsChanged()
        {
            List<String> NewTags = TagUtils.TagsStringToTagSet(m_TextBox.Text);
            List<String> NewRecognizedTags = new List<String>();
            List<String> NewUnrecognizedTags = new List<String>();
            
            RecognizeTags(NewTags, NewRecognizedTags, NewUnrecognizedTags);

            bool ShouldRaiseEvent = !TagUtils.AreTagSetsEquivalent(NewRecognizedTags, m_RecognizedTags);
            
            m_Tags = NewTags;
            m_RecognizedTags = NewRecognizedTags;
            m_UnrecognizedTags = NewUnrecognizedTags;

            if (ShouldRaiseEvent)
            {
                RaiseTagsChangedEvent();
            }

        }

		/// Raise the event signifying that tag changes should be applied (i.e. focus was lost or enter was pressed)
        private void RaiseApplyTagChangesEvent()
        {
            if (ApplyTagChanges != null)
            {
                ApplyTagChanges(this, false, m_RecognizedTags.AsReadOnly(), m_UnrecognizedTags.AsReadOnly());
            }
        }

        /// Raise the event signifying that tags in the listbox have changed.
		private void RaiseTagsChangedEvent()
        {
            if (TagsChanged != null)
            {
                TagsChanged(this, true, m_RecognizedTags.AsReadOnly(), m_UnrecognizedTags.AsReadOnly());
            }
        }


        /// <summary>
        /// Sorts tags into RecognizedTags and UnrecognizedTags.
        /// </summary>
        /// <param name="Tags">Tags to be sorted</param>
        /// <param name="RecognizedTags">All the tags that were recognized. (This collection will be cleared)</param>
        /// <param name="UnrecognizedTags">All the tags that were unrecognized. (This collection will be cleared)</param>
        private void RecognizeTags(List<String> Tags, List<String> RecognizedTags, List<String> UnrecognizedTags)
        {
            RecognizedTags.Clear();
            UnrecognizedTags.Clear();
            foreach (String Tag in Tags)
            {
                if ( m_ValidTags.ContainsKey(Tag) )
                {
                    RecognizedTags.Add(Tag);
                }
                else
                {
                    UnrecognizedTags.Add(Tag);
                }
            }
        }
        
    }


}
