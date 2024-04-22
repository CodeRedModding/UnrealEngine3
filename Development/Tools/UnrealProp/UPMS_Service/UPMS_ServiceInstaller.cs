/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.ServiceProcess;
using System.Xml;

namespace UnrealProp
{
    [RunInstaller( true )]
    public partial class UPMS_ServiceInstaller : Installer
    {
        private ServiceInstaller Installer;
        private ServiceProcessInstaller ProcessInstaller;

        public UPMS_ServiceInstaller()
        {
            InitializeComponent();
            Installer = new ServiceInstaller();
            Installer.StartType = System.ServiceProcess.ServiceStartMode.Automatic;
            Installer.ServiceName = "UPMS_Service";
            Installer.DisplayName = "UPMS_Service";
            Installer.Description = "UnrealProp Master Service";
            Installers.Add( Installer );

            ProcessInstaller = new ServiceProcessInstaller();
            ProcessInstaller.Account = ServiceAccount.User;
            ProcessInstaller.Username = "EPICGAMES\\UnrealProp";
            Installers.Add( ProcessInstaller );
        }

        // Must exists, because installation will fail without it.
        public override void Install( System.Collections.IDictionary stateSaver )
        {
            base.Install( stateSaver );
        }
    }
}