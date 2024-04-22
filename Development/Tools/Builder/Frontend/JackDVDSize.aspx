<%@ Page Language="C#" AutoEventWireup="true" CodeFile="JackDVDSize.aspx.cs" Inherits="JackDVDSize" %>

<%@ Register assembly="System.Web.DataVisualization, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35" namespace="System.Web.UI.DataVisualization.Charting" tagprefix="asp" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head id="Head1" runat="server">
    <title></title>
</head>
<body>
<center>
    <form id="form1" runat="server">
    <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Jack DVD size (MB) vs. Date (Last 90 Days)" Width="720px"></asp:Label>
<br />
    <div>
    
        <asp:Chart ID="JackDVDSizeChart" runat="server" Height="1000px" Width="1500px" 
			Palette="Excel">
            <Legends>
                <asp:Legend Name="Legend1" Alignment="Center" Docking="Bottom">
                </asp:Legend>
            </Legends>
            <series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.FRA.ESM)[CZE.HUN.NOR.DAN.FIN]" Legend="Legend1" Color="Magenta" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.FRA.ESM)[CZE.HUN.NOR.DAN.FIN.DUT]" Legend="Legend1" Color="Magenta" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.FRA.ESM)[POL.CZE.HUN.KOR.CHN.NOR.DAN.FIN.DUT]" Legend="Legend1" Color="Magenta" >
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.JPN.PTB)" Legend="Legend1" Color="Blue" >
				</asp:Series>
				<asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.FRA.DEU)" Color="Blue" Legend="Legend1">
				</asp:Series>
				<asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.ESN.ITA)" Color="Blue" Legend="Legend1" >
				</asp:Series>
				<asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.ITA.ESN)" Color="Blue" Legend="Legend1" >
				</asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.POL.RUS)" Legend="Legend1" Color="DarkGreen" >
                </asp:Series>  
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Jack(INT.RUS)" Legend="Legend1" Color="DarkGreen" >
                </asp:Series>  
                <asp:Series ChartArea="ChartArea1" ChartType="Line" Name="Xbox360DVDCapacity" Legend="Legend1" Color="Red" >
                </asp:Series>             
             </series>
            <chartareas>
                <asp:ChartArea Name="ChartArea1">
                	<AxisY Maximum="10000" Minimum="6000">
					</AxisY>
                </asp:ChartArea>
            </chartareas>
        </asp:Chart>
    
    </div>
    </form>
    </center>
</body>
</html>

