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

public partial class Web_User_ManageBuilds : System.Web.UI.Page
{
    protected void Page_Load( object sender, EventArgs e )
    {
        if( !Global.IsUser( Context.User.Identity.Name ) )
        {
            Response.Redirect( "~/Default.aspx" );
        }

        if( BuildsScriptManager.AsyncPostBackSourceElementID.IndexOf( "TimerProgress" ) == -1 )
        {
            UpdateControlsState();
        }
    }

    protected void UpdateControlsState()
    {
        if( !IsPostBack )
        {
            if( Session["UMB_Project"] != null )
            {
                ccdProjects.SelectedValue = Session["UMB_Project"].ToString().Trim();
                BuildDataSource.SelectParameters[0].DefaultValue = WebUtils.GetParamFromCCD( ccdProjects.SelectedValue, true );
            }

            if( Session["UMB_Platform"] != null )
            {
                ccdPlatforms.SelectedValue = Session["UMB_Platform"].ToString().Trim();
                BuildDataSource.SelectParameters[1].DefaultValue = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, true );
            }

            if( Session["UMB_Status"] != null )
            {
                BuildDataSource.SelectParameters[2].DefaultValue = ManageBuildsStatusDropDown.SelectedValue.Trim();
            }

            Session["UMB_BuildCount"] = 0;
        }
        else
        {
            Session["UMB_Project"] = ccdProjects.SelectedValue.Trim();
            Session["UMB_Platform"] = ccdPlatforms.SelectedValue.Trim();
            Session["UMB_Status"] = ManageBuildsStatusDropDown.SelectedValue.Trim();
        }
    }

    protected void ManageBuildsUserDropDown_PreRender( object sender, EventArgs e )
    {
        string[] Name = User.Identity.Name.Split( '\\' );
		string Email = Name[1] + "@" + Name[0] + ".com";
        int UserNameID = Global.User_GetID( Email );
        ManageBuildsUserDropDown.Items.Insert( 0, new ListItem( Email, UserNameID.ToString()) );
        ManageBuildsUserDropDown.Enabled = false;
    }

    protected void ManageBuildsStatusDropDown_DataBound( object sender, EventArgs e )
    {
        ManageBuildsStatusDropDown.Items.Insert( 0, new ListItem( "All Statuses", "-1" ) );
        if( Session["UMB_Status"] != null )
        {
            ManageBuildsStatusDropDown.SelectedValue = Session["UMB_Status"].ToString().Trim();
        }
    }

    protected void UserBuildsGridView_RowUpdating( object sender, GridViewUpdateEventArgs e )
    {
        long BuildID = Int64.Parse( UserBuildsGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );
        BuildStatus StatusID = ( BuildStatus )Int16.Parse( e.NewValues["StatusID"].ToString().Trim() );
        string BuildName = e.NewValues["Title"].ToString().Trim();
        UnrealProp.Global.PlatformBuild_ChangeStatus( BuildID, StatusID, BuildName );

        // to avoid datasource update request
        e.Cancel = true;
        UserBuildsGridView.EditIndex = -1;
    }

	protected void UserBuildsGridView_RowDeleting( object sender, GridViewDeleteEventArgs e )
	{
		long BuildID = Int64.Parse( UserBuildsGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );
		UnrealProp.Global.PlatformBuild_ChangeStatus( BuildID, BuildStatus.Hidden, null );

		// to avoid datasource delete request
		e.Cancel = true;
		UserBuildsGridView.DataBind();
	}

    protected void TimerProgress_Tick( object sender, EventArgs e )
    {
        // Check for a new build being available
        int BuildCount = Global.PlatformBuild_GetCount( "Any", "Any" );
        if( Session["UMB_BuildCount"] == null || BuildCount != ( int )Session["UMB_BuildCount"] )
        {
            UserBuildsGridView.DataBind();
            UserManageBuildsUpdatePanel.Update();

            Session["UMB_BuildCount"] = BuildCount;
            return;
        }

        // Also refresh progress on grid if a build is analyzing
        if( UserBuildsGridView.EditIndex == -1 )
        {
            int StatusIndex = 4;

            foreach( GridViewRow Row in UserBuildsGridView.Rows )
            {
                string Status = ( ( Label )Row.Cells[StatusIndex].FindControl( "Label1" ) ).Text.Trim();
                if( Status == "Discovered" || Status.IndexOf( "Analyzing" ) > -1 )
                {
                    UserBuildsGridView.DataBind();
                    UserManageBuildsUpdatePanel.Update();
                    return;
                }
            }
        }
    }

    protected void UserBuildsGridView_RowDataBound( object sender, GridViewRowEventArgs e )
    {
        if( e.Row.RowType == DataControlRowType.DataRow )
        {
            RemotableType.PlatformBuilds.PlatformBuildsRow Build = ( RemotableType.PlatformBuilds.PlatformBuildsRow )( ( DataRowView )e.Row.DataItem ).Row;

            if( e.Row.RowIndex != UserBuildsGridView.EditIndex )
            {
                int Progress = 100;
                if( Build.StatusID == ( short )BuildStatus.Discovered )
                {
                    Progress = 0;
                }

                UnrealProp.ProgressBar ProgressBarControl = ( UnrealProp.ProgressBar )e.Row.Cells[4].FindControl( "AnalysingProgressBar" );
                ProgressBarControl.Visible = false;

                if( Build.StatusID == ( short )BuildStatus.Analyzing )
                {
                    Progress = Global.PlatformBuild_GetAnalyzingProgress( Build.ID );

                    Label ProgressLabel = ( ( Label )e.Row.Cells[4].FindControl( "Label1" ) );
                    ProgressLabel.Text = ProgressLabel.Text + " [" + Progress + "%]";
                    ProgressBarControl.Visible = true;
                }

                ProgressBarControl.Progress = Progress;
            }
        }
    }
}
