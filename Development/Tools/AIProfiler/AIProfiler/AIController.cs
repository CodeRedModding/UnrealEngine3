using System;
using System.Collections.Generic;
using System.IO;

namespace AIProfiler
{
	/** Simple representation of an AI Controller from UE3 */
	class AIController
	{
		/** Index in the name table of the controller's name */
		private Int32					ControllerNameIndex;

		/** Instance number of the controller's name */
		private Int32					ControllerNameInstance;

		/** Index in the name table of the controller's class name */
		private Int32					ControllerClassNameIndex;

		/** Creation time of the controller (relative to WorldInfo.TimeSeconds) */
		private Single					CreationTime;

		/** Profiler stream associated with this AIController */
		private ProfilerStream			AssociatedProfilerStream;

		/** Token symbolizing the destruction of this controller, if any */
		public ControllerDestroyedToken DestructionToken = null;

		/** Tokens emitted by this AIController within the profiler stream */
		public List<EmittedTokenBase>	EmittedTokens = new List<EmittedTokenBase>();

		/**
		 * Constructor
		 * 
		 * @param	InProfilerStream	ProfilerStream associated with this AIController
		 * @param	InStream			Binary reader containing controller data
		 */
		public AIController(ProfilerStream InProfilerStream, BinaryReader InStream)
		{
			ControllerNameIndex = InStream.ReadInt32();
			ControllerNameInstance = InStream.ReadInt32();
			ControllerClassNameIndex = InStream.ReadInt32();
			CreationTime = InStream.ReadSingle();
			AssociatedProfilerStream = InProfilerStream;
		}

		/**
		 * Returns the class name of the controller
		 * 
		 * @return	Class name of the controller
		 */
		public String GetClassName()
		{
			return AssociatedProfilerStream.GetName(ControllerClassNameIndex);
		}

		/**
		 * Returns the name of the controller
		 * 
		 * @return	Name of the controller
		 */
		public String GetName()
		{
			return String.Format("{0}_{1}", AssociatedProfilerStream.GetName(ControllerNameIndex), ControllerNameInstance);
		}

		/**
		 * Overridden string representation of the controller
		 * 
		 * @return	String representation of the controller
		 */
		public override String ToString()
		{
			String ReturnString;
			if (DestructionToken != null)
			{
				ReturnString = String.Format("{0,-30}{1,-30}{2,-8:F2}{3,-15:F2}", GetClassName(), GetName(), CreationTime, DestructionToken.WorldTimeSeconds);
			}
			else
			{
				ReturnString = String.Format("{0,-30}{1,-30}{2,-15:F2}", GetClassName(), GetName(), CreationTime);
			}
			return ReturnString;
		}
	}
}
