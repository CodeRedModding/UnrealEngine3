
using System;
using System.Windows.Forms;
using System.Drawing;

namespace ueScriptProfilerControls
{
	public class UEStatusBar : StatusBar
	{
		public ProgressBar LoadingProgressBar;

		public UEStatusBar()
		{
			// Create a progress bar and hide it.  We will make this visible whenever
			// the status bar goes into the "progress" state.

			LoadingProgressBar = new ProgressBar();
			LoadingProgressBar.Step = 1;
			LoadingProgressBar.Visible = false;

			Controls.Add( LoadingProgressBar );
		}

		/**
		 * Toggles the "progress" state of the status bar.
		 * 
		 * @param	InShow	If true, sets the state.
		 */

		public void ShowProgress( bool InShow )
		{
			ShowPanels = InShow;

			// If we are entering the state, position the progress bar control so that
			// it completely covers the second panel.

			if( InShow )
			{
				StatusBarPanel panel0 = Panels[0];
				StatusBarPanel panel1 = Panels[1];

				// I don't see how to query for the exact location of a status bar panel, so I had
				// to set the left edge of the progress bar equal to the width of the first panel.
				// Hacky, but it works well enough for now.

				LoadingProgressBar.Location = new Point( panel0.Width, LoadingProgressBar.Location.Y );
				LoadingProgressBar.Width = panel1.Width;
			}

			LoadingProgressBar.Visible = InShow;
		}
	}
}
