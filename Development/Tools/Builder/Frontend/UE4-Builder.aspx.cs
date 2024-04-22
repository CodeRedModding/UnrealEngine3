// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class UE4Builder : BasePage
{
	protected void Page_Load( object sender, EventArgs e )
	{
		string LoggedOnUser = Context.User.Identity.Name;
		string MachineName = Context.Request.UserHostName;

		Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"";
	}
}