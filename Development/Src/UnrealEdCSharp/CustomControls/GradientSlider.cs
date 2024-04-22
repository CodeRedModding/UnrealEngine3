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
using UnrealEd;
using System.Globalization;

namespace CustomControls
{
	/// A slider created for use in the Color Picker.
    public class GradientSlider : Control
    {
		private static readonly double MinWhitepoint = 0.02;

		static GradientSlider()
        {
            DefaultStyleKeyProperty.OverrideMetadata(typeof(GradientSlider), new FrameworkPropertyMetadata(typeof(GradientSlider)));
        }

		/// The GradientSlider uses a single part, which is the slider track.
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			// Regular slider
			{
				mPART_Track = Template.FindName("PART_Track", this) as Rectangle;
			}

			// Variable Range support
			{
				// Release any previously existing handlers for the whitepoint caret
				if (mPART_WhitepointCaret != null)
				{
					mPART_WhitepointCaret.MouseLeftButtonDown -= mPART_WhitepointCaret_MouseLeftButtonDown;
					mPART_WhitepointCaret.MouseLeftButtonUp -= mPART_WhitepointCaret_MouseLeftButtonUp;
					mPART_WhitepointCaret.MouseMove -= mPART_WhitepointCaret_MouseMove;
				}

				// Attempt to support the whitepoint caret if the template provides it
				mPART_WhitepointCaret = Template.FindName("mPART_WhitepointCaret", this) as FrameworkElement;
				if (mPART_WhitepointCaret != null)
				{
					mPART_WhitepointCaret.MouseLeftButtonDown += mPART_WhitepointCaret_MouseLeftButtonDown;
					mPART_WhitepointCaret.MouseLeftButtonUp += mPART_WhitepointCaret_MouseLeftButtonUp;
					mPART_WhitepointCaret.MouseMove += mPART_WhitepointCaret_MouseMove;
				}
			}
		}


		#region Whitepoint (1.0) Caret Handling

		/// Handle dragging the white point caret.
		void mPART_WhitepointCaret_MouseMove(object sender, MouseEventArgs e)
		{
			if (mPART_WhitepointCaret == Mouse.Captured && mPART_Track != null)
			{
				Point MousePos = Mouse.GetPosition(mPART_Track);
				Whitepoint = MathUtils.Clamp( (MousePos.X - WhitepointDragOffset) / mPART_Track.ActualWidth, MinWhitepoint, 1);
			}
			e.Handled = true;
		}

		/// Whitepoint caret is released
		void mPART_WhitepointCaret_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
		{
			mPART_WhitepointCaret.ReleaseMouseCapture();
			e.Handled = true;

			if (ValueCommitted != null)
			{
				ValueCommitted();
			}
		}

		/// Whitepoint caret is grabbed.
		void mPART_WhitepointCaret_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
		{
			if (mPART_WhitepointCaret != null)
			{
				mPART_WhitepointCaret.CaptureMouse();
				WhitepointDragOffset = Mouse.GetPosition(mPART_WhitepointCaret).X;
				e.Handled = true;
			}
		}

		/// An optional component that is necessary only for variable range sliders
		private FrameworkElement mPART_WhitepointCaret;

		/// When we're dragging the whitepoint we want to grab anywhere on the widget.
		/// So, we store the offset to the center of the whitepoint caret.
		private double WhitepointDragOffset;

		#endregion


		/// When the slider is resized, update the caret position.
		protected override void OnRenderSizeChanged(SizeChangedInfo sizeInfo)
		{
			CaretOffset = OffsetFromNormalizedValue(this.Value) * Whitepoint;
			WhitepointOffset = OffsetFromNormalizedValue(this.Whitepoint);
			base.OnRenderSizeChanged(sizeInfo);
		}

		/// Capture the mouse when it is pressed and update the value.
		protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
		{
			this.CaptureMouse();
			UpdateValueFromLocation( Mouse.GetPosition(mPART_Track) );
			base.OnMouseLeftButtonDown(e);
		}

		/// Release mouse capture when the mouse is unpressed.
		protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
		{
			this.ReleaseMouseCapture();
			base.OnMouseLeftButtonDown(e);

			if (ValueCommitted != null)
			{
				ValueCommitted();
			}
		}

		/// If the mouse is moved while captured (i.e. pressed), update value and caret position.
		protected override void OnMouseMove(MouseEventArgs e)
		{
			if (Mouse.Captured == this)
			{
				UpdateValueFromLocation(Mouse.GetPosition(mPART_Track));
				base.OnMouseMove(e);
			}
		}

		/// Given a local position, update the value and caret
		private void UpdateValueFromLocation( Point LocalPosition )
		{
			if ( mPART_Track != null )
			{
				double TmpValue = MathUtils.Clamp(LocalPosition.X / mPART_Track.ActualWidth, 0, 1.0);
				NormalizedValue = TmpValue;
				Value = (float)(TmpValue / Whitepoint);
				
				CaretOffset = OffsetFromNormalizedValue(NormalizedValue);
			}
		}
		
		/// Pick an appropriate whitepoint for displaying the currently selected value.
		/// For values 0..1 -> Whitepoint = 1
		/// For Values 1..Max -> Whitepoint = 1/Value
		public void AdjustRangeToValue()
		{
			double TmpValue = Value;
			TmpValue = Math.Max(TmpValue, 1.0);
			double NewWhitepoint = MathUtils.Clamp(1 / TmpValue, MinWhitepoint, 1);
			Whitepoint = NewWhitepoint;
			CaretOffset = OffsetFromNormalizedValue(Value)*Whitepoint;
		}

		/// Update caret position when value is changed.
		public static void ValueChangedCallback(DependencyObject d, DependencyPropertyChangedEventArgs e)
		{
			GradientSlider This = (GradientSlider)(d);
			float NewValue = (float)e.NewValue;
			
			double CurMax = 1 / This.Whitepoint;
			if ( This.IsVariableRange && NewValue > CurMax )
			{
				This.AdjustRangeToValue();	
			}
			else
			{
				This.CaretOffset = This.OffsetFromNormalizedValue(This.Value * This.Whitepoint);
			}
			
			
			if (This.ValueChanged != null)
			{
				This.ValueChanged( (float)e.NewValue );
			}
		}

		/// Update whitepoint caret position. Notify that the whitepoint has changed.
		public static void WhitepointChangedCallback(DependencyObject d, DependencyPropertyChangedEventArgs e)
		{
			GradientSlider This = (GradientSlider)(d);
			This.WhitepointOffset = This.OffsetFromNormalizedValue(This.Whitepoint);
			This.UpdateValueFromLocation(new Point(This.CaretOffset, 0));

			if (This.WhitepointChanged != null)
			{
				This.WhitepointChanged(This.Whitepoint);
			}
		}	

		public delegate void ValueChanged_Handler(float NewValue);
		public ValueChanged_Handler ValueChanged;

		/// The Value represented by this slider.
		public float Value
		{
			get { return (float)GetValue(ValueProperty); }
			set { SetValue(ValueProperty, value); }
		}
		public static readonly DependencyProperty ValueProperty =
			DependencyProperty.Register("Value", typeof(float), typeof(GradientSlider), new FrameworkPropertyMetadata(0.0f, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault, ValueChangedCallback));


		/// The value of the slider ignoring the Whitepoint; always between 0 and 1
		private double NormalizedValue { get; set; }


		/// The offset of the Caret; used by the template. (see generic.xaml)
		public double CaretOffset
		{
			get { return (double)GetValue(CaretOffsetProperty); }
			set { SetValue(CaretOffsetProperty, value); }
		}
		public static readonly DependencyProperty CaretOffsetProperty =
			DependencyProperty.Register("CaretOffset", typeof(double), typeof(GradientSlider), new UIPropertyMetadata(0.0));

		/// Update the caret position
		protected double OffsetFromNormalizedValue( double InValue )
		{
			if (mPART_Track != null)
			{
				return mPART_Track.ActualWidth * InValue;
			}
			return 0;
		}

		
		/// Variable range sliders have a user-adjustable 1.0 point; used for HDR support.
		public bool IsVariableRange
		{
			get { return (bool)GetValue(IsVariableRangeProperty); }
			set { SetValue(IsVariableRangeProperty, value); }
		}
		public static readonly DependencyProperty IsVariableRangeProperty =
			DependencyProperty.Register("IsVariableRange", typeof(bool), typeof(GradientSlider), new UIPropertyMetadata(false));

		/// The location of the 1.0 point. This value between 0 and 1 determines the maximum value of the slider as (1.0 / Whitepoint).
		public double Whitepoint
		{
			get { return (double)GetValue(WhitepointProperty); }
			set { SetValue(WhitepointProperty, value); }
		}
		public static readonly DependencyProperty WhitepointProperty =
			DependencyProperty.Register("Whitepoint", typeof(double), typeof(GradientSlider), new FrameworkPropertyMetadata(1.0, FrameworkPropertyMetadataOptions.BindsTwoWayByDefault, WhitepointChangedCallback));

		public delegate void WhitepointChanged_Handler(double NewWhitepointValue);
		public event WhitepointChanged_Handler WhitepointChanged;

		public delegate void ValueCommitted_Handler();
		public event ValueCommitted_Handler ValueCommitted;


		/// The offset of the Whitepoint from the left of the  slider in screen units.
		public double WhitepointOffset
		{
			get { return (double)GetValue(WhitepointOffsetProperty); }
			set { SetValue(WhitepointOffsetProperty, value); }
		}
		public static readonly DependencyProperty WhitepointOffsetProperty =
			DependencyProperty.Register("WhitepointOffset", typeof(double), typeof(GradientSlider), new UIPropertyMetadata(0.0));



		/// The label that appears next to the slider.
		public object Header
		{
			get { return (object)GetValue(HeaderProperty); }
			set { SetValue(HeaderProperty, value); }
		}
		public static readonly DependencyProperty HeaderProperty =
			DependencyProperty.Register("Header", typeof(object), typeof(GradientSlider), new UIPropertyMetadata(null));





		/// The track component; must be found in the template for the gradient slider to function.
		protected Rectangle mPART_Track;

    }

	/// <summary>
	/// Converts the whitepoint to the maximum value attainable by this slider
	/// </summary>
	[ValueConversion(typeof(double), typeof(String))]
	public class WhitepointToMaxValue : IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			double InWhitepoint = (double)value;
			return String.Format("{0:00.00}", 1 / InWhitepoint);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			return null;	// Not supported
		}
	}

	/// <summary>
	/// Formats the value string.
	/// </summary>
	[ValueConversion(typeof(float), typeof(String))]
	public class ValueToStringConverter : IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			float InValue = (float)value;
			return String.Format("{0:#0.00}", InValue);
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			return null;
		}
	}


}
