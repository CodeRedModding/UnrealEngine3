/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;

namespace UnrealConsole
{
	/// <summary>
	/// Class for serializing command history.
	/// </summary>
	public class ConsoleTargetCmdHistory
	{
		ConsoleTargetInfo mInfo = new ConsoleTargetInfo();
		AutoCompleteStringCollection mCmdHistory = new AutoCompleteStringCollection();

		/// <summary>
		/// Gets/Sets information about the target the command history belongs to.
		/// </summary>
		public ConsoleTargetInfo TargetInfo
		{
			get { return mInfo; }
			set
			{
				if(value != null)
				{
					mInfo = value;
				}
			}
		}

		/// <summary>
		/// Gets/Sets the command history to be serialized.
		/// </summary>
		public AutoCompleteStringCollection CommandHistory
		{
			get { return mCmdHistory; }
			set
			{
				if(value != null)
				{
					mCmdHistory = value;
				}
			}
		}
	}
}
