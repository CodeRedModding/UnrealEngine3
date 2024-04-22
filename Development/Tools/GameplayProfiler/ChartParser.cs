/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms.DataVisualization;
using System.Windows.Forms.DataVisualization.Charting;
using System.Drawing;

// Microsoft Chart Controls Add-on for Microsoft Visual Studio 2008
//
// http://www.microsoft.com/downloads/details.aspx?familyid=1D69CE13-E1E5-4315-825C-F14D33A303E9&displaylang=en

namespace GameplayProfiler
{
	class ChartParser
	{
		/**
		 * Parses passed in stream's frames into chart to visualize frame and tracked time.
		 */
		public static void Parse( Chart FPSChart, ProfilerStream ProfilerStream )
		{
			var StartTime = DateTime.UtcNow;

			FPSChart.BeginInit();

			// Reset existing data.
			foreach( var Series in FPSChart.Series )
			{
				Series.Points.Clear();
			}
			FPSChart.ResetAutoValues();
			FPSChart.Invalidate();

			// Add frame time and tracked time stats. We only track actor & component tick and script
			// function calls so there might be a noticeable divergence. There is also tracking related
			// overhead that is mostly in the untracked section so actual time spent in TrackedTime is
			// close to being correct.

            Series FrameTimeSeries = FPSChart.Series.FindByName("FrameTime");
            Series TrackedTimeSeries = FPSChart.Series.FindByName("TrackedTime");
			for( int FrameIndex=0; FrameIndex<ProfilerStream.Frames.Count; FrameIndex++ )
			{
				var Frame = ProfilerStream.Frames[FrameIndex];
				FrameTimeSeries.Points.AddXY( FrameIndex, Frame.FrameTime );
				TrackedTimeSeries.Points.AddXY( FrameIndex, Frame.TrackedTime );
			}

			// Let the system deal with automatically scaling the axis.
			FPSChart.ChartAreas["DefaultChartArea"].RecalculateAxesScale();
			FPSChart.EndInit();

			Console.WriteLine("Adding data to chart took {0} seconds", (DateTime.UtcNow - StartTime).TotalSeconds);
		}

        public static void ShowFunctionGraph(Chart FPSChart, ProfilerStream ProfilerStream, HashSet<string> FunctionNames)
        {
            HideFunctionGraph(FPSChart);

            // Build a new one
            if (FunctionNames.Count != 0)
            {
                Series InclSeries = new Series("SelectedTimeIncl");
                InclSeries.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line;
                InclSeries.Color = Color.Red;
                InclSeries.ToolTip = "Inclusive time in selection (ms)";

                Series ExclSeries = new Series("SelectedTimeExcl");
                ExclSeries.ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line;
                ExclSeries.Color = Color.DarkRed;
                ExclSeries.ToolTip = "Exclusive time in selection (ms)";

			    for( int FrameIndex=0; FrameIndex<ProfilerStream.Frames.Count; FrameIndex++ )
			    {
                    double X = FrameIndex;
                    double InclY = 0.0;
                    double ExclY = 0.0;
				    var Frame = ProfilerStream.Frames[FrameIndex];

                    foreach (string FunctionName in FunctionNames)
                    {
                        FunctionInfo Info;
                        if (Frame.NameToFunctionInfoMap.TryGetValue(FunctionName, out Info))
                        {
                            InclY += Info.InclusiveTime;
                            ExclY += Info.InclusiveTime - Info.ChildrenTime;
                        }
                    }

                    InclSeries.Points.AddXY(X, InclY);
                    ExclSeries.Points.AddXY(X, ExclY);
                }

                FPSChart.Series.Add(InclSeries);
                FPSChart.Series.Add(ExclSeries);
            }

            FPSChart.Invalidate();
        }

        public static void HideFunctionGraph(Chart FPSChart)
        {
            // Remove the chart
            Series InclSeries = FPSChart.Series.FindByName("SelectedTimeIncl");
            Series ExclSeries = FPSChart.Series.FindByName("SelectedTimeExcl");
            if (InclSeries != null)
            {
                FPSChart.Series.Remove(InclSeries);
            }
            if (ExclSeries != null)
            {
                FPSChart.Series.Remove(ExclSeries);
            }
        }

	}
}
