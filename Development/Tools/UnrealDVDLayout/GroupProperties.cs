/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace UnrealDVDLayout
{
    public partial class GroupProperties : Form
    {
        private TOC ActiveTOC = null;
        private TOCGroup ActiveGroup = null;
        private GroupSortType SortType = GroupSortType.Alphabetical;

        public GroupProperties()
        {
            InitializeComponent();
        }

        private void PopulateFileListView()
        {
            List<TOCInfo> Entries = ActiveTOC.GetEntriesInGroup( ActiveGroup );

            GroupSortType TempSortType = ActiveGroup.SortType;
            ActiveGroup.SortType = SortType;
            ActiveTOC.ApplySort( Entries );
            ActiveGroup.SortType = TempSortType;

            GroupPropListView.Items.Clear();
            foreach( TOCInfo Entry in Entries )
            {
                ListViewItem Item = GroupPropListView.Items.Add( Entry.Path );
                float SizeMB = Entry.Size / ( 1024.0f * 1024.0f );
                Item.SubItems.Add( SizeMB.ToString( "f2" ) );
            }
        }

        public void Init( TOC TableOfContents, TOCGroup Group )
        {
            ActiveTOC = TableOfContents;
            ActiveGroup = Group;
            SortType = Group.SortType;

            // Init the main properties
            GroupPropNameLabel.Text = "Group Name: " + ActiveGroup.GroupName;
            RegularExpressionTextBox.Text = ActiveGroup.RegExp;

            // Populate list view with files
            PopulateFileListView();

            // Populate the sort type combo
            int SelectedIndex = 0;
            foreach( string SortTypeString in Enum.GetNames( typeof( GroupSortType ) ) )
            {
                GroupPropSortTypeCombo.Items.Add( SortTypeString );
                if( SortTypeString == ActiveGroup.SortType.ToString() )
                {
                    GroupPropSortTypeCombo.SelectedIndex = SelectedIndex;
                }
                SelectedIndex++;
            }
        }

        public void ApplyChanges()
        {
            ActiveGroup.RegExp = RegularExpressionTextBox.Text;

            if( GroupPropSortTypeCombo.SelectedItem != null )
            {
                GroupSortType SortType = ( GroupSortType )Enum.Parse( typeof( GroupSortType ), ( string )GroupPropSortTypeCombo.SelectedItem, false );
                ActiveGroup.SortType = SortType;
            }
        }

        private void GroupPropSortType_Changed( object sender, EventArgs e )
        {
            SortType = ( GroupSortType )Enum.Parse( typeof( GroupSortType ), ( string )GroupPropSortTypeCombo.SelectedItem, false );
            PopulateFileListView();
        }
    }
}