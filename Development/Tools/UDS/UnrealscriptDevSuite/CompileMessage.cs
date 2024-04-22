using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnrealscriptDevSuite
{
	class CompileMessage
	{
		public string Description;
		public string Filename;
		public int LineNo;
		public bool bIsWarning;

		public CompileMessage(string NewDescription, string NewFilename, int NewLineNo, bool bNewIsWarning)
		{
			Description = NewDescription;
			Filename = NewFilename;
			LineNo = NewLineNo;
			bIsWarning = bNewIsWarning;
		}


	}
}
