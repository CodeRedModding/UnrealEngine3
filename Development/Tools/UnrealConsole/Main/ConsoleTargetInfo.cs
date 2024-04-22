/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealConsole
{
	/// <summary>
	/// Class for serializing information about a target.
	/// </summary>
	public class ConsoleTargetInfo
	{
		string mPlatformName = "";
		string mTargetName = "";

		/// <summary>
		/// Gets/Sets the name of the platform the target belongs to.
		/// </summary>
		public string PlatformName
		{
			get { return mPlatformName; }
			set
			{
				if(value != null)
				{
					mPlatformName = value;
				}
			}
		}

		/// <summary>
		/// Gets/Sets the name of the target.
		/// </summary>
		public string TargetName
		{
			get 
			{ 
				return mTargetName; 
			}
			set
			{
				if(value != null)
				{
					mTargetName = value;
				}
			}
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		public ConsoleTargetInfo()
		{
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="Platform">The name of the platform the target belongs to.</param>
		/// <param name="Target">The name of the target.</param>
		public ConsoleTargetInfo(string Platform, string Target)
		{
			if(Platform == null)
			{
				throw new ArgumentNullException("Platform");
			}

			if(Target == null)
			{
				throw new ArgumentNullException("Target");
			}

			mPlatformName = Platform;
			mTargetName = Target;
		}
	}
}
