using System;
using System.Windows.Forms;

namespace AIProfiler
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main(String[] Args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			String FileName = String.Empty;
			if (Args.Length > 0)
			{
				FileName = Args[0];
			}
			Application.Run(new MainWindow(FileName));
		}
	}
}
