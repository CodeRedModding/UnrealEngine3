/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnrealDatabaseProxy
{
	/// <summary>
	/// Attribute for specifying functions that are packet handlers.
	/// </summary>
	[AttributeUsage( AttributeTargets.Method, AllowMultiple = false, Inherited = false )]
	public class CommandHandlerAttribute : Attribute
	{
		string mCmdName;

		/// <summary>
		/// Gets the name of the command.
		/// </summary>
		public string CommandName
		{
			get { return mCmdName; }
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="CmdName">The name of the command to be handled.</param>
		public CommandHandlerAttribute( string CmdName )
		{
			if( CmdName == null || CmdName.Length == 0 )
			{
				throw new ArgumentException( "Invalid command name!" );
			}

			this.mCmdName = CmdName;
		}
	}
}
