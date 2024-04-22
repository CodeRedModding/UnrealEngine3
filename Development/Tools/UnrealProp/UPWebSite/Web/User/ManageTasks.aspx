<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" enableEventValidation="false" AutoEventWireup="true" CodeFile="ManageTasks.aspx.cs" Inherits="Web_User_ManageTasks" Title="UnrealProp - Manage Tasks" %>

<asp:Content ID="Content1" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
    <ajaxToolkit:ToolkitScriptManager ID="TasksUserScriptManager" runat="server" />
    <br />
    <center>
        <table style="width: 90%; font-size: small;" >
            <tr>
                <td align="center" class="header">Task Status:</td>
                <td align="center" class="header">Project / Build:</td>
                <td align="center" class="header">Platform / Target Machine:</td>
                <td align="center" class="header">UPDS:</td>
            </tr>
            <tr>
                <td valign="top" align="center" style="width: 20%; height: 21px;">
                    <asp:DropDownList ID="UserTaskStatusDropDown" runat="server" Font-Size="Small" Width="85%" AutoPostBack="True" />
                </td>
                <td valign="top" align="center" style="width: 20%; height: 21px;" >
                    <asp:DropDownList ID="UserTaskProjectDropDown" runat="server" Font-Size="Small" Width="85%" AutoPostBack="True" />
                </td>
                <td valign="top" align="center" style="width: 20%; height: 21px;" >
                    <asp:DropDownList ID="UserTaskPlatformDropDown" runat="server" Font-Size="Small" Width="85%" AutoPostBack="True" />
                </td>
                <td valign="top" align="center" style="width: 20%; height: 21px;">
                    <asp:UpdatePanel ID="UserTaskUPDSUpdatePanel" runat="server" UpdateMode="Conditional">
                        <ContentTemplate>
                            <asp:DropDownList ID="UserTaskUPDSDropDown" runat="server" Width="85%" Font-Size="Small" OnDataBinding="UserTaskUPDSDropDown_DataBinding" AutoPostBack="True" />
                        </ContentTemplate>
                    </asp:UpdatePanel>
                </td>
            </tr>
            <tr>
                <td style="width: 20%;" align="center" valign="top" >
                </td>
                <td style="width: 20%;" align="center" valign="top" >
                    <asp:DropDownList ID="UserTaskBuildDropDown" runat="server" Font-Size="Small" Width="95%" AutoPostBack="True" />
                </td>
                <td style="width: 20%;" align="center" valign="top" >
                    <asp:DropDownList ID="UserTaskClientMachineDropDown" runat="server" Width="95%" Font-Size="Small" AutoPostBack="True" />
                </td>
                <td style="width: 20%;" align="center" valign="top" >
                </td>
            </tr>
        </table>
    </center>            
    
    <ajaxToolkit:CascadingDropDown ID="ccdStatuses" runat="server" Category="Status"
            LoadingText="[Loading statuses...]" PromptText="All Statuses" ServiceMethod="GetTaskStatuses"
            ServicePath="~/Web/UPWebService.asmx" TargetControlID="UserTaskStatusDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdClientMachines" runat="server"
            Category="ClientMachine" LoadingText="[Loding clients...]" ParentControlID="UserTaskPlatformDropDown"
            PromptText="All Machines" ServiceMethod="GetClientMachinesForPlatform"
            ServicePath="~/Web/UPWebService.asmx" TargetControlID="UserTaskClientMachineDropDown">
        </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdPlatforms" runat="server"
            Category="Platform" LoadingText="[Loading platforms...]" PromptText="All Platforms"
                    ServiceMethod="GetPlatforms" ServicePath="~/Web/UPWebService.asmx" TargetControlID="UserTaskPlatformDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdBuilds" runat="server" Category="Title"
            LoadingText="[Loading builds...]" ParentControlID="UserTaskProjectDropDown" PromptText="All Builds"
            ServiceMethod="GetBuilds" ServicePath="~/Web/UPWebService.asmx" TargetControlID="UserTaskBuildDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdProjects" runat="server" Category="Project"
            LoadingText="[Loading projects...]" PromptText="All Projects" ServiceMethod="GetProjects"
            ServicePath="~/Web/UPWebService.asmx" TargetControlID="UserTaskProjectDropDown">
    </ajaxToolkit:CascadingDropDown>
            
    <asp:UpdatePanel ID="UserTaskGridViewUpdatePanel" runat="server" UpdateMode="Conditional">
        <ContentTemplate>
            <center>
            <asp:GridView ID="UserTasksGridView" runat="server" AllowSorting="True" AutoGenerateColumns="False" CellPadding="4" DataKeyNames="ID" DataSourceID="TaskDataSource" Font-Size="Small" PageSize="20"
            ForeColor="#333333" GridLines="None" Width="95%" AllowPaging="True" OnRowDataBound="UserTasksGridView_RowDataBound" OnRowDeleting="UserTasksGridView_RowDeleting" >
                <FooterStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <Columns>
                    <asp:CommandField ButtonType="Link" DeleteText="<img src='../../Images/bin.png' alt='Delete' border='0' />" ShowDeleteButton="true" />
                    <asp:TemplateField HeaderText="Status" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Status">
                        <ItemTemplate>
                            <asp:Label ID="UserTaskStatusLabel" runat="server" Text='<%# Bind("Status") %>' Width="71px">
                            </asp:Label>
                            <br />
                            <UnrealProp:ProgressBar ID="UserTaskPropProgressBar" runat="Server" ForeColor="SaddleBrown" Height="2px" Width="70px" />
                        </ItemTemplate>
                    </asp:TemplateField>
                    <asp:BoundField DataField="ScheduleTime" HeaderText="Execution Time" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="ScheduleTime" DataFormatString="{0:dd-MM-yyyy HH:mm}" HtmlEncode="False" >
                        <ItemStyle Wrap="False" />
                    </asp:BoundField>
                    <asp:BoundField DataField="Project" HeaderText="Project" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Project" />
                    <asp:BoundField DataField="Title" HeaderText="Friendly Name" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Title" />
                    <asp:BoundField DataField="TargetPlatform" HeaderText="Platform" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="TargetPlatform" />
                    <asp:BoundField DataField="ClientMachineName" HeaderText="Target" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="ClientMachineName" />
                    <asp:BoundField DataField="Email" HeaderText="Email" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Email"/>
                    <asp:BoundField DataField="AssignedUPDS" HeaderText="Assigned UPDS" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="AssignedUPDS" />
                    <asp:BoundField DataField="Recurring" HeaderText="Daily?" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Recurring" />
                </Columns>
                <RowStyle BackColor="#FFFBD6" ForeColor="#333333" />
                <PagerStyle BackColor="#FFCC66" ForeColor="#333333" HorizontalAlign="Center" />
                <HeaderStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <AlternatingRowStyle BackColor="White" />
                <EmptyDataTemplate>
                    <strong>There are no tasks matching the current filter.</strong>
                </EmptyDataTemplate>
            </asp:GridView>
            </center>
            <br />

            <asp:ObjectDataSource ID="TaskDataSource" runat="server" SelectMethod="Task_GetList" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}" />
        </ContentTemplate>
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="UserTaskStatusDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="UserTaskProjectDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="UserTaskPlatformDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="UserTaskUPDSDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="UserTaskBuildDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="UserTaskClientMachineDropDown" EventName="SelectedIndexChanged" />
        </Triggers>
    </asp:UpdatePanel>
    
    <asp:Timer ID="TimerProgress" runat="server" Interval="5000" OnTick="TimerProgress_Tick">
    </asp:Timer>
    
    <asp:UpdatePanel ID="UserTaskProgressUpdatePanel" runat="server" UpdateMode="Conditional">
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="TimerProgress" EventName="Tick" />
        </Triggers>
    </asp:UpdatePanel>
    <br />
</asp:Content>

