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
// using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using UnrealEd;

namespace CustomControls
{
	/// <summary>
	/// Background reading: http://en.wikipedia.org/wiki/Color_wheel
	/// 
	/// A Color Wheel widget allows the user to choose Hue and Saturation by picking
	/// a point within the wheel. Hue is chosen based on the angle and saturation based
	/// on the distance from the center of the circle.
	/// 
	/// When the user makes a choice (by clicking) we transform the (X,Y) mouse coordinates radial coordinates with
	/// the origin at the wheel's center. The angular coordinates map to Hue and Saturation.
	/// The selection glyph's location is derived from the Hue and Saturation values and converted
	/// into Cartesian coordinates.
	/// 
	/// The Color Wheel reads a brightness value that dims the wheel to give a better representation of the available color choices.
	/// </summary>
    public class ColorWheel : Control
    {
        static ColorWheel()
        {
            DefaultStyleKeyProperty.OverrideMetadata(typeof(ColorWheel), new FrameworkPropertyMetadata(typeof(ColorWheel)));
        }

		/// Color selection begins when the mouse is pressed.
		protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
		{
			this.CaptureMouse();
			base.OnMouseLeftButtonDown(e);
		}

		/// Color selection ends when the mouse is released.
		protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
		{
			this.ReleaseMouseCapture();
			base.OnMouseLeftButtonUp(e);
		}

		/// Color selection occurs if the mouse moves.
		protected override void OnMouseMove(MouseEventArgs e)
		{
			// If we were clicked on and the mouse is down
			if (Mouse.Captured == this)
			{
				float Radius = (float)(this.ActualWidth / 2);
				
				Point LocalMousePos = Mouse.GetPosition(this);
				Point WheelSpacePos = new Point(LocalMousePos.X - Radius, LocalMousePos.Y - Radius);

				// Compute Hue
				double Atan2 = Math.Atan2(WheelSpacePos.Y, WheelSpacePos.X);
				if (Atan2 < 0)
				{
					Atan2 = 2 * Math.PI + Atan2;
				}
				Hue = (float)(Atan2 / Math.PI * 180.0);

				// Compute Saturation
				double MousePosMagnitude = Math.Sqrt(WheelSpacePos.X * WheelSpacePos.X + WheelSpacePos.Y * WheelSpacePos.Y);
				if (MousePosMagnitude > 1.0f)
				{
					WheelSpacePos = new Point(WheelSpacePos.X / MousePosMagnitude, WheelSpacePos.Y / MousePosMagnitude);
				}
				Saturation = Math.Min( 1.0f, (float)(MousePosMagnitude / (Radius)) );


				UpdateSelectorPos( Radius );
				

				e.Handled = true;
			}
		}

		/// Updates the position of the selector to match the current Hue and Saturation given a widget radius.
		private void UpdateSelectorPos( double WidgetRadius )
		{
			Point SelectorPos = ComputeNormalizedSelectorCartesianPosition();
			double SelectorRadius = SelectorDiameter / 2;
			SelectorOffsetX = SelectorPos.X * WidgetRadius + WidgetRadius - SelectorRadius;
			SelectorOffsetY = SelectorPos.Y * WidgetRadius + WidgetRadius - SelectorRadius;
		}

		/// Override Arrange to force a fixed 1:1 aspect ratio.
		/// Also update the color selection for the new size.
		protected override Size ArrangeOverride( Size ArrangeBounds )
		{
			// Maintain aspect ratio
			Size BaseSize = base.ArrangeOverride(ArrangeBounds);

			if (BaseSize.Width < BaseSize.Height)
			{
				BaseSize.Height = BaseSize.Width;
			}
			else
			{
				BaseSize.Width = BaseSize.Height;
			}

			UpdateSelectorPos( BaseSize.Width / 2 );

			return BaseSize;
		}

		/// Get the Cartesian position in ColorWheel's normalized widget space (i.e. x and y are between 0 and 1).
		public Point ComputeNormalizedSelectorCartesianPosition()
		{
			float Radius = Saturation;
			float Angle = (float)(Hue / 180f * Math.PI);

			float X = (float)(Radius * Math.Cos(Angle));
			float Y = (float)(Radius * Math.Sin(Angle));

			return new Point(X, Y);
		}

        public static void HueChangedCallback(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ColorWheel This = (ColorWheel)(d);
            This.UpdateSelectorPos(This.ActualWidth / 2);
        }

        public static void SaturationChangedCallback(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ColorWheel This = (ColorWheel)(d);
            This.UpdateSelectorPos(This.ActualWidth / 2);
        }

		public static void BrightnessChangedCallback(DependencyObject d, DependencyPropertyChangedEventArgs e)
		{
			ColorWheel This = (ColorWheel)(d);
			double OverlayBrightness = MathUtils.Clamp(1 - This.Brightness, 0, 1);
			This.OverlayColor = new SolidColorBrush(Color.FromArgb((byte)(OverlayBrightness*byte.MaxValue), 0, 0, 0));
		}

        #region Properties
		
		public double SelectorDiameter
		{
			get { return (double)GetValue(SelectorDiameterProperty); }
			set { SetValue(SelectorDiameterProperty, value); }
		}
		public static readonly DependencyProperty SelectorDiameterProperty =
			DependencyProperty.Register("SelectorDiameter", typeof(double), typeof(ColorWheel), new UIPropertyMetadata(8.0));

        public float Hue
        {
            get { return (float)GetValue(HueProperty); }
            set { SetValue(HueProperty, value); }
        }

		public static readonly DependencyProperty HueProperty =
			DependencyProperty.Register("Hue", typeof(float), typeof(ColorWheel), new FrameworkPropertyMetadata(0.0f, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault, HueChangedCallback));

		public float Saturation
		{
			get { return (float)GetValue(SaturationProperty); }
			set { SetValue(SaturationProperty, value); }
		}
		public static readonly DependencyProperty SaturationProperty =
			DependencyProperty.Register("Saturation", typeof(float), typeof(ColorWheel), new FrameworkPropertyMetadata(0.0f, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault, SaturationChangedCallback));




		public float Brightness
		{
			get { return (float)GetValue(BrightnessProperty); }
			set { SetValue(BrightnessProperty, value); }
		}
		public static readonly DependencyProperty BrightnessProperty =
			DependencyProperty.Register("Brightness", typeof(float), typeof(ColorWheel), new UIPropertyMetadata(0.0f, BrightnessChangedCallback));




		public SolidColorBrush OverlayColor
		{
			get { return (SolidColorBrush)GetValue(OverlayColorProperty); }
			set { SetValue(OverlayColorProperty, value); }
		}
		public static readonly DependencyProperty OverlayColorProperty =
			DependencyProperty.Register("OverlayColor", typeof(SolidColorBrush), typeof(ColorWheel), new UIPropertyMetadata(null));




		public double SelectorOffsetX
		{
			get { return (double)GetValue(SelectorOffsetXProperty); }
			set { SetValue(SelectorOffsetXProperty, value); }
		}
		public static readonly DependencyProperty SelectorOffsetXProperty =
			DependencyProperty.Register("SelectorOffsetX", typeof(double), typeof(ColorWheel), new UIPropertyMetadata(0.0));


		public double SelectorOffsetY
		{
			get { return (double)GetValue(SelectorOffsetYProperty); }
			set { SetValue(SelectorOffsetYProperty, value); }
		}
		public static readonly DependencyProperty SelectorOffsetYProperty =
			DependencyProperty.Register("SelectorOffsetY", typeof(double), typeof(ColorWheel), new UIPropertyMetadata(0.0));

		#endregion

    }
}
