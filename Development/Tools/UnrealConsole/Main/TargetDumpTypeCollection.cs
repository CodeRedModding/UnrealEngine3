/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealConsole
{
	/// <summary>
	/// A serializable collection of target names and dump types.
	/// </summary>
	public class TargetDumpTypeCollection
	{
		UnrealControls.SerializableDictionary<string, ConsoleInterface.DumpType> mInternalDictionary = new UnrealControls.SerializableDictionary<string, ConsoleInterface.DumpType>();

		/// <summary>
		/// Gets the serializable values associated with the collection.
		/// </summary>
		public UnrealControls.SerializableDictionary<string, ConsoleInterface.DumpType> Values
		{
			get { return mInternalDictionary; }
			set
			{
				if(value != null)
				{
					mInternalDictionary = value;
				}
			}
		}

		/// <summary>
		/// Tries to retrieve the dump type for the specified target.
		/// </summary>
		/// <param name="TargetName">The name of the target to search for.</param>
		/// <param name="OutDumpType">Receives the dump type.</param>
		/// <returns>True if the dump type was retrieved.</returns>
		public bool TryGetValue(string TargetName, out ConsoleInterface.DumpType OutDumpType)
		{
			if(TargetName == null)
			{
				throw new ArgumentNullException("TargetName");
			}

			return mInternalDictionary.TryGetValue(TargetName, out OutDumpType);
		}

		/// <summary>
		/// Associates a dump type with a target.
		/// </summary>
		/// <param name="TargetName">The name of the target.</param>
		/// <param name="NewValue">The dump type to associate with the target.</param>
		public void SetValue(string TargetName, ConsoleInterface.DumpType NewValue)
		{
			if(TargetName == null)
			{
				throw new ArgumentNullException("TargetName");
			}

			mInternalDictionary[TargetName] = NewValue;
		}
	}
}
