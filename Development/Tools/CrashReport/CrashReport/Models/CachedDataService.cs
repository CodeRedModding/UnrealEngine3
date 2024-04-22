// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Web.Caching;

namespace CrashReport.Models
{
	public class CachedDataService
	{
		private ICrashRepository _repository;
		private Cache cache;
		private IBuggRepository _buggRepository;

		private const string CacheKeyPrefix = "__CachedDataService";
		private const string CallstackKeyPrefix = "_CallStack_";
		private const string FunctionCallKeyPrefix = "_functionCallStack_";

		public CachedDataService(Cache cache)
		{
			this.cache = cache;
			_repository = new CrashRepository();
		}
		public CachedDataService(Cache cache, CrashRepository repository)
		{
			this.cache = cache;
			_repository = repository;
		}

		public CachedDataService(Cache cache, BuggRepository repository)
		{
			this.cache = cache;
			_buggRepository = repository;
		}

		public CallStackContainer GetCallStack(Crash Crash)
		{
			string key = CacheKeyPrefix + CallstackKeyPrefix + Crash.Id;
			CallStackContainer CallStack = (CallStackContainer)cache[key];
			if (CallStack == null)
			{
				CallStack  = this._repository.CreateCallStackContainer(Crash);
				cache.Insert(key, CallStack);
			}
			return CallStack;
		}

		public IList<Crash> GetCrashes(string DateFrom, string DateTo)
		{
			string key = CacheKeyPrefix + DateFrom + DateTo;
			IList<Crash> Data = (IList<Crash>)cache[key];
			if (Data == null)
			{
				IQueryable<Crash> DataQuery = _repository.ListAll();
				DataQuery = _repository.FilterByDate(DataQuery, DateFrom, DateTo);
				Data = DataQuery.ToList();
				cache.Insert(key, Data);
			}
			return Data;
		}

		public IList<Bugg> GetBuggs(int Limit)
		{
			string key = CacheKeyPrefix + Limit;
			IList<Bugg> Data = (IList<Bugg>)cache[key];
			if (Data == null)
			{
				IQueryable<Bugg> DataQuery = _buggRepository.ListAll(Limit);
				Data = DataQuery.ToList();
				cache.Insert(key, Data);
			}
			return Data;
		}

		//public IList<string> GetFunctionCallIdsByPattern(String Pattern)

		public IList<string> GetFunctionCalls(string Pattern)
		{
			string key = CacheKeyPrefix + FunctionCallKeyPrefix + Pattern;
			IList<string> FunctionCalls = (IList<string>)cache[key];
			if (FunctionCalls == null)
			{
				var Ids = Pattern.Split(new char[] { '+' });
				IList<int> IdList = new List<int>();
				foreach (var id in Ids)
				{
					int i;
					if (int.TryParse(id, out i))
					{
						IdList.Add(i);
					}
				}
				FunctionCalls = _buggRepository.GetFunctionCalls(IdList);
				cache.Insert(key, FunctionCalls);
			}
			return FunctionCalls;
		}
	}
}