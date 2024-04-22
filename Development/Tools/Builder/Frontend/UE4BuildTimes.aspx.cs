// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class UE4BuildTimes : BasePage
{
	private void FillSeries( SqlConnection Connection, string Series, string Item, int CounterID, int MachineID )
	{
		using( SqlCommand Command = new SqlCommand( "SELECT Changelist, IntValue / 1000 AS " + Item + ", DateTimeStamp FROM PerformanceData " +
													"WHERE ( CounterID = " + CounterID.ToString() + " ) AND ( MachineID = " + MachineID.ToString() + " ) AND ( DATEDIFF( day, '8/31/2012', DateTimeStamp ) >= 0 ) " +
													"ORDER BY Changelist", Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				RemoveOutliers( Table );

				UE4CompileChart.Series[Series].Points.DataBindXY( Table.Rows, "Changelist", Table.Rows, Item );
			}

			Reader.Close();
		}
	}

	protected void Page_Load( object sender, EventArgs e )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();

			FillSeries( Connection, "UE4 ExampleGameA", "UE4UdkWin64", 1437, 93 );
			FillSeries( Connection, "UE4 ExampleGame", "UE4UdkWin64", 1437, 94 );
			FillSeries( Connection, "UE4 UDKGame", "UE4UdkWin64", 1438, 93 );
			FillSeries( Connection, "UE4 FortniteGame", "UE4UdkWin64", 1485, 93 );

			Connection.Close();
		}
	}
}