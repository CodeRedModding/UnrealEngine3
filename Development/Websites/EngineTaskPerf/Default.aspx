<%@ Page Language="C#" AutoEventWireup="true"  CodeFile="Default.aspx.cs" Inherits="_Default" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml">
<head runat="server">
    <title>engine task perf tracking</title>
    <style type="text/css">
        #form1
        {
        }
    </style>
</head>
<body>
    <form id="form1" runat="server">
    Filter by:
    <asp:DropDownList ID="TaskToSortBy" runat="server" 
        DataSourceID="SqlDataSource1" DataTextField="TaskDescription" DataValueField="TaskID" 
        onselectedindexchanged="TaskToSortBy_SelectedIndexChanged" 
        AutoPostBack="True">
    </asp:DropDownList>
    <br />
    <br />
    <asp:SqlDataSource ID="SqlDataSource1" runat="server" 
        ConnectionString="<%$ ConnectionStrings:EngineTaskPerfConnectionString %>" 
        
        SelectCommand="SELECT [TaskDescription], [TaskID] FROM [Tasks] ORDER BY [TaskDescription]">
    </asp:SqlDataSource>
    <asp:GridView ID="SQLGridView" runat="server"
        CellPadding="4" DataSourceID="SQLSourceForGridView" ForeColor="#333333" GridLines="None" 
        Height="16px" AllowPaging="True" AutoGenerateColumns="False" 
        Font-Size="Small" HorizontalAlign="Left" PageSize="50" Width="100%" 
        AllowSorting="True">
        <FooterStyle BackColor="#507CD1" Font-Bold="True" ForeColor="White" />
        <RowStyle BackColor="#EFF3FB" />
        <Columns>
            <asp:BoundField DataField="ID" HeaderText="ID" InsertVisible="False" 
                ReadOnly="True" SortExpression="ID">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="GameName" HeaderText="GameName" 
                SortExpression="GameName">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="MachineName" HeaderText="MachineName" 
                SortExpression="MachineName">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="TaskDescription" HeaderText="Task" 
                SortExpression="Task">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="TaskParameter" HeaderText="TaskInfo" 
                SortExpression="TaskInfo">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="Duration" DataFormatString="{0:0.0}" 
                HeaderText="Duration" SortExpression="Duration">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="Changelist" HeaderText="Changelist" 
                SortExpression="Changelist">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="ConfigName" HeaderText="Config" 
                SortExpression="ConfigName">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
            <asp:BoundField DataField="Date" HeaderText="Date" SortExpression="Date">
                <HeaderStyle HorizontalAlign="Right" />
                <ItemStyle HorizontalAlign="Right" />
            </asp:BoundField>
        </Columns>
        <PagerStyle BackColor="#2461BF" ForeColor="White" HorizontalAlign="Center" />
        <SelectedRowStyle BackColor="#D1DDF1" Font-Bold="True" ForeColor="#333333" />
        <HeaderStyle BackColor="#507CD1" Font-Bold="True" ForeColor="White" />
        <EditRowStyle BackColor="#2461BF" />
        <AlternatingRowStyle BackColor="White" />
    </asp:GridView>
    <asp:SqlDataSource ID="SQLSourceForGridView" runat="server" 
        ConnectionString="<%$ ConnectionStrings:EngineTaskPerfConnectionString %>" SelectCommand="SELECT	Runs.RunID AS ID, Date, ConfigName, GameName, UserName, MachineName, TaskDescription, TaskParameter, StatValue AS Duration, Changelist
FROM	RunData
JOIN	Runs			ON RunData.RunID = Runs.RunID
JOIN	Tasks			ON Runs.TaskID = Tasks.TaskID
JOIN	TaskParameters		ON Runs.TaskParameterID = TaskParameters.TaskParameterID
JOIN	Users			ON Runs.UserID = Users.UserID
JOIN	Machines		ON Runs.MachineID = Machines.MachineID
JOIN	Games			ON Runs.GameID = Games.GameID
JOIN	Configs			ON Runs.ConfigID = Configs.ConfigID
ORDER BY Date DESC
"></asp:SqlDataSource>
    </form>
</body>
</html>
