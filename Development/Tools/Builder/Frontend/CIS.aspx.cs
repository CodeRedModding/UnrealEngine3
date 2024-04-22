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

public partial class CIS : BasePage
{
    protected void Page_Load( object sender, EventArgs e )
    {
        string LoggedOnUser = Context.User.Identity.Name;
        string MachineName = Context.Request.UserHostName;

        Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"";
    }

	protected void Button_ResetCIS_Click( object sender, EventArgs e )
	{
		Button Pressed = ( Button )sender;
		if( Pressed.ID == "Button_ResetCIS" )
		{
			// Issue a query that simply clears out all active and pending CIS jobs
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				string CancelCISJobsQueryExample = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (Example)')";
				Update( Connection, CancelCISJobsQueryExample );
				string CancelCISJobsQueryGear = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (Gear)')";
				Update( Connection, CancelCISJobsQueryGear );
				string CancelCISJobsQueryUDK = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (UDK)')";
				Update( Connection, CancelCISJobsQueryUDK );
				string CancelCISJobsQueryMobile = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (Mobile)')";
				Update( Connection, CancelCISJobsQueryMobile );
				string CancelCISJobsQueryXenon = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (XBox360)')";
				Update( Connection, CancelCISJobsQueryXenon );
				string CancelCISJobsQueryPS3 = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (PS3)')";
				Update( Connection, CancelCISJobsQueryPS3 );
				string CancelCISJobsQueryTools = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (Tools)')";
				Update( Connection, CancelCISJobsQueryTools );
				string CancelCISJobsQueryScript = "UPDATE Jobs SET Complete = 1 WHERE (Name = 'CIS Code Builder (Script)')";
				Update( Connection, CancelCISJobsQueryScript );

				// 408 is "CIS Code Builder (Refresh)"
				string KickoffCommandString = "UPDATE Commands SET Pending = 1, Operator = '" + Context.User.Identity.Name + "' WHERE ( ID = 408 )";
				Update( Connection, KickoffCommandString );

				Connection.Close();
			}
		}
	}
}
