/*=============================================================================
	HitProxies.cpp: Hit proxy implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/** The global list of allocated hit proxies, indexed by hit proxy ID. */
TSparseArray<HHitProxy*> GHitProxies;

FHitProxyId::FHitProxyId(FColor Color)
{
	Index = ((INT)Color.R << 16) | ((INT)Color.G << 8) | ((INT)Color.B << 0);
}

FColor FHitProxyId::GetColor() const
{
	return FColor(
		((Index >> 16) & 0xff),
		((Index >> 8) & 0xff),
		((Index >> 0) & 0xff),
		0
		);
}

HHitProxy::HHitProxy(EHitProxyPriority InPriority):
	Priority(InPriority),
	OrthoPriority(InPriority)
{
	InitHitProxy();
}

HHitProxy::HHitProxy(EHitProxyPriority InPriority, EHitProxyPriority InOrthoPriority):
	Priority(InPriority),
	OrthoPriority(InOrthoPriority)
{
	InitHitProxy();
}

HHitProxy::~HHitProxy()
{
	check(IsInGameThread());

	// Remove this hit proxy from the global array.
	GHitProxies.Remove(Id.Index);
}

void HHitProxy::InitHitProxy()
{
	check(IsInGameThread());

	// Allocate an entry in the global hit proxy array for this hit proxy, and use the index as the hit proxy's ID.
	Id = FHitProxyId(GHitProxies.AddItem(this));
}

UBOOL HHitProxy::IsA(HHitProxyType* TestType) const
{
	UBOOL bIsInstance = FALSE;
	for(HHitProxyType* Type = GetType();Type;Type = Type->GetParent())
	{
		if(Type == TestType)
		{
			bIsInstance = TRUE;
			break;
		}
	}
	return bIsInstance;
}

HHitProxy* GetHitProxyById(FHitProxyId Id)
{
	if(Id.Index >= 0 && Id.Index < GHitProxies.GetMaxIndex() && GHitProxies.IsAllocated(Id.Index))
	{
		return GHitProxies(Id.Index);
	}
	else
	{
		return NULL;
	}
}
