<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" enableEventValidation="false" AutoEventWireup="true" CodeFile="ManageTasks.aspx.cs" Inherits="Web_Admin_ManageTasks" Title="UnrealProp - Manage Tasks" %>

<asp:Content ID="Content1" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
    <ajaxToolkit:ToolkitScriptManager ID="TasksAdminScriptManager" runat="server" />
    <br />
    <table style="width: 100%; text-align:center; font-size: small;" >
        <tr>
            <td class="header">Task Status:</td>                         
            <td class="header">Project / Build:</td>                 
            <td class="header">Platform / Target Machine:</td>       
            <td class="header">User:</td>                            
            <td class="header">UPDS:</td>
        </tr>
        <tr>
            <td valign="top" align="center" style="width: 18%; height: 21px;" >
                <asp:DropDownList ID="AdminTaskStatusDropDown" runat="server" Font-Size="Small" Width="85%"  AutoPostBack="True" />
            </td>
            <td  valign="top" align="center" style="width: 18%; height: 21px;" >
                <asp:DropDownList ID="AdminTaskProjectDropDown" runat="server" Font-Size="Small" Width="85%" AutoPostBack="True" />
            </td>
            <td  valign="top" align="center" style="width: 18%; height: 21px;" >
                <asp:DropDownList ID="AdminTaskPlatformDropDown" runat="server" Font-Size="Small" Width="85%" AutoPostBack="True" />
            </td>
            <td valign="top" align="center" style="width: 22%; height: 21px;" >
                 <asp:DropDownList ID="AdminTaskUserDropDown" runat="server" AutoPostBack="True" Width="85%" Font-Size="Small" DataSourceID="UserDataSource" DataTextField="Description" DataValueField="ID" OnDataBound="AdminTaskUserDropDown_DataBound" />
            </td>
            <td valign="top" align="center" style="width: 18%; height: 21px;" >
                <asp:UpdatePanel ID="AdminTaskUPDSUpdatePanel" runat="server" UpdateMode="Conditional">
                    <ContentTemplate>
                        <asp:DropDownList ID="AdminTaskUPDSDropDown" runat="server" Width="85%" Font-Size="Small" OnDataBinding="AdminTaskUPDSDropDown_DataBinding" AutoPostBack="True" />
                    </ContentTemplate>
                </asp:UpdatePanel>
            </td>
        </tr>
        <tr>
            <td align="center" valign="top">
            </td>
            <td align="center" valign="top">
                <asp:DropDownList ID="AdminTaskBuildDropDown" runat="server" Font-Size="Small" Width="95%" AutoPostBack="True" />
            </td>
            <td align="center" valign="top">
                <asp:DropDownList ID="AdminTaskClientMachineDropDown" runat="server" Width="95%" Font-Size="Small" AutoPostBack="True" />
            </td>
            <td align="center" valign="top">
            </td>
            <td align="center" valign="top">
            </td>
        </tr>
    </table>
            
    <ajaxToolkit:CascadingDropDown ID="ccdStatuses" runat="server" Category="Status"
            LoadingText="[Loading statuses...]" PromptText="All Statuses" ServiceMethod="GetTaskStatuses"
            ServicePath="~/Web/UPWebService.asmx" TargetControlID="AdminTaskStatusDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdClientMachines" runat="server"
            Category="ClientMachine" LoadingText="[Loding clients...]" ParentControlID="AdminTaskPlatformDropDown"
            PromptText="All Machines" ServiceMethod="GetClientMachinesForPlatform"
            ServicePath="~/Web/UPWebService.asmx" TargetControlID="AdminTaskClientMachineDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdPlatforms" runat="server"
            Category="Platform" LoadingText="[Loading platforms...]" PromptText="All Platforms"
            ServiceMethod="GetPlatforms" ServicePath="~/Web/UPWebService.asmx" TargetControlID="AdminTaskPlatformDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdBuilds" runat="server" Category="Title"
            LoadingText="[Loading builds...]" ParentControlID="AdminTaskProjectDropDown" PromptText="All Builds"
            ServiceMethod="GetBuilds" ServicePath="~/Web/UPWebService.asmx" TargetControlID="AdminTaskBuildDropDown">
    </ajaxToolkit:CascadingDropDown>
    <ajaxToolkit:CascadingDropDown ID="ccdProjects" runat="server" Category="Project"
            LoadingText="[Loading projects...]" PromptText="All Projects" ServiceMethod="GetProjects"
            ServicePath="~/Web/UPWebService.asmx" TargetControlID="AdminTaskProjectDropDown">
    </ajaxToolkit:CascadingDropDown>
            
    <asp:UpdatePanel ID="AdminTaskGridViewUpdatePanel" runat="server" UpdateMode="Conditional">
        <ContentTemplate>
            <center>
            <asp:GridView ID="AdminTasksGridView" runat="server" AllowSorting="True" AutoGenerateColumns="False" CellPadding="4" DataKeyNames="ID" DataSourceID="TaskDataSource" Font-Size="Small" PageSize="20"
            ForeColor="#333333" GridLines="None" Width="95%" AllowPaging="True" OnRowDataBound="AdminTasksGridViewTasks_RowDataBound" OnRowDeleting="AdminTasksGridView_RowDeleting" >
                <FooterStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <Columns>
                    <asp:CommandField ButtonType="Link" DeleteText="<img src='../../Images/bin.png' alt='Delete' border='0' />" ShowDeleteButton="true" />
                    <asp:TemplateField HeaderText="Status" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Status">
                        <ItemTemplate>
                            <asp:Label ID="AdminTaskStatusLabel" runat="server" Text='<%# Bind("Status") %>'>
                            </asp:Label>
                            <br />
                            <UnrealProp:ProgressBar ID="AdminTaskPropProgressBar" runat="Server" ForeColor="SaddleBrown" Height="2px" Width="70px" />
                        </ItemTemplate>
                    </asp:TemplateField>
                    <asp:BoundField DataField="ScheduleTime" HeaderText="Execution Time" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="ScheduleTime" DataFormatString="{0:dd-MM-yyyy HH:mm}" HtmlEncode="False" ReadOnly="True" >
                        <ItemStyle Wrap="False" />
                    </asp:BoundField>
                    <asp:BoundField DataField="Project" HeaderText="Project" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Project" ReadOnly="True" />
                    <asp:BoundField DataField="Title" HeaderText="Friendly Name" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Title" ReadOnly="True" />
                    <asp:BoundField DataField="TargetPlatform" HeaderText="Platform" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="TargetPlatform" ReadOnly="True" />
                    <asp:BoundField DataField="ClientMachineName" HeaderText="Target" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="ClientMachineName" ReadOnly="True" />
                    <asp:BoundField DataField="Email" HeaderText="Email" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Email" ReadOnly="True" />
                    <asp:BoundField DataField="AssignedUPDS" HeaderText="Assigned UPDS" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="AssignedUPDS" ReadOnly="True" />
                    <asp:BoundField DataField="Recurring" HeaderText="Daily?" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Recurring" />
                </Columns>
                <RowStyle BackColor="#FFFBD6" ForeColor="#333333" />
                <PagerStyle BackColor="#FFCC66" ForeColor="#333333" HorizontalAlign="Center" />
                <HeaderStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <AlternatingRowStyle BackColor="White" />
                <EmptyDataTemplate>
                    <strong>There are no tasks according to current filter.</strong>
                </EmptyDataTemplate>
            </asp:GridView>
            </center>
            <br />
            <asp:ObjectDataSource ID="TaskDataSource" runat="server" SelectMethod="Task_GetList" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}" />
            <asp:ObjectDataSource ID="TaskStatusesDataSource" runat="server" SelectMethod="TaskStatus_GetList" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}" />
           <asp:ObjectDataSource ID="UserDataSource" runat="server" SelectMethod="User_GetListFromTasks" TypeName="UnrealProp.Global" />
        </ContentTemplate>

        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="AdminTaskStatusDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTaskProjectDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTaskPlatformDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTaskUserDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTaskUPDSDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTaskBuildDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTaskClientMachineDropDown" EventName="SelectedIndexChanged" />
        </Triggers>
    </asp:UpdatePanel>
    
    <asp:Timer ID="TimerProgress" runat="server" Interval="5000" OnTick="TimerProgress_Tick">
    </asp:Timer>
    
    <asp:UpdatePanel ID="AdminTasksProgressUpdatePanel" runat="server" UpdateMode="Conditional">
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="TimerProgress" EventName="Tick" />
        </Triggers>
    </asp:UpdatePanel>
    <br />
</asp:Content>

