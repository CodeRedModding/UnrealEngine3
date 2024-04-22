// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Data;
using System.Configuration;
using System.Collections;
using System.Data.SqlClient;
using System.Drawing;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

public partial class Maintenance : BasePage
{
    protected void Page_Load( object sender, EventArgs e )
    {
        string LoggedOnUser = Context.User.Identity.Name;
        string MachineName = Context.Request.UserHostName;

        Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"";
    }

	protected void MaintenanceDBRepeater_ItemCommand( object source, RepeaterCommandEventArgs e )
	{
		if( e.Item != null )
		{
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				// Find the command id that matches the description
				int CommandID = Int32.Parse( ( string )e.CommandName );
				if( CommandID != 0 )
				{
					string User = Context.User.Identity.Name;
					int Offset = User.LastIndexOf( '\\' );
					if( Offset >= 0 )
					{
						User = User.Substring( Offset + 1 );
					}

					Button CommandSource = ( Button )e.CommandSource;
					string CommandString = "UPDATE Commands SET Pending = 1, MachineLock = '" + CommandSource.Text + "', Operator = '" + User + "' WHERE ( ID = " + CommandID.ToString() + " )";
					Update( Connection, CommandString );
				}

				Connection.Close();
				Response.Redirect( "Default.aspx" );
			}
		}
	}

	protected internal void MaintenanceDBRepeater_OnPreRender( object Source, EventArgs e )
	{
		// If this object is a Button, check for our disable case
		Button SourceAsButton = Source as Button;
		if( ( SourceAsButton != null ) &&
			( SourceAsButton.CommandArgument != "None" ) )
		{
			SourceAsButton.ForeColor = Color.Red;
		}
	}
}
