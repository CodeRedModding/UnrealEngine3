//! @file SubstanceAirUncopyable.h

#ifndef _SUBSTANCE_AIR_INTEGRATION_UNCOPYABLE_H
#define _SUBSTANCE_AIR_INTEGRATION_UNCOPYABLE_H

class Uncopyable
{
protected:
	Uncopyable() {}
	~Uncopyable() {}

private:
	Uncopyable(const Uncopyable &);
	Uncopyable & operator=(const Uncopyable &);
};

#endif _SUBSTANCE_AIR_INTEGRATION_UNCOPYABLE_H
