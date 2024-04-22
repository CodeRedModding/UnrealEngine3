// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Data.Linq;

namespace CrashReport.Models
{
	public interface IBuggRepository
	{
		CrashReportDataContext GetDataContext();
		Bugg Get(int id);
		Bugg GetByPattern(string Pattern);
		IQueryable<Crash> GetCrashesByPattern(string pattern);
		string GetFunctionCall(int Id);
		int GetFunctionCallId(string Call);
		IList<string> GetFunctionCalls(string Pattern);
		IList<string> GetFunctionCalls(IList<int> Ids);
		void CreateBuggFromPattern(String Pattern);
		void UpdateBuggData(Bugg Bugg, IEnumerable<Crash> Crashes);
		void UpdateBuggData(Bugg Bugg, EntitySet<Crash> Crashes);
		void AddCrashToBugg(Crash Crash);
		void LinkCrashToBugg(Bugg Bugg, Crash Crash);
		void LinkUserToBugg(Bugg Bugg, Crash Crash);
		void LinkUserToBugg(Bugg Bugg, string UserName);
		void LinkUserGroupToBugg(Bugg Bugg, Crash Crash);
		void LinkUserGroupToBugg(Bugg Bugg, int? UserGroupId);
		void Insert(Bugg bugg);
		void Insert(Crash crash);
		void AddCrashes(Bugg bugg, IQueryable<Crash> Crashes);
		User AddUser(string UserName);
		User AddUser(string UserName, string UserGroup);
		User AddUser(string UserName, string UserGroup, int UserGroupId);
		void BuildPattern(Crash crash);
		void SubmitChanges();
		IQueryable<Bugg> ListAll();
		IQueryable<Bugg> ListAll(int Limit);
		IQueryable<Bugg> FilterByUserGroup(IQueryable<Bugg> Results, string UserGroup);
		BuggsViewModel GetResults(String SearchQuery, int Page, int PageSize, string SortTerm, string SortOrder, string PreviousOrder, string UserGroup, string DateFrom, string DateTo, string GameName, Boolean OneQuery);
		IQueryable<Bugg> Search(IQueryable<Bugg> Results, string query);
		IQueryable<Bugg> Search(IQueryable<Bugg> Results, string query, Boolean OneQuery);
	}
}