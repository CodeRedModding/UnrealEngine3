// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace CrashReport.Models
{
	using System;
	using System.Collections.Generic;
	using System.Data.Linq.SqlClient;
	using System.Diagnostics;
	using System.Linq;
	using System.Web;
	using Naspinski.IQueryableSearch;
	using System.Data.Linq;

	public class BuggRepository : IBuggRepository
	{
		private const string DefaultUserGroup = "Undefined";
		private const int DefaultUserGroupId = 5;

		private readonly CrashReportDataContext _mCrashReportDataContext;

		public BuggRepository()
		{
				this._mCrashReportDataContext = new CrashReportDataContext();
		}

		#region IBuggRepository Members
		public CrashReportDataContext GetDataContext()
		{
			return this._mCrashReportDataContext;
		}

		public void Insert(Bugg bugg)
		{
			this._mCrashReportDataContext.Buggs.InsertOnSubmit(bugg);
		}

		public void Insert(Crash crash)
		{
			this._mCrashReportDataContext.Crashes.InsertOnSubmit(crash);
		}

		public void AddCrashes(Bugg bugg, IQueryable<Crash> Crashes)
		{

			//TODO When do we use this? I think this can get removed/deprecated
			//I think where I was using this is now used by AddCrashToBugg()

			DateTime? TimeOfFirstCrash = null;
			DateTime? TimeOfLastCrash = null;
			List<string> users = new List<string>();
			int CrashCount = 0;
			// TODO set or return min date max date while we're iterating through the crashes
			foreach (Crash crash in Crashes)
			{
				CrashCount++;
				if (TimeOfFirstCrash == null || crash.TimeOfCrash < TimeOfFirstCrash)
				{ 
					TimeOfFirstCrash = crash.TimeOfCrash; 
				}

				if (TimeOfLastCrash == null || crash.TimeOfCrash > TimeOfLastCrash)
				{
					TimeOfLastCrash = crash.TimeOfCrash; 
				}

				// Handel user count
				if (users.Contains(crash.UserName.ToString()))
				{
				}
				else
				{
					users.Add(crash.UserName);
				}
				this.LinkCrashToBugg(bugg, crash);
			}

			if (CrashCount > 0)
			{
				bugg.TimeOfLastCrash = TimeOfLastCrash;
				bugg.TimeOfFirstCrash = TimeOfFirstCrash;
				bugg.NumberOfUsers = users.Count();
				bugg.NumberOfCrashes = Crashes.Count();
			}
		}

		// TODO add the User relationship to Buggs in CrashReport.cs  and replace this code with that
		//Not terribly important as it would run pretty much the same code as we have here.
		public User AddUser(string UserName)
		{
			return this.AddUser(UserName, DefaultUserGroup, DefaultUserGroupId);
		}

		public User AddUser(string UserName, string UserGroup)
		{
			UserGroup ug = new UserGroup();
			int id;
			try
			{
				ug = this._mCrashReportDataContext.UserGroups.Where(g => g.Name == UserGroup).First();
				id = ug.Id;
			}
			catch (InvalidOperationException)
			{
				id = DefaultUserGroupId;
			}
			
			return this.AddUser(UserName, UserGroup, id);
		}

		public User AddUser(string UserName, string UserGroup, int UserGroupId)
		{
			User User = new User();
			try
			{
				User = this._mCrashReportDataContext.Users.Where(u => u.UserName == UserName).Single<User>();
			}
			catch (InvalidOperationException)
			{
				User.UserName = UserName;
				User.UserGroup = UserGroup;
				User.UserGroupId = UserGroupId;
				this._mCrashReportDataContext.Users.InsertOnSubmit(User);
			}

			return User;
		}

		public void BuildPattern(Crash crash)
		{
			CrashRepository CrashRepository = new CrashRepository(this._mCrashReportDataContext);
			CrashRepository.BuildPattern(crash);

		}

		public void SubmitChanges()
		{
			// Turn on the logging.
			this._mCrashReportDataContext.Log = Console.Out;
			try
			{
				this._mCrashReportDataContext.SubmitChanges(ConflictMode.ContinueOnConflict);
			}
			catch (ChangeConflictException)
			{
				this._mCrashReportDataContext.ChangeConflicts.ResolveAll(RefreshMode.KeepChanges);
			}
			catch (Exception e)
			{
				//TODO Catch The Proper exceptions and do something with them
				//for now do nothing, just eat the exception
				try{
					var changeSet = this._mCrashReportDataContext.GetChangeSet();
					Console.Write(this._mCrashReportDataContext.ChangeConflicts);
					Console.Write(this._mCrashReportDataContext.GetChangeSet().ToString());
				}
				catch(NotSupportedException notSupportedException)
				{
					Console.WriteLine(notSupportedException.Message);
				}
			}
		}

		public Bugg Get(int id)
		{
			IQueryable<Bugg> bugg =
			(
				from b in this._mCrashReportDataContext.Buggs
				where b.Id == id
				select b
			);
			return (Bugg)bugg.FirstOrDefault();
		}

		public Bugg GetByPattern(string Pattern)
		{
			IQueryable<Bugg> bugg =
			(
				from b in this._mCrashReportDataContext.Buggs
				where b.Pattern == Pattern
				select b

			);
			return (Bugg)bugg.FirstOrDefault();
		}

		public string GetFunctionCall(int Id)
		{
			// TODO would be cleaner to just have one return and we may want to return something else besides an empty string
			// But if there is no function call then the empty string does make sense.
			try
			{
				var FunctionCall = this._mCrashReportDataContext.FunctionCalls.Where(fc => fc.Id == Id).First();
				return FunctionCall.Call;
			}
			catch (InvalidOperationException)
			{
				return string.Empty;
			}
		}

		public int GetFunctionCallId(string Call)
		{
			// TODO would be cleaner to just have one return and we may want to return something else besides an empty string
			// But if there is no function call then the empty string does make sense.
			try
			{
				var FunctionCall = this._mCrashReportDataContext.FunctionCalls.Where(fc => fc.Call.Contains(Call)).First();
				return FunctionCall.Id;
			}
			catch (InvalidOperationException)
			{
				return 0;
			}
		}
		
		public IList<string> GetFunctionCalls(string Pattern)
		{
			var CachedResults = new CachedDataService(HttpContext.Current.Cache, this);

			IList<string> FunctionCalls = CachedResults.GetFunctionCalls(Pattern);

			return FunctionCalls;
		}

		// GetFunctionCalls
		// Retrieves Functions Call Strings from the database.
		// Primarily used to fill GetFunctionCalls in CachedDataService
		public IList<string> GetFunctionCalls(IList<int> Ids)
		{
			var fcs =
				from i in Ids
				join f in this._mCrashReportDataContext.FunctionCalls on i equals f.Id
				select f.Call;

			return fcs.ToList(); 
		}

		public IQueryable<Crash> GetCrashesByPattern(string pattern)
		{
			// Don't need the % at the beginning or end if you're bulding it that way.
			// string p = "%"+pattern+"%";

			IQueryable<Crash> crashes =
			(
				from c in this._mCrashReportDataContext.Crashes
				where SqlMethods.Like(c.Pattern, pattern)
				select c
			).OrderByDescending(c => c.TimeOfCrash)
			;
			return crashes;
		}

		public void CreateBuggFromPattern(string Pattern)
		{
			//if (Pattern.Length > 200)
			//{
			//    Pattern = Pattern.Substring(0, 200);
			//    Pattern = Pattern + "%";
			//}
			
			//// Grab all the crashes in the system to match this pattern. These will be the crashes that belong to the bugg.
			//// This result set will include the Crash we passed in so we don't need to treat it specially or anything.
			//var Crashes = this.GetCrashesByPattern(Pattern);

			//if (Crashes.Count() > 1)
			//{
			//    // We have enough crashes to make a bugg. (i.e. more than 1)
			//    var Bugg = new Bugg();
			//    Bugg.Pattern = Pattern;

			//    // Save the bugg we created to get the ID
			//    this._mCrashReportDataContext.Buggs.InsertOnSubmit(Bugg);
			//    this.SubmitChanges();

			//    // Run through all crashes and make sure they're added to the Bugg
			//    this.UpdateBuggData(Bugg, Crashes.AsEnumerable());
			//}
		}

		public void UpdateBuggData(Bugg Bugg, EntitySet<Crash> Crashes)
		{
			
			UpdateBuggData(Bugg, Bugg.Crashes.AsEnumerable());
		}

		public void UpdateBuggData(Bugg Bugg, IEnumerable<Crash> Crashes)
		{
			
			DateTime? TimeOfFirstCrash = null;
			DateTime? TimeOfLastCrash = null;
			List<string> Users = new List<string>();
			List<int?> UserGoupIds = new List<int?>();
			int CrashCount = 0;
			//Set or return min date max date while we're iterating through the crashes
			foreach (Crash crash in Crashes)
			{
				CrashCount++;
				if (TimeOfFirstCrash == null || crash.TimeOfCrash < TimeOfFirstCrash)
				{
					TimeOfFirstCrash = crash.TimeOfCrash;
				}

				if (TimeOfLastCrash == null || crash.TimeOfCrash > TimeOfLastCrash)
				{
					TimeOfLastCrash = crash.TimeOfCrash;
				}

				// Handle user count
				if (Users.Contains(crash.UserName.ToLower()) || Bugg.Id == 0)
				{
					// Don't do anything this user has already been accounted for.
				}
				else
				{
					//Add username to local users variable that we will use later to set user count
					//and associate users and usergroups to the bugg.
					Users.Add(crash.UserName.ToLower());

					//Fill up variable with the users groups while we've got the crash data queue'd up
					if (UserGoupIds.Contains(crash.User.UserGroupId) || Bugg.Id == 0)
					{
						// UserGroup already accounted for
					}
					else
					{
						// Add usergrop to local usergroupids variable so we don't have to do more queries later on.
						UserGoupIds.Add(crash.User.UserGroupId);
					}
				}

				crash.FixedChangeList = Bugg.FixedChangeList;
				crash.TTPID = Bugg.TTPID;
				crash.Status = Bugg.Status;

				if (Bugg.Id != 0)
				{
					this.LinkCrashToBugg(Bugg, crash);
				}
			}

			if (CrashCount > 1)
			{
				Bugg.TimeOfLastCrash = TimeOfLastCrash;
				Bugg.TimeOfFirstCrash = TimeOfFirstCrash;
				Bugg.NumberOfUsers = Users.Count();
				Bugg.NumberOfCrashes = CrashCount;
			 
				foreach (string username in Users)
				{
					//Link User to Bugg
					this.LinkUserToBugg(Bugg, username.ToLower());
				}

				foreach (int? usergroupId in UserGoupIds)
				{
					//Link UserGroup to Bugg
					this.LinkUserGroupToBugg(Bugg, usergroupId);
				}

				this.SubmitChanges();
			}
		}

		public void AddCrashToBugg(Crash Crash)
		{
///////////////////////////////////////////
		}

		public void LinkCrashToBugg(Bugg bugg, Crash Crash)
		{
			// TODO Should be able to replace this code with bugg.Add(Crash)
			// Make sure we don't already have this relationship
			if (this._mCrashReportDataContext.Bugg_Crashes.Where(c => c.Crash == Crash && c.Bugg == bugg).Count() < 1)
			{
				// We don't so create the relationship
				var bc = new Bugg_Crash();
				bc.Crash = Crash;
				bc.Bugg = bugg;
				this._mCrashReportDataContext.Bugg_Crashes.InsertOnSubmit(bc);
				this.SubmitChanges();
			}
		}

		// TODO Decide whether these should return ids /0 or not
		public void LinkUserToBugg(Bugg Bugg, Crash Crash)
		{
			this.LinkUserToBugg(Bugg, Crash.UserName);
		}

		public void LinkUserToBugg(Bugg Bugg, string UserName)
		{
			UserName = UserName.ToLower();
			var BuggUserCount = Bugg.Bugg_Users.Where(b => b.Bugg == Bugg && b.UserName == UserName).Count();
			if (BuggUserCount < 1)
			{
				var bu = new Bugg_User();
				bu.Bugg = Bugg;
				bu.UserName = UserName;
				this._mCrashReportDataContext.Bugg_Users.InsertOnSubmit(bu);
				this.SubmitChanges();
			}
		}

		public void LinkUserGroupToBugg(Bugg Bugg, Crash Crash)
		{
			this.LinkUserGroupToBugg(Bugg, Crash.User.UserGroupId);
		}

		public void LinkUserGroupToBugg(Bugg Bugg, int? UserGroupId)
		{
			if (UserGroupId == null)
			{
				// do nothing
			}
			else
			{
				var BuggUserGroupCount = Bugg.Bugg_UserGroups.Where(b => b.Bugg == Bugg && b.UserGroupId == UserGroupId).Count();
				if (BuggUserGroupCount < 1)
				{
					var bUg = new Bugg_UserGroup();
					bUg.Bugg = Bugg;
					bUg.UserGroupId = (int)UserGroupId;
					this._mCrashReportDataContext.Bugg_UserGroups.InsertOnSubmit(bUg);
					// Question Do I have to submit changes each time here?
					// Does the bugg have to be saved pevious to this?
					this.SubmitChanges();
				}
			}
		}

		public IQueryable<Bugg> ListAll()
		{
			var buggs =
				(
					from b in this._mCrashReportDataContext.Buggs
					select b
				);
			return buggs;
		}

		public IQueryable<Bugg> ListAll(int Limit)
		{
			var buggs =
				(
					from b in this._mCrashReportDataContext.Buggs
					orderby b.Id descending 
					select b
				).Take(Limit);
			return buggs;
		}

		public IQueryable<Bugg> Search(IQueryable<Bugg> Results, string query)
		{ 
			bool OneQuery = false;
			return Search(Results, query, OneQuery);
		}

		public IQueryable<Bugg> Search(IQueryable<Bugg> Results, string query, bool OneQuery)
		{  
			// TODO Optimize by cutting the fields we're searching for
			// Also may want to revisit how we search since this could get inefficient for a big search set.
			IQueryable<Bugg> buggs;
			try
			{
				var q = HttpUtility.HtmlDecode(query.ToString());
				if (q == null)
				{
					q = string.Empty;
				}

				if (OneQuery)
				{
					string[] Term = { q };

					buggs = (IQueryable<Bugg>)Results
						.Search(Term);
				}
				else
				{
					char[] SearchDelimiters = { ',', ' ', ';'};
					// Take out terms starting with a - 
					var terms = q.Split(SearchDelimiters);
					var excludes = new List<string>();
					string TermsToUse = string.Empty;
					foreach (string term in terms)
					{
						// TODO Convert FunctionCall to functionCallId and 
						// filter results by the FunctionCall before doing the full text search
						// OR only search by functionCalls

						var CallId = this.GetFunctionCallId(term).ToString();

						if (term.StartsWith("-"))
						{
							char[] ExcludingCharacters = { '-' };
							excludes.Add(CallId);
						}
						else
						{
							if (term != "-" && !String.IsNullOrWhiteSpace(term))
							{
								if (TermsToUse.Contains(CallId))
								{
									// Do nothing to ensure that we're only using unique terms
								}
								else
								{
									// This term is unique so add it to TermsToUse
									TermsToUse = TermsToUse + "," + CallId;
								}
							}
						}
					}

					if (TermsToUse == string.Empty)
					{
						buggs = Results;
					}
					else
					{
						//  string[] ColumnsToSearch = { "RawCallStack" };
						//  IQueryable<Crash> crashes = (IQueryable<Crash>)_dataContext.Crashes.Search(new string[] { "GameName" }, new object[] { "Gear" });
						buggs = (IQueryable<Bugg>)Results
							.Search(new string[] { "Pattern" },TermsToUse.Split(SearchDelimiters));

						// remove results with the excluded terms in the callstack
						// TODO do this for Every field or by user requested fields
						if (excludes.Count() > 0)
						{
							foreach (string exclude in excludes)
							{
								if (exclude != string.Empty && exclude != " " && exclude != "-")
								{
									buggs = buggs.Except(buggs.Where(b => b.Pattern.Contains(exclude) ));
								}
							}
						}
					}
				}
			}

			catch (NullReferenceException e)
			{
				buggs = Results;
			}

			return buggs;
	
		}
        public BuggsViewModel GetResults(String SearchQuery, int Page, int PageSize, string SortTerm, string SortOrder, string PreviousOrder, string UserGroup, string DateFrom, string DateTo, string GameName, Boolean OneQuery)
		{
			// TODO Look into whether or not we can change the way we get results to a case statement based on the parameters.
			// Right now we take a Result IQueryable starting with ListAll() Buggs then we widdle away the result set by tacking on 
			// linq queries. Essentially it's Results.ListAll().Where().Where().Where().Where().Where().Where()
			// Could possibly create more optimized queries when we know exactly what we're querying
			// The downside is that if we add more parameters each query may need to be updated.... Or we just add new case statements
			// The other downside is that there's less code reuse, but that may be worth it.

			// TODO Test the framework by timing page load with a simple linq query (i.e. top 100 results from the db) vs this built one
			// This may help decide how much time needs to go into optimizing the queries etc.

			IQueryable<Bugg> Results = null;
			int Skip = (Page - 1) * PageSize;
			int Take = PageSize;

			// Set Default Dates
			if (DateFrom == null || DateFrom == "")
			{
				var df = new DateTime();
				df = DateTime.Now;
				df = df.AddDays(-7);
				DateFrom = df.ToShortDateString();
			}

			if (DateTo == null || DateTo == "")
			{
				var dt = new DateTime();
				dt = DateTime.Now;
				DateTo = dt.ToShortDateString();
			}

			Results = this.ListAll();

			// Look at all buggs that are still 'open' i.e. the last crash occurred in our date range.
			// TODO May want to do other things as well
			Results = this.FilterByDate(Results, DateFrom, DateTo);

			//Grab Results 

			// Start Filtering the results

			// Run at the end
			if (string.IsNullOrEmpty(SearchQuery))
			{
				//Do nothing
			}
			else
			{
				Results = Search(Results, SearchQuery.Trim(), OneQuery);
			}

			// Get UserGroup ResultCounts
			var GroupCounts = this.GetCountsByGroup(Results);

			// TODO Modify this to only switch if the SortTerm is the same
			// ...Otherwise Default to Descending

			// Filter by Usergroup if Present
			
			Results = FilterByUserGroup(Results, UserGroup);
			// Pass in the results and return them sorted properly

			// Set the Count for Pagination
			int ResultCount = Results.Count();

			IEnumerable<BuggsResult> bResults = this.GetSortedResults(Results, SortTerm, SortOrder, DateFrom, DateTo);

			// Grab just the results we want to display on this page
			bResults = bResults.Skip(Skip).Take(Take).ToList();

			///////////////////////////////////////////////////////////

			return new BuggsViewModel
			{
				Results = bResults,
				Query = SearchQuery,
				PagingInfo = new PagingInfo { CurrentPage = Page, PageSize = PageSize, TotalResults = ResultCount },
				Order = SortOrder,
				Term = SortTerm,
				UserGroup = UserGroup,
				DateFrom = DateFrom,
				DateTo = DateTo,
				GameName = GameName,
				GroupCounts = GroupCounts
			};
		}

		public IDictionary<string, int> GetCountsByGroup(IQueryable<Bugg> Buggs)
		{
			IDictionary<string, int> Results = new Dictionary<string, int>();

			var buggsByGroup =
					from b in Buggs
					from bu in this._mCrashReportDataContext.Bugg_UserGroups
					where b.Id == bu.BuggId
					select new { bu.BuggId, bu.UserGroupId };

			var counts =
				from bg in
					(
						from bUg in buggsByGroup
						join g in this._mCrashReportDataContext.UserGroups on bUg.UserGroupId equals g.Id
						select new { g.Name, bUg.BuggId }
						)
				group bg by bg.Name into BuggsGrouped
				select new { Key = BuggsGrouped.Key, Count = BuggsGrouped.Count() };

			int AllResultsCount = 0;
			Results.Add("Epic", 0);
			foreach (var c in counts)
			{
				Results.Add(c.Key, c.Count);
				if (c.Key == "Tester" || c.Key == "Coder" || c.Key == "General")
				{
					AllResultsCount += c.Count;
				}
			}
			Results["Epic"] = AllResultsCount;

			return Results;

		}

		public IDictionary<string, DateTime> GetDates(string DateFrom, string DateTo)
		{
			var df = new DateTime();
			var dt = new DateTime();

			//TODO Figure out what to do here since ToDateTime returns an execption and not NULL
			try
			{
				df = System.Convert.ToDateTime(DateFrom);
				dt = System.Convert.ToDateTime(df.AddDays(1));
			}
			catch (FormatException e)
			{
				df = DateTime.Now;
				df = df.AddDays(-3);
				dt = df.AddDays(1);

			}
			if (!string.IsNullOrEmpty(DateTo))
			{
				dt = System.Convert.ToDateTime(DateTo);
				// add a day since we're using less than date@12:00 am. 
				dt = dt.AddDays(1);
			}
			
			IDictionary<string, DateTime> Dates = new Dictionary<string, DateTime>();
			Dates.Add("DateFrom", df);
			Dates.Add("DateTo", dt);
			return Dates;
		}

		public IQueryable<Bugg> FilterByDate(IQueryable<Bugg> Results, string DateFrom, string DateTo)
		{
			//Filter By Date

			var Dates = this.GetDates(DateFrom, DateTo);

			var BuggsInTimeFrame = Results.Where(b => b.TimeOfLastCrash >= Dates["DateFrom"] && b.TimeOfLastCrash <= Dates["DateTo"]);
			
			return BuggsInTimeFrame;
		}

		public IQueryable<BuggsResult>GetCrashesInTimeFrame(IQueryable<Bugg> Results, string DateFrom, string DateTo)
		{
			var Dates = this.GetDates(DateFrom, DateTo);

			IQueryable<BuggsResult> results =
				(
					from bc in this._mCrashReportDataContext.Bugg_Crashes
					where bc.Crash.TimeOfCrash >= Dates["DateFrom"] && bc.Crash.TimeOfCrash <= Dates["DateTo"]
					group bc by bc.BuggId into CrashesGrouped
					orderby CrashesGrouped.Count() descending
					join b in this._mCrashReportDataContext.Buggs on CrashesGrouped.Key equals b.Id
					select new BuggsResult(b, CrashesGrouped.Count())
				)
				;

			return results;
		}

		public IQueryable<Bugg> FilterByUserGroup(IQueryable<Bugg> Results, string UserGroup)
		{
			if(UserGroup == "Epic")
			{
				var q =
					(
						from b in Results
						where
							b.Bugg_UserGroups.Select(g => g.UserGroup.Name).Contains("General") ||
							b.Bugg_UserGroups.Select(g => g.UserGroup.Name).Contains("Tester") ||
							b.Bugg_UserGroups.Select(g => g.UserGroup.Name).Contains("General") 
						select b
					 );
				return q;
			}
			else
			{
				var q =
				(
					 from b in Results
					 where b.Bugg_UserGroups.Select(g => g.UserGroup.Name).Contains(UserGroup) 
					 select b
				);
				return q;
			}
		}

		public IQueryable<BuggsResult> GetSortedResults(IQueryable<Bugg> Results, string SortTerm, string SortOrder, string DateFrom, string DateTo)
		{
			// if orderby crashes by timeframe do this
			 var Dates = this.GetDates(DateFrom, DateTo);

			 if (SortTerm == "CrashesInTimeFrame")
			 {
				 if (SortOrder == "Descending")
				 {
					 var results = (
						 from bc in this._mCrashReportDataContext.Bugg_Crashes
						 where bc.Crash.TimeOfCrash >= Dates["DateFrom"] && bc.Crash.TimeOfCrash <= Dates["DateTo"]
						 group bc by bc.BuggId into CrashesGrouped
						 orderby CrashesGrouped.Count() descending
						 join b in Results on CrashesGrouped.Key equals b.Id
						 select new BuggsResult(b, CrashesGrouped.Count())
						 )
						 ;
					 return results;
				 }
				 else
				 {
					 var results = 
						(
							from bc in this._mCrashReportDataContext.Bugg_Crashes
							where bc.Crash.TimeOfCrash >= Dates["DateFrom"] && bc.Crash.TimeOfCrash <= Dates["DateTo"]
							group bc by bc.BuggId into CrashesGrouped
							orderby CrashesGrouped.Count()
							join b in Results on CrashesGrouped.Key equals b.Id
							select new BuggsResult(b, CrashesGrouped.Count())
						)
						;
					return results;
				}
			}
			else
			{
				var results =
					(
						from bc in this._mCrashReportDataContext.Bugg_Crashes
						where bc.Crash.TimeOfCrash >= Dates["DateFrom"] && bc.Crash.TimeOfCrash <= Dates["DateTo"]
						group bc by bc.BuggId into CrashesGrouped
						join b in Results on CrashesGrouped.Key equals b.Id
						select new BuggsResult {Bugg = b, CrashesInTimeFrame = CrashesGrouped.Count()}
					)
					;
				// else  do the above set to var results = minus the orderby in the middle

				try
				{
					// TODO Talk to WesH about how to shrink this up a bit
					// Or figure out how to use reflection properly to make this work dynamically
					switch (SortTerm)
					{
						case "Id":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.Id);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.Id);
							}
							break;

						case "LatestCrash":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.TimeOfLastCrash);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.TimeOfLastCrash);
							}
							break;

						case "FirstCrash":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.TimeOfFirstCrash);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.TimeOfFirstCrash);
							}
							break;

						case "NumberOfUsers":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.NumberOfUsers);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.NumberOfUsers);
							}
							break;

						case "NumberOfCrashes":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.NumberOfCrashes);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.NumberOfCrashes);
							}

							break;
						case "Pattern":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.Pattern);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.Pattern);
							}

							break;

						case "Status":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.Status);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.Status);
							}

							break;

						case "FixedChangeList":
							if (SortOrder == "Descending")
							{
								results = results.OrderByDescending(b => b.Bugg.FixedChangeList);
							}
							else
							{
								results = results.OrderBy(b => b.Bugg.FixedChangeList);
							}
							break;
					}
				}
				catch (Exception e)
				{
					Console.WriteLine(e.Message);
					Console.WriteLine(Results.AsQueryable().ToString());
				}
				return results;
			}
		}
		#endregion
	}
}
