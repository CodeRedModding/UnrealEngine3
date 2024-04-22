/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.Diagnostics;
using System.Linq;

namespace UnrealDatabaseProxy
{
	[RunInstaller( true )]
	public partial class ServiceInstaller : Installer
	{
		public ServiceInstaller()
		{
			InitializeComponent();

			if( !EventLog.SourceExists( "UnrealDatabaseProxy" ) )
			{
				EventLog.CreateEventSource( "UnrealDatabaseProxy", null );
			}
		}
	}
}
