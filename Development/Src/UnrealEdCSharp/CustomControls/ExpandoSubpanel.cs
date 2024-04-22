/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


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

namespace CustomControls
{

	[TemplatePart( Name = "PART_Header", Type = typeof( UIElement ) )]

	/// ExpandoSubpanels are meant to be inserted into an ExpandoPanel.
	/// An ExpandoSubpanel is a CellSizer that adds a collapse property.
	/// 
	/// An ExpandoPanel's MinHeight is defined implicitly my its header;
	/// MinHeight is the height of the header because the header should
	/// always remain visible.		
	public class ExpandoSubpanel : CellSizer
	{
		#region ToggleExpand Command

		private static RoutedUICommand mToggleExpansionCommand = new RoutedUICommand( "ToggleExpansion", "ToggleExpansionCommand", typeof( ExpandoSubpanel ) );
		/// ToggleExpansion command for ExpandoSubpanel. This command signals that a Subpanel should be expanded or collapsed.
		public static RoutedUICommand ToggleExpansionCommand { get { return mToggleExpansionCommand; } }

		/// Handle the expansion toggle command by toggling the IsCollapsed state.
		static void OnExpansionToggledCommand( Object Sender, ExecutedRoutedEventArgs e )
		{
			ExpandoSubpanel ExpansionToggleSender = (ExpandoSubpanel)Sender;
			ExpansionToggleSender.IsCollapsed = !ExpansionToggleSender.IsCollapsed;
		}


		#endregion

		/// Create the ExpandoPanel class.
		static ExpandoSubpanel()
		{
			// Override the metadata so that ExpandoSubpanel gets its own Style/Template to use.
			DefaultStyleKeyProperty.OverrideMetadata( typeof( ExpandoSubpanel ), new FrameworkPropertyMetadata( typeof( ExpandoSubpanel ) ) );

			// Register a handler for the ExpansionToggledCommand.
			CommandManager.RegisterClassCommandBinding( typeof( ExpandoSubpanel ),
														new CommandBinding( ToggleExpansionCommand, new ExecutedRoutedEventHandler( OnExpansionToggledCommand ) ) );
		}

		/// Create an ExpandoSubpanel.
		public ExpandoSubpanel()
		{
			// Create a handler for the header changing in size.
			HeaderSizeChangedHandler = new SizeChangedEventHandler( PART_Header_SizeChanged );
		}

		protected override void OnIsCollapsedPropertyChanged( DependencyPropertyChangedEventArgs EventArgs )
		{
			base.OnIsCollapsedPropertyChanged( EventArgs );

			// We must update the grip visibility and control sizing ourselves, because the Visibility of ExpandoSubpanels does not change when they are collapsed.
			UpdateGripVisibilityAndControlSizing();
		}


		/// Handle a new template being assigned to this instance of an ExpandoPanel.
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();
			
			// We expect that the ExpandoPanel's template has a PART_Header,
			// which is a ContentPresenter for the Header.
			if ( PART_Header != null )
			{
				PART_Header.SizeChanged -= HeaderSizeChangedHandler;
			}			
			PART_Header = (ContentPresenter)Template.FindName( "PART_Header", this );
			if (PART_Header != null)
			{
				PART_Header.SizeChanged += HeaderSizeChangedHandler;
			}
		}

		// The event handler for the header changing size.
		SizeChangedEventHandler HeaderSizeChangedHandler;
		/// When the header changes size we should adjust the MinHeight of the ExpandoSubpanel.
		void PART_Header_SizeChanged( object sender, SizeChangedEventArgs e )
		{
			this.MinHeight = PART_Header.ActualHeight + GripSize;
		}

		/// A handle to the Header ContentPresenter in the template.
		ContentPresenter PART_Header = null;
	}
}
