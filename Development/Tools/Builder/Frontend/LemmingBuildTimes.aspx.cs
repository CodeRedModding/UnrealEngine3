// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class LemmingBuildTimes : BasePage
{
    private void FillSeries(SqlConnection Connection, string Series, string Item, int CounterID)
    {
        using (SqlCommand Command = new SqlCommand("SELECT DateTimeStamp, IntValue / 1000 AS " + Item + " FROM PerformanceData " +
                                                    "WHERE ( CounterID = " + CounterID.ToString() + " ) AND ( DATEDIFF( day, DateTimeStamp, GETDATE() ) < 60 ) " +
                                                    "ORDER BY DateTimeStamp DESC", Connection))
        {
            SqlDataReader Reader = Command.ExecuteReader();
            if (Reader.HasRows)
            {
                DataTable Table = new DataTable();
                Table.Load(Reader);

                RemoveOutliers(Table);

                LemmingCompileChart.Series[Series].Points.DataBindXY(Table.Rows, "DateTimeStamp", Table.Rows, Item);
            }

            Reader.Close();
        }
    }

    protected void Page_Load(object sender, EventArgs e)
    {
        using (SqlConnection Connection = new SqlConnection(ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString))
        {
            Connection.Open();
            FillSeries(Connection, "Lemming Example Win64", "LemmingExampleWin64", 1394);
            FillSeries(Connection, "Lemming GearGame Win64", "LemmingGearGameWin64", 1389);
            FillSeries(Connection, "Lemming UDK Win64", "LemmingUdkWin64", 1391);
            Connection.Close();
        }
    }
}
