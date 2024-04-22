// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;

namespace CrashReport.Models
{
	public class BuggsViewModel
	{
		public IQueryable<Bugg> Buggs { get; set; }
		public string Query { get; set; }
		public IEnumerable<BuggsResult> Results { get; set; }
		public PagingInfo PagingInfo { get; set; }
		public string Term { get; set; }
		public string Order { get; set; }
		public FormCollection FormCollection { get; set; }
		public IEnumerable<string> SetStatus { get { return new List<String>(new string[] { "Unset", "Reviewed", "New", "Coder", "Tester" }); } }
		public string UserGroup { get; set; }
		public string DateFrom { get; set; }
		public string DateTo { get; set; }
		public string GameName { get; set; }
		public IList<CallStackContainer> CallStacks { get; set; }
		public IDictionary<string, int> GroupCounts { get; set; }
	}
}