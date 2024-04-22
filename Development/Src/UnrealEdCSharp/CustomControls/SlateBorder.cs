//=============================================================================
//	SlateBorder.cs: Slate-Style Border
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

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
	public class SlateBorder : ContentControl
	{
		static SlateBorder()
		{
			DefaultStyleKeyProperty.OverrideMetadata( typeof( SlateBorder ), new FrameworkPropertyMetadata( typeof( SlateBorder ) ) );
		}

		public enum BorderStyleType
		{
			Sunken,
			Raised,
			FilterHeader,
			ActiveFilterHeader,
			Splitter,
			Button,
			ButtonPressed,
			ToolbarButton,
			ToolbarButtonHover,
			ToolbarButtonPressed,
		}

		#region BorderStyleProperty

		/// How should we render the border? (Raised, Sunken, etc...) This is a dependency property.
		public BorderStyleType BorderStyle
		{
			get { return (BorderStyleType)GetValue( BorderStyleProperty ); }
			set { SetValue( BorderStyleProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for BorderStyle.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty BorderStyleProperty =
			DependencyProperty.Register( "BorderStyle", typeof( BorderStyleType ), typeof( SlateBorder ), new UIPropertyMetadata( BorderStyleType.Sunken ) );

		#endregion


		#region CornerRadiusProperty

		/// Corner Radius for SlateBorder
		public CornerRadius CornerRadius
		{
			get { return (CornerRadius)GetValue( CornerRadiusProperty ); }
			set { SetValue( CornerRadiusProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for CornerRadius.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty CornerRadiusProperty =
			DependencyProperty.Register( "CornerRadius", typeof( CornerRadius ), typeof( SlateBorder ), new UIPropertyMetadata( new CornerRadius( 0 ) ) );
		
		#endregion


		#region InnerBorderThicknessProperty

		public Thickness InnerBorderPadding
		{
			get { return (Thickness)GetValue( InnerBorderPaddingProperty ); }
			set { SetValue( InnerBorderPaddingProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for InnerBorderPadding.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty InnerBorderPaddingProperty =
			DependencyProperty.Register( "InnerBorderPadding", typeof( Thickness ), typeof( SlateBorder ), new UIPropertyMetadata( new Thickness(0) ) );

		#endregion


		#region InnerBorderBrushProperty

		/// The Brush with which to render the inner border.
		public Brush InnerBorderBrush
		{
			get { return (Brush)GetValue( InnerBorderBrushProperty ); }
			set { SetValue( InnerBorderBrushProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for InnerBorderBrush.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty InnerBorderBrushProperty =
			DependencyProperty.Register( "InnerBorderBrush", typeof( Brush ), typeof( SlateBorder ) );

		
		#endregion


		#region OuterBorderBrushProperty

		/// The Brush with which to render the outer border.
		public Brush OuterBorderBrush
		{
			get { return (Brush)GetValue( OuterBorderBrushProperty ); }
			set { SetValue( OuterBorderBrushProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for OuterBorderBrush.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty OuterBorderBrushProperty =
			DependencyProperty.Register( "OuterBorderBrush", typeof( Brush ), typeof( SlateBorder ) );

		
		#endregion


		#region InnerBorderThicknessProperty

		/// The thickness of the inner sub-border in the composite slate border.
		public Thickness InnerBorderThickness
		{
			get { return (Thickness)GetValue( InnerBorderThicknessProperty ); }
			set { SetValue( InnerBorderThicknessProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for InnerBorderThickness.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty InnerBorderThicknessProperty =
			DependencyProperty.Register( "InnerBorderThickness", typeof( Thickness ), typeof( SlateBorder ), new UIPropertyMetadata( new Thickness(1) ) );

		#endregion


		#region OuterBorderThicknessProperty

		/// The thickness of the outer sub-border in the composite slate border.
		public Thickness OuterBorderThickness
		{
			get { return (Thickness)GetValue( OuterBorderThicknessProperty ); }
			set { SetValue( OuterBorderThicknessProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for OuterBorderThickness.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty OuterBorderThicknessProperty =
			DependencyProperty.Register( "OuterBorderThickness", typeof( Thickness ), typeof( SlateBorder ), new UIPropertyMetadata( new Thickness( 1 ) ) );

		
		#endregion



	}
}
