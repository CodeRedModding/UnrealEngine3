//--------------------------------------------------------------------------------------
// File: ue3stereo.h
// Authors: John McDonald
// Email: devsupport@nvidia.com
//
// Utility classes for stereo
//
// Copyright (c) 2009 NVIDIA Corporation. All rights reserved.
//
// NOTE: This file is provided as-is, with no warranty either expressed or implied.
//--------------------------------------------------------------------------------------

#ifndef __UE3STEREO__
#define __UE3STEREO__ 1

#if (!CONSOLE && !PLATFORM_MACOSX)

#include "nvapi.h"

namespace nv {
    namespace stereo {
        typedef struct  _Nv_Stereo_Image_Header
        {
            unsigned int    dwSignature;
            unsigned int    dwWidth;
            unsigned int    dwHeight;
            unsigned int    dwBPP;
            unsigned int    dwFlags;
        } NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;

        #define     NVSTEREO_IMAGE_SIGNATURE 0x4433564e //NV3D
        #define     NVSTEREO_SWAP_EYES 0x00000001

        inline void PopulateTextureData(float* leftEye, float* rightEye, LPNVSTEREOIMAGEHEADER header, unsigned int width, unsigned int height, unsigned int pixelBytes, float eyeSep, float sep, float conv)
        {
            // Normally sep is in [0, 100], and we want the fractional part of 1. 
            float finalSeparation = eyeSep * sep * 0.01f;
            leftEye[0] = -finalSeparation;
            leftEye[1] = conv;
	        leftEye[2] = 1.0f;

            rightEye[0] = -leftEye[0];
            rightEye[1] = leftEye[1];
	        rightEye[2] = -leftEye[2];

            // Fill the header
            header->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
            header->dwWidth = width;
            header->dwHeight = height;
            header->dwBPP = pixelBytes * 8;
            header->dwFlags = 0;
        }

        bool IsStereoEnabled();

#if !NO_STEREO_D3D9
        // The D3D9 "Driver" for stereo updates, encapsulates the logic that is Direct3D9 specific.
        struct D3D9Type
        {
            typedef IDirect3DDevice9 Device;
            typedef IDirect3DTexture9 Texture;
            typedef IDirect3DSurface9 StagingResource;

            static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX9_REGISTRY_PROFILE;

            static const int StereoTexWidth = 8;
            static const int StereoTexHeight = 1;
            static const D3DFORMAT StereoTexFormat = D3DFMT_G32R32F;
            static const int StereoBytesPerPixel = 8;

            static StagingResource* CreateStagingResource(Device* pDevice, float eyeSep, float sep, float conv)
            {
                StagingResource* staging = 0;
                unsigned int stagingWidth = StereoTexWidth * 2;
                unsigned int stagingHeight = StereoTexHeight + 1;

                pDevice->CreateOffscreenPlainSurface(stagingWidth, stagingHeight, StereoTexFormat, D3DPOOL_SYSTEMMEM, &staging, NULL);

                if (!staging) {
                    return 0;
                }

                D3DLOCKED_RECT lr;
                staging->LockRect(&lr, NULL, 0);
                unsigned char* sysData = (unsigned char *) lr.pBits;
                unsigned int sysMemPitch = stagingWidth * StereoBytesPerPixel;

                float* leftEyePtr = (float*)sysData;
                float* rightEyePtr = (float*)(sysData + StereoTexWidth * StereoBytesPerPixel);
                LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)(sysData + sysMemPitch);
                PopulateTextureData(leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv);
                staging->UnlockRect();

                return staging;
            }
            
            static void UpdateTextureFromStaging(Device* pDevice, Texture* tex, StagingResource* staging)
            {
                RECT stereoSrcRect;
                stereoSrcRect.top = 0;
                stereoSrcRect.bottom = StereoTexHeight;
                stereoSrcRect.left = 0;
                stereoSrcRect.right = StereoTexWidth;

                POINT stereoDstPoint;
                stereoDstPoint.x = 0;
                stereoDstPoint.y = 0;

                IDirect3DSurface9* texSurface;
                tex->GetSurfaceLevel( 0, &texSurface );

                pDevice->UpdateSurface(staging, &stereoSrcRect, texSurface, &stereoDstPoint);
                texSurface->Release();
            }
        };
#endif // NO_STEREO_D3D9

#if !NO_STEREO_D3D11
		// The D3D11 "Driver" for stereo updates, encapsulates the logic that is Direct3D11 specific.
		struct D3D11Type
		{
			typedef ID3D11Device Device;
			typedef ID3D11Texture2D Texture;
			typedef ID3D11Texture2D StagingResource;

			static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX10_REGISTRY_PROFILE;

			static const int StereoTexWidth = 8;
			static const int StereoTexHeight = 1;
			static const DXGI_FORMAT StereoTexFormat = DXGI_FORMAT_R32G32_FLOAT;
			static const int StereoBytesPerPixel = 8;

			static StagingResource* CreateStagingResource(Device* pDevice, float eyeSep, float sep, float conv)
			{
				StagingResource* staging = 0;
				unsigned int stagingWidth = StereoTexWidth * 2;
				unsigned int stagingHeight = StereoTexHeight + 1;

				// Allocate the buffer sys mem data to write the stereo tag and stereo params
				D3D11_SUBRESOURCE_DATA sysData;
				sysData.SysMemPitch = StereoBytesPerPixel * stagingWidth;
				sysData.pSysMem = new unsigned char[sysData.SysMemPitch * stagingHeight];

				float* leftEyePtr = (float*)sysData.pSysMem;
				float* rightEyePtr = (float*)((unsigned char*)sysData.pSysMem + StereoTexWidth * StereoBytesPerPixel);
				LPNVSTEREOIMAGEHEADER header = (LPNVSTEREOIMAGEHEADER)((unsigned char*)sysData.pSysMem + sysData.SysMemPitch);
				PopulateTextureData(leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv);

				D3D11_TEXTURE2D_DESC desc;
				memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
				desc.Width = stagingWidth;
				desc.Height = stagingHeight;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = StereoTexFormat;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				pDevice->CreateTexture2D(&desc, &sysData, &staging);
				delete [] sysData.pSysMem;
				return staging;
			}

			static void UpdateTextureFromStaging(Device* pDevice, Texture* tex, StagingResource* staging)
			{
				D3D11_BOX stereoSrcBox;
				stereoSrcBox.front = 0;
				stereoSrcBox.back = 1;
				stereoSrcBox.top = 0;
				stereoSrcBox.bottom = StereoTexHeight;
				stereoSrcBox.left = 0;
				stereoSrcBox.right = StereoTexWidth;

				ID3D11DeviceContext *pIC = NULL;
				pDevice->GetImmediateContext(&pIC);
				if(pIC)
				{
					pIC->CopySubresourceRegion(tex, 0, 0, 0, 0, staging, 0, &stereoSrcBox);
					pIC->Release();
				}
			}
		};
#endif // NO_STEREO_D3D11

        // The UE3 Stereo class, which can work for either D3D9 or D3D11, depending on which type it's specialized for
        // Note that both types can live side-by-side in two seperate instances as well.
        // Also note that there are convenient typedefs below the class definition.
        template <class D3DType>
        class UE3Stereo
        {
        public:
            typedef typename D3DType Parms;
            typedef typename D3DType::Device Device;
            typedef typename D3DType::Texture Texture;
            typedef typename D3DType::StagingResource StagingResource;

            UE3Stereo(bool onetimeinit) : 
              mEyeSeparation(0),
              mSeparation(0),
              mConvergence(0),
              mStereoHandle(0),
              mInitialized(false),
              mActive(false),
              mDeviceLost(true) 
            { 
				NvAPI_ShortString errStr;
				if (onetimeinit) 
				{
					// mDeviceLost is set to true to initialize the texture with good data at app startup.
					NvAPI_Status st = NvAPI_Initialize();
					if (st != NVAPI_OK) { NvAPI_GetErrorMessage(st, errStr); }

					st = NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3DType::RegistryProfileType);
					if (st != NVAPI_OK) { NvAPI_GetErrorMessage(st, errStr); }
				}
			 }

            ~UE3Stereo() 
            {
                if (mStereoHandle) {
                    NvAPI_Stereo_DestroyHandle(mStereoHandle);
                    mStereoHandle = 0;
                }
            }

            void Init(Device* dev)
            {
                NvAPI_ShortString errStr;

                NvAPI_Status st = NvAPI_Stereo_CreateHandleFromIUnknown(dev, &mStereoHandle);
                if (st != NVAPI_OK) { NvAPI_GetErrorMessage(st, errStr); }

                // Set that we've initialized regardless--we'll only try to init once.
                mInitialized = true;
            }

            // Not const because we will update the various values if an update is needed.
            bool RequiresUpdate(bool deviceLost)
            {
                bool active = IsStereoActive();
                bool updateRequired;
	            float eyeSep, sep, conv;
                if (active) {
	                if (NVAPI_OK != NvAPI_Stereo_GetEyeSeparation(mStereoHandle, &eyeSep))
		                return false;
                    if (NVAPI_OK != NvAPI_Stereo_GetSeparation(mStereoHandle, &sep ))
		                return false;
                    if (NVAPI_OK != NvAPI_Stereo_GetConvergence(mStereoHandle, &conv ))
		                return false;

	                updateRequired = (eyeSep != mEyeSeparation)
					              || (sep != mSeparation)
					              || (conv != mConvergence)
								  || (active != mActive);
                } else {
                    eyeSep = sep = conv = 0;
                    updateRequired = active != mActive;
                }

                // If the device was lost and is now restored, need to update the texture contents again.
                updateRequired = updateRequired || (!deviceLost && mDeviceLost);
                mDeviceLost = deviceLost;

                if (updateRequired) {
	                mEyeSeparation = eyeSep;
	                mSeparation = sep;
	                mConvergence = conv;
                    mActive = active;
                    return true;
                }

                return false;
            }

            bool IsStereoActive() const 
            {
                NvU8 stereoActive = 0;
                if (NVAPI_OK != NvAPI_Stereo_IsActivated(mStereoHandle, &stereoActive)) {
                    return false;
                }

                return stereoActive != 0;
            }

            void UpdateStereoTexture(Device* dev, Texture* tex, bool deviceLost)
            {
				check(dev && tex);

                if (!mInitialized) {
                    Init(dev);
                }

                if (!RequiresUpdate(deviceLost)) {
                    return;
                }

                StagingResource *staging = D3DType::CreateStagingResource(dev, mEyeSeparation, mSeparation, mConvergence);
                if (staging) {
                    D3DType::UpdateTextureFromStaging(dev, tex, staging);
                    staging->Release();
                }
            }

        private:
            float mEyeSeparation;
            float mSeparation;
            float mConvergence;

            StereoHandle mStereoHandle;
            bool mInitialized;
            bool mActive;
            bool mDeviceLost;
        };

#if !NO_STEREO_D3D9
        typedef UE3Stereo<D3D9Type> UE3StereoD3D9;
#endif

#if !NO_STEREO_D3D11
		typedef UE3Stereo<D3D11Type> UE3StereoD3D11;
#endif 
    };
};

#else /* (!CONSOLE) */
namespace nv {
	namespace stereo {
		inline bool IsStereoEnabled()
		{
			return false;
		}
	};
};


#endif

#endif /* __UE3STEREO__ */
