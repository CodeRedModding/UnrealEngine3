//=============================================================================
//	ImageRadioButton.cs: A radio button that has two images
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
using UnrealEd;

namespace CustomControls
{
    public class ImageRadioButton : BindableRadioButton
	{
		static ImageRadioButton()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( ImageRadioButton ), new FrameworkPropertyMetadata( typeof( ImageRadioButton ) ) );
		}



		public BitmapImage UncheckedImage
		{
			get { return (BitmapImage)GetValue( UncheckedImageProperty ); }
            set 
            {
                SetValue(UncheckedImageProperty, value);
                SetValue(DisabledImageProperty, value);  // For fallback
            }
		}

		// Using a DependencyProperty as the backing store for UncheckedImage.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty UncheckedImageProperty =
			DependencyProperty.Register( "UncheckedImage", typeof( BitmapImage ), typeof( ImageRadioButton ), new UIPropertyMetadata( null ) );

        public BitmapImage DisabledImage
        {
            get { return (BitmapImage)GetValue(DisabledImageProperty); }
            set { SetValue(DisabledImageProperty, value); }
        }

        // Using a DependencyProperty as the backing store for UncheckedImage.  This enables animation, styling, binding, etc...
        public static readonly DependencyProperty DisabledImageProperty =
            DependencyProperty.Register("DisabledImage", typeof(BitmapImage), typeof(ImageRadioButton), new UIPropertyMetadata(null));

		public BitmapImage CheckedImage
		{
			get { return (BitmapImage)GetValue( CheckedImageProperty ); }
			set { SetValue( CheckedImageProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for CheckedImage.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty CheckedImageProperty =
			DependencyProperty.Register( "CheckedImage", typeof( BitmapImage ), typeof( ImageRadioButton ), new UIPropertyMetadata( null ) );

	}
}
