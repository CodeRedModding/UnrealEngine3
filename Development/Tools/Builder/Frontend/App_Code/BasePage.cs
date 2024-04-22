// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.Configuration;
using System.Data.SqlClient;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

public class BasePage : System.Web.UI.Page
{
    protected void Update( SqlConnection Connection, string CommandString )
    {
        try
        {
			using( SqlCommand Command = new SqlCommand( CommandString, Connection ) )
			{
				Command.ExecuteNonQuery();
			}
        }
        catch
        {
            System.Diagnostics.Debug.WriteLine( "Exception in Update" );
        }
    }

    protected int ReadInt( SqlConnection Connection, string CommandString )
    {
        int Result = 0;

        try
        {
			using( SqlCommand Command = new SqlCommand( CommandString, Connection ) )
			{
				SqlDataReader DataReader = Command.ExecuteReader();
				if( DataReader.Read() )
				{
					Result = DataReader.GetInt32( 0 );
				}
				DataReader.Close();
			}
        }
        catch
        {
            System.Diagnostics.Debug.WriteLine( "Exception in ReadInt" );
        }

        return ( Result );
    }

    protected int ReadIntSP( SqlConnection Connection, string StoredProcedure )
    {
        int Result = 0;

        try
        {
			using( SqlCommand Command = new SqlCommand( StoredProcedure, Connection ) )
			{
				Command.CommandType = CommandType.StoredProcedure;

				SqlDataReader DataReader = Command.ExecuteReader();
				if( DataReader.Read() )
				{
					Result = DataReader.GetInt32( 0 );
				}
				DataReader.Close();
			}
        }
        catch
        {
            System.Diagnostics.Debug.WriteLine( "Exception in ReadIntSP" );
        }

        return ( Result );
    }

    protected string ReadString( SqlConnection Connection, string CommandString )
    {
        string Result = "";

        try
        {
			using( SqlCommand Command = new SqlCommand( CommandString, Connection ) )
			{
				SqlDataReader DataReader = Command.ExecuteReader();
				if( DataReader.Read() )
				{
					Result = DataReader.GetString( 0 );
				}
				DataReader.Close();
			}
        }   
        catch
        {
            System.Diagnostics.Debug.WriteLine( "Exception in ReadString" );
        }

        return( Result );
    }

    protected DateTime ReadDateTime( SqlConnection Connection, string CommandString )
    {
        DateTime Result = DateTime.UtcNow;

        try
        {
			using( SqlCommand Command = new SqlCommand( CommandString, Connection ) )
			{
				SqlDataReader DataReader = Command.ExecuteReader();
				if( DataReader.Read() )
				{
					Result = DataReader.GetDateTime( 0 );
				}
				DataReader.Close();
			}
        }
        catch
        {
            System.Diagnostics.Debug.WriteLine( "Exception in ReadDateTime" );
        }

        return ( Result );
    }

    protected void BuilderDBRepeater_ItemCommand( object source, RepeaterCommandEventArgs e )
    {
        if( e.Item != null )
        {
			using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
			{
				Connection.Open();

				// Find the command id that matches the description
				int CommandID = Int32.Parse( ( string )e.CommandName );
				if( CommandID != 0 )
				{
					// Make sure it's not already running
					string MachineQuery = "SELECT Machine FROM Commands WHERE ( ID = " + CommandID.ToString() + " )";
					string MachineName = ReadString( Connection, MachineQuery );
					if( MachineName.Length == 0 )
					{
						string User = Context.User.Identity.Name;
						int Offset = User.LastIndexOf( '\\' );
						if( Offset >= 0 )
						{
							User = User.Substring( Offset + 1 );
						}

						string CommandString = "UPDATE Commands SET Pending = 1, Operator = '" + User + "' WHERE ( ID = " + CommandID.ToString() + " )";
						Update( Connection, CommandString );
					}
				}

				Connection.Close();
				Response.Redirect( "Default.aspx" );
			}
        }
    }

	protected void SimpleTriggerBuild( string BuildType )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();

			string User = Context.User.Identity.Name;
			int Offset = User.LastIndexOf( '\\' );
			if( Offset >= 0 )
			{
				User = User.Substring( Offset + 1 );
			}

			string CommandString = "UPDATE Commands SET Pending = 1, Operator = '" + User + "' WHERE ( Description = '" + BuildType + "' )";
			Update( Connection, CommandString );

			Connection.Close();
		}
	}

	protected internal void BuilderDBRepeater_OnPreRender( object Source, EventArgs e )
	{
		// If this object is a Button, check for our disable case
		Button SourceAsButton = Source as Button;
		if( ( SourceAsButton != null ) &&
			( SourceAsButton.CommandArgument != "" ) )
		{
			SourceAsButton.Enabled = false;
		}
	}

	protected void RemoveOutliers( DataTable Table )
	{
        if (Table.Rows.Count > 1)
        {
            // Create a simple rolling average
            List<Int64> RollingAverage = new List<Int64>();
            Int64 Seed = (Int64)Table.Rows[0].ItemArray[1];
            Seed += (Int64)Table.Rows[1].ItemArray[1];
            Seed /= 2;
            RollingAverage.Add(Seed);
            RollingAverage.Add(Seed);

            for (int RowIndex = 2; RowIndex < Table.Rows.Count - 2; RowIndex++)
            {
                Seed += (Int64)Table.Rows[RowIndex - 2].ItemArray[1];
                Seed += (Int64)Table.Rows[RowIndex - 1].ItemArray[1];
                Seed += (Int64)Table.Rows[RowIndex].ItemArray[1];
                Seed += (Int64)Table.Rows[RowIndex + 1].ItemArray[1];
                Seed += (Int64)Table.Rows[RowIndex + 2].ItemArray[1];
                Seed /= 6;
                RollingAverage.Add(Seed);
            }

            RollingAverage.Add(Seed);
            RollingAverage.Add(Seed);

            // Remove numbers that aren't within range of the rolling average
            DataRow LastRow = Table.Rows[0];
            for (int RowIndex = 1; RowIndex < Table.Rows.Count - 1; RowIndex++)
            {
                DataRow CurrentRow = Table.Rows[RowIndex];

                if ((Int64)CurrentRow.ItemArray[1] > RollingAverage[RowIndex] * 1.5)
                {
                    Table.Rows.RemoveAt(RowIndex);
                    RollingAverage.RemoveAt(RowIndex);
                    RowIndex--;
                    continue;
                }

                if ((Int64)CurrentRow.ItemArray[1] < RollingAverage[RowIndex] / 1.5)
                {
                    Table.Rows.RemoveAt(RowIndex);
                    RollingAverage.RemoveAt(RowIndex);
                    RowIndex--;
                    continue;
                }

                LastRow = CurrentRow;
            }
        }
	}
}
