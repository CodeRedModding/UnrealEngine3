<%-- // Copyright 1998-2013 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<CrashesViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>
<%@ Import Namespace="System.Security.Policy" %>
<%@ Import Namespace="System.Data.SqlClient" %>


<asp:Content ID="Content6" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/CrashesIndex.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="Content1" ContentPlaceHolderID="TitleContent" runat="server">
	Crash Reports
</asp:Content>

<asp:Content ID="Content3"  ContentPlaceHolderID="PageTitle" runat="server" >
Crashes
</asp:Content>

<asp:Content ID="Content5"  ContentPlaceHolderID="ScriptContent" runat="server" >

	<script type="text/javascript">
		$(function () {
			$("#dateFrom").datepicker({ maxDate: '+0D' });
			$("#dateTo").datepicker({ maxDate: '+0D' });
		});
	</script>

	<script>
		$(document).ready(function () {
			//Select All
			$("#CheckAll").click(function () {
				$(":checkbox").attr('checked', true);
				$("#CheckAll").css("color", "Black");
				$("#CheckNone").css("color", "Blue");
				$("#SetInput").unblock();
			});

			//Select None
			$("#CheckNone").click(function () {
				$(":checkbox").attr('checked', false);
				$("#CheckAll").css("color", "Blue");
				$("#CheckNone").css("color", "Black");
				$("#SetInput").block({
					message: null
				});
			});

			//Shift Check box
			$(":checkbox").shiftcheckbox();

			//Zebrastripes
			$("#CrashesTable tr:nth-child(even)").css("background-color", "#C3CAD0");
			$.blockUI.defaults.overlayCSS.top = " -6pt";
			$.blockUI.defaults.overlayCSS.left = " -6pt";
			$.blockUI.defaults.overlayCSS.padding = "6pt";
			$.blockUI.defaults.overlayCSS.backgroundColor = "#eeeeee";

			$("#SetInput").block({
				message: null
			});

			$("input:checkbox").click(function () {
				var n = $("input:checked").length;
				if (n > 0) {
					$("#SetInput").unblock();
				}
				else 
				{
					$("#SetInput").block({
						message: null
					});
				}
			});

			$(".CrashType").click(function () {
				$("#FilterCrashesForm").submit();
			});
		});
	</script>

</asp:Content>

<asp:Content ID="Content4"  ContentPlaceHolderID="AboveMainContent" runat="server" >
	<br style='clear' />
	<div id='SearchForm'>		
		
	<% using (Html.BeginForm("", "Crashes", FormMethod.Get, new {id = "FilterCrashesForm"}))
	{ %>
		<%=Html.HiddenFor(u => u.UserGroup)%>
		<%=Html.Hidden("SortTerm", Model.Term)%>
		<%=Html.Hidden("SortOrder", Model.Order)%>
		<span style = "float:left;">
			<input type="radio" name = "CrashType" class = "CrashType" value="CrashesAsserts" <%=(Model.CrashType == "CrashesAsserts") ? "checked='checked'" : "" %> /> <span title='All Crashes Except Ensures'>Crashes+Asserts</span>
			<input type="radio" name = "CrashType" class = "CrashType" value="Ensure" <%=(Model.CrashType == "Ensure") ? "checked='checked'" : "" %>/> <span title='Only Ensures'>Ensures</span>
			<input type="radio" name = "CrashType" class = "CrashType" value="Assert" <%=(Model.CrashType == "Assert") ? "checked='checked'" : "" %>/> <span title='Only Asserts'>Asserts</span>
			<input type="radio" name = "CrashType" class = "CrashType" value="Crashes" <%=(Model.CrashType == "Crashes") ? "checked='checked'" : "" %>/> <span title='Crashes Except Ensures and Asserts'>Crashes</span>
			<input type="radio" name = "CrashType" class = "CrashType" value="All" <%=(Model.CrashType == "All") ? "checked='checked'" : "" %>/> <span title='All Crashes'>All</span>
		</span>
		<br style='clear' />
		<br style='clear' />

		<div id="SearchBox" ><%= Html.TextBox("SearchQuery", Model.Query, new { width = "1000" })%>  <input type="submit" value="Search" class='SearchButton' /></div>
	
		<script>$.datepicker.setDefaults($.datepicker.regional['']);</script>

		<span style="margin-left: 10px; font-weight:bold;">
			Filter by Date 
		</span>

		<span>
			From: <input id="dateFrom" name="dateFrom" type="text" value="<%=Model.DateFrom %>" AUTOCOMPLETE=OFF/>
		</span>

		<span>
			To: <input  id="dateTo" name="dateTo" type="text" value="<%=Model.DateTo %>" AUTOCOMPLETE=OFF/>
		</span>

		<span style="margin-left: 10px; font-weight:bold;">
			Filter Game Name: 
		</span>
		
		<span>
			<input  id="GameName" name="GameName" type="text" value="<%=Model.GameName %>" AUTOCOMPLETE=OFF/>
		</span>
	<% } %>
	</div>
</asp:Content>

<asp:Content ID="Content2" ContentPlaceHolderID="MainContent" runat="server">

<div id='CrashesTableContainer'>
	<div id='UserGroupBar'>
		<%foreach(var GroupCount in Model.GroupCounts){%>
		<span class = <%if (Model.UserGroup == GroupCount.Key){ %> "UserGroupTabSelected" <%} else {%> "UserGroupTab"<%} %> id="<%=GroupCount.Key%>Tab">
			<%= Url.UserGroupLink(GroupCount.Key, Model)%> 
			<span class="UserGroupResults">
				(<%=GroupCount.Value%>)
			</span>
		</span>
		<%} %>
	</div>

	<div id='SetForm'>
		<% using (Html.BeginForm("", "Crashes"))
		   { %>
	</div>
			<br sytle='clear:both' />
			<!--[if !IE]> -->
			<div style="background-color: #E8EEF4; margin-bottom: -5px; width: 19.7em;">
			<!-- <![endif]-->
			<!--[if IE 9]>
			<div style="background-color: #E8EEF4; margin-bottom: -5px; width: 19.7em; padding-bottom:0px;">
			<![endif]-->
            <!--[if lt IE 9]>
			<div style="background-color: #E8EEF4; margin-bottom: 10px; width: 19.7em; padding-bottom:0px;">
			<![endif]-->
				<span style="background-color: #E8EEF4; font-size: medium; padding-left: 1em;"> <%=Html.ActionLink("Crashes", "Index", "Crashes", new { }, new { style = "color:black; text-decoration:none;" })%></span>
				<span style="background-color: #C3CAD0; font-size: medium; margin-left: 1em; padding:0 1em;" title='A Bugg is a collection of Crashes that have the exact same callstack.'> <%=Html.ActionLink("CrashGroups", "Index", "Buggs", new { }, new { style = "color:black; text-decoration:none;" })%></span>
			</div>
				<% Html.RenderPartial("/Views/Crashes/ViewCrash.ascx"); %>
		<% } %>

	<div class="PaginationBox">
		<%= Html.PageLinks(Model.PagingInfo, i => Url.Action("", new { page = i, SearchQuery = Model.Query, SortTerm = Model.Term, SortOrder = Model.Order, UserGroup = Model.UserGroup, DateFrom = Model.DateFrom, DateTo = Model.DateTo, CrashType = Model.CrashType }))%>
	</div>
</div>

</asp:Content>
