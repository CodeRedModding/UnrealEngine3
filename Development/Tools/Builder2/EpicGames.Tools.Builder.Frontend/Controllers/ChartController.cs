using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Web.Mvc;
using System.Web.UI.DataVisualization.Charting;
using EpicGames.Tools.Builder.Frontend.Database;

namespace EpicGames.Tools.Builder.Frontend.Controllers
{
    public class ChartController : BaseController
    {
        #region Fields

        /// <summary>
        /// Hold the font to be used in the charts.
        /// </summary>

        protected Font chartFont = new Font("Segoe UI", 15);

        protected Font smallFont = new Font("Segoe UI", 11);

        protected Color blueColor = Color.CornflowerBlue;

        protected Color greenColor = ColorTranslator.FromHtml("#55aa55");

        protected Color orangeColor = ColorTranslator.FromHtml("#ff9933");

        protected Color redColor = ColorTranslator.FromHtml("#ee5555");


        #endregion


        #region Actions

        /// <summary>
        /// Creates pie chart for the state of all build machines.
        /// </summary>
        /// <returns>Pie chart image.</returns>

        public FileResult BranchesState ()
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = chartFont;

            // branch series
            var branchSeries = new Series();

            branchSeries.BorderColor = Color.DarkGray;
            branchSeries.ChartType = SeriesChartType.Pie;
            branchSeries.Font = chartFont;
            branchSeries.Name = "Branches";
            branchSeries.Label = "#VALY";
            branchSeries.Legend = "Default";

            branchSeries["PieDrawingStyle"] = "Concave";
            branchSeries["PieLabelStyle"] = "Outside";

            var branchStatus = dataContext.SP_GetBuildBranches().ToList();

            int activeBranches = branchStatus.Count(b => (b.NumCommands.HasValue && (b.NumCommands > 0)));

            int a = branchSeries.Points.AddXY("Active", activeBranches);

            branchSeries.Points[a].Color = blueColor;
            branchSeries.Points[a].LegendText = "Active";

            int i = branchSeries.Points.AddXY("Active", branchStatus.Count() - activeBranches);

            branchSeries.Points[i].Color = redColor;
            branchSeries.Points[i].LegendText = "Inactive";

            // chart area
            var chartArea = new ChartArea();

            // chart
            var chart = new Chart();

            chart.Height = 480;
            chart.Width = 480;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(branchSeries);

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates a stacked area chart for the number of builds per day.
        /// </summary>
        /// <param name="numDays">The number of days to track.</param>
        /// <returns>Stacked chart image.</returns>

        public FileResult BuildsPerDay(int? numDays)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.Interval = 1;
            chartArea.AxisX.IsLabelAutoFit = true;
            chartArea.AxisX.IntervalType = DateTimeIntervalType.Days;
            chartArea.AxisX.LabelStyle.Interval = 1;
            chartArea.AxisX.LabelStyle.IntervalType = DateTimeIntervalType.Months;
            chartArea.AxisX.MajorGrid.Interval = 1;
            chartArea.AxisX.MajorGrid.IntervalType = DateTimeIntervalType.Months;
            chartArea.AxisX.MajorTickMark.Interval = 1;
            chartArea.AxisX.MajorTickMark.IntervalType = DateTimeIntervalType.Weeks;

            // chart
            var chart = new Chart();

            chart.Height = 512;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);

            chart.Series.Add(GetBuildsPerDaySeries("Promotions", numDays.GetValueOrDefault(180), "Promote", false));
            chart.Series.Add(GetBuildsPerDaySeries("Cooks", numDays.GetValueOrDefault(180), "Cook", false));
            chart.Series.Add(GetBuildsPerDaySeries("PCS", numDays.GetValueOrDefault(180), "PCS", false));
            chart.Series.Add(GetBuildsPerDaySeries("Verification Builds", numDays.GetValueOrDefault(180), "CIS", true));
            chart.Series.Add(GetBuildsPerDaySeries("Builds", numDays.GetValueOrDefault(180), "Build", false));
            chart.Series.Add(GetBuildsPerDaySeries("Compile Jobs", numDays.GetValueOrDefault(180), "Jobs/Unreal", false));
            chart.Series.Add(GetBuildsPerDaySeries("CIS Jobs", numDays.GetValueOrDefault(180), "Jobs/CIS", false));

            chart.DataManipulator.InsertEmptyPoints(1, IntervalType.Days, "Verification Builds, Compile Jobs, CIS Jobs, PCS, Cooks, Builds, Promotions");

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates a stacked area chart for the CIS turnaround times.
        /// </summary>
        /// <param name="numDays">The number of days to track.</param>
        /// <returns>Stacked chart image.</returns>

        public FileResult CisTurnaround(int? numDays)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // creation delay series
            var seriesCreationDelay = new Series();

            seriesCreationDelay.ChartType = SeriesChartType.StackedArea;
            seriesCreationDelay.Name = "Creation Delay";
            seriesCreationDelay.Legend = "Default";

            // wait time series
            var seriesWaitTime = new Series();

            seriesWaitTime.ChartType = SeriesChartType.StackedArea;
            seriesWaitTime.Name = "Wait Time";
            seriesWaitTime.Legend = "Default";

            // duration series
            var seriesDuration = new Series();

            seriesDuration.ChartType = SeriesChartType.StackedArea;
            seriesDuration.Name = "Duration";
            seriesDuration.Legend = "Default";

            // populate series
            foreach (var t in dataContext.SP_GetCisTurnaround(numDays.GetValueOrDefault(14)))
            {
                DateTime creationTime = new DateTime(t.SpawnTime.Value);
                DateTime submissionTime = t.TimeStamp;
                DateTime startTime = t.BuildStarted.Value;
                DateTime endTime = t.BuildEnded.Value;

                seriesCreationDelay.Points.AddXY(submissionTime, (creationTime - submissionTime).TotalMinutes);
                seriesWaitTime.Points.AddXY(submissionTime, (startTime - creationTime).TotalMinutes);
                seriesDuration.Points.AddXY(submissionTime, (endTime - startTime).TotalMinutes);
            }

            // chart area
            var chartArea = new ChartArea();

            // chart
            var chart = new Chart();

            chart.Height = 512;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(seriesCreationDelay);
            chart.Series.Add(seriesDuration);
            chart.Series.Add(seriesWaitTime);

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates the log chart for the specified command.
        /// </summary>
        /// <param name="id">Command ID.</param>
        /// <param name="days">Number of days of historical data.</param>
        /// <returns>Log chart.</returns>

        public FileResult CommandLog (int id, int days)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // failed builds series
            var seriesFailed = new Series();

            seriesFailed.ChartType = SeriesChartType.StackedColumn;
            seriesFailed.Color = redColor;
            seriesFailed.Font = smallFont;
            seriesFailed.Name = "Command Failed";
            seriesFailed.IsXValueIndexed = true;
            seriesFailed.ToolTip = "\t= #VALX{d} \n\t= #VALY{f}";
            seriesFailed.Legend = "Default";

            seriesFailed["EmptyPointValue"] = "Zero";

            foreach (var b in dataContext.SP_GetCommandActivity(id, "Failed", days).ToList())
            {
                seriesFailed.Points.AddXY(b.BuildStartedDate.Value, b.LogCount);
            }

            // successful builds series
            var seriesSucceeded = new Series();

            seriesSucceeded.ChartType = SeriesChartType.StackedColumn;
            seriesSucceeded.Color = greenColor;
            seriesSucceeded.Font = smallFont;
            seriesSucceeded.Name = "Command Succeeded";
            seriesSucceeded.IsXValueIndexed = true;
            seriesSucceeded.ToolTip = "\t= #VALX{d} \n\t= #VALY{f}";
            seriesSucceeded.Legend = "Default";

            seriesSucceeded["EmptyPointValue"] = "Zero";

            foreach (var b in dataContext.SP_GetCommandActivity(id, "Succeeded", days).ToList())
            {
                seriesSucceeded.Points.AddXY(b.BuildStartedDate.Value, b.LogCount);
            }

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.Interval = 4.0;
            chartArea.AxisX.IsLabelAutoFit = false;
            chartArea.AxisX.LabelStyle.Enabled = true;
            chartArea.AxisX.LabelStyle.Font = smallFont;
            chartArea.AxisX.MajorGrid.Enabled = false;
            chartArea.AxisX.MajorGrid.LineColor = Color.Gray;
            chartArea.AxisX.MajorGrid.Interval = 7;
            chartArea.AxisX.MinorGrid.Enabled = false;
            chartArea.AxisX.MinorGrid.LineColor = Color.Silver;
            chartArea.AxisX.MinorGrid.Interval = 1;
            chartArea.AxisX.MinorGrid.IntervalOffset = 0.5;

            // chart
            var chart = new Chart();

            chart.Height = 320;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(seriesFailed);
            chart.Series.Add(seriesSucceeded);

            if ((seriesFailed.Points.Count > 0) || (seriesSucceeded.Points.Count > 0))
            {
                chart.DataManipulator.InsertEmptyPoints(1, IntervalType.Days, "Command Failed, Command Succeeded");
            }

            // highlight weekends
            if (seriesFailed.Points.Count > 0)
            {
                var weekendLine = new StripLine();

                weekendLine.BackColor = Color.Gainsboro;
                weekendLine.Interval = 7;
                weekendLine.IntervalOffset = 5.5 - (int)DateTime.FromOADate(seriesFailed.Points[0].XValue).DayOfWeek;
                weekendLine.StripWidth = 2;

                chartArea.AxisX.StripLines.Add(weekendLine);
            }

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates a performance chart for build commands.
        /// </summary>
        /// <param name="id">The identifier of the command to create the chart for.</param>
        /// <param name="counter">The performance counter ID.</param>
        /// <param name="machine">The machine ID.</param>
        /// <returns>Line chart image.</returns>

        public FileResult CommandPerformance (int id, int counter, int machine)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // chart area
            var chartArea = new ChartArea();

            // chart
            var chart = new Chart();

            chart.Height = 384;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);

            // performance counter series
            var buildTimes = dataContext.SP_GetOverallBuildTime(id, 60);
            string currentMachine = "";

            Series s = null;

            foreach (var t in buildTimes)
            {
                if (t.MachineName != currentMachine)
                {
                    if (s != null)
                    {
                        chart.Series.Add(s);
                    }

                    s = new Series();

                    s.ChartType = SeriesChartType.Column;
                    s.Font = chartFont;
                    s.Name = t.MachineName;

                    currentMachine = t.MachineName;
                }

                s.Points.AddXY(t.DateTimeStamp, t.IntValue / 1000);
            }

            if (s != null)
            {
                chart.Series.Add(s);
            }

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates pie chart for the state of all build machines.
        /// </summary>
        /// <returns>Pie chart image.</returns>

        public FileResult CommandsState ()
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = chartFont;

            // branch series
            var branchSeries = new Series();

            branchSeries.BorderColor = Color.DarkGray;
            branchSeries.ChartType = SeriesChartType.Pie;
            branchSeries.Font = chartFont;
            branchSeries.Name = "Commands";
            branchSeries.Label = "#VALY";
            branchSeries.Legend = "Default";

            branchSeries["PieDrawingStyle"] = "Concave";
            branchSeries["PieLabelStyle"] = "Outside";

            var commands = dataContext.SP_GetBuildCommands().ToList();

            int failed = commands.Count(c => !c.BuildStarted.HasValue && (c.LastGoodChangelist != c.LastAttemptedChangelist));
            int building = commands.Count(c => c.BuildStarted.HasValue);

            int f = branchSeries.Points.AddXY("Failed", failed);

            branchSeries.Points[f].Color = redColor;
            branchSeries.Points[f].LegendText = "Failed";

            int b = branchSeries.Points.AddXY("Building", building);

            branchSeries.Points[b].Color = blueColor;
            branchSeries.Points[b].LegendText = "Building";

            int s = branchSeries.Points.AddXY("Succeeded", commands.Count() - failed - building);

            branchSeries.Points[s].Color = greenColor;
            branchSeries.Points[s].LegendText = "Succeeded";

            // chart area
            var chartArea = new ChartArea();

            // chart
            var chart = new Chart();

            chart.Height = 480;
            chart.Width = 480;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(branchSeries);

            return GetChartFile(chart);
        }

        public FileResult DesktopPerformance()
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.IsLabelAutoFit = true;
            chartArea.AxisY.IsStartedFromZero = false;

            // chart
            var chart = new Chart();

            chart.Height = 768;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);

            chart.Series.Add(GetDesktopPerformanceSeries("UCSUDK", 1438, 93));
            chart.Series.Add(GetDesktopPerformanceSeries("UCSExample", 1437, 93));
            chart.Series.Add(GetDesktopPerformanceSeries("UCSFortnite", 1485, 93));
            chart.Series.Add(GetDesktopPerformanceSeries("T7600UDK", 1438, 120));
            chart.Series.Add(GetDesktopPerformanceSeries("T7600Example", 1437, 120));
            chart.Series.Add(GetDesktopPerformanceSeries("T7600Fortnite", 1485, 120));
            chart.Series.Add(GetDesktopPerformanceSeries("Z820UDK", 1438, 122));
            chart.Series.Add(GetDesktopPerformanceSeries("Z820Example", 1437, 122));
            chart.Series.Add(GetDesktopPerformanceSeries("Z820Fortnite", 1485, 122));

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates pie chart for the state of all build machines.
        /// </summary>
        /// <returns>Pie chart image.</returns>

        public FileResult InfrastructureState ()
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = chartFont;

            // machines series
            var machinesSeries = new Series();

            machinesSeries.BorderColor = Color.DarkGray;
            machinesSeries.ChartType = SeriesChartType.Pie;
            machinesSeries.Font = chartFont;
            machinesSeries.Name = "Machines";
            machinesSeries.Label = "#VALY";
            machinesSeries.Legend = "Default";

            machinesSeries["PieDrawingStyle"] = "Concave";
            machinesSeries["PieLabelStyle"] = "Outside";

            foreach (var s in dataContext.SP_GetInfrastructureState().ToList())
            {
                int p = machinesSeries.Points.AddXY(s.State, s.MachineCount);

                machinesSeries.Points[p].Color = GetStateColor(s.State);
                machinesSeries.Points[p].LegendText = s.State;
            }

            // chart area
            var chartArea = new ChartArea();

            // chart
            var chart = new Chart();

            chart.Height = 480;
            chart.Width = 480;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(machinesSeries);

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates the log chart for the specified machine.
        /// </summary>
        /// <param name="id">Machine name.</param>
        /// <returns>Log chart.</returns>

        public FileResult MachineLog (string id)
        {
            if (string.IsNullOrEmpty(id))
            {
                id = "%";
            }

            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // failed builds series
            var seriesFailed = new Series();

            seriesFailed.ChartType = SeriesChartType.StackedColumn;
            seriesFailed.Color = redColor;
            seriesFailed.Font = smallFont;
            seriesFailed.Name = "Failed Commands";
            seriesFailed.IsXValueIndexed = true;
            seriesFailed.ToolTip = "\t= #VALX{d} \n\t= #VALY{f}";
            seriesFailed.Legend = "Default";

            seriesFailed["EmptyPointValue"] = "Zero";

			var FailedCommands = dataContext.SP_GetMachineActivity(id, "Failed", true, 45).ToList();
			var SucceededCommands = dataContext.SP_GetMachineActivity(id, "Succeeded", true, 45).ToList();

			for( int Day = 0; Day < 45; Day++ )
			{
				if( FailedCommands.Count > Day && SucceededCommands.Count > Day )
				{
					var Failed = FailedCommands[Day];
					var Succeeded = SucceededCommands[Day];

					if( Failed.LogCount + Succeeded.LogCount > 0 )
					{
						seriesFailed.Points.AddXY( Failed.BuildStartedDate.Value, ( Failed.LogCount * 100 ) / ( Failed.LogCount + Succeeded.LogCount ) );
					}
				}
			}

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.Interval = 4.0;
            chartArea.AxisX.IsLabelAutoFit = false;
            chartArea.AxisX.LabelStyle.Enabled = true;
            chartArea.AxisX.LabelStyle.Font = smallFont;
            chartArea.AxisX.MajorGrid.Enabled = false;
            chartArea.AxisX.MajorGrid.LineColor = Color.Gray;
            chartArea.AxisX.MajorGrid.Interval = 7;
            chartArea.AxisX.MinorGrid.Enabled = false;
            chartArea.AxisX.MinorGrid.LineColor = Color.Silver;
            chartArea.AxisX.MinorGrid.Interval = 1;
            chartArea.AxisX.MinorGrid.IntervalOffset = 0.5;
            chartArea.AxisY.IsLogarithmic = false;
            // chart
            var chart = new Chart();

            chart.Height = 480;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(seriesFailed);

            // InsertEmptyPoints is broken for two or more series; gotta do it manually
            //chart.DataManipulator.InsertEmptyPoints(45, IntervalType.Days, "Command Failed, Command Succeeded");
            AlignChartSeries(chart);

            // highlight weekends
            if (seriesFailed.Points.Count > 0)
            {
                var weekendLine = new StripLine();

                weekendLine.BackColor = Color.Gainsboro;
                weekendLine.Interval = 7;
                weekendLine.IntervalOffset = 5.5 - (int)DateTime.FromOADate(seriesFailed.Points[0].XValue).DayOfWeek;
                weekendLine.StripWidth = 2;

                chartArea.AxisX.StripLines.Add(weekendLine);
            }

            return GetChartFile(chart);
        }

        public FileResult PerforceSizes (int? numWeeks)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // submitted gigabytes series
            var seriesSubmittedSizes = new Series();

            seriesSubmittedSizes.ChartType = SeriesChartType.Line;
            seriesSubmittedSizes.Font = smallFont;
            seriesSubmittedSizes.Name = "Submitted Gigabytes";
            seriesSubmittedSizes.IsXValueIndexed = true;
            seriesSubmittedSizes.Legend = "Default";

            seriesSubmittedSizes["EmptyPointValue"] = "Zero";

            foreach (var s in dataContext.SP_GetPerforceSize(numWeeks.GetValueOrDefault(80)).ToList())
            {
                seriesSubmittedSizes.Points.AddXY(s.WeekOfYear, s.SubmittedBytes / (1024.0 * 1024.0 * 1024.0));
            }

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.Interval = 5;
            chartArea.AxisX.IsStartedFromZero = true;
            chartArea.AxisY.IsStartedFromZero = false;

            // chart
            var chart = new Chart();

            chart.Height = 384;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(seriesSubmittedSizes);

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates the log chart for UE4 build times.
        /// </summary>
        /// <returns>Build times chart.</returns>

        public FileResult UE4BuildTimes (int counter, int days)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // successful builds series
            var timesSeries = new Series();

            timesSeries.ChartType = SeriesChartType.Line;
            timesSeries.Color = greenColor;
            timesSeries.Font = chartFont;
            timesSeries.Name = "Changelist";
            timesSeries.ToolTip = "";
            timesSeries.Legend = "Default";

            foreach (var d in dataContext.SP_GetPerformanceData(counter, days).OrderBy(x => x.Changelist).ToList())
            {
                if (d.MachineID == 93) // BUILD-07
                {
                    int p = timesSeries.Points.AddXY(d.Changelist, d.IntValue / 1000);

                    //timesSeries.Points[p].ToolTip = d.Changelist.ToString();
                }              
            }

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.Interval = 2500;
            chartArea.AxisX.IsLabelAutoFit = false;
            chartArea.AxisX.LabelStyle.Enabled = true;
            chartArea.AxisX.LabelStyle.Angle = 45;
            chartArea.AxisX.MajorGrid.Enabled = true;
            chartArea.AxisX.MajorGrid.LineColor = Color.Gray;
            chartArea.AxisX.MajorGrid.LineDashStyle = ChartDashStyle.Dot;

            // chart
            var chart = new Chart();

            chart.Height = 512;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(timesSeries);

            return GetChartFile(chart);
        }

        /// <summary>
        /// Creates the changelist submission chart for the specified user.
        /// </summary>
        /// <param name="id">User name.</param>
        /// <param name="days">Number of days of historical data.</param>
        /// <returns>Changelist submission chart.</returns>

        public FileResult UserChangelists (string id, int days)
        {
            // legend
            var legend = new Legend("Default");

            legend.Alignment = StringAlignment.Far;
            legend.Docking = Docking.Bottom;
            legend.Font = smallFont;

            // submissions series
            var seriesSubmissions = new Series();

            seriesSubmissions.ChartType = SeriesChartType.StackedColumn;
            seriesSubmissions.Color = blueColor;
            seriesSubmissions.Font = smallFont;
            seriesSubmissions.Name = "Check-ins";
            seriesSubmissions.IsXValueIndexed = true;
            seriesSubmissions.Legend = "Default";

            seriesSubmissions["EmptyPointValue"] = "Zero";

            foreach (var b in dataContext.SP_GetUserActivity(id.Replace(".", "_"), days).ToList())
            {
                seriesSubmissions.Points.AddXY(b.TimeStamp.Value, b.ChangelistCount);
            }

            // chart area
            var chartArea = new ChartArea();

            chartArea.AxisX.Interval = 4.0;
            chartArea.AxisX.IsLabelAutoFit = false;
            chartArea.AxisX.LabelStyle.Enabled = true;
            chartArea.AxisX.LabelStyle.Font = smallFont;
            chartArea.AxisX.MajorGrid.Enabled = false;
            chartArea.AxisX.MajorGrid.LineColor = Color.Gray;
            chartArea.AxisX.MajorGrid.Interval = 7;
            chartArea.AxisX.MinorGrid.Enabled = false;
            chartArea.AxisX.MinorGrid.LineColor = Color.Silver;
            chartArea.AxisX.MinorGrid.Interval = 1;
            chartArea.AxisX.MinorGrid.IntervalOffset = 0.5;

            // chart
            var chart = new Chart();

            chart.Height = 320;
            chart.Width = 1024;

            chart.ChartAreas.Add(chartArea);
            chart.Legends.Add(legend);
            chart.Series.Add(seriesSubmissions);

            if (seriesSubmissions.Points.Count() > 0)
            {
                chart.DataManipulator.InsertEmptyPoints(1, IntervalType.Days, "Check-ins");
            }

            // highlight weekends
            if (seriesSubmissions.Points.Count > 0)
            {
                var weekendLine = new StripLine();

                weekendLine.BackColor = Color.Gainsboro;
                weekendLine.Interval = 7;
                weekendLine.IntervalOffset = 5.5 - (int)DateTime.FromOADate(seriesSubmissions.Points[0].XValue).DayOfWeek;
                weekendLine.StripWidth = 2;

                chartArea.AxisX.StripLines.Add(weekendLine);
            }

            return GetChartFile(chart);
        }

        #endregion


        #region Implementation

        /// <summary>
        /// Aligns all series in the given chart by inserting empty data points.
        /// </summary>
        /// <remarks>
        /// chart.DataManipulator.InsertEmptyPoints() appears to be broken in some cases for
        /// two or more series, so we are aligning them manually using this function here.
        /// </remarks>
        /// <param name="chart">The chart to align.</param>

        protected void AlignChartSeries (Chart chart)
        {
            var xvals = new List<double>();

            foreach (var s in chart.Series)
            {
                foreach (var p in s.Points)
                {
                    if (!xvals.Contains(p.XValue))
                    {
                        xvals.Add(p.XValue);
                    }
                }
            }

            foreach (var x in xvals)
            {
                foreach (var s in chart.Series)
                {
                    try
                    {
                        s.Points.FindByValue(x, "X").ToString();
                    }
                    catch
                    {
                        s.Points.AddXY(x, 0);
                    }
                }
            }

            foreach (var s in chart.Series)
            {
                s.Sort(PointSortOrder.Ascending, "X");
            }
        }

        /// <summary>
        /// Generates a chart series for the number of builds per day.
        /// </summary>
        /// <param name="name">The name of the series to generate.</param>
        /// <param name="numDays">The number of days to track.</param>
        /// <param name="commandType">The type of command to track.</param>
        /// <param name="exceptions">Whether certain builds should be excluded (see stored procedure for details).</param>
        /// <returns></returns>

        protected Series GetBuildsPerDaySeries(string name, int numDays, string commandType, bool exceptions)
        {
            var result = new Series();

            result.ChartType = SeriesChartType.StackedArea;
            result.Font = smallFont;
            result.Name = name;
            result.IsXValueIndexed = true;
            result.Legend = "Default";

            result["EmptyPointValue"] = "Zero";

            foreach (var b in dataContext.SP_GetBuildsPerDay(numDays, commandType, exceptions).ToList())
            {
                result.Points.AddXY(b.BuildDate.Value.Date, b.CommandCount);
            }

            return result;
        }

        /// <summary>
        /// Generates a chart series for Fortnite's desktop performance since 8/31/2012.
        /// </summary>
        /// <param name="seriesName">The name of the series to generate.</param>
        /// <param name="counterId">The identifier of the performance counter.</param>
        /// <param name="machineId">The identifier of the machine that genreated the data.</param>

        protected Series GetDesktopPerformanceSeries(string seriesName, int counterId, int machineId)
        {
            var result = new Series();

            result.ChartType = SeriesChartType.Line;
            result.Font = smallFont;
            result.Name = seriesName;
            result.Legend = "Default";

            foreach (var p in dataContext.SP_GetDesktopPerformance(counterId, machineId))
            {
                result.Points.AddXY(p.Changelist, p.Performance);
            }

            return result;
        }

        /// <summary>
        /// Renders the specified chart to a file result.
        /// </summary>
        /// <param name="chart">The chart to create a file result for.</param>
        /// <returns>File result containing the chart.</returns>

        protected FileResult GetChartFile (Chart chart)
        {
            if (chart == null)
            {
                return null;
            }

            MemoryStream ms = new MemoryStream();

            chart.SaveImage(ms);

            return File(ms.GetBuffer(), @"image/png");
        }

        protected Color GetStateColor (string stateName)
        {
            if (stateName == "Building")
            {
                return greenColor;
            }

            if (stateName == "Dead")
            {
                return orangeColor;
            }

            if (stateName == "Zombied")
            {
                return ColorTranslator.FromHtml("#ff3333");
            }

            return blueColor;
        }

        #endregion
    }
}
