#pragma once

#include "mathlib.h"

class CGAInstance_Test
{
public:
	CGAInstance_Test()
	{
	}

	void SetRandomInitalState()
	{
		a = frand(-1,1);
		b = frand(0,1);
		c = frand(0,100);
	}

	float *GetDataAccess( DWORD &OutCount )
	{
		OutCount = sizeof(*this)/sizeof(float);

		return (float *)this;
	}

	void Renormalize()
	{
	}

	void Debug()
	{
		char str[256];

		sprintf_s(str,sizeof(str),"%f %f %f\n",a,b,c);
		OutputDebugString(str);
	}

	// smaller means better
	float ComputeFitnessValue() const
	{
		float x = (a-3)*(b-0.5f)+c;

		return x*x;
	}

private: // -------------------------------------

	// -1..1
	float		a;
	// 0..1
	float		b;
	// 0..100
	float		c;
};
