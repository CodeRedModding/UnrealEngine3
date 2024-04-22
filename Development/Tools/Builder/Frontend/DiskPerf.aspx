<%@ Page Language="C#" AutoEventWireup="true" CodeFile="DiskPerf.aspx.cs" Inherits="DiskPerf" %>

<%@ Register assembly="System.Web.DataVisualization, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35" namespace="System.Web.UI.DataVisualization.Charting" tagprefix="asp" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head id="Head1" runat="server">
    <title></title>
</head>
<body>
<center>
    <form id="form1" runat="server">
    <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Relative Disk Performance" Width="640px"></asp:Label>
<br />
    <div>
    
        <asp:Chart ID="CommandCountChart" runat="server" Height="1000px" Width="1500px">
            <Legends>
                <asp:Legend Name="Legend1" Alignment="Center" Docking="Bottom">
                </asp:Legend>
            </Legends>
            <series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskIsilon1" 
                    Legend="Legend1" Color="Red" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskIsilon2" 
                    Legend="Legend1" Color="Red" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskIsilon3" 
                    Legend="Legend1" Color="Red" >
                </asp:Series>

                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskSAN1" 
                    Legend="Legend1" Color="Lime">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskSAN2" 
                    Legend="Legend1" Color="Lime">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskSAN3" 
                    Legend="Legend1" Color="Lime">
                </asp:Series>

                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskSATA1" 
                    Legend="Legend1" Color="Blue">
                </asp:Series>                
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskSATA2" 
                    Legend="Legend1" Color="Blue">
                </asp:Series>   
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="DiskSATA3" 
                    Legend="Legend1" Color="Blue">
                </asp:Series>     
            </series>
            <chartareas>
                <asp:ChartArea Name="ChartArea1">
                </asp:ChartArea>
            </chartareas>
        </asp:Chart>
    
    </div>
    </form>
    </center>
</body>
</html>

