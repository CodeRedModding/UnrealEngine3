/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;

namespace UnrealDatabaseProxy
{
	public partial class Service : ServiceBase
	{
		Server mServer = new Server();

		/// <summary>
		/// Constructor.
		/// </summary>
		public Service()
		{
			InitializeComponent();
		}

		/// <summary>
		/// Called by the SCM to start the service.
		/// </summary>
		/// <param name="args">Additional arguments.</param>
		protected override void OnStart( string[] args )
		{
			mServer.Start();
		}

		/// <summary>
		/// Called by the SCM to stop the service.
		/// </summary>
		protected override void OnStop()
		{
			mServer.Stop();
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		static void Main( string[] Args )
		{
			Service UDProxy = new Service();

			if( Args.Contains( "/manual" ) )
			{
				UDProxy.OnStart( Args );
				Console.WriteLine( "Press enter to quit..." );
				Console.ReadLine();
				UDProxy.OnStop();
			}
			else
			{
				ServiceBase.Run( UDProxy );
			}
		}
	}
}
