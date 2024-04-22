//=============================================================================
//	Switcher.cs: Custom control for a tri-state toggle button
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;


namespace CustomControls
{
	/// <summary>
	/// ========================================
	/// .NET Framework 3.0 Custom Control
	/// ========================================
	///
	/// Follow steps 1a or 1b and then 2 to use this custom control in a XAML file.
	///
	/// Step 1a) Using this custom control in a XAML file that exists in the current project.
	/// Add this XmlNamespace attribute to the root element of the markup file where it is 
	/// to be used:
	///
	///     xmlns:MyNamespace="clr-namespace:UnrealEdCSharp.CustomControls"
	///
	///
	/// Step 1b) Using this custom control in a XAML file that exists in a different project.
	/// Add this XmlNamespace attribute to the root element of the markup file where it is 
	/// to be used:
	///
	///     xmlns:MyNamespace="clr-namespace:UnrealEdCSharp.CustomControls;assembly=UnrealEdCSharp.CustomControls"
	///
	/// You will also need to add a project reference from the project where the XAML file lives
	/// to this project and Rebuild to avoid compilation errors:
	///
	///     Right click on the target project in the Solution Explorer and
	///     "Add Reference"->"Projects"->[Browse to and select this project]
	///
	///
	/// Step 2)
	/// Go ahead and use your control in the XAML file. Note that Intellisense in the
	/// XML editor does not currently work on custom controls and its child elements.
	///
	///     <MyNamespace:Switcher/>
	///
	/// </summary>
	public class Switcher : Selector
	{
		public enum ButtonState
		{
			One,
			Two,
			Three
		};

		static Switcher()
		{
			DefaultStyleKeyProperty.OverrideMetadata(typeof(Switcher), new FrameworkPropertyMetadata(typeof(Switcher)));
		}

		protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
		{
			base.OnMouseLeftButtonDown(e);
			if (Items.Count > 0)
			{
				this.SelectedIndex = (this.SelectedIndex + 1) % Items.Count;
			}
			else
			{
				this.SelectedIndex = 0;
			}			
		}

	}
}
