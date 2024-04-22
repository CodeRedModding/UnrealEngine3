using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace GameplayProfiler
{
	/** Helper class encapsulating a frame in the token stream. */
	class Frame
	{
		/** Index into token array this frame starts at. */
		public int StartIndex;
		/** Time this frame took overall, including untracked items. */
		public float FrameTime;
		/** Time tracked this frame by profiler. */
		public float TrackedTime = 0;
		/** List of ticked actors. */
		public List<TokenActor> Actors = null;
		/** Mapping from function name to function info.	*/
		public Dictionary<string, FunctionInfo> NameToFunctionInfoMap = null;

		/** Constructor, initializing all members. */
		public Frame(List<TokenActor> InActors, Dictionary<string, FunctionInfo> InNameToFunctionInfoMap, int InStartIndex, float InFrameTime, float InTrackedTime)
		{
			StartIndex = InStartIndex;
			FrameTime = InFrameTime;
			TrackedTime = InTrackedTime;
			Actors = new List<TokenActor>(InActors);
			NameToFunctionInfoMap = new Dictionary<string, FunctionInfo>(InNameToFunctionInfoMap);
		}
	}
}
