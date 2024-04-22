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

public partial class Web_Admin_ManageBuilds : System.Web.UI.Page
{
    protected void Page_Load( object sender, EventArgs e )
    {
        if( !Global.IsAdmin( Context.User.Identity.Name ) )
        {
            Response.Redirect( "~/Default.aspx" );
        }

        if( BuildsAdminScriptManager.AsyncPostBackSourceElementID.IndexOf( "TimerProgress" ) == -1 )
        {
            UpdateControlsState();
        }
    }

    protected void UpdateControlsState()
    {
        if( !IsPostBack )
        {
            if( Session["MB_Project"] != null )
            {
                ccdProjects.SelectedValue = Session["MB_Project"].ToString().Trim();
                BuildDataSource.SelectParameters[0].DefaultValue = WebUtils.GetParamFromCCD( ccdProjects.SelectedValue, true );
            }

            if( Session["MB_Platform"] != null )
            {
                ccdPlatforms.SelectedValue = Session["MB_Platform"].ToString().Trim();
                BuildDataSource.SelectParameters[1].DefaultValue = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, true );
            }

            if( Session["MB_User"] != null )
            {
                BuildDataSource.SelectParameters[3].DefaultValue = ManageBuildsUserDropDown.SelectedValue.Trim();
            }

            if( Session["MB_Status"] != null )
            {
                BuildDataSource.SelectParameters[2].DefaultValue = ManageBuildsStatusDropDown.SelectedValue.Trim();
            }

            Session["MB_BuildCount"] = 0;
        }
        else
        {
            Session["MB_Project"] = ccdProjects.SelectedValue.Trim();
            Session["MB_Platform"] = ccdPlatforms.SelectedValue.Trim();
            Session["MB_User"] = ManageBuildsUserDropDown.SelectedValue.Trim();
            Session["MB_Status"] = ManageBuildsStatusDropDown.SelectedValue.Trim();
        }
    }

    protected void ManageBuildsUserDropDown_DataBound( object sender, EventArgs e )
    {
        ManageBuildsUserDropDown.Items.Insert( 0, new ListItem( "All Users", "1" ) );
        if( Session["MB_User"] != null )
        {
            ManageBuildsUserDropDown.SelectedValue = Session["MB_User"].ToString().Trim();
        }
    }

    protected void ManageBuildsStatusDropDown_DataBound( object sender, EventArgs e )
    {
        ManageBuildsStatusDropDown.Items.Insert( 0, new ListItem( "All Statuses", "-1" ) );
        if( Session["MB_Status"] != null )
        {
            ManageBuildsStatusDropDown.SelectedValue = Session["MB_Status"].ToString().Trim();
        }
    }

    protected void BuildsGridView_RowUpdating( object sender, GridViewUpdateEventArgs e )
    {
        long BuildID = Int64.Parse( BuildsGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );
        BuildStatus StatusID = ( BuildStatus )Int16.Parse( e.NewValues["StatusID"].ToString().Trim() );
        string BuildName = e.NewValues["Title"].ToString().Trim();
        UnrealProp.Global.PlatformBuild_ChangeStatus( BuildID, StatusID, BuildName );

        // to avoid datasource update request
        e.Cancel = true;
        BuildsGridView.EditIndex = -1;
    }

	protected void BuildsGridView_RowDeleting( object sender, GridViewDeleteEventArgs e )
	{
		long BuildID = Int64.Parse( BuildsGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );
		UnrealProp.Global.PlatformBuild_ChangeStatus( BuildID, BuildStatus.Hidden, null );

		// to avoid datasource delete request
		e.Cancel = true;
		BuildsGridView.DataBind();
	}

    protected void TimerProgress_Tick( object sender, EventArgs e )
    {
        // Check for a new build being available
        int BuildCount = Global.PlatformBuild_GetCount( "Any", "Any" );
        if( Session["MB_BuildCount"] == null || BuildCount != ( int )Session["MB_BuildCount"] )
        {
            BuildsGridView.DataBind();
            ManageBuildsUpdatePanel.Update();

            Session["MB_BuildCount"] = BuildCount;
            return;
        }

        // Also refresh progress on grid if a build is analyzing
        if( BuildsGridView.EditIndex == -1 )
        {
            int StatusIndex = 4;

            foreach( GridViewRow Row in BuildsGridView.Rows )
            {
                string Status = ( ( Label )Row.Cells[StatusIndex].FindControl( "Label1" ) ).Text.Trim();
                if( Status == "Discovered" || Status.IndexOf( "Analyzing" ) > -1 )
                {
                    BuildsGridView.DataBind();
                    ManageBuildsUpdatePanel.Update();
                    return;
                }
            }
        }
    }

    protected void BuildsGridView_RowDataBound( object sender, GridViewRowEventArgs e )
    {
        if( e.Row.RowType == DataControlRowType.DataRow )
        {
            RemotableType.PlatformBuilds.PlatformBuildsRow Build = ( RemotableType.PlatformBuilds.PlatformBuildsRow )( ( DataRowView )e.Row.DataItem ).Row;

            if( e.Row.RowIndex != BuildsGridView.EditIndex )
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
