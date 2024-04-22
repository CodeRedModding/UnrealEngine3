/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Text;
using System.Collections.Generic;

namespace UnrealConsole
{
	/// <summary>
	/// This dialog shows the user a list of available game instances that
	/// can be connected to
	/// </summary>
	public partial class NetworkConnectionDialog : System.Windows.Forms.Form
	{
		/// <summary>
		/// Holds the selected server item
		/// </summary>
		public ConsoleInterface.PlatformTarget[] SelectedTargets;

		private bool bTargetsRetrieved = false;

		/// <summary>
		/// Default constructor
		/// </summary>
		public NetworkConnectionDialog()
		{
			// Required for Windows Form Designer support
			InitializeComponent();

			mCheckBox_ShowAllTargetInfo.Checked = UnrealConsoleWindow.CurrentSettings.ShowAllTargetInfo;

			// Init connection list view
			ConnectionList.ListViewItemSorter = new ListViewItemComparer( ConnectionList, 1 );
		}

		/// <summary>
		/// Enumerates all available targets.
		/// </summary>
		private void GetPossibleTargets()
		{
			ConnectionList.Items.Clear();

			// Spawn generic progress bar
			ManualResetEvent Event = new ManualResetEvent( false );
			Thread TargetsThread = new Thread( new ParameterizedThreadStart( EnumerateTargetsThreadHandler ) );
			TargetsThread.Name = "TargetEnumeration UI Thread";
			TargetsThread.IsBackground = true;

			TargetsThread.Start( Event );

			// go over all the platforms
			foreach( ConsoleInterface.Platform CurPlatform in ConsoleInterface.DLLInterface.Platforms )
			{
				CurPlatform.EnumerateAvailableTargets();

				// go over all the targets for this platform
				foreach( ConsoleInterface.PlatformTarget CurTarget in CurPlatform.Targets )
				{
					ListViewItem lvi = null;

					if( mCheckBox_ShowAllTargetInfo.Checked )
					{
						// Add the server info with IP addr
						lvi = new ListViewItem( CurTarget.Name );
						lvi.SubItems.Add( CurPlatform.Name );
						lvi.SubItems.Add( CurTarget.DebugIPAddress.ToString() );
						lvi.SubItems.Add( CurTarget.IPAddress.ToString() );
						lvi.SubItems.Add( CurTarget.ConsoleType.ToString() );
						lvi.Tag = CurTarget;
					}
					else
					{
						lvi = new ListViewItem( CurTarget.TargetManagerName );
						lvi.SubItems.Add( CurPlatform.Name );
						lvi.SubItems.Add( "n/a" );
						lvi.SubItems.Add( "n/a" );
						lvi.SubItems.Add( "n/a" );
						lvi.Tag = CurTarget;
					}

					ConnectionList.Items.Add( lvi );
				}
			}

			// Tell progress bar to go away
			Event.Set();
		}

		/// <summary>
		/// Shows a dialog box in a separate thread while all targets are being enumerated.
		/// </summary>
		/// <param name="State">The event that will signal the thread when the targets are finished being enumerated.</param>
		private void EnumerateTargetsThreadHandler(object State)
		{
			ManualResetEvent Event = State as ManualResetEvent;

			if( Event != null )
			{
				using( EnumeratingTargetsForm ProgressBar = new EnumeratingTargetsForm() )
				{
					while( !Event.WaitOne( 5 ) )
					{
						Application.DoEvents();
					}
				}
			}
		}

		/// <summary>
		/// Rebuilds the connection and sends the server announce request
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void RefreshButton_Click(object sender, System.EventArgs e)
		{
			ConnectionList.Items.Clear();

			GetPossibleTargets();
		}

		/// <summary>
		/// Shuts down any sockets and sets our out variables
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void NetworkConnectionDialog_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			// Store the selected item as the server to connect to
			if( ConnectionList.SelectedItems.Count > 0 )
			{
				List<ConsoleInterface.PlatformTarget> Targets = new List<ConsoleInterface.PlatformTarget>();

				foreach( ListViewItem CurItem in ConnectionList.SelectedItems )
				{
					Targets.Add( ( ConsoleInterface.PlatformTarget )CurItem.Tag );
				}

				SelectedTargets = Targets.ToArray();
			}

			UnrealConsoleWindow.CurrentSettings.ShowAllTargetInfo = mCheckBox_ShowAllTargetInfo.Checked;
		}

		/// <summary>
		/// Handles double clicking on a specific server. Same as clicking once
		/// and closing the dialog via Connect
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ConnectionList_ItemActivate(object sender, System.EventArgs e)
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		/// <summary>
		/// Event handler for when a column header has been clicked.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void ConnectionList_ColumnClick(object sender, System.Windows.Forms.ColumnClickEventArgs e)
		{
            if( ConnectionList.Sorting == System.Windows.Forms.SortOrder.Ascending )
            {
                ConnectionList.Sorting = SortOrder.Descending;
            }
            else
            {
                ConnectionList.Sorting = SortOrder.Ascending;
            }

            ConnectionList.ListViewItemSorter = new ListViewItemComparer( ConnectionList, e.Column );
		}

		/// <summary>
		/// Event handler for when a target has been selected.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		private void ConnectionList_SelectedIndexChanged(object sender, EventArgs e)
		{
			ConnectButton.Enabled = ConnectionList.SelectedIndices.Count > 0 && ConnectionList.SelectedIndices[0] >= 0;
		}

		private void AllTargetInfo_CheckChanged( object sender, EventArgs e )
		{
			if( bTargetsRetrieved && mCheckBox_ShowAllTargetInfo.Checked )
			{
				RefreshButton_Click( null, null );
			}
		}

		private void NetworkConnectionShown( object sender, EventArgs e )
		{
			GetPossibleTargets();
			bTargetsRetrieved = true;
		}
	}

	/// <summary>
	/// Implements the manual sorting of items by columns.
	/// </summary>
	class ListViewItemComparer : IComparer
	{
        private ListView Parent;
		private int ColumnIndex;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParent">The owner of the comparer.</param>
        public ListViewItemComparer( ListView InParent )
		{
            Parent = InParent;
			ColumnIndex = 0;
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParent">THe owner of the comparer.</param>
		/// <param name="Column">The column being compared.</param>
		public ListViewItemComparer( ListView InParent, int Column )
		{
            Parent = InParent;
            ColumnIndex = Column;
		}

		/// <summary>
		/// Compares 2 objects for equality.
		/// </summary>
		/// <param name="x">The first object.</param>
		/// <param name="y">The second object.</param>
		/// <returns>0 if they're equal.</returns>
		public int Compare(object x, object y)
		{
            string A, B;
            //int NameIndex;

            switch( ColumnIndex )
            {
                case 0:
                    A = ( ( ListViewItem )x ).SubItems[ColumnIndex].Text;
                    B = ( ( ListViewItem )y ).SubItems[ColumnIndex].Text;
                    break;

                case 1:
                    A = ( ( ListViewItem )x ).SubItems[ColumnIndex].Text;
                    B = ( ( ListViewItem )y ).SubItems[ColumnIndex].Text;
                    break;

                default:
                    A = "";
                    B = "";
                    break;
            }

            if( Parent.Sorting == System.Windows.Forms.SortOrder.Ascending )
            {
                return String.Compare( A, B );
            }
            else
            {
                return String.Compare( B, A );
            }
		}
	}
}
