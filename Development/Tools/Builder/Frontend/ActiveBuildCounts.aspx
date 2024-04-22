<%@ Page Language="C#" AutoEventWireup="true" CodeFile="ActiveBuildCounts.aspx.cs" Inherits="ActiveBuildCounts" %>

<%@ Register assembly="System.Web.DataVisualization, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35" namespace="System.Web.UI.DataVisualization.Charting" tagprefix="asp" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml">
<head runat="server">
    <title></title>
</head>
<body>
<center>
    <form id="ActiveBuildCountsForm" runat="server">
    <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Number of Builds vs. Time of Day (Last 60 Days)" Width="640px"></asp:Label>
<br />
    <div>
    <asp:Chart ID="BuildCount" runat="server" Height="1000px" Width="1500px" 
            SuppressExceptions="False">
            <Legends>
                <asp:Legend Name="Legend1" Docking="Bottom" Alignment="Center">
                </asp:Legend>
            </Legends>
            <series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedArea" Legend="Legend1" 
                    Name="ActivePrimary" Color="Green" XValueType="Time">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedArea" Color="RoyalBlue" 
                    Legend="Legend1" Name="ActiveNonPrimary" XValueType="Time">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedArea" Color="Red" 
                    Legend="Legend1" Name="PendingPrimary" XValueType="Time">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedArea" Color="Gold" 
                    Legend="Legend1" Name="PendingNonPrimary" XValueType="Time">
                </asp:Series>
            </series>
            <chartareas>
                <asp:ChartArea Name="ChartArea1">
                    <AxisX Interval="1" IntervalType="Hours">
                    </AxisX>
                </asp:ChartArea>
            </chartareas>
        </asp:Chart>
    </div>
    
    Primary builds produce a data that is checked into Perforce or published to a server (e.g. precompiled shaders or cooks)<br />
    Non Primary builds are verification builds (e.g. CIS tasks or verification builds) <br />
    </form>
    </center>
</body>
</html>
