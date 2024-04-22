/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;

namespace UnrealBuildTool
{
	/**
	 * Caches include dependency information to speed up preprocessing on subsequent runs.
	 */
	[Serializable]
	class DependencyCache
	{
		/** The time the cache was created. Used to invalidate entries. */
		public DateTimeOffset CacheCreateDate;

		/** The time the cache was last updated. Stored as the creation date when saved. */
		[NonSerialized]
		private DateTimeOffset CacheUpdateDate;

		/** Path to store the cache data to. */
		[NonSerialized]
		private string CachePath;

		/** Dependency lists, keyed on uppercase invariant absolute path. */
		private Dictionary<string, List<string>> DependencyMap;

		/** Whether the dependency cache is dirty and needs to be saved. */
		[NonSerialized]
		private bool bIsDirty;

		/**
		 * Creates and deserializes the dependency cache at the passed in location
		 * 
		 * @param	CachePath	Name of the cache file to deserialize
		 */
		public static DependencyCache Create(string CachePath)
		{
			// See whether the cache file exists.
			FileItem Cache = FileItem.GetItemByPath(CachePath);
			if (Cache.bExists)
			{
				// Deserialize cache from disk if there is one.
				DependencyCache Result = Load(Cache);
				if (Result != null)
				{
					// Successfully serialize, create the transient variables and return cache.
					Result.CachePath = CachePath;
                    Result.CacheUpdateDate = DateTimeOffset.Now;
					return Result;
				}
			}
			// Fall back to a clean cache on error or non-existance.
			return new DependencyCache(Cache);
		}

		/**
		 * Loads the cache from the passed in file.
		 * 
		 * @param	Cache	File to deserialize from
		 */
		public static DependencyCache Load(FileItem Cache)
		{
			DependencyCache Result = null;
			try
			{
				using (FileStream Stream = new FileStream(Cache.AbsolutePath, FileMode.Open, FileAccess.Read))
				{
					BinaryFormatter Formatter = new BinaryFormatter();
					Result = Formatter.Deserialize(Stream) as DependencyCache;
				}
			}
			catch (Exception Ex)
			{
                // Don't bother logging this expected error.
                // It's due to a change in the CacheCreateDate type.
                if (Ex.Message != "Object of type 'System.DateTime' cannot be converted to type 'System.DateTimeOffset'")
                {
                    Console.Error.WriteLine("Failed to read dependency cache: {0}", Ex.Message);
                }
			}
			return Result;
		}

		/**
		 * Constructor
		 * 
		 * @param	Cache	File associated with this cache
		 */
		protected DependencyCache(FileItem Cache)
		{
            CacheCreateDate = DateTimeOffset.Now;
            CacheUpdateDate = DateTimeOffset.Now;
			CachePath = Cache.AbsolutePath;
			DependencyMap = new Dictionary<string, List<string>>();
			bIsDirty = false;
		}

		/**
		 * Saves the dependency cache to disk using the update time as the creation time.
		 */
		public void Save()
		{
			// Only save if we've made changes to it since load.
			if( bIsDirty )
			{
				// Save update date as new creation date.
                CacheCreateDate = CacheUpdateDate;

				// Serialize the cache to disk.
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(CachePath));
					using (FileStream Stream = new FileStream(CachePath, FileMode.Create, FileAccess.Write))
					{
						BinaryFormatter Formatter = new BinaryFormatter();
						Formatter.Serialize(Stream, this);
					}
				}
				catch (Exception Ex)
				{
					Console.Error.WriteLine("Failed to write dependency cache: {0}", Ex.Message);
				}
			}
		}

		/**
		 * Returns the dependencies of the specified FileItem if it exists in the cache and if the file 
		 * and each of its (recursive) dependencies has a last write time before the creation time of 
		 * the cache. 
		 * 
		 * The cache holds a flattened full dependency graph so we don't need to recurse as it already 
		 * contains the dependencies of all dependencies. 
		 * 
		 * The code also keeps track of whether dependencies have been successfully accessed for a given 
		 * file.
		 * 
		 * @param	File			File to try to find dependencies in cache
		 * @param	Result	[out]	List of dependencies if successful, null otherwise
		 */
		public bool TryFetchDependencies(FileItem File, out List<FileItem> Result)
		{
			Result = null;
			List<string> DependencyPaths = null;

			// Check whether File is in cache.
			if (DependencyMap.TryGetValue(File.AbsolutePathUpperInvariant, out DependencyPaths))
			{
				// File is in cache, now check whether last write time is prior to cache creation time.
                if (File.LastWriteTime < CacheCreateDate)
				{
					// Cached version is up to date, now check its dependencies. We don't need to recurse here
					// the list of dependencies already is a flattened graph.
					List<FileItem> Dependencies = new List<FileItem>();
					foreach (string Path in DependencyPaths)
					{
						FileItem Dependency = FileItem.GetItemByPath(Path);
						// Check whether dependency's last write time is prior to cache creation time.
                        if (Dependency.LastWriteTime < CacheCreateDate)
						{
							// It is, add to list of dependencies.
							Dependencies.Add(Dependency);
						}
						// Outdated/ stale dependency.
						else
						{
							// Remove entry from cache as it's stale.
							DependencyMap.Remove(File.AbsolutePathUpperInvariant);
							return false;
						}
					}

					// Use assembled list as result as it contains full recursive (flattened) dependency tree.
					Result = Dependencies;
					return true;
				}
				// File has been modified since the last time it was cached.
				else
				{
					// Remove entry from cache as it's stale.
					DependencyMap.Remove(File.AbsolutePathUpperInvariant);
					return false;
				}
			}
			// Not in cache.
			else
			{
				return false;
			}
		}

		/**
		 * Returns the direct dependencies of the specified FileItem if it exists in the cache and if the
		 * file has a last write time before the creation time of the cache. 
		 * 
		 * The code also keeps track of whether dependencies have been successfully accessed for a given
		 * file.
		 * 
		 * @param	File			File to try to find dependencies in cache
		 * @param	Result	[out]	List of dependencies if successful, null otherwise
		 */
		public bool TryFetchDirectDependencies(FileItem File, out List<string> Result)
		{
			Result = null;
			List<string> DependencyPaths = null;

			// Check whether File is in cache.
			if (DependencyMap.TryGetValue(File.AbsolutePathUpperInvariant, out DependencyPaths))
			{
				// File is in cache, now check whether last write time is prior to cache creation time.
                if (File.LastWriteTime < CacheCreateDate)
				{
					// Cached version is up to date, return it.
					Result = DependencyPaths;
					return true;
				}
				// File has been modified since the last time it was cached.
				else
				{
					// Remove entry from cache as it's stale.
					DependencyMap.Remove(File.AbsolutePathUpperInvariant);
					return false;
				}
			}
			// Not in cache.
			else
			{
				return false;
			}
		}

		/**
		 * Update cache with dependencies for the passed in file.
		 * 
		 * @param	File			File to update dependencies for
		 * @param	Dependencies	List of dependencies to cache for passed in file
		 */
		public void Update(FileItem File, List<FileItem> Dependencies)
		{
			List<string> DependencyStrings = new List<string>();
			foreach (FileItem Item in Dependencies)
			{
				// Explicitly don't add value in upper invariant as we use it later to create FileItems. The potential
				// mangline of case can cause needless recompilation due to command line diffing.
				DependencyStrings.Add(Item.AbsolutePath);
			}
			Update(File,DependencyStrings);
		}

		/**
		 * Update cache with dependencies for the passed in file.
		 * 
		 * @param	File			File to update dependencies for
		 * @param	Dependencies	List of dependencies to cache for passed in file
		 */
		public void Update(FileItem File, List<string> Dependencies)
		{
			DependencyMap[File.AbsolutePathUpperInvariant] = Dependencies;
			// Mark cache as dirty for saving.
			bIsDirty = true;
		}
	}
}
