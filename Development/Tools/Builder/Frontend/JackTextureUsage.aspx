<%@ Page Language="C#" AutoEventWireup="true" CodeFile="JackTextureUsage.aspx.cs" Inherits="JackTextureUsage" %>

<%@ Register assembly="System.Web.DataVisualization, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35" namespace="System.Web.UI.DataVisualization.Charting" tagprefix="asp" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head id="Head1" runat="server">
    <title></title>
</head>
<body>
<center>
    <form id="form1" runat="server">
    <asp:Label ID="Label_Title" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" ForeColor="Blue" Height="48px" Text="Jack Texture Data (MB) (Weighted by Usage)" Width="720px"></asp:Label>
    <div>
<br />
    <asp:Button ID="Button_SaveAsCSV" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="X-Large" Text="Save as .CSV" OnClick="Button_SaveAsCSV_Click" />
<br />
    
        <asp:Chart ID="JackTextureUsageChart" runat="server" Width="1600px" Height="15000px" >
            <Legends>
                <asp:Legend Name="Legend1" Alignment="Center" Docking="Right">
                </asp:Legend>
            </Legends>
            <series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedBar" Name="Texture2D" Legend="Legend1" Color="Red" XValueType="String" ShadowOffset="2">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedBar" Name="LightMapTexture2D" Legend="Legend1" Color="Yellow" XValueType="String" ShadowOffset="2">
                </asp:Series>
                <asp:Series ChartArea="ChartArea1" ChartType="StackedBar" Name="ShadowMapTexture2D" Legend="Legend1" Color="Blue" XValueType="String" ShadowOffset="2">
                </asp:Series>                
            </series>
            <chartareas>
                <asp:ChartArea Name="ChartArea1">
                	<AxisX IntervalAutoMode="VariableCount" IsReversed="True">
						<MajorGrid Enabled="False" />
					</AxisX>
                </asp:ChartArea>
            </chartareas>
        </asp:Chart>
    
    </div>
    </form>
    </center>
</body>
</html>

