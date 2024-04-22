/**
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

using System.Diagnostics;

namespace CustomControls
{

	/// <summary>
	/// The ExpandoPanel is a stack of resizeable and collapseable panels.
	/// Resizing the ExpandoPanel itself re-distributes space between all the non-collapsed panels.
	/// 
	/// To accomplish this we leverage the DockPanel where all the Subpanels are docked to the
	/// top the moment that they are added.
	/// When a subpanel is resized its adjacent subpanel is adjusted by the same amount.
	/// When a subpanel is toggled (collapsed or expanded) all the subpanels beneath it are adjusted to reflect
	/// this change. If the bottommost subpanel is toggled the space is distributed between
	/// all the subpanels in the Expando panel.
	/// 
	/// NOTE: Removing Subpanels is currently not supported.
	/// </summary>
	public class ExpandoPanel : DockPanel
	{
		/// Register some metadata properties about ExpandoPanel
		static ExpandoPanel()
		{			
			DefaultStyleKeyProperty.OverrideMetadata( typeof( ExpandoPanel ), new FrameworkPropertyMetadata( typeof( ExpandoPanel ) ) );
			// We are re-using the DockPanel; set the LastChildFill property to false by default as we will manage this ourselves.
			LastChildFillProperty.OverrideMetadata( typeof(ExpandoPanel), new FrameworkPropertyMetadata(false) );
		}

		/// Create an ExpandoPanel.
		public ExpandoPanel()
		{
			// Register for changes in the ExpandoPanel because we need to adjust all the subpanels
			// when this happens.
			this.SizeChanged += new SizeChangedEventHandler( ExpandoPanel_SizeChanged );
		}

		/// A flag that lets us know whether we need to re-distribute the available space.
		bool HeightDistributionDirty = false;
		/// Any panels above the SharingBoundary will be immune from space sharing.
		ExpandoSubpanel SpaceSharingBoundary = null;
		/// A list of ExpandoSubpanel panels that we are managing.
		List<ExpandoSubpanel> PanelsToManage = null;

		/// <summary>
		/// Handle Subpanels being added and removed. Note that removing Subpanels is not currently supported.
		/// </summary>
		/// <param name="visualAdded"> A Subpanel added. </param>
		/// <param name="visualRemoved"> A Subpanel removed. </param>
		protected override void OnVisualChildrenChanged( DependencyObject visualAdded, DependencyObject visualRemoved )
		{
			if (visualAdded != null)
			{
				ExpandoSubpanel NewSubpanel = (ExpandoSubpanel)visualAdded;
				// Make sure all subpanels are arranged vertically.
				SetDock( NewSubpanel, Dock.Top );
				// Register a handler for the subpanel being resized.
				NewSubpanel.ResizedByUser += new CellSizer.SizeAlteredHandler( OnSubpanelResized );
				// Register a handler for the subpanel being toggled.
				NewSubpanel.CollapsedToggled += new CellSizer.SizeAlteredHandler( OnSubpanelCollapseToggled );

				base.OnVisualChildrenChanged( visualAdded, visualRemoved );

				// Build up a list of panels whose sizes we are managing (mostly for convenience).
				PanelsToManage = new List<ExpandoSubpanel>();
				foreach ( UIElement Child in Children )
				{
					if ( Child is ExpandoSubpanel )
					{
						ExpandoSubpanel MySubpanel = Child as ExpandoSubpanel;
						PanelsToManage.Add( MySubpanel );
					}
				}
			}
		}

		/// When a subpanel is resized we must adjust the adjacent subpanel.
		void OnSubpanelResized( CellSizer Sender, Size OldSize )
		{
			AdjustAdjacentPanel( (ExpandoSubpanel)Sender, OldSize );
		}

		/// When a subpanel is toggled, we must re-distribute the vertical space.
		/// Usually we try to affect only the subpanel below the toggled one.
		/// If the bottommost subpanel is toggled, we affect the spacing of
		/// all the other panels.
		void OnSubpanelCollapseToggled( CellSizer Sender, Size OldSize )
		{
			// Remember which subpanel was toggled. Ideally, collapsing a subpanel affects the
			// spacing of the subpanels below it.
			SpaceSharingBoundary = Sender as ExpandoSubpanel;

			// However, if this is the bottommost expanded panel.
			// then we must affect all the non-collapsed panels evenly.
			{
				// Find which panel the user just resized.
				int ToggledPanelIndex = PanelsToManage.IndexOf( SpaceSharingBoundary );

				// Find the next non-collapsed Subpanel.
				int NonCollapsedPanelIndex = PanelsToManage.FindIndex( ToggledPanelIndex + 1, delegate( ExpandoSubpanel Candidate ) { return !Candidate.IsCollapsed; } );
				if ( NonCollapsedPanelIndex == -1 )
				{
					// If it does not exist then we must re-distribute space evenly.
					SpaceSharingBoundary = null;
				}
			}
			
			HeightDistributionDirty = true;
		}

		/// When the toplevel panel changes size we must re-distribute the space
		/// equally between all the subpanels.
		void ExpandoPanel_SizeChanged( object sender, SizeChangedEventArgs e )
		{
			SpaceSharingBoundary = null;
			DistributeAvailHeight();
		}

		/// Given the ResizedSubpanel and its OldSize, adjust the first expanded subpanel below
		/// it such that vertical space appears to have been re-distributed to the user.
		void AdjustAdjacentPanel( ExpandoSubpanel ResizedSubpanel, Size OldSize )
		{
			// Find which panel the user just resized.
			int ResizedPanelIndex = PanelsToManage.IndexOf( ResizedSubpanel );
			
			// Find the next non-collapsed Subpanel.
			int AdjacentPanelIndex = PanelsToManage.FindIndex( ResizedPanelIndex + 1, delegate( ExpandoSubpanel Candidate ) { return !Candidate.IsCollapsed; } );

			// There is no room for this panel to change size...
			if ( AdjacentPanelIndex == -1 )
			{
				// ... so force it to its old size.
				ResizedSubpanel.Height = OldSize.Height;
			}
			else
			{
				ExpandoSubpanel AdjacentSubpanel = PanelsToManage[ AdjacentPanelIndex ];
				// Figure out how much the user resized the subpanel.
				// Note that this might be more than the AdjacentPanel can accomodate.
				double DesiredDelta = ResizedSubpanel.Height - OldSize.Height;
				// Figure out how much we can actually resize the AdjacentPanel.
				double OldAdjacentHeight = AdjacentSubpanel.Height;
				AdjacentSubpanel.Height = UnrealEd.MathUtils.Clamp( AdjacentSubpanel.Height - DesiredDelta, AdjacentSubpanel.MinHeight, AdjacentSubpanel.MaxHeight );
				// Now that we know how much we actually adjusted the panel ...
				double ActualDelta = OldAdjacentHeight - AdjacentSubpanel.Height;

				// ... we must ensure that the originally resized panel only changed by that amount.
				if ( ActualDelta < DesiredDelta )
				{
					ResizedSubpanel.Height = ResizedSubpanel.Height + ( ActualDelta - DesiredDelta );
				}					
			}
		}

		/// Distributes the height between panels.
		/// We try to maintain the ratios between panel sizes.
		/// Any collapsed panels are immune from re-distribution.
		/// The SharingBoundary subpanel and any panels above it are also immune from re-distribution.
		void DistributeAvailHeight()
		{
			// How much space we have to distribute (it's at most the whole panel height).
			double HeightToDistribute = this.ActualHeight;

			// Sometimes this is triggered with a bogus height. We don't want to redistribute anything in that case.
			if ( HeightToDistribute > 0.0 )
			{
				// We do not actually have the whole height. The SharingBoundary subpanel and anything above
				// it are immune to space-sharing. Collapsed panels are also immune from space-sharing.
				// We must subtract their heights.
				{
					bool MustShareSpace = ( SpaceSharingBoundary == null );
					foreach ( ExpandoSubpanel Child in PanelsToManage )
					{
						if ( !MustShareSpace || Child.IsCollapsed )
						{
							HeightToDistribute -= Child.ActualHeight;
						}
						if ( Child == SpaceSharingBoundary )
						{
							MustShareSpace = true;
						}
					}
				}


				// What is the total height of the subpanels that we are about to adjust?
				// These are all the subpanels below the space-sharing immunity line and
				// not collapsed.
				double TotalHeightOfAffected = 0;
				{
					bool MustShareSpace = ( SpaceSharingBoundary == null );
					foreach ( ExpandoSubpanel Child in PanelsToManage )
					{
						if ( MustShareSpace && !Child.IsCollapsed )
						{
							TotalHeightOfAffected += Child.ActualHeight;
						}

						if ( Child == SpaceSharingBoundary )
						{
							MustShareSpace = true;
						}
					}
				}

				// Redistribute the space between subpanels proportionately.
				{
					bool MustShareSpace = ( SpaceSharingBoundary == null );
					foreach ( ExpandoSubpanel Child in PanelsToManage )
					{
						if ( MustShareSpace && !Child.IsCollapsed )
						{
							double Fraction = Child.ActualHeight / TotalHeightOfAffected;
							// Subpanel's new height should be the same fraction of affected space as it was before.
							Child.Height = UnrealEd.MathUtils.Clamp( HeightToDistribute * Fraction, Child.MinHeight, Child.MaxHeight );
						}
						if ( Child == SpaceSharingBoundary )
						{
							MustShareSpace = true;
						}
					}
				}

				// We have adjusted the layout to reflect the user's subpanel-resize action.
				// Clear the SharingBoundary.
				SpaceSharingBoundary = null;
				HeightDistributionDirty = false;
			}			
		}

		/// Sometimes it is convenient to re-distribute space here by
		/// calling DistributeAvailHeight.
		protected override Size ArrangeOverride( Size FinalSize )
		{
			FinalSize = base.ArrangeOverride( FinalSize );

			if (HeightDistributionDirty)
			{
				this.DistributeAvailHeight();
			}
		
			return FinalSize;
		}

		/// Returns a string representation of the panels contained in this ExpandoPanel
		public String GetPanelStateAsString()
		{
			StringBuilder sb = new StringBuilder();
			foreach ( ExpandoSubpanel Subpanel in PanelsToManage )
			{
				sb.AppendFormat("{0}:{1}:{2}|", Subpanel.Name, Subpanel.IsCollapsed, Subpanel.ActualHeight);
			}

			return sb.ToString();
		}

		/// Given the String SerializedPanelState, update the ExpandoPanel state to match
		/// the state decoded from the string. Return true on success and false if decoding errors occured.
		/// Should a panel mentiond in the string not be found, simply ignore that data.
		public bool RestoreFromString( String SerializedPanelState )
		{
			// Tokenize the string
			List<String> Tokens = new List<String>( SerializedPanelState.Split( new char[]{'|', ':'}, StringSplitOptions.RemoveEmptyEntries ) );
			if ( Tokens.Count % 3 != 0 )
			{
				// The string is malformed.
				return false;
			}

			try
			{
				for ( int CurTok = 0; CurTok < Tokens.Count; CurTok += 3 )
				{
					// Every 3 tokens represent a subpanel.
					// Parse the Name
					String SubpanelName = Tokens[CurTok + 0];
					// Parse the Collapsed state
					bool SubpanelIsCollapsed = bool.Parse( Tokens[CurTok + 1] );
					// Parse the Height
					double SubpanelHeight = double.Parse( Tokens[CurTok + 2] );

					// Find this subpanel in our ExpandoPanel
					ExpandoSubpanel Subpanel = this.PanelsToManage.Find( delegate( ExpandoSubpanel Candidate ) { return Candidate.Name == SubpanelName; } );
					if (Subpanel != null)
					{
						// If we found it then update it
						Subpanel.IsCollapsed = SubpanelIsCollapsed;
						Subpanel.Height = SubpanelHeight;
					}					
				}
			}
			catch (ArgumentException)
			{
				return false;
			}
			catch (FormatException)
			{
				return false;
			}

			return true;
		}
	}
}
