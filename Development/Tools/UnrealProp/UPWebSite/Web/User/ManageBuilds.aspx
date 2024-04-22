<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" enableEventValidation="false" AutoEventWireup="true" CodeFile="ManageBuilds.aspx.cs" Inherits="Web_User_ManageBuilds" Title="UnrealProp - Manage Builds" %>

<asp:Content ID="Content1" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
    <ajaxToolkit:ToolkitScriptManager ID="BuildsScriptManager" runat="server" />
    <br />
    <center>
        <table style="width: 95%; font-size: small;" >
            <tr>
                <td align="center" class="header">Projects:</td>
                <td align="center" class="header">Platforms:</td>
                <td align="center" class="header">User:</td>
                <td align="center" class="header">Status:</td>
            </tr>
            <tr>
                <td valign="top" align="center" style="width: 20%; height: 21px;" >
                    <asp:DropDownList ID="ManageBuildsProjectDropDown" runat="server" AutoPostBack="True" Width="85%" Font-Size="Small" />
                    <ajaxToolkit:CascadingDropDown ID="ccdProjects" runat="server" Category="Project"
                        LoadingText="[Loading projects...]" PromptText="All Projects"
                        ServiceMethod="GetProjects" ServicePath="~/Web/UPWebService.asmx" TargetControlID="ManageBuildsProjectDropDown">
                    </ajaxToolkit:CascadingDropDown>
                </td>
                <td valign="top" align="center" style="width: 20%; height: 21px;" >
                    <asp:DropDownList ID="ManageBuildsPlatformDropDown" runat="server" AutoPostBack="True" Width="85%" Font-Size="Small" />
                    <ajaxToolkit:CascadingDropDown ID="ccdPlatforms" runat="server"
                        Category="Platform" LoadingText="[Loading platforms...]" PromptText="All Platforms"
                        ServiceMethod="GetPlatforms" ServicePath="~/Web/UPWebService.asmx" TargetControlID="ManageBuildsPlatformDropDown">
                    </ajaxToolkit:CascadingDropDown>
                </td>
                <td valign="top" align="center" style="width: 30%; height: 21px;" >
                    <asp:DropDownList ID="ManageBuildsUserDropDown" runat="server" AutoPostBack="True" Width="85%" Font-Size="Small" OnPreRender="ManageBuildsUserDropDown_PreRender" />
                </td>                   
                <td valign="top" align="center" style="width: 20%; height: 21px;" >
                    <asp:DropDownList ID="ManageBuildsStatusDropDown" runat="server" AutoPostBack="True" Width="85%" Font-Size="Small" DataSourceID="BuildStatusDataSource" DataTextField="Description" DataValueField="ID" OnDataBound="ManageBuildsStatusDropDown_DataBound" />
                </td>
            </tr>
        </table>
    </center>
    <br />
    <asp:UpdatePanel ID="UserManageBuildsUpdatePanel" runat="server" UpdateMode="Conditional">
        <ContentTemplate>
        <center>
            <asp:GridView ID="UserBuildsGridView" runat="server" DataSourceID="BuildDataSource" AutoGenerateColumns="false" CellPadding="4" DataKeyNames="ID" ForeColor="#333333" Width="95%" PageSize="20"
                GridLines="None" AllowPaging="True" AllowSorting="True" Font-Size="Small" OnRowDeleting="UserBuildsGridView_RowDeleting" OnRowUpdating="UserBuildsGridView_RowUpdating" OnRowDataBound="UserBuildsGridView_RowDataBound">
                <FooterStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <Columns>
                    <asp:CommandField ShowDeleteButton="True" ShowEditButton="True" ButtonType="Image" CancelImageUrl="~/Images/cancel.png" EditImageUrl="~/Images/table_edit.png" DeleteImageUrl="~/Images/bin.png" UpdateImageUrl="~/Images/accept.png" />
                    <asp:BoundField DataField="DiscoveryTime" HeaderText="Last Accessed" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="DiscoveryTime" DataFormatString="{0:dd-MM-yyyy HH:mm}" HtmlEncode="False" ReadOnly="True" />
                    <asp:BoundField DataField="Project" HeaderText="Project" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Project" ReadOnly="True" />
                    <asp:BoundField DataField="Title" HeaderText="Friendly Name" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Title" />
                    <asp:BoundField DataField="Email" HeaderText="Email" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Email" ReadOnly="True" />
                    <asp:BoundField DataField="Platform" HeaderText="Platform" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Platform" ReadOnly="True" />
                    <asp:TemplateField HeaderText="Status" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Status">
                        <EditItemTemplate>
                            <asp:DropDownList ID="DropDownListBuildStatus" runat="server" DataSourceID="BuildStatusDataSource" DataTextField="Description" DataValueField="ID" Font-Size="Small" SelectedValue='<%# Bind("StatusID") %>' />
                        </EditItemTemplate>
                        <ItemTemplate>
                            <asp:Label ID="Label1" runat="server" Text='<%# Bind("Status") %>'>
                            </asp:Label>
                            <br />
                            <UnrealProp:ProgressBar id="AnalysingProgressBar" width="70px" height="2px" runat="Server" ForeColor="SaddleBrown" />
                        </ItemTemplate>
                    </asp:TemplateField>
                    <asp:TemplateField HeaderText="Size" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Size">
                        <EditItemTemplate>
                            <asp:Label ID="Label1" runat="server" Text='<%# ((Int64)Eval("Size"))/(1024*1024) +" MB" %>'>
                            </asp:Label>
                        </EditItemTemplate>
                        <ItemTemplate>
                            <asp:Label ID="Label2" runat="server" Text='<%# ((Int64)Eval("Size"))/(1024*1024) +" MB" %>'>
                            </asp:Label>
                        </ItemTemplate>
                    </asp:TemplateField>
                </Columns>
                <RowStyle BackColor="#FFFBD6" ForeColor="#333333" />
                <SelectedRowStyle BackColor="#FFCC66" Font-Bold="True" ForeColor="Navy" />
                <PagerStyle BackColor="#FFCC66" ForeColor="#333333" HorizontalAlign="Center" />
                <HeaderStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <AlternatingRowStyle BackColor="White" />
                <EmptyDataTemplate>
                    <strong>There are no builds matching the current filter.</strong>
                </EmptyDataTemplate>                
            </asp:GridView>
            </center>
            <asp:ObjectDataSource ID="BuildDataSource" runat="server" SelectMethod="PlatformBuild_GetListForProjectPlatformUserAndStatus" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}" MaximumRowsParameterName="MaxRows">
                <SelectParameters>
                    <asp:ControlParameter ControlID="ManageBuildsProjectDropDown" DefaultValue="-1" Name="ProjectID" PropertyName="SelectedValue" Type="Int16" />
                    <asp:ControlParameter ControlID="ManageBuildsPlatformDropDown" DefaultValue="-1" Name="PlatformID" PropertyName="SelectedValue" Type="Int16" />
                    <asp:ControlParameter ControlID="ManageBuildsUserDropDown" DefaultValue="1" Name="UserNameID" PropertyName="SelectedValue" Type="Int32" />
                    <asp:ControlParameter ControlID="ManageBuildsStatusDropDown" DefaultValue="-1" Name="StatusID" PropertyName="SelectedValue" Type="Int16" />
                </SelectParameters>
            </asp:ObjectDataSource>
            <asp:ObjectDataSource ID="BuildStatusDataSource" runat="server" SelectMethod="PlatformBuildStatus_GetList" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}" />
        </ContentTemplate>
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="ManageBuildsProjectDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="ManageBuildsPlatformDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="ManageBuildsStatusDropDown" EventName="SelectedIndexChanged" />
        </Triggers>
    </asp:UpdatePanel>
    <br />
    <asp:UpdatePanel ID="ManageBuildsProgressUpdatePanel" runat="server">
        <ContentTemplate>
            <asp:Timer ID="TimerProgress" runat="server" Interval="4000" OnTick="TimerProgress_Tick">
            </asp:Timer>
        </ContentTemplate>
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="TimerProgress" EventName="Tick" />
        </Triggers>
    </asp:UpdatePanel>
    <br />
</asp:Content>

