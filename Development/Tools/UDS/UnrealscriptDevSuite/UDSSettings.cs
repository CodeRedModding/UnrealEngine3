using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

namespace UnrealscriptDevSuite
{
	public class UDSSettings
	{
		private const string MASTER_REG_KEY = @"SOFTWARE\Epic Games\UDS\Settings";

		public bool bBrowserVisible 
		{ 
			get
			{
				string Value = ReadRegistry("bBrowserVisible");
				return !( Value == null || Value.ToLower().Trim() == "false" );
			}
			set
			{
				WriteRegistry("bBrowserVisible", value.ToString());
			}
		}

		public bool bForceCompileEXE
		{
			get
			{
				string Value = ReadRegistry("bForceCompileExe");
				return !( Value == null || Value.ToLower().Trim() == "false" );
			}
			set
			{
				WriteRegistry("bForceCompileExe", value.ToString());
			}
		}

		public string CompileExeFilename
		{
			get
			{
				return ReadRegistry("CompileExeName");
			}
			set
			{
				WriteRegistry("CompileExeName", value);
			}
		}

		public bool bAppendDebug
		{
			get
			{
				string Value = ReadRegistry("bAppendDebug");
				return !( Value == null || Value.ToLower().Trim() == "false" );
			}
			set
			{
				WriteRegistry("bAppendDebug", value.ToString());
			}
		}

		public string AdditionalCompileCmdLine
		{
			get
			{
				return ReadRegistry("AdditionalCompileCmdLine");
			}
			set
			{
				WriteRegistry("AdditionalCompileCmdLine", value);
			}
		}

		public bool bUseCOMFile
		{
			get
			{
				string Value = ReadRegistry("bUseCOMFile");
				return !( Value == null || Value.ToLower().Trim() == "false" );
			}
			set
			{
				WriteRegistry("bUseCOMFile", value.ToString());
			}
		}

		public string NewClassHeader
		{
			get
			{
				return ReadRegistry("NewClassHeader");
			}
			set
			{
				WriteRegistry("NewClassHeader", value);
			}
		}

		public bool bClearOutputWindow
		{
			get
			{
				string Value = ReadRegistry("bClearOutputWindow");
				return !( Value == null || Value.ToLower().Trim() == "false" );
			}
			set
			{
				WriteRegistry("bClearOutputWindow", value.ToString());
			}
		}

		public int InitCount
		{
			get
			{
				string Value = ReadRegistry("InitCount");
				try
				{
					return Int32.Parse(Value);
				}
				catch
				{
					return 0;
				}
			}
			set
			{
				WriteRegistry("InitCount",value.ToString());
			}

		}

		public UDSSettings()
		{
		}

		private string ReadRegistry(string Key)
		{
			RegistryKey Reg = Registry.CurrentUser.CreateSubKey(MASTER_REG_KEY);
			if (Reg != null)
			{
				return (string)Reg.GetValue(Key,"");
			}

			return null;
		}

		private void WriteRegistry(string Key, string Value)
		{
			try
			{
				RegistryKey Reg = Registry.CurrentUser.CreateSubKey(MASTER_REG_KEY);
				if ( Reg != null )
				{
					Reg.SetValue(Key, Value);
				}
			}
			catch (Exception e)
			{
				MessageBox.Show(e.ToString());
			}
		}
	}



}
