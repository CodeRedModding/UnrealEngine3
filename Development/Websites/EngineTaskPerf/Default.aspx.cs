using System;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.HtmlControls;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Xml.Linq;


public partial class _Default : System.Web.UI.Page 
{
    const string SQLQueryFormatString = "SELECT Runs.RunID AS ID, Date, ConfigName, GameName, UserName, MachineName, TaskDescription, TaskParameter, StatValue AS Duration, Changelist " +
                                        "FROM	RunData " +
                                        "JOIN	Runs			ON RunData.RunID = Runs.RunID " +
                                        "JOIN	Tasks			ON Runs.TaskID = Tasks.TaskID " +
                                        "JOIN	TaskParameters		ON Runs.TaskParameterID = TaskParameters.TaskParameterID " +
                                        "JOIN	Users			ON Runs.UserID = Users.UserID " +
                                        "JOIN	Machines		ON Runs.MachineID = Machines.MachineID " +
                                        "JOIN	Games			ON Runs.GameID = Games.GameID " +
                                        "JOIN	Configs			ON Runs.ConfigID = Configs.ConfigID " +
                                        "WHERE  Tasks.TaskID = {0} " +
                                        "ORDER BY Date DESC";
   
    protected void Page_Load(object sender, EventArgs e)
    {
        if( Page.IsPostBack )
        {
            SQLSourceForGridView.SelectCommand = String.Format(SQLQueryFormatString, TaskToSortBy.SelectedValue);
        }
    }

    protected void TaskToSortBy_SelectedIndexChanged(object sender, EventArgs e)
    {
        SQLSourceForGridView.SelectCommand = String.Format(SQLQueryFormatString, TaskToSortBy.SelectedValue);
        SQLGridView.DataBind();
    }
}
