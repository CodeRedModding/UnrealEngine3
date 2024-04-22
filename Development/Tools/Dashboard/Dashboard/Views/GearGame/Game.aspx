<%@ Page Language="C#" MasterPageFile="~/Views/Shared/Site.Master" Inherits="System.Web.Mvc.ViewPage<GameViewModel>" %>
<%@ Import Namespace="Dashboard.Models" %>
<asp:Content ID="Content1" ContentPlaceHolderID="TitleContent" runat="server">
    Match - <%=Model.Session.ID %>
</asp:Content>
<asp:Content ID="Content3" ContentPlaceHolderID="SubTitleContent" runat="server">
    <%=Model.Session.MapName %> - <%=Model.Session.GameClass %>
</asp:Content>

<asp:Content ID="Content5"  ContentPlaceHolderID="ScriptContent" runat="server" >
        <script type="text/javascript" src="http://www.google.com/jsapi"></script>
        

    <script src="../../Scripts/jquery-ui-1.8.5.custom.min.js" type="text/javascript"></script>
    <link href="../../Content/css/ui-darkness/jquery-ui-1.8.5.custom.css" rel="stylesheet"
        type="text/css" />

    <script src="../../Scripts/jquery-tooltip/lib/jquery.bgiframe.js" type="text/javascript"></script>

    <script src="../../Scripts/jquery-tooltip/lib/jquery.dimensions.js" type="text/javascript"></script>
    <link href="../../Scripts/jquery-tooltip/jquery.tooltip.css" rel="stylesheet" type="text/css" />
    <script src="../../Scripts/jquery-tooltip/jquery.tooltip.js" type="text/javascript"></script>

    <script type="text/javascript">
        google.load('visualization', '1', { packages: ['table'] });

        google.setOnLoadCallback(function () {
            var charts = $('.chart');
            for (i = 0; i < charts.length; i++) {
                drawChart(charts[i]);
            }

        });

        function drawChart(div, params) {
            var url = div.getAttribute('et:url');

            $.getJSON(url, function (response) {
                var data = new google.visualization.DataTable();


                for (var i in response.cols) {
                    data.addColumn(response.cols[i].type, response.cols[i].title);
                }

                for (var i in response.rows) {
                    var cell = response.rows[i].cell;
                    data.addRow(cell);

                }
                data.insertColumn(0, "string", "Name");
                for (i = 0; i < response.rows.length; i++) {
                    data.setCell(i, 0, response.rows[i].name);
                }

                google.visualization.DataTable.get

                var table = new google.visualization.Table(div);

                if (response.popup != false) {
                    for (var i = 1; i <= response.cols.length; i++) {
                        var formatter = new google.visualization.TablePatternFormat('<span class ="PopupData" PlayerName ="{1}" EventId ="' + response.cols[i - 1].id + '">{0}</span>');
                        formatter.format(data, [i, 0]); // Apply formatter and set the formatted value of the i-th column.
                    }
                }

                table.draw(data, { showRowNumber: false, allowHtml: true });


            });
        }
        $(".google-visualization-table-td-number .PopupData").live('hover', function () {

            $(".google-visualization-table-td-number .PopupData").tooltip({
                bodyHandler: function () {
                    //
                    var PlayerName = $(this).attr("PlayerName");

                    var EventId = $(this).attr("EventId");
                    return $.ajax({
                        url: "/Home/PopupTable",
                        async: false,
                        data: "SessionId=<%=Model.Session.ID %>&RoundId=0&EventId=" + EventId + "&PlayerName=" + PlayerName

                    }).responseText;

                },
                showURL: false
            });

        });
    </script>
    <script type="text/javascript">
        $(function () {
            $("#tabs").tabs({
                event: 'click'
            });
        });

        $(function () {
            $("#WeaponTabs").tabs({
                event: 'click'
            });
        });

    </script>


    <link href="../../Scripts/jquery-tooltip/jquery.tooltip.css" rel="stylesheet" type="text/css" />
   
    <script src="../../Scripts/jquery-tooltip/lib/jquery.bgiframe.js" type="text/javascript"></script>
    <script src="../../Scripts/jquery-tooltip/lib/jquery.dimensions.js" type="text/javascript"></script>
    <script src="../../Scripts/jquery-tooltip/jquery.tooltip.js" type="text/javascript"></script>

    <script type="text/javascript">

        $(document).ready(function () {
            $(".HeatMapThumb").click(function () {
                $(".HeatMap").hide();
                var key = $(this).attr("rel");
                $("#HeatMap-" + key).show();
            });
        });


    </script>

</asp:Content>


<asp:Content ID="Content2" ContentPlaceHolderID="MainContent" runat="server">



<div style='float:left'>
    <table width=1360>
    <tr>
        <th>Engine Version</th>
        <th>AppTitleID</th>
        <th>Language</th>
        <th>GameplaySessionTimeStamp</th>
        <th>GameplaySessionTime</th>
        <th>GameplaySessionID</th>
        <th>GameClassName</th>
    </tr>
    <tr>
        <td>  <%=Model.Session.EngineVersion%></td>
       <td><%=Model.Session.TitleID.ToString("X") %></td>
       <td><%=Model.Session.Language %></td>
       <td><%=Model.Session.DatePlayed%></td>
       <td><%TimeSpan t = TimeSpan.FromSeconds(Model.Session.EndTime - Model.Session.StartTime);%><%=t.Minutes %>:<%=t.Seconds %></td>
       <td><%=Model.Session.SessionUID %>: <%=Model.Session.InstanceIdx %></td>
       <td><%=Model.Session.GameClass %></td>
    </tr>
    <tr>
          <th>MapName</th>
        <th colspan=6>MapUrl</th>
    </tr>
       <td><%=Model.Session.MapName %></td> 
        <td colspan=6><%=Model.Session.MapUrl %></td>

    </table>
</div>
<br style='clear:both;'/>
    <div id="HeatMaps">
    <h2>HeatMaps</h2>
    <%
    try{ string show = "block";
        foreach (var Map in Model.Maps)
            { %>
                
                <div id="HeatMap-<%=Map.Key %>" class = "HeatMap" style="display: <%=show %>; float:left; margin:10px">
                    <h3 style="margin-top:0; padding-top: 0;"><%=Map.Key %></h3>
                    <img src="<%=Map.Value %>" />
                    
                </div>
        <%
            show = "none";
        } %>
           <% foreach (var Map in Model.Maps)
            { %>
                <div id="ThumbHeatMap-<%=Map.Key %>" rel="<%=Map.Key %>" class = "HeatMapThumb" style="display: inline; float:left; margin:10px">
                    
                    <img width="100" src="<%=Map.Value %>" />
                    <h4><%=Map.Key %></h4>
                </div>
        <%} %>


    <%} catch(NullReferenceException e)
        {%>
            <h3> No HeatMaps</h3> 
        
       <% } %>

    </div>

<div style='float:right;'>
 
 <h2>Highlights</h2>               
    <div class="chart" style="width:730px; "
        et:url="/GearGame/HighlightsGridData/<%=Model.Session.ID%>" >
                        
    </div>
</div>
<br class='clear' />
<h2>Player Statistics</h2>
<div id="tabs" style="max-width:1320px; width:1320px; margin-top: 20px">
   <ul>
      <li><a href="#tabs-1">Player Stats</a></li>
      <li><a href="#tabs-2">Weapon Stats</a></li>
      <li><a href="#tabs-3">Damage Dealt Stats</a></li>
      <li><a href="#tabs-4">Damage Received Stats</a></li>
      <li><a href="#tabs-5">Projectile Stats</a></li>
      <li><a href="#tabs-6">Pawn Stats</a></li>
   </ul>
    <div id="tabs-1" class="chart" style="width:1280px; margin:0 "
        et:url="/GearGame/PlayerStatsGridData/<%=Model.Session.ID%>" >

    </div>
    <div id="tabs-2"class="chart" style="width:1280px; "
    et:url="/GearGame/PlayerWeaponStatsGridData/<%=Model.Session.ID%>" >

    </div>
    <div id="tabs-3" class="chart" style="width:1280px; "
        et:url="/GearGame/DamageDealtStatsGridData/<%=Model.Session.ID%>" >
    </div>
    <div id="tabs-4" class="chart" style="width:1280px; "
        et:url="/GearGame/DamageReceivedStatsGridData/<%=Model.Session.ID%>" >
    </div>
    <div id="tabs-5" class="chart" style="width:1280px; "
        et:url="/GearGame/ProjectileStatsGridData/<%=Model.Session.ID%>" >
    </div>
    <div id="tabs-6" class="chart" style="width:380px; "
        et:url="/GearGame/PawnStatsGridData/<%=Model.Session.ID%>" >
    </div>
 
</div>
<h2>Game Totals</h2>
<div id="WeaponTabs" style="max-width:1320px; width:1320px; margin-top: 20px">
   <ul>
      <li><a href="#WeaponTabs-1">Weapon Stats</a></li>
      <li><a href="#WeaponTabs-2">Damage Dealt Stats</a></li>
      <li><a href="#WeaponTabs-3">Damage Received Stats</a></li>
      <li><a href="#WeaponTabs-4">Projectile Stats</a></li>
      <li><a href="#WeaponTabs-5">Pawn Stats</a></li>
   </ul>
     <div id="WeaponTabs-1" class="chart" style="width:1280px; "
        et:url="/GearGame/GameWeaponStatsGridData/<%=Model.Session.ID%>" >
    </div> 
         <div id="WeaponTabs-2" class="chart" style="width:1280px; "
        et:url="/GearGame/GameDamageDealtStatsGridData/<%=Model.Session.ID%>" >
    </div> 
     <div id="WeaponTabs-3" class="chart" style="width:1280px; "
        et:url="/GearGame/GameDamageReceivedStatsGridData/<%=Model.Session.ID%>" >
    </div> 
         <div id="WeaponTabs-4" class="chart" style="width:1280px; "
        et:url="/GearGame/GameProjectileStatsGridData/<%=Model.Session.ID%>" >
    </div> 
         <div id="WeaponTabs-5" class="chart" style="width:1280px; "
        et:url="/GearGame/GamePawnStatsGridData/<%=Model.Session.ID%>" >
    </div> 
</div>


</asp:Content>