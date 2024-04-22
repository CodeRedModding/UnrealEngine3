// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.ComponentModel;
using System.Windows.Forms;

namespace P4PopulateDepot
{
	public class TabControlTabless : TabControl
	{
		private bool bTabsVisible;
		private const int TCM_ADJUSTRECT = 0x1328;

		[DefaultValue(false)]
		public bool TabsVisible
		{
			get 
			{ 
				return bTabsVisible; 
			}
			set
			{
				if (bTabsVisible == value)
				{
					return;
				}
				bTabsVisible = value;
				RecreateHandle();
			}
		}

		public TabControlTabless()
			: base()
		{
		}

		protected override void WndProc(ref Message InMessage)
		{

			if (InMessage.Msg == TCM_ADJUSTRECT)
			{
				if (!TabsVisible && !DesignMode)
				{
					// Mark this message as handled
					InMessage.Result = (IntPtr)1;
					return;
				}
			}
			base.WndProc(ref InMessage);
		}
	}
}
