//=============================================================================
//	TagBoxAdorner.cs: Adorner for tag boxes
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Documents;
using System.Windows;
using System.Windows.Media;
using System.Windows.Controls;
using System.ComponentModel;
using System.Collections.ObjectModel;
using System.Text.RegularExpressions;


namespace CustomControls
{
	/// <summary>
	/// The Adorner is responsible for rendering syntax and semantics errors in the textbox
	/// </summary>
    class TagBoxAdorner : Adorner
    {
		/// A refernece to the list of tagbox errors to render.
        private List<TagBox.TagBoxError> m_ErrorsToShow = new List<TagBox.TagBoxError>();
		
		/// <summary>
		/// Trigger a refresh of the syntax/sematic errors. (This is a deferred refresh; there is no other kind.)
		/// </summary>
		/// <param name="ErrorsToShowIn">List of errors to display.</param>
        public void TriggerUpdate(List<TagBox.TagBoxError> ErrorsToShowIn)
        {
            m_ErrorsToShow = ErrorsToShowIn;
            this.ClipToBounds = true;
            this.InvalidateVisual();
        }

        /// <summary>
        /// Triggered when the validation is completed
        /// </summary>
        /// <param name="sender">Ignored</param>
        /// <param name="e">Ignored</param>
        private void WorkCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            this.InvalidateVisual();
        }

        /// <summary>
        /// Construct a new TagBoxAdorner.
        /// </summary>
        /// <param name="AdornedTextBoxIn">Textbox to adorn</param>
        /// <param name="Dictionary"></param>
        /// <param name="TagValidator"></param>
        public TagBoxAdorner(TextBox AdornedTextBoxIn)
            : base(AdornedTextBoxIn)
        {
            m_AdornedTextBox = AdornedTextBoxIn;

			// Create a dash style; 6 WPF units of dash, 6 WPF units of space
            double[] Dashes = { 3, 3 };
            DashStyle UnderlineStyle = new DashStyle(Dashes, 0);

            // Set up the bad syntax pen
			DottedSyntaxPen = new Pen( new SolidColorBrush( Colors.Red ), 1.0 );
            DottedSyntaxPen.DashStyle = UnderlineStyle;

            // Set up the bad semantics pen
            DottedSemanticsPen = new Pen(new SolidColorBrush(Colors.LightGreen), 1.0);
            DottedSemanticsPen.DashStyle = UnderlineStyle;
        }

		/// Pen to underscore bad syntax
        private Pen DottedSyntaxPen;
		
		/// Pen to underscore bad sematics (i.e. unknown tags)
        private Pen DottedSemanticsPen;

		/// The text box being adorned
        private TextBox m_AdornedTextBox;


		/// <summary>
		/// Render the decorations.
		/// </summary>
		/// <param name="drawingContext">Drawing context under which the rendering occurs.</param>
        protected override void OnRender(DrawingContext MyDawingContext)
        {
            //this.SnapsToDevicePixels = true;
            foreach ( TagBox.TagBoxError ErrorToShow in m_ErrorsToShow )
            {
                if (ErrorToShow.Cause == TagBox.ErrorCause.SyntaxError)
                {
                    MyDawingContext.DrawLine(DottedSyntaxPen, ErrorToShow.Location.BottomLeft, ErrorToShow.Location.BottomRight);
                }
                else
                {
                    MyDawingContext.DrawLine(DottedSemanticsPen, ErrorToShow.Location.BottomLeft, ErrorToShow.Location.BottomRight);
                }
            }



        }
    }
}
