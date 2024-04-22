/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;
using System.Xml.Serialization;
using System.Windows.Input;

namespace UnrealFrontend.Pipeline
{
	public abstract class Step : INotifyPropertyChanged
	{
		[XmlIgnore]
		public abstract bool SupportsClean { get; }
		[XmlIgnore]
		public abstract String StepName { get; }
		[XmlIgnore]
		public abstract String StepNameToolTip { get; }
		[XmlIgnore]
		public abstract bool SupportsReboot { get; }
		[XmlIgnore]
		public abstract String ExecuteDesc { get; }
		[XmlIgnore]
		public virtual String CleanAndExecuteDesc { get { return "Clean and Execute Step"; } }
		[XmlIgnore]
		public virtual Key KeyBinding { get { return Key.None; } }

		public abstract bool Execute(IProcessManager ProcessManager, Profile InProfile);
		public abstract bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile);

		private bool mShouldSkipThisStep = false;
		public bool ShouldSkipThisStep
		{
			get { return mShouldSkipThisStep; }
			set { AssignBool("ShouldSkipThisStep", ref mShouldSkipThisStep, value); }
		}

		private bool mRebootBeforeStep = true;
		public bool RebootBeforeStep
		{
			get { return mRebootBeforeStep; }
			set { AssignBool("RebootBeforeStep", ref mRebootBeforeStep, value); }
		}

		#region INotifyPropertyChanged

		public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
		protected void NotifyPropertyChanged(String PropertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new System.ComponentModel.PropertyChangedEventArgs(PropertyName));
			}
		}

		protected void AssignBool(String InPropName, ref bool InOutProp, bool InNewValue)
		{
			if (InOutProp != InNewValue)
			{
				InOutProp = InNewValue;
				NotifyPropertyChanged(InPropName);
			}
		}

		#endregion
	}

}
