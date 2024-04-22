/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.ComponentModel;
using RemotableType;
using UnrealProp;

namespace UnrealProp
{
    /// <summary>
    /// Progress bar control is used for multiple purposes to 
    /// display the progress or status of a particular action
    /// for example, it can be used as bars in the voting control
    /// </summary>
    [
    DefaultProperty( "Progress" ),
    ToolboxData( "<{0}:ProgressBar runat=server></{0}:ProgressBar>" )
    ]
    public class ProgressBar : WebControl, INamingContainer
    {
        // Constructor
        public ProgressBar()
        {
            // Set up some defaults
            this.BackColor = System.Drawing.Color.LightGray;
            this.ForeColor = System.Drawing.Color.Blue;
        }

        public override System.Drawing.Color BackColor
        {
            get
            {
                return base.BackColor;
            }
            set
            {
                base.BackColor = value;
            }
        }

        public override System.Drawing.Color ForeColor
        {
            get
            {
                return base.ForeColor;
            }
            set
            {
                base.ForeColor = value;
            }
        }

        // Set the percentage of progress
        public int Progress
        {
            get
            {
                if( ViewState["Progress"] != null )
                {
                    return ( int )ViewState["Progress"];
                }
                return 0;
            }
            set
            {

                ViewState["Progress"] = value;

                if( value > 100 )	// Greater than 100 is still 100
                {
                    ViewState["Progress"] = 100;
                }
                else if( value < 0 )	// Less than 0 is stil 0
                {
                    ViewState["Progress"] = 0;
                }
            }
        }

        // Render the progress bar
        protected override void Render( HtmlTextWriter output )
        {
            // We render a table
            Table table = new Table();
            TableRow row = new TableRow();

            table.CellPadding = 0;
            table.CellSpacing = 0;
            table.BorderWidth = 0;
            table.Width = this.Width;
            table.Height = this.Height;

            // Add a row
            table.Rows.Add( row );

            int percentOfProgress = Progress / 5;
            // Create 20 cells
            for( int i = 0; i < 20; i++ )
            {
                TableCell td = new TableCell();
                td.Width = 5;
                td.Height = 5;

                // How should we fill this table?
                if( i < percentOfProgress )
                {
                    td.BackColor = this.ForeColor;
                }
                else
                {
                    td.BackColor = this.BackColor;
                }

                // Add cell to row
                row.Cells.Add( td );
            }

            this.Controls.Add( table );
            table.RenderControl( output );
        }
    }
}
