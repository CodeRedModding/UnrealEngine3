/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Configuration;
using System.Collections;
using System.Data;
using System.Diagnostics;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using RemotableType;
using UnrealProp;

public partial class Web_Admin_AddNews : System.Web.UI.Page
{
    protected void Page_Load( object sender, EventArgs e )
    {
        if( !Global.IsAdmin( Context.User.Identity.Name ) )
        {
            Response.Redirect( "~/Default.aspx" );
        }
    }

    protected void NewsTextBox_PreRender( object Sender, EventArgs e )
    {
        NewsTextBox.Content = Global.News_Get();
    }

    protected void SaveNewsButton_Click( object sender, EventArgs e )
    {
		Global.News_Add( NewsTextBox.Content );
    }
}
