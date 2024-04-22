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

namespace UnrealLoc
{
    public partial class OptionsDialog : Form
    {
        private UnrealLoc Main = null;

        public OptionsDialog( UnrealLoc InMain, SettableOptions Options )
        {
            Main = InMain;

            InitializeComponent();

            OptionsGrid.SelectedObject = Options;
        }
    }
}