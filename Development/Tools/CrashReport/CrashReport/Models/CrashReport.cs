// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Linq;
using System;
using System.Data.Linq; 

namespace CrashReport.Models
{
	partial class CrashReportDataContext
	{
	}

	public struct BuggsResult
	{
		public Bugg Bugg;
		public int CrashesInTimeFrame;

		public BuggsResult(Bugg b, Int32 count)
		{
			this.Bugg = b;
			this.CrashesInTimeFrame = count;
		}
	}

	public partial class Bugg
	{
		public EntitySet<Crash> mCrashes { get; set; }
		public IEnumerable<string> mUsers { get; set;}

		public int CrashesInTimeFrame { get; set; }

		public EntitySet<Crash> GetCrashes()
		{
			return GetCrashes(true);
		}

		public EntitySet<Crash> GetCrashes(Boolean FillCallStacks)
		{
			Bugg b = this;

			if (mCrashes == null)
			{
				mCrashes = this.Crashes;
			}

			int CrashCount = mCrashes.Count;
			if (this.NumberOfCrashes == CrashCount )
			{
				// Do Nothing everthing is in order
			}
			else
			{
				this.NumberOfCrashes = CrashCount;

				if (this.NumberOfCrashes > 0)
				{
					IBuggRepository mBuggRepository = new BuggRepository();
					mBuggRepository.UpdateBuggData(this, mCrashes);
				}
				else
				{
					// Do Nothing
					// TODO Decide if we should do something here
				}
			}

			if (FillCallStacks)
			{
				// Just fill the CallStackContainers
				foreach (Crash c in mCrashes)
				{
					if (c.CallStackContainer == null)
					{
						c.CallStackContainer = c.GetCallStack();
					}
				}
			}

			return mCrashes;
		}

		public IList<string> GetFunctionCalls()
		{
			return this.GetFunctionCalls(4);
		}
		
		public IList<string> GetFunctionCalls(int max)
		{
			IBuggRepository mBuggRepository = new BuggRepository();
			IList<string> Results = new List<string>();
			IList<int> Ids = new List<int>();
		
			Results = mBuggRepository.GetFunctionCalls(Pattern);

			return Results;
		}

		public IList<string> GetCallStackByPattern(string Pattern)
		{
			// Not 100% positive this is the best way to do this 
			//but I figure it's better than copying and pasting the function
			Crash c = new Crash();
			var Results = c.GetCallStackByPattern(Pattern);
			return Results;
		}

		public string[] GetFunctionCallIdsByPattern(string Pattern)
		{
			var Ids = Pattern.Split(new char[] { '+' });
			return Ids;
		}


		/// <summary>
		/// http://www.codeproject.com/KB/linq/linq-to-sql-many-to-many.aspx
		/// </summary>
		private System.Data.Linq.EntitySet<Crash> _crashes;
		public System.Data.Linq.EntitySet<Crash> Crashes
		{
			get
			{
				if (_crashes == null)
				{
					_crashes = new System.Data.Linq.EntitySet<Crash>(OnCrashesAdd, OnCrashesRemove);
					_crashes.SetSource(Bugg_Crashes.Select(c => c.Crash));
				}
				return _crashes;
			}
			set
			{
				_crashes.Assign(value);
			}
		}

		[System.Diagnostics.DebuggerNonUserCode]
		private void OnCrashesAdd(Crash entity)
		{
			this.Bugg_Crashes.Add(new Bugg_Crash { Bugg = this, Crash = entity });
			SendPropertyChanged(null);
		}

		[System.Diagnostics.DebuggerNonUserCode]
		private void OnCrashesRemove(Crash entity)
		{
			var buggCrash = this.Bugg_Crashes.FirstOrDefault(
				c => c.BuggId == Id
				&& c.CrashId == entity.Id);
			this.Bugg_Crashes.Remove(buggCrash);
			SendPropertyChanged(null);
		}
		// End 
	}

	public partial class Crash
	{
		private string mLogUrl = string.Empty;
		private string mDumpUrl = string.Empty;
		private string mVideoUrl = string.Empty;

		public CallStackContainer CallStackContainer { get; set; }

		public string GetLogUrl()
		{
			if (mLogUrl == string.Empty)
			{
				mLogUrl = Properties.Settings.Default.AutoReportFile_URL+this.Id+"_Launch.log";
			}
			return mLogUrl;
		}

		public string GetDumpUrl()
		{
			if (mDumpUrl == string.Empty)
			{
				mDumpUrl = Properties.Settings.Default.AutoReportFile_URL + this.Id + "_MiniDump.dmp";
			}
			return mDumpUrl;
		}

		public string GetVideoUrl()
		{
			if (mVideoUrl == string.Empty)
			{
				mVideoUrl = Properties.Settings.Default.AutoReportVideo_URL + this.Id + "_CrashVideo.avi";
			}
			return mVideoUrl;
		}
		
		public IList<CallStackEntry> GetCallStackEntries()
		{
			int max = 3;
			return this.GetCallStackEntries(max);
		}

		public IList<CallStackEntry> GetCallStackEntries(int max)
		{
				
			IList<CallStackEntry> Results = new List<CallStackEntry>();

			try
			{
				int count = this.CallStackContainer.GetEntries().Count;
				int total = count;
				if (count > max) count = max;
				int i = 0;

				foreach (CallStackEntry e in this.CallStackContainer.GetEntries())
				{
					//CallStackEntry e = CallStackContainer.GetEntry(i);
					if (e.GetTrimmedFunctionName(60).StartsWith("Address"))
					{
						// Do Nothing because this is an Address line. 
					}
					else
					{
						Results.Add(e);
						i++;
					}
					if (i == count)
					{
						break;
					}
				}
			}
			catch (NullReferenceException)
			{
				CallStackEntry empty = new CallStackEntry();
				Results.Add(empty);
			}
			return Results;
		}

		public IList<string> GetCallStackErrors()
		{
			int MinLength = 5;
			int MaxLength = 50;
			string Exclude = "Address";
			return this.GetCallStackErrors(MinLength, MaxLength, Exclude);
		}

		public IList<string> GetCallStackErrors(int MinLength, int MaxLength, string Exclude)
		{
				IList<string> Results = new List<string>();
				char[] sep = Environment.NewLine.ToCharArray();
				try
				{
					if (CallStackContainer.ErrorMessage != null)
					{
						string[] Errors = CallStackContainer.ErrorMessage.Split(sep);

						foreach (string error in Errors)
						{
							if (error.Length < MinLength || error.StartsWith(Exclude))
							{
								//do nothing	
							}
							else
							{
								string e;
								if (error.Length > MaxLength)
								{
									e = error.Substring(0, MaxLength) + "...";
								}
								else
								{
									e = error;
								}
								Results.Add(e);
							}
						}
					}
					else
					{
						Results.Add("");
					}
				}
				catch (NullReferenceException e)
				{
					Results.Add("");
				}
					
				return Results;
		}

		public string[] GetCommandLines()
		{
			string[] Results = Regex.Split(CommandLine, @"(?<=[\\? ])");
			return Results;
		}

		public string[] GetTimeOfCrash()
		{
			string[] Results = Regex.Split(TimeOfCrash.ToString(), @"(?<=[ ])");
			return Results;
		}

		public CallStackContainer GetCallStack()
		{
			ICrashRepository CrashRepository = new CrashRepository();
			return CrashRepository.GetCallStack(this);
		}

		public IList<string> GetCallStackByPattern(string Pattern)
		{
			IBuggRepository mBuggRepository = new BuggRepository();
			return mBuggRepository.GetFunctionCalls(Pattern);
		}

		public string[] GetFunctionCallIdsByPattern(string Pattern)
		{
			var Ids = Pattern.Split(new char[] { '+' });
			return Ids;
		}

		/// <summary>
		/// http://www.codeproject.com/KB/linq/linq-to-sql-many-to-many.aspx
		/// </summary>
		private System.Data.Linq.EntitySet<Bugg> _buggs;
		public System.Data.Linq.EntitySet<Bugg> Buggs
		{
			get
			{
				if (_buggs == null)
				{
					_buggs = new System.Data.Linq.EntitySet<Bugg>(OnBuggsAdd, OnBuggsRemove);
					_buggs.SetSource(Bugg_Crashes.Select(c => c.Bugg));
				}
				return _buggs;
			}
			set
			{
				_buggs.Assign(value);
			}
		}

		[System.Diagnostics.DebuggerNonUserCode]
		private void OnBuggsAdd(Bugg entity)
		{
			this.Bugg_Crashes.Add(new Bugg_Crash { Crash = this, Bugg = entity });
			SendPropertyChanged(null);
		}

		[System.Diagnostics.DebuggerNonUserCode]
		private void OnBuggsRemove(Bugg entity)
		{
			var buggCrash = this.Bugg_Crashes.FirstOrDefault(
				c => c.CrashId == Id
				&& c.BuggId == entity.Id);
			this.Bugg_Crashes.Remove(buggCrash);
			SendPropertyChanged(null);
		}

		private System.Data.Linq.EntitySet<FunctionCall> _functionCalls;
		public System.Data.Linq.EntitySet<FunctionCall> FunctionCalls
		{
			get
			{
				if (_functionCalls == null)
				{
					_functionCalls = new System.Data.Linq.EntitySet<FunctionCall>(OnFunctionCallsAdd, OnFunctionCallsRemove);
					_functionCalls.SetSource(Crash_FunctionCalls.Select(c => c.FunctionCall));
				}
				return _functionCalls;
			}
			set
			{
				_functionCalls.Assign(value);
			}
		}

		[System.Diagnostics.DebuggerNonUserCode]
		private void OnFunctionCallsAdd(FunctionCall entity)
		{
			this.Crash_FunctionCalls.Add(new Crash_FunctionCall { Crash = this, FunctionCall = entity });
			SendPropertyChanged(null);
		}

		[System.Diagnostics.DebuggerNonUserCode]
		private void OnFunctionCallsRemove(FunctionCall entity)
		{
			var buggCrash = this.Crash_FunctionCalls.FirstOrDefault(
				c => c.CrashId == Id
				&& c.FunctionCallId == entity.Id);
			this.Crash_FunctionCalls.Remove(buggCrash);
			SendPropertyChanged(null);
		}
		// End 
	}
}