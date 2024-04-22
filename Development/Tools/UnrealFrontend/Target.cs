/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;

namespace UnrealFrontend
{
	public class Target : System.ComponentModel.INotifyPropertyChanged
	{
		public Target( String InName, bool bShouldUse )
		{
			mName = InName;
			mShouldUseTarget = bShouldUse;
		}

		public Target(ConsoleInterface.PlatformTarget InTarget)
		{
			TheTarget = InTarget;
			mName = TheTarget.TargetManagerName;
		}

		public ConsoleInterface.PlatformTarget TheTarget { get; private set; }

		private String mName = "Unknown";
		public String Name
		{
			get { return mName; }
			set
			{
				if (mName != value)
				{
					mName = value;
					NotifyPropertyChanged("Name");
				}
			}
		}

		private String mTitleIp = "n/a";
		public String TitleIp
		{
			get { return mTitleIp; }
			set
			{
				if (mTitleIp != value)
				{
					mTitleIp = value;
					NotifyPropertyChanged("TitleIp");
				}
			}
		}

		private String mDebugIp = "n/a";
		public String DebugIp
		{
			get { return mDebugIp; }
			set
			{
				if (mDebugIp != value)
				{
					mDebugIp = value;
					NotifyPropertyChanged("DebugIp");
				}
			}
		}

		private String mTargetType = "n/a";
		public String TargetType
		{
			get { return mTargetType; }
			set
			{
				if (mTargetType != value)
				{
					mTargetType = value;
					NotifyPropertyChanged("TargetType");
				}
			}
		}

		private bool mShouldUseTarget = false;
		public bool ShouldUseTarget
		{
			get { return mShouldUseTarget; }
			set
			{
				if (mShouldUseTarget != value)
				{
					mShouldUseTarget = value;
					NotifyPropertyChanged("ShouldUseTarget");
				}
			}
		}




		#region INotifyPropertyChanged

		public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
		private void NotifyPropertyChanged(String PropertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new System.ComponentModel.PropertyChangedEventArgs(PropertyName));

				Session.Current.SaveSessionSettings();
			}
		}

		#endregion


		public Target Clone()
		{
			return (Target)this.MemberwiseClone();
		}

	}
}
