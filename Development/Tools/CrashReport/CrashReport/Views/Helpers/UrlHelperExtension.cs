// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System.Web.Mvc;
using CrashReport.Models;
using System.Text;

namespace CrashReport.Views.Helpers
{
	public static class UrlHelperExtension
	{
		//Methods for the CrashesViewModel
		public static MvcHtmlString TableHeader(this UrlHelper helper, string HeaderName, string SortTerm, CrashesViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action( "Index", new { controller = "crashes", SortTerm = SortTerm, PreviousOrder = Model.Order, PreviousTerm = Model.Term, Page = Model.PagingInfo.CurrentPage, SearchQuery = Model.Query, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = Model.UserGroup, GameName = Model.GameName, CrashType = Model.CrashType } );

			Tag.MergeAttribute("href", url );
			string arrow = "";

			if (Model.Term == SortTerm)
			{
				if (Model.Order == "Descending")
				{
					arrow = "<img border=0 src='../../Content/Images/SortDescending.png' />";
				}
				else
				{
					arrow = "<img border=0 src='../../Content/Images/SortAscending.png' />";
				}
			}
			else
			{
				arrow = "<img border=0 src='../../Content/Images/SortPlaceHolder.png' />";
			}
			
			Tag.InnerHtml = "<span>" + HeaderName+"&nbsp;"+arrow+"</span>" ;
			Tag.AddCssClass("SortLink");

			result.AppendLine(Tag.ToString() );
			return MvcHtmlString.Create(result.ToString());
		}

		public static MvcHtmlString CallStackSearchLink(this UrlHelper helper, string CallStack, CrashesViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action("Index", new { controller = "crashes", SortTerm = Model.Term, SortOrder = Model.Order, SearchQuery = CallStack, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = Model.UserGroup, GameName = Model.GameName, OneQuery = true, CrashType = Model.CrashType });

			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = CallStack;
			Tag.AddCssClass("CallStackSearchLink");

			result.AppendLine(Tag.ToString());
			return MvcHtmlString.Create(result.ToString());
		}

		public static MvcHtmlString UserGroupLink(this UrlHelper helper, string UserGroup, CrashesViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action("Index", new { controller = "crashes", SortTerm = Model.Term, SortOrder = Model.Order, SearchQuery = Model.Query, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = UserGroup, GameName = Model.GameName, CrashType = Model.CrashType });

			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = UserGroup;

			result.AppendLine(Tag.ToString());
			return MvcHtmlString.Create(result.ToString());
		}

		//Methods for the BuggsViewModel
		public static MvcHtmlString TableHeader(this UrlHelper helper, string HeaderName, string SortTerm, BuggsViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action("Index", new { controller = "buggs", SortTerm = SortTerm, PreviousOrder = Model.Order, PreviousTerm = Model.Term, Page = Model.PagingInfo.CurrentPage, SearchQuery = Model.Query, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = Model.UserGroup, GameName = Model.GameName });

			Tag.MergeAttribute("href", url);
			string arrow = "";

			if (Model.Term == SortTerm)
			{
				if (Model.Order == "Descending")
				{
					arrow = "<img border=0 src='../../Content/Images/SortDescending.png' />";
				}
				else
				{
					arrow = "<img border=0 src='../../Content/Images/SortAscending.png' />";
				}
			}
			else
			{
				arrow = "<img border=0 src='../../Content/Images/SortPlaceHolder.png' />";
			}

			Tag.InnerHtml = "<span>" + HeaderName + "&nbsp;" + arrow + "</span>";
			Tag.AddCssClass("SortLink");
			result.AppendLine(Tag.ToString());

			return MvcHtmlString.Create(result.ToString());
		}

		public static MvcHtmlString CallStackSearchLink(this UrlHelper helper, string CallStack, BuggsViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action("Index", new { controller = "buggs", SortTerm = Model.Term, SortOrder = Model.Order, SearchQuery = CallStack, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = Model.UserGroup, GameName = Model.GameName, OneQuery = true });

			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = CallStack;
			Tag.AddCssClass("CallStackSearchLink");

			result.AppendLine(Tag.ToString());
			return MvcHtmlString.Create(result.ToString());
		}

		public static MvcHtmlString UserGroupLink(this UrlHelper helper, string UserGroup, BuggsViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action("Index", new { controller = "Buggs", SortTerm = Model.Term, SortOrder = Model.Order, SearchQuery = Model.Query, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = UserGroup, GameName = Model.GameName });

			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = UserGroup;

			result.AppendLine(Tag.ToString());
			return MvcHtmlString.Create(result.ToString());
		}

		//Methods for the BuggViewModel
		//These are mostly disabled and just generate links to the same page for now
		public static MvcHtmlString TableHeader(this UrlHelper helper, string HeaderName, string SortTerm, BuggViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			//TODO Put in Sorting if this is requested. Took it out temporarily since I don't think this will be needed in this view
			//var url = helper.Action("Index", new { controller = "buggs", SortTerm = SortTerm, PreviousOrder = Model.Order, Page = Model.PagingInfo.CurrentPage, SearchQuery = Model.Query, DateFrom = Model.DateFrom, DateTo = Model.DateTo, UserGroup = Model.UserGroup, GameName = Model.GameName });
			var url = helper.Action("Show", new { controller = "buggs", id=Model.Bugg.Id});

			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = "<span>" + HeaderName+"</span>";
			Tag.AddCssClass("SortLink"); 

			result.AppendLine(Tag.ToString());

			return MvcHtmlString.Create(result.ToString());
		}

		public static MvcHtmlString CallStackSearchLink(this UrlHelper helper, string CallStack, BuggViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag
			var url = helper.Action("Show", new { controller = "buggs", id = Model.Bugg.Id});
			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = CallStack;
			Tag.AddCssClass("CallStackSearchLink");

			result.AppendLine(Tag.ToString());
			return MvcHtmlString.Create(result.ToString());
		}

		public static MvcHtmlString UserGroupLink(this UrlHelper helper, string UserGroup, BuggViewModel Model)
		{
			StringBuilder result = new StringBuilder();
			TagBuilder Tag = new TagBuilder("a"); // Construct an <a> Tag

			var url = helper.Action("Show", new { controller = "Buggs",  id = Model.Bugg.Id });

			Tag.MergeAttribute("href", url);
			Tag.InnerHtml = UserGroup;

			result.AppendLine(Tag.ToString());
			return MvcHtmlString.Create(result.ToString());
		}
	}
}