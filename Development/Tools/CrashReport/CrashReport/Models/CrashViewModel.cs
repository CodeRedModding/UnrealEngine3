// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace CrashReport.Models
{
	public class CrashViewModel
	{
		public Crash Crash { get; set; }
		public CallStackContainer CallStack { get; set; }
	}
}