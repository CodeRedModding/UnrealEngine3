/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace UnrealFrontend
{
	/// <summary>
	/// Interaction logic for ProfileListItem.xaml
	/// </summary>
	public partial class ProfileListItem : UserControl
	{
		public ProfileListItem()
		{
			InitializeComponent();
		}

		/// Shortctus: enter - accept, esc - cancel
		private void OnRenameTextboxPreviewKeyDown(object sender, KeyEventArgs e)
		{
			if (e.Key == Key.Enter)
			{
				this.AcceptNewName();
				e.Handled = true;
			}
			else if (e.Key == Key.Escape)
			{
				this.RejectNewName();
				e.Handled = true;
			}
		}
		
		/// Textbox unfocused -> accept profile rename
		private void OnRenameTextboxLostFocus(object sender, RoutedEventArgs e)
		{
			AcceptNewName();
		}

		/// Called to begin renaming
		private void OnEnteredRenameMode()
		{
			OldName = TheProfile.Name;
			mNameTextBlock.Visibility = Visibility.Collapsed;
			mRenameTextBox.Visibility = Visibility.Visible;
			Keyboard.Focus(mRenameTextBox);
			mRenameTextBox.SelectAll();
		}

		/// Commit the new name and exit rename mode
		private void AcceptNewName()
		{
			TheProfile.IsProfileBeingRenamed = false;
			OldName = "";
			if (mRenameTextBox.Text != TheProfile.Name)
			{
				String ValidProfileName = FileUtils.SuggestValidProfileName(mRenameTextBox.Text);
				if (ValidProfileName != mRenameTextBox.Text)
				{
					mRenameTextBox.Text = ValidProfileName;
				}
			}

			OnEndedRenameMode();
			Session.Current.SaveSessionSettings();
		}

		/// Cancel the rename
		private void RejectNewName()
		{
			TheProfile.IsProfileBeingRenamed = false;
			this.mRenameTextBox.Text = OldName;
			OldName = "";
			OnEndedRenameMode();
		}

		/// Exit rename mode
		private void OnEndedRenameMode()
		{
			mNameTextBlock.Visibility = Visibility.Visible;
			mRenameTextBox.Visibility = Visibility.Collapsed;
		}


		/// The profile being visualized
		public Profile TheProfile
		{
			get { return (Profile)GetValue(TheProfileProperty); }
			set { SetValue(TheProfileProperty, value); }
		}
		public static readonly DependencyProperty TheProfileProperty =
			DependencyProperty.Register("TheProfile", typeof(Profile), typeof(ProfileListItem), new UIPropertyMetadata(null, OnTheProfileChanged));

		public static void OnTheProfileChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
		{
			ProfileListItem This = (ProfileListItem)d;
			// Unsubscribe old notifications for property changes on Profile
			if (e.OldValue != null)
			{
				Profile OldProfile = (Profile)e.OldValue;
				OldProfile.PropertyChanged -= This.OnPropertyChanged;
			}

			// Subscribe for notifications for property changes on Profile
			if (e.NewValue != null)
			{
				Profile NewProfile = (Profile)e.NewValue;
				NewProfile.PropertyChanged += This.OnPropertyChanged;
			}
		}

		private void OnPropertyChanged(object sender, System.ComponentModel.PropertyChangedEventArgs e)
		{
			if (e.PropertyName == "IsProfileBeingRenamed")
			{
				if (TheProfile.IsProfileBeingRenamed)
				{
					this.OnEnteredRenameMode();
				}
			}
		}

		/// Name to restore to if the user cancels a rename.
		private static String OldName="";


	}
}
