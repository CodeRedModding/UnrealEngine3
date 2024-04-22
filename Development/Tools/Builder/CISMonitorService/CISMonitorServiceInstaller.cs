// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.Linq;
using System.ServiceProcess;

namespace Builder.CISMonitor
{
	[RunInstaller( true )]
	public partial class CISMonitorServiceInstaller : System.Configuration.Install.Installer
	{
		private ServiceInstaller Installer;
		private ServiceProcessInstaller ProcessInstaller;

		public CISMonitorServiceInstaller()
		{
			InitializeComponent();

			InitializeComponent();
			Installer = new ServiceInstaller();
			Installer.StartType = ServiceStartMode.Automatic;
			Installer.ServiceName = "CISMonitor";
			Installer.DisplayName = "CIS Monitor";
			Installer.Description = "Monitors the state of the build as stored in the database.";
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
