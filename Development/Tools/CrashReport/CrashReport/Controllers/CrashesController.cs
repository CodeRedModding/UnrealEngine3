// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Web.Mvc;
using CrashReport.Models;

namespace CrashReport.Controllers
{
	[HandleError]
	public class CrashesController : Controller
	{
		private ICrashRepository mCrashRepository;

		public CrashesController() : this( new CrashRepository() )
		{
		}

		public CrashesController(ICrashRepository repository)
		{
			this.mCrashRepository = repository;
		}
			
#if DEBUG
		//do nothing
#else
		[OutputCache (Duration=120, VaryByParam="*")]
#endif
		public ActionResult Index(FormCollection form)
		{
			// Set up Default values if there is no QueryString and set values to the Query string if it is there.
			String SearchQuery = Request.QueryString["SearchQuery"];

			int Page = 1;
			bool parsePage= int.TryParse(Request.QueryString["Page"], out Page); 
			if (parsePage && Page > 0)
			{
				// do nothing
			}
			else
			{
				Page = 1;
			}

			int PageSize = 100;
			bool parsePageSize = int.TryParse(Request.QueryString["PageSize"], out PageSize);
			if (parsePageSize && PageSize > 0)
			{
				// do nothing
			}
			else
			{
				PageSize = 100;
			}

			string SortTerm = "TimeOfCrash";
			if (!String.IsNullOrEmpty(Request.QueryString["SortTerm"] ))
			{
				SortTerm = Request.QueryString["SortTerm"];
			}
			
			string SortOrder = "Descending";
			if (!String.IsNullOrEmpty(Request.QueryString["SortOrder"]) )
			{
				SortOrder = Request.QueryString["SortOrder"];
			}

			string PreviousOrder = Request.QueryString["PreviousOrder"];
			string PreviousTerm = Request.QueryString["PreviousTerm"];
			
			string UserGroup = "Epic";
			if (!String.IsNullOrEmpty(Request.QueryString["UserGroup"] ))
			{
				UserGroup = Request.QueryString["UserGroup"];
			}

			string GameName = Request.QueryString["GameName"];

			bool OneQuery = false;
			if (Request.QueryString["OneQuery"] == "true")
			{
				OneQuery = true;
			}

			string DateFrom = string.Empty;
			if (!String.IsNullOrEmpty(Request.QueryString["DateFrom"]))
			{
				DateFrom = Request.QueryString["DateFrom"];
			}

			string DateTo = string.Empty;
			if (!String.IsNullOrEmpty(Request.QueryString["DateTo"]))
			{
				DateTo = Request.QueryString["DateTo"];
			}
			
			//Set Default CrashType filter
			string CrashType = "CrashesAsserts";
			if (!String.IsNullOrEmpty(Request.QueryString["CrashType"]))
			{
				CrashType = Request.QueryString["CrashType"];
			}

			// If nothing was passed in the query and something was passed in the form use the form data.
			if (!String.IsNullOrEmpty(form["DateFrom"] ) && String.IsNullOrEmpty(DateFrom))
			{
				//assuming that is DateFrom is set in the form then we should use whatever value (if any) is in the DateTo field of the form
				DateFrom = form["DateFrom"].Trim(',');
				DateTo = form["DateTo"].Trim(',');
			}

			// Handle any edits made in the Set form fields
			foreach (var entry in form)
			{
				int id = 0;
				bool canConvert = int.TryParse(entry.ToString(), out id);
				if (canConvert == true)
				{
					Crash c = this.mCrashRepository.Get(id);
					if(!String.IsNullOrEmpty(form["SetStatus"])) c.Status = form["SetStatus"];
					if (!String.IsNullOrEmpty(form["SetFixedIn"])) c.FixedChangeList = form["SetFixedIn"];
					if (!String.IsNullOrEmpty(form["SetTTP"])) c.TTPID = form["SetTTP"];  
				}
				this.mCrashRepository.SubmitChanges(); 
			}

			//Set the sort order 
			if (PreviousOrder == "Descending" && PreviousTerm == SortTerm)
			{
				SortOrder = "Ascending";
			}
			else if (PreviousOrder == "Ascending" && PreviousTerm == SortTerm)
			{
				SortOrder = "Descending";
			}
			else if (String.IsNullOrEmpty(PreviousOrder))
			{
				//keep SortOrder Where it's at.
			}
			else
			{
				SortOrder = "Descending";
			}
			
			// Use the GetResults() function in the repository. 
			CrashesViewModel Result = this.mCrashRepository.GetResults(SearchQuery, Page  , PageSize, SortTerm , SortOrder, PreviousOrder, UserGroup, DateFrom, DateTo, GameName, OneQuery, CrashType);

			//Add the FromCollection to the CrashesViewModel since we don't need it for the get results function but we do want to post it back to the page.
			Result.FormCollection = form;

			var ViewModel = Result;
			if (Request.IsAjaxRequest())
			{
				return View("Index", ViewModel);
			}
			else
			{
				return View("Index", ViewModel);
			}
		}

		public ActionResult Show(FormCollection form, int id)
		{
			int CrashId;
			bool parseCrashId = int.TryParse(Request.QueryString["CrashId"], out CrashId);
			if (parseCrashId)
			{
				// do nothing
			}
			else 
			{
				CrashId = 0;
			}

			// Handle old urls that went to /crashes/show?CrashId=<id>
			if (id == 0)
			{
				return RedirectToAction("Show", new { id = CrashId });
			}

			var crash = this.mCrashRepository.Get(id);

			foreach (var entry in form)
			{
				if (!String.IsNullOrEmpty(form["SetStatus"])) crash.Status = form["SetStatus"];
				if (!String.IsNullOrEmpty(form["SetFixedIn"])) crash.FixedChangeList = form["SetFixedIn"];
				if (!String.IsNullOrEmpty( form["SetTTP"]) ) crash.TTPID = form["SetTTP"];
				if (!String.IsNullOrEmpty(form["Description"]))
				{
					crash.Description = form["Description"];
				}else{
					if(!String.IsNullOrEmpty(crash.Description) && String.IsNullOrEmpty(form["Description"]) )
					{
						crash.Description = string.Empty;
					}
				}
				this.mCrashRepository.SubmitChanges();
			}
		
			var callStack = new CallStackContainer(crash.RawCallStack, 100, true, true);
			// Set callstack properties
			callStack.DisplayFunctionNames = true;
			callStack.DisplayFileNames = true;
			callStack.DisplayFilePathNames = true;
			callStack.DisplayUnformattedCallStack = false;

			crash.CallStackContainer = crash.GetCallStack();

			return View("Show", new CrashViewModel
			{
				Crash = crash,
				CallStack = callStack
			});
		}

#if DEBUG
		// Do nothing
#else
		[OutputCache(Duration = 600, VaryByParam = "*")]
#endif
		public ActionResult ShowOld(int id)
		{
			var crash = new Crash();
			crash = this.mCrashRepository.GetByAutoReporterId(id);
			if (crash == null)
			{
				return Redirect("http://AutoReportService/SingleReportView.aspx?rowid=" + id);
			}
			else
			{
				return RedirectToAction("Show", new { id = crash.Id });
			}
		}

		public ActionResult DashBoard()
		{
			return View("DashBoard");
		}
	}
}
