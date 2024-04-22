// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace CrashReport.Models
{
	public class BuggViewModel
	{
		public Bugg Bugg { get; set; }
		public IEnumerable<Crash> Crashes { get; set; }
		public CallStackContainer CallStack { get; set; }
		public String Pattern { get; set; }
		public int CrashId { get; set; }
	}
}