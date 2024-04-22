﻿/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Linq;
using System.Runtime.Remoting.Channels.Ipc;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting;

namespace iPhonePackager
{
    // Implementation of DeploymentServer -> Application interface
    [Serializable]
    class DeployTimeReporter : DeployTimeReportingInterface
    {
        public void Log(string Line)
        {
            Program.Log(Line);
        }

        public void Error(string Line)
        {
            Program.Error(Line);
        }

        public void Warning(string Line)
        {
            Program.Warning(Line);
        }

        public void SetProgressIndex(int Progress)
        {
            Program.ProgressIndex = Progress;
        }

        public int GetTransferProgressDivider()
        {
            return (Program.BGWorker != null) ? 1000 : 25;
        }
    }

    class DeploymentHelper
    {
        static DeploymentInterface DeployTimeInstance;

        public static Process DeploymentServerProcess = null;

        public static void InstallIPAOnConnectedDevices(string IPAPath)
        {
            // Read the mobile provision to check for issues
            FileOperations.ReadOnlyZipFileSystem Zip = new FileOperations.ReadOnlyZipFileSystem(IPAPath);
            MobileProvision Provision = null;
            try
            {
                MobileProvisionParser.ParseFile(Zip.ReadAllBytes("embedded.mobileprovision"));
            }
            catch (System.Exception ex)
            {
                Program.Warning(String.Format("Couldn't find an embedded mobile provision ({0})", ex.Message));
                Provision = null;
            }

            if (Provision != null)
            {
                var DeviceList = DeploymentHelper.Get().EnumerateConnectedDevices();

                foreach (var DeviceInfo in DeviceList)
                {
                    string UDID = DeviceInfo.UDID;
                    string DeviceName = DeviceInfo.DeviceName;

                    // Check the IPA's mobile provision against the connected device to make sure this device is authorized
                    // We'll still try installing anyways, but this message is more friendly than the failure we get back from MobileDeviceInterface
                    if (UDID != String.Empty)
                    {
                        if (!Provision.ContainsUDID(UDID))
                        {
                            Program.Warning(String.Format("Embedded provision in IPA does not include the UDID {0} of device '{1}'.  The installation is likely to fail.", UDID, DeviceName));
                        }
                    }
                    else
                    {
                        Program.Warning(String.Format("Unable to query device for UDID, and therefore unable to verify IPA embedded mobile provision contains this device."));
                    }
                }
            }

            DeploymentHelper.Get().InstallIPAOnDevice(IPAPath);
        }

        /**
         * Verify the IPA to be deployed is around
         */
        private static string VerifyIPAExists()
        {
            FileInfo Info = new FileInfo(Config.GetIPAPath(".ipa"));
            if (Info.Exists)
            {
                return (Info.FullName);
            }

            Program.Error(String.Format("Failed to find IPA file: '{0}'", Info.FullName));
            return "";
        }

        public static bool ExecuteDeployCommand(string Command, string RPCCommand)
        {
            switch (Command.ToLowerInvariant())
            {
                case "backup":
                    {
                        string ApplicationIdentifier = RPCCommand;
                        if (ApplicationIdentifier == null)
                        {
                            ApplicationIdentifier = Utilities.GetStringFromPList("CFBundleIdentifier");
                        }

                        if (!DeploymentHelper.Get().BackupDocumentsDirectory(ApplicationIdentifier, Config.GetRootBackedUpDocumentsDirectory()))
                        {
                            Program.Error("Failed to transfer documents directory from device to PC");
                            Program.ReturnCode = 100;
                        }
                    }
                    break;
                case "uninstall":
                    {
                        string ApplicationIdentifier = RPCCommand;
                        if (ApplicationIdentifier == null)
                        {
                            ApplicationIdentifier = Utilities.GetStringFromPList("CFBundleIdentifier");
                        }

                        if (!DeploymentHelper.Get().UninstallIPAOnDevice(ApplicationIdentifier))
                        {
                            Program.Error("Failed to uninstall IPA on device");
                            Program.ReturnCode = 100;
                        }
                    }
                    break;
                case "deploy":
                case "install":
                    {
                        string IPAPath = RPCCommand;
                        if (IPAPath == null)
                        {
                            IPAPath = VerifyIPAExists();
                        }

                        if (!DeploymentHelper.Get().InstallIPAOnDevice(IPAPath))
                        {
                            Program.Error("Failed to install IPA on device");
                            Program.ReturnCode = 100;
                        }
                    }
                    break;

                default:
                    return false;
            }

            return true;
        }

        static DeployTimeReporter Reporter = new DeployTimeReporter();

        public static DeploymentInterface Get()
        {
            if (DeployTimeInstance == null)
            {
                if (DeploymentServerProcess == null)
                {
                    DeploymentServerProcess = CreateDeploymentServerProcess();
                }

                DeployTimeInstance = (DeploymentInterface)Activator.GetObject(
                  typeof(DeploymentInterface),
                  @"ipc://iPhonePackager/DeploymentServer_PID" + Process.GetCurrentProcess().Id.ToString());
            }

            if (DeployTimeInstance == null)
            {
                Program.Error("Failed to connect to deployment server");
                throw new Exception("Failed to connect to deployment server");
            }

            DeployTimeInstance.SetReportingInterface(Reporter);

            return DeployTimeInstance;
        }

        static Process CreateDeploymentServerProcess()
        {
            Process NewProcess = new Process();
            NewProcess.StartInfo.WorkingDirectory = Config.DeviceConfigSpecificBinariesDirectory;
            NewProcess.StartInfo.FileName = NewProcess.StartInfo.WorkingDirectory + "\\DeploymentServer.exe";
            NewProcess.StartInfo.Arguments = "-iphonepackager " + Process.GetCurrentProcess().Id.ToString();
            NewProcess.StartInfo.UseShellExecute = false;

            try
            {
                NewProcess.Start();
                System.Threading.Thread.Sleep(500);
            }
            catch (System.Exception ex)
            {
                Program.Error("Failed to create deployment server process ({0})", ex.Message);
            }

            return NewProcess;
        }
    }
}
