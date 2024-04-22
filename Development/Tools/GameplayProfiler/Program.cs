using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace GameplayProfiler
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main(string[] Args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			string FileName = null;
			if( Args.Length > 0)
			{
				FileName = Args[0];
			}
			Application.Run(new MainWindow(FileName));
		}
	}
}
