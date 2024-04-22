﻿// Software License Agreement (BSD License)
// 
// Copyright (c) 2007, Peter Dennis Bartok <PeterDennisBartok@gmail.com>
// All rights reserved.
// 
// Redistribution and use of this software in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above
//   copyright notice, this list of conditions and the
//   following disclaimer.
// 
// * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the
//   following disclaimer in the documentation and/or other
//   materials provided with the distribution.
// 
// * Neither the name of Peter Dennis Bartok nor the names of its
//   contributors may be used to endorse or promote products
//   derived from this software without specific prior
//   written permission of Yahoo! Inc.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Diagnostics;
using MobileDeviceInterface;

namespace Manzana
{
    public class MobileDeviceInstanceManager
    {
        /// <summary>
        /// Registered device notification callback
        /// </summary>
        private static DeviceNotificationCallback DeviceCallbackHandle;

        /// <summary>
        /// The <c>Connect</c> event is triggered when a iPhone is connected to the computer
        /// </summary>
        public static event ConnectEventHandler ConnectEH;

        /// <summary>
        /// The <c>Disconnect</c> event is triggered when the iPhone is disconnected from the computer
        /// </summary>
        public static event ConnectEventHandler DisconnectEH;

        /// <summary>
        /// List of connected devices (device ptr -> device instance)
        /// </summary>
        public static Dictionary<TypedPtr<AppleMobileDeviceConnection>, MobileDeviceInstance> ConnectedDevices = new Dictionary<TypedPtr<AppleMobileDeviceConnection>, MobileDeviceInstance>();

        public static IEnumerable<MobileDeviceInstance> GetSnapshotInstanceList()
        {
            // Clone a copy to prevent problems from delayed enumeration
            List<MobileDeviceInstance> Result = new List<MobileDeviceInstance>();
            Result.AddRange(ConnectedDevices.Values);

            return Result;
        }

        /// <summary>
        /// Returns true if any devices are currently connected
        /// </summary>
        /// <returns></returns>
        public static bool AreAnyDevicesConnected()
        {
            lock (ConnectedDevices)
            {
                return ConnectedDevices.Count > 0;
            }
        }

        /// <summary>
        /// Initialize the mobile device manager, which handles discovery of connected Apple mobile devices
        /// </summary>
        /// <param name="myConnectHandler"></param>
        /// <param name="myDisconnectHandler"></param>
        public static void Initialize(ConnectEventHandler myConnectHandler, ConnectEventHandler myDisconnectHandler)
        {
            ConnectEH += myConnectHandler;
            DisconnectEH += myDisconnectHandler;
        
            DeviceCallbackHandle = new DeviceNotificationCallback(NotifyCallback);
            
            int ret = MobileDevice.AMDeviceMethods.NotificationSubscribe(DeviceCallbackHandle);
            if (ret != 0)
            {
                throw new Exception("AMDeviceNotificationSubscribe failed with error " + ret);
            }
        }

        /// <summary>
        /// Raises the <see>Connect</see> event.
        /// </summary>
        /// <param name="args">A <see cref="ConnectEventArgs"/> that contains the event data.</param>
        protected static void OnConnect(ConnectEventArgs args)
        {
            ConnectEventHandler handler = ConnectEH;

            if (handler != null)
            {
                handler(null, args);
            }
        }

        /// <summary>
        /// Raises the <see>Disconnect</see> event.
        /// </summary>
        /// <param name="args">A <see cref="ConnectEventArgs"/> that contains the event data.</param>
        protected static void OnDisconnect(ConnectEventArgs args)
        {
            ConnectEventHandler handler = DisconnectEH;

            if (handler != null)
            {
                handler(null, args);
            }
        }

        private static void NotifyCallback(ref AMDeviceNotificationCallbackInfo callback)
        {
            if (callback.msg == NotificationMessage.Connected)
            {
                MobileDeviceInstance Inst;
                if (ConnectedDevices.TryGetValue(callback.dev, out Inst))
                {
                    // Already connected, not sure why we got another message...
                }
                else
                {
                    Inst = new MobileDeviceInstance(callback.dev);
                    ConnectedDevices.Add(callback.dev, Inst);
                }

                if (Inst.ConnectToPhone())
                {
                    OnConnect(new ConnectEventArgs(callback));
                }
            }
            else if (callback.msg == NotificationMessage.Disconnected)
            {
                MobileDeviceInstance Inst;
                if (ConnectedDevices.TryGetValue(callback.dev, out Inst))
                {
                    Inst.connected = false;
                    OnDisconnect(new ConnectEventArgs(callback));
                    ConnectedDevices.Remove(callback.dev);
                }
            }
        }
    }

    /// <summary>
    /// Exposes access to a mobile device running iOS
    /// </summary>
    public class MobileDeviceInstance
    {
        #region Locals

        private DeviceRestoreNotificationCallback drn1;
        private DeviceRestoreNotificationCallback drn2;
        private DeviceRestoreNotificationCallback drn3;
        private DeviceRestoreNotificationCallback drn4;

        internal TypedPtr<AppleMobileDeviceConnection> iPhoneHandle;
        internal TypedPtr<AFCCommConnection> AFCCommsHandle;
        internal IntPtr hService;
        internal IntPtr hInstallService;
        public bool connected;
        private string current_directory;
        #endregion	// Locals

        #region Constructors
        /// <summary>
        /// Initializes a new iPhone object.
        /// </summary>
        private void doConstruction()
        {
            drn1 = new DeviceRestoreNotificationCallback(DfuConnectCallback);
            drn2 = new DeviceRestoreNotificationCallback(RecoveryConnectCallback);
            drn3 = new DeviceRestoreNotificationCallback(DfuDisconnectCallback);
            drn4 = new DeviceRestoreNotificationCallback(RecoveryDisconnectCallback);

            int ret = MobileDevice.AMDeviceMethods.AMRestoreRegisterForDeviceNotifications(drn1, drn2, drn3, drn4, 0, IntPtr.Zero);
            if (ret != 0)
            {
                throw new Exception("AMRestoreRegisterForDeviceNotifications failed with error " + ret);
            }
            current_directory = "/";
        }

        /// <summary>
        /// Creates a new iPhone object. If an iPhone is connected to the computer, a connection will automatically be opened.
        /// </summary>
        public MobileDeviceInstance(TypedPtr<AppleMobileDeviceConnection> Connection)
        {
            iPhoneHandle = Connection;
            doConstruction();
        }
        #endregion	// Constructors

        #region Properties
        /// <summary>
        /// Gets the current activation state of the phone
        /// </summary>
        public string ActivationState
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "ActivationState");
            }
        }

        /// <summary>
        /// Returns true if an iPhone is connected to the computer
        /// </summary>
        public bool IsConnected
        {
            get
            {
                return connected;
            }
        }

        /// <summary>
        /// Returns the Device information about the connected iPhone
        /// </summary>
        public TypedPtr<AppleMobileDeviceConnection> Device
        {
            get
            {
                return iPhoneHandle;
            }
        }

        ///<summary>
        /// Returns the 40-character UUID of the device
        ///</summary>
        public string DeviceId
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "UniqueDeviceID");
            }
        }

        ///<summary>
        /// Returns the type of the device, should be either 'iPhone' or 'iPod'.
        ///</summary>
        public string DeviceType
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "DeviceClass");
            }
        }

        ///<summary>
        /// Returns the current OS version running on the device (2.0, 2.2, 3.0, 3.1, etc).
        ///</summary>
        public string DeviceVersion
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "ProductVersion");
            }
        }
        ///<summary>
        /// Returns the name of the device, like "Dan's iPhone"
        ///</summary>
        public string DeviceName
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "DeviceName");
            }
        }

        ///<summary>
        /// Returns the model number of the device, like "MA712"
        ///</summary>
        public string ModelNumber
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "ModelNumber");
            }
        }
        ///<summary>
        /// Returns the product type of the device, like "iPhone1,1"
        ///</summary>
        public string ProductType
        {
            get
            {
                return MobileDevice.AMDeviceCopyValue(iPhoneHandle, "ProductType");
            }
        }
        

        /// <summary>
        /// Returns the handle to the iPhone com.apple.afc service
        /// </summary>
        public TypedPtr<AFCCommConnection> AFCHandle
        {
            get
            {
                return AFCCommsHandle;
            }
        }

        /// <summary>
        /// Gets/Sets the current working directory, used by all file and directory methods
        /// </summary>
        public string CurrentDirectory
        {
            get
            {
                return current_directory;
            }

            set
            {
                string new_path = FullPath(current_directory, value);
                if (!IsDirectory(new_path))
                {
                    throw new Exception("Invalid directory specified");
                }
                current_directory = new_path;
            }
        }
        #endregion	// Properties

        #region Events

        /// <summary>
        /// Write Me
        /// </summary>
        public event EventHandler DfuConnect;

        /// <summary>
        /// Raises the <see>DfuConnect</see> event.
        /// </summary>
        /// <param name="args">A <see cref="DeviceNotificationEventArgs"/> that contains the event data.</param>
        protected void OnDfuConnect(DeviceNotificationEventArgs args)
        {
            EventHandler handler = DfuConnect;

            if (handler != null)
            {
                handler(this, args);
            }
        }

        /// <summary>
        /// Write Me
        /// </summary>
        public event EventHandler DfuDisconnect;

        /// <summary>
        /// Raises the <see>DfiDisconnect</see> event.
        /// </summary>
        /// <param name="args">A <see cref="DeviceNotificationEventArgs"/> that contains the event data.</param>
        protected void OnDfuDisconnect(DeviceNotificationEventArgs args)
        {
            EventHandler handler = DfuDisconnect;

            if (handler != null)
            {
                handler(this, args);
            }
        }

        /// <summary>
        /// The RecoveryModeEnter event is triggered when the attached iPhone enters Recovery Mode
        /// </summary>
        public event EventHandler RecoveryModeEnter;

        /// <summary>
        /// Raises the <see>RecoveryModeEnter</see> event.
        /// </summary>
        /// <param name="args">A <see cref="DeviceNotificationEventArgs"/> that contains the event data.</param>
        protected void OnRecoveryModeEnter(DeviceNotificationEventArgs args)
        {
            EventHandler handler = RecoveryModeEnter;

            if (handler != null)
            {
                handler(this, args);
            }
        }

        /// <summary>
        /// The RecoveryModeLeave event is triggered when the attached iPhone leaves Recovery Mode
        /// </summary>
        public event EventHandler RecoveryModeLeave;

        /// <summary>
        /// Raises the <see>RecoveryModeLeave</see> event.
        /// </summary>
        /// <param name="args">A <see cref="DeviceNotificationEventArgs"/> that contains the event data.</param>
        protected void OnRecoveryModeLeave(DeviceNotificationEventArgs args)
        {
            EventHandler handler = RecoveryModeLeave;

            if (handler != null)
            {
                handler(this, args);
            }
        }

        #endregion	// Events

        #region Filesystem


        /// <summary>
        /// Sanitizes a filename for use on PC (just the filename, not a full path)
        /// </summary>
        static public string SanitizeFilename(string InputFilename)
        {
            char[] Filename = InputFilename.ToCharArray();
            char[] BadChars = Path.GetInvalidFileNameChars();
            for (int i = 0; i < Filename.Length; ++i)
            {
                foreach (char BadChar in BadChars)
                {
                    if (Filename[i] == BadChar)
                    {
                        Filename[i] = '_';
                        break;
                    }
                }
            }

            return new string(Filename);
        }

        /// <summary>
        /// Sanitizes a path for use on PC (just the path, no filename)
        /// </summary>
        static public string SanitizePathNoFilename(string InputPath)
        {
            char[] DirectoryName = InputPath.ToCharArray();
            char[] BadChars = Path.GetInvalidPathChars();
            for (int i = 0; i < DirectoryName.Length; ++i)
            {
                foreach (char BadChar in BadChars)
                {
                    if ((DirectoryName[i] == BadChar) || (DirectoryName[i] == ':'))
                    {
                        DirectoryName[i] = '_';
                        break;
                    }
                }
            }

            return new string(DirectoryName);
        }

        void RecursiveBackup(string SourceFolderOnDevice, string TargetFolderOnPC)
        {
            string[] Directories = GetDirectories(SourceFolderOnDevice);
            foreach (string Directory in Directories)
            {
                string NewSourceFolder = SourceFolderOnDevice + Directory + "/";
                string NewTargetFolder = Path.Combine(TargetFolderOnPC, SanitizePathNoFilename(Directory));

                RecursiveBackup(NewSourceFolder, NewTargetFolder);
            }

            string[] Filenames = GetFiles(SourceFolderOnDevice);
            foreach (string Filename in Filenames)
            {
                string SourceFilename = SourceFolderOnDevice + Filename;
                string DestFilename = Path.Combine(TargetFolderOnPC, SanitizeFilename(Filename));
                WriteProgressLine("Copying '{0}' -> '{1}' ...", 0, SourceFilename, DestFilename);
                CopyFileFromPhone(DestFilename, SourceFilename, 1024 * 1024);
            }
        }

		public void DumpInstalledApplications()
		{
			Dictionary<string, object> AppBundles;
			MobileDevice.AMDeviceMethods.LookupApplications(iPhoneHandle, IntPtr.Zero, out AppBundles);

			foreach (var Bundle in AppBundles)
			{
				WriteProgressLine(String.Format("Application bundle {0} has the following pairs:", Bundle.Key), 0);

				Dictionary<object, object> BundlePairs = (Dictionary<object, object>)Bundle.Value;
				foreach (var KVP in BundlePairs)
				{
					WriteProgressLine(String.Format("    {0} -> {1}", KVP.Key, KVP.Value), 0);
				}
				
			}
		}

        /// <summary>
        /// Tries to back up all of the files on a phone in a particular directory to the PC
        /// (requires the bundle identifier to be able to mount that directory)
        /// </summary>
        public bool TryBackup(string BundleIdentifier, string SourceFolderOnDevice, string TargetFolderOnPC)
        {
            if (ConnectToBundle(BundleIdentifier))
            {
                WriteProgressLine("Connected to bundle '{0}'", 0, BundleIdentifier);

                try
                {
                    RecursiveBackup(SourceFolderOnDevice, TargetFolderOnPC);
                    return true;
                }
                catch (Exception ex)
                {
                    WriteProgressLine("Failed to transfer a file, extended error is '{0}'", 100, ex.Message);
                    return false;
                }
            }
            else
            {
                WriteProgressLine("Error: Failed to connect to bundle '{0}'", 100, BundleIdentifier);
                return false;
            }
        }

        /// <summary>
        /// Returns the names of files in a specified directory
        /// </summary>
        /// <param name="path">The directory from which to retrieve the files.</param>
        /// <returns>A <c>String</c> array of file names in the specified directory. Names are relative to the provided directory</returns>
        public string[] GetFiles(string path)
        {
            return GetFiles(path, false);
        }

        public string[] GetFiles(string path, bool bIncludeDirs)
        {
            if (!IsConnected)
            {
                throw new Exception("Not connected to phone");
            }

            string full_path = FullPath(CurrentDirectory, path);

            IntPtr hAFCDir = IntPtr.Zero;
            if (MobileDevice.AFCDirectoryOpen(AFCCommsHandle, full_path, ref hAFCDir) != 0)
            {
                throw new Exception("Path does not exist");
            }

            string buffer = null;
            ArrayList paths = new ArrayList();
            MobileDevice.AFCDirectoryRead(AFCCommsHandle, hAFCDir, ref buffer);

            while (buffer != null)
            {
                if (!IsDirectory(FullPath(full_path, buffer)))
                {
                    paths.Add(buffer);
                }
                else
                {
                    if (bIncludeDirs)
                    {
                        paths.Add(buffer + "/");
                    }
                }
                MobileDevice.AFCDirectoryRead(AFCCommsHandle, hAFCDir, ref buffer);
            }
            MobileDevice.AFC.DirectoryClose(AFCCommsHandle, hAFCDir);
            return (string[])paths.ToArray(typeof(string));
        }

        /// <summary>
        /// Returns the FileInfo dictionary
        /// </summary>
        /// <param name="path">The file or directory for which to retrieve information.</param>
        public Dictionary<string, string> GetFileInfo(string path)
        {
            Dictionary<string, string> ans = new Dictionary<string, string>();
            TypedPtr<AFCDictionary> Data;

            int ret = MobileDevice.AFC.FileInfoOpen(AFCCommsHandle, path, out Data);
            if ((ret == 0) && (Data.Handle != IntPtr.Zero))
            {
                IntPtr pname;
                IntPtr pvalue;

                while (MobileDevice.AFC.KeyValueRead(Data, out pname, out pvalue) == 0 && pname != null && pvalue != null)
                {
                    string name = Marshal.PtrToStringAnsi(pname);
                    string value = Marshal.PtrToStringAnsi(pvalue);
                    if ((name != null) && (value != null))
                    {
                            ans.Add(name, value);
                    }
                    else
                    {
                        break;
                    }
                }

                MobileDevice.AFC.KeyValueClose(Data);
            }

            return ans;
        }

        /// <summary>
        /// Returns the st_ifmt of a path
        /// </summary>
        /// <param name="path">Path to query</param>
        /// <returns>string representing value of st_ifmt</returns>
        private string Get_st_ifmt(string path)
        {
            Dictionary<string, string> fi = GetFileInfo(path);
            return fi["st_ifmt"];
        }

        /// <summary>
        /// Returns the size and type of the specified file or directory.
        /// </summary>
        /// <param name="path">The file or directory for which to retrieve information.</param>
        /// <param name="size">Returns the size of the specified file or directory</param>
        /// <param name="directory">Returns <c>true</c> if the given path describes a directory, false if it is a file.</param>
        public void GetFileInfo(string path, out ulong size, out bool directory)
        {
            Dictionary<string, string> fi = GetFileInfo(path);

            size = fi.ContainsKey("st_size") ? System.UInt64.Parse(fi["st_size"]) : 0;

            bool SLink = false;
            directory = false;
            if (fi.ContainsKey("st_ifmt"))
            {
                switch (fi["st_ifmt"])
                {
                    case "S_IFDIR": directory = true; break;
                    case "S_IFLNK": SLink = true; break;
                }
            }

            if (SLink)
            {
                // test for symbolic directory link
                IntPtr hAFCDir = IntPtr.Zero;

                if (directory = (MobileDevice.AFCDirectoryOpen(AFCCommsHandle, path, ref hAFCDir) == 0))
                {
                    MobileDevice.AFC.DirectoryClose(AFCCommsHandle, hAFCDir);
                }
            }
        }

        /// <summary>
        /// Returns the size of the specified file or directory.
        /// </summary>
        /// <param name="path">The file or directory for which to obtain the size.</param>
        /// <returns></returns>
        public ulong FileSize(string path)
        {
            bool is_dir;
            ulong size;

            GetFileInfo(path, out size, out is_dir);
            return size;
        }

        /// <summary>
        /// Creates the directory specified in path
        /// </summary>
        /// <param name="path">The directory path to create</param>
        /// <returns>true if directory was created</returns>
        public bool CreateDirectory(string path)
        {
            return !(MobileDevice.AFC.DirectoryCreate(AFCCommsHandle, FullPath(CurrentDirectory, path)) != 0);
        }

        /// <summary>
        /// Gets the names of subdirectories in a specified directory.
        /// </summary>
        /// <param name="path">The path for which an array of subdirectory names is returned.</param>
        /// <returns>An array of type <c>String</c> containing the names of subdirectories in <c>path</c>.</returns>
        public string[] GetDirectories(string path)
        {
            if (!IsConnected)
            {
                throw new Exception("Not connected to phone");
            }

            IntPtr hAFCDir = IntPtr.Zero;
            string full_path = FullPath(CurrentDirectory, path);
            //full_path = "/private"; // bug test

            int res = MobileDevice.AFCDirectoryOpen(AFCCommsHandle, full_path, ref hAFCDir);
            if (res != 0)
            {
                throw new Exception("Path does not exist: " + res.ToString());
            }

            string buffer = null;
            ArrayList paths = new ArrayList();
            MobileDevice.AFCDirectoryRead(AFCCommsHandle, hAFCDir, ref buffer);

            while (buffer != null)
            {
                if ((buffer != ".") && (buffer != "..") && IsDirectory(FullPath(full_path, buffer)))
                {
                    paths.Add(buffer);
                }
                MobileDevice.AFCDirectoryRead(AFCCommsHandle, hAFCDir, ref buffer);
            }
            MobileDevice.AFC.DirectoryClose(AFCCommsHandle, hAFCDir);
            return (string[])paths.ToArray(typeof(string));
        }

        /// <summary>
        /// Moves a file or a directory and its contents to a new location or renames a file or directory if the old and new parent path matches.
        /// </summary>
        /// <param name="sourceName">The path of the file or directory to move or rename.</param>
        /// <param name="destName">The path to the new location for <c>sourceName</c>.</param>
        ///	<remarks>Files cannot be moved across filesystem boundaries.</remarks>
        public bool Rename(string sourceName, string destName)
        {
            return MobileDevice.AFC.RenamePath(AFCCommsHandle, FullPath(CurrentDirectory, sourceName), FullPath(CurrentDirectory, destName)) == 0;
        }

        /// <summary>
        /// Returns the root information for the specified path. 
        /// </summary>
        /// <param name="path">The path of a file or directory.</param>
        /// <returns>A string containing the root information for the specified path. </returns>
        public string GetDirectoryRoot(string path)
        {
            return "/";
        }

        /// <summary>
        /// Determines whether the given path refers to an existing file or directory on the phone. 
        /// </summary>
        /// <param name="path">The path to test.</param>
        /// <returns><c>true</c> if path refers to an existing file or directory, otherwise <c>false</c>.</returns>
        public bool Exists(string path)
        {
            TypedPtr<AFCDictionary> data = IntPtr.Zero;

            int ret = MobileDevice.AFC.FileInfoOpen(AFCCommsHandle, path, out data);
            if (ret == 0)
            {
                MobileDevice.AFC.KeyValueClose(data);
            }

            return ret == 0;
        }

        /// <summary>
        /// Determines whether the given path refers to an existing directory on the phone. 
        /// </summary>
        /// <param name="path">The path to test.</param>
        /// <returns><c>true</c> if path refers to an existing directory or is a symbolic link to one, otherwise <c>false</c>.</returns>
        public bool IsDirectory(string path)
        {
            bool is_dir;
            ulong size;

            GetFileInfo(path, out size, out is_dir);
            return is_dir;
        }

        /// <summary>
        /// Test if path represents a regular file
        /// </summary>
        /// <param name="path">path to query</param>
        /// <returns>true if path refers to a regular file, false if path is a link or directory</returns>
        public bool IsFile(string path)
        {
            return Get_st_ifmt(path) == "S_IFREG";
        }

        /// <summary>
        /// Test if path represents a link
        /// </summary>
        /// <param name="path">path to test</param>
        /// <returns>true if path is a symbolic link</returns>
        public bool IsLink(string path)
        {
            return Get_st_ifmt(path) == "S_IFLNK";
        }

        /// <summary>
        /// Deletes an empty directory from a specified path.
        /// </summary>
        /// <param name="path">The name of the empty directory to remove. This directory must be writable and empty.</param>
        public void DeleteDirectory(string path)
        {
            string full_path = FullPath(CurrentDirectory, path);
            if (IsDirectory(full_path))
            {
                MobileDevice.AFC.RemovePath(AFCCommsHandle, full_path);
            }
        }

        /// <summary>
        /// Deletes the specified directory and, if indicated, any subdirectories in the directory.
        /// </summary>
        /// <param name="path">The name of the directory to remove.</param>
        /// <param name="recursive"><c>true</c> to remove directories, subdirectories, and files in path; otherwise, <c>false</c>. </param>
        public void DeleteDirectory(string path, bool recursive)
        {
            if (!recursive)
            {
                DeleteDirectory(path);
                return;
            }

            string full_path = FullPath(CurrentDirectory, path);
            if (IsDirectory(full_path))
            {
                InternalDeleteDirectory(path);
            }

        }

        /// <summary>
        /// Deletes the specified file.
        /// </summary>
        /// <param name="path">The name of the file to remove.</param>
        public void DeleteFile(string path)
        {
            string full_path = FullPath(CurrentDirectory, path);
            if (Exists(full_path))
            {
                MobileDevice.AFC.RemovePath(AFCCommsHandle, full_path);
            }
        }
        #endregion	// Filesystem

        #region Public Methods

        /// <summary>
        /// Close the AFC connection
        /// </summary>
        public void Disconnect()
        {
            if (AFCCommsHandle.Handle != IntPtr.Zero)
            {
                int ans = MobileDevice.AFC.ConnectionClose(AFCCommsHandle);
                ans = MobileDevice.AMDeviceMethods.StopSession(iPhoneHandle);
                ans = MobileDevice.AMDeviceMethods.Disconnect(iPhoneHandle);
            }

            AFCCommsHandle = IntPtr.Zero;
        }

        /// <summary>
        /// Close and Reopen AFC Connection
        /// </summary>
        /// <returns>status from reopen</returns>
        public void Reconnect()
        {
            Disconnect();
            ConnectToPhone();
        }

        #endregion // public Methods

        void CopyFileToPhone(string PathOnPC, string PathOnPhone)
        {
            CopyFileToPhone(PathOnPC, PathOnPhone, 1024 * 1024);
        }

        void CopyFileToPhone(string PathOnPC, string PathOnPhone, int ChunkSize)
        {
            DateTime StartTime = DateTime.Now;

            byte[] buffer = new byte[ChunkSize];

            // Make sure the directory exists on the phone
            string DirectoryOnPhone = PathOnPhone.Remove(PathOnPhone.LastIndexOf('/'));
            CreateDirectory(DirectoryOnPhone);

            FileStream SourceFile = File.OpenRead(PathOnPC);
            iPhoneFile DestinationFile = iPhoneFile.OpenWrite(this, PathOnPhone);

            
            long ProgressInterval = Math.Max(buffer.Length, (SourceFile.Length / TransferProgressDivisor));

            long NextProgressPrintout = ProgressInterval;
            long TotalBytesRead = 0;


            int BytesRead = SourceFile.Read(buffer, 0, buffer.Length);
            while (BytesRead > 0)
            {
                if (TotalBytesRead >= NextProgressPrintout)
                {
                    NextProgressPrintout += ProgressInterval;

                    if (OnGenericProgress != null)
                    {
                        int PercentDone = (int)((100 * TotalBytesRead) / SourceFile.Length);
                        string Msg = "Transferred " + (SourceFile.Position / 1024) + " KB of " + (SourceFile.Length / 1024) + " KB";
                        OnGenericProgress(Msg, PercentDone);
                    }
                    else
                    {
                        Console.WriteLine(" ... Transferred " + (SourceFile.Position / 1024) + " KB of " + (SourceFile.Length / 1024) + " KB");
                    }
                }

                DestinationFile.Write(buffer, 0, BytesRead);
                BytesRead = SourceFile.Read(buffer, 0, buffer.Length);

                TotalBytesRead += BytesRead;
            }

            DestinationFile.Flush();
            DestinationFile.Close();
            SourceFile.Close();

            TimeSpan CopyLength = DateTime.Now - StartTime;
            Console.WriteLine(" ... Finished copying to '{1}' in {0:0.00} s", CopyLength.TotalSeconds, PathOnPhone);
        }

        // Default level is 6, and 0,3,7 are regularly used.  Bump the logging level up to 7 to get verbose logs.
        void SetLoggingLevel(int Threshold)
        {
            Int32 LoggingThreshold = Math.Min(Math.Max(0, Threshold), 7);
            MobileDevice.CFPreferencesSetAppValue(
                (IntPtr)MobileDevice.CFStringMakeConstantString("LogLevel"),
                (IntPtr)MobileDevice.CFNumberCreate(LoggingThreshold),
                (IntPtr)MobileDevice.CFStringMakeConstantString("com.apple.MobileDevice"));
        }

        void WriteProgressLine(string Fmt, int ProgressCount, params object[] Args)
        {
            string Line = String.Format(Fmt, Args);

            if (OnGenericProgress != null)
            {
                OnGenericProgress(Line, ProgressCount);
            }
            else
            {
                Console.WriteLine(Line);
            }
        }

        /// <summary>
        /// Copies a file from the phone to the PC
        /// </summary>
        void CopyFileFromPhone(string PathOnPC, string PathOnPhone, int ChunkSize)
        {
            DateTime StartTime = DateTime.Now;

            byte[] buffer = new byte[ChunkSize];

            // Make sure the directory exists on the PC
            string DirectoryOnPC = Path.GetDirectoryName(PathOnPC);
            Directory.CreateDirectory(DirectoryOnPC);

            iPhoneFile SourceFile = iPhoneFile.OpenRead(this, PathOnPhone);
            FileStream DestinationFile = File.OpenWrite(PathOnPC);

            long TotalBytesRead = 0;

            int BytesRead = SourceFile.Read(buffer, 0, buffer.Length);
            while (BytesRead > 0)
            {
                DestinationFile.Write(buffer, 0, BytesRead);
                BytesRead = SourceFile.Read(buffer, 0, buffer.Length);

                TotalBytesRead += BytesRead;
            }

            DestinationFile.Flush();
            DestinationFile.Close();
            SourceFile.Close();

            TimeSpan CopyLength = DateTime.Now - StartTime;
            WriteProgressLine(" ... Finished copying to '{1}' in {0:0.00} s", 100, CopyLength.TotalSeconds, PathOnPC);
        }


        string MakeUnixPath(string PlatformPath)
        {
            string Result = Path.GetFullPath(PlatformPath);

            // Convert C:\ to /C\, the \ will get converted to / in the next step
            if (Path.IsPathRooted(Result))
            {
                string Root = Path.GetPathRoot(Result);

                Result = '/' + Root.Replace(":", "") + Result.Substring(Root.Length);
            }

            // Convert slash directions
            if (Path.DirectorySeparatorChar != '/')
            {
                Result = Result.Replace(Path.DirectorySeparatorChar, '/');
            }

            return Result;
        }

        void ReconnectWithInstallProxy()
        {
            Reconnect();
            if (MobileDevice.AMDeviceMethods.StartService(iPhoneHandle, MobileDevice.CFStringMakeConstantString("com.apple.mobile.installation_proxy"), ref hInstallService, IntPtr.Zero) != 0)
            {
                Console.WriteLine("Unable to start installation_proxy service!");
            }
        }

        public delegate void ProgressCallback(string Message, int PercentDone);

        public ProgressCallback OnGenericProgress = null;
        public int TransferProgressDivisor = 25;

        /// <summary>
        /// Generic progress callback implementation (used for install/uninstall/etc...)
        /// </summary>
        void HandleProgressCallback(string OuterFunction, TypedPtr<CFDictionary> SourceDict)
        {
            Dictionary<string, object> Dict = MobileDevice.ConvertCFDictionaryToDictionaryStringy(SourceDict);

            // Expecting:
            // string,string -> "Status",PhaseOfInstaller
            // string,number -> "PercentComplete",%Done
            try
            {
                string Phase = Dict["Status"] as string;
                int PercentDone = (int)((Double)(Dict["PercentComplete"]));

                if (OnGenericProgress != null)
                {
                    string Msg = String.Format("{0} is {1}% complete at phase '{2}'", OuterFunction, PercentDone, Phase);
                    OnGenericProgress(Msg, PercentDone);
                }
                else
                {
                    Console.WriteLine(" ... {0} {1}% complete (phase '{2}')", OuterFunction, PercentDone, Phase);
                }
            }
            catch (System.Exception)
            {
            }
        }

        /// <summary>
        /// Progress callback for upgrading or installing
        /// </summary>
        void InstallProgressCallback(IntPtr UntypedSourceDict, IntPtr UserData)
        {
            HandleProgressCallback("Install", UntypedSourceDict);
        }

        /// <summary>
        /// Progress callback for uninstalling
        /// </summary>
        void UninstallProgressCallback(IntPtr UntypedSourceDict, IntPtr UserData)
        {
            HandleProgressCallback("Uninstall", UntypedSourceDict);
        }

        /// <summary>
        /// Try to install an IPA on the mobile device (it must have already been copied to the PublicStaging directory, with the same filename as the path passed in)
        /// </summary>
        public bool TryInstall(string IPASourcePathOnPC)
        {
            DateTime StartTime = DateTime.Now;

            ReconnectWithInstallProxy();

            IntPtr LiveConnection = IntPtr.Zero;
            IntPtr ClientOptions = IntPtr.Zero;

            TypedPtr<CFURL> UrlPath = MobileDevice.CreateFileUrlHelper(IPASourcePathOnPC, false);
            string UrlPathAsString = MobileDevice.GetStringForUrl(UrlPath);

            int Result = MobileDevice.AMDeviceMethods.SecureInstallApplication(LiveConnection, iPhoneHandle, UrlPath, ClientOptions, InstallProgressCallback, IntPtr.Zero);
            if (Result == 0)
            {
                Console.WriteLine("Install of \"{0}\" completed successfully in {2:0.00} seconds", Path.GetFileName(IPASourcePathOnPC), Result, (DateTime.Now - StartTime).TotalSeconds);
            }
            else
            {
                Console.WriteLine("Install of \"{0}\" failed with error code {1} in {2:0.00} seconds", Path.GetFileName(IPASourcePathOnPC), MobileDevice.GetErrorString(Result), (DateTime.Now - StartTime).TotalSeconds);
            }

            return Result == 0;
        }

        /// <summary>
        /// Try to update an IPA on the mobile device (it must have already been copied to the PublicStaging directory, with the same filename as the path passed in)
        /// </summary>
        public bool TryUpgrade(string IPASourcePathOnPC)
        {
            DateTime StartTime = DateTime.Now;

            ReconnectWithInstallProxy();

            IntPtr LiveConnection = IntPtr.Zero;
            IntPtr ClientOptions = IntPtr.Zero;

            TypedPtr<CFURL> UrlPath = MobileDevice.CreateFileUrlHelper(IPASourcePathOnPC, false);
            string UrlPathAsString = MobileDevice.GetStringForUrl(UrlPath);

            int Result = MobileDevice.AMDeviceMethods.SecureUpgradeApplication(LiveConnection, iPhoneHandle, UrlPath, ClientOptions, InstallProgressCallback, IntPtr.Zero);

            if (Result == 0)
            {
                Console.WriteLine("Install \\ Update of \"{0}\" finished in {2:0.00} seconds", Path.GetFileName(IPASourcePathOnPC), Result, (DateTime.Now - StartTime).TotalSeconds);
            }
            else
            {
                Console.WriteLine("Install \\ Update of \"{0}\" failed with {1} in {2:0.00} seconds", Path.GetFileName(IPASourcePathOnPC), MobileDevice.GetErrorString(Result), (DateTime.Now - StartTime).TotalSeconds);
            }

            return Result == 0;
        }

        public bool TryUninstall(string ApplicationIdentifier)
        {
            DateTime StartTime = DateTime.Now;

            ReconnectWithInstallProxy();

            TypedPtr<CFString> CF_ApplicationIdentifier = MobileDevice.CFStringMakeConstantString(ApplicationIdentifier);

            IntPtr CF_ClientOptions = IntPtr.Zero;
            IntPtr ExtraKey = IntPtr.Zero;
            IntPtr ExtraValue = IntPtr.Zero;
            IntPtr ConnectionHandle = IntPtr.Zero;

            int Result = MobileDevice.AMDeviceMethods.SecureUninstallApplication(ConnectionHandle, iPhoneHandle, CF_ApplicationIdentifier, CF_ClientOptions, UninstallProgressCallback, IntPtr.Zero);

            if (Result == 0)
            {
                Console.WriteLine("Uninstall of \"{0}\" completed successfully in {2:0.00} seconds", ApplicationIdentifier, Result, (DateTime.Now - StartTime).TotalSeconds);
            }
            else
            {
                Console.WriteLine("Uninstall of \"{0}\" failed with {1} in {2:0.00} seconds", ApplicationIdentifier, MobileDevice.GetErrorString(Result), (DateTime.Now - StartTime).TotalSeconds);
            }

            return Result == 0;
        }

        /// <summary>
        /// Copies a file to the PublicStaging directory on the mobile device, preserving the filename
        /// </summary>
        /// <param name="SourceFile"></param>
        public void CopyFileToPublicStaging(string SourceFile)
        {
            string IpaFilename = Path.GetFileName(SourceFile);
            CopyFileToPhone(SourceFile, "/PublicStaging/" + IpaFilename);
            //Dictionary<string, string> fi = GetFileInfo("/PublicStaging/" + IpaFilename);
        }

        #region Private Methods
        public bool ConnectToPhone()
        {
            SetLoggingLevel(7);

            // Make sure we can connect to the phone and that it has been paired with this machine previously
            if (MobileDevice.AMDeviceMethods.Connect(iPhoneHandle) != 0)
            {
                return false;
            }

            if (MobileDevice.AMDeviceMethods.IsPaired(iPhoneHandle) != 1)
            {
                return false;
            }

            if (MobileDevice.AMDeviceMethods.ValidatePairing(iPhoneHandle) != 0)
            {
                return false;
            }

            // Start a session
            if (MobileDevice.AMDeviceMethods.StartSession(iPhoneHandle) == 1)
            {
                return false;
            }

            if (MobileDevice.AMDeviceMethods.StartService(iPhoneHandle, MobileDevice.CFStringMakeConstantString("com.apple.afc"), ref hService, IntPtr.Zero) != 0)
            {
                return false;
            }


            // Open a file sharing connection
            if (MobileDevice.AFC.ConnectionOpen(hService, 0, out AFCCommsHandle) != 0)
            {
                return false;
            }

            connected = true;
            return true;
        }

        public bool ConnectToBundle(string BundleName)
        {
            Reconnect();

            TypedPtr<CFString> CFBundleName = MobileDevice.CFStringMakeConstantString(BundleName);

            // Open the bundle
            int Result = MobileDevice.AMDeviceMethods.StartHouseArrestService(iPhoneHandle, CFBundleName, IntPtr.Zero, ref hService, 0); 
            if (Result != 0)
            {
                Console.WriteLine("Failed to connect to bundle '{0}' with {1}", BundleName, MobileDevice.GetErrorString(Result));
                return false;
            }

            // Open a file sharing connection
            if (MobileDevice.AFC.ConnectionOpen(hService, 0, out AFCCommsHandle) != 0)
            {
                return false;
            }

            return true;
        }

        private void DfuConnectCallback(ref AMRecoveryDevice callback)
        {
            OnDfuConnect(new DeviceNotificationEventArgs(callback));
        }

        private void DfuDisconnectCallback(ref AMRecoveryDevice callback)
        {
            OnDfuDisconnect(new DeviceNotificationEventArgs(callback));
        }

        private void RecoveryConnectCallback(ref AMRecoveryDevice callback)
        {
            OnRecoveryModeEnter(new DeviceNotificationEventArgs(callback));
        }

        private void RecoveryDisconnectCallback(ref AMRecoveryDevice callback)
        {
            OnRecoveryModeLeave(new DeviceNotificationEventArgs(callback));
        }

        private void InternalDeleteDirectory(string path)
        {
            string full_path = FullPath(CurrentDirectory, path);
            string[] contents = GetFiles(path);
            for (int i = 0; i < contents.Length; i++)
            {
                DeleteFile(full_path + "/" + contents[i]);
            }

            contents = GetDirectories(path);
            for (int i = 0; i < contents.Length; i++)
            {
                InternalDeleteDirectory(full_path + "/" + contents[i]);
            }

            DeleteDirectory(path);
        }

        static char[] path_separators = { '/' };
        internal string FullPath(string path1, string path2)
        {

            if ((path1 == null) || (path1 == String.Empty))
            {
                path1 = "/";
            }

            if ((path2 == null) || (path2 == String.Empty))
            {
                path2 = "/";
            }

            string[] path_parts;
            if (path2[0] == '/')
            {
                path_parts = path2.Split(path_separators);
            }
            else if (path1[0] == '/')
            {
                path_parts = (path1 + "/" + path2).Split(path_separators);
            }
            else
            {
                path_parts = ("/" + path1 + "/" + path2).Split(path_separators);
            }

            string[] result_parts = new string[path_parts.Length];
            int target_index = 0;

            for (int i = 0; i < path_parts.Length; i++)
            {
                if (path_parts[i] == "..")
                {
                    if (target_index > 0)
                    {
                        target_index--;
                    }
                }
                else if ((path_parts[i] == ".") || (path_parts[i] == ""))
                {
                    // Do nothing
                }
                else
                {
                    result_parts[target_index++] = path_parts[i];
                }
            }

            return "/" + String.Join("/", result_parts, 0, target_index);
        }
        #endregion	// Private Methods
    }
}
