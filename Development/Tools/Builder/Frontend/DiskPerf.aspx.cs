// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class DiskPerf : BasePage
{
	private void FillSeries( SqlConnection Connection, string Series, int MachineID, int CounterID )
	{
		// 1240, 1248 and 1242
		string Query = "SELECT DateTimeStamp, ( IntValue / 1000 ) AS Duration FROM PerformanceData";
		Query += " WHERE CounterID = " + CounterID.ToString() + " AND MachineID = " + MachineID.ToString() + " AND DATEDIFF( day, DateTimeStamp, GETDATE() ) < 8";
		Query += " ORDER BY DateTimeStamp";

		using( SqlCommand Command = new SqlCommand( Query, Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				CommandCountChart.Series[Series].Points.DataBindXY( Table.Rows, "DateTimeStamp", Table.Rows, "Duration" );
			}

			Reader.Close();
		}
	}
	
	protected void Page_Load( object sender, EventArgs e )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();
			FillSeries( Connection, "DiskIsilon1", 103, 1240 );
			FillSeries( Connection, "DiskIsilon2", 103, 1248 );
			FillSeries( Connection, "DiskIsilon3", 103, 1245 );

			FillSeries( Connection, "DiskSAN1", 104, 1240 );
			FillSeries( Connection, "DiskSAN2", 104, 1248 );
			FillSeries( Connection, "DiskSAN3", 104, 1245 );

			FillSeries( Connection, "DiskSATA1", 106, 1240 );
			FillSeries( Connection, "DiskSATA2", 106, 1248 );
			FillSeries( Connection, "DiskSATA3", 106, 1245 );
			Connection.Close();
		}
	}
}
