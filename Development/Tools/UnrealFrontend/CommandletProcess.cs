/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

namespace UnrealFrontend
{
	public class CommandletOutputEventArgs : EventArgs
	{
		string mMsg = string.Empty;

		public string Message
		{
			get { return mMsg; }
		}

		public CommandletOutputEventArgs(string Msg)
		{
			if (Msg == null)
			{
				throw new ArgumentNullException("Msg");
			}

			mMsg = Msg;
		}
	}


	public class CommandletProcess : IDisposable
	{
		Process mCmdletProc;
		string mExecutablePath;
		string mCmdLine;
		ConsoleInterface.Platform mPlatform;
		Pipes.NamedPipe mLogPipe;
		Thread mOutputThread;

		public event EventHandler<EventArgs> Exited;
		public event EventHandler<CommandletOutputEventArgs> Output;
		/** If there are multiple processes in flight, this will be > 0, and can be used to modify the output (like Visual Studio) */
		public int ConcurrencyIndex;

		public int ExitCode
		{
			get
			{
				if (mCmdletProc != null)
				{
					return mCmdletProc.ExitCode;
				}

				return 0;
			}
		}

		public bool HasExited
		{
			get
			{
				lock (this)
				{
					if (mCmdletProc != null)
					{
						return mCmdletProc.HasExited;
					}

					return true;
				}
			}
		}

		public string ExecutablePath
		{
			get { return mExecutablePath; }
		}

		public string CommandLine
		{
			get { return mCmdLine; }
		}

		public CommandletProcess(string ExecutablePath, ConsoleInterface.Platform Platform)
		{
			if (ExecutablePath == null)
			{
				throw new ArgumentNullException("ExecutablePath");
			}

			if (Platform == null)
			{
				throw new ArgumentNullException("Platform");
			}

			this.mExecutablePath = ExecutablePath;
			this.mPlatform = Platform;
		}

		public void Start( string CmdLine, string CWD )
		{
			Start(CmdLine, CWD, true);
		}

		public bool Start(string CmdLine, string CWD, bool bCreateNoWindow)
		{
			lock (this)
			{
				if (mCmdletProc != null && !mCmdletProc.HasExited)
				{
					return false;
				}

				this.mCmdLine = CmdLine;

				Regex GameExeRegex = new Regex( ".*(game|udk|udkmobile)(.exe|(-win(32|64)-(debug|shipping).exe))" );
				bool bIsGameExe = GameExeRegex.IsMatch( Path.GetFileName( mExecutablePath ).ToLower() );

				ProcessStartInfo Info = new ProcessStartInfo(mExecutablePath, mCmdLine);
				Info.CreateNoWindow = bCreateNoWindow;
				Info.WorkingDirectory = CWD;
				Info.UseShellExecute = false;
				Info.RedirectStandardError = !bIsGameExe;
				Info.RedirectStandardOutput = !bIsGameExe;
				Info.RedirectStandardInput = true;

				mCmdletProc = new Process();
				mCmdletProc.EnableRaisingEvents = true;
				mCmdletProc.StartInfo = Info;
				mCmdletProc.Exited += new EventHandler(mCmdletProc_Exited);
				mCmdletProc.OutputDataReceived += new DataReceivedEventHandler(mCmdletProc_OutputDataReceived);

				// For iPhone targets, set up some default environment variables, if necessary
				if (mPlatform.Type == ConsoleInterface.PlatformType.IPhone)
				{
					// The set of required environment variables
					Dictionary<string, string> IPhoneEnvironmentVariables = new Dictionary<string, string>();

					// Common variables
					IPhoneEnvironmentVariables.Add("ue3.iPhone_SigningServerName", "a1487");

					if (mCmdLine.StartsWith("PackageDeviceDistribution"))
					{
						IPhoneEnvironmentVariables.Add("ue3.iPhone_SigningPrefix", "Distro_");
						IPhoneEnvironmentVariables.Add("ue3.iPhone_CertificateName", "iPhone Distribution");
					}
					else
					{
						IPhoneEnvironmentVariables.Add("ue3.iPhone_CertificateName", "iPhone Developer");
					}

					// If a required environment variable isn't defined, provide a good default
					foreach (KeyValuePair<string, string> KVP in IPhoneEnvironmentVariables)
					{
						if (mCmdletProc.StartInfo.EnvironmentVariables.ContainsKey(KVP.Key) == false)
						{
							mCmdletProc.StartInfo.EnvironmentVariables.Add(KVP.Key, KVP.Value);
						}
					}
				}

				bool bProcessStarted = mCmdletProc.Start();

				if (!bIsGameExe)
				{
					mCmdletProc.BeginOutputReadLine();
				}
				else
				{
					mLogPipe = new Pipes.NamedPipe();

					if (mLogPipe.Connect(mCmdletProc))
					{
						if (mOutputThread != null && mOutputThread.IsAlive)
						{
							mOutputThread.Abort();
						}

						mOutputThread = new Thread(new ParameterizedThreadStart(PollOutput));
						mOutputThread.Start(this);
					}
				}

				return bProcessStarted;
			}
		}

		void mCmdletProc_OutputDataReceived(object sender, DataReceivedEventArgs e)
		{
			try
			{
				if (Output != null && e.Data != null)
				{
					Output(this, new CommandletOutputEventArgs(e.Data));
				}
			}
			catch (Exception ex)
			{
				System.Diagnostics.Debug.WriteLine(ex.ToString());
			}
		}

		private const string UNI_COLOR_MAGIC = "`~[~`";
		void PollOutput(object State)
		{
			try
			{
				while (mCmdletProc != null && !mCmdletProc.HasExited)
				{
					string Msg = mLogPipe.Read();

					if (Output != null && Msg.Length > 0 && !Msg.StartsWith(UNI_COLOR_MAGIC))
					{
						Output(this, new CommandletOutputEventArgs(Msg.Replace("\r\n", "")));
					}
				}
			}
			catch (ThreadAbortException)
			{
			}
			catch (Exception ex)
			{
				System.Diagnostics.Debug.WriteLine(ex.ToString());
			}
		}

		void mCmdletProc_Exited(object sender, EventArgs e)
		{
			if (Exited != null)
			{
				Exited(this, e);
			}

			if (mOutputThread != null && mOutputThread.IsAlive)
			{
				mOutputThread.Abort();
			}
		}


		private void KillProcesses()
		{
			List<Process> ChildProcesses = ProcessHelper.GetChildProcesses(mCmdletProc);

			// Kill the parent first.
			mCmdletProc.Kill();
			foreach( Process Proc in ChildProcesses )
			{
				Proc.Kill();
			}
		}

		public void Kill()
		{
			lock (this)
			{
				if (mCmdletProc != null)
				{
					KillProcesses();
				}

				if (mOutputThread != null && mOutputThread.IsAlive)
				{
					mOutputThread.Abort();
				}
			}
		}

		#region IDisposable Members

		public void Dispose()
		{
			if (mCmdletProc != null)
			{
				mCmdletProc.Exited -= new EventHandler(mCmdletProc_Exited);
				mCmdletProc.OutputDataReceived -= new DataReceivedEventHandler(mCmdletProc_OutputDataReceived);
				mCmdletProc.Dispose();
				mCmdletProc = null;
			}

			if (mOutputThread != null && mOutputThread.IsAlive)
			{
				mOutputThread.Abort();
			}
		}

		#endregion
	}
}

public static class ProcessHelper
{
	[DllImport("KERNEL32.DLL")]
	private static extern int OpenProcess(EDesiredAccess DesiredAccess,
	bool bInheritHandle, int ProcessId);
	[DllImport("KERNEL32.DLL")]
	private static extern int CloseHandle(int Object);
	[DllImport("NTDLL.DLL")]
	private static extern int NtQueryInformationProcess(int Process, PROCESSINFOCLASS ProcessInfoClass, ref PROCESS_BASIC_INFORMATION ProcessInfo, int InfoSize, ref int ReturnLength);
	private enum PROCESSINFOCLASS : int
	{
		ProcessBasicInformation = 0,
		ProcessQuotaLimits,
		ProcessIoCounters,
		ProcessVmCounters,
		ProcessTimes,
		ProcessBasePriority,
		ProcessRaisePriority,
		ProcessDebugPort,
		ProcessExceptionPort,
		ProcessAccessToken,
		ProcessLdtInformation,
		ProcessLdtSize,
		ProcessDefaultHardErrorMode,
		ProcessIoPortHandlers,
		ProcessPooledUsageAndLimits,
		ProcessWorkingSetWatch,
		ProcessUserModeIOPL,
		ProcessEnableAlignmentFaultFixup,
		ProcessPriorityClass,
		ProcessWx86Information,
		ProcessHandleCount,
		ProcessAffinityMask,
		ProcessPriorityBoost,
		MaxProcessInfoClass
	} ;

	[StructLayout(LayoutKind.Sequential)]
	private struct PROCESS_BASIC_INFORMATION
	{
		public int ExitStatus;
		public int PebBaseAddress;
		public int AffinityMask;
		public int BasePriority;
		public int UniqueProcessId;
		public int InheritedFromUniqueProcessId;
		public int Size
		{
			get { return (6 * 4); }
		}
	} ;

	private enum EDesiredAccess : int
	{
		DELETE = 0x00010000,
		READ_CONTROL = 0x00020000,
		WRITE_DAC = 0x00040000,
		WRITE_OWNER = 0x00080000,
		SYNCHRONIZE = 0x00100000,
		STANDARD_RIGHTS_ALL = 0x001F0000,
		PROCESS_TERMINATE = 0x0001,
		PROCESS_CREATE_THREAD = 0x0002,
		PROCESS_SET_SESSIONID = 0x0004,
		PROCESS_VM_OPERATION = 0x0008,
		PROCESS_VM_READ = 0x0010,
		PROCESS_VM_WRITE = 0x0020,
		PROCESS_DUP_HANDLE = 0x0040,
		PROCESS_CREATE_PROCESS = 0x0080,
		PROCESS_SET_QUOTA = 0x0100,
		PROCESS_SET_INFORMATION = 0x0200,
		PROCESS_QUERY_INFORMATION = 0x0400,
		PROCESS_ALL_ACCESS = SYNCHRONIZE | 0xFFF
	}

	public static int GetParentId(Process ProcessToCheck)
	{
		int ParentID = 0;
		int ProcessHandle = OpenProcess(EDesiredAccess.PROCESS_QUERY_INFORMATION, false, ProcessToCheck.Id);
		if (ProcessHandle != 0)
		{
			PROCESS_BASIC_INFORMATION ProcessInfo = new PROCESS_BASIC_INFORMATION();
			int Size = 0;
			if (NtQueryInformationProcess(ProcessHandle, PROCESSINFOCLASS.ProcessBasicInformation, ref ProcessInfo, ProcessInfo.Size, ref Size) != -1)
			{
				ParentID = ProcessInfo.InheritedFromUniqueProcessId;
			}
	
			CloseHandle(ProcessHandle);
			
		}

		return ParentID; 
	}	

	public static List<Process> GetChildProcesses(Process ProcessToCheck)
	{
		List<Process> ProcessList = new List<Process>();
			
		foreach( Process Proc in Process.GetProcesses() )
		{
			if (ProcessHelper.GetParentId(Proc) == ProcessToCheck.Id)
			{
				// Add this process to the list of child processes
				ProcessList.Add(Proc);
				// Add any of this processes child processes.
				ProcessList.AddRange(GetChildProcesses(Proc));
			}
		}

		return ProcessList;
	}
}
