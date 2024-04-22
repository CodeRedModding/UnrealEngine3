// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Data;
using System.Configuration;
using System.Collections;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Drawing;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

public partial class DefaultOld : BasePage
{
    private int ActiveBuilderCount = -1;
	private HttpCookie UserDetailLevel = null;

#if false
	public class ButtonDefinition
	{
		public string ID = "";
		public string Text = "";
		public int UserLevel = 0;

		public ButtonDefinition( string InID, string InText, int InUserLevel )
		{
			ID = InID;
			Text = InText;
			UserLevel = InUserLevel;
		}
	}

	public List<ButtonDefinition> ButtonDefs = new List<ButtonDefinition>()
	{ 
		new ButtonDefinition( "Button_Engine", "Trigger Engine Build", 0 ),
		new ButtonDefinition( "Button_Promote", "Promote Build", 1 ),
		new ButtonDefinition( "Button_Example", "Trigger Example Build", 2 ),
		new ButtonDefinition( "Button_UDK", "Trigger UDK Build", 1 ),
		new ButtonDefinition( "Button_Rift", "Trigger Rift Build", 0 ),
		new ButtonDefinition( "Button_RiftBeta", "Trigger Rift Beta Build", 1 ),
		new ButtonDefinition( "Button_Sword", "Trigger Sword Build", 2 ),
		new ButtonDefinition( "Button_InfinityBlade1", "Trigger Infinity Blade Build", 2 ),
		new ButtonDefinition( "Button_Maintenance", "Builder Maintenance", 2 ),
		new ButtonDefinition( "Button_Tools", "Trigger Tool Build", 2 ),
		new ButtonDefinition( "Button_CIS", "Trigger CIS Build", 1 ),
		new ButtonDefinition( "Button_Verification", "Trigger Verification Build", 1 ),
	};
#endif

    protected void Page_Load( object sender, EventArgs e )
    {
        string LoggedOnUser = Context.User.Identity.Name;
        string MachineName = Context.Request.UserHostName;

        Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\".";

        ScriptTimer_Tick( sender, e );
		JobTimer_Tick( sender, e );
		VerifyTimer_Tick( sender, e );

		UserDetailLevel = Request.Cookies["UserDetailLevel"];
		if( UserDetailLevel == null )
		{
			ComboBox_UserDetail.SelectedIndex = 0;

			UserDetailLevel = new HttpCookie( "UserDetailLevel", "0" );
			UserDetailLevel.Expires = DateTime.Now.AddDays( 10.0 );
			Response.Cookies.Add( UserDetailLevel );
		}
		else
		{
			if( !IsPostBack )
			{
				ComboBox_UserDetail.SelectedIndex = Int32.Parse( UserDetailLevel.Value );
			}
		}
    }

	protected void SelectedIndexChanged( object sender, EventArgs e )
	{
		int UserDetailValue = ComboBox_UserDetail.SelectedIndex;
		Response.Cookies["UserDetailLevel"].Expires = DateTime.Now.AddDays( 10.0 );
		Response.Cookies["UserDetailLevel"].Value = UserDetailValue.ToString();
	}

    protected void Button_TriggerBuild_Click( object sender, EventArgs e )
    {
		List<string> Buttons = new List<string>()
		{
			"Button_Promote",
			"Button_Maintenance",
			"Button_Tools",
			"Button_CIS",
			"Button_Verification",
			"Button_BuildCharts",
		};

		int UserDetailValue = ComboBox_UserDetail.SelectedIndex;
		string UserDetailArg = "&UserDetail=" + UserDetailValue.ToString();

        Button Pressed = ( Button )sender;
        if( Pressed.ID == "Button_BuildStatus" )
        {
            Response.Redirect( "BuildStatus.aspx?BranchName=UnrealEngine3" );
        }
		else if( Pressed.ID == "Button_BuildStatus_GFx" )
		{
			Response.Redirect( "BuildStatus.aspx?BranchName=UnrealEngine3-GFx" );
		}
		else if( Pressed.ID.StartsWith( "Button_Engine" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=1000&MaxAccess=1600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Example" ))
		{
			Response.Redirect( "Game.aspx?MinAccess=4000&MaxAccess=4600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_UDK" ))
		{
			Response.Redirect( "Game.aspx?MinAccess=5000&MaxAccess=5600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Exodus" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=15000&MaxAccess=15600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Fortress" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=13000&MaxAccess=13600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Lemming" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=11000&MaxAccess=11600" + UserDetailArg );
		}
		else if (Pressed.ID.StartsWith("Button_UE4"))
		{
			Response.Redirect("Game.aspx?MinAccess=21000&MaxAccess=21600" + UserDetailArg);
		}
		else if (Pressed.ID.StartsWith("Button_RiftRift"))
		{
			Response.Redirect( "Game.aspx?MinAccess=12000&MaxAccess=12600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_JackJack" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=16000&MaxAccess=16600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Rift" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=7000&MaxAccess=7600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Sword" ))
		{
			Response.Redirect( "Game.aspx?MinAccess=9000&MaxAccess=9600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_InfinityBlade1" ))
		{
			Response.Redirect( "Game.aspx?MinAccess=10000&MaxAccess=10600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_InfinityBlade2" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=30000&MaxAccess=30600" + UserDetailArg );
		}
		else if( Pressed.ID.StartsWith( "Button_Scaleform" ) )
		{
			Response.Redirect( "Game.aspx?MinAccess=20000&MaxAccess=20600" + UserDetailArg );
		}
		else 
        {
			foreach( string ButtonName in Buttons )
			{
				if( Pressed.ID.StartsWith( ButtonName ) )
				{
					string NewPage = ButtonName.Substring( "Button_".Length );
					Response.Redirect( NewPage + ".aspx" );
				}
			}
        }
	}

    protected void Repeater_BuildLog_ItemCommand( object source, RepeaterCommandEventArgs e )
    {
        if( e.Item != null )
        {
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				// Find the command id that matches the description
				LinkButton Pressed = ( LinkButton )e.CommandSource;
				string Build = Pressed.Text.Substring( "Stop ".Length );
				string CommandString = "SELECT [ID] FROM [Commands] WHERE ( Description = '" + Build + "' )";
				int CommandID = ReadInt( Connection, CommandString );

				if( CommandID != 0 )
				{
					string User = Context.User.Identity.Name;
					int Offset = User.LastIndexOf( '\\' );
					if( Offset >= 0 )
					{
						User = User.Substring( Offset + 1 );
					}

					CommandString = "UPDATE Commands SET Killing = 1, Killer = '" + User + "' WHERE ( ID = " + CommandID.ToString() + " )";
					Update( Connection, CommandString );
				}

				Connection.Close();
			}
        }
    }
    
    protected string DateDiff( object Start )
    {
        TimeSpan Taken = DateTime.UtcNow - ( DateTime )Start;

        string TimeTaken = "Time taken :" + Environment.NewLine;
        TimeTaken += Taken.Hours.ToString( "00" ) + ":" + Taken.Minutes.ToString( "00" ) + ":" + Taken.Seconds.ToString( "00" );

        return ( TimeTaken );
    }

    protected string DateDiff2( object Start )
    {
        TimeSpan Taken = DateTime.UtcNow - ( DateTime )Start;

        string TimeTaken = "( " + Taken.Hours.ToString( "00" ) + ":" + Taken.Minutes.ToString( "00" ) + ":" + Taken.Seconds.ToString( "00" ) + " )";

        return ( TimeTaken );
    }

    protected Color CheckConnected( object LastPing )
    {
        if( LastPing.GetType() == DateTime.UtcNow.GetType() )
        {
            TimeSpan Taken = DateTime.UtcNow - ( DateTime )LastPing;

            // Check for no ping in 900 seconds
            if( Taken.TotalSeconds > 900 )
            {
                return ( Color.Red );
            }
        }

        return ( Color.DarkGreen );
    }

    protected string GetAvailability( object Machine, object LastPing )
    {
        if( LastPing.GetType() == DateTime.UtcNow.GetType() )
        {
            TimeSpan Taken = DateTime.UtcNow - ( DateTime )LastPing;

            if( Taken.TotalSeconds > 300 )
            {
                return ( "Controller '" + ( string )Machine + "' is NOT responding!" );
            }
        }

        return ( "Controller '" + ( string )Machine + "' is available" );
    }

    protected void ScriptTimer_Tick( object sender, EventArgs e )
    {
		try
		{
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				DataSet StatusData = new DataSet();
				SqlDataAdapter StatusCommand = new SqlDataAdapter( "EXEC SelectBuildStatus", Connection );
				StatusCommand.Fill( StatusData, "ActiveBuilds" );
				Repeater_BuildLog.DataSource = StatusData;
				Repeater_BuildLog.DataBind();

				int BuilderCount = ReadIntSP( Connection, "GetActiveBuilderCount" );

				if( BuilderCount != ActiveBuilderCount )
				{
					DataSet MainBranchData = new DataSet();
					SqlDataAdapter MainBranchCommand = new SqlDataAdapter( "EXEC SelectBuilds 100,0", Connection );
					MainBranchCommand.Fill( MainBranchData, "MainBranch" );
					Repeater_MainBranch.DataSource = MainBranchData;
					Repeater_MainBranch.DataBind();

					DataSet BuildersData = new DataSet();
					SqlDataAdapter BuildersCommand = new SqlDataAdapter( "EXEC SelectActiveBuilders", Connection );
					BuildersCommand.Fill( BuildersData, "ActiveBuilders" );
					Repeater_Builders.DataSource = BuildersData;
					Repeater_Builders.DataBind();

					ActiveBuilderCount = BuilderCount;
				}

				Connection.Close();
			}
		}
		catch
		{
		}
    }

	protected void JobTimer_Tick( object sender, EventArgs e )
	{
		try
		{
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				DataSet JobsData = new DataSet();
				SqlDataAdapter JobsCommand = new SqlDataAdapter( "EXEC SelectJobStatus", Connection );
				JobsCommand.Fill( JobsData, "ActiveJobs" );
				Repeater_JobLog.DataSource = JobsData;
				Repeater_JobLog.DataBind();

				string Query = "SELECT COUNT(*) FROM [Jobs] WHERE ( Active = 0 AND Complete = 0 AND Optional = 0 AND PrimaryBuild = 0 )";
				int PendingTasks = ReadInt( Connection, Query );

				Label_PendingCISTasks.Text = "There are " + PendingTasks.ToString() + " pending CIS tasks";

				Connection.Close();
			}
		}
		catch
		{
		}
	}

    protected void VerifyTimer_Tick( object sender, EventArgs e )
	{
		try
		{
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				DataSet StatusData = new DataSet();
				SqlDataAdapter StatusCommand = new SqlDataAdapter( "EXEC SelectVerifyStatus", Connection );
				StatusCommand.Fill( StatusData, "ActiveBuilds" );
				Repeater_Verify.DataSource = StatusData;
				Repeater_Verify.DataBind();

				Connection.Close();
			}
		}
		catch
		{
		}
	}
}
