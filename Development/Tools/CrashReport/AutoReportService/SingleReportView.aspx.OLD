<%@ Page Language="C#" AutoEventWireup="true" CodeFile="SingleReportView.aspx.cs" Inherits="SingleReportView" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>Single Report View</title>


</head>
<body>
    <form id="form1" runat="server">
    <div>
        <asp:DetailsView ID="DetailsView1" runat="server" AutoGenerateRows="False" CellPadding="4" DataSourceID="SqlDataSource10" ForeColor="#333333" GridLines="None"
            Height="50px" OnDataBound="DetailsView1_DataBound" Width="100%">
            <FooterStyle BackColor="#507CD1" Font-Bold="True" ForeColor="White" />
            <CommandRowStyle BackColor="#D1DDF1" Font-Bold="True" />
            <EditRowStyle BackColor="#2461BF" />
            <RowStyle BackColor="#EFF3FB" />
            <PagerStyle BackColor="#2461BF" ForeColor="White" HorizontalAlign="Center" />
            <Fields>
                <asp:BoundField DataField="ID" HeaderText="ID" InsertVisible="False" ReadOnly="True"
                    SortExpression="ID" />
                <asp:HyperLinkField DataTextField="ID" HeaderText="Saved Files" Text="Logs" />
                <asp:BoundField DataField="Status" HeaderText="Status" SortExpression="Status" />
                <asp:BoundField DataField="TTP" HeaderText="TTP" SortExpression="TTP" />
                <asp:BoundField DataField="ComputerName" HeaderText="ComputerName" SortExpression="ComputerName" />
                <asp:BoundField DataField="UserName" HeaderText="UserName" SortExpression="UserName" />
                <asp:BoundField DataField="GameName" HeaderText="GameName" SortExpression="GameName" />
                <asp:BoundField DataField="PlatformName" HeaderText="Platform" ReadOnly="True" SortExpression="PlatformName" />
                <asp:BoundField DataField="EngineMode" HeaderText="EngineMode" SortExpression="EngineMode" />
                <asp:BoundField DataField="LanguageExt" HeaderText="LanguageExt" SortExpression="LanguageExt" />
                <asp:BoundField DataField="TimeOfCrash" HeaderText="TimeOfCrash" SortExpression="TimeOfCrash" />
                <asp:BoundField DataField="BuildVer" HeaderText="BuildVer" SortExpression="BuildVer" />
                <asp:BoundField DataField="ChangelistVer" HeaderText="ChangelistVer" SortExpression="ChangelistVer" />
                <asp:BoundField DataField="CommandLine" HeaderText="CommandLine" SortExpression="CommandLine" />
                <asp:BoundField DataField="BaseDir" HeaderText="BaseDir" SortExpression="BaseDir" />
                <asp:BoundField DataField="Summary" HeaderText="Summary" SortExpression="Summary" />
                <asp:BoundField DataField="CrashDescription" HeaderText="CrashDescription" SortExpression="CrashDescription" />
                <asp:BoundField DataField="CallStack" HeaderText="CallStack" SortExpression="CallStack" />
            </Fields>
            <FieldHeaderStyle BackColor="#DEE8F5" Font-Bold="True" />
            <HeaderStyle BackColor="#507CD1" Font-Bold="True" ForeColor="White" />
            <AlternatingRowStyle BackColor="White" />
        </asp:DetailsView>
        <asp:SqlDataSource ID="SqlDataSource10" runat="server" ConnectionString="<%$ ConnectionStrings:DatabaseConnection %>"
            SelectCommand="SELECT * FROM [ReportData] WHERE ([ID] = @ID)">
            <SelectParameters>
                <asp:QueryStringParameter DefaultValue="67" Name="ID" QueryStringField="rowid" Type="Int32" />
            </SelectParameters>
        </asp:SqlDataSource>
        <asp:CheckBoxList ID="CheckBoxList1" runat="server" OnSelectedIndexChanged="CheckBoxList1_SelectedIndexChanged"
            RepeatDirection="Horizontal" AutoPostBack="True">
            <asp:ListItem Selected="True">Functions</asp:ListItem>
            <asp:ListItem Selected="True">FileNames &amp; Line Num</asp:ListItem>
            <asp:ListItem>FilePaths</asp:ListItem>
            <asp:ListItem>Unformatted</asp:ListItem>
        </asp:CheckBoxList></div>

    </form>
</body>
</html>
