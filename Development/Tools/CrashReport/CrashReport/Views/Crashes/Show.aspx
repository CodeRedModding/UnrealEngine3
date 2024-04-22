<%-- // Copyright 1998-2013 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<CrashViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>

<asp:Content ID="Content6" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/CrashesShow.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="Content1" ContentPlaceHolderID="TitleContent" runat="server">
	Show
</asp:Content>

<asp:Content ID="Content3" ContentPlaceHolderID="PageTitle" runat="server">
Crash <%=Model.Crash.Id %>
</asp:Content>

<asp:Content ID="Content5"  ContentPlaceHolderID="ScriptContent" runat="server" >
<script type="text/javascript">
	$(document).ready(function () {
		$("#EditDescription").click(function () {
			$("#CrashDescription").css("display", "none");
			$("#ShowCrashDescription input").css("display", "block");
			$("#EditDescription").css("display", "none");
			$("#SaveDescription").css("display", "inline");
		});

		$("#SaveDescription").click(function () {
			$('#EditCrashDescriptionForm').submit();
		});

		$("#DisplayFunctionNames").click(function () {
			$(".function-name").toggle();
		});

		$("#DisplayFilePathNames").click(function () {
			$(".file-path").toggle();
		});

		$("#DisplayFileNames").click(function () {
			$(".file-name").toggle();
			$(".line-number").toggle();
		});

		$("#DisplayUnformattedCallStack").click(function () {
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
<% using (Html.BeginForm("Show", "Crashes"))
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

		<input type="submit" name="SetStatusSubmit" value="Set" class="SetButton"   />
		<span id="set-ttp" style="">TTP</span>
		<input name="SetTTP" type="text" id="ttp-text-box"   />
		<input type="submit" name="SetTTPSubmit" value="Set" class="SetButton" />
		<span id="set-fixed-in" style="">FixedIn</span>
		<input name="SetFixedIn" type="text" id="fixed-in-text-box" />
		<%=Html.Hidden("Description", Model.Crash.Description)%> 
		<input type="submit" name="SetFixedInSubmit" value="Set" class="SetButton" />
	</div>
<% } %>
</div>

<div id='CrashesShowContainer'>
	<div id='CrashDataContainer' >
		<dl style='list-style-type: none; font-weight: bold' >
			<dt>ID</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.Id )%></dd>

			<dt>Bugg(s)</dt>
				
				<%
				if(Model.Crash.Buggs.Count >0)
				{
					foreach (Bugg b in Model.Crash.Buggs)
					{%>
					<dd>
						<%=Html.ActionLink(b.Id.ToString(), "Show", new {controller = "Buggs", Id = b.Id})%></dd>
					<%
					}
				}
				else
				{%>
					<dd></dd>
				<%	
				}%>
				
			<dt>Saved Files</dt>
				<dd class='even'>
					<%if (!Model.Crash.HasLogFile.HasValue || Model.Crash.HasLogFile.Equals(true)) 
					{ %>
						<a style='font-size:large; text-decoration:none; vertical-align:middle;' href='<%=Model.Crash.GetLogUrl()%>'><img src="../../Content/Images/Icons/log.png" style="height:20px;border:none;" />&nbsp;Log</a>
						<%
					} %>
		
				</dd>
			<dt>&nbsp;</dt>
				<dd>	
					<%if (!Model.Crash.HasMiniDumpFile.HasValue || Model.Crash.HasMiniDumpFile.Equals(true)) 
					{ %>
						<a style='font-size:large; text-decoration:none;' href='<%=Model.Crash.GetDumpUrl()%>'><img src="../../Content/Images/Icons/miniDump.png" style="height:20px;border:none;" />&nbsp;MiniDump</a>
						<%
					} %>
				</dd>
			<dt>&nbsp;</dt>
				<dd>
					<%if (Model.Crash.HasVideoFile.HasValue && Model.Crash.HasVideoFile.Equals(true)) 
					{ %>
						<a id="VideoLink" style="font-size:large;text-decoration:none;" href='<%=Model.Crash.GetVideoUrl()%>'><img src="../../Content/Images/Icons/video.png" style="height:20px;border:none;" />&nbsp;Video</a>
						<%
					} %>
				</dd>
				<dt>Time of Crash</dt> 
				<dd class='even'>
				<%=Model.Crash.GetTimeOfCrash()[0]%><br />
				<%=Model.Crash.GetTimeOfCrash()[1]%>
				<%=Model.Crash.GetTimeOfCrash()[2]%>

			<dt>User</dt>
				<dd><%=Html.DisplayFor(m => Model.Crash.UserName) %></dd>

			<dt>Game Name</dt>
				<dd  class='even'><%=Html.DisplayFor(m => Model.Crash.GameName) %></dd>

			<dt>Engine Mode</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.EngineMode) %></dd>

			<dt>Language</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.LanguageExt) %></dd>
			
			<dt>Platform</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.PlatformName) %></dd>

			<dt>Computer</dt> 
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.ComputerName) %></dd>

			<dt>Build Version</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.BuildVersion) %></dd>

			<dt>FixedChangeList #</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.FixedChangeList) %></dd>

			<dt>ChangeList #</dt>
				<dd class='even'><%=Html.DisplayFor(m => Model.Crash.ChangeListVersion) %></dd>

			<dt>TTP</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.TTPID) %></dd>

			<dt>Status</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.Status) %></dd>
			<dt>Module</dt>
				<dd ><%=Html.DisplayFor(m => Model.Crash.Module) %></dd>
	</div>

	<div id="CallStackContainer" >
		<div class='CrashViewTextBox'>
			<div class='CrashViewTextBoxRight'>
				<h3>Call Stack</h3>
				<div id='RawCallStackContainer' style='display: none'>
					<p>
						<% foreach (String Line in Model.CallStack.GetLines())
						   {%>
							<%=Html.DisplayFor(x => Line)%>
							<br />
						<% } %>
					</p>
				</div>
				<div id="FormattedCallStackContainer">
					<p>
						<% foreach (string error in Model.Crash.GetCallStackErrors(5, 500," "))
						   {  %>
							<span class='callstack-error'><%=Html.Encode(error)%></span>
							<br />
						<% } %>
				
						<% foreach (CallStackEntry CallStackLine in Model.CallStack.GetEntries())
						   {%>
								<span class = "function-name">
									<%=Html.DisplayFor(m => CallStackLine.FunctionName)%>
								</span>
		
								<span class = "file-path" style='display: none'>
									<%=Html.DisplayFor(m => CallStackLine.FilePath) %>
								</span>

									<%if (Model.CallStack.DisplayFileNames)
									{ %>

										<span class = "file-name">
											<%=Html.DisplayFor(m => CallStackLine.FileName) %>
										</span>

										<span class = "line-number">
											<%=Html.DisplayFor(m => CallStackLine.LineNumber) %>
										</span>
		
								<%  } %>
								<br />
						<% } %>
					</p>
				</div><!-- Formatted CallstackContainer -->
			</div>
		</div>
		<div id='FormatCallStack'>
		<% using (Html.BeginForm("Show", "Crashes" ) )
		   { %>
			<%= Html.CheckBox("DisplayFunctionNames", true)%>
			<%= Html.Label("Functions") %>
			<%= Html.CheckBox("DisplayFileNames", true)%>
			<%= Html.Label("FileNames & Line Num") %>
			<%= Html.CheckBox("DisplayFilePathNames", false)%>
			<%= Html.Label("FilePaths") %>
			<%= Html.CheckBox("DisplayUnformattedCallStack", false)%>
			<%= Html.Label("Unformatted") %>
		<% } %>
		</div>
		<div id='ShowCrashSummary' class='CrashViewTextBox'>
			<div class='CrashViewTextBoxRight'>
				<h3>Summary</h3>
					<p><span id='CrashSummary'><%=Html.DisplayFor(m => Model.Crash.Summary) %></span></p>
			</div>
		</div>
		<% using (Html.BeginForm("Show", "Crashes", FormMethod.Post, new { id = "EditCrashDescriptionForm"}))
		   { %>
			<div id = 'ShowCrashDescription' class='CrashViewTextBox'>
				<div class='CrashViewTextBoxRight'>	  
					<h3>Description <span class='EditButton' id='EditDescription'>Edit</span> <span class='EditButton' id='SaveDescription'>Save</span></h3>
					<p>
						<span id='CrashDescription'><%=Html.DisplayFor(m => Model.Crash.Description) %> </span>
						<%=Html.TextBox("Description", Model.Crash.Description)%> 
					</p>
				</div>
			</div>
		<% } %>
		<div id = 'ShowCommandLine' class='CrashViewTextBox'>
			<div class='CrashViewTextBoxRight'>
				<h3>Command Line</h3>
				<p>
					<span id='CrashCommandLine'><%=Html.DisplayFor(m => Model.Crash.CommandLine) %></span>
				</p>
			</div>
		</div>
	</div>
</div>
&nbsp;
<br class='clear' />
<br class='clear' />

</asp:Content>