<%-- // Copyright 1998-2013 Epic Games, Inc. All Rights Reserved. --%>

<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<BuggViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>

<asp:Content ID="Content1" ContentPlaceHolderID="TitleContent" runat="server">
	Create New Bugg
</asp:Content>
<asp:Content ID="Content6" ContentPlaceHolderID="CssContent" runat="server">
	<link href="../../Content/BuggsEdit.css" rel="stylesheet" type="text/css" />
</asp:Content>

<asp:Content ID="Content5"  ContentPlaceHolderID="ScriptContent" runat="server" >
	<style type="text/css">
	#feedback { font-size: 1em; }
	#selectable .ui-selecting { background: #FECA40; }
	#selectable .ui-selected { background: #F39814; color: white; }
	#selectable { list-style-type: none; margin: 0; padding: 0; width: 100%; }
	#selectable li { margin: 3px; padding: 0.4em; font-size: 1em; height: 20px; }
	</style>
	<script type="text/javascript">
		$(function () {
			$("#selectable").selectable({
				stop: function () {
					var result = $("#select-result").empty();
					var BuiltPattern = "";
					var PreviousIndex = null;
					$(".ui-selected", this).each(function () 
					{
						var index = $("#selectable li").index(this);
						result.append(" #" + (index + 1));

						if (PreviousIndex != index - 1 && index != 0) 
						{
							BuiltPattern = BuiltPattern + "%+"
						}

						BuiltPattern = BuiltPattern + $("#id-" + index).html() + "+";
						PreviousIndex = index;
					});
					BuiltPattern = BuiltPattern.substring(0, BuiltPattern.length - 1);
					result.html(BuiltPattern + "%");
					$("#FormPattern").val(BuiltPattern + "%");
				}
			});
		});
	</script>

<script type="text/javascript">
	$(document).ready(function () {

		//Zebrastripes
		$(".crashes ol li:nth-child(even)").css("background-color", "#C3CAD0");

		$("#BuggCallStack input").change(function () {
			var id = $(this).attr('id');
			$("#id-" + id).toggle();
			$("#wildcard-" + id).toggle();
		});
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
		<div id='BuggCallStack' class='crashes'>
			<ul id="selectable">
	
			<%
			int index = 0;
			var callstack = b.GetCallStackByPattern(b.Pattern);
			foreach( string function in callstack ) 
			  {%>
				<li class="ui-widget-content">
					<%=Html.Encode(function).Substring(0, ((function.Length < 50) ? function.Length : 50))%>
				</li>
			<% } %>
			</ul>
		</div> <!--BuggCallStack -->
		<div id = "bugg-<%=b.Id %>" class='bugg'  style="font-size: large;">
			ID: <%=b.Id %>
			<p>
			# of Matching Crashes: <%=crashes.Count() %></p>
			 <p id="Pattern">Base Crash Pattern: 
				%
				index = 0;
				foreach(string id in b.GetFunctionCallIdsByPattern(b.Pattern))
				{%>
					<span id='wildcard-<%=index%>' class='PatternValue' style='display:none;'>%</span>
					<span id='id-<%=index%>' class='PatternValue'><%=id %></span>
					<%index++;
				} %>
			</p>

			<p>Most Recent Crash: <%=crashes.First().TimeOfCrash %>
			First Crash: <%=crashes.Last().TimeOfCrash%></p>
			<p id="feedback">
				Working Pattern: <span id="select-result"><%=Model.Pattern%></span>
			</p>
		</div>
		<%using(Html.BeginForm("create", "buggs"))
		   {%>
			<%=Html.Hidden("FormPattern", Model.Pattern) %>
			<input type="submit" value="Find Crashes By Pattern" class='SearchButton' />
		<% } %>
		<%using(Html.BeginForm("save", "buggs"))
		  {%>
			<%=Html.Hidden("FormPattern", Model.Pattern) %>
			<%=Html.Hidden("FormCrashId", Model.CrashId) %>
			<input type="submit" value="Save Bugg/ Lock Pattern" class='SearchButton' />
		<%} %>
	 <br class='clear' />
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