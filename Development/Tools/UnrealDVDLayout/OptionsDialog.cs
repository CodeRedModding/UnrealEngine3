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
    public partial class OptionsDialog : Form
    {
        private UnrealDVDLayout Main = null;

        public OptionsDialog( UnrealDVDLayout InMain, SettableOptions Options )
        {
            Main = InMain;

            InitializeComponent();

            OptionsGrid.SelectedObject = Options;
        }
    }
}
    