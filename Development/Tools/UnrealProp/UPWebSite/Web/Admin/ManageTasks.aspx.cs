/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Configuration;
using System.Collections;
using System.Data;
using System.Drawing;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using RemotableType;
using UnrealProp;

public partial class Web_Admin_ManageTasks : System.Web.UI.Page
{
    protected void Page_Load( object sender, EventArgs e )
    {
        if( !Global.IsAdmin( Context.User.Identity.Name ) )
        {
            Response.Redirect( "~/Default.aspx" );
        }

        if( !IsPostBack )
        {
            AdminTaskUPDSDropDown.DataBind();
        }

        if( TasksAdminScriptManager.AsyncPostBackSourceElementID.IndexOf( "TimerProgress" ) == -1 )
        {
            UpdatesControlsState();
        }
    }

    protected void AdminTaskUPDSDropDown_DataBinding( object sender, EventArgs e )
    {
        string[] DistributionServers;

        DistributionServers = Global.DistributionServer_GetListFromTasks();

        AdminTaskUPDSDropDown.Items.Clear();
        AdminTaskUPDSDropDown.Items.Add( "All Servers" );
        AdminTaskUPDSDropDown.Items[0].Value = "";

        if( DistributionServers != null )
        {
            foreach( string Server in DistributionServers )
            {
                AdminTaskUPDSDropDown.Items.Add( Server );
            }
        }
    }

    private void ConstructQuery( ref string Query, string Key, string Value )
    {
        if( Value != "-1" && Value != "'-1'" && Value != "" )
        {
            if( Query != string.Empty )
            {
                Query += " AND ";
            }
            
            Query += Key + " = " + Value;
        }
    }

    protected void UpdatesControlsState()
    {
        if( !IsPostBack )
        {
            AdminTasksGridView.Sort( "ScheduleTime", SortDirection.Descending );

            if( Session["MT_Status"] != null )
            {
                ccdStatuses.SelectedValue = Session["MT_Status"].ToString().Trim();
            }

            if( Session["MT_Project"] != null )
            {
                ccdProjects.SelectedValue = Session["MT_Project"].ToString().Trim();
            }

            if( Session["MT_Title"] != null )
            {
                ccdBuilds.SelectedValue = Session["MT_Title"].ToString().Trim();
            }

            if( Session["MT_Platform"] != null )
            {
                ccdPlatforms.SelectedValue = Session["MT_Platform"].ToString().Trim();
            }

            if( Session["MT_ClientMachine"] != null )
            {
                ccdClientMachines.SelectedValue = Session["MT_ClientMachine"].ToString().Trim();
            }
        }
        else
        {
            Session["MT_Status"] = ccdStatuses.SelectedValue.Trim();
            Session["MT_Project"] = ccdProjects.SelectedValue.Trim();
            Session["MT_Title"] = ccdBuilds.SelectedValue.Trim();
            Session["MT_Platform"] = ccdPlatforms.SelectedValue.Trim();
            Session["MT_ClientMachine"] = ccdClientMachines.SelectedValue.Trim();

            Session["MT_AdminUser"] = AdminTaskUserDropDown.SelectedValue.Trim();
        }

        string SqlQuery = string.Empty;

        string Param = WebUtils.GetParamFromCCD( ccdStatuses.SelectedValue, true );
        ConstructQuery( ref SqlQuery, "StatusID", Param );

        Param = WebUtils.GetParamFromCCD( ccdProjects.SelectedValue, false );
        ConstructQuery( ref SqlQuery, "Project", "'" + Param + "'" );

        Param = WebUtils.GetParamFromCCD( ccdBuilds.SelectedValue, false );
        ConstructQuery( ref SqlQuery, "Title", "'" + Param + "'" );

		Param = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, false );
		ConstructQuery( ref SqlQuery, "TargetPlatform", "'" + Param + "'" );

        Param = WebUtils.GetParamFromCCD( ccdClientMachines.SelectedValue, false );
        ConstructQuery( ref SqlQuery, "ClientMachineName", "'" + Param + "'" );

		if( AdminTaskUserDropDown.SelectedItem != null )
		{
			string SelectedUser = AdminTaskUserDropDown.SelectedItem.ToString();
			if( SelectedUser != "All Users" )
			{
				ConstructQuery( ref SqlQuery, "Email", "'" + SelectedUser + "'" );
			}
		}

        if( AdminTaskUPDSDropDown.SelectedValue != "" )
        {
            ConstructQuery( ref SqlQuery, "AssignedUPDS", "'" + AdminTaskUPDSDropDown.SelectedItem + "'" );
        }

        if( SqlQuery != string.Empty )
        {
            TaskDataSource.FilterExpression = SqlQuery;
        }
        else
        {
            TaskDataSource.FilterExpression = "ID IS NOT NULL";
        }

        AdminTasksGridView.DataBind();
    }

    protected void AdminTaskUserDropDown_DataBound( object sender, EventArgs e )
    {
        AdminTaskUserDropDown.Items.Insert( 0, new ListItem( "All Users", "" ) );
        if( Session["MT_AdminUser"] != null )
        {
            AdminTaskUserDropDown.SelectedValue = Session["MT_AdminUser"].ToString().Trim();
        }
    }

    protected void TimerProgress_Tick( object sender, EventArgs e )
    {
        int StatusIndex = 1;
        foreach( GridViewRow row in AdminTasksGridView.Rows )
        {
            string Status = ( ( Label )row.Cells[StatusIndex].FindControl( "AdminTaskStatusLabel" ) ).Text.Trim();
            if( Status == "Scheduled" || Status.IndexOf( "In Progress" ) > -1 )
            {
                UpdatesControlsState();
                AdminTaskGridViewUpdatePanel.Update();
                break;
            }
        }
    }

    protected void AdminTasksGridViewTasks_RowDataBound( object sender, GridViewRowEventArgs e )
    {
        int StatusIndex = 1;
        if( e.Row.RowType == DataControlRowType.DataRow )
        {
            Tasks.TasksRow Task = ( Tasks.TasksRow )( ( DataRowView )e.Row.DataItem ).Row;

            Label StatusLabel = ( ( Label )e.Row.Cells[StatusIndex].FindControl( "AdminTaskStatusLabel" ) );
            UnrealProp.ProgressBar PropProgressBar = ( ( UnrealProp.ProgressBar )e.Row.Cells[4].FindControl( "AdminTaskPropProgressBar" ) );

            if( Task.StatusID == ( short )TaskStatus.InProgress ) 
            {
                StatusLabel.Text = StatusLabel.Text + " " + Task.Progress + "%";
                StatusLabel.ForeColor = Color.Purple;
                PropProgressBar.Progress = Task.Progress;
                PropProgressBar.Visible = true;
            }
            else
            {
                PropProgressBar.Visible = false;
            }

            if( Task.StatusID == ( short )TaskStatus.Failed || Task.StatusID == ( short )TaskStatus.Canceled ) 
            {
                StatusLabel.ForeColor = Color.Red;
                StatusLabel.Font.Bold = true;
            }

            if( Task.StatusID == ( short )TaskStatus.Finished )
            {
                StatusLabel.ForeColor = System.Drawing.Color.DarkGreen;
            }

            if( Task.StatusID == ( short )TaskStatus.Finished || Task.StatusID == ( short )TaskStatus.Canceled || Task.StatusID == ( short )TaskStatus.Failed )
            {
                LinkButton Button = ( LinkButton )e.Row.Cells[0].Controls[0];
                Button.Visible = false;
            }
        }
    }

    protected void AdminTasksGridView_RowDeleting( object sender, GridViewDeleteEventArgs e )
    {
        long TaskID = Int64.Parse( AdminTasksGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );
        Global.Task_UpdateStatus( TaskID, TaskStatus.Canceled, 100, "Canceled" );

        // to avoid datasource delete request
        e.Cancel = true;
        AdminTasksGridView.DataBind();
    }
}
