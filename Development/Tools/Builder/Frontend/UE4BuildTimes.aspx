<%@ Page Language="C#" AutoEventWireup="true" CodeFile="UE4BuildTimes.aspx.cs" Inherits="UE4BuildTimes" %>

<%@ Register assembly="System.Web.DataVisualization, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35" namespace="System.Web.UI.DataVisualization.Charting" tagprefix="asp" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head id="Head1" runat="server">
    <title></title>
</head>
<body>
    <center>
        <form id="form1" runat="server">
            <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Time of UE4 Compile vs. Changelist since branch creation" Width="640px"></asp:Label>
            <br />
    
            <div>
                <asp:Chart ID="UE4CompileChart" runat="server" Height="1000px" Width="1500px">
                    <Legends>
                        <asp:Legend Name="Legend1" Alignment="Center" Docking="Bottom">
                        </asp:Legend>
                    </Legends>
                    <series>
                        <asp:Series ChartArea="ChartArea1" ChartType="Line" Legend="Legend1" Name="UE4 ExampleGameA"></asp:Series>
                        <asp:Series ChartArea="ChartArea1" ChartType="Line" Legend="Legend1" Name="UE4 ExampleGame"></asp:Series>
                        <asp:Series ChartArea="ChartArea1" ChartType="Line" Legend="Legend1" Name="UE4 UDKGame"></asp:Series>
                        <asp:Series ChartArea="ChartArea1" ChartType="Line" Legend="Legend1" Name="UE4 FortniteGame"></asp:Series>
                    </series>
                    <chartareas>
                        <asp:ChartArea Name="ChartArea1"></asp:ChartArea>
                    </chartareas>
                </asp:Chart>
            </div>
        </form>
    </center>
</body>
</html>
