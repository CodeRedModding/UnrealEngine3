//=============================================================================
//	DragSlider.cs: Slider control with a pretty click+drag interface
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Text;
using System.Windows;
using System.Windows.Data;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Input;
using UnrealEd;

namespace CustomControls
{

	/// <summary>
	/// Drag slider
	/// </summary>
	[TemplatePart( Name = "PART_ValueTextBox", Type = typeof( TextBox ) )]
	public class DragSlider : Slider
	{
		/// True if scalar value should be displayed as a percentage
		public bool DrawAsPercentage
		{
			get
			{
				return m_DrawAsPercentage;
			}
			set
			{
				m_DrawAsPercentage = value;
				UpdateWidgetState();
			}
		}

		/// True if scalar value should be displayed as an integer (mutually exclusive with m_DrawAsPercentage)
		public bool DrawAsInteger
		{
			get
			{
				return m_DrawAsInteger;
			}
			set
			{
				m_DrawAsInteger = value;
				UpdateWidgetState();
			}
		}

		/// How much to scale the progress bar progress exponentially
		public double ProgressRectExponent
		{
			get
			{
				return m_ProgressRectExponent;
			}
			set
			{
				m_ProgressRectExponent = value;
				UpdateWidgetState();
			}
		}


		/// How much the value should change when dragging the slider by one pixel
		public double ValuesPerDragPixel
		{
			get { return (double)GetValue( ValuesPerDragPixelProperty ); }
			set { SetValue( ValuesPerDragPixelProperty, value ); }
		}
		public static readonly DependencyProperty ValuesPerDragPixelProperty =
			DependencyProperty.Register( "ValuesPerDragPixel", typeof( double ), typeof( DragSlider ), new UIPropertyMetadata(1.0) );


		/// How much the value should change with each mouse wheel step
		public double ValuesPerMouseWheelScroll
		{
			get { return (double)GetValue( ValuesPerMouseWheelScrollProperty ); }
			set { SetValue( ValuesPerMouseWheelScrollProperty, value ); }
		}
		public static readonly DependencyProperty ValuesPerMouseWheelScrollProperty =
            DependencyProperty.Register("ValuesPerMouseWheelScroll", typeof(double), typeof(DragSlider), new UIPropertyMetadata(1.0) );

        /// True if dragging the bar should update to match the mouse cursor value immediately rather than using the
        /// ValuesPerDragPixel setting.
        public bool AbsoluteDrag
        {
            get { return (bool)GetValue(AbsoluteDragProperty); }
            set { SetValue(AbsoluteDragProperty, value); }
        }
        public static readonly DependencyProperty AbsoluteDragProperty =
            DependencyProperty.Register("AbsoluteDrag", typeof(bool), typeof(DragSlider), new UIPropertyMetadata(false));



		/// True if user is allowed to edit the value inline by clicking on the value text
		public bool AllowInlineEdit;


		/// True if inline edits should be applied as they are typed.  Otherwise they're only applied after
		/// the user pressed enter, tab, or after the text box loses focus.
		public bool ApplyInlineEditWhileTyping;


        /// Constructor
		public DragSlider()
		{
			if( DesignerProperties.GetIsInDesignMode( this ) )
			{
				// ...
			}

			Init();
		}




		/// Static constructor
		static DragSlider()
		{
			// NOTE: This is required for WPF to load the style and control template from generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( DragSlider ), new FrameworkPropertyMetadata( typeof( DragSlider ) ) );
		}


		/// Called when a new template is applied to this control.
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			if( m_PART_ValueTextBox != null )
			{
				m_PART_ValueTextBox.TextChanged -= new TextChangedEventHandler( m_PART_ValueTextBox_TextChanged );
				m_PART_ValueTextBox.KeyDown -= new KeyEventHandler( m_PART_ValueTextBox_KeyDown );
				m_PART_ValueTextBox.LostFocus -= new RoutedEventHandler( m_PART_ValueTextBox_LostFocus );
			}

			m_PART_ValueTextBox = (TextBox)Template.FindName( "PART_ValueTextBox", this );

			if( m_PART_ValueTextBox != null )
			{
				m_PART_ValueTextBox.TextChanged += new TextChangedEventHandler( m_PART_ValueTextBox_TextChanged );
				m_PART_ValueTextBox.KeyDown += new KeyEventHandler( m_PART_ValueTextBox_KeyDown );
				m_PART_ValueTextBox.LostFocus += new RoutedEventHandler( m_PART_ValueTextBox_LostFocus );
			}
		}


		/// Initialize the control.  Must be called after the control is created.
		public void Init()
		{
			DrawAsPercentage = false;
			DrawAsInteger = false;
			ProgressRectExponent = 1.0;
			AllowInlineEdit = true;
			ApplyInlineEditWhileTyping = false;

			PreviewMouseDown += new MouseButtonEventHandler( DragSlider_PreviewMouseDown );
			MouseMove += new MouseEventHandler( DragSlider_MouseMove );
			MouseWheel += new MouseWheelEventHandler( DragSlider_MouseWheel );
			ValueChanged += new RoutedPropertyChangedEventHandler<double>( DragSlider_ValueChanged );

			UpdateWidgetState();
		}

        protected override void OnMouseDown(MouseButtonEventArgs e) 
        {
            // Prevent toggling selection when placed in listboxes 
            e.Handled = true;
        }

		/// Start editing text
		void StartEditingInlineText()
		{
			IsEditingInlineText = true;

			m_PART_ValueTextBox.Text = ValueText;
			m_PART_ValueTextBox.SelectAll();
			Keyboard.Focus( m_PART_ValueTextBox );
		}


		/// Stop editing text
		void StopEditingInlineText()
		{
			SetValueFromInlineText();

			IsEditingInlineText = false;
			m_LastStopEditingInlineTextTime = DateTime.UtcNow;
		}


		/// Apply value edit text changes 
		void SetValueFromInlineText()
		{
			// Update our value based on the edited text
			if( DrawAsPercentage )
			{
				// Remove percent signs if there are any
				String CleanText = m_PART_ValueTextBox.Text.Replace( "%", "" );

				double PercentValue;
				if( double.TryParse( CleanText, out PercentValue ) )
				{
					Value = PercentValue * 0.01;
				}
			}
			else
			{
				double NewValue;
				if( double.TryParse( m_PART_ValueTextBox.Text, out NewValue ) )
				{
					Value = NewValue;
				}
			}

		}

	
		/// Called when a key is pressed in the value text box
		void m_PART_ValueTextBox_KeyDown( object sender, KeyEventArgs e )
		{
			if( IsEditingInlineText )
			{
				if( e.Key == Key.Return || e.Key == Key.Tab )
				{
					// Accept the value!
					StopEditingInlineText();

					e.Handled = true;

					if (e.Key == Key.Tab && null != mNextDragSliderControl)
					{
						mNextDragSliderControl.StartEditingInlineText();
					}
				}
			}
		}



		/// Called when value edit text box loses focus
		void m_PART_ValueTextBox_LostFocus( object sender, RoutedEventArgs e )
		{
			if( IsEditingInlineText )
			{
				// Accept the value!
				StopEditingInlineText();

				e.Handled = true;
			}
		}


	
		/// Called when the text has changed in the value text box
		void m_PART_ValueTextBox_TextChanged( object sender, TextChangedEventArgs e )
		{
			if( IsEditingInlineText )
			{
				if( ApplyInlineEditWhileTyping )
				{
					SetValueFromInlineText();

					e.Handled = true;
				}
			}
		}



		/// Called when our value has changed
		void DragSlider_ValueChanged( object sender, RoutedPropertyChangedEventArgs<double> e )
		{
			UpdateWidgetState();
		}



		/// Called when mouse button is pressed over the widget
		void DragSlider_PreviewMouseDown( object sender, MouseButtonEventArgs e )
		{
			if( !m_IsMouseDown )
			{
				// Set focus to ourself so that the edit box will lose focus if it has it
				Keyboard.Focus( this );

				// Make sure the inline text box isn't focused
				if( IsEditingInlineText )
				{
					StopEditingInlineText();
				}

				m_IsMouseDown = true;

				// Remember where the mouse cursor is on the screen
				m_WidgetScreenSpaceClickPosition = PointToScreen( e.GetPosition( this ) );
				m_WidgetLastMousePosition = e.GetPosition( this );


				// e.Handled = true;
			}
		}

		/// Called when mouse button is released over the widget
		protected override void OnMouseUp(MouseButtonEventArgs e)
		{
			if (m_IsMouseDown)
			{
				m_IsMouseDown = false;

				if (m_IsDragging)
				{
					// Release mouse capture and show the mouse cursor
					ReleaseMouseCapture();

                    if (!AbsoluteDrag)
                    {
                        // Restore the mouse position
                        NativeMethods.SetCursorPos((int)m_WidgetScreenSpaceClickPosition.X, (int)m_WidgetScreenSpaceClickPosition.Y);
                    }

					// End drag
					m_IsDragging = false;
					ClearValue(FrameworkElement.CursorProperty);
				}
				else
				{
					// Make sure at least a bit of time has gone by since the last time that we finished
					// editing inline text
					bool EnoughTimePassed =
						(DateTime.UtcNow - m_LastStopEditingInlineTextTime).TotalSeconds > 0.25;
					if (AllowInlineEdit && EnoughTimePassed)
					{
						// Start editing text
						StartEditingInlineText();
					}
				}
			}
			base.OnMouseUp(e);
		}


		/// Called when the mouse cursor moves over the widget
		void DragSlider_MouseMove( object sender, MouseEventArgs e )
		{
			if( m_IsMouseDown )
			{
                Point NewMousePosition = e.GetPosition( this );
				Vector PositionDelta = NewMousePosition - m_WidgetLastMousePosition;

				if( !m_IsDragging )
				{
					if( Math.Abs( PositionDelta.X ) >= 2 || Math.Abs( PositionDelta.Y ) >= 2 )
					{
						// Capture the mouse and hide the mouse cursor
						CaptureMouse();
						m_IsDragging = true;
						Cursor = Cursors.None;
					}
				}

                if( m_IsDragging )
				{
                    if( AbsoluteDrag )
                    {
                        m_WidgetLastMousePosition = NewMousePosition;

                        double LogPosition = MathUtils.Clamp((float)NewMousePosition.X / (float)ActualWidth, 0.0, 1.0);

                        // convert from Exp scale to linear scale
                        double LinearPosition = 1.0f - Math.Pow(1.0f - LogPosition, 1.0 / ProgressRectExponent);

                        double NewValue = MathUtils.Clamp((SliderMax - SliderMin) * LinearPosition + SliderMin, SliderMin, SliderMax);
                        Value = NewValue;                      
                    }
                    else
                    {
                        // Move the mouse back to where it was before we recorded the movement delta.  This allows the
                        // user to scroll as far as they want, regardless of the cursor's position on the desktop
                        NativeMethods.SetCursorPos((int)m_WidgetScreenSpaceClickPosition.X, (int)m_WidgetScreenSpaceClickPosition.Y);
                        m_WidgetLastMousePosition = PointFromScreen(m_WidgetScreenSpaceClickPosition);

                        // Apply delta.  If SliderMin and SliderMax are not set, this clamps between Minimum and Maximum
                        double NewValue = MathUtils.Clamp(Value + PositionDelta.X * ValuesPerDragPixel, SliderMin, SliderMax);
                        Value = NewValue;
                    }
				}
                
				e.Handled = true;
			}
		}


		/// Called when the mouse wheel is scrolled over the widget
		void DragSlider_MouseWheel( object sender, MouseWheelEventArgs e )
		{
			if( !m_IsDragging && !IsEditingInlineText )
			{
				// Apply delta.  If SliderMin and SliderMax are not set, this clamps between Minimum and Maximum
                double NewValue = MathUtils.Clamp(Value + (double)e.Delta * ValuesPerMouseWheelScroll / 120.0, SliderMin, SliderMax );
				Value = NewValue;

				e.Handled = true;
			}
		}


		/// Updates the state of the widget (amount bar/percentage, etc.)
		void UpdateWidgetState()
		{
			// Update the text label
			// @todo: Make number of digits configurable!
			if( DrawAsPercentage )
			{
				ValueText = ( Value * 100.0 ).ToString( "F0" ) + "%";
			}
			else if (DrawAsInteger)
			{
				ValueText = Math.Round(Value).ToString();
			}
			else
			{
				ValueText = Value.ToString( "F2" );
			}


			// Update the progress rectangle graphic
			{
				// Compute amount as a scalar
                double Progress = 0.0;                

                // Compute the actual max and min to clamp against.
                double ActualMax = SliderMax;
                double ActualMin = SliderMin;

                if (ActualMax > ActualMin)
                {
                    Progress = MathUtils.Clamp(
                         (Value - ActualMin) /
                         (ActualMax - ActualMin),
                         0.001,
                         1.0);
                }
                

				// Convert to exponential scale
				double ExpProgress = 1.0f - Math.Pow( 1.0f - Progress, ProgressRectExponent );

				// Update render transform
				double FinalProgress = ExpProgress;
				ProgressAmount = FinalProgress;
			}
		}

		/// Next drag-slider control property to allow tabbing
		private DragSlider mNextDragSliderControl;
		public DragSlider NextDragSliderControl
		{
			get { return mNextDragSliderControl; }
			set { mNextDragSliderControl = value; }
		}

		/// Value text
		public string ValueText
		{
			get { return (string)GetValue( ValueTextProperty ); }
			set { SetValue( ValueTextProperty, value ); }
		}
		public static readonly DependencyProperty ValueTextProperty =
			DependencyProperty.Register( "ValueText", typeof( string ), typeof( DragSlider ) );

		/// Progress amount
		public double ProgressAmount
		{
			get { return (double)GetValue( ProgressAmountProperty ); }
			set { SetValue( ProgressAmountProperty, value ); }
		}
		public static readonly DependencyProperty ProgressAmountProperty =
			DependencyProperty.Register( "ProgressAmount", typeof( double ), typeof( DragSlider ) );

		/// True if we're currently editing text
		public bool IsEditingInlineText
		{
			get { return (bool)GetValue( IsEditingInlineTextProperty ); }
			set { SetValue( IsEditingInlineTextProperty, value ); }
		}
		public static readonly DependencyProperty IsEditingInlineTextProperty =
			DependencyProperty.Register( "IsEditingInlineText", typeof( bool ), typeof( DragSlider ) );

        /// Optional Maximum slider value to be used when dragging the slider.
        public double SliderMax
        {
			get { double ReturnValue = (double)GetValue(SliderMaxProperty); return Double.IsNaN(ReturnValue) ? Maximum : ReturnValue; }
            set { SetValue( SliderMaxProperty, value ); }
        }

        // We NaN values to indicate that the property hasn't been overridden and the slider-specifc range is not enabled yet.
        public static readonly DependencyProperty SliderMaxProperty =
            DependencyProperty.Register("SliderMax", typeof(double), typeof(DragSlider), new UIPropertyMetadata(Double.NaN));

        /// Optional Minimum slider value to be used when dragging the slider.
        public double SliderMin
        {
			get { double ReturnValue = (double)GetValue(SliderMinProperty); return Double.IsNaN(ReturnValue) ? Minimum : ReturnValue; }
            set { SetValue( SliderMinProperty, value ); }
        }
        // We NaN values to indicate that the property hasn't been overridden and the slider-specifc range is not enabled yet.
        public static readonly DependencyProperty SliderMinProperty =
            DependencyProperty.Register("SliderMin", typeof(double), typeof(DragSlider), new UIPropertyMetadata(Double.NaN));


		/// True if scalar value should be displayed as a percentage
		bool m_DrawAsPercentage;

		/// True if scalar value should be displayed as an integer (mutually exclusive with m_DrawAsPercentage)
		bool m_DrawAsInteger;

		/// How much to scale the progress bar progress exponentially
		double m_ProgressRectExponent;

		/// True if the mouse button is down
		bool m_IsMouseDown;

		/// True if we're currently dragging interactively
		bool m_IsDragging;

		/// Position of the mouse the last time we dragged interactively
		Point m_WidgetLastMousePosition;

		/// Screen position of the mouse cursor at the point the user clicked on the widget
		Point m_WidgetScreenSpaceClickPosition;

		/// A reference to the text box control for typing in values directly
		TextBox m_PART_ValueTextBox = null;

		/// Time that we last stopped editing inline text
		DateTime m_LastStopEditingInlineTextTime;
	}

}
