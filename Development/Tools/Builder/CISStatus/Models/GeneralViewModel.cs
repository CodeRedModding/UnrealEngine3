// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace CISStatus.Models
{
	public class GeneralViewModel
	{
		public string Title
		{
			get;
			set;
		}
	
		public bool bIsFirst
		{
			get;
			set;
		}

		public CISStatus.Controllers.BranchInfo CurrentBranchInfo
		{
			get;
			set;
		}

		public List<CISStatus.Controllers.BranchInfo> Branches
		{
			get;
			set;
		}

		public List<CISStatus.Controllers.ChangelistInfo> Changelists
		{
			get;
			set;
		}

		public List<CISStatus.Controllers.VerificationInfo> Verifications
		{
			get;
			set;
		}

		public List<CISStatus.Controllers.BuildInfo> BuildStatuses
		{
			get;
			set;
		}

		public GeneralViewModel()
		{
			bIsFirst = true;
		}
	}
}

