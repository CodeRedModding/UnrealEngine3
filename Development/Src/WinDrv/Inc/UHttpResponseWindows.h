// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#if WITH_UE3_NETWORKING
// private data that the native implementation needs
private: friend class FHttpTickerWindows;
private: void AssignResponsePointer(FHttpResponseWinInet* ResponseImpl);
private: virtual void BeginDestroy();
#endif
