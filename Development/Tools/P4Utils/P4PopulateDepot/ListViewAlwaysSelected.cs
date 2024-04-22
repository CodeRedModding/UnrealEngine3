// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Drawing;
using System.Windows.Forms;

namespace P4PopulateDepot
{
	class ListViewAlwaysSelected : ListView 
	{
		private const int WM_LBUTTONDOWN = 0x0201;
		private const int WM_MBUTTONDBLCLK = 0x0209;

		protected override void WndProc(ref Message InMessage) 
		{
			// Messages in outside the desired area are Swallow mouse messages that are not in the client area to prevent de-selection
			if (InMessage.Msg >= WM_LBUTTONDOWN && InMessage.Msg <= WM_MBUTTONDBLCLK) 
			{
				Point MousePoint = new Point();
				// Extract the screen space mouse pointer coords
				MousePoint.X = (InMessage.LParam.ToInt32() & 0xffff);
				MousePoint.Y = (InMessage.LParam.ToInt32() >> 16);
				var Hit = this.HitTest(MousePoint);
				if (Hit.Location == ListViewHitTestLocations.None				||
					Hit.Location == ListViewHitTestLocations.LeftOfClientArea	||
					Hit.Location == ListViewHitTestLocations.AboveClientArea	||
					Hit.Location == ListViewHitTestLocations.RightOfClientArea
					)
				{
					return;
				}
			}
			base.WndProc(ref InMessage);
		}
	}
}

