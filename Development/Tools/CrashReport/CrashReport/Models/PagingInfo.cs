// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;

namespace CrashReport.Models
{
	public class PagingInfo
	{
		public int CurrentPage { get; set; }
		public int TotalResults { get; set; }
		public int PageSize { get; set; }

		public int PageCount
		{
			//return total number of pages
			// grab the total responses divided by PageCount
			get { return (int)Math.Ceiling((decimal)TotalResults / PageSize); }		 
		}

		public int FirstPageIndex
		{
			//return the Index for the first result in the paging list to display
			get {if(CurrentPage >= this.PagingListSize) return CurrentPage;  else return 1 ; }
		}

		public int LastPageIndex
		{
			//return the Index for the last result in the paging list to display
			get { if (((FirstPageIndex - 1) + PagingListSize) < LastPage) return (FirstPageIndex - 1) + PagingListSize; else return LastPage; }
		}

		public int PagingListSize
		{
			//return the number of pages to display on the screen
			get { return 10; } 
			
		}

		public int PreviousPageIndex
		{
			//return the Index for the previous result in the paging list to display
			get { if (CurrentPage - 1 > 1) return CurrentPage - 1; else return 1; }
		}

		public int NextPageIndex
		{
			//return the Index for the previous result in the paging list to display
			get { if (CurrentPage + 1 <= LastPage) return CurrentPage + 1; else return LastPage; }
		}

		public int FirstPage
		{
			//return the Index for the first result in the paging list to display
			get {  return 1; }
		}

		public int LastPage
		{
			//return the Index for the last result 
			get { return PageCount; }
		}
	}
}