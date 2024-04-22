// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class JackDVDSize : BasePage
{
	private void FillSeries( SqlConnection Connection, string Item, int CounterID )
	{
		using( SqlCommand Command = new SqlCommand( "SELECT DateTimeStamp, IntValue / ( 1024 * 1024 ) AS '" + Item + "' FROM PerformanceData " +
													"WHERE ( CounterID = " + CounterID.ToString() + " ) AND ( DATEDIFF( day, DateTimeStamp, GETDATE() ) < 90 ) " +
													"ORDER BY DateTimeStamp DESC", Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			DataTable Table = new DataTable();

			Table.Load( Reader );
			JackDVDSizeChart.Series[Item].Points.DataBindXY( Table.Rows, "DateTimeStamp", Table.Rows, Item );

			Reader.Close();
		}
	}
	
	protected void Page_Load( object sender, EventArgs e )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();
			FillSeries( Connection, "Xbox360DVDCapacity", 969 );
			FillSeries( Connection, "Jack(INT.FRA.ESM)[CZE.HUN.NOR.DAN.FIN]", 1772 );
			FillSeries( Connection, "Jack(INT.FRA.ESM)[CZE.HUN.NOR.DAN.FIN.DUT]", 1786 );
			FillSeries( Connection, "Jack(INT.FRA.ESM)[POL.CZE.HUN.KOR.CHN.NOR.DAN.FIN.DUT]", 1794 );
			FillSeries( Connection, "Jack(INT.JPN.PTB)", 1773 );
			FillSeries( Connection, "Jack(INT.FRA.DEU)", 1776 );
			FillSeries( Connection, "Jack(INT.ESN.ITA)", 1777 );
			FillSeries( Connection, "Jack(INT.ITA.ESN)", 1795 );
			FillSeries( Connection, "Jack(INT.POL.RUS)", 1778 );
			FillSeries( Connection, "Jack(INT.RUS)", 1796 );
			Connection.Close();
		}
	}
}
