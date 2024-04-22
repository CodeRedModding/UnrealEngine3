/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealConsoleRemoting
{
	/// <summary>
	/// Interface for interacting with UnrealConsole across process boundries.
	/// </summary>
	public interface IUnrealConsole
	{
		/// <summary>
		/// Opens a tab for the specified target.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <param name="bClearOutputWindow">True if the output for the specified target is to be cleared.</param>
		/// <returns>True if the target is located.</returns>
		bool OpenTarget(string Platform, string Target, bool bClearOutputWindow);

		/// <summary>
		/// Searches for the specified target.
		/// </summary>
		/// <param name="Platform">The platform the target belongs to.</param>
		/// <param name="Target">The name, debug channel IP, or title IP of the requested target.</param>
		/// <returns>True if the target is located.</returns>
		bool HasTarget(string Platform, string Target);
	}
}
