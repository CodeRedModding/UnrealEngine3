// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.Linq;
using System.ServiceProcess;

namespace Builder.UnrealSyncService
{
	[RunInstaller( true )]
	public partial class UnrealSyncServiceInstaller : System.Configuration.Install.Installer
	{
		private ServiceInstaller Installer;
		private ServiceProcessInstaller ProcessInstaller;

		public UnrealSyncServiceInstaller()
		{
			InitializeComponent();

			InitializeComponent();
			Installer = new ServiceInstaller();
			Installer.StartType = ServiceStartMode.Automatic;
			Installer.ServiceName = "UnrealSyncService";
			Installer.DisplayName = "UnrealSync Service";
			Installer.Description = "A web service that interfaces between UnrealSync2 on client machines and the database.";
			Installers.Add( Installer );

			ProcessInstaller = new ServiceProcessInstaller();
			ProcessInstaller.Account = ServiceAccount.NetworkService;
			Installers.Add( ProcessInstaller );
		}

		// Must exists, because installation will fail without it.
		public override void Install( IDictionary StateSaver )
		{
			base.Install( StateSaver );
		}
	}
}
