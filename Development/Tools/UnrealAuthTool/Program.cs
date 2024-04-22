using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace UnrealAuthTool
{
	static class Program
	{
		// show a usage dialog
		static void ShowUsage()
		{
			System.Windows.Forms.MessageBox.Show("Usage: UnrealAuthTool -appid <YourAppId> [-perms <CommaSeparatedList>]");
		}

		// parse a token from the command line, and return it
		static string ParseToken(ref string CommandLine)
		{
			// go to next token
			int Offset = CommandLine.IndexOf(' ');
			CommandLine = CommandLine.Substring(Offset);
			
			// skip over any other spaces
			while (CommandLine.StartsWith(" "))
			{
				CommandLine = CommandLine.Substring(1);
			}

			char EndChar = ' ';
			// if its quoted, the end is a quote
			if (CommandLine.StartsWith("\""))
			{
				EndChar = '\"';
				CommandLine = CommandLine.Substring(1);
			}

			// look for end of token
			Offset = CommandLine.IndexOf(EndChar);
			string Token;
			if (Offset == -1)
			{
				// if it's not found, then we use rest of command line
				Token = CommandLine;
				CommandLine = "";
			}
			else
			{
				// otherwise get just the token
				Token = CommandLine.Substring(0, Offset);
				CommandLine = CommandLine.Substring(Offset + 1);
			}

			while (CommandLine.StartsWith(" "))
			{
				CommandLine = CommandLine.Substring(1);
			}

			return Token;
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			string CommandLine = Environment.CommandLine;
			
			// skip over the exe name
			int Offset = CommandLine.IndexOf(' ');
			if (Offset == -1)
			{
				ShowUsage();
				return;
			}

			// strip it off
			CommandLine = CommandLine.Substring(Offset + 1);

			string AppId = null;
            //string OutputFile = null;
			string Permissions = null;
			while (CommandLine != "")
			{
				if (CommandLine.StartsWith("-appid "))
				{
					AppId = ParseToken(ref CommandLine);
				}
				else if (CommandLine.StartsWith("-perms "))
				{
					Permissions = ParseToken(ref CommandLine);
				}
				else
				{
					ParseToken(ref CommandLine);
				}
			}

			if (AppId == null)
			{
				ShowUsage();
				return;
			}

			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			Application.Run(new MainView(AppId, Permissions));
		}
	}
}
