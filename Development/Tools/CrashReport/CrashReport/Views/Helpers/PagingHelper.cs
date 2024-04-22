// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Web.Mvc;
using CrashReport.Models;
using System.Text;

namespace CrashReport.Views.Helpers
{
	public static class PagingHelper
	{
		public static MvcHtmlString PageLinks(this HtmlHelper html, PagingInfo pagingInfo, Func<int, string> pageUrl)
		{
			StringBuilder result = new StringBuilder();

			// go to first page
			TagBuilder FirstTag = new TagBuilder("a"); // Construct an <a> Tag
			FirstTag.MergeAttribute("href", pageUrl(pagingInfo.FirstPage));
			FirstTag.InnerHtml = "<<";

			result.AppendLine(FirstTag.ToString());

			// go to previous page
			TagBuilder PreviousTag = new TagBuilder("a"); // Construct an <a> Tag
			PreviousTag.MergeAttribute("href", pageUrl(pagingInfo.PreviousPageIndex));
			PreviousTag.InnerHtml = "<";

			result.AppendLine(PreviousTag.ToString());

			for (int i = pagingInfo.FirstPageIndex; i <= pagingInfo.LastPageIndex; i++)
			{
				TagBuilder tag = new TagBuilder("a"); // Construct an <a> Tag
				tag.MergeAttribute("href", pageUrl(i));
				tag.InnerHtml = i.ToString();
				if (i == pagingInfo.CurrentPage)
					tag.AddCssClass("selectedPage");
				result.AppendLine(tag.ToString());
			}

			// go to next page
			TagBuilder NextTag = new TagBuilder("a"); // Construct an <a> Tag
			NextTag.MergeAttribute("href", pageUrl(pagingInfo.NextPageIndex));
			NextTag.InnerHtml = ">";

			result.AppendLine(NextTag.ToString());

			// go to last page
			TagBuilder LastTag = new TagBuilder("a"); // Construct an <a> Tag
			LastTag.MergeAttribute("href", pageUrl(pagingInfo.LastPage));
			LastTag.InnerHtml = ">>";

			result.AppendLine(LastTag.ToString());

			return MvcHtmlString.Create(result.ToString());
		}
	}
}