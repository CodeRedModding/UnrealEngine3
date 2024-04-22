<%@ Page Language="C#" AutoEventWireup="true" CodeFile="Default.aspx.cs" Inherits="_Default" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>Auto Report Viewer</title>
</head>
<body>
    <form id="form1" runat="server">
    <div>
        <asp:Label ID="Label1Filter1" runat="server" Height="22px" Text="Filter1" Width="41px"></asp:Label>&nbsp;<asp:DropDownList
            ID="FieldDropdownFilter1" runat="server" AutoPostBack="True" OnSelectedIndexChanged="FieldDropdown_SelectedIndexChanged">
            <asp:ListItem Selected="True">Status</asp:ListItem>
            <asp:ListItem>User</asp:ListItem>
            <asp:ListItem>Game</asp:ListItem>
            <asp:ListItem>EngineMode</asp:ListItem>
            <asp:ListItem>Changelist</asp:ListItem>
            <asp:ListItem>Callstack</asp:ListItem>
            <asp:ListItem>Time</asp:ListItem>
            <asp:ListItem>Platform</asp:ListItem>
        </asp:DropDownList>
        <asp:DropDownList ID="ValueDropdownFilter1" runat="server" AutoPostBack="True" OnSelectedIndexChanged="ValueDropdown_SelectedIndexChanged" Height="21px" Width="74px">
            <asp:ListItem Selected="True">All</asp:ListItem>
            <asp:ListItem>New</asp:ListItem>
            <asp:ListItem>Reviewed</asp:ListItem>
            <asp:ListItem>Coder</asp:ListItem>
            <asp:ListItem>Tester</asp:ListItem>
        </asp:DropDownList>
        &nbsp; &nbsp; &nbsp;&nbsp;<asp:TextBox
            ID="TextBox1Filter1" runat="server" Visible="False" Width="107px"></asp:TextBox>
        <asp:TextBox ID="TextBox2Filter1" runat="server" Visible="False" Width="104px"></asp:TextBox>&nbsp; <asp:Button ID="GoFilter1" runat="server" OnClick="Button1_Click" Text="Go" Visible="False" /> &nbsp; &nbsp; &nbsp; &nbsp;<asp:Label ID="Label2" runat="server" Height="22px" Text="Set Status" Width="66px"></asp:Label><asp:DropDownList
            ID="DropDownList2" runat="server" AutoPostBack="True" OnSelectedIndexChanged="DropDownList2_SelectedIndexChanged">
            <asp:ListItem Selected="True">Unset</asp:ListItem>
            <asp:ListItem>Reviewed</asp:ListItem>
            <asp:ListItem>New</asp:ListItem>
            <asp:ListItem>Coder</asp:ListItem>
            <asp:ListItem>Tester</asp:ListItem>
        </asp:DropDownList>
        &nbsp; &nbsp; &nbsp; &nbsp;<asp:Label ID="Label5" runat="server" Text="TTP" Height="21px" Width="27px"></asp:Label><asp:TextBox ID="TTPTextBox" runat="server" Height="15px" Width="75px"></asp:TextBox><asp:Button ID="SetTTP" runat="server" Text="Set" Height="22px" OnClick="SetTTP_Click" Width="30px" />&nbsp; &nbsp;&nbsp; 
        <asp:Label ID="Label6" runat="server" Text="FixedIn" Height="21px" Width="47px"></asp:Label><asp:TextBox ID="FixedInTextBox" runat="server" Height="15px" Width="73px"></asp:TextBox><asp:Button ID="SetFixedIn" runat="server" Text="Set" Height="22px" OnClick="SetFixedIn_Click" Width="30px" />&nbsp; &nbsp; &nbsp;<asp:Label ID="Label3" runat="server" Height="22px" Text="Select" Width="42px"></asp:Label><asp:DropDownList
            ID="DropDownList3" runat="server" AutoPostBack="True" OnSelectedIndexChanged="DropDownList3_SelectedIndexChanged" Height="19px">
            <asp:ListItem Selected="True">Unset</asp:ListItem>
            <asp:ListItem>None</asp:ListItem>
            <asp:ListItem>All</asp:ListItem>
            <asp:ListItem>Range</asp:ListItem>
        </asp:DropDownList>&nbsp;<br />
        <asp:Label ID="Label4Filter2" runat="server" Height="22px" Text="Filter2" Width="41px"></asp:Label>&nbsp;<asp:DropDownList
            ID="FieldDropdownFilter2" runat="server" AutoPostBack="True" OnSelectedIndexChanged="FieldDropdown_SelectedIndexChanged">
            <asp:ListItem Selected="True">Status</asp:ListItem>
            <asp:ListItem>User</asp:ListItem>
            <asp:ListItem>Game</asp:ListItem>
            <asp:ListItem>EngineMode</asp:ListItem>
            <asp:ListItem>Changelist</asp:ListItem>
            <asp:ListItem>Callstack</asp:ListItem>
            <asp:ListItem>Time</asp:ListItem>
            <asp:ListItem>Platform</asp:ListItem>
        </asp:DropDownList>&nbsp;<asp:DropDownList ID="ValueDropdownFilter2" runat="server" AutoPostBack="True" OnSelectedIndexChanged="ValueDropdown_SelectedIndexChanged" Height="21px" Width="74px">
            <asp:ListItem Selected="True">All</asp:ListItem>
            <asp:ListItem>New</asp:ListItem>
            <asp:ListItem>Reviewed</asp:ListItem>
            <asp:ListItem>Coder</asp:ListItem>
            <asp:ListItem>Tester</asp:ListItem>
        </asp:DropDownList>
        &nbsp; &nbsp; &nbsp;&nbsp;<asp:TextBox ID="TextBox1Filter2" runat="server" Visible="False"
                Width="107px"></asp:TextBox>
        <asp:TextBox ID="TextBox2Filter2" runat="server" Visible="False" Width="104px"></asp:TextBox>
        <asp:Button ID="GoFilter2" runat="server" OnClick="Button1_Click" Text="Go" Visible="False" />&nbsp;<br />
        &nbsp;&nbsp; &nbsp; &nbsp;
        <asp:Calendar ID="Calendar1" runat="server" BackColor="#FFFFCC" BorderColor="#FFCC66"
            BorderWidth="1px" DayNameFormat="Shortest" Font-Names="Verdana" Font-Size="6pt"
            ForeColor="#663399" Height="200px" OnSelectionChanged="Calendar1_SelectionChanged"
            ShowGridLines="True" Visible="False" Width="220px">
            <SelectedDayStyle BackColor="#CCCCFF" Font-Bold="True" />
            <TodayDayStyle BackColor="#FFCC66" ForeColor="White" />
            <SelectorStyle BackColor="#FFCC66" />
            <OtherMonthDayStyle ForeColor="#CC9966" />
            <NextPrevStyle Font-Size="6pt" ForeColor="#FFFFCC" />
            <DayHeaderStyle BackColor="#FFCC66" Font-Bold="True" Height="1px" />
            <TitleStyle BackColor="#990000" Font-Bold="True" Font-Size="6pt" ForeColor="#FFFFCC" />
        </asp:Calendar>
        &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;<asp:GridView ID="GridView1" runat="server" AllowPaging="True" AllowSorting="True"
            AutoGenerateColumns="False" CellPadding="4" DataSourceID="SqlDataSource1"
            ForeColor="#333333" GridLines="None" OnRowDataBound="GridView1_RowDataBound" PageSize="120" OnPageIndexChanged="GridView1_PageIndexChanged" OnSorted="GridView1_Sorted">
            <FooterStyle BackColor="#507CD1" Font-Bold="True" ForeColor="White" />
            <Columns>
                <asp:TemplateField> 
                    <ItemTemplate>
                        <asp:CheckBox ID="Selector" runat="server" />
                    </ItemTemplate>
                </asp:TemplateField>
                <asp:BoundField DataField="ID" HeaderText="ID" InsertVisible="False" ReadOnly="True"
                    SortExpression="ID" />
                <asp:BoundField DataField="Status" HeaderText="Status" SortExpression="Status" />
                <asp:BoundField DataField="UserName" HeaderText="UserName" SortExpression="UserName" />
                <asp:BoundField DataField="TimeOfCrash" HeaderText="TimeOfCrash" SortExpression="TimeOfCrash" />
                <asp:BoundField DataField="ChangelistVer" HeaderText="ChangelistVer" SortExpression="ChangelistVer" />
                <asp:BoundField DataField="GameName" HeaderText="GameName" SortExpression="GameName" />
                <asp:BoundField DataField="PlatformName" HeaderText="Platform" ReadOnly="True" SortExpression="PlatformName" />
                <asp:BoundField DataField="EngineMode" HeaderText="EngineMode" SortExpression="EngineMode" />
                <asp:BoundField DataField="Summary" HeaderText="Summary" SortExpression="Summary" />
                <asp:BoundField DataField="CrashDescription" HeaderText="CrashDescription" SortExpression="CrashDescription" />
                <asp:BoundField DataField="CallStack" HeaderText="CallStack" SortExpression="CallStack" />
                <asp:BoundField DataField="CommandLine" HeaderText="CommandLine" SortExpression="CommandLine" />
                <asp:BoundField DataField="ComputerName" HeaderText="Computer" SortExpression="ComputerName" />
                <asp:BoundField DataField="TTP" HeaderText="TTP" SortExpression="TTP" />

  
            </Columns>
            <RowStyle BackColor="#EFF3FB" />
            <EditRowStyle BackColor="#2461BF" />
            <SelectedRowStyle BackColor="#D1DDF1" Font-Bold="True" ForeColor="#333333" />
            <PagerStyle BackColor="#2461BF" ForeColor="White" HorizontalAlign="Center" />
            <HeaderStyle BackColor="#507CD1" Font-Bold="True" ForeColor="White" />
            <AlternatingRowStyle BackColor="White" />
            <PagerSettings Position="TopAndBottom" />
        </asp:GridView>
        &nbsp; &nbsp;<br />
        <asp:SqlDataSource ID="SqlDataSource1" runat="server" ConnectionString="<%$ ConnectionStrings:DatabaseConnection %>"
            SelectCommand="SELECT [ID], [UserName], [GameName], [TimeOfCrash], [ChangelistVer], [Summary], [CrashDescription], [CallStack], [Status], [TTP], [EngineMode], [CommandLine], [ComputerName], [Selected], [PlatformName] FROM [ReportData] ORDER BY [ID] DESC"></asp:SqlDataSource>
    </div>
    </form>
</body>
</html>
