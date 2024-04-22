using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace UnrealscriptDevSuite
{
	public partial class SearchDialog : Form
	{
		public string SearchFor { get { return tbSearchFor.Text; } }
		public int SearchDirection { get { return cbDirection.SelectedIndex; } }
		public bool bMatchCase { get { return rbMatchCase.Checked; } }
		public bool bWholeWords { get { return rbWholeWords.Checked; } }
		public bool bUseFind2 { get { return ckFind2.Checked; } }

		public SearchDialog()
		{
			InitializeComponent();
			cbDirection.SelectedIndex = 0;
		}
	}
}
