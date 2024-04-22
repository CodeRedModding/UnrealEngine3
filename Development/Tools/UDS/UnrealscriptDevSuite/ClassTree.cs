using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace UnrealscriptDevSuite
{
	class ClassTree : TreeView
	{
		/**
		 * We override the DefWndProc and swallow the double click so that nodes work properly.
		 */
		protected override void DefWndProc( ref Message m )
		{
			if (m.Msg == 0x203)	// Swallow the double click
			{
				return;
			}
			base.DefWndProc(ref m);
		}
	}
}
