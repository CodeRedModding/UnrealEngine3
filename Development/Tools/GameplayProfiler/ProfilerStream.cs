using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace GameplayProfiler
{
	/** Profiler data stream as serialized from file with minor processing. */
	class ProfilerStream
	{
		/** Name table, as serialized from UE3. */
		public List<string> NameTable = new List<string>();

		/** Class list (including hierarchy) as serialized from UE3. */
		public List<Class> Classes = new List<Class>();
		/** Mapping from class name index to class object. */
		public Dictionary<Int32,Class> IndexToClassMap = new Dictionary<Int32,Class>();
		/** AActor class. */
		public Class ActorClass = null;
		/** UActorComponent class. */
		public Class ComponentClass = null;
		/** UFunction class. */
		public Class FunctionClass = null;

		/** List of tokens that make up this stream. */
		public List<TokenBase> Tokens = new List<TokenBase>();

		/** List of frames in this stream. */
		public List<Frame> Frames = new List<Frame>();

		/** Aggregate mapping from function name to function info.	*/
		public Dictionary<string, FunctionInfo> AggregateNameToFunctionInfoMap = new Dictionary<string, FunctionInfo>(); 

		/**
		 * Returns the name associated with the passed in index.
		 * 
		 * @param	Index	Index in name table
		 * @return	Name associated with name table index
		 */
		public string GetName(int Index)
		{
			return NameTable[Index];
		}

		/**
		 * Convertes passed in indices to an object name in Class Outermost.Outer.Object_ObjectInstance form, skipping
		 * parts that are passed in as -1 and folding identical Outer and Outermost into a single entry.
		 * 
		 * @param	ObjectInstanceIndex		The numerical suffix of an object's name
		 * @param	ObjectNameIndex			The object's name's index into the name table
		 * @param	ClassNameIndex			The object's class' name's index into the name table
		 * @param	ClassNameIndex			The object's outer name's index into the name table
		 * @param	OutermostNameIndex		The object's outermost name's index into the name table
		 * @param	AssetNameIndex			The object's asset name's index into the name table
		 * 
		 * @return	Name in Class Outermost.Outer.Object_ObjectInstance format
		 */
		public string GetObjectName(int ObjectInstanceIndex, int ObjectNameIndex, int ClassNameIndex, int OuterNameIndex, int OutermostNameIndex, int AssetNameIndex)
		{
			string FullName = "";
			if(ClassNameIndex >= 0)
			{
				FullName += GetName(ClassNameIndex) + " ";
			}
			if(OutermostNameIndex != OuterNameIndex && OutermostNameIndex >= 0)
			{
				FullName += GetName(OutermostNameIndex) + ".";
			}
			if(OuterNameIndex >= 0)
			{
				FullName += GetName(OuterNameIndex) + ".";
			}
			FullName += GetName(ObjectNameIndex);
			if(ObjectInstanceIndex >= 0)
			{
				FullName += "_" + ObjectInstanceIndex.ToString();
			}
			if (AssetNameIndex > 0)
			{
				FullName += " (" + GetName(AssetNameIndex) + ")";
			}
			return FullName;
		}
	}
}

