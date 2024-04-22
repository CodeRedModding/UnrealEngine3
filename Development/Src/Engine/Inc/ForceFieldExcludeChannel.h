/*=============================================================================
ForceFieldExcludeChannel.h
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef FORCE_FIELD_EXCLUDE_CHANNEL_H
#define FORCE_FIELD_EXCLUDE_CHANNEL_H

#if WITH_NOVODEX

struct ForceFieldExcludeChannel
{
	TMap<class ARB_ForceFieldExcludeVolume*,UserForceFieldShapeGroup*> Groups;
};

#endif // WITH_NOVODEX

#endif //FORCE_FIELD_EXCLUDE_CHANNEL_H
