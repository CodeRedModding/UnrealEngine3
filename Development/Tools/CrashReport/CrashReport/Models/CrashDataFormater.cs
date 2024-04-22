// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace CrashReport.Models
{
	public class CrashDataFormater
	{
	}
	public class CallStackEntry
	{
		string mFileName = "";
		string mFilePath = "";
		string mFunctionName = "";
		int? mLineNumber;

		public string FileName
		{
			get { return mFileName; }
		}

		public string FilePath
		{
			get { return mFilePath; }
		}

		public string FunctionName
		{
			get { return mFunctionName; }
		}

		public int? LineNumber
		{
			get { return mLineNumber; }
		}

		public CallStackEntry()
		{
		}

		public CallStackEntry(string FileName, string FilePath, string FuncName, int? LineNumber)
		{
			mFileName = FileName;
			mFilePath = FilePath;
			mFunctionName = FuncName;
			mLineNumber = LineNumber;
		}

		public string GetTrimmedFunctionName()
		{
			int MaxLength = 40;
			return this.GetTrimmedFunctionName(MaxLength);
		}

		public string GetTrimmedFunctionName(int MaxLength)
		{
			var line = FunctionName;
			if (MaxLength > 0)
			{
				var length = line.Length;
				if (length > MaxLength) length = MaxLength;
				line = line.Substring(0, length);
			}
			return line;
		}

		public string GetModuleName()
		{
			var fullPath = mFilePath;
			var moduleName = String.Empty;
			var mType = String.Empty;
			var mPlatform = String.Empty;
			
			//Parse for ue4 modules
			if(fullPath.Contains("ue4"))
			{
				// Look for Engine/Source then 
				if (fullPath.Contains("engine\\source"))
				{
					// engine/source/<type>/[<platform>]/*ModuleName*/<ignore>
					// type = { editor, runtime, developer, thirdparty}
					// platform = {???}
					// First pass at platforms core, Dingo, Mac, Windows, Mocha
					string[] initialSeparator = {"engine\\source"};
					string[] parts = fullPath.Split(initialSeparator, StringSplitOptions.RemoveEmptyEntries);
					string[] pathSeparator = {"\\"};
					
					string[] typeStrings = {"editor", "runtime", "developer", "ThirdParty" };
					
					string[] platformStrings = {"Dingo", "Mac", "Windows", "Mocha"};

					string[] moduleParts = parts[1].Split(pathSeparator, StringSplitOptions.RemoveEmptyEntries);
					moduleName = moduleParts[1];
					foreach (var typeString in typeStrings)
					{
						if (moduleParts[0].StartsWith(typeString))
						{
							mType = moduleParts[0];
							break;
						}
					}
					
					foreach (var platformString in platformStrings)
					{
						if (moduleParts.Length > 2 && moduleParts[1].StartsWith(platformString))
						{
							mPlatform = moduleParts[1];
							moduleName = moduleParts[2];
							break;
						}
					}
				}
				else
				{
					// Else look for ue4/<gameName>/source/*ModuleName*/<ignore>
					string[] initialSeparator = { "ue4" };
					string[] parts = fullPath.Split(initialSeparator, StringSplitOptions.RemoveEmptyEntries);
					string[] pathSeparator = { "\\" };
					string[] moduleParts = parts[1].Split(pathSeparator, StringSplitOptions.RemoveEmptyEntries);
					if(moduleParts.Length > 2)
					{
						moduleName = moduleParts[2];
					}
					else
					{
						// Saw an instance where the path was separated by \\ but the above separator wasn't working. Added this check to check if the path was holding the separator as literally \\ instead of \. 
						string[] pathSeparator1 = { @"\\" };
						moduleParts = parts[1].Split(pathSeparator1, StringSplitOptions.RemoveEmptyEntries);
						if (moduleParts.Length > 2)
						{
							moduleName = moduleParts[2];
						}
					}
				}
			}
			return moduleName;
		}
	}

	public class CallStackContainer
	{
		private static readonly Regex mRegex = new Regex(@"([^(]*[(][^)]*[)])([^\[]*([\[][^\]]*[\]]))*", RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled);

		private string mErrorMsg;
		private string mUnformattedCallStack;
		private IList<CallStackEntry> mEntries = new List<CallStackEntry>();
		private String[] Lines;

		public bool DisplayUnformattedCallStack;
		public bool DisplayFunctionNames;
		public bool DisplayFileNames;
		public bool DisplayFilePathNames;

		public string ErrorMessage
		{
			get { return mErrorMsg; }
		}

		public CallStackContainer(
			string CallStack,
			int FunctionParseCount,
			bool InDisplayFunctions,
			bool InDisplayFileNames)
		{
			ParseCallStack(CallStack, FunctionParseCount, InDisplayFunctions, InDisplayFileNames);
		}

		public IList<CallStackEntry> GetEntries()
		{
			return mEntries;
		}
		public CallStackEntry GetEntry(int position)
		{
			return mEntries.ElementAt(position);
		}

		public String[] GetLines()
		{
			return Lines;
		}


		public string GetModuleName()
		{
			// List of functions that should be skipped 
			// The general trend seems to be that the call that caused the crash is sometimes followed by calls to close things down, issue an ensure etc.
			// This is a list of functions I've found to be among those trailing. 
			List<string> FunctionsToSkip = new List<string>()
											{
												"appFailAssert()"
												,"appFailEnsure()"
												,"FOutputDeviceWindowsError::Serialize()"
												,"Stack: FWindowsPlatformStackWalk::StackWalkAndDump()"
												,"appLogFunc__VA()"
												,"FDebug::AssertFailed()"
												,"Stack: FDebug::AssertFailed()"
												,"FOutputDeviceWindowsError::Serialize()"
												,"FOutputDevice::Logf__VA()"
												,"FModuleManager::GetModuleInterface()"
												,"FDebug::EnsureFailed()"
												,"FDebug::EnsureNotFalse()"
												,"FMsg::Logf__VA()"
												,"FArchive::SerializeCompressed()"
												,"FModuleManager::GetModuleInterface()"
											};

			var ModuleName = String.Empty;

			foreach (CallStackEntry e in GetEntries())
			{
				// Copying variable to avoid modified closure situation
				CallStackEntry entry = e;

				// Walk through function calls in the callstack until you find the appropriate one to get the module out of.
				// If Callstack has a trailing function that doesn't contain the module then don't return a module name.
				if (entry != null)
				{
					
					if( FunctionsToSkip.Any(f=>entry.FunctionName.Contains(f)) )
					{
						ModuleName = String.Empty;
					}
					else if (String.IsNullOrEmpty(ModuleName) && !String.IsNullOrEmpty(entry.FilePath))
					{
						// If load module checked is found the get the module name out of the function instead of the path
						if (entry.FunctionName.Contains("FModuleManager::LoadModuleChecked"))
						{
							string[] moduleSeparator = { "<", ">" };
							string[] moduleParts = entry.FunctionName.Split(moduleSeparator, StringSplitOptions.RemoveEmptyEntries);
							ModuleName = moduleParts[1];
						}
						else
						{
							ModuleName = e.GetModuleName();
						}
						
					}
				}
			}
			return ModuleName;
		}
	
		private void ParseCallStack(
			string CallStack,
			int FunctionParseCount,
			bool InDisplayFunctions,
			bool InDisplayFileNames)
		{
			DisplayUnformattedCallStack = false;
			DisplayFunctionNames = InDisplayFunctions;
			DisplayFileNames = InDisplayFileNames;
			DisplayFilePathNames = false;

			if (CallStack == null)
			{
				//do nothing
			}
			else
			{

				mUnformattedCallStack = CallStack;

				Lines = CallStack.Split(new char[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);

				StringBuilder ErrorBldr = new StringBuilder();

				foreach (string CurLine in Lines)
				{
					if (mEntries.Count >= FunctionParseCount)
					{
						break;
					}

					Match CurMatch = mRegex.Match(CurLine);

					if (CurMatch.Success && CurMatch.Value != "")
					{
						string FuncName = CurMatch.Groups[1].Value;
						string FileName = "";
						string FilePath = "";
						int? LineNumber = null;

						Group ExtraInfo = CurMatch.Groups[CurMatch.Groups.Count - 1];

						if (ExtraInfo.Success)
						{
							const string FILE_START = "[File=";
							//const string PATH_START = "[in ";

							foreach (Capture CurCapture in ExtraInfo.Captures)
							{
								if (CurCapture.Value.StartsWith(FILE_START, StringComparison.OrdinalIgnoreCase))
								{
									// -1 cuts off closing ]
									string Segment = CurCapture.Value.Substring(FILE_START.Length, CurCapture.Length - FILE_START.Length - 1);
									int LineNumSeparator = Segment.LastIndexOf(':');
									string LineNumStr = "";

									if (LineNumSeparator != -1)
									{
										LineNumStr = Segment.Substring(LineNumSeparator + 1);
										Segment = Segment.Substring(0, LineNumSeparator);
									}

									// if the path is corrupt we don't want the exception to fall through
									try
									{
										FileName = Path.GetFileName(Segment);
									}
									catch (Exception)
									{
										// if it fails just assign the file name to the segment
										FileName = Segment;
									}

									// if the path is corrupt we don't want the exception to fall through
									try
									{
										FilePath = Path.GetDirectoryName(Segment);
									}
									catch (Exception)
									{
									}

									int TempLineNum;
									if (int.TryParse(LineNumStr, out TempLineNum))
									{
										LineNumber = TempLineNum;
									}
								}
							}
						}
						mEntries.Add(new CallStackEntry(FileName, FilePath, FuncName, LineNumber));
					}
					else
					{
						if (CurLine != "" && CurLine != " ")
						{
							ErrorBldr.Append(CurLine);
							ErrorBldr.Append('\n');
						}
					}
				}
				mErrorMsg = ErrorBldr.ToString();
			}
		}

		public string GetFormattedCallStack()
		{
			if (DisplayUnformattedCallStack)
			{
				return mUnformattedCallStack;
			}

			StringBuilder formattedCallStack = new StringBuilder(mErrorMsg);
			formattedCallStack.Append("<br>");

			foreach (CallStackEntry currentDesc in mEntries)
			{
				if (DisplayFunctionNames)
				{
					formattedCallStack.AppendFormat("<b><font color=\"#151B8D\">{0}</font></b>", currentDesc.FunctionName);
				}

				if (DisplayFileNames && DisplayFilePathNames && currentDesc.FileName.Length > 0 && currentDesc.FilePath.Length > 0)
				{
					formattedCallStack.Append(" --- ");

					if (currentDesc.LineNumber.HasValue)
					{
						formattedCallStack.AppendFormat("<b>{0} line={1}</b>", Path.Combine(currentDesc.FilePath, currentDesc.FileName), currentDesc.LineNumber.Value.ToString());
					}
					else
					{
						formattedCallStack.AppendFormat("<b>{0}</b>", Path.Combine(currentDesc.FilePath, currentDesc.FileName));
					}
				}
				else if (DisplayFilePathNames && currentDesc.FilePath.Length > 0)
				{
					formattedCallStack.Append(" --- ");
					formattedCallStack.AppendFormat("<b>{0}</b>", currentDesc.FilePath);
				}
				else if (DisplayFileNames && currentDesc.FileName.Length > 0)
				{
					formattedCallStack.Append(" --- ");

					if (currentDesc.LineNumber.HasValue)
					{
						formattedCallStack.AppendFormat("<b>{0} line={1}</b>", currentDesc.FileName, currentDesc.LineNumber.Value.ToString());
					}
					else
					{
						formattedCallStack.AppendFormat("<b>{0}</b>", currentDesc.FileName);
					}
				}
				formattedCallStack.Append("<br>");
			}
			return formattedCallStack.ToString();
		}
	}
}