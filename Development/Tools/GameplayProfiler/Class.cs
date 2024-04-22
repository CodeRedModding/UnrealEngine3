using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace GameplayProfiler
{
	/** Helper structure encapsulating a class. */
	class Class
	{
		/** Name of the class.					*/
		public string ClassName;
		/** Index of name in name table.		*/
		public Int32 ClassNameIndex;
		/** Super class or null if root class.	*/
		public Class SuperClass;

		/** Constructor, initializing all members. */
		public Class( string InClassName, Int32 InClassNameIndex, Class InSuperClass )
		{
			ClassName = InClassName;
			ClassNameIndex = InClassNameIndex;
			SuperClass = InSuperClass;
		}

		/**
		 * Returns whether this class is a child of. Also returns true if same.
		 * 
		 * @param	CheckClass		Class to see whether this is a child of (or same)
		 * @return	TRUE if this class is a child of or same as CheckClass, FALSE otherwise
		 */
		public bool IsChildOf( Class CheckClass )
		{
			bool bIsOfPassedInType = false;
			Class CurrentClass = this;
			while( CurrentClass != null )
			{
				if( CurrentClass.ClassNameIndex == CheckClass.ClassNameIndex )
				{
					bIsOfPassedInType = true;
					break;
				}
				CurrentClass = CurrentClass.SuperClass;
			}
			return bIsOfPassedInType;
		}
	}
}
