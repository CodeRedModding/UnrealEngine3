<%@ Page Language="C#" AutoEventWireup="true" CodeFile="Tools.aspx.cs" Inherits="Tools" Debug="true" %>

<%@ Register assembly="System.Web.DataVisualization, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35" namespace="System.Web.UI.DataVisualization.Charting" tagprefix="asp" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>Trigger a Build</title>
</head>
<body>
	<center>
		<form id="form1" runat="server">
			<div>
				<asp:Label ID="Label1" runat="server" Font-Bold="True" Font-Names="Arial" Font-Size="XX-Large"
				Height="56px" Text="Epic Build System - Trigger Build" Width="640px" ForeColor="Blue"></asp:Label><br /><br />
				<asp:Label ID="Label_Welcome" runat="server" Height="32px" Width="384px" Font-Bold="True" Font-Names="Arial" Font-Size="Small" ForeColor="Blue"></asp:Label><br />
				<br />
				<asp:SqlDataSource ID="BuilderDBSource_Trigger" runat="server" ConnectionString="<%$ ConnectionStrings:BuilderConnectionString %>"
				SelectCommandType="StoredProcedure" SelectCommand="GetTriggerable">
				<SelectParameters>
				<asp:QueryStringParameter Name="MinAccess" Type="Int32" DefaultValue="18000" />
				<asp:QueryStringParameter Name="MaxAccess" Type="Int32" DefaultValue="18600" />
				</SelectParameters>
				</asp:SqlDataSource>
				<asp:Repeater ID="BuilderDBRepeater_Trigger" runat="server" DataSourceID="BuilderDBSource_Trigger" OnItemCommand="BuilderDBRepeater_ItemCommand">
				<ItemTemplate>
				<asp:Button ID="Button1" runat="server" Font-Bold="True" Height="26px" OnPreRender="BuilderDBRepeater_OnPreRender" Width="384px"
											ForeColor=<%# System.Drawing.Color.FromName( ( string )DataBinder.Eval(Container.DataItem, "[\"Hint\"]") ) %>
											Text=<%# DataBinder.Eval(Container.DataItem, "[\"Description\"]") %> 
											CommandName=<%# DataBinder.Eval(Container.DataItem, "[\"ID\"]") %> 
											CommandArgument=<%# DataBinder.Eval(Container.DataItem, "[\"Machine\"]") %>  />				<br /><br />
				</ItemTemplate>
				</asp:Repeater>
			</div>
		</form>
	</center>    
</body>
</html>

