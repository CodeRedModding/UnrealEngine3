// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Data;
using System.Configuration;
using System.Collections;
using System.Data.SqlClient;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

public partial class Game : BasePage
{
	private string GetQueryString( string Key, string DefaultValue )
	{
		string Value = DefaultValue;
		if( Request.QueryString[Key] != null )
		{
			Value = Request.QueryString[Key];
		}

		return ( Value );
	}

    protected void Page_Load( object sender, EventArgs e )
    {
        string LoggedOnUser = Context.User.Identity.Name;
        string MachineName = Context.Request.UserHostName;

        Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"";

		BuilderDBSource_Trigger.SelectParameters["MinAccess"].DefaultValue = GetQueryString( "MinAccess", "0" );
		BuilderDBSource_Trigger.SelectParameters["MaxAccess"].DefaultValue = GetQueryString( "MaxAccess", "0" );
		BuilderDBSource_Trigger.SelectParameters["UserDetail"].DefaultValue = GetQueryString( "UserDetail", "1" );
	}
}
