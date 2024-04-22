/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;


namespace CustomControls
{
	/// UnrealTextBox adds an in-textbox gray hint text and a clear button to the generic text box control.
	public class UnrealTextBox : System.Windows.Controls.TextBox
	{

		/// Backstore for the ClearTextCommand
		private static RoutedUICommand mClearTextCommand = new RoutedUICommand( "Clear Text", "ClearTextCommand", typeof( UnrealTextBox ) );

		/// The ClearTextCommand - Clear the Text
		public static RoutedUICommand ClearTextCommand { get { return mClearTextCommand; } }


		static UnrealTextBox()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( UnrealTextBox ), new FrameworkPropertyMetadata( typeof( UnrealTextBox ) ) );
		}

		/// Default Constructor
		public UnrealTextBox()
			:
			base()
		{
			// Handle clear text commands; 
			CommandManager.RegisterClassCommandBinding(
				typeof( UnrealTextBox ),
				new CommandBinding( mClearTextCommand, new ExecutedRoutedEventHandler( OnClearText ) ) );
		}


		/// Handle the ClearTextCommand - clear the text in the decorated textbox
		static void OnClearText( Object Sender, ExecutedRoutedEventArgs e )
		{
			UnrealTextBox MyUnrealTextBox = (UnrealTextBox)Sender;
			MyUnrealTextBox.Text = "";
			Keyboard.Focus( MyUnrealTextBox );
		}


		/// We hide the hint text if there is text in the textbox; show the hint if the textbox is empty.
		protected override void OnTextChanged( TextChangedEventArgs e )
		{
			base.OnTextChanged( e );

			HintTextVisibility = ( this.Text == String.Empty ) ? Visibility.Visible : Visibility.Hidden;
			CanClearText = this.Text != String.Empty;
			e.Handled = true;
		}


		#region HintTextVisibilityProperty

		/// <summary>
		/// Whether the HintTextIsVisible
		/// </summary>
		private Visibility HintTextVisibility
		{
			get { return (Visibility)GetValue( HintTextVisibilityProperty ); }
			set { SetValue( HintTextVisibilityProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for HintTextVisibility.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty HintTextVisibilityProperty =
			DependencyProperty.Register( "HintTextVisibility", typeof( Visibility ), typeof( UnrealTextBox ), new UIPropertyMetadata( Visibility.Visible ) );

		#endregion


		#region HintTextProperty

		/// The hint text that appears to the user when the textbox is empty
		public String HintText
		{
			get { return (String)GetValue( HintTextProperty ); }
			set { SetValue( HintTextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for HintText.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty HintTextProperty =
			DependencyProperty.Register( "HintText", typeof( String ), typeof( UnrealTextBox ), new UIPropertyMetadata( "" ) );

		#endregion



		#region CanClearTextProperty

		public bool CanClearText
		{
			get { return (bool)GetValue( CanClearTextProperty ); }
			set { SetValue( CanClearTextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for CanClearText.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty CanClearTextProperty =
			DependencyProperty.Register( "CanClearText", typeof( bool ), typeof( UnrealTextBox ), new UIPropertyMetadata( false ) );

		#endregion





		#region ShowClearButtonProperty

		public bool ShowClearButton
		{
			get { return (bool)GetValue( ShowClearButtonProperty ); }
			set { SetValue( ShowClearButtonProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for ShowClearButton.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty ShowClearButtonProperty =
			DependencyProperty.Register( "ShowClearButton", typeof( bool ), typeof( UnrealTextBox ), new UIPropertyMetadata( true ) );


		#endregion


		#region HintTextAlignmentProperty

		/// Alignment of the hint text label.
		public HorizontalAlignment HintTextAlignment
		{
			get { return (HorizontalAlignment)GetValue( HintTextAlignmentProperty ); }
			set { SetValue( HintTextAlignmentProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for HintTextAlignment.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty HintTextAlignmentProperty =
			DependencyProperty.Register( "HintTextAlignment", typeof( HorizontalAlignment ), typeof( UnrealTextBox ), new UIPropertyMetadata( HorizontalAlignment.Center ) );


		#endregion



	}

    // EnterUpdateTextBox: A TextBox that updates its source after pressing Enter
	public class EnterUpdateTextBox : System.Windows.Controls.TextBox
	{
        protected override void OnKeyUp(KeyEventArgs Args)
        {
            base.OnKeyUp(Args);
            if (Args.Key == Key.Enter)
            {
                string OldText = Text;
                GetBindingExpression(TextProperty).UpdateSource();
                // refresh contents, in case the property adjusted them
                GetBindingExpression(TextProperty).UpdateTarget();
                if (OldText != Text)
                {
                    // scroll to end if text changed
                    ScrollToEnd();
                    SelectionStart = Text.Length;
                }
                Args.Handled = true;
            }
        }

        protected override void OnGotFocus(RoutedEventArgs Args)
        {
            base.OnGotFocus(Args);
            SelectAll();
        }

        protected override void OnLostFocus(RoutedEventArgs Args)
        {
            string OldText = Text;
            base.OnLostFocus(Args);
            // refresh contents, in case the property adjusted them
            GetBindingExpression(TextProperty).UpdateTarget();
            if (OldText != Text)
            {
                // scroll to end if text changed
                ScrollToEnd();
                SelectionStart = Text.Length;
            }
        }
    }
}