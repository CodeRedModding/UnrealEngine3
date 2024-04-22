using System;
using System.Collections.Generic;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class BuildCharts : BasePage
{
	protected void Page_Load( object sender, EventArgs e )
	{
		string LoggedOnUser = Context.User.Identity.Name;
		string MachineName = Context.Request.UserHostName;
		
		Label_Welcome.Text = "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"";
	}

	protected void Button_Trigger_Click( object sender, EventArgs e )
	{
		Button Pressed = ( Button )sender;
		if( Pressed.ID == "Button_BuildStats" )
		{
			SimpleTriggerBuild( "Stats" );
			Response.Redirect( "Default.aspx" );
		}
	}

	protected void Button_PickChart_Click( object sender, EventArgs e )
	{
		Button Pressed = ( Button )sender;
		if( Pressed.ID == "Button_RiftBuildCharts" )
		{
			Response.Redirect( "RiftBuildCharts.aspx" );
		}
		else if( Pressed.ID == "Button_CISBuildTimes" )
		{
			Response.Redirect( "CISBuildTimes.aspx" );
		}
		else if( Pressed.ID == "Button_CISLatency" )
		{
			Response.Redirect( "CISLatency.aspx" );
		}
		else if( Pressed.ID == "Button_SoakBuildTimes" )
		{
			Response.Redirect( "SoakBuildTimes.aspx" );
		}
		else if( Pressed.ID == "Button_ScriptBuildTimes" )
		{
			Response.Redirect( "ScriptBuildTimes.aspx" );
		}
		else if( Pressed.ID == "Button_CISDownTime" )
		{
			Response.Redirect( "CISDownTime.aspx" );
		}
        else if (Pressed.ID == "Button_UE4BuildTimes")
        {
            Response.Redirect("UE4BuildTimes.aspx");
        }
        else if (Pressed.ID == "Button_CrashReporter")
        {
            Response.Redirect("CrashReporter.aspx");
        }
        else if (Pressed.ID == "Button_ActiveBuilds")
        {
            // Can't do this one atm as this was the one function of the build monitor that was actually used
            Response.Redirect("ActiveBuildCounts.aspx");
        }
        else if (Pressed.ID == "Button_BuildCounts")
        {
            Response.Redirect("TotalJobCounts.aspx");
        }
        else if (Pressed.ID == "Button_DesktopPerf")
        {
            Response.Redirect("DesktopPerf.aspx");
        }
	}
}
