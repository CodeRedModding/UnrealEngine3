<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" Title="UnrealProp - Manage News" CodeFile="AddNews.aspx.cs" Inherits="Web_Admin_AddNews" ValidateRequest="false" %>

<asp:Content ID="AddNewsContent" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
    <ajaxToolkit:ToolkitScriptManager ID="AddNewsScriptManager" runat="server" />
    <br />
    <ajaxToolkit:HTMLEditor.Editor ID="NewsTextBox" runat="server" Width="95%" Rows="30" AutoFocus="true" OnPreRender="NewsTextBox_PreRender" />
    <br />
    <asp:Button ID="SaveNewsButton" runat="server" Text="Save News" Font-Size="Small" Width="15%" OnClick="SaveNewsButton_Click" />
</asp:Content>

