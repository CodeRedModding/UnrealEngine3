<%@ Page Language="C#" MasterPageFile="~/MasterPage.master" enableEventValidation="false" CodeFile="ManageClientMachines.aspx.cs" Inherits="Web_Admin_ManageClientsMachines" Title="UnrealProp - Manage Target Machines" %>

<asp:Content ID="Content1" ContentPlaceHolderID="ContentPlaceHolder1" Runat="Server">
    <ajaxToolkit:ToolkitScriptManager ID="ClientMachinesAdminScriptManager" runat="server" />
    <asp:UpdatePanel ID="ClientMachineUpdatePanel" runat="server">
        <ContentTemplate>
        <asp:Menu id="MenuTabs" CssClass="menuTabs" StaticMenuItemStyle-CssClass="tab" StaticSelectedStyle-CssClass="selectedTab" Orientation="Horizontal" OnMenuItemClick="MenuTabs_MenuItemClick"
        Runat="server" BackColor="#FFFBD6" DynamicHorizontalOffset="2" Font-Bold="True" Font-Names="Verdana" Font-Size="0.8em" ForeColor="#990000" StaticSubMenuIndent="10px">
        <Items>
        <asp:MenuItem Text="Target Machine List" Value="0" Selected="True" />
        <asp:MenuItem Text="Add New" Value="1"/>
            
        </Items>
            <StaticMenuItemStyle CssClass="tab" HorizontalPadding="5px" VerticalPadding="2px" />
            <DynamicHoverStyle BackColor="#990000" ForeColor="White" />
            <DynamicMenuStyle BackColor="#FFFBD6" />
            <StaticSelectedStyle BackColor="#FFCC66" CssClass="selectedTab" />
            <DynamicSelectedStyle BackColor="#FFCC66" />
            <DynamicMenuItemStyle HorizontalPadding="5px" VerticalPadding="2px" />
            <StaticHoverStyle BackColor="#990000" ForeColor="White" />
    </asp:Menu> 
            <hr />            
    <asp:MultiView ID="MultiView1" runat="server" ActiveViewIndex="0">
        <asp:View ID="ClientMachineListView" runat="server">
    <asp:UpdatePanel ID="ClientMachineGridViewUpdatePanel" runat="server">
        <ContentTemplate>
        <center>
        
    <table style="width: 70%; font-size: small;" >
        <tr>
            <td align="center" class="header">Platforms:</td>
            <td align="center" class="header">User:</td>
        </tr>
        <tr>
            <td valign="top" align="center" style="width: 30%; height: 21px;" >
                <asp:DropDownList ID="PlatformDropDown" runat="server" AutoPostBack="True" Width="85%" Font-Size="Small" />
                <ajaxToolkit:CascadingDropDown ID="PlatformCascadingDropDown" runat="server"
                        Category="Platform" LoadingText="[Loading platforms...]" PromptText="All Platforms"
                        ServiceMethod="GetPlatforms" ServicePath="~/Web/UPWebService.asmx" TargetControlID="PlatformDropDown">
                </ajaxToolkit:CascadingDropDown>            
            </td>
            <td valign="top" align="center" style="width: 30%; height: 21px;" >
                <asp:DropDownList ID="AdminTargetsUserDropDown" runat="server" Font-Size="Small" Width="85%" AutoPostBack="True" DataSourceID="UserDataSource" DataTextField="Description" DataValueField="ID" OnDataBound="TargetsUserDropDown_DataBound" />
                <asp:ObjectDataSource ID="UserDataSource" runat="server" SelectMethod="User_GetListFromTargets" TypeName="UnrealProp.Global" />
            </td>            
        </tr>
    </table>
    <br />
            <asp:GridView ID="ClientMachineGridView" runat="server" AutoGenerateColumns="False" DataKeyNames="ID" DataSourceID="ClientMachinesDataSource" CellPadding="4" ForeColor="#333333" GridLines="None" Width="95%" PageSize="20"
                OnRowDeleting="ClientMachineGridView_RowDeleting" OnRowUpdating="ClientMachineGridView_RowUpdating" OnRowDataBound="ClientMachineGridView_RowDataBound" AllowSorting="True" Font-Size="Small">
                <Columns>
                    <asp:CommandField ShowDeleteButton="True" ShowEditButton="True" ButtonType="Image" CancelImageUrl="~/Images/cancel.png" DeleteImageUrl="~/Images/bin.png" EditImageUrl="~/Images/table_edit.png" UpdateImageUrl="~/Images/accept.png" />
                    <asp:BoundField DataField="ID" HeaderText="ID" InsertVisible="False" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" ReadOnly="True" SortExpression="ID" Visible="False" />
                    <asp:BoundField DataField="Name" HeaderText="Friendly Name" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Name" />
                    <asp:BoundField DataField="Path" HeaderText="Path/IP" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Path" />
                    <asp:BoundField DataField="Platform" HeaderText="Platform" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Platform" ReadOnly="True" />
                    <asp:BoundField DataField="ClientGroupName" HeaderText="Group" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="ClientGroupName" />
                    <asp:BoundField DataField="Email" HeaderText="Email" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Email" />
                    <asp:CheckBoxField DataField="Reboot" HeaderText="Reboot?" HeaderStyle-HorizontalAlign="Left" ItemStyle-HorizontalAlign="Left" SortExpression="Reboot" />
                </Columns>
                <FooterStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <RowStyle BackColor="#FFFBD6" ForeColor="#333333" />
                <SelectedRowStyle BackColor="#FFCC66" Font-Bold="True" ForeColor="Navy" />
                <PagerStyle BackColor="#FFCC66" ForeColor="#333333" HorizontalAlign="Center" />
                <HeaderStyle BackColor="#990000" Font-Bold="True" ForeColor="White" />
                <AlternatingRowStyle BackColor="White" />
                <EmptyDataTemplate>
                    <strong>There are no client machines passing the current filter.</strong>
                </EmptyDataTemplate>                
            </asp:GridView>
        </center>
        </ContentTemplate>
    </asp:UpdatePanel>
        </asp:View>
        <asp:View ID="AddNewMachineView" runat="server">
    <asp:UpdatePanel ID="ClientMachineNewUpdatePanel" runat="server">
        <ContentTemplate>
            <strong>Add new target client machine:<br />
                    <br />
            </strong>
                    <table style="background-color: white; font-weight: bold;">
                        <tr>
                            <td>Email:</td>
                            <td><asp:TextBox ID="TargetEmail" runat="server" Width="350px"></asp:TextBox></td>
                        </tr>                            
                        <tr>
                            <td>Group:</td>
                            <td><asp:TextBox ID="TargetGroup" runat="server" Width="200px"></asp:TextBox></td>
                        </tr>
                        <tr>
                            <td>Platform:</td>
                            <td><asp:DropDownList ID="AddNewTargetPlatform" runat="server" DataSourceID="PlatformDataSource" DataTextField="Name" DataValueField="ID" AutoPostBack="true" /></td>
                        </tr>
                        <tr>
                            <td>Friendly Name:</td>
                            <td><asp:TextBox ID="TargetName" runat="server" Width="350px"></asp:TextBox></td>
                        </tr>
                        <tr>
                            <td>Path/IP:</td>
                            <td><asp:TextBox ID="TargetPath" runat="server" Width="200px"></asp:TextBox></td>
                        </tr>
                        <tr>
                            <td>Reboot before propping?:</td>
                            <td><asp:CheckBox ID="TargetReboot" runat="server" Checked="true" ></asp:CheckBox></td>
                        </tr>                                                
                    </table>
                    <br />
                    <asp:Button ID="AddNewTargetButton" runat="server" OnClick="AddNewTargetButton_Click" Text="Add New" />
            <br />
            <br />
            For all new targets:
			<ul>
				<li>It's useful if you use an appropriate <strong>Group</strong> name (the same as others in your group)</li>
				<li>The <strong>Friendly Name</strong> is only needs to be as friendly as you want it to be</li>
			</ul>
            For Xenon and PS3 targets:
            <ul>
				<li>When filling in the fields above:</li>
				<ul>
					<li>The <strong>Path\IP</strong> is typically just the static IP address assigned to your development kit, such as 10.1.6.3</li>
				</ul>
				<li>Click the <strong>Add New</strong> button</li>
				<li>Click <strong>Task Scheduler</strong>, select a Project and Platform that match your console, and confirm you see your new target in the <strong>Consoles</strong> list</li>
            </ul>
            For PC targets:
            <ul>
				<li>Create a shared folder on your PC and give "Co-Owner / Full Control" rights to the user <strong>UnrealProp</strong>. This is very important, since without proper access rights to your PC, UnrealProp will fail to copy your requested build.</li>
				<li>When filling in the fields above:</li>
				<ul>
					<li>The <strong>Path\IP</strong> must be the network navigable path that can be seen from the UnrealProp machines, such as \\MyMachine\MyProps</li>
					<li>Note that <strong>Reboot before propping</strong> doesn't do anything on PC at this time</li>
				</ul>
				<li>Click the <strong>Add New</strong> button</li>
				<li>Click <strong>Task Scheduler</strong> and confirm you see your PC in the <strong>PCs</strong> list</li>
			</ul>
        </ContentTemplate>
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="PlatformDropDown" EventName="SelectedIndexChanged" />
        </Triggers>        
    </asp:UpdatePanel>
        </asp:View>
    </asp:MultiView>
            <asp:ObjectDataSource ID="ClientMachinesDataSource" runat="server" SelectMethod="ClientMachine_GetListForPlatformAndUser" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}">
                <SelectParameters>
                    <asp:ControlParameter ControlID="PlatformDropDown" DefaultValue="-1" Name="PlatformID" PropertyName="SelectedValue" Type="Int16" />
                    <asp:ControlParameter ControlID="AdminTargetsUserDropDown" DefaultValue="1" Name="UsernameID" PropertyName="SelectedValue" Type="Int32" />
                </SelectParameters>
            </asp:ObjectDataSource>
            <asp:ObjectDataSource ID="PlatformDataSource" runat="server" SelectMethod="Platform_GetList" TypeName="UnrealProp.Global" OldValuesParameterFormatString="{0}" />
        </ContentTemplate>
        <Triggers>
            <asp:AsyncPostBackTrigger ControlID="PlatformDropDown" EventName="SelectedIndexChanged" />
            <asp:AsyncPostBackTrigger ControlID="AdminTargetsUserDropDown" EventName="SelectedIndexChanged" />
        </Triggers>        
    </asp:UpdatePanel>
</asp:Content>

