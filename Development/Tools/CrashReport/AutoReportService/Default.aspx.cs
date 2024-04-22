/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Data;
using System.Configuration;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Data.SqlClient;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;
using CrashDataFormattingRoutines;

public partial class _Default : System.Web.UI.Page
{
    //have to update these when changing columns
    private const int RowIDIndex = 1;
    private const int UserIndex = 3;
    private const int ChangelistIndex = 8;
    private const int CallStackCellIndex = 11;

    protected void Page_Load(object sender, EventArgs e)
    {

    }
    protected void GridView1_RowDataBound(object sender, GridViewRowEventArgs e)
    {
        if (e.Row.RowType == DataControlRowType.DataRow)
        {
            int[] cellLimits = { 100, 100, 100, 100, 100, 100, 30, 200, 200, 200 };

            if (e.Row.Cells.Count > CallStackCellIndex)
            {
                TableCell CallStackCell = e.Row.Cells[CallStackCellIndex];

                CallStackContainer callStack = new CallStackContainer(CallStackCell.Text, 3, true, false);
                CallStackCell.Text = callStack.GetFormattedCallStack().Replace("\n", "<br>");
            }

            if (e.Row.Cells.Count > RowIDIndex)
            {
                string RowID = e.Row.Cells[RowIDIndex].Text;
                string SingleReportLink = "<a href=\"SingleReportView.aspx?rowid=" + RowID + "\">" + RowID + "</a>";
                e.Row.Cells[RowIDIndex].Text = SingleReportLink;
            }

            for (int i = 0; i < e.Row.Cells.Count && i < cellLimits.Length; i++)
            {
                TableCell currentCell = e.Row.Cells[i];
                if (currentCell.Text.Length > cellLimits[i])
                {
                    currentCell.Text = currentCell.Text.Substring(0, cellLimits[i]);
                    currentCell.Text += "...";
                }
            }
        }
    }

    protected void FieldDropdown_SelectedIndexChanged(object sender, EventArgs e)
    {
        int SelectedIndex = ((DropDownList)sender).SelectedIndex;
        string SelectedItemName = ((DropDownList)sender).SelectedItem.Text;

        int FilterSubstringBegin = ((DropDownList)sender).ID.IndexOf("Filter");
        string FilterSubstring = ((DropDownList)sender).ID.Substring(FilterSubstringBegin);
        TextBox TextBox1 = (TextBox)FindControl("TextBox1" + FilterSubstring);
        TextBox TextBox2 = (TextBox)FindControl("TextBox2" + FilterSubstring);
        DropDownList ValueDropdown = (DropDownList)FindControl("ValueDropdown" + FilterSubstring);
        
        if (SelectedItemName.Equals("Status"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("New", "New"));
            ValueDropdown.Items.Add(new ListItem("Reviewed", "Reviewed"));
            ValueDropdown.Items.Add(new ListItem("Coder", "Coder"));
            ValueDropdown.Items.Add(new ListItem("Tester", "Tester"));
        }
        else if (SelectedItemName.Equals("User"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("Coders", "Coders"));
            ValueDropdown.Items.Add(new ListItem("Automated", "Automated"));
            ValueDropdown.Items.Add(new ListItem("Testers", "Testers"));
            ValueDropdown.Items.Add(new ListItem("General", "General"));
            ValueDropdown.Items.Add(new ListItem("Specified", "Specified"));
            ValueDropdown.Items.Add(new ListItem("Selected", "Selected"));
        }
        else if (SelectedItemName.Equals("Game"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("Example", "Example"));
            ValueDropdown.Items.Add(new ListItem("UT", "UT"));
            ValueDropdown.Items.Add(new ListItem("War", "War"));
            ValueDropdown.Items.Add(new ListItem("Gear", "Gear"));
        }
        else if (SelectedItemName.Equals("EngineMode"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("Editor", "Editor"));
            ValueDropdown.Items.Add(new ListItem("Game", "Game"));
            ValueDropdown.Items.Add(new ListItem("Commandlet", "Commandlet"));
        }
        else if (SelectedItemName.Equals("Changelist"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("Range", "Range"));
            ValueDropdown.Items.Add(new ListItem("Selected", "Selected"));
        }
        else if (SelectedItemName.Equals("Callstack"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("Specified", "Specified"));
            //ValueDropdown.Items.Add(new ListItem("Selected", "Selected"));
        }
        else if (SelectedItemName.Equals("Time"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            if (FilterSubstring.Equals("Filter1"))
            {
                ValueDropdown.Items.Add(new ListItem("Range", "Range"));
            }
            ValueDropdown.Items.Add(new ListItem("Last 24hrs", "Last 24hrs"));
            ValueDropdown.Items.Add(new ListItem("Last 2days", "Last 2days"));
            ValueDropdown.Items.Add(new ListItem("Last 7days", "Last 7days"));
            ValueDropdown.Items.Add(new ListItem("Last Month", "Last Month"));
        }
        else if (SelectedItemName.Equals("Platform"))
        {
            ValueDropdown.Items.Clear();
            ValueDropdown.Items.Add(new ListItem("All", "All"));
            ValueDropdown.Items.Add(new ListItem("PC", "PC"));
            ValueDropdown.Items.Add(new ListItem("PS3", "PS3"));
            ValueDropdown.Items.Add(new ListItem("Xbox360", "Xbox360"));
        }

        ValueDropdown_SelectedIndexChanged(ValueDropdown, new EventArgs());
    }

    protected void ValueDropdown_SelectedIndexChanged(object sender, EventArgs e)
    {
        int FilterSubstringBegin = ((DropDownList)sender).ID.IndexOf("Filter");
        string FilterSubstring = ((DropDownList)sender).ID.Substring(FilterSubstringBegin);
        TextBox TextBox1 = (TextBox)FindControl("TextBox1" + FilterSubstring);
        TextBox TextBox2 = (TextBox)FindControl("TextBox2" + FilterSubstring);
        DropDownList FieldDropdown = (DropDownList)FindControl("FieldDropdown" + FilterSubstring);
        Button GoButton = (Button)FindControl("Go" + FilterSubstring);

        string SelectedColumnName = FieldDropdown.SelectedItem.Text;
        string SelectedCategoryName = ((DropDownList)sender).SelectedItem.Text;

        string SelectFragment = "SELECT [ID], T1.[UserName], T1.[GameName], T1.[PlatformName], T1.[TimeOfCrash], T1.[ChangelistVer], [Summary], [CrashDescription], [CallStack], T1.[Status], [TTP], T1.[EngineMode], [CommandLine], [ComputerName] FROM [ReportData] T1 ";
        string WhereFragment = "";
        string JoinFragment = "";
        string OrderFragment = "ORDER BY [ID] DESC";

        TextBox1.Visible = false;
        TextBox2.Visible = false;
        GoButton.Visible = false;
        Calendar1.Visible = false;

        if (SelectedColumnName.Equals("Status"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("New"))
            {
                WhereFragment = "T1.[Status] = 'New' ";
            }
            else if (SelectedCategoryName.Equals("Reviewed"))
            {
                WhereFragment = "T1.[Status] = 'Reviewed' ";
            }
            else if (SelectedCategoryName.Equals("Coder"))
            {
                WhereFragment = "T1.[Status] = 'Coder' ";
            }
            else if (SelectedCategoryName.Equals("Tester"))
            {
                WhereFragment = "T1.[Status] = 'Tester' ";
            }

        }
        else if (SelectedColumnName.Equals("User"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("Coders"))
            {
                JoinFragment = "INNER JOIN [CoderUsers] T2 ON T1.[UserName] = T2.[UserName] ";
            }
            else if (SelectedCategoryName.Equals("Automated"))
            {
                JoinFragment = "INNER JOIN [AutomatedProcesses] T2 ON T1.[UserName] = T2.[UserName] ";
            }
            else if (SelectedCategoryName.Equals("Testers"))
            {
                JoinFragment = "INNER JOIN [TesterUsers] T2 ON T1.[UserName] = T2.[UserName] ";
            }
            else if (SelectedCategoryName.Equals("General"))
            {
                //todo: need to exclude AutomatedProcesses and any future user groups
                //WhereFragment = "T1.[UserName] NOT IN (SELECT T2.[UserName] FROM [CoderUsers] T2) ";
                //for now, I've just created a "General" group
                JoinFragment = "INNER JOIN [GeneralUsers] T2 ON T1.[UserName] = T2.[UserName] ";
            }
            else if (SelectedCategoryName.Equals("Specified"))
            {
                TextBox1.Text = "";
                TextBox1.Visible = true;
                GoButton.Visible = true;
            }
            else if (SelectedCategoryName.Equals("Selected"))
            {
                GoButton.Visible = true;
            }
        }
        else if (SelectedColumnName.Equals("Game"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("Example"))
            {
                WhereFragment = "T1.[GameName] = 'Example' ";
            }
            else if (SelectedCategoryName.Equals("UT"))
            {
                WhereFragment = "T1.[GameName] = 'UT' ";
            }
            else if (SelectedCategoryName.Equals("War"))
            {
                WhereFragment = "T1.[GameName] = 'War' ";
            }
            else if (SelectedCategoryName.Equals("Gear"))
            {
                WhereFragment = "T1.[GameName] = 'Gear' ";
            }
        }
        else if (SelectedColumnName.Equals("EngineMode"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("Editor"))
            {
                WhereFragment = "T1.[EngineMode] = 'Editor' ";
            }
            else if (SelectedCategoryName.Equals("Game"))
            {
                WhereFragment = "T1.[EngineMode] = 'Game' ";
            }
            else if (SelectedCategoryName.Equals("Commandlet"))
            {
                WhereFragment = "T1.[EngineMode] = 'Commandlet' ";
            }
        }
        else if (SelectedColumnName.Equals("Changelist"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("Range"))
            {
                TextBox1.Text = "";
                TextBox2.Text = "";
                TextBox1.Visible = true;
                TextBox2.Visible = true;
                GoButton.Visible = true;
            }
            else if (SelectedCategoryName.Equals("Selected"))
            {
                GoButton.Visible = true;
            }
        }
        else if (SelectedColumnName.Equals("Callstack"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("Specified"))
            {
                TextBox1.Text = "";
                TextBox1.Visible = true;
                GoButton.Visible = true;
            }
            else if (SelectedCategoryName.Equals("Selected"))
            {
                GoButton.Visible = true;
            }
        }
        else if (SelectedColumnName.Equals("Time"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("Range"))
            {
                TextBox1.Text = "";
                TextBox2.Text = "";
                TextBox1.Visible = true;
                TextBox2.Visible = true;
                GoButton.Visible = true;
                Calendar1.Visible = true;
            }
            else if (SelectedCategoryName.Equals("Last 24hrs"))
            {
                WhereFragment = "T1.[TimeOfCrash] >= @TwentyFourHoursAgo ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["TwentyFourHoursAgo"]);
                SqlDataSource1.SelectParameters.Add("TwentyFourHoursAgo", TypeCode.DateTime, DateTime.Now.AddDays(-1).ToString());
            }
            else if (SelectedCategoryName.Equals("Last 2days"))
            {
                WhereFragment = "T1.[TimeOfCrash] >= @TwoDaysAgo ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["TwoDaysAgo"]);
                SqlDataSource1.SelectParameters.Add("TwoDaysAgo", TypeCode.DateTime, DateTime.Now.AddDays(-2).ToString());
            }
            else if (SelectedCategoryName.Equals("Last 7days"))
            {
                WhereFragment = "T1.[TimeOfCrash] >= @OneWeekAgo ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["OneWeekAgo"]);
                SqlDataSource1.SelectParameters.Add("OneWeekAgo", TypeCode.DateTime, DateTime.Now.AddDays(-7).ToString());
            }
            else if (SelectedCategoryName.Equals("Last Month"))
            {
                WhereFragment = "T1.[TimeOfCrash] >= @OneMonthAgo ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["OneMonthAgo"]);
                SqlDataSource1.SelectParameters.Add("OneMonthAgo", TypeCode.DateTime, DateTime.Now.AddMonths(-1).ToString());
            }
        }
        else if (SelectedColumnName.Equals("Platform"))
        {
            if (SelectedCategoryName.Equals("All"))
            {
                WhereFragment = "";
            }
            else if (SelectedCategoryName.Equals("PC"))
            {
                WhereFragment = "T1.[PlatformName] = 'PC' OR T1.[PlatformName] is null ";
            }
            else if (SelectedCategoryName.Equals("PS3"))
            {
                WhereFragment = "T1.[PlatformName] = 'PS3' ";
            }
            else if (SelectedCategoryName.Equals("Xbox360"))
            {
                WhereFragment = "T1.[PlatformName] = 'Xbox360' ";
            }
        }

        SetSelectFragment(SelectFragment);
        if (FilterSubstring.Equals("Filter1"))
        {
            SetFirstJoinFragment(JoinFragment);
            SetFirstWhereFragment(WhereFragment);
        }
        else
        {
            SetSecondJoinFragment(JoinFragment);
            SetSecondWhereFragment(WhereFragment);
        }
        SetOrderFragment(OrderFragment);
        SetSelectCommand();
        GridView1.DataBind();
    }

    protected void Button1_Click(object sender, EventArgs e)
    {
        int FilterSubstringBegin = ((Button)sender).ID.IndexOf("Filter");
        string FilterSubstring = ((Button)sender).ID.Substring(FilterSubstringBegin);
        TextBox TextBox1 = (TextBox)FindControl("TextBox1" + FilterSubstring);
        TextBox TextBox2 = (TextBox)FindControl("TextBox2" + FilterSubstring);
        DropDownList FieldDropdown = (DropDownList)FindControl("FieldDropdown" + FilterSubstring);
        DropDownList ValueDropdown = (DropDownList)FindControl("ValueDropdown" + FilterSubstring);
        Button GoButton = (Button)FindControl("Go" + FilterSubstring);

        string SelectedColumnName = FieldDropdown.SelectedItem.Text;
        string SelectedCategoryName = ValueDropdown.SelectedItem.Text;

        string SelectFragment = "SELECT [ID], T1.[UserName], T1.[GameName], T1.[PlatformName], T1.[TimeOfCrash], T1.[ChangelistVer], [Summary], [CrashDescription], [CallStack], T1.[Status], [TTP], T1.[EngineMode], [CommandLine], [ComputerName] FROM [ReportData] T1 ";
        string WhereFragment = "";
        string OrderFragment = "ORDER BY [ID] DESC";

        if (SelectedColumnName.Equals("User") && SelectedCategoryName.Equals("Specified"))
        {
            if (TextBox1.Text.Length > 0)
            {
                WhereFragment = "T1.[UserName] = @UserNameParam ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["UserNameParam"]);
                SqlDataSource1.SelectParameters.Add("UserNameParam", TypeCode.String, TextBox1.Text);
            }
        }
        else if (SelectedColumnName.Equals("Changelist") && SelectedCategoryName.Equals("Range"))
        {
            //TextBox1 is the smallest value, TextBox2 is the largest
            if (TextBox1.Text.Length > 0)
            {
                //if only the smallest value is specified, use that
                if (TextBox2.Text.Length == 0)
                {
                    WhereFragment = "T1.[ChangelistVer] = @ChangelistParam ";
                    SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["ChangelistParam"]);
                    SqlDataSource1.SelectParameters.Add("ChangelistParam", TypeCode.String, TextBox1.Text);
                }
                //use the range
                else
                {
                    WhereFragment = "T1.[ChangelistVer] >= @ChangelistMinParam AND T1.[ChangelistVer] <= @ChangelistMaxParam ";
                    SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["ChangelistMinParam"]);
                    SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["ChangelistMaxParam"]);
                    SqlDataSource1.SelectParameters.Add("ChangelistMinParam", TypeCode.String, TextBox1.Text);
                    SqlDataSource1.SelectParameters.Add("ChangelistMaxParam", TypeCode.String, TextBox2.Text);
                }
            }
        }
        else if (SelectedColumnName.Equals("Callstack") && SelectedCategoryName.Equals("Specified"))
        {
            if (TextBox1.Text.Length > 0)
            {
                WhereFragment = "T1.[CallStack] LIKE @CallStackSubstring ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["CallStackSubstring"]);
                SqlDataSource1.SelectParameters.Add("CallStackSubstring", TypeCode.String, "%" + TextBox1.Text + "%");
            }
        }
        else if (SelectedColumnName.Equals("Time") && SelectedCategoryName.Equals("Range"))
        {
            if (TextBox1.Text.Length > 0)
            {
                if (TextBox2.Text.Length == 0)
                {
                    WhereFragment = "T1.[TimeOfCrash] = @MatchTime ";
                    SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["MatchTime"]);
                    DateTime MatchTime = DateTime.Parse(TextBox1.Text);
                    SqlDataSource1.SelectParameters.Add("MatchTime", TypeCode.DateTime, MatchTime.ToString());
                }
                else
                {
                    WhereFragment = "T1.[TimeOfCrash] >= @MinTime AND T1.[TimeOfCrash] <= @MaxTime ";
                    SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["MinTime"]);
                    SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["MaxTime"]);
                    SqlDataSource1.SelectParameters.Add("MinTime", TypeCode.DateTime, TextBox1.Text);
                    SqlDataSource1.SelectParameters.Add("MaxTime", TypeCode.DateTime, TextBox2.Text);
                }
            }
        }
        else if (SelectedColumnName.Equals("User") && SelectedCategoryName.Equals("Selected"))
        {
            GridViewRow CheckedRow = GetFirstCheckedRow(GridView1);
            if (CheckedRow != null)
            {
                WhereFragment = "T1.[UserName] = @SelectedUser ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["SelectedUser"]);
                SqlDataSource1.SelectParameters.Add("SelectedUser", TypeCode.String, CheckedRow.Cells[UserIndex].Text);
            }
        }
        else if (SelectedColumnName.Equals("Changelist") && SelectedCategoryName.Equals("Selected"))
        {
            GridViewRow CheckedRow = GetFirstCheckedRow(GridView1);
            if (CheckedRow != null)
            {
                WhereFragment = "T1.[ChangelistVer] = @SelectedChangelist ";
                SqlDataSource1.SelectParameters.Remove(SqlDataSource1.SelectParameters["SelectedChangelist"]);
                SqlDataSource1.SelectParameters.Add("SelectedChangelist", TypeCode.String, CheckedRow.Cells[ChangelistIndex].Text);
            }
        }

        SetSelectFragment(SelectFragment);
        if (FilterSubstring.Equals("Filter1"))
        {
            SetFirstJoinFragment("");
            SetFirstWhereFragment(WhereFragment);
        }
        else
        {
            SetSecondJoinFragment("");
            SetSecondWhereFragment(WhereFragment);
        }
        SetOrderFragment(OrderFragment);
        SetSelectCommand();

        GridView1.DataBind();
    }

    protected void DropDownList2_SelectedIndexChanged(object sender, EventArgs e)
    {
        int SelectedIndex = ((DropDownList)sender).SelectedIndex;
        string NewStatus = "";

        if (SelectedIndex == 1)
        {
            NewStatus = "Reviewed";
        }
        else if (SelectedIndex == 2)
        {
            NewStatus = "New";
        }
        else if (SelectedIndex == 3)
        {
            NewStatus = "Coder";
        }
        else if (SelectedIndex == 4)
        {
            NewStatus = "Tester";
        }
        else
        {
            return;
        }

        string UpdateStatusCommand = "UPDATE ReportData SET Status = '" + NewStatus + "' WHERE ID IN (";

        Collection<string> SelectedIDs = GetSelectedRowIDs();

        bool FirstID = true;
        foreach (string RowID in SelectedIDs)
        {
            if (!FirstID)
            {
                UpdateStatusCommand += ",";
            }
            FirstID = false;
            UpdateStatusCommand += RowID;
        }

        if (SelectedIDs.Count > 0)
        {
            SqlDataSource1.UpdateCommand = UpdateStatusCommand + ")";
            SqlDataSource1.Update();
        }

        SetSelectCommand();
        ((DropDownList)sender).SelectedIndex = 0;
    }

    protected void DropDownList3_SelectedIndexChanged(object sender, EventArgs e)
    {
        int SelectedIndex = ((DropDownList)sender).SelectedIndex;

        if (SelectedIndex == 1)
        {
            for (int rowIndex = 0; rowIndex < GridView1.Rows.Count; rowIndex++)
            {
                GridViewRow currentRow = GridView1.Rows[rowIndex];
                CheckBox selectBox = (CheckBox)(currentRow.FindControl("Selector"));
                if (selectBox != null)
                {
                    selectBox.Checked = false;
                }
            }
        }
        else if (SelectedIndex == 2)
        {
            for (int rowIndex = 0; rowIndex < GridView1.Rows.Count; rowIndex++)
            {
                GridViewRow currentRow = GridView1.Rows[rowIndex];
                CheckBox selectBox = (CheckBox)(currentRow.FindControl("Selector"));
                if (selectBox != null)
                {
                    selectBox.Checked = true;
                }
            }
        }
        else if (SelectedIndex == 3)
        {
            bool bCurrentlyInSelectRange = false;
            for (int rowIndex = 0; rowIndex < GridView1.Rows.Count; rowIndex++)
            {
                GridViewRow currentRow = GridView1.Rows[rowIndex];
                CheckBox selectBox = (CheckBox)(currentRow.FindControl("Selector"));

                if (selectBox != null)
                {
                    if (selectBox.Checked)
                    {
                        bCurrentlyInSelectRange = !bCurrentlyInSelectRange;
                    }

                    if (bCurrentlyInSelectRange)
                    {
                        selectBox.Checked = true;
                    }
                }
            }
        }

        ((DropDownList)sender).SelectedIndex = 0;
        
    }

    private GridViewRow GetFirstCheckedRow(GridView SearchGridView)
    {
        for (int rowIndex = 0; rowIndex < SearchGridView.Rows.Count; rowIndex++)
        {
            GridViewRow currentRow = SearchGridView.Rows[rowIndex];
            CheckBox selectBox = (CheckBox)(currentRow.FindControl("Selector"));

            if (selectBox != null)
            {
                if (selectBox.Checked)
                {
                    return currentRow;
                }
            }
        }
        return null;
    }

    private void SetSelectCommand()
    {
        if (Session.Keys.Count > 0)
        {
            object StoredSelectFragment = Session.Contents["CrashReportSelectFragment"];
            object StoredFirstJoinFragment = Session.Contents["CrashReportFirstJoinFragment"];
            object StoredSecondJoinFragment = Session.Contents["CrashReportSecondJoinFragment"];
            object StoredFirstWhereFragment = Session.Contents["CrashReportFirstWhereFragment"];
            object StoredSecondWhereFragment = Session.Contents["CrashReportSecondWhereFragment"];
            object StoredOrderFragment = Session.Contents["CrashReportOrderFragment"];
            if (StoredSelectFragment != null && StoredOrderFragment != null)
            {
                string SelectCommand = StoredSelectFragment.ToString();

                if (StoredFirstJoinFragment != null && StoredFirstJoinFragment.ToString().Length > 0)
                {
                    SelectCommand += StoredFirstJoinFragment.ToString();
                }
                else if (StoredSecondJoinFragment != null && StoredSecondJoinFragment.ToString().Length > 0)
                {
                    SelectCommand += StoredSecondJoinFragment.ToString();
                }

                bool WhereInserted = false;
                if (StoredFirstWhereFragment != null && StoredFirstWhereFragment.ToString().Length > 0)
                {
                    WhereInserted = true;
                    SelectCommand += "WHERE " + StoredFirstWhereFragment.ToString();
                }

                if (StoredSecondWhereFragment != null && StoredSecondWhereFragment.ToString().Length > 0)
                {
                    if (WhereInserted)
                    {
                        SelectCommand += "AND " + StoredSecondWhereFragment.ToString();
                    }
                    else
                    {
                        SelectCommand += "WHERE " + StoredSecondWhereFragment.ToString();
                    }
                }

                SelectCommand += StoredOrderFragment.ToString();

                SqlDataSource1.SelectCommand = SelectCommand;
            }
        }
        
    }

    private void SetSelectFragment(string SelectFragment)
    {
        Session.Contents.Remove("CrashReportSelectFragment");
        Session.Add("CrashReportSelectFragment", SelectFragment);
    }

    private void SetFirstJoinFragment(string JoinFragment)
    {
        Session.Contents.Remove("CrashReportFirstJoinFragment");
        Session.Add("CrashReportFirstJoinFragment", JoinFragment);
    }

    private void SetSecondJoinFragment(string JoinFragment)
    {
        Session.Contents.Remove("CrashReportSecondJoinFragment");
        Session.Add("CrashReportSecondJoinFragment", JoinFragment);
    }

    private void SetFirstWhereFragment(string FirstWhereFragment)
    {
        Session.Contents.Remove("CrashReportFirstWhereFragment");
        Session.Add("CrashReportFirstWhereFragment", FirstWhereFragment);
    }

    private void SetSecondWhereFragment(string SecondWhereFragment)
    {
        Session.Contents.Remove("CrashReportSecondWhereFragment");
        Session.Add("CrashReportSecondWhereFragment", SecondWhereFragment);
    }

    private void SetOrderFragment(string OrderFragment)
    {
        Session.Contents.Remove("CrashReportOrderFragment");
        Session.Add("CrashReportOrderFragment", OrderFragment);
    }

    protected void GridView1_PageIndexChanged(object sender, EventArgs e)
    {
        SetSelectCommand();
    }

    protected void GridView1_Sorted(object sender, EventArgs e)
    {
        SetSelectCommand();
    }

    protected void Calendar1_SelectionChanged(object sender, EventArgs e)
    {
        if (TextBox1Filter1.Text.Length == 0)
        {
            TextBox1Filter1.Text = ((Calendar)sender).SelectedDate.ToShortDateString();
        }
        else if (TextBox2Filter1.Text.Length == 0)
        {
            TextBox2Filter1.Text = ((Calendar)sender).SelectedDate.ToShortDateString();
        }
        SetSelectCommand();
    }


    protected void SetTTP_Click(object sender, EventArgs e)
    {
        if (TTPTextBox.Text.Length > 0)
        {
            string UpdateTTPCommand = "UPDATE ReportData SET TTP = @NewTTP WHERE ID IN (";
            SqlDataSource1.UpdateParameters.Remove(SqlDataSource1.UpdateParameters["NewTTP"]);
            SqlDataSource1.UpdateParameters.Add("NewTTP", TypeCode.String, TTPTextBox.Text);

            Collection<string> SelectedIDs = GetSelectedRowIDs();

            bool FirstID = true;
            foreach (string RowID in SelectedIDs)
            {
                if (!FirstID)
                {
                    UpdateTTPCommand += ",";
                }
                FirstID = false;
                UpdateTTPCommand += RowID;
            }

            if (SelectedIDs.Count > 0)
            {
                SqlDataSource1.UpdateCommand = UpdateTTPCommand + ")";
                SqlDataSource1.Update();
            }

            SetSelectCommand();
        }
    }

    private Collection<string> GetSelectedRowIDs()
    {
        Collection<string> SelectedIDs = new Collection<string>();

        for (int rowIndex = 0; rowIndex < GridView1.Rows.Count; rowIndex++)
        {
            GridViewRow currentRow = GridView1.Rows[rowIndex];
            CheckBox selectBox = (CheckBox)(currentRow.FindControl("Selector"));
            if (selectBox != null && selectBox.Checked)
            {
                int RowIDIndex = 1;
                string RowIDURL = currentRow.Cells[RowIDIndex].Text;
                int RowIDBeginIndex = RowIDURL.IndexOf("\">");
                int RowIDEndIndex = RowIDURL.IndexOf("</a>");
                string RowID = RowIDURL.Substring(RowIDBeginIndex + 2, RowIDEndIndex - RowIDBeginIndex - 2);
                SelectedIDs.Add(RowID);
            }
        }
        return SelectedIDs;
    }

    protected void SetFixedIn_Click(object sender, EventArgs e)
    {
        if (FixedInTextBox.Text.Length > 0)
        {
            string UpdateFixedChangelistCommand = "UPDATE ReportData SET FixedChangelist = @NewFixedChangelist WHERE ID IN (";
            SqlDataSource1.UpdateParameters.Remove(SqlDataSource1.UpdateParameters["NewFixedChangelist"]);
            SqlDataSource1.UpdateParameters.Add("NewFixedChangelist", TypeCode.String, FixedInTextBox.Text);

            Collection<string> SelectedIDs = GetSelectedRowIDs();

            bool FirstID = true;
            foreach (string RowID in SelectedIDs)
            {
                if (!FirstID)
                {
                    UpdateFixedChangelistCommand += ",";
                }
                FirstID = false;
                UpdateFixedChangelistCommand += RowID;
            }

            if (SelectedIDs.Count > 0)
            {
                SqlDataSource1.UpdateCommand = UpdateFixedChangelistCommand + ")";
                SqlDataSource1.Update();
            }

            SetSelectCommand();
        }
    }
}
