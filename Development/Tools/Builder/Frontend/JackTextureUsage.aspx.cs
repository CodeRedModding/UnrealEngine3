// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Web;
using System.Web.UI;
using System.Web.UI.DataVisualization.Charting;
using System.Web.UI.WebControls;

public partial class JackTextureUsage : BasePage
{
	Dictionary<string,int> FileSizes = new Dictionary<string,int>();

	private void GetFileSizes( SqlConnection Connection )
	{
		string Query = "SELECT PackageName, UncompressedSize, CompressedSize FROM Packages";
		using( SqlCommand Command = new SqlCommand( Query, Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				foreach( DataRow Row in Table.Rows )
				{
					string FileName = ( ( string )Row.ItemArray[0] ).Trim();
					int FileSize = ( Int32 )Row.ItemArray[1];
					int CompressedFileSize = ( Int32 )Row.ItemArray[2];

					FileSizes[FileName] = FileSize;
					if( CompressedFileSize != -1 )
					{
						FileSizes[FileName] = CompressedFileSize;
					}
				}
			}
			Reader.Close();
		}
	}

	private Dictionary<string, int> GetObjectCounts( SqlConnection Connection, string Item )
	{
		Dictionary<string, int> ObjectCounts = new Dictionary<string, int>();
		string Query = "SELECT COUNT( Objects.ObjectName ), Objects.ObjectName FROM Objects " +
						"INNER JOIN Exports ON Objects.ObjectID = Exports.ObjectID " +
						"INNER JOIN Classes ON Objects.ClassID = Classes.ClassID " +
						"WHERE Classes.ClassName = '" + Item + "' AND LEFT( Objects.ObjectName, 4 ) = 'TFC_' " +
						"GROUP BY Objects.ObjectName";

		using( SqlCommand Command = new SqlCommand( Query, Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				foreach( DataRow Row in Table.Rows )
				{
					int Count = ( Int32 )Row.ItemArray[0];
					string ObjectName = ( string )Row.ItemArray[1];

					ObjectCounts[ObjectName.Trim()] = Count;
				}
			}

			Reader.Close();
		}

		return ( ObjectCounts );
	}

	private Dictionary<string, float> GetPackageTotals( SqlConnection Connection, Dictionary<string, int> ObjectCounts, string Item )
	{
		Dictionary<string, float> PackageTotals = new Dictionary<string, float>();
		string Query = "SELECT Exports.Size, Objects.ObjectName, Packages.PackageName FROM Objects " +
						"INNER JOIN Exports ON Objects.ObjectID = Exports.ObjectID " +
						"INNER JOIN Packages ON Exports.PackageID = Packages.PackageID " +
						"INNER JOIN Classes ON Objects.ClassID = Classes.ClassID " +
						"WHERE Classes.ClassName = '" + Item + "' AND LEFT( Objects.ObjectName, 4 ) = 'TFC_' " +
						"ORDER BY Packages.PackageName";

		using( SqlCommand Command = new SqlCommand( Query, Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				foreach( DataRow Row in Table.Rows )
				{
					long Size = ( Int64 )Row.ItemArray[0];
					string ObjectName = ( ( string )Row.ItemArray[1] ).Trim();
					string PackageName = ( ( string )Row.ItemArray[2] ).Trim();
					float FloatValue = 0.0f;

					int ObjectCount = ObjectCounts[ObjectName];
					if( !PackageTotals.TryGetValue( PackageName, out FloatValue ) )
					{
						PackageTotals[PackageName] = 0.0f;
					}

					PackageTotals[PackageName] += ( float )Size / ( ( float )ObjectCount * 1024.0f * 1024.0f );
				}
			}

			Reader.Close();
		}

		return ( PackageTotals );
	}

	private Dictionary<string, float> GetLightingTotals( SqlConnection Connection, Dictionary<string, float> OldPackageTotals, string Item )
	{
		// Clear out existing data
		Dictionary<string, float> PackageTotals = new Dictionary<string, float>();
		foreach( string Key in OldPackageTotals.Keys )
		{
			PackageTotals[Key] = 0.0f;
		}

		// Grab the data from the database
		string Query = "SELECT SUM( Exports.Size ) / ( 1024.0 * 1024.0 ) AS TotalSize, Packages.PackageName FROM Objects " +
						"INNER JOIN Exports ON Objects.ObjectID = Exports.ObjectID " +
						"INNER JOIN Packages ON Exports.PackageID = Packages.PackageID " +
						"INNER JOIN Classes ON Objects.ClassID = Classes.ClassID " +
						"WHERE Classes.ClassName = '" + Item + "' " +
						"GROUP BY Packages.PackageName " +
						"ORDER BY Packages.PackageName";

		using( SqlCommand Command = new SqlCommand( Query, Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				DataTable Table = new DataTable();
				Table.Load( Reader );

				foreach( DataRow Row in Table.Rows )
				{
					DataPoint Point = new DataPoint();

					string XAxis = ( ( string )Row.ItemArray[1] ).Trim();
					decimal YValue = ( decimal )Row.ItemArray[0];
					float FloatValue = ( float )YValue;

					if( PackageTotals.TryGetValue( XAxis, out FloatValue ) )
					{
						PackageTotals[XAxis] = ( float )YValue;
					}
				}
			}

			Reader.Close();
		}

		return ( PackageTotals );
	}

	private void FillSeries( string Item, Dictionary<string, float> PackageTotals )
	{
		foreach( string Package in PackageTotals.Keys )
		{
			DataPoint Point = new DataPoint();

			Point.AxisLabel = Package;
			Point.YValues[0] = PackageTotals[Package];

			JackTextureUsageChart.Series[Item].Points.Add( Point );
		}
	}

	protected void Page_Load( object sender, EventArgs e )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["EngineDbConnectionString"].ConnectionString ) )
		{
			Connection.Open();

			Dictionary<string, int> ObjectCounts = GetObjectCounts( Connection, "Texture2D" );

			// Calculate the weighted average of texture usage per package; if a texture is used in 3 packages, each package gets a third of the size
			Dictionary<string, float> PackageTotals = GetPackageTotals( Connection, ObjectCounts, "Texture2D" );
			FillSeries( "Texture2D", PackageTotals );

			// Calculate the lighting texture usage - this is unique per map
			Dictionary<string, float> LightMapTotals = GetLightingTotals( Connection, PackageTotals, "LightMapTexture2D" );
			FillSeries( "LightMapTexture2D", LightMapTotals );

			// Calculate the shadow texture usage - this is unique per map
			Dictionary<string, float> ShadowMapTotals = GetLightingTotals( Connection, PackageTotals, "ShadowMapTexture2D" );
			FillSeries( "ShadowMapTexture2D", ShadowMapTotals );

			// Get the package sizes for the csv file
			GetFileSizes( Connection );

			Connection.Close();
		}

		JackTextureUsageChart.ChartAreas["ChartArea1"].Position.X = 0.0f;
		JackTextureUsageChart.ChartAreas["ChartArea1"].Position.Y = 0.0f;
		JackTextureUsageChart.ChartAreas["ChartArea1"].Position.Width = 100.0f;
		JackTextureUsageChart.ChartAreas["ChartArea1"].Position.Height = 100.0f;
	}

	private Dictionary<string, float> GetValues( string SeriesName )
	{
		Dictionary<string, float> Values = new Dictionary<string, float>();
		foreach( DataPoint Point in JackTextureUsageChart.Series[SeriesName].Points )
		{
			Values[Point.AxisLabel] = ( float )Point.YValues[0];
		}

		return ( Values );
	}

	protected void Button_SaveAsCSV_Click( object sender, EventArgs e )
	{
		Response.Clear();
		Response.Output.WriteLine( "Map, WeightedTextureSize, LightMapTextureSize, ShadowMapTextureSize, FileSize" );

		Dictionary<string, float> Textures = GetValues( "Texture2D" );
		Dictionary<string, float> LightMaps = GetValues( "LightMapTexture2D" );
		Dictionary<string, float> ShadowMaps = GetValues( "ShadowMapTexture2D" );

		foreach( string Key in FileSizes.Keys )
		{
			float TextureSize = 0.0f;
			float LightMapSize = 0.0f;
			float ShadowMapSize = 0.0f;
			float FileSize = ( float )( FileSizes[Key] / ( 1024.0f * 1024.0f ) );
			Textures.TryGetValue( Key, out TextureSize );
			LightMaps.TryGetValue( Key, out LightMapSize );
			ShadowMaps.TryGetValue( Key, out ShadowMapSize );

			Response.Output.WriteLine( Key + ", "
									+ TextureSize.ToString() + ", "
									+ LightMapSize.ToString() + ", "
									+ ShadowMapSize.ToString() + ", "
									+ FileSize.ToString() );
		}

		Response.AddHeader( "Content-Disposition", "attachment; filename=\"JackTextureUsage.csv\"" );
		Response.Flush();
		Response.End();
	}
}
