/**********************************************************************

Filename    :   ScaleformAllocator.h
Content     :   Implements FGFxAllocator (appMalloc wrapped allocator)

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#ifndef ScaleformAllocator_h
#define ScaleformAllocator_h

#if WITH_GFx

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_SysAlloc.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#include "ScaleformStats.h"

class FGFxAllocator : public SysAllocBase_SingletonSupport<FGFxAllocator, SysAllocPaged>
{
	public:
		unsigned m_TotalAlloc, m_FrameAllocPeak;

		inline void StartFrame()
		{
			m_FrameAllocPeak = m_TotalAlloc;
			SET_DWORD_STAT ( STAT_GFxFramePeakMem, m_FrameAllocPeak );
		}

		virtual void* Alloc ( UPInt size, UPInt align )
		{
			void* pMem = appMalloc ( size );

			if ( pMem )
			{
				m_TotalAlloc += size;

				if ( m_TotalAlloc > m_FrameAllocPeak )
				{
					m_FrameAllocPeak = m_TotalAlloc;
					SET_DWORD_STAT ( STAT_GFxFramePeakMem, m_FrameAllocPeak );
				}
			}

			SET_DWORD_STAT ( STAT_GFxInternalMem, m_TotalAlloc );

			return pMem;
		}

		virtual bool Free ( void* pmemBlock, UPInt size, UPInt align )
		{
			if ( pmemBlock != NULL )
			{
				m_TotalAlloc -= size;
				appFree ( pmemBlock );
			}

			SET_DWORD_STAT ( STAT_GFxInternalMem, m_TotalAlloc );

			return 1;
		}

		void GetInfo ( Info* i ) const
		{
			i->MinAlign    = 1;
			i->MaxAlign    = 1;
			i->Granularity = 64 * 1024;
			i->HasRealloc  = false;
		}

		virtual UPInt   GetFootprint() const
		{
			return m_TotalAlloc;
		}
		virtual UPInt   GetUsedSpace() const
		{
			return m_TotalAlloc;
		}
};

#endif // WITH_GFx

#endif // ScaleformAllocator_h
