// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class ActiveBuildCounts : BasePage
{
	private void FillSeries( SqlConnection Connection, string Item, int CounterID )
	{
		string MinuteOfDayQuery = "DATEDIFF( minute, FLOOR( CAST( DATEADD( hour, DATEDIFF( hour, GETUTCDATE(), GETDATE() ), DateTimeStamp ) AS FLOAT ) ), DATEADD( hour, DATEDIFF( hour, GETUTCDATE(), GETDATE() ), DateTimeStamp ) )";
		using( SqlCommand Command = new SqlCommand(
			"SELECT DATEADD( minute, " + MinuteOfDayQuery + ", '1970-01-01' ) AS MinuteOfDay, AVG( CAST( IntValue AS FLOAT ) ) AS " + Item + " " +
			"FROM PerformanceData " +
			"WHERE ( CounterID = "+ CounterID.ToString() + " ) AND ( DATEDIFF( day, DateTimeStamp, GETDATE() ) < 60 ) " +
			"GROUP BY " + MinuteOfDayQuery +
			"ORDER BY MinuteOfDay",	Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				BuildCount.Series[Item].Points.DataBindXY( Table.Rows, "MinuteOfDay", Table.Rows, Item );
			}
			Reader.Close();
		}
	}

	protected void Page_Load( object sender, EventArgs e )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();
			FillSeries( Connection, "PendingPrimary", 398 );
			FillSeries( Connection, "ActivePrimary", 397 );
			FillSeries( Connection, "ActiveNonPrimary", 399 );
			FillSeries( Connection, "PendingNonPrimary", 400 );
			Connection.Close();
		}
	}
}
