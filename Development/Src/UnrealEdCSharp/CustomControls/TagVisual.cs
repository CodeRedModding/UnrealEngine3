//=============================================================================
//	TagVisual.cs: Tag visual custom control
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


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
	/// <summary>
	/// ========================================
	/// .NET Framework 3.0 Custom Control
	/// ========================================
	///
	/// Follow steps 1a or 1b and then 2 to use this custom control in a XAML file.
	///
	/// Step 1a) Using this custom control in a XAML file that exists in the current project.
	/// Add this XmlNamespace attribute to the root element of the markup file where it is 
	/// to be used:
	///
	///     xmlns:MyNamespace="clr-namespace:ProvingGround"
	///
	///
	/// Step 1b) Using this custom control in a XAML file that exists in a different project.
	/// Add this XmlNamespace attribute to the root element of the markup file where it is 
	/// to be used:
	///
	///     xmlns:MyNamespace="clr-namespace:ProvingGround;assembly=ProvingGround"
	///
	/// You will also need to add a project reference from the project where the XAML file lives
	/// to this project and Rebuild to avoid compilation errors:
	///
	///     Right click on the target project in the Solution Explorer and
	///     "Add Reference"->"Projects"->[Browse to and select this project]
	///
	///
	/// Step 2)
	/// Go ahead and use your control in the XAML file. Note that Intellisense in the
	/// XML editor does not currently work on custom controls and its child elements.
	///
	///     <MyNamespace:TagVisual/>
	///
	/// </summary>
	public class TagVisual : System.Windows.Controls.Control
	{
		static TagVisual()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( TagVisual ), new FrameworkPropertyMetadata( typeof( TagVisual ) ) );

			CommandManager.RegisterClassCommandBinding( typeof( TagVisual ),
														new CommandBinding( mClickCommand, new ExecutedRoutedEventHandler( OnClick ) ) );
		}

		static void OnClick( Object Sender, ExecutedRoutedEventArgs e )
		{
			TagVisual ClickSender = (TagVisual)Sender;
			RoutedEventArgs NewEventArgs = new RoutedEventArgs();
			NewEventArgs.RoutedEvent = ClickEvent;
			NewEventArgs.Source = ClickSender;
			ClickSender.RaiseEvent( NewEventArgs );
		}

		/// The click command which occurs when someone clicks on a button
		private static RoutedUICommand mClickCommand = new RoutedUICommand( "Click", "ClickCommand", typeof( TagVisual ) );
		public static RoutedUICommand ClickCommand { get { return mClickCommand; } }

		#region Events
		public static readonly RoutedEvent ClickEvent = EventManager.RegisterRoutedEvent(
			"Click",
			RoutingStrategy.Bubble,
			typeof( ClickEventHandler ),
			typeof( TagVisual ) );

		public delegate void ClickEventHandler( object sender, RoutedEventArgs args );

		public event ClickEventHandler Click
		{
			add { AddHandler( ClickEvent, value ); }
			remove { RemoveHandler( ClickEvent, value ); }
		}
		#endregion


		#region Properties

		/// Get or set whether the TagVisual is semi-present.
		/// E.g. A tag that is on some but not all items in a selection is "SemiPresent". This is a dependency property.
		public bool IsSemiPresent
		{
			get { return (bool)GetValue( IsSemiPresentProperty ); }
			set { SetValue( IsSemiPresentProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for SemiPresent.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsSemiPresentProperty =
			DependencyProperty.Register( "IsSemiPresent", typeof( bool ), typeof( TagVisual ), new UIPropertyMetadata( false ) );


		/// The tag that we're visualizing. This is a dependency property.
		public ContentBrowser.Tag AssetTag
		{
			get { return (ContentBrowser.Tag)GetValue( AssetTagProperty ); }
			set { SetValue( AssetTagProperty, value ); }
		}
		public static readonly DependencyProperty AssetTagProperty =
			DependencyProperty.Register( "AssetTag", typeof( ContentBrowser.Tag ), typeof( TagVisual ), new UIPropertyMetadata( null ) );


		/// Are we allowed to apply this tag to the current selection?
		/// Useful when a tag visual is used as an element that applies the tag that it represents to some entity.
		/// This is a dependency property.
		public bool IsTagginAllowed
		{
			get { return (bool)GetValue( IsTagginAllowedProperty ); }
			set { SetValue( IsTagginAllowedProperty, value ); }
		}
		public static readonly DependencyProperty IsTagginAllowedProperty =
			DependencyProperty.Register( "IsTagginAllowed", typeof( bool ), typeof( TagVisual ), new UIPropertyMetadata( true ) );



		#endregion

	}
}
