// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Web.Mvc;
using CrashReport.Models;

namespace CrashReport.Controllers
{
	public class DashboardController : Controller
	{
		//
		// GET: /Dashboard/

		public ActionResult Index()
		{
			CrashRepository CrashRepository = new CrashRepository();
			IQueryable<Crash> Crashes = CrashRepository.ListAll();

			var GeneralResults = CrashRepository.GetWeeklyCountsByGroup(Crashes, "General");
			var CoderResults = CrashRepository.GetWeeklyCountsByGroup(Crashes, "Coder");
			var TesterResults = CrashRepository.GetWeeklyCountsByGroup(Crashes, "Tester");
			var AutomatedResults = CrashRepository.GetWeeklyCountsByGroup(Crashes, "Automated");
			var AllResults = CrashRepository.GetWeeklyCountsByGroup(Crashes, "All");
			var DailyGeneralResults = CrashRepository.GetDailyCountsByGroup(Crashes, "General");
			var DailyCoderResults = CrashRepository.GetDailyCountsByGroup(Crashes, "Coder");
			var DailyTesterResults = CrashRepository.GetDailyCountsByGroup(Crashes, "Tester");
			var DailyAutomatedResults = CrashRepository.GetDailyCountsByGroup(Crashes, "Automated");
			var DailyAllResults = CrashRepository.GetDailyCountsByGroup(Crashes, "All");

			string CrashesByDate = string.Empty;

			foreach(var r in AllResults)
			{
				string line = string.Empty;

				int GeneralCrashes = 0;
				int CoderCrashes = 0;
				int TesterCrashes = 0;
				int AutomatedCrashes = 0;
				try
				{
					GeneralCrashes = GeneralResults[r.Key];
					CoderCrashes = CoderResults[r.Key];
					TesterCrashes = TesterResults[r.Key];
					AutomatedCrashes = AutomatedResults[r.Key];
				}
				catch (KeyNotFoundException)
				{
					//Do nothing everything already has the appropriate default value
				}

				int year = r.Key.Year;

				int month = r.Key.AddMonths(-1).Month;
				if (r.Key.Month == 13 || r.Key.Month == 1)
				{
					month = 0;
				}
				
				int day = (r.Key.Day + 6);

				

				line = "[new Date(" + year+ ", " + month + ", " + day+ "), " + GeneralCrashes + ", " + CoderCrashes + ", " + TesterCrashes + ", " + AutomatedCrashes+", " +r.Value + "], ";
				CrashesByDate = CrashesByDate + line;
			}

			CrashesByDate = CrashesByDate.TrimEnd(new char[] {',', ' '});

			string CrashesByDay= string.Empty;

			foreach (var r in DailyAllResults)
			{
				string line = string.Empty;

				int GeneralCrashes = 0;
				int CoderCrashes = 0;
				int TesterCrashes = 0;
				int AutomatedCrashes = 0;
				try
				{
					GeneralCrashes = DailyGeneralResults[r.Key];
					CoderCrashes = DailyCoderResults[r.Key];
					TesterCrashes = DailyTesterResults[r.Key];
					AutomatedCrashes = DailyAutomatedResults[r.Key];
				}
				catch (KeyNotFoundException)
				{
					//Do nothing everything already has the appropriate default value
				}

				int year = r.Key.Year;

				int month = r.Key.AddMonths(-1).Month;
				if (r.Key.Month == 13 || r.Key.Month == 1)
				{
					month = 0;
				}

				int day = r.Key.Day;

				line = "[new Date(" + year + ", " + month + ", " + day + "), " + GeneralCrashes + ", " + CoderCrashes + ", " + TesterCrashes + ", " + AutomatedCrashes + ", " + r.Value + "], ";
				CrashesByDay = CrashesByDay + line;
			}

			CrashesByDay = CrashesByDay.TrimEnd(new char[] { ',', ' ' });

			return View("Index", new DashboardViewModel 
			{
				CrashesByDate = CrashesByDate,
				CrashesByDay = CrashesByDay
			});
		}
	}
}