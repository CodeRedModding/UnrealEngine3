/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Data;
using System.Configuration;
using System.Collections;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using AjaxControlToolkit;
using RemotableType;
using UnrealProp;

public partial class Web_User_TaskScheduler : System.Web.UI.Page
{
    protected void Page_Load( object sender, EventArgs e )
    {
        if( !Global.IsUser( Context.User.Identity.Name ) )
        {
            Response.Redirect( "~/Default.aspx" );
        }

        UpdateControlsState();
    }

    protected void ScheduleTask( bool GetItNow )
    {
        try
        {
            // Check for some builds to select
            if( BuildsListBox.Items.Count > 0 )
            {
                long PlatformBuildID = 0;
                try
                {
                    PlatformBuildID = long.Parse( BuildsListBox.SelectedValue );
                }
                catch
                {
                }

                // Work out the build index
                if( PlatformBuildID > 0 )
                {
                    // Create a list of targets
                    ArrayList ClientTargets = new ArrayList();

                    // Add in the console targets
                    foreach( DataListItem ClientTarget in ClientMachineList.Items )
                    {
                        CheckBox ClientTargetCheckBox = ( CheckBox )ClientTarget.FindControl( "TargetCheckBox" );
                        HiddenField ClientTargetHiddenID = ( HiddenField )ClientTarget.FindControl( "HiddenID" );
                        if( ClientTargetCheckBox.Checked )
                        {
                            ClientTargets.Add( long.Parse( ClientTargetHiddenID.Value ) );
                        }
                    }

                    // Add in the PC targets
                    foreach( DataListItem ClientTarget in PCMachineList.Items )
                    {
                        CheckBox ClientTargetCheckBox = ( CheckBox )ClientTarget.FindControl( "PCCheckBox" );
                        HiddenField ClientTargetHiddenID = ( HiddenField )ClientTarget.FindControl( "HiddenID" );
                        if( ClientTargetCheckBox.Checked )
                        {
                            ClientTargets.Add( long.Parse( ClientTargetHiddenID.Value ) );
                        }
                    }

                    if( ClientTargets.Count > 0 )
                    {
                        DateTime ScheduledTime;

                        // Get the default config for running after a prop
                        string SelectedConfig = "Release";
                        if( DropDownList_AvailableConfigs.SelectedItem != null )
                        {
                            SelectedConfig = DropDownList_AvailableConfigs.SelectedItem.Text.Trim();
                        }

                        // Get the commandline
                        string CommandLine = TextBox_RunCommandLine.Text;

                        if( PlatformBuildID > 0 )
                        {
                            if( GetItNow )
                            {
                                ScheduledTime = DateTime.Now;
                            }
                            else
                            {
                                string DateAndTime = DateTime.Now.ToString( "MM/dd/yyyy" ) + " " + ScheduleTime.Text;
                                ScheduledTime = DateTime.ParseExact( DateAndTime, "MM/dd/yyyy HH:mm:ss", null );
                                if( ScheduledTime < DateTime.Now )
                                {
                                    ScheduledTime = ScheduledTime.AddDays( 1.0 );
                                }
                            }

                            foreach( object Target in ClientTargets )
                            {
                                string[] Name = User.Identity.Name.Split( '\\' );
                                int TargetIndex = ( int )( long )Target;
                                Global.Task_AddNew( PlatformBuildID, ScheduledTime, TargetIndex, Name[1] + "@" + Name[0] + ".com",
                                    CheckBox_RunAfterProp.Checked, SelectedConfig, CommandLine, CheckBox_RecurringTask.Checked );
                            }

                            // Touch the build time to make it more recent
                            Global.PlatformBuild_ChangeTime( PlatformBuildID, ScheduledTime );

                            TaskScheduledInfoLabel.Text = "Build " + BuildsListBox.SelectedItem.Text.Trim() + " has been scheduled to copy.";

                            // Disable the recurring task after scheduling
                            CheckBox_RecurringTask.Checked = false;

                            if( GetItNow )
                            {
                                Response.Redirect( "~/Web/User/ManageTasks.aspx" );
                            }
                        }
                    }
                    else
                    {
                        TaskScheduledInfoLabel.Text = "Please select target machine(s)!";
                    }
                }
                else
                {
                    TaskScheduledInfoLabel.Text = "Please select a build!";
                }
            }
            else
            {
                TaskScheduledInfoLabel.Text = "Please select a project and platform!";
            }
        }
        catch( Exception ex )
        {
            TaskScheduledInfoLabel.Text = "ERROR: " + ex.Message;
        }

        TaskScheduledInfoLabel.Visible = true;
    }

    protected void GetBuildNowButton_Click( object sender, EventArgs e )
    {
        ScheduleTask( true );
    }

    protected void ScheduleTaskButton_Click( object sender, EventArgs e )
    {
        ScheduleTask( false );
    }

    private void PopulateGroupList()
    {
        int ItemIndex = 0;
        GroupCheckBoxes.Items.Clear();

        string Platform = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, false );
        if( Platform != "-1" )
        {
            GroupCheckBoxes.Items.Insert( ItemIndex++, new ListItem( "My Targets", "1" ) );
            Session["TSCB_My Targets"] = false;

            ClientGroups Groups = Global.ClientGroups_GetByPlatform( Platform );
            foreach( ClientGroups.ClientGroupsRow Row in Groups.Tables[0].Rows )
            {
                GroupCheckBoxes.Items.Insert( ItemIndex++, new ListItem( Row.GroupName, Row.ID.ToString() ) );
                Session["TSCB_" + Row.GroupName] = false;
            }
        }
    }

    protected void AvailableConfigs_DataBind( object sender, EventArgs e )
    {
        string Platform = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, false );
        string[] Configs = Global.BuildConfigs_GetForPlatform( Platform );

        DropDownList_AvailableConfigs.Items.Clear();

        if( Configs != null )
        {
            foreach( string Config in Configs )
            {
                DropDownList_AvailableConfigs.Items.Add( Config );
            }
        }
    }

    protected void UpdateControlsState()
    {
        if( !IsPostBack )
        {
            ScheduleTime.Text = DateTime.Now.ToString( "HH:mm:ss" );

            if( Session["TS_Project"] != null )
            {
                ccdProjects.SelectedValue = Session["TS_Project"].ToString().Trim();

                BuildDataSource.SelectParameters[0].DefaultValue = WebUtils.GetParamFromCCD( ccdProjects.SelectedValue, true );
            }

            if( Session["TS_Platform"] != null )
            {
                ccdPlatforms.SelectedValue = Session["TS_Platform"].ToString().Trim();
                // set PlatformID to datasource param
                TargetMachineDataSource.SelectParameters[0].DefaultValue = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, true );
                BuildDataSource.SelectParameters[1].DefaultValue = WebUtils.GetParamFromCCD( ccdPlatforms.SelectedValue, true );
            }

            if( Session["TS_RecurringTask"] != null )
            {
                CheckBox_RecurringTask.Checked = ( bool )Session["TS_RecurringTask"];
            }

            // Set up the config dropdown for the run after prop options
            AvailableConfigs_DataBind( null, null );

            if( Session["TS_RunAfterProp"] != null )
            {
                CheckBox_RunAfterProp.Checked = ( bool )Session["TS_RunAfterProp"];
            }

            if( Session["TS_AvailConfig"] != null )
            {
                DropDownList_AvailableConfigs.SelectedItem.Text = ( string )Session["TS_AvailConfig"];
            }

            if( Session["TS_CommandLine"] != null )
            {
                TextBox_RunCommandLine.Text = ( string )Session["TS_CommandLine"];
            }

            PopulateGroupList();
        }
        else
        {
            BuildDataSource.SelectParameters[0].DefaultValue = null;
            BuildDataSource.SelectParameters[1].DefaultValue = null;
            TargetMachineDataSource.SelectParameters[0].DefaultValue = null;

            Session["TS_Project"] = ccdProjects.SelectedValue.Trim();
            Session["TS_Build"] = BuildsListBox.SelectedValue.Trim();

            if( Session["TS_Platform"] != null )
            {
                if( Session["TS_Platform"].ToString().Trim() != ccdPlatforms.SelectedValue.Trim() )
                {
                    PopulateGroupList();
                    AvailableConfigs_DataBind( null, null );
                }
            }

            Session["TS_Platform"] = ccdPlatforms.SelectedValue.Trim();
            Session["TS_RecurringTask"] = CheckBox_RecurringTask.Checked;
            Session["TS_RunAfterProp"] = CheckBox_RunAfterProp.Checked;
            if( DropDownList_AvailableConfigs.SelectedItem != null )
            {
                Session["TS_AvailConfig"] = DropDownList_AvailableConfigs.SelectedItem.Text.Trim();
            }

            Session["TS_CommandLine"] = TextBox_RunCommandLine.Text.Trim();
        }
    }

    protected void GroupButtonNone_Click( object sender, EventArgs e )
    {
        foreach( ListItem GroupCheckBox in GroupCheckBoxes.Items )
        {
            GroupCheckBox.Selected = false;
            Session["TSCB_" + GroupCheckBox.Text] = false;
        }

        foreach( DataListItem ClientMachine in ClientMachineList.Items )
        {
            ( ( CheckBox )ClientMachine.FindControl( "TargetCheckBox" ) ).Checked = false;
        }
    }

    protected void GroupButtonAll_Click( object sender, EventArgs e )
    {
        foreach( ListItem GroupCheckBox in GroupCheckBoxes.Items )
        {
            GroupCheckBox.Selected = true;
            Session["TSCB_" + GroupCheckBox.Text] = true;
        }

        foreach( DataListItem ClientMachine in ClientMachineList.Items )
        {
            ( ( CheckBox )ClientMachine.FindControl( "TargetCheckBox" ) ).Checked = true;
        }
    }

    protected void GroupCheckBoxes_SelectedIndexChanged( object sender, EventArgs e )
    {
        // Update the state of the group checkboxes
        ListItem ChangedItem = null;
        foreach( ListItem GroupCheckBox in GroupCheckBoxes.Items )
        {
            string CheckBoxName = GroupCheckBox.Text;
            bool OldSelected = ( bool )Session["TSCB_" + CheckBoxName];
            if( OldSelected != GroupCheckBox.Selected )
            {
                ChangedItem = GroupCheckBox;
                Session["TSCB_" + CheckBoxName] = GroupCheckBox.Selected;
            }
        }

        // Apply the changes to the group boxes
        if( ChangedItem != null )
        {
            string Platform = WebUtils.GetParamFromCCD( ( string )Session["TS_Platform"], false );
            ClientMachines TargetMachines = Global.ClientMachine_GetListForPlatformGroupUser( Platform, ChangedItem.Text, Global.GetEmail( Context.User.Identity.Name ) );

            foreach( ClientMachines.ClientMachinesRow Row in TargetMachines.Tables[0].Rows )
            {
                foreach( DataListItem ClientMachine in ClientMachineList.Items )
                {
                    CheckBox ClientMachineCheckBox = ( CheckBox )ClientMachine.FindControl( "TargetCheckBox" );
                    if( ClientMachineCheckBox.Text == Row.Name )
                    {
                        ClientMachineCheckBox.Checked = ChangedItem.Selected;
                    }
                }
            }
        }
    }

    protected void BuildsListBox_PreRender( object sender, EventArgs e )
    {
        if( Session["TS_Build"] != null )
        {
            ListItem Build = BuildsListBox.Items.FindByValue( Session["TS_Build"].ToString().Trim() );
            if( Build != null )
            {
                Build.Selected = true;
                BuildsListBox.SelectedValue = Session["TS_Build"].ToString().Trim();
            }
        }
    }
}
