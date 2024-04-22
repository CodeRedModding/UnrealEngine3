<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" CodeFile="UserActions.aspx.cs" Inherits="Web_User_UserActions" Title="UnrealProp - User Actions" %>
<asp:Content ID="Content1" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
<center>
    <asp:Label ID="ProppedAmountLabel" runat="server" Font-Size="X-Large" ForeColor="DarkGreen" >
    </asp:Label>
    <br />
    <br />
    <asp:Label ID="ProppedAmountLabelGearXenon" runat="server" Font-Size="Large" ForeColor="DarkGreen" >
    </asp:Label>
    <br />
    <asp:Label ID="ProppedAmountLabelGearPC" runat="server" Font-Size="Large" ForeColor="DarkGreen" >
    </asp:Label>
    <br />
</center>
This is the main page for user actions.<br />
<br />
Click on "Task Scheduler" to select a build to propagate.<br />
Click on "My Tasks" to view the status of your current and past propagations.<br />
Click on "My Targets" to view or add your target machines.<br />
Click on "My Builds" to view or edit builds you have uploaded from UnrealFrontend.<br />
<br />


</asp:Content>

