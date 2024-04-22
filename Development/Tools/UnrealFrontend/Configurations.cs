/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using ConsoleInterface;
using System.Collections;

namespace UnrealFrontend
{
	public static class ConfigManager
	{
		private static Dictionary<ConsoleInterface.PlatformType, PlatformConfigs> PlatformToConfigMap;

		public static PlatformConfigs ConfigsFor( ConsoleInterface.PlatformType InPlatformType )
		{
			return PlatformToConfigMap[InPlatformType];
		}

		static ConfigManager()
		{
			PlatformToConfigMap = new Dictionary<ConsoleInterface.PlatformType, PlatformConfigs>();

			PlatformConfigs AllPlatformConfigs = new PlatformConfigs
			{
				LaunchConfigs = new ConfigList
				{
					// Config                                    UDK 
					{ Profile.Configuration.Release_32,          true },
					{ Profile.Configuration.Release_64,          true },
					{ Profile.Configuration.Debug_32,            true },
					{ Profile.Configuration.Debug_64,            true },
					{ Profile.Configuration.Shipping_32,         true },
					{ Profile.Configuration.Shipping_64,         true },
					{ Profile.Configuration.Test_32,			 true },
					{ Profile.Configuration.Test_64,			 true },
				},
				CommandletConfigs = new ConfigList{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          true },
					{ Profile.Configuration.Release_64,          true },
					{ Profile.Configuration.Debug_32,            true },
					{ Profile.Configuration.Debug_64,            true },
					{ Profile.Configuration.Shipping_32,         true },
					{ Profile.Configuration.Shipping_64,         true },
				},
				ScriptConfigs = new ConfigList{
					// Config                                    UDK
					{ Profile.Configuration.ReleaseScript,       true },
					{ Profile.Configuration.DebugScript,         true },					
					{ Profile.Configuration.FinalReleaseScript,  true },
				}
			};

			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.Linux, AllPlatformConfigs);

			PlatformConfigs MacPlatformConfigs = new PlatformConfigs
			{
				LaunchConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_64,          true },
					{ Profile.Configuration.Debug_64,            false },					
					{ Profile.Configuration.Shipping_64,         true },
				},
				CommandletConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          false },
					{ Profile.Configuration.Release_64,          false },
					{ Profile.Configuration.Debug_32,            false },
					{ Profile.Configuration.Debug_64,            false },
					{ Profile.Configuration.Shipping_32,         true  },
					{ Profile.Configuration.Shipping_64,         false },
				},
				ScriptConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.ReleaseScript,       true },
					{ Profile.Configuration.DebugScript,         true },
					{ Profile.Configuration.FinalReleaseScript,  true },
				}
			};

			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.MacOSX, MacPlatformConfigs);

			PlatformConfigs MobilePlatformConfigs = new PlatformConfigs
			{
				LaunchConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          true },
					{ Profile.Configuration.Debug_32,            false },					
					{ Profile.Configuration.Shipping_32,         true },
					{ Profile.Configuration.Test_32,			 true },
				},
				CommandletConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          false },
					{ Profile.Configuration.Release_64,          false },
					{ Profile.Configuration.Debug_32,            false },
					{ Profile.Configuration.Debug_64,            false },
					{ Profile.Configuration.Shipping_32,         true  },
					{ Profile.Configuration.Shipping_64,         false },
				},
				ScriptConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.ReleaseScript,       true },
					{ Profile.Configuration.DebugScript,         true },
					{ Profile.Configuration.FinalReleaseScript,  true },
				}
			};
			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.IPhone, MobilePlatformConfigs);
			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.Android, MobilePlatformConfigs);
			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.NGP, MobilePlatformConfigs);
			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.Flash, MobilePlatformConfigs);

			PlatformConfigs ConsolePlatformConfigs = new PlatformConfigs
			{
				LaunchConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          true },
					{ Profile.Configuration.Debug_32,            true },
					{ Profile.Configuration.Shipping_32,         true },
					{ Profile.Configuration.Test_32,			 true },
				},
				CommandletConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          true },
					{ Profile.Configuration.Release_64,          true },
					{ Profile.Configuration.Debug_32,            true },
					{ Profile.Configuration.Debug_64,            true },
				},
				ScriptConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.ReleaseScript,       true },
					{ Profile.Configuration.DebugScript,         true },
					{ Profile.Configuration.FinalReleaseScript,  true },
				}
			};

			PlatformToConfigMap.Add( ConsoleInterface.PlatformType.Xbox360, ConsolePlatformConfigs );
			PlatformToConfigMap.Add( ConsoleInterface.PlatformType.PS3, ConsolePlatformConfigs );
			PlatformToConfigMap.Add(ConsoleInterface.PlatformType.WiiU, ConsolePlatformConfigs );

			PlatformConfigs PCPlatformConfigs = new PlatformConfigs
			{
				LaunchConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.Release_32,          false },
					{ Profile.Configuration.Release_64,          false },
					{ Profile.Configuration.Debug_32,            false },
					{ Profile.Configuration.Debug_64,            false },
					{ Profile.Configuration.Shipping_32,         true },
					{ Profile.Configuration.Shipping_64,         false },
                    { Profile.Configuration.Test_32,			 false },
					{ Profile.Configuration.Test_64,			 false },
				},
				CommandletConfigs = new ConfigList
				{
					// Config                                    UDK 
					{ Profile.Configuration.Release_32,          false },
					{ Profile.Configuration.Release_64,          false },
					{ Profile.Configuration.Debug_32,            false },
					{ Profile.Configuration.Debug_64,            false },
					{ Profile.Configuration.Shipping_32,         true },
					{ Profile.Configuration.Shipping_64,         false },
				},
				ScriptConfigs = new ConfigList
				{
					// Config                                    UDK
					{ Profile.Configuration.ReleaseScript,       true },
					{ Profile.Configuration.DebugScript,         false },
					{ Profile.Configuration.FinalReleaseScript,  false },
				}
			};

			PlatformToConfigMap.Add( ConsoleInterface.PlatformType.PC, PCPlatformConfigs );
			PlatformToConfigMap.Add( ConsoleInterface.PlatformType.PCConsole, PCPlatformConfigs );
			PlatformToConfigMap.Add( ConsoleInterface.PlatformType.PCServer, PCPlatformConfigs );
		}

		#region Implementation for config tables
		public struct ConfigInfo
		{
			public Profile.Configuration CodeConfig;
			public bool bIsAllowedForUDK;

			public bool AllowedFor(EUDKMode InUDKMode)
			{
				return ((InUDKMode == EUDKMode.None) || (bIsAllowedForUDK && InUDKMode == EUDKMode.UDK) );
			}
		}

		/// A list of configs with a convenient Add method to support the above syntax.
		public class ConfigList : List<ConfigInfo>
		{
			public ConfigList()
				: base()
			{

			}

			public ConfigList(int NumItems)
				: base(NumItems)
			{

			}

			public ConfigList(IEnumerable<ConfigInfo> collection)
				: base(collection)
			{

			}

			public List<Profile.Configuration> GetConfigsFor(EUDKMode InUDKMode)
			{
				List<ConfigInfo> FilteredList = new List<ConfigInfo>(this.Where(SomeConfig => SomeConfig.AllowedFor(InUDKMode)));
				return FilteredList.ConvertAll<Profile.Configuration>(SomeConfig => SomeConfig.CodeConfig);
			}

			public void Add(Profile.Configuration InConfig, bool bIsAllowedForUDK)
			{
				this.Add(new ConfigInfo { CodeConfig = InConfig, bIsAllowedForUDK = bIsAllowedForUDK });
			}
		}

		/// A set of all EXE configuration for a single platform
		public struct PlatformConfigs
		{
			public ConfigList LaunchConfigs;
			public ConfigList CommandletConfigs;
			public ConfigList ScriptConfigs;
		}

		#endregion
	}
}
