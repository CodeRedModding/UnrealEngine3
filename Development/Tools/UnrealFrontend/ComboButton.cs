using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Controls.Primitives;

namespace UnrealFrontend
{
	/// <summary>
	/// ComboButton is a button that pops up content when it is clicked.
	/// ComboButton.Header is content of the button.
	/// ComboButton.Content is the content that will pop up when the button is clicked.
	/// </summary>
	public class ComboButton : HeaderedContentControl
	{
		static ComboButton()
		{
			DefaultStyleKeyProperty.OverrideMetadata(typeof(ComboButton), new FrameworkPropertyMetadata(typeof(ComboButton)));
		}

		/// <summary>
		/// Call this method to notify the ComboButton that a menu item has been clicked and
		/// is resulted in an action being taken (rather than a checkable menu item being toggled).
		/// Causes the ComboButton to close.
		/// </summary>
		public void OnMenuItemAction()
		{
			if (mPART_Popup != null)
			{
				mPART_Popup.IsOpen = false;
			}
		}

		/// <summary>
		/// Get references to control parts and attach handlers.
		/// </summary>
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			if (mPART_Popup != null)
			{
				mPART_Popup.Opened -= mPART_Popup_Opened;
				mPART_Popup.PreviewMouseDown -= mPART_Popup_PreviewMouseDown;
			}

			mPART_Popup = Template.FindName("mPART_Popup", this) as Popup;
            mPART_Button = Template.FindName("mPART_Button", this) as ToggleButton;

			if (mPART_Popup != null)
			{
				mPART_Popup.PreviewMouseDown += mPART_Popup_PreviewMouseDown;
				mPART_Popup.Opened += mPART_Popup_Opened;
			}
		}

		/// <summary>
		/// Called when the ComboButton's popup has opened.
		/// </summary>
		void mPART_Popup_Opened(object sender, EventArgs e)
		{
			if (mPART_Button != null)
			{
				mPART_Popup.MinWidth = mPART_Button.ActualWidth;
			}
			
		}

		/// <summary>
		/// Intercept mouse presses when the popup is open.
		/// This prevents hysteresis when clicking on the button part while the popup is open.
		/// </summary>
		void mPART_Popup_PreviewMouseDown(object sender, MouseButtonEventArgs e)
		{
			if (mPART_Button.IsMouseOver)
			{
				e.Handled = true; 
			}
		}


		/// The button part; we expect to find this in the template
		ToggleButton mPART_Button;
		/// The popup part; we expect to find this in the template
		Popup mPART_Popup;

	}
}
