<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" enableEventValidation="false" AutoEventWireup="true" CodeFile="TaskScheduler.aspx.cs" Inherits="Web_User_TaskScheduler" Title="UnrealProp - Task Scheduler" %>

<asp:Content ID="Content1" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
    <strong>
    <span style="font-size: 8pt;">
        <ajaxToolkit:ToolkitScriptManager ID="TaskSchedulerScriptManager" runat="server" />
        <br />
        <asp:Panel ID="Panel_BuildHeader" runat="server" CssClass="collapsePanelHeader">               
            <div style="padding: 5px; cursor: pointer; vertical-align: middle;">
                <div style="float: left;"> 
                    1. Select Build:
                </div>
                <div style="float: right; vertical-align: middle;">
                    <asp:ImageButton ID="ImageBuild" runat="server" ImageUrl="~/images/expand.jpg" AlternateText="(Show Details...)" />
                </div>
                <div style="float: right;">
                    <span style="color: limegreen;">[Required] &nbsp;&nbsp;
                    </span>
                </div>
             </div>
        </asp:Panel>
                
        <asp:Panel ID="Panel_Builds" runat="server" Height="260px" Width="100%" BackColor="#f6f1dd">
            <table width="100%" cellspacing="10">
                <tr>
                    <td style="background-color: inherit; width: 35%;" valign="top" align="left">
                        Projects:
                        <br />
                        <asp:DropDownList ID="TaskSchedulerProjectDropDown" runat="server" Width="85%" AutoPostBack="True" Font-Size="Small" />
                        <br />
                        <ajaxToolkit:CascadingDropDown ID="ccdProjects" runat="server" Category="Project"
                            LoadingText="[Loading projects...]" PromptText="Please select a project" ServiceMethod="GetProjects"
                            ServicePath="~/Web/UPWebService.asmx" TargetControlID="TaskSchedulerProjectDropDown">
                        </ajaxToolkit:CascadingDropDown>
                        <br />
                        Available platforms:
                        <br />
                        <asp:DropDownList ID="TaskSchedulerPlatformDropDown" runat="server" Width="85%" AutoPostBack="True" Font-Size="Small" />
                        <br />
                        <ajaxToolkit:CascadingDropDown ID="ccdPlatforms" runat="server" Category="Platform"
                            LoadingText="[Loading platforms...]" ParentControlID="TaskSchedulerProjectDropDown" PromptText="Please select a platform"
                            ServiceMethod="GetPlatformsForProject" ServicePath="~/Web/UPWebService.asmx" TargetControlID="TaskSchedulerPlatformDropDown">
                        </ajaxToolkit:CascadingDropDown>
                    </td>
                    <td style="background-color: inherit;" valign="top" align="left">
                        Builds:
                        <br />
                        <asp:UpdatePanel ID="BuildListUpdatePanel" runat="server" UpdateMode="Conditional">
                            <ContentTemplate>
                                <asp:ListBox ID="BuildsListBox" runat="server" Font-Size="Small" Height="300px" 
									Width="90%" OnPreRender="BuildsListBox_PreRender" AutoPostBack="True" 
									DataSourceID="BuildDataSource" DataTextField="Description" DataValueField="ID" >
                                </asp:ListBox>
                            </ContentTemplate>
                            <Triggers>
                                <asp:AsyncPostBackTrigger ControlID="TaskSchedulerProjectDropDown" EventName="SelectedIndexChanged" />
                                <asp:AsyncPostBackTrigger ControlID="TaskSchedulerPlatformDropDown" EventName="SelectedIndexChanged" />
                            </Triggers>
                        </asp:UpdatePanel>
                        <asp:ObjectDataSource ID="BuildDataSource" runat="server" SelectMethod="PlatformBuild_GetSimpleListForProjectAndPlatform" TypeName="UnrealProp.Global">
                            <SelectParameters>
                                <asp:ControlParameter ControlID="TaskSchedulerProjectDropDown" Name="ProjectID" PropertyName="SelectedValue" Type="Int16" />
                                <asp:ControlParameter ControlID="TaskSchedulerPlatformDropDown" Name="PlatformID" PropertyName="SelectedValue" Type="Int16" />
                            </SelectParameters>
                        </asp:ObjectDataSource>
                    </td>
                </tr>
            </table>
        </asp:Panel>
        
        <br />
        
        <asp:Panel ID="Panel_TargetsHeader" runat="server" CssClass="collapsePanelHeader">
            <div style="padding: 5px; cursor: pointer; vertical-align: middle;">
                <div style="float: left;">
                    2. Select Target Machines:
                </div>
                <div style="float: right; vertical-align: middle;">
                    <asp:ImageButton ID="ImageTargets" runat="server" ImageUrl="~/images/expand.jpg" AlternateText="(Show Details...)" />
                </div>
                <div style="float: right;">
                    <span style="color: limegreen;">[Required] &nbsp;&nbsp;
                    </span>
                </div>
            </div>
        </asp:Panel>
        
        <asp:Panel ID="Panel_Targets" runat="server" Height="200px" Width="100%" BackColor="#f6f1dd" HorizontalAlign="Left" ScrollBars="Vertical">
            <asp:UpdatePanel ID="ClientGroupUpdatePanel" runat="server" UpdateMode="Conditional">
                <ContentTemplate>
                    <table width="100%" cellspacing="10">
                        <tr>
                            <td style="background-color: inherit; width: 35%;" valign="top" align="left">
                                Groups:
                                <br />
                                <asp:Button ID="GroupButtonNone" runat="server" Font-Size="Small" Text="None" Width="15%" OnClick="GroupButtonNone_Click" />
                                <asp:Button ID="GroupButtonAll" runat="server" Font-Size="Small" Text="All" Width="15%" OnClick="GroupButtonAll_Click" />
                                <br />
                                <br />
                                <asp:CheckBoxList ID="GroupCheckBoxes" runat="server" AutoPostBack="True" Font-Size="Small" BackColor="#f6f1dd"
                                        OnSelectedIndexChanged="GroupCheckBoxes_SelectedIndexChanged" Width="30%">
                                </asp:CheckBoxList>
                            </td>
                            <td style="background-color: inherit; width: 65%;" valign="top" align="left">
                                Consoles:
                                <br />
                                <br />
                                <asp:DataList ID="ClientMachineList" runat="server" DataKeyField="ID" DataSourceID="TargetMachineDataSource" RepeatColumns="3" >
                                    <ItemTemplate>
                                        <asp:CheckBox ID="TargetCheckBox" runat="server" Font-Size="Small" BackColor="#f6f1dd"
                                            Text='<%# Eval("Name") %>' ToolTip='<%# Eval("Path") %>' />
                                        <asp:HiddenField ID="HiddenID" runat="server" Value='<%# Eval("ID") %>' />
                                    </ItemTemplate>
                                </asp:DataList>
                            </td>
                        </tr>
                        <tr>
                            <td>
                            </td>
                            <td style="background-color: inherit; width: 65%;" valign="top" align="left">
                                PCs:
                                <br />
                                <br />
                                <asp:DataList ID="PCMachineList" runat="server" DataKeyField="ID" DataSourceID="PCMachineDataSource" RepeatColumns="3" >
                                    <ItemTemplate>
                                        <asp:CheckBox ID="PCCheckBox" runat="server" Font-Size="Small" BackColor="#f6f1dd"
                                            Text='<%# Eval("Name") %>' ToolTip='<%# Eval("Path") %>' />
                                        <asp:HiddenField ID="HiddenID" runat="server" Value='<%# Eval("ID") %>' />
                                    </ItemTemplate>
                                </asp:DataList>
                            </td>                        
                        </tr>
                    </table>
                </ContentTemplate>
                <Triggers>
                    <asp:AsyncPostBackTrigger ControlID="TaskSchedulerPlatformDropDown" EventName="SelectedIndexChanged" />
                    <asp:AsyncPostBackTrigger ControlID="TaskSchedulerProjectDropDown" EventName="SelectedIndexChanged" />
                </Triggers>
            </asp:UpdatePanel>
            
            <br />
            
            <asp:ObjectDataSource ID="TargetMachineDataSource" runat="server" SelectMethod="ClientMachine_GetListForPlatform" TypeName="UnrealProp.Global" >
                <SelectParameters>
                    <asp:ControlParameter ControlID="TaskSchedulerPlatformDropDown" Name="PlatformID" PropertyName="SelectedValue" Type="Int16" />
                </SelectParameters>
            </asp:ObjectDataSource>
            
            <asp:ObjectDataSource ID="PCMachineDataSource" runat="server" SelectMethod="ClientMachine_GetListForPlatform" TypeName="UnrealProp.Global">
                <SelectParameters>
                    <asp:Parameter Name="PlatformID" DefaultValue="1" Type="Int16" />
                </SelectParameters>
            </asp:ObjectDataSource>            
        </asp:Panel>
        
        <br />
        
        <asp:Panel ID="Panel_TimeHeader" runat="server" CssClass="collapsePanelHeader">
            <div style="padding: 5px; cursor: pointer; vertical-align: middle;">
                <div style="float: left;">
                    3. Set Execution Time:
                </div>
                <div style="float: right; vertical-align: middle;">
                    <asp:ImageButton ID="ImageTime" runat="server" ImageUrl="~/images/expand.jpg" AlternateText="(Show Details...)" />
                </div><div style="float: right;">
                    <span style="color: yellow;">[Optional] &nbsp;&nbsp; </span>
                </div>
            </div>
        </asp:Panel>
        
        <asp:Panel ID="Panel_Time" runat="server" Height="120px" Width="100%" BackColor="#f6f1dd">
        <asp:UpdatePanel ID="UpdatePanelTime" runat="server" >
            <ContentTemplate>
                <table width="35%" cellpadding="0" cellspacing="10">
                    <tr>
                        <td style="background-color: inherit; width: 100%;" valign="top">
                            <asp:CheckBox ID="CheckBox_RecurringTask" runat="server" />
                            Run this task every day?
                            <br />
                            <br /> 
                        </td>
                    </tr>
                    <tr>
                        <td style="background-color: inherit; width: 100%;" valign="top">
                            Task Time (24 hour clock):
                            <br />
                            <asp:TextBox ID="ScheduleTime" runat="server" Height="16px" ValidationGroup="MKE" Width="85%">
                            </asp:TextBox>
                            
                            <ajaxToolkit:MaskedEditExtender ID="MaskedEditExtender3" runat="server" AcceptAMPM="False"
                                Mask="99:99:99" MaskType="Time" MessageValidatorTip="true"
                                OnFocusCssClass="MaskedEditFocus" OnInvalidCssClass="MaskedEditError" TargetControlID="ScheduleTime" Century="2000">
                            </ajaxToolkit:MaskedEditExtender>
                            
                            <ajaxToolkit:MaskedEditValidator ID="MaskedEditValidator3" runat="server" ControlExtender="MaskedEditExtender3"
                                ControlToValidate="ScheduleTime" Display="Dynamic" EmptyValueBlurredText="*" EmptyValueMessage="Time is required"
                                InvalidValueBlurredMessage="*" InvalidValueMessage="Time is invalid" IsValidEmpty="False"
                                TooltipMessage="Input a time" ValidationGroup="MKE">
                            </ajaxToolkit:MaskedEditValidator>
                            <br />
                        </td>
                    </tr>
                </table>
                <br />
            </ContentTemplate>
        </asp:UpdatePanel>
        </asp:Panel>
        
        <br />
     
        <asp:Panel ID="Panel_RunOptionsHeader" runat="server" CssClass="collapsePanelHeader">
            <div style="padding:5px; cursor: pointer; vertical-align: middle;">
                <div style="float: left;">
                    4. Options for running the task after propping:
                </div>
                <div style="float: right; vertical-align: middle;">
                    <asp:ImageButton ID="ImageRunOptions" runat="server" ImageUrl="~/images/expand.jpg" AlternateText="(Show Details...)" />
                </div>
                <div style="float: right;">
                    <span style="color: yellow;">[Optional] &nbsp;&nbsp;
                    </span>
                </div>
            </div>
        </asp:Panel>    
        
        <asp:Panel ID="Panel_RunOptions" runat="server" Height="110px" Width="100%" BackColor="#f6f1dd">
            <table width="100%" cellspacing="10">
                <tr>
                    <td style="background-color: inherit; height: 15px; width: 100%;">
                        <asp:UpdatePanel ID="RunOptionsUpdatePanel" runat="server">
                            <ContentTemplate>
                                Run options:
                                <table width="100%">
                                    <tr>
                                        <td style="width: 35%;" valign="top">
                                            <asp:CheckBox ID="CheckBox_RunAfterProp" runat="server" AutoPostBack="True" />
                                            Run build after propping?
                                            <br />
                                            <br />
                                        </td>            
                                    </tr>
                                    <tr>
                                        <td style="width: 35%;" >
                                            Configuation:
                                        </td>            
                                        <td style="width: 65%;" >
                                            Command line:
                                        </td>                             
                                    </tr>
                                    <tr>
                                        <td style="width: 35%;" >
                                            <asp:DropDownList ID="DropDownList_AvailableConfigs" runat="server" Width="85%" AutoPostBack="True" OnDataBinding="AvailableConfigs_DataBind" Font-Size="Small" />
                                        </td>
                                        <td style="width: 65%;" >
                                            <asp:TextBox ID="TextBox_RunCommandLine" runat="server" AutoPostBack="True" Width="85%" />
                                        </td>
                                    </tr>
                                </table>
                                <br />
                            </ContentTemplate>
                        </asp:UpdatePanel>
                    </td>
                </tr>
            </table>
        </asp:Panel> 
        
        <br />
        <center>
        <hr style="width: 95%;" size="5px" />
        </center>
        
        <asp:UpdatePanel ID="ScheduleTaskUpdatePanel" runat="server">
            <ContentTemplate>
                <br />
                <asp:Button ID="GetBuildNow" runat="server" OnClick="GetBuildNowButton_Click" Text="Get Build Now!" Font-Size="Large" />
                &nbsp;&nbsp;&nbsp;&nbsp;
                <asp:Button ID="ScheduleTaskButton" runat="server" OnClick="ScheduleTaskButton_Click" Text="Schedule Task" Font-Size="Large" />
                <br />
                <br />
                <asp:Label ID="TaskScheduledInfoLabel" runat="server" Font-Bold="True" ForeColor="#660000" Text="Label" Visible="False">
                </asp:Label>
            </ContentTemplate>
        </asp:UpdatePanel>
        
        <asp:ObjectDataSource ID="UPDS_DataSource" runat="server" SelectMethod="DistributionServer_GetConnectedList" TypeName="UnrealProp.Global" />
        
        <ajaxToolkit:CollapsiblePanelExtender ID="CollapsiblePanelExtender1" runat="server"
            TargetControlID="Panel_Builds" CollapseControlID="Panel_BuildHeader" CollapsedImage="~/images/expand.jpg" ExpandControlID="Panel_BuildHeader" ExpandedImage="~/images/collapse.jpg" ImageControlID="ImageBuild" CollapsedText="(Show Details...)" ExpandedText="(Hide Details...)" SuppressPostBack="True">
        </ajaxToolkit:CollapsiblePanelExtender>
        
        <ajaxToolkit:CollapsiblePanelExtender ID="CollapsiblePanelExtender2" runat="server"
            TargetControlID="Panel_Targets" CollapseControlID="Panel_TargetsHeader" CollapsedImage="~/images/expand.jpg" ExpandControlID="Panel_TargetsHeader" ExpandedImage="~/images/collapse.jpg" ImageControlID="ImageTargets" CollapsedText="(Show Details...)" ExpandedText="(Hide Details...)" SuppressPostBack="True">
        </ajaxToolkit:CollapsiblePanelExtender>
        
        <ajaxToolkit:CollapsiblePanelExtender ID="CollapsiblePanelExtender3" runat="server"
            TargetControlID="Panel_Time" CollapseControlID="Panel_TimeHeader" CollapsedImage="~/images/expand.jpg" ExpandControlID="Panel_TimeHeader" ExpandedImage="~/images/collapse.jpg" ImageControlID="ImageTime" CollapsedText="(Show Details...)" ExpandedText="(Hide Details...)" SuppressPostBack="True" Collapsed="True">
        </ajaxToolkit:CollapsiblePanelExtender>
        
        <ajaxToolkit:CollapsiblePanelExtender ID="CollapsiblePanelExtender4" runat="server"
            TargetControlID="Panel_RunOptions" CollapseControlID="Panel_RunOptionsHeader" CollapsedImage="~/images/expand.jpg" ExpandControlID="Panel_RunOptionsHeader" ExpandedImage="~/images/collapse.jpg" ImageControlID="ImageRunOptions" CollapsedText="(Show Details...)" ExpandedText="(Hide Details...)" SuppressPostBack="True" Collapsed="True">
        </ajaxToolkit:CollapsiblePanelExtender>
                
        <br />
        <br />
        <br />
        </span>
    </strong>
</asp:Content>

