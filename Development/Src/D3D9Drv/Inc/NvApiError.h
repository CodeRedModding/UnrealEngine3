/*=============================================================================
This code taken with minimal modifications from PCCompat2007.6.29.4.
=============================================================================*/

#ifndef __NVAPIERROR_H__
#define __NVAPIERROR_H__

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef NVAPIENTRY
#   define NVAPIENTRY APIENTRY
#endif//NVAPIENTRY

#define NVRESULT_SUCCESS(nr)    ((int)(nr) <= 0) // Success (NV_OK or NV_WARN_xxx)
#define NVRESULT_FAILURE(nr)    ((int)(nr) >  0) // Failure
#define NVRESULT_WARN(nr)       ((int)(nr) <  0) // Warning

#define NVRESULT_ERRORCODE(nr)  ((NVRESULT)(abs((int)nr))) // Extract error code from NVRESULT value (warning --> error)

typedef enum NVRESULT
{
    NV_OK                   =   0,  // Success.
//----------------------------------------------------------------------------
    NV_INTERNALERROR        =   1,  // Internal error.
    NV_ALREADYINITIALIZED   =   2,  // Already initialized.
    NV_NOTINITIALIZED       =   3,  // Not initialized.
    NV_OUTOFMEMORY          =   4,  // Not enough memory for operation.
    NV_NOTSUPPORTED         =   5,  // Feature not supported.
    NV_NOTAVAILABLE         =   6,  // Feature not presently available.
    NV_NOTIMPLEMENTED       =   7,  // Feature not implemented.
    NV_BADPARAMETER         =   8,  // Invalid parameter.
    NV_ACCESSDENIED         =   9,  // Access denied.
    NV_RUNNING              =  10,  // Operation requires inactive environment.
    NV_NOTRUNNING           =  11,  // Operation requires active environment.
    NV_FILENOTFOUND         =  12,  // Unable to locate file.
    NV_NOMORE               =  13,  // No more items.
    NV_ILLEGALSTATE         =  14,  // Illegal state could not be resolved.
    NV_NOTFOUND             =  15,  // Not found
    NV_ALLOCATED            =  16,  // Indicates that the request cannot be completed as the resource is already allocated/deallocated
    NV_BADPARAMETERSIZE     =  17,  // As mentioned by Satish, we require to give details of what bad parameter is
//----------------------------------------------------------------------------
    NV_WARN_INTERNALERROR   =  -1,  // Internal warning.
    NV_WARN_ILLEGALSTATE    = -14,  // Illegal state was automatically resolved.
    NV_WARN_NOTEQUAL        = -15,  // State compare failed
    NV_WARN_NOMORE          = -16,  // Warning that state compare failed and there are no more to enum
    NV_WARN_ALREADYINITIALIZED = -17,//Warning that we are already initialized
    NV_WARN_ALLOCATED       = -18,  // Warning that this state is already allocation/deallocated
    NV_WARN_NOTINITIALIZED  = -19,  // Warning that the api is not initialized
} NVRESULT;

NVRESULT NVAPIENTRY NvGetLastError();
typedef NVRESULT (NVAPIENTRY* fNvGetLastError)();

LPCWSTR NVAPIENTRY NvGetLastErrorMessageW();
LPCSTR NVAPIENTRY NvGetLastErrorMessageA();
typedef LPCWSTR (NVAPIENTRY* fNvGetLastErrorMessageW)();
typedef LPCSTR (NVAPIENTRY* fNvGetLastErrorMessageA)();

LPCWSTR NVAPIENTRY NvGetErrorMessageW(NVRESULT nr);
LPCSTR NVAPIENTRY NvGetErrorMessageA(NVRESULT nr);
typedef LPCWSTR (NVAPIENTRY* fNvGetErrorMessageW)(NVRESULT nr);
typedef LPCSTR (NVAPIENTRY* fNvGetErrorMessageA)(NVRESULT nr);

#ifdef  UNICODE
#   define NvGetLastErrorMessage NvGetLastErrorMessageW
#   define NvGetErrorMessage NvGetLastMessageW
#else
#   define NvGetLastErrorMessage NvGetLastErrorMessageA
#   define NvGetErrorMessage NvGetLastMessageA
#endif//UNICODE

#ifdef __cplusplus
} //extern "C" {
#endif

#endif//__NVAPIERROR_H__