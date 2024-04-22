/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Collections.Generic;

namespace UnrealFrontend
{
	/// <summary>
	/// Each profile has a list of targets.
	/// That list and associated functionality is found in this class.
	/// </summary>
	public class TargetsList : INotifyPropertyChanged
	{
		public TargetsList(Profile InPersistentProfile)
		{
			PersistentProfile = InPersistentProfile;
			Targets = new ObservableCollection<Target>();
			AreTargetsDirty = true;
		}
		
		private Profile PersistentProfile { get; set; }


		public ObservableCollection<Target> Targets { get; private set; }
		private bool AreTargetsDirty { get; set; }

		public void QueueUpdateTargets( bool ForceRefresh )
		{
			bool bPerformRefresh = ForceRefresh || AreTargetsDirty;

			if (bPerformRefresh)
			{
				ConsoleInterface.Platform PlatformToRefresh = null;
				ConsoleInterface.DLLInterface.TryGetPlatform(PersistentProfile.TargetPlatformType, ref PlatformToRefresh);

				// Early out if we're a PC
				if (PlatformToRefresh == null || PlatformToRefresh.Type == ConsoleInterface.PlatformType.PC)
				{
					Targets.Clear();
				}
				else
				{
					if (this.Targets.Count != 0)
					{
						// Unless this is the first run, we should save the active targets so we can properly restore them later.
						SaveActiveTargets();
					}

					// Queue up work to get additional target information.
					System.Windows.Threading.Dispatcher UIDispatcher = System.Windows.Application.Current.Dispatcher;
					Session.Current.QueueRefreshTask(() =>
					{
						// Quickly repopulate the list with all potentially available targets
						PlatformToRefresh.EnumerateAvailableTargets();

						List<Target> DetailedInfoTargets = new List<Target>(PlatformToRefresh.Targets.Length);
						// Get detailed information about the targets in the list. (can be time-consuming due to timeouts)
						foreach (ConsoleInterface.PlatformTarget ThePlatformTarget in PlatformToRefresh.Targets)
						{
							Target TargetWithDetailedInfo = new Target(ThePlatformTarget)
							{
								Name = ThePlatformTarget.Name,
								TitleIp = ThePlatformTarget.IPAddress.ToString(),
								DebugIp = ThePlatformTarget.DebugIPAddress.ToString(),
								TargetType = ThePlatformTarget.ConsoleType.ToString(),
							};

							TargetWithDetailedInfo.ShouldUseTarget =
								PersistentProfile.ActiveTargets.Contains(TargetWithDetailedInfo.Name) ||
								PersistentProfile.ActiveTargets.Contains(TargetWithDetailedInfo.TitleIp) || 
								PersistentProfile.ActiveTargets.Contains(TargetWithDetailedInfo.DebugIp);

							DetailedInfoTargets.Add(TargetWithDetailedInfo);
						}

						// Dispatch a more detailed UI update.
						UIDispatcher.BeginInvoke(new VoidDelegate(() =>
						{
							Targets.Clear();
							foreach (Target SomeTarget in DetailedInfoTargets)
							{
								Targets.Add(SomeTarget);
							}
							EnsureTargetSelection();
							SaveActiveTargets();
						}));

						AreTargetsDirty = false;
					});
				}
			}
		}


		public void EnsureTargetSelection()
		{
			if (Targets.Count > 0)
			{
				foreach (Target SomeTarget in Targets)
				{
					if (SomeTarget.ShouldUseTarget)
					{
						return;
					}
				}
				Targets[0].ShouldUseTarget = true;
			}

		}
		
		#region INotifyPropertyChanged

		private void AssignBool(String InPropName, ref bool InOutProp, bool InNewValue)
		{
			if (InOutProp != InNewValue)
			{
				InOutProp = InNewValue;
				NotifyPropertyChanged(InPropName);
			}
		}

		public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
		private void NotifyPropertyChanged(String PropertyName)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new System.ComponentModel.PropertyChangedEventArgs(PropertyName));
			}
		}

		#endregion

		/// Targets can be time consuming to refresh due to timeouts.
		/// Clone this info to decrease iteration times.
		public TargetsList Clone( Profile InNewOwner )
		{
			TargetsList NewTargetList = new TargetsList(InNewOwner);

			foreach(Target SomeTarget in Targets)
			{
				NewTargetList.Targets.Add(SomeTarget.Clone());
			}

			return NewTargetList;
			
		}

		internal void SaveActiveTargets()
		{
			List<String> CurrentActiveTargets = new List<String>();
			foreach (Target SomeTarget in Targets)
			{
				if (SomeTarget.ShouldUseTarget)
				{
					CurrentActiveTargets.Add(SomeTarget.Name);
				}
			}
			PersistentProfile.ActiveTargets = CurrentActiveTargets;
		}
	}
}
