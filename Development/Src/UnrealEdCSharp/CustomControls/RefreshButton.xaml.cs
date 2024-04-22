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

namespace CustomControls
{
	/// <summary>
	/// Interaction logic for RefreshButton.xaml
	/// </summary>
	public partial class RefreshButton : UserControl
	{
		public RefreshButton()
		{
			InitializeComponent();
		}

		void MenuItem_Click( object sender, RoutedEventArgs e )
		{
			this.mDownArrowButton.IsChecked = false;
		}

	}
}
