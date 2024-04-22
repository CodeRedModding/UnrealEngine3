<%-- // Copyright 1998-2013 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<BuggViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>

<asp:Content ID="Content6" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/BuggsShow.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="Content1" ContentPlaceHolderID="TitleContent" runat="server">
	Show Bugg <%=Html.DisplayFor(m => Model.Bugg.Id )%> 
</asp:Content>

<asp:Content ID="Content3" ContentPlaceHolderID="PageTitle" runat="server">
	Show Bugg <%=Html.DisplayFor(m => Model.Bugg.Id )%> 
</asp:Content>

<asp:Content ID="Content5"  ContentPlaceHolderID="ScriptContent" runat="server" >
<script type="text/javascript">
	$(document).ready(function() {
		$("#EditDescription").click(function() {
			$("#CrashDescription").css("display", "none");
			$("#ShowCrashDescription input").css("display", "block");
			$("#EditDescription").css("display", "none");
			$("#SaveDescription").css("display", "inline");
		});

		$("#SaveDescription").click(function() {
			$('#EditCrashDescriptionForm').submit();
		});
		//Zebrastripes
		$("#CrashesTable tr:nth-child(even)").css("background-color", "#C3CAD0");

		//Get Table Height and set Main Height
		$("#main").height($("#CrashesTable").height() + 800);
		$("#CrashesTable").width($("#ShowCommandLine").width());

		//Show/Hide parts of the call stack
		$("#DisplayFunctionNames").click(function() {
			$(".function-name").toggle();
		});
		$("#DisplayFilePathNames").click(function() {
			$(".file-path").toggle();
		});
		$("#DisplayFileNames").click(function() {
			$(".file-name").toggle();
			$(".line-number").toggle();
		});
		$("#DisplayUnformattedCallStack").click(function() {
			$("#FormattedCallStackContainer").toggle();
			$("#RawCallStackContainer").toggle();
		});
	});
</script>
</asp:Content>

<asp:Content ID="Content4" ContentPlaceHolderID="AboveMainContent" runat="server">
	<br class='clear' />
</asp:Content>

<asp:Content ID="Content2" ContentPlaceHolderID="MainContent" runat="server">

<div id="SetBar">
	<%using (Html.BeginForm("Show", "Buggs"))
	  { %>
		<div id="SetInput">
			<span id="set-status" style="vertical-align: middle;">Set Status</span>

			<select name="SetStatus" id="SetStatus" >
				<option selected="selected" value=""></option>
				<option  value="Unset">Unset</option>
				<option value="Reviewed">Reviewed</option>
				<option value="New">New</option>
				<option value="Coder">Coder</option>
				<option value="Tester">Tester</option>
			</select>

			<input type="submit" name="SetStatusSubmit" value="Set" class="SetButton" />

			<span id="set-ttp" style="">TTP</span>
			<input name="SetTTP" type="text" id="ttp-text-box" />
			<input type="submit" name="SetTTPSubmit" value="Set" class="SetButton" />
	
			<span id="set-fixed-in" style="">FixedIn</span>
			<input name="SetFixedIn" type="text" id="fixed-in-text-box" />
			<input type="submit" name="SetFixedInSubmit" value="Set" class="SetButton" />
		</div>
<%} %>

</div>
<div id='CrashesShowContainer'>
	<div id='CrashDataContainer' >
		<h2>Bugg #<%=Html.DisplayFor(m => Model.Bugg.Id )%> </h2>
			<dl style='list-style-type: none; font-weight: bold' >
				<dt>ID</dt>
					<dd ><%=Html.DisplayFor(m => Model.Bugg.Id )%>	  </dd>

				<dt>Time of Latest Crash</dt> 
					<dd class='even' style='width:8em'>
						<%=Model.Bugg.TimeOfLastCrash%>
					</dd>

				<dt>Time of First Crash</dt> 
					<dd style='width:8em'>
						<%=Model.Bugg.TimeOfFirstCrash%>
					</dd>

				<dt>Number of Users</dt>
					<dd  class='even'><%=Html.DisplayFor(m => Model.Bugg.NumberOfUsers) %></dd>

				<dt>Number of Crashes</dt>
					<dd ><%=Html.DisplayFor(m => Model.Bugg.NumberOfCrashes) %></dd>

				<dt>TTP</dt>
					<dd  class='even'><%=Html.DisplayFor(m => Model.Bugg.TTPID) %></dd>

				<dt>Status</dt>
					<dd ><%=Html.DisplayFor(m => Model.Bugg.Status) %></dd>

				<dt>Fixed Change List</dt>
					<dd  class='even'><%=Html.DisplayFor(m => Model.Bugg.FixedChangeList) %></dd>
	</div>

	<div id="CallStackContainer" >
		<div class='CrashViewTextBox'>
			<div class='CrashViewTextBoxRight'>
				<h3>Call Stack of Most Recent Crash</h3>

				<div id='RawCallStackContainer' style='display:none' >
					<p>
						<% foreach (String Line in Model.CallStack.GetLines())
							{%>
								<%=Html.DisplayFor(x => Line)%>
								<br />
						<%} %>
					</p>
				</div>
				<div id="FormattedCallStackContainer">
					<p>
						<%foreach (string error in Model.Crashes.LastOrDefault().GetCallStackErrors(5, 500," "))
						  {  %>
							<span class='callstack-error'><%=Html.Encode(error)%></span>
							<br />
						<%} %>
						<%foreach (CallStackEntry CallStackLine in Model.CallStack.GetEntries())
							{%>
								<span class = "function-name">
									<%=Html.DisplayFor(m => CallStackLine.FunctionName)%>
								</span>
		
								<span class = "file-path" style='display:none'>
									<%=Html.DisplayFor(m => CallStackLine.FilePath) %>
								</span>

								<span class = "file-name">
									<%=Html.DisplayFor(m => CallStackLine.FileName) %>
								</span>

								<span class = "line-number">
									<%=Html.DisplayFor(m => CallStackLine.LineNumber) %>
								</span>

								<br />
						<%} %>
					</p>
				</div><!-- Formatted CallstackContainer -->
			</div>
		</div>
		<div id='FormatCallStack'>
			<%using (Html.BeginForm("Show", "Buggs" ) )
			  {	%>
				<%= Html.CheckBox("DisplayFunctionNames", true)%>
				<%= Html.Label("Functions") %>
		
				<%= Html.CheckBox("DisplayFileNames", true)%>
				<%= Html.Label("FileNames & Line Num") %>

				<%= Html.CheckBox("DisplayFilePathNames", false)%>
				<%= Html.Label("FilePaths") %>

				<%= Html.CheckBox("DisplayUnformattedCallStack", false)%>
				<%= Html.Label("Unformatted") %>
			<%} %>
		</div>

		<%using (Html.BeginForm("Show", "Buggs", FormMethod.Post, new { id = "EditCrashDescriptionForm"}))
			{ %>
			<div id = 'ShowCrashDescription' class='CrashViewTextBox'>
				<div class='CrashViewTextBoxRight'>	  
					<h3>Description <span class='EditButton' id='EditDescription'>Edit</span> <span class='EditButton' id='SaveDescription'>Save</span></h3>
					<p>
						<span id='CrashDescription'><%=Html.DisplayFor(m => Model.Bugg.Description) %> </span>
						<%=Html.TextBox("Description", Model.Bugg.Description)%> 
					</p>
				</div>
			</div>
		<%} %>

		<div id = 'ShowCommandLine' class='CrashViewTextBox'>
			<div class='CrashViewTextBoxRight'>
				<h3>Crashes</h3>
				<table id ='CrashesTable' style="table-layout:auto; width:auto; margin-top:10px; border-top: thin solid black"  cellspacing="0" cellpadding="0">
					<tr>
						<th>&nbsp;</th>
						<!-- if you add a new column be sure to update the switch statement in crashescontroller.cs -->
						<th style='width: 6em;'><%=Url.TableHeader("Id", "Id", Model)%></th>
						<th style='width: 15em'><%=Url.TableHeader("TimeOfCrash", "TimeOfCrash", Model)%></th>
						<th style='width: 12em'><%=Url.TableHeader("UserName", "UserName", Model)%></th>
						<th style='width: 12em;'><%=Url.TableHeader("CallStack", "RawCallStack", Model)%></th>
						<th>Summary</th>
						<th>Description</th>
					</tr>
					<%if (Model.Crashes.ToList() != null)
						{
						int iter = 0;
						foreach (Crash c in (IEnumerable)Model.Crashes)
						{
							iter++;%>
			
						<tr class='CrashRow <%=c.User.UserGroup %>'>
							<td class="CrashTd"><INPUT TYPE="CHECKBOX" Value="<%=iter %>"NAME="<%=c.Id%>" id="<%=c.Id %>" class='input CrashCheckBox' ></td> 
							<td class="Id" ><%=Html.ActionLink(c.Id.ToString(), "Show", new { controller = "crashes", id = c.Id }, null)%></td> 
							<td class="TimeOfCrash">
									<%=c.GetTimeOfCrash()[0]%><br />
								<%=c.GetTimeOfCrash()[1]%>
								<%=c.GetTimeOfCrash()[2]%><br />
								In CL: <%= c.ChangeListVersion %><br />
							</td>
							<td class="Username">
							<%=c.UserName%><br />
							<%=c.GameName %><br />
							<%=c.EngineMode %><br />
							<%=c.PlatformName %><br />
							</td>
							<td class="CallStack" >
								<div style="clip : auto; ">
									<div id='<%=c.Id %>-TrimmedCallStackBox' class='TrimmedCallStackBox'>
										<%
										int i = 0;
										foreach (string error in c.GetCallStackErrors(5,50, "Address"))
										{%>
											<%=Html.Encode(error)%>
											<%
											i++;
											if (i > 2) break;
										}%>
										<br />
										<%
										foreach(CallStackEntry Entry in c.GetCallStackEntries(3) )
										{%>
											<span class = "function-name">
											<%=Url.CallStackSearchLink( Html.Encode(Entry.GetTrimmedFunctionName(45) ), Model )%>
											</span><br />
										<%} %>
									</div>
						
									<a class='FullCallStackTrigger' ><span class='FullCallStackTriggerText'>Full Callstack</span>
										<div id='<%=c.Id %>-FullCallStackBox' class='FullCallStackBox'>
											<%foreach(CallStackEntry Entry in c.GetCallStackEntries(60) )
											  {%>
												<span class = "FunctionName">
													<%=Html.Encode(Entry.GetTrimmedFunctionName(0))%>
												</span>
												<span class = "FileName">
													<%=Html.DisplayFor(m => Entry.FileName) %>
												</span>
												<span class = "LineNumber">
													<em><%=Html.DisplayFor(m => Entry.LineNumber) %></em>
												</span>
												<br />
											<%} %>
										</div>
									</a>
								</div>
							</td>
							<td class="Summary"><%=c.Summary%>&nbsp;</td> 
							<td class="Description"><span class="TableData"><%=c.Description%>&nbsp;</span></td> 
						</tr>
						<% } %>
					<%} %>
					</table>
				</div>
		</div>
	</  div><!--CallStackContainer -->
</div><!-- CrashesShowContainer -->
&nbsp;
<br class='clear' />
<br class='clear' />

</asp:Content>
