/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/


/*********************************************************************NVMH4****

File:  NvCpl.h

Copyright NVIDIA Corporation 2005
TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED
*AS IS* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS
OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS
BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES
WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS,
BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS)
ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS
BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

******************************************************************************/

#ifndef NV_CPL_H
#define NV_CPL_H

#include <windows.h>
#include "NvPanelApi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display quality
#define NV_DISPLAY_DIGITAL_VIBRANCE_MIN 0
#define NV_DISPLAY_DIGITAL_VIBRANCE_MAX 63
#define NV_DISPLAY_BRIGHTNESS_MIN -125
#define NV_DISPLAY_BRIGHTNESS_MAX 125
#define NV_DISPLAY_CONTRAST_MIN -82
#define NV_DISPLAY_CONTRAST_MAX 82
#define NV_DISPLAY_GAMMA_MIN 0.5
#define NV_DISPLAY_GAMMA_MAX 6

// Gamma ramp types
typedef struct _GAMMARAMP {
	WORD Red[256];
	WORD Green[256];
	WORD Blue[256];
} GAMMARAMP, *PGAMMARAMP;

// Function prototype for NvCplGetDataIntType
typedef BOOL (*NvCplGetDataIntType)(long, long*);
typedef BOOL (*NvCplSetDataIntType)(long, long);

#ifdef __cplusplus
}
#endif

#endif /* NV_CPL_H */
