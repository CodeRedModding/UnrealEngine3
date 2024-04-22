//=============================================================================
//	NameEntryPrompt.xaml.cs: User control that prompts the user to enter a string.
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
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
using System.Windows.Controls.Primitives;
using System.IO;
using System.ComponentModel;
using System.Collections.ObjectModel;
using ContentBrowser;
using System.Text.RegularExpressions;
using System.Windows.Media.Animation;
using System.Collections;


namespace CustomControls
{
	/// <summary>
	/// Interaction logic for InfoPanel.xaml
	/// </summary>
	public partial class InfoPanel : UserControl
	{
		/// Construct a SortableColumnHeader
		public InfoPanel()
		{
			InitializeComponent();
		}

		/// <summary>
		/// Shows the popup with a warning label and hides the info label
		/// </summary>
		/// <returns></returns>
		public void SetErrorText(String InErrorText)
		{
			HideAll();
			mErrorLabel.Text = InErrorText;
			ErrorBorder.Visibility = Visibility.Visible;
		}

		/// <summary>
		/// Shows the popup with a warning label and hides the info label
		/// </summary>
		/// <returns></returns>
		public void SetWarningText(String InWarningText)
		{
			HideAll();
			mWarningLabel.Text = InWarningText;
			WarningBorder.Visibility = Visibility.Visible;
		}

		/// <summary>
		/// Shows the popup with a info label and hides the warning label
		/// </summary>
		/// <returns></returns>
		public void SetInfoText(String InInfoText)
		{
			HideAll();
			mInfoLabel.Text = InInfoText;
			InfoBorder.Visibility = Visibility.Visible;
		}


		/// <summary>
		/// Hides info, warning, and error panels
		/// </summary>
		/// <returns></returns>
		void HideAll()
		{
			ErrorBorder.Visibility = Visibility.Collapsed;
			WarningBorder.Visibility = Visibility.Collapsed;
			InfoBorder.Visibility = Visibility.Collapsed;
		}


		/// Show the prompt
		public void Show()
		{
			Visibility = Visibility.Visible;
		}

		/// Hide the prompt
		public void Hide()
		{
			Visibility = Visibility.Collapsed;
		}

		/// An optional, user-provided parameter that will be passed back to the user upon prompt acceptance.
		public Object Parameters { get; set; }
	}
}
