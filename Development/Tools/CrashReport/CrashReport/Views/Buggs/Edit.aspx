<%-- // Copyright 1998-2013 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<BuggViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>

<asp:Content ID="Content1" ContentPlaceHolderID="TitleContent" runat="server">
	Show
</asp:Content>

<asp:Content ID="Content6" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/BuggsEdit.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="Content5"  ContentPlaceHolderID="ScriptContent" runat="server" >
	<script type="text/javascript">
		$(document).ready(function () {
			//Zebrastripes
			$(".crashes li:nth-child(even)").css("background-color", "#C3CAD0");
		});
	
	</script>
</asp:Content>

<asp:Content ID="Content2" ContentPlaceHolderID="MainContent" runat="server">
	<div id='BuggsShowContainer'> 
	<div id='BuggData'>
	<% var b = Model.Bugg;
		  var crashes = Model.Crashes;
		
		/*TODO 
		 * Remove Crash
		 * Remove Multiple Crashes
		 * Create New Bugg from Multiple Crashes
		 * ? Add/Remove Function from Bugg Search
		 * ? - would require rebuilding Bugg? -need to keep track of crashes manualyl removed

		 */

		   %>
		<div id = "bugg-<%=b.Id %>" class='bugg'  style="font-size: large;">
	
			ID: <%=b.Id %>(<%=crashes.Count() %>)
		
			<p>Most Recent Crash: <%=b.TimeOfLastCrash %>
			First Crash: <%=b.TimeOfFirstCrash %></p>
			<p> Pattern: <%=b.Pattern %></p>
			<p> Number of Users: <%=b.NumberOfUsers%></p>

		</div>
		<%foreach(Crash c in crashes ) 
		  {%>
			<div id = "crashes-<%=c.Id %>" class='crashes' >
				<span class='DeleteCrash' style='float:right;'><%=Html.ActionLink("X", "Show", new { controller = "Buggs", DeleteCrash = c.Id }) %></span>
				<ol>
					<%=Html.ActionLink(c.Id.ToString(), "Show", new { controller = "crashes", id = c.Id }, null)%>
					<%foreach(CallStackEntry Entry in c.GetCallStackEntries(60) )
					{%><li>
							<span class = "FunctionName">
								<%=Html.Encode(Entry.GetTrimmedFunctionName(40))%>
							</span>
							<span class = "FileName">
								<%=Html.DisplayFor(m => Entry.FileName) %>
							</span>
							<span class = "LineNumber">
								<em><%=Html.DisplayFor(m => Entry.LineNumber) %></em>
							</span>
						</li>
					<%} %>
				</ol>
			</div>
		<%} %>
		<br class='clear' />
	</div>
</div>
	<br class='clear' />
	<br class='clear' />	

</asp:Content>
