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
using System.Windows.Shapes;

namespace TestBench
{
    /// <summary>
    /// Interaction logic for CustomWidgetTestWindow.xaml
    /// </summary>
    public partial class CustomWidgetTestWindow : Window
    {
        public CustomWidgetTestWindow()
        {
            InitializeComponent();

			mHDRGradientSlider.WhitepointChanged += mHDRGradientSlider_WhitepointChanged;
			mHDRGradientSlider.ValueChanged += new CustomControls.GradientSlider.ValueChanged_Handler(mHDRGradientSlider_ValueChanged);
			mResetRangeButton.Click += new RoutedEventHandler(mResetRangeButton_Click);
        }

		void mHDRGradientSlider_ValueChanged(float NewValue)
		{
			
		}

		void mResetRangeButton_Click(object sender, RoutedEventArgs e)
		{
			mHDRGradientSlider.AdjustRangeToValue();
		}

		void mHDRGradientSlider_WhitepointChanged(double NewWhitepointValue)
		{
			LinearGradientBrush HDRBackground = new LinearGradientBrush();
			HDRBackground.GradientStops.Add( new GradientStop(Colors.Black, 0) );
			HDRBackground.GradientStops.Add( new GradientStop(Colors.White, NewWhitepointValue) );
			HDRBackground.StartPoint = new Point(0, 0);
			HDRBackground.EndPoint = new Point(1, 0);

			mHDRGradientSlider.Background = HDRBackground;
		}



		public float Hue
		{
			get { return (float)GetValue(HueProperty); }
			set { SetValue(HueProperty, value); }
		}
		public static readonly DependencyProperty HueProperty =
			DependencyProperty.Register("Hue", typeof(float), typeof(CustomWidgetTestWindow), new UIPropertyMetadata(0.0f));

		public float Sat
		{
			get { return (float)GetValue(SatProperty); }
			set { SetValue(SatProperty, value); }
		}
		public static readonly DependencyProperty SatProperty =
			DependencyProperty.Register("Sat", typeof(float), typeof(CustomWidgetTestWindow), new UIPropertyMetadata(0.0f));






		public float Brightness
		{
			get { return (float)GetValue(BrightnessProperty); }
			set { SetValue(BrightnessProperty, value); }
		}
		public static readonly DependencyProperty BrightnessProperty =
			DependencyProperty.Register("Brightness", typeof(float), typeof(CustomWidgetTestWindow), new UIPropertyMetadata(0.0f));




    }
}
