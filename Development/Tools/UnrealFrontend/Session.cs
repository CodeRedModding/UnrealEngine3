/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Windows;
using System.Windows.Threading;
using ConsoleInterface;
using Assembly = System.Reflection.Assembly;
using ChannelServices = System.Runtime.Remoting.Channels.ChannelServices;
using Color = System.Drawing.Color;
using Directory = System.IO.Directory;
using File = System.IO.File;
using IpcChannel = System.Runtime.Remoting.Channels.Ipc.IpcChannel;
using Path = System.IO.Path;
using ProcessStartInfo = System.Diagnostics.ProcessStartInfo;
using RemoteUCObject = UnrealConsoleRemoting.RemoteUCObject;



namespace UnrealFrontend
{
	public enum EUDKMode
	{
		None,
		UDK,
	}

	public interface IProcessManager
	{
		bool StartProcess(String ExecutablePath, String ExeArgs, String CWD, Platform InPlatform);
		void StopProcesses();
		bool WaitForActiveProcessesToComplete();
	}

	public class ProcessManager : IProcessManager
	{

		public ProcessManager(SessionLog InSessionLog)
		{
			SessionLog = InSessionLog;
			ActiveProcesses = new List<CommandletProcess>();
		}

		//AutoResetEvent ProcessesFinishedEvent = new AutoResetEvent(false);
		private SessionLog SessionLog { get; set; }

		/// The current commandlet process.
		List<CommandletProcess> ActiveProcesses { get; set; }
		/// Were processes recently killed by a user?
		bool bAnyProcessesKilledByUser = false;

		/// <summary>
		/// Async start the new commandlet
		/// </summary>
		/// <param name="ExecutablePath">The executable to run</param>
		/// <param name="ExeArgs">Argument to pass to the executable</param>
		/// <param name="CWD">The directory in which to start the exe</param>
		/// <param name="InPlatform">Platform we are targeting</param>
		/// <returns>true if the commandlet started; false if starting the commandlet failed</returns>
		public bool StartProcess(String ExecutablePath, String ExeArgs, String CWD, Platform InPlatform)
		{
			lock (this)
			{

				CommandletProcess NewProcess = new CommandletProcess(ExecutablePath, InPlatform);
				NewProcess.Exited += OnProcessExited;
				NewProcess.Output += SessionLog.HandleProcessOutput;

				try
				{
					NewProcess.Start(ExeArgs.ToString(), CWD);

					SessionLog.WriteCommandletEvent(Color.Green, string.Format("COMMANDLET \'{0} {1}\' STARTED IN '{2}'", Path.GetFileName(NewProcess.ExecutablePath), NewProcess.CommandLine, CWD));

					// if we have multiple in flight, assign an index to the process so that the output can have the 
					// index prepended to it (like multiple projects in Visual Studio)
					if (ActiveProcesses.Count == 1)
					{
						ActiveProcesses[0].ConcurrencyIndex = 1;
					}
					if (ActiveProcesses.Count >= 1)
					{
						NewProcess.ConcurrencyIndex = ActiveProcesses.Count + 1;
					}

					// add this process to list of actives
					ActiveProcesses.Add(NewProcess);

					return true;
				}
				catch (Exception ex)
				{
					SessionLog.WriteCommandletEvent(Color.Red, string.Format("COMMANDLET \'{0} {1}\' FAILED", Path.GetFileName(NewProcess.ExecutablePath), NewProcess.CommandLine));
					SessionLog.AddLine(Color.Red, ex.ToString());

					NewProcess.Dispose();

					return false;
				}
			}
		}

		/// Terminate the currently active processes. Cancel any queued work in the worker thread.
		public void StopProcesses()
		{
			lock (this)
			{
				foreach (CommandletProcess Process in ActiveProcesses)
				{
					if (!Process.HasExited)
					{
						SessionLog.WriteCommandletEvent(Color.Maroon, string.Format("STOPPING COMMANDLET '{0} {1}'...", Path.GetFileName(Process.ExecutablePath), Process.CommandLine));

						// We might want to terminate some operations if the user kills a process; WaitForActiveProcessesToComplete() will
						// return false because being killed isn't a natural way to exit a process.
						// For example, any active RunProfile will stop because  will return false.
						bAnyProcessesKilledByUser = true;

						Process.Kill();
					}
				}

				// no longer need to track any of these processes
				ActiveProcesses.Clear();
			}
		}

		/// <summary>
		/// Wait for the currently active commandlets to exit
		/// </summary>
		/// <returns>true if all processes terminated correctly; false if any exited with errors.</returns>
		public bool WaitForActiveProcessesToComplete()
		{
			bool bDidAllProcessesSucceed = true;

			
			// loop until we are out of processes to watch
			while (ActiveProcesses.Count > 0)
			{
				// give up some time
				System.Threading.Thread.Sleep(100);

				foreach (CommandletProcess Process in ActiveProcesses)
				{
					if (Process.HasExited)
					{
						// The step failed; should probably cancel all remaining queued work
						if (Process.ExitCode != 0)
						{
							bDidAllProcessesSucceed = false;
						}

						// this process is now done, no longer need it
						ActiveProcesses.Remove(Process);

						// break out of the iterator, so the foreach doesn't fall off the end
						break;
					}
				}
			}

			// Processes didn't succeed if we forced them to stop.
			bDidAllProcessesSucceed = bDidAllProcessesSucceed && !bAnyProcessesKilledByUser;
			lock (this)
			{
				// If any new processes are started, we don't want to assume
				bAnyProcessesKilledByUser = false;
			}

			return bDidAllProcessesSucceed;
		}


		/// Called when the active process exits.
		public void OnProcessExited(object sender, EventArgs e)
		{
			SessionLog.CommandletProcess_Exited(sender, e);
		}

	}




	public class Session : INotifyPropertyChanged
	{
		#region Singleton

		public static Session Current { get; private set; }
		public static void Init()
		{
			Current = new Session();
			Current.InitInternal();
		}

		#endregion

		/// Thread where all the actual work is done.
		private WorkerThread Worker {get; set;}

		/// Are we a UDK product?
		public EUDKMode UDKMode { get; private set; }

		/// Info string: changelist, engine version, etc.
		public String VersionString { get { return (UDKMode != EUDKMode.None) ? "" : String.Format("cl {0} v{1}", Versioning.Changelist, Versioning.EngineVersion); } }

		private UnrealControls.EngineVersioning Versioning = new UnrealControls.EngineVersioning();

		/// List of known platforms based on available DLLs.
		public List<ConsoleInterface.PlatformType> KnownPlatformTypes { get; private set; }

		/// Games that were discovered during startup.
		public List<String> KnownGames { get; private set; }

		/// Is RPCUtil.exe is available to us
		public bool IsRPCUtilityFound { get; private set; }

		/// Document for the log window.
		public UnrealControls.OutputWindowDocument OutpuDocument { get { return SessionLog.OutputDocument; } }

		public FrontendCommandLine CommandLineArgs { get; set; }

		// An event set by a task completing on a worker thread that triggers the session to quit
		public ManualResetEvent QuitOnCompleteSignal { get; set; }

		/// The profile matching OverrideProfileName read from the command line
		public Profile OverrideProfile { get; private set; }

		/// The profiles available to the user.
		public System.Collections.ObjectModel.ObservableCollection<Profile> Profiles { get; private set; }

		/// Default profiles serve as templates when the users need guidance.
		private List<Profile> DefaultProfiles { get; set; }

		/// Abstracts queueing up of messages into the UI thread.
		public SessionLog SessionLog { get; private set; }

		/// UFE settings; e.g. last selected profile.
		public Settings Settings { get; set; }
		
		/// Studio-wide UFE settings
		public StudioSettings StudioSettings { get; set; }

		private void InitInternal()
		{
			// Start up the worker thread.
			System.Windows.Threading.Dispatcher UIDispatcher = System.Windows.Application.Current.Dispatcher;
			Worker = new WorkerThread("UFE2MainWorker");
			Worker.PickedUpTask = (TaskType) =>
			{
				DispatchWorkStarted(UIDispatcher, TaskType);
			};
			Worker.WorkQueueEmpty = () =>
			{
				DispatchWorkCompleted(UIDispatcher);
			};
			

			// Bring up the ConsoleInterface; use the worker thread to avoid COM issues.
			Worker.QueueWork( new WorkerThread.VoidTask(delegate(){
				InitConsoleInterface();
			}));
			Worker.Flush();

			String RPCUtilFullpath = Path.GetFullPath("RPCUtility.exe");
			this.IsRPCUtilityFound = File.Exists(RPCUtilFullpath);
			

			// Check for UDK usage
			UDKMode = EUDKMode.None;
			if (Versioning.SpecialDefines.Contains("UDK=1"))
			{
				UDKMode = EUDKMode.UDK;
			}

			// Cache a list of all known platforms; this value does not change throughout the session.
			KnownPlatformTypes = new List<ConsoleInterface.PlatformType>();
			foreach (ConsoleInterface.Platform SomePlatform in ConsoleInterface.DLLInterface.Platforms)
			{
				KnownPlatformTypes.Add(SomePlatform.Type);
			}

			// Restrict the available platforms for UDK, UDKM
			if (UDKMode == EUDKMode.UDK)
			{
				KnownPlatformTypes.RemoveAll(SomePlatformType => SomePlatformType != PlatformType.PC && SomePlatformType != PlatformType.IPhone && SomePlatformType != PlatformType.MacOSX);
			}

			// Add in the known games that have a content folder
			KnownGames = UnrealControls.GameLocator.LocateGames().ConvertAll<String>(SomeGame=>SomeGame+"Game");


			this.SessionLog = new SessionLog();
			ProcessManager = new ProcessManager(SessionLog);
			this.Profiles = new System.Collections.ObjectModel.ObservableCollection<Profile>();

			this.Settings = new Settings();
			this.StudioSettings = new StudioSettings();
		}

		void InitConsoleInterface()
		{
			// Load in the CookerSync xml file (required only for PS3)
			ConsoleInterface.DLLInterface.LoadCookerSyncManifest();

			// force loading of platform dll's here
			ConsoleInterface.DLLInterface.LoadPlatforms(ConsoleInterface.PlatformType.All);
		}

		void ShutdownConsoleInterface()
		{
			ConsoleInterface.DLLInterface.UnloadPlatforms(ConsoleInterface.PlatformType.All);

			foreach (ConsoleInterface.Platform CurPlatform in ConsoleInterface.DLLInterface.Platforms)
			{
				CurPlatform.Dispose();
			}
		}

		/// The session of UFE is starting; load all the settins, profiles, etc.
		public void OnSessionStarting()
		{
			// Load Studio settings (e.g. per-game Cooker help URLs)
			if ( File.Exists( Settings.StudioSettingsFilename ) )
			{
				this.StudioSettings = UnrealControls.XmlHandler.ReadXml<UnrealFrontend.StudioSettings>(Settings.StudioSettingsFilename);
			}
			this.StudioSettings.AddGamesIfMissing(KnownGames);

			// Load UFE2 settings
			if (File.Exists( Settings.SettingsFilename ))
			{
				this.Settings = UnrealControls.XmlHandler.ReadXml<UnrealFrontend.Settings>(Settings.SettingsFilename);
			}

			PopulateProfiles();

			// Print something so the user is not confused about an empty window.
			SessionLog.AddLine( Color.Purple, String.Format("Unreal Frontend started {0}...", DateTime.Now) );
		}

		/// This session of UFE is shutting down; save out all settings/profiles.
		public void OnSessionClosing()
		{
			// Cancel all work. Shut down the worker thread.
			this.StopProcesses();
			Worker.QueueWork(() =>
			{
				ShutdownConsoleInterface();
			});
			Worker.Flush();
			Worker.BeginShutdown();

			SaveSessionSettings();
		}

		public void SaveSessionSettings()
		{
			SaveProfiles();

			// Save UFE2 settings
			{
				UnrealControls.XmlHandler.WriteXml(this.Settings, Settings.SettingsFilename, "");
			}

			// Save the studio settings
			{
				UnrealControls.XmlHandler.WriteXml(this.StudioSettings, Settings.StudioSettingsFilename, "");
			}
		}

		private void PopulateProfiles()
		{
			// Load default profiles for each known game
			DefaultProfiles = new List<Profile>();
			foreach (String Game in Session.Current.KnownGames)
			{
				// Find the default profile for game and try to load it
				String DefaultProfilePath = Path.GetFullPath(PathUtils.Combine("..", Game, "Config", "UnrealFrontend.DefaultProfiles"));
				if ( Directory.Exists(DefaultProfilePath) )
				{
					XmlUtils LoadProfileHelper = new XmlUtils();
					DefaultProfiles.AddRange(LoadProfileHelper.ReadProfiles(DefaultProfilePath));
				}
			}

			
			// Load user profiles
			XmlUtils XmlHelper = new XmlUtils();
			List<Profile> LoadedProfiles = XmlHelper.ReadProfiles(Settings.ProfileDirLocation);
			foreach (Profile SomeProfile in LoadedProfiles)
			{
				Current.Profiles.Add(SomeProfile);

				if (0 == string.Compare(SomeProfile.Filename, Current.CommandLineArgs.Profile, true))
				{
					Current.OverrideProfile = SomeProfile;
				}
			}


			// There are no user profiles, so populate list with defaults.
			if (Session.Current.Profiles.Count == 0)
			{				
				foreach (Profile SomeProfile in DefaultProfiles)
				{
					Session.Current.Profiles.Add( SomeProfile.Clone() );
				}

				if (Session.Current.Profiles.Count == 0)
				{
					Session.Current.Profiles.Add(new Profile("Default Profile", new ProfileData()));
				}
				
			}
		}

		private void SaveProfiles()
		{
			String ProfileLocation = Settings.ProfileDirLocation;

			// Save profiles
			foreach (Profile SomeProfile in Profiles)
			{
				SomeProfile.OnAboutToSave();
				XmlUtils XmlHelper = new XmlUtils();
				XmlHelper.WriteProfile(SomeProfile, SomeProfile.Filename);
			}

			// We never delete profiles until the session is closing.
			// Now is the time to clean up any profiles that are not in our
			// active profile list.
			List<String> FilesInProfileDir = FileUtils.GetFilesInProfilesDirectory(Settings.ProfileDirLocation);
			List<String> FilenamesOfProfilesInMemory = Session.Current.Profiles.ToList().ConvertAll<String>(SomeProfile => Path.Combine(ProfileLocation, SomeProfile.Filename));

			foreach (String SomeProfileOnDisk in FilesInProfileDir)
			{
				// If we have a profile on disk that is not in memory, let's delete it.
				if (null == FilenamesOfProfilesInMemory.Find(SomeProfileInMemory => SomeProfileInMemory.Equals(SomeProfileOnDisk, StringComparison.InvariantCultureIgnoreCase)))
				{
					try
					{
						File.Delete(SomeProfileOnDisk);
					}
					catch (Exception)
					{
					}

				}
			}
		}
		
		private DateTime LastTaskStartTime;
		/// How long the queued work took to complete.
		public double LastTaskElapsedSeconds { get; private set; }

		private bool mIsRefreshingTargets = false;
		/// True when a target refresh is ongoing
		public bool IsRefreshingTargets
		{
			get { return mIsRefreshingTargets; }
			set
			{
				if (mIsRefreshingTargets != value)
				{
					mIsRefreshingTargets = value;
					NotifyPropertyChanged("IsRefreshingTargets");
				}
			}
		}

		public bool IsUDK
		{
			get { return UDKMode == EUDKMode.UDK; }
		}

		private bool mIsWorking = false;
		/// When true, there is work in the work queue; we should be spinning a throbber and showing a stop.
		public bool IsWorking
		{
			get { return mIsWorking; }
			set
			{
				if (mIsWorking != value)
				{
					if (value == true)
					{
						LastTaskStartTime = DateTime.Now;
					}
					else
					{
						LastTaskElapsedSeconds = (DateTime.Now - LastTaskStartTime).TotalSeconds;
					}					
					mIsWorking = value;
					NotifyPropertyChanged("IsWorking");
				}
			}
		}

		/// Given a Profile, queue up execution of the Nth step in the pipeline, where
		/// N is specified by the Index. If bClean is true, CleanAndExecute the step.
		public void QueueExecuteStep( Profile InProfile, int Index, bool bClean )
		{
			// Clone data so the WorkerThread can safely do work on it.
			Profile ClonedProfile = InProfile.Clone();
			ClonedProfile.Validate();
			QueueWork( ()=>
			{
				if ( bClean )
				{
					ClonedProfile.Pipeline.Steps[Index].CleanAndExecute(ProcessManager, ClonedProfile);
				}
				else
				{
					ClonedProfile.Pipeline.Steps[Index].Execute(ProcessManager, ClonedProfile);	
				}
				
			}, null);
		}

		
		/// Run the entire pipeline for this profile. Certain frequent usage scenarios are encoded
		/// by RunOptions; e.g. Full recook or Cook INIs only.
		public void RunProfile(Profile ProfileToExecute, int StartingStep, Pipeline.ERunOptions RunOptions, EventWaitHandle SetOnComplete)
		{
			// Fix any incompatible/invalid settings the user may have set
			ProfileToExecute.Validate();

			// Make a copy because user changes to the profile should not affect the work already queued.
			ProfileToExecute = ProfileToExecute.Clone();

			// Find targets for the cloned profile
            ProfileToExecute.TargetsList.QueueUpdateTargets(true);

			QueueWork(() =>
			{
				bool bNoErrors = true;
				int CurrStep = 0;
				foreach (Pipeline.Step SomeStep in ProfileToExecute.Pipeline.Steps)
				{
					if (CurrStep < StartingStep)
					{
						CurrStep++;
						continue;
					}
					if (!SomeStep.ShouldSkipThisStep && bNoErrors)
					{
						Pipeline.Step StepToExecute = SomeStep;
						if (StepToExecute is Pipeline.Cook)
						{
							Pipeline.Cook CookStep = StepToExecute as Pipeline.Cook;
							if ( (RunOptions & Pipeline.ERunOptions.FullReCook) != 0 )
							{
								bNoErrors = CookStep.CleanAndExecute(ProcessManager, ProfileToExecute);
							}
							else if ( (RunOptions & Pipeline.ERunOptions.CookINIsOnly) != 0 )
							{
								bNoErrors = CookStep.ExecuteINIsOnly(ProcessManager, ProfileToExecute);
							}
							else
							{
								bNoErrors = CookStep.Execute(ProcessManager, ProfileToExecute);
							}
						}
						else if ( (RunOptions & Pipeline.ERunOptions.RebuildScript) != 0 && SomeStep is Pipeline.MakeScript)
						{
							bNoErrors = StepToExecute.CleanAndExecute(ProcessManager, ProfileToExecute);
						}
						else
						{
							bNoErrors = StepToExecute.Execute(ProcessManager, ProfileToExecute);
						}

						if (bNoErrors)
						{
							bNoErrors = ProcessManager.WaitForActiveProcessesToComplete();
						}
						
					}
					else
					{
						SessionLog.AddLine(bNoErrors ? Color.DarkMagenta : Color.Red, String.Format("\n[Skipping {0}]", SomeStep.StepName));
					}
				
					CurrStep++;
				}
				if( bNoErrors )
				{
					SessionLog.AddLine(Color.Green, String.Format("\n[{0}] ALL PIPELINE STEPS COMPLETED SUCCESSFULLY.", DateTime.Now.ToString("MMM d, h:mm tt")));
				}
				else
				{
					SessionLog.AddLine(Color.Red, String.Format("\n[{0}] PIPELINE FAILED TO COMPLETE.", DateTime.Now.ToString("MMM d, h:mm tt")));
                    QuitOnCompleteSignal = null;    // Don't autoquit, leave UFE open so the user can see what the problem was
				}
			}, SetOnComplete);
		}

		/// Queue up a reboot of all active tasks for the fiven profile.
		public void QueueReboot( Profile ProfileToReboot )
		{
			this.QueueWork( () => Pipeline.Launch.CheckedReboot(ProfileToReboot, false, null), null );
		}

		public delegate void QueueWorkDelegate();

		public bool HasQueuedWork { get { return Worker.HasWork(); } }

		private void QueueWork(QueueWorkDelegate InWork, EventWaitHandle SetOnComplete)
		{
			Worker.QueueWork(() =>
								{
									InWork();
									ProcessManager.WaitForActiveProcessesToComplete();
								}, SetOnComplete);
		}

		/// Queue up a refresh task; refresh tasks are executed which higher priority than any other task in the worker queue.
		public void QueueRefreshTask(QueueWorkDelegate InWork)
		{
			Worker.QueuePlatformRefresh(() =>
			{
				InWork();
				ProcessManager.WaitForActiveProcessesToComplete();
			});
		}

		/// The work thread is empty; queue up an update on the UI thread.
		private void DispatchWorkCompleted(System.Windows.Threading.Dispatcher UIDispatcher)
		{
			UIDispatcher.BeginInvoke(new WorkerThread.VoidTask(delegate()
			{
				IsWorking = false;
				IsRefreshingTargets = false;
			}));
		}

		/// The worker thread has new tasks; queue u pan update in the UI thread.
		private void DispatchWorkStarted(System.Windows.Threading.Dispatcher UIDispatcher, WorkerThread.ETaskType TaskType)
		{
			UIDispatcher.BeginInvoke(new WorkerThread.VoidTask(delegate()
			{
				if (TaskType == WorkerThread.ETaskType.Refresh)
				{
					IsWorking = false;
					IsRefreshingTargets = true;
				}
				else
				{
					IsWorking = true;
					IsRefreshingTargets = false;
				}
			}));
		}

		/// Launch unreal console for each active target; do it on the worker thread.
		public void QueueLaunchUnrealConsole( Profile InProfile )
		{
			Worker.QueueWork(() =>
			{
				bool bLaunchedUnrealConsole = false;
				foreach (Target SomeTarget in InProfile.TargetsList.Targets)
				{
					if (SomeTarget.ShouldUseTarget)
					{
						if (!SomeTarget.TheTarget.IsConnected)
						{
							if (!SomeTarget.TheTarget.Connect())
							{
								SessionLog.AddLine(Color.Red, "Failed connection attempt with target \'" + SomeTarget.Name + "\'!");
							}
						}
						bLaunchedUnrealConsole |= this.LaunchUnrealConsole(SomeTarget.TheTarget, InProfile.Launch_ClearUCWindow);
					}
				}

				if (!bLaunchedUnrealConsole)
				{
					this.LaunchUnrealConsole(null, false);
				}
			});
		}

		/// <summary>
		/// Launch the editor
		/// </summary>
		/// <param name="InProfile">Profile for which to launch the editor</param>
		public void QueueLaunchUnrealEd(Profile InProfile)
		{
			Worker.QueueWork(() =>
			{
				String ExecutablePath = UnrealFrontend.Pipeline.CommandletStep.GetExecutablePath(InProfile, true, false);
				String Arguments = "editor";
				
				// Some users like to shut down their editor when cooking.
				// If they are testing a specific map, the editor launch into that map.
				if (!InProfile.LaunchDefaultMap)
				{
					Arguments += " " + InProfile.MapToPlay.Name;
				}

				UnrealFrontend.Pipeline.Launch.ExecuteProgram(ExecutablePath, Arguments, false);
				SessionLog.AddLine(Color.Green, "Starting: " + ExecutablePath + " " + Arguments);
			});
		}

		/// <summary>
		/// @todo : This needs some serious cleanup. Ported over from the old UFE.
		/// </summary>
		#region Commandlet Stuff (needs reworking)

		// used to talk to unreal console
		private IpcChannel UCIpcChannel;

		/// <summary>
		/// Launches a new instance of UnrealConsole for the specified target.
		/// </summary>
		/// <param name="Target">The target for unreal console to connect to.</param>
		public bool LaunchUnrealConsole(ConsoleInterface.PlatformTarget Target, bool bClearUnrealWindow)
		{
			bool UCLaunched = false;

			while (!UCLaunched)
			{
				try
				{
					if (UCIpcChannel == null)
					{
						UCIpcChannel = new IpcChannel();
						ChannelServices.RegisterChannel(UCIpcChannel, true);
					}

					string Dir = Path.GetDirectoryName(Assembly.GetEntryAssembly().Location);
					Dir = Path.Combine(Dir, "UnrealConsole.exe");
					string TargetName = string.Empty;

					if (Target != null)
					{
						// everything except the PS3 has an accessible debug channel IP but the PS3 has an always accessible name
						if (Target.ParentPlatform.Type == ConsoleInterface.PlatformType.PS3 ||
							Target.ParentPlatform.Type == ConsoleInterface.PlatformType.NGP)
						{
							TargetName = Target.Name;
						}
						else
						{
							TargetName = Target.DebugIPAddress.ToString();
						}

						if (Target.IsConnected && Target.ParentPlatform.Type == ConsoleInterface.PlatformType.Xbox360)
						{
							while (Target.State == ConsoleInterface.TargetState.Rebooting)
							{
								System.Threading.Thread.Sleep(50);
							}
						}
					}

					// See if there is a UC window already open...
					System.Diagnostics.Process[] LocalProcesses = System.Diagnostics.Process.GetProcessesByName("UnrealConsole");

					// Make sure the UC window is in our directory (so that people working in separate branches can open multiple Unreal
					bool ProcInDir = false;
					if (LocalProcesses.Length != 0)
					{
						foreach (System.Diagnostics.Process CurProc in LocalProcesses)
						{
							if (Path.GetDirectoryName(Assembly.GetEntryAssembly().Location).Equals(Path.GetDirectoryName(CurProc.MainModule.FileName), StringComparison.InvariantCultureIgnoreCase))
							{
								ProcInDir = true;
								break;
							}
						}
					}


					if (LocalProcesses.Length == 0 || !ProcInDir)
					{
						// Spawn a new instance of UC if none are already running
						ProcessStartInfo Info = (Target!=null)
							? new ProcessStartInfo(Dir, string.Format("-platform={0} -target={1}", Target.ParentPlatform.Name, TargetName))
							: new ProcessStartInfo(Dir, "");
						Info.CreateNoWindow = false;
						Info.UseShellExecute = true;
						Info.Verb = "open";

						SessionLog.AddLine(Color.Green, "Spawning: " + Dir + " " + Info.Arguments);
						System.Diagnostics.Process.Start(Info).Dispose();
						UCLaunched = true;
					}
					else if (Target != null)
					{
						// Create a new tab in and existing instance
						foreach (System.Diagnostics.Process CurProc in LocalProcesses)
						{
							if (Path.GetDirectoryName(Assembly.GetEntryAssembly().Location).Equals(Path.GetDirectoryName(CurProc.MainModule.FileName)))
							{
								try
								{
									// Get a remote object in UnrealConsole
									RemoteUCObject RemoteObj = (RemoteUCObject)Activator.GetObject(typeof(RemoteUCObject), string.Format("ipc://{0}/{0}", CurProc.Id.ToString()));

									// This always returns true
									bool bFoundTarget = RemoteObj.HasTarget(Target.ParentPlatform.Name, TargetName);
									if (bFoundTarget)
									{
										try
										{
											SessionLog.AddLine(Color.Green, " ... using existing UnrealConsole: -platform=" + Target.ParentPlatform.Name + " -target=" + TargetName);
											RemoteObj.OpenTarget(Target.ParentPlatform.Name, TargetName, bClearUnrealWindow);

											UCLaunched = true;
										}
										catch (Exception ex)
										{
											string ErrStr = ex.ToString();
											System.Diagnostics.Debug.WriteLine(ErrStr);

											SessionLog.AddLine(Color.Orange, "Warning: Could not open target in UnrealConsole instance \'" + CurProc.Id.ToString() + "\'");
										}

										break;
									}
								}
								catch (Exception)
								{
									SessionLog.AddLine(Color.Orange, "Failed to connect to existing instance of UnrealConsole. Please select it manually.");
									UCLaunched = true;
								}
							}
						}
					}

					foreach (System.Diagnostics.Process CurProc in LocalProcesses)
					{
						CurProc.Dispose();
					}
				}
				catch (Exception ex)
				{
					string ErrStr = ex.ToString();
					System.Diagnostics.Debug.WriteLine(ErrStr);

					SessionLog.AddLine(Color.Red, ErrStr);

					// Fatal error - break out
					UCLaunched = true;
				}
			}

			return (UCLaunched);
		}

		#endregion


		#region INotifyPropertyChanged

		public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
		private void NotifyPropertyChanged(String PropertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new System.ComponentModel.PropertyChangedEventArgs(PropertyName));
			}
		}

		private void AssignBool(String InPropName, ref bool InOutProp, bool InNewValue)
		{
			if (InOutProp != InNewValue)
			{
				InOutProp = InNewValue;
				NotifyPropertyChanged(InPropName);
			}
		}

		#endregion

		private IProcessManager ProcessManager { get; set; }

		internal void WaitForProcesses()
		{
			Worker.Flush();
		}

		internal void StopProcesses()
		{
			Worker.CancelQueuedWork();
			ProcessManager.StopProcesses();
			//Worker.Flush();
		}
	}

}
