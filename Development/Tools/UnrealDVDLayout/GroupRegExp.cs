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
    public partial class GroupRegExp : Form
    {
        public GroupRegExp()
        {
            InitializeComponent();
        }

        private void GroupRegExpOKButton_Click( object sender, EventArgs e )
        {
            Close();
        }

        public void SetExpression( string Exp )
        {
            GroupRegExpExpressionCombo.Text = Exp;
        }

        public string GetExpression()
        {
            return ( GroupRegExpExpressionCombo.Text );
        }

        public void SetGroupNames( List<string> GroupNames )
        {
            GroupRegExpGroupCombo.Text = "";
            if( GroupNames.Count > 0 )
            {
                GroupRegExpGroupCombo.Text = GroupNames[0];
            }

            foreach( string GroupName in GroupNames )
            {
                GroupRegExpGroupCombo.Items.Add( GroupName );
            }
        }

        public string GetGroupName()
        {
            return ( GroupRegExpGroupCombo.Text );
        }
    }
}