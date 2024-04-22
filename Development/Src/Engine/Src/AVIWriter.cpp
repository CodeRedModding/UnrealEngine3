/*=============================================================================
	AVIWriter.cpp: AVI creation implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "AVIWriter.h"

#if _WINDOWS

#pragma warning(disable : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(disable : 4264) // 'virtual_function' : no override available for virtual member function from base 'cla
#pragma pack(push,8)
#include <streams.h>
#pragma pack(pop)
#pragma warning(default : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(default : 4264) // 'virtual_function' : no override available for virtual member function from base 'cla

#include <dshow.h>
#include <initguid.h>

#include "CaptureSource.h"

#define SAFE_RELEASE(x) { if (x) x->Release(); x = NULL; }

#define g_wszCapture    L"Capture Filter"

/** An event for synchronizing direct show encoding with the game thread */
FEvent* GCaptureSyncEvent = NULL;

// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
	&MEDIATYPE_Video,       // Major type
	&MEDIASUBTYPE_NULL      // Minor type
};

const AMOVIESETUP_PIN sudOutputPinDesktop = 
{
	L"Output",      // Obsolete, not used.
	FALSE,          // Is this pin rendered?
	TRUE,           // Is it an output pin?
	FALSE,          // Can the filter create zero instances?
	FALSE,          // Does the filter create multiple instances?
	&CLSID_NULL,    // Obsolete.
	NULL,           // Obsolete.
	1,              // Number of media types.
	&sudOpPinTypes  // Pointer to media types.
};

const AMOVIESETUP_FILTER sudPushSourceDesktop =
{
	&CLSID_CaptureSource,	// Filter CLSID
	g_wszCapture,			// String name
	MERIT_DO_NOT_USE,       // Filter merit
	1,                      // Number pins
	&sudOutputPinDesktop    // Pin details
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[1] = 
{
	{ 
		g_wszCapture,					// Name
		&CLSID_CaptureSource,			// CLSID
		FCaptureSource::CreateInstance, // Method to create an instance of MyComponent
		NULL,                           // Initialization function
		&sudPushSourceDesktop           // Set-up information (for filters)
	},
};

INT g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);    

/**
 * Windows implementation relying on DirectShow.
 */
class FAVIWriterWin : public FAVIWriter
{
public:
	FAVIWriterWin( ) 
		: Graph(NULL),
		Control(NULL),
		Capture(NULL),
		CapturePin(NULL),
		CaptionSource(NULL)
	{

	};

public:

	void StartCapture(FViewport* Viewport /*= NULL*/)
	{
		GCaptureSyncEvent = GSynchronizeFactory->CreateSynchEvent();
		if (!bCapturing)
		{

			FrameNumber = 0;
			MovieCaptureIndex = 0;
			if (!Viewport)
			{
				Viewport = GEngine->GetAViewport();
				if (!Viewport)
				{
					debugf( NAME_Error, TEXT( "ERROR - Could not get a valid viewport for capture!" ));
					return;
				}
			}
			CaptureViewport = Viewport;
			BufferSize = CaptureViewport->GetSizeX() * CaptureViewport->GetSizeY() * sizeof( FColor );
			Buffer.Empty();
			Buffer.Add(BufferSize);

			// Initialize the COM library.
			HRESULT hr = CoInitialize(NULL);
			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not initialize COM library!" ));
				return;
			}

			// Create the filter graph manager and query for interfaces.
			hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&Graph);
			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not create the Filter Graph Manager!" ));
				return;
			}

			// Create the capture graph builder
			hr = CoCreateInstance (CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, (void **) &Capture);
			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not create the Capture Graph Manager!" ));
				return;
			}

			// Specify a filter graph for the capture graph builder to use
			hr = Capture->SetFiltergraph(Graph);
			if (FAILED(hr))
			{
				debugf( NAME_Error, TEXT( "ERROR - Failed to set capture filter graph!" ));
				return;
			}

			CaptionSource = new FCaptureSource(NULL, &hr);
			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not create CaptureSource filter!" ));
				Graph->Release();
				CoUninitialize(); 
				return;
			}
			CaptionSource->AddRef();

			hr = Graph->AddFilter(CaptionSource, L"PushSource"); 
			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not add CaptureSource filter!" ));
				CaptionSource->Release();
				Graph->Release();
				CoUninitialize(); 
				return;
			}

			IAMVideoCompression* VideoCompression = NULL;
			if (GEngine->bCompressMatineeCapture)
			{
				// Create the filter graph manager and query for interfaces.
				hr = CoCreateInstance (CLSID_MJPGEnc, NULL, CLSCTX_INPROC, IID_IBaseFilter, ( void ** ) &VideoCompression);
				if (FAILED(hr)) 
				{
					debugf( NAME_Error, TEXT( "ERROR - Could not create the video compression!" ));
					return;
				}

				hr = Graph->AddFilter( (IBaseFilter *)VideoCompression, TEXT("Compressor") );

				if (FAILED(hr)) 
				{
					debugf( NAME_Error, TEXT( "ERROR - Could not add filter!" ));
					CaptionSource->Release();
					Graph->Release();
					CoUninitialize(); 
					return;
				}	
			}

			IPin* temp = NULL;
			hr = CaptionSource->FindPin(L"1", &temp);
			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not find pin of filter CaptureSource!" ));
				CaptionSource->Release();
				Graph->Release();
				CoUninitialize(); 
				return;
			}
			CapturePin = (FCapturePin*)temp;

			// Attempt to make the dir if it doesn't exist.
			FString OutputDir = appGameDir() + TEXT("MovieCaptures");
			GFileManager->MakeDirectory(*OutputDir, TRUE);

			TCHAR File[MAX_SPRINTF] = TEXT("");
			if (GEngine->bStartWithMatineeCapture)
			{
				UBOOL bFoundUnusedName = FALSE;
				while (!bFoundUnusedName)
				{
					const FString FileNameInfo = FString::Printf( TEXT("_%dfps_%dx%d"), 
						GEngine->MatineeCaptureFPS,  CaptureViewport->GetSizeX(), CaptureViewport->GetSizeY() );
					if (GEngine->bCompressMatineeCapture)
					{
						appSprintf( File, TEXT("%s_%d.avi"), *(OutputDir + PATH_SEPARATOR + GEngine->MatineePackageCaptureName + "_" + GEngine->MatineeCaptureName + FileNameInfo + "_compressed"), MovieCaptureIndex );
					}
					else
					{
						appSprintf( File, TEXT("%s_%d.avi"), *(OutputDir + PATH_SEPARATOR + GEngine->MatineePackageCaptureName + "_" + GEngine->MatineeCaptureName + FileNameInfo), MovieCaptureIndex );
					}
					if (GFileManager->FileSize(File) != -1)
					{
						MovieCaptureIndex++;
					}
					else
					{
						bFoundUnusedName = TRUE;
					}
				}
			}
			else
			{
				UBOOL bFoundUnusedName = FALSE;
				while (!bFoundUnusedName)
				{
					FString FileName = FString::Printf( TEXT("Movie%i.avi"), MovieCaptureIndex );
					appSprintf( File, TEXT("%s"), *(OutputDir + PATH_SEPARATOR + FileName) );
					if (GFileManager->FileSize(File) != -1)
					{
						MovieCaptureIndex++;
					}
					else
					{
						bFoundUnusedName = TRUE;
					}
				}
			}

			IBaseFilter *pMux;
			hr = Capture->SetOutputFileName(
				&MEDIASUBTYPE_Avi,  // Specifies AVI for the target file.
				File,				// File name.
				&pMux,              // Receives a pointer to the mux.
				NULL);              // (Optional) Receives a pointer to the file sink.

			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Failed to set capture filter graph output name!" ));
				CaptionSource->Release();
				Graph->Release();
				CoUninitialize(); 
				return;
			}

			hr = Capture->RenderStream(
				NULL,								// Pin category.
				&MEDIATYPE_Video,					// Media type.
				CaptionSource,					// Capture filter.
				(IBaseFilter *)VideoCompression,	// Intermediate filter (optional).
				pMux);								// Mux or file sink filter.


			if (FAILED(hr)) 
			{
				debugf( NAME_Error, TEXT( "ERROR - Could not start capture!" ));
				CaptionSource->Release();
				Graph->Release();
				CoUninitialize(); 
				return;
			}
			// Release the mux filter.
			pMux->Release();

			SAFE_RELEASE(VideoCompression);
			
			Graph->QueryInterface(IID_IMediaControl, (void **)&Control);

			MovieCaptureIndex++;
			bReadyForCapture = TRUE;
		}
	}

	void StopCapture()
	{
		if (bCapturing)
		{
			bReadyForCapture = FALSE;
			bCapturing = FALSE;
			bMatineeFinished = FALSE;
			Control->Stop();
			CaptureViewport = NULL;
			SAFE_RELEASE(CapturePin);
			SAFE_RELEASE(CaptionSource);
			SAFE_RELEASE(Capture);
			SAFE_RELEASE(Control);
			SAFE_RELEASE(Graph);
			CoUninitialize();
			GSynchronizeFactory->Destroy( GCaptureSyncEvent );
			GCaptureSyncEvent = NULL;
		}
	}

	void Close()
	{
		StopCapture();
	}

	void Update()
	{
		if (GIsRequestingExit)
		{
			return;
		}

		if (bMatineeFinished)
		{
			if (GEngine->MatineeCaptureType == 0)
			{
				if (CaptureViewport)
				{
					CaptureViewport->GetClient()->CloseRequested(CaptureViewport);
				}
			}
			else
			{
				FViewport* ViewportUsed = GEngine->GetAViewport();
				if (ViewportUsed)
				{
					ViewportUsed->GetClient()->CloseRequested(ViewportUsed);
				}
			}
			StopCapture();
		}
		else if (bCapturing)
		{
			// Wait for the directshow thread to finish encoding the last data
			GCaptureSyncEvent->Wait();
			CaptureViewport->ReadPixels(( BYTE* )&Buffer( 0 ));
			// Allow the directshow thread to encode more data
			GCaptureSyncEvent->Trigger();

			debugf(NAME_DevMovieCapture, TEXT("-----------------START------------------"));
			debugf(NAME_DevMovieCapture, TEXT(" INCREASE FrameNumber from %d "), FrameNumber);
			FrameNumber++;
			
		
			
		}
		else if (bReadyForCapture)
		{
			CaptureViewport->ReadPixels(( BYTE* )&Buffer( 0 ));
			// Allow the directshow thread to process the pixels we just read
			GCaptureSyncEvent->Trigger();
			Control->Run();
			bReadyForCapture = FALSE;
			bCapturing = TRUE;
			debugf(NAME_DevMovieCapture, TEXT("-----------------START------------------"));
			debugf(NAME_DevMovieCapture, TEXT(" INCREASE FrameNumber from %d "), FrameNumber);
			FrameNumber++;
			
		}
	}
private:
	IGraphBuilder* Graph;
	IMediaControl* Control;
	ICaptureGraphBuilder2* Capture;
	FCapturePin* CapturePin;
	IBaseFilter* CaptionSource;
};
#endif


FAVIWriter* FAVIWriter::GetInstance()
{
#if _WINDOWS
	static FAVIWriterWin Instance;
	return &Instance;
#else
	return NULL;
#endif
}
