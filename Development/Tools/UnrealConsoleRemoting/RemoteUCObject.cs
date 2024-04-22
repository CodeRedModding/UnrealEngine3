/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealConsoleRemoting
{
	/// <summary>
	/// Object for marshaling calls across process boundires.
	/// </summary>
	public class RemoteUCObject : MarshalByRefObject, IUnrealConsole
	{
		static IUnrealConsole mInternalUC;

		/// <summary>
		/// Sets the global unreal console instance.
		/// </summary>
		public static IUnrealConsole InternalUnrealConsole
		{
			set { mInternalUC = value; }
		}

		/// <summary>
		/// Opens a tab for the specified target.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <param name="bClearOutputWindow">True if the output for the specified target is to be cleared.</param>
		/// <returns>True if the target is located.</returns>
		public bool OpenTarget(string Platform, string Target, bool bClearOutputWindow)
		{
			if(mInternalUC != null)
			{
				return mInternalUC.OpenTarget(Platform, Target, bClearOutputWindow);
			}

			return false;
		}

		/// <summary>
		/// Searches for the specified target.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <returns>True if the target is located.</returns>
		public bool HasTarget(string Platform, string Target)
		{
			if(mInternalUC != null)
			{
				return mInternalUC.HasTarget(Platform, Target);
			}

			return false;
		}
	}
}
