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
using RemotableType;
using UnrealProp;

public partial class Web_User_ManageClientsMachines : System.Web.UI.Page
{
    protected void Page_Load( object sender, EventArgs e )
    {
        if( !Global.IsUser( Context.User.Identity.Name ) )
        {
            Response.Redirect( "~/Default.aspx" );
        }

        UpdateControlsState();
    }

    protected void UpdateControlsState()
    {
        if( !IsPostBack )
        {
            if( Session["UCM_Platform"] != null )
            {
                PlatformCascadingDropDown.SelectedValue = Session["UCM_Platform"].ToString().Trim();
                UserClientMachinesDataSource.SelectParameters[0].DefaultValue = WebUtils.GetParamFromCCD( PlatformCascadingDropDown.SelectedValue, true );
            }
        }
        else
        {
            Session["UCM_Platform"] = PlatformCascadingDropDown.SelectedValue.Trim();
        }
    }

    protected void MenuTabs_MenuItemClick( object sender, MenuEventArgs e )
    {
		int TabIndex = Int32.Parse( MenuTabs.SelectedValue.Trim() );
		MultiView1.ActiveViewIndex = TabIndex;

		switch( TabIndex )
		{
		case 0:
			break;
		case 1:
			string[] FullUserName = User.Identity.Name.Split( '\\' );
			TargetEmail.Text = FullUserName[1] + "@" + FullUserName[0].ToLower() + ".com";
			break;
		default:
			break;
		}
    }

    protected void FormView1_ItemInserted( object sender, FormViewInsertedEventArgs e )
    {
        MultiView1.ActiveViewIndex = 0;
    }

    protected void TargetsUserDropDown_PreRender( object sender, EventArgs e )
    {
        string[] Name = User.Identity.Name.Split( '\\' );
		string Email = Name[1] + "@" + Name[0] + ".com";
		int UserNameID = Global.User_GetID( Email );
        UserTargetsUserDropDown.Items.Insert( 0, new ListItem( Email, UserNameID.ToString() ) );
        UserTargetsUserDropDown.Enabled = false;
    }

    protected void UserClientMachineGridView_RowDeleting( object sender, GridViewDeleteEventArgs e )
    {
        int ClientMachineID = Int32.Parse( UserClientMachineGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );
        Global.ClientMachine_Delete( ClientMachineID );

        // to avoid datasource delete request
        e.Cancel = true;
        UserClientMachineGridView.DataBind();
    }
	
	private string GetNewValue( GridViewUpdateEventArgs e, string Element )
	{
		string NewValue = "";
		if( e.NewValues[Element] != null )
		{
			NewValue = e.NewValues[Element].ToString().Trim();
		}

		return( NewValue );
	}

    protected void UserClientMachineGridView_RowUpdating( object sender, GridViewUpdateEventArgs e )
    {
        int ClientMachineID = Int32.Parse( UserClientMachineGridView.DataKeys[e.RowIndex].Values["ID"].ToString().Trim() );

        string Platform = e.OldValues["Platform"].ToString().Trim();
		string Name = GetNewValue( e, "Name" );
		string Path = GetNewValue( e, "Path" );
		string ClientGroupName = GetNewValue( e, "ClientGroupName" );
		string Email = GetNewValue( e, "Email" );
		bool Reboot = Boolean.Parse( GetNewValue( e, "Reboot" ) );
        Global.ClientMachine_Update( ClientMachineID, Platform, Name, Path, ClientGroupName, Email, Reboot );

        // to avoid datasource update request
        e.Cancel = true;
        UserClientMachineGridView.EditIndex = -1;
    }

    protected void AddNewTargetButton_Click( object sender, EventArgs e )    
    {
        string Platform = AddNewTargetPlatform.SelectedItem.ToString().Trim();
        string Name = TargetName.Text.Trim();
        string Path = TargetPath.Text.Trim();
        string ClientGroupName = TargetGroup.Text.Trim();
        string Email = TargetEmail.Text.Trim();
        Global.ClientMachine_Update( -1, Platform, Name, Path, ClientGroupName, Email, TargetReboot.Checked );

        TargetName.Text = "";
        TargetPath.Text = "";
        TargetGroup.Text = "";
        TargetReboot.Checked = true;

        UserClientMachineGridView.DataBind();
        MultiView1.ActiveViewIndex = 0;
    }

    protected void UserClientMachineGridView_RowDataBound( object sender, GridViewRowEventArgs e )
    {
        if( e.Row.RowType == DataControlRowType.DataRow && e.Row.RowIndex == UserClientMachineGridView.EditIndex )
        {
            TextBox tbName = ( TextBox )e.Row.Cells[2].Controls[0];
            TextBox tbPath = ( TextBox )e.Row.Cells[3].Controls[0];
            tbName.Text = tbName.Text.Trim();
            tbPath.Text = tbPath.Text.Trim();

            tbName.Width = 150;
            tbPath.Width = 400;
        }
    }
}
