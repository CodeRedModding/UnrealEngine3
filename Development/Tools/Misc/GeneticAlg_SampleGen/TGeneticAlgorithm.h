/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

#include <assert.h>
#include <vector>
#include <algorithm>
#include <limits>

// iteratively optimize a set of individual objects for the smallest fitness value, based on genetic algorithm theory
// each object needs to expose it's properties as an array of floats, a SetRandomInitalState() method is used to initialize
// and to mutate
//
template <class T>
class TGeneticAlgorithm
{
	template <class T>
	class _TWithFitnessValue :public T
	{
	public:
		float	CachedFitness;

		bool operator<(_TWithFitnessValue &rhs ) const
		{
			return CachedFitness<rhs.CachedFitness; 
		}
	};

	typedef _TWithFitnessValue<T> TWithFitnessValue;

public:

	// _MutationRate: 0=no mutation, 1=maximum mutation
	TGeneticAlgorithm( const DWORD _IndividualCount, const DWORD _ChildenCountPerStep, const float _MutationRate ) 
		:IndividualCount(_IndividualCount), 
		ChildenCountPerStep(_ChildenCountPerStep),
		MutationRate(_MutationRate),
		bBetterOneFound(false)
	{
		assert(IndividualCount>2);		// at least one of each: father, mother, child
		assert(ChildenCountPerStep<=IndividualCount-2);

		BestIndividual.CachedFitness = FLT_MAX;
		Individuals.resize(IndividualCount);

		std::vector<TWithFitnessValue>::iterator it, end=Individuals.end();

		for(it=Individuals.begin();it!=end;++it)
		{
			float &refFitness = it->CachedFitness;
			T& ref = (T &)(*it);

			ref.SetRandomInitalState();

			refFitness = ref.ComputeFitnessValue();

			UpdateBest(*it);
		} 
	}

	// ChildrenCount 0..IndividualCount-2
	// return Better one found
	bool Step( const bool bExtensiveDebug )
	{
		bBetterOneFound=false;

		std::sort(Individuals.begin(),Individuals.end());		// first good parents, then children

		Debug(bExtensiveDebug);

		DWORD dwParentCount = IndividualCount-ChildenCountPerStep;

		std::vector<TWithFitnessValue>::iterator it;
		std::vector<TWithFitnessValue>::iterator itChildrenStart = Individuals.begin() + dwParentCount;
		std::vector<TWithFitnessValue>::iterator end = Individuals.end();

		for(it = itChildrenStart; it != end; ++it)
		{
			float &refFitness = it->CachedFitness;
			T& ref = (T &)(*it);

			DWORD IndexFather = rand() %(dwParentCount);
			TWithFitnessValue &refFather = Individuals[IndexFather];

			DWORD IndexMother = rand() % (dwParentCount-1);

			if(IndexMother==IndexFather)
				IndexMother = dwParentCount-1;

			TWithFitnessValue &refMother = Individuals[IndexMother];

			assert(&refMother != &refFather);

			// create a new child from father and mother
			{
				DWORD DataCountChild;
				float *pDataChild = ref.GetDataAccess(DataCountChild);

				DWORD DataCountFather;
				float *pDataFather = refFather.GetDataAccess(DataCountFather);

				DWORD DataCountMother;
				float *pDataMother = refMother.GetDataAccess(DataCountMother);

				assert(DataCountChild == DataCountFather);
				assert(DataCountChild == DataCountMother);
				assert(DataCountChild >= 1);

				DWORD CrossOverPos = rand() % (DataCountChild-1) + 1;

				for(DWORD i=0; i < CrossOverPos; ++i)
				{
					*pDataChild++ = *pDataFather++; 
				}

				pDataMother += CrossOverPos;
				for(DWORD i=CrossOverPos; i < DataCountChild; ++i)
				{
					*pDataChild++ = *pDataMother++; 
				}

				ref.Renormalize();

				// Update Fitness
				refFitness = ref.ComputeFitnessValue();
				UpdateBest(*it);
/*
				char str[256];

				sprintf_s(str,sizeof(str),"new ID%d and ID%d at POS%d = %f: ",IndexFather,IndexMother,CrossOverPos,refFitness);
				OutputDebugString(str);
				ref.Debug();
*/
			}

			// mutate father, can be optimized
			{
				T Mutant;

				Mutant.SetRandomInitalState();

				DWORD DataCountMutant;
				float *pDataMutant = Mutant.GetDataAccess(DataCountMutant);

				DWORD DataCountFather;
				float *pDataFather = refFather.GetDataAccess(DataCountFather);

				assert(DataCountMutant == DataCountFather);

				DWORD GenePos = rand() % DataCountMutant;
 
				pDataMutant += GenePos;
				pDataFather += GenePos;

				*pDataFather = *pDataFather + (*pDataMutant - *pDataFather) * MutationRate;

				refFather.Renormalize();

				float NewFatherCachedFitness = refFather.ComputeFitnessValue();

				if(refFather.CachedFitness == BestIndividual.CachedFitness
				&& NewFatherCachedFitness > refFather.CachedFitness )
				{
					// father was the best - we should keep him, unless the new Fitness is better
					refFather = BestIndividual;
				}
				else
				{
					refFather.CachedFitness = NewFatherCachedFitness;
/*
					char str[256];

					sprintf_s(str,sizeof(str),"mutate ID%d POS%d = %f: ",IndexFather,GenePos,refFather.CachedFitness);
					OutputDebugString(str);
					refFather.Debug();
				*/
					UpdateBest(refFather);
				}
			}
		}

//		OutputDebugString("---------------------------------------------------\n");

		return bBetterOneFound;
	}

	// FitnessPos 0..SumFitness
	TWithFitnessValue &FindIndividual( const float FitnessPos, TWithFitnessValue *pExclude = 0 )
	{
		float Pos = 0;
		TWithFitnessValue *pRet = &Individuals.front();
		std::vector<TWithFitnessValue>::iterator it, end = Individuals.end();

		for(it = Individuals.begin(); it != end; ++it)
		{
			T& ref = (T &)(*it);

			if(&ref == pExclude)
				continue;

			float &refFitness = it->CachedFitness;

			Pos += refFitness;

			pRet = &(*it);

			if(Pos>FitnessPos)
				break;
		}

		return *pRet;
	}

	const T &GetBest() const
	{
		return BestIndividual;
	}

	float GetBestFitnessValue() const
	{
		return BestIndividual.CachedFitness;
	}

	void UpdateBestFitnessValue()
	{
		BestIndividual.CachedFitness = BestIndividual.ComputeFitnessValue();
	}

private: // --------------------------------------------------------------------

	void Debug( const bool bExtensiveDebug )
	{
		TWithFitnessValue *pBest = &Individuals.front();
		DWORD i = 0;
		std::vector<TWithFitnessValue>::iterator it, end = Individuals.end();

		for(it = Individuals.begin(); it != end; ++it, ++i)
		{
			float &refFitness = it->CachedFitness;

			if(bExtensiveDebug)
			{
				T& ref = (T &)(*it);
				char str[256];

				sprintf_s(str, sizeof(str), "ID%d %f: ", i, refFitness);
				OutputDebugString(str);

				ref.Debug();
			}

			if(refFitness<pBest->CachedFitness)
			{
				pBest = &(*it);
			}
		}
/*
		char str[256];

		sprintf_s(str, sizeof(str), "Best now %f ever %f:", pBest->CachedFitness, BestIndividual.CachedFitness);
		OutputDebugString(str);
		OutputDebugString("\n");
		*/

	}

	void UpdateBest( const TWithFitnessValue & Other )
	{
		if(Other.CachedFitness<BestIndividual.CachedFitness)
		{
			BestIndividual = Other;
			bBetterOneFound = true;
		}
	}

// ------------------------------------------------------------------------------

	//
	TWithFitnessValue							BestIndividual;
	// [0..IndividualCount-1]
	std::vector<TWithFitnessValue>				Individuals;
	// >=3
	DWORD										IndividualCount;
	// >0, <IndividualCount-2
	DWORD										ChildenCountPerStep;
	// 0=no mutation, 1=maximum mutation
	float										MutationRate;

	bool										bBetterOneFound;
};
