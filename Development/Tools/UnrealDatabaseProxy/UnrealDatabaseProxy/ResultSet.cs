/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Data;

namespace UnrealDatabaseProxy
{
	/// <summary>
	/// Wraps a <see cref="DataTable"/> so that it can be read from remotely.
	/// </summary>
	public class ResultSet : IDisposable
	{
		DataTable mTable;
		int mCurRow;

		/// <summary>
		/// Returns True if the entire result set has been read.
		/// </summary>
		public bool IsAtEnd
		{
			get { return mCurRow >= mTable.Rows.Count; }
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="Table">The table to wrap.</param>
		public ResultSet( DataTable Table )
		{
			if( Table == null )
			{
				throw new ArgumentNullException( "Table" );
			}

			mTable = Table;
		}

		#region IDisposable Members

		/// <summary>
		/// Cleans up resources.
		/// </summary>
		public void Dispose()
		{
			mTable.Dispose();
		}

		#endregion

		/// <summary>
		/// Gets the current row.
		/// </summary>
		/// <returns>The current row.</returns>
		DataRow GetCurrentRow()
		{
			if( mCurRow >= 0 && mCurRow < mTable.Rows.Count )
			{
				return mTable.Rows[mCurRow];
			}

			return null;
		}

		/// <summary>
		/// Returns the value of the specified column as a <see cref="System.Int32"/>.
		/// </summary>
		/// <param name="ColumnName">The name of the column whose value is to be retrieved.</param>
		/// <returns>The value of the specified column as a <see cref="System.Int32"/>.</returns>
		public int GetInt( string ColumnName )
		{
			DataRow Row = GetCurrentRow();

			if( Row != null )
			{
				return ( int )Row[ColumnName];
			}

			return 0;
		}

		/// <summary>
		/// Returns the value of the specified column as a <see cref="System.Single"/>.
		/// </summary>
		/// <param name="ColumnName">The name of the column whose value is to be retrieved.</param>
		/// <returns>The value of the specified column as a <see cref="System.Single"/>.</returns>
		public float GetFloat( string ColumnName )
		{
			DataRow Row = GetCurrentRow();

			if( Row != null )
			{
				object Obj = Row[ColumnName];

				if( Obj is Double )
				{
					return ( float )( ( double )Obj );
				}
				else
				{
					return ( float )Obj;
				}
			}

			return 0f;
		}

		/// <summary>
		/// Returns the value of the specified column as a <see cref="System.String"/>.
		/// </summary>
		/// <param name="ColumnName">The name of the column whose value is to be retrieved.</param>
		/// <returns>The value of the specified column as a <see cref="System.String"/>.</returns>
		public string GetString( string ColumnName )
		{
			DataRow Row = GetCurrentRow();

			if( Row != null )
			{
				return ( string )Row[ColumnName];
			}

			return string.Empty;
		}

		/// <summary>
		/// Moves to the first row in the result set.
		/// </summary>
		public void MoveToFirst()
		{
			mCurRow = 0;
		}

		/// <summary>
		/// Moves to the next row in the result set.
		/// </summary>
		public void MoveToNext()
		{
			if( !IsAtEnd )
			{
				++mCurRow;
			}
		}
	}
}
