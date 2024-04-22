<%@ Page Title="" Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<DashboardViewModel>" %>
<%@ Import Namespace="CrashReport.Models" %>


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

         $(document).ready(function () {
            });

             
    </script>



</asp:Content>

<asp:Content ID="Content4"  ContentPlaceHolderID="AboveMainContent" runat="server" >
    <br style='clear' />


</asp:Content>
<asp:Content ID="Content2" ContentPlaceHolderID="MainContent" runat="server">

<div id='CrashesTableContainer'>
    
    <br class= 'clear' />
    <div style='margin-top: 30px;'> 
    
     <script type='text/javascript' src='http://www.google.com/jsapi'></script>
    <script type='text/javascript'>
        google.load('visualization', '1', { 'packages': ['annotatedtimeline'] });
        google.setOnLoadCallback(drawChart);
        function drawChart() {
            var data = new google.visualization.DataTable();
            data.addColumn('date', 'Date');
            data.addColumn('number', 'General Crashes');
            data.addColumn('number', 'Coder Crashes');
            data.addColumn('number', 'Tester Crashes');
            data.addColumn('number', 'Automated Crashes');
            data.addColumn('number', 'All Crashes');
            data.addRows([
<%=Model.CrashesByDate%>
]);
            var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('weekly_chart'));
            chart.draw(data, { displayAnnotations: true });
        }
    </script>

    <script type='text/javascript'>
        google.load('visualization', '1', { 'packages': ['annotatedtimeline'] });
        google.setOnLoadCallback(drawChart);
        function drawChart() {
            var data = new google.visualization.DataTable();
            data.addColumn('date', 'Date');
            data.addColumn('number', 'General Crashes');
            data.addColumn('number', 'Coder Crashes');
            data.addColumn('number', 'Tester Crashes');
            data.addColumn('number', 'Automated Crashes');
            data.addColumn('number', 'All Crashes');
            data.addRows([
<%=Model.CrashesByDay%>
]);
            var chart = new google.visualization.AnnotatedTimeLine(document.getElementById('daily_chart'));
            chart.draw(data, { displayAnnotations: true });
        }
    </script>



        <h2>Crashes By Week</h2>
        <div id='weekly_chart' style='width: 1200px; height: 640px;'></div>
        <br /><br />
        
        <h2>Crashes By Day</h2>
        <div id='daily_chart' style='width: 1200px; height: 640px;'></div>
    </div>

 <%// Html.RenderPartial("/Views/Crashes/ViewCrash.ascx"); %>

 </div>

</asp:Content>
