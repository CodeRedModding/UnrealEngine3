// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Configuration;
using System.Collections;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;
using System.Drawing;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

public partial class BuildStatus : BasePage
{
    protected Dictionary<string, string> Errors = new Dictionary<string, string>();

    private int GetChangelist( SqlConnection Connection, string Branch, string Key )
    {
        string Query = "SELECT " + Key + " FROM BranchConfig WHERE ( Branch = '" + Branch + "' )";
        int Changelist = ReadInt( Connection, Query );
        return ( Changelist );
    }

	private void GetError( SqlConnection Connection, string Branch, string CISType )
    {
        string Key = CISType + "Error";
		string Query = "SELECT " + Key + " FROM BranchConfig WHERE ( Branch = '" + Branch + "' )";
        string VariableString = ReadString( Connection, Query );
        VariableString = VariableString.Replace( Environment.NewLine, "<br />" );
        if( VariableString.Length > 0 )
        {
            VariableString += "<br /><br />";
        }

        Errors.Add( CISType, VariableString );
    }

    private void GoodBuild( SqlConnection Connection, Label DisplayLabel, string Branch, string CISType, string Text )
    {
        Errors.Add( CISType, "" );

        int Changelist = GetChangelist( Connection, Branch, "LastGood" + CISType );
		if( Changelist < 0 )
		{
			DisplayLabel.Visible = false;
		}
		else
		{
			DisplayLabel.Visible = true;
			DisplayLabel.Text = Text + " " + Changelist.ToString();
			DisplayLabel.ForeColor = Color.Green;
		}
    }

	private void BadBuild( SqlConnection Connection, Label DisplayLabel, string Branch, string CISType, string Text )
    {
        int Changelist = GetChangelist( Connection, Branch, "LastFail" + CISType );
		if( Changelist < 0 )
		{
			DisplayLabel.Visible = false;
		}
		else
		{
			GetError( Connection, Branch, CISType );
			DisplayLabel.Visible = true;
			DisplayLabel.Text = Text + Changelist.ToString();
			DisplayLabel.ForeColor = Color.Red;
		}
    }

	private void SelectBuild( SqlConnection Connection, Label DisplayLabel, string Branch, string CISType, string GoodText, string BadText )
    {
        int LastAttempted = GetChangelist( Connection, Branch, "LastAttempted" + CISType );
		if( LastAttempted > 0 )
		{
			int LastGood = GetChangelist( Connection, Branch, "LastGood" + CISType );
			int LastFailed = GetChangelist( Connection, Branch, "LastFail" + CISType );

			if( LastFailed > LastGood )
			{
				BadBuild( Connection, DisplayLabel, Branch, CISType, BadText );
			}
			else
			{
				GoodBuild( Connection, DisplayLabel, Branch, CISType, GoodText );
			}
		}
		else
		{
			Errors.Add( CISType, "" );
		}
    }

	private void DisplayGoodBuild( SqlConnection Connection, string Branch, int LastChangeProcessed )
    {
        Label_State.Text = "The Build is GOOD! =)";
        Label_State.ForeColor = Color.Green;
		Label_Changelist.Text = "The overall build is currently GOOD. The last changelist submitted to CIS was " + LastChangeProcessed.ToString() + " and may still be processing. See below for additional details.";
        Label_Changelist.ForeColor = Color.Green;

		GoodBuild( Connection, Label_CISExamplePC, Branch, "Example", "CIS for ExampleGame PC is good and has completed through changelist " );
		GoodBuild( Connection, Label_CISGearPC, Branch, "Gear", "CIS for GearGame PC is good and has completed through changelist " );
		GoodBuild( Connection, Label_CISUDKPC, Branch, "UDK", "CIS for UDKGame PC is good and has completed through changelist " );
		GoodBuild( Connection, Label_CISMobile, Branch, "Mobile", "CIS for Mobile is good and has completed through changelist " );
		GoodBuild( Connection, Label_CISXbox360, Branch, "Xbox360", "CIS for Xbox360 is good and has completed through changelist " );
		GoodBuild( Connection, Label_CISPS3, Branch, "PS3", "CIS for PS3 is good and has completed through changelist " );
		GoodBuild( Connection, Label_CISTools, Branch, "Tools", "CIS for Tools is good and has completed through changelist " );
    }

	private void DisplayBadBuild( SqlConnection Connection, string Branch, int LastChangeProcessed )
    {
        Label_State.Text = "The Build is BAD! =(";
        Label_State.ForeColor = Color.Red;
		Label_Changelist.Text = "The overall build is currently BAD. The last changelist submitted to CIS was " + LastChangeProcessed.ToString() + " and may still be processing. See below for additional details.";
        Label_Changelist.ForeColor = Color.Red;

		SelectBuild( Connection, Label_CISExamplePC, Branch, "Example", "CIS for ExampleGame PC is good and has completed through changelist ", "CIS for ExampleGame PC is bad and has completed through changelist " );
		SelectBuild( Connection, Label_CISGearPC, Branch, "Gear", "CIS for GearGame PC is good and has completed through changelist ", "CIS for GearGame PC is bad and has completed through changelist " );
		SelectBuild( Connection, Label_CISUDKPC, Branch, "UDK", "CIS for UDKGame PC is good and has completed through changelist ", "CIS for UDKGame PC is bad and has completed through changelist " );
		SelectBuild( Connection, Label_CISMobile, Branch, "Mobile", "CIS for Mobile is good and has completed through changelist ", "CIS for Mobile is bad and has completed through changelist " );
		SelectBuild( Connection, Label_CISXbox360, Branch, "Xbox360", "CIS for Xbox360 is good and has completed through changelist ", "CIS for Xbox360 is bad and has completed through changelist " );
		SelectBuild( Connection, Label_CISPS3, Branch, "PS3", "CIS for PS3 is good and has completed through changelist ", "CIS for PS3 is bad and has completed through changelist " );
		SelectBuild( Connection, Label_CISTools, Branch, "Tools", "CIS for Tools is good and has completed through changelist ", "CIS for Tools is bad and has completed through changelist " );
	}

    protected void Page_Load( object sender, EventArgs e )
    {
		// Standard header info
		string LoggedOnUser = Context.User.Identity.Name;
		string MachineName = Context.Request.UserHostName;

		Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"";

		Label_BranchName.Text = Request.QueryString["BranchName"];
		if( Label_BranchName.Text.Length == 0 )
		{
			Label_BranchName.Text = "UnrealEngine3";
		}

		// Set up an array with all the variables we are interested in
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();

			int LastGoodOverall = GetChangelist( Connection, Label_BranchName.Text, "LastGoodOverall" );
			int LastFailedOverall = GetChangelist( Connection, Label_BranchName.Text, "LastFailOverall" );
			int LastChangeProcessed = GetChangelist( Connection, Label_BranchName.Text, "LastAttemptedOverall" );

			// Clear out any pending errors
			Errors.Clear();

			// Display overall build state
			if( LastGoodOverall > LastFailedOverall )
			{
				DisplayGoodBuild( Connection, Label_BranchName.Text, LastChangeProcessed );
			}
			else
			{
				DisplayBadBuild( Connection, Label_BranchName.Text, LastChangeProcessed );
			}

			Connection.Close();
		}
    }
}
