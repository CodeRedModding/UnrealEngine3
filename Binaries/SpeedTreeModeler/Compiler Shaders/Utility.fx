///////////////////////////////////////////////////////////////////////  
//  Utility Shaders
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2008 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      Web: http://www.idvinc.com




///////////////////////////////////////////////////////////////////////  
//  Modulate_Float
//
//  Returns x % y (some compilers generate way too many instructions when
//  using the native '%' operator)

float Modulate_Float(float x, float y)
{
    return x - (int(x / y) * y);
}



///////////////////////////////////////////////////////////////////////  
//  RotationMatrix_zAxis
//
//  Constructs a Z-axis rotation matrix

float3x3 RotationMatrix_zAxis(float fAngle)
{
    // compute sin/cos of fAngle
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float3x3(vSinCos.y, -vSinCos.x, 0.0f, 
                    vSinCos.x, vSinCos.y, 0.0f, 
                    0.0f, 0.0f, 1.0f);
}


///////////////////////////////////////////////////////////////////////  
//  Rotate_zAxis
//
//  Returns an updated .xy value

float2 Rotate_zAxis(float fAngle, float3 vPoint)
{
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float2(dot(vSinCos.yx, vPoint.xy), dot(float2(-vSinCos.x, vSinCos.y), vPoint.xy));
}


///////////////////////////////////////////////////////////////////////  
//  RotationMatrix_yAxis
//
//  Constructs a Y-axis rotation matrix

float3x3 RotationMatrix_yAxis(float fAngle)
{
    // compute sin/cos of fAngle
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float3x3(vSinCos.y, 0.0f, vSinCos.x,
                    0.0f, 1.0f, 0.0f,
                    -vSinCos.x, 0.0f, vSinCos.y);
}


///////////////////////////////////////////////////////////////////////  
//  Rotate_yAxis
//
//  Returns an updated .xz value

float2 Rotate_yAxis(float fAngle, float3 vPoint)
{
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float2(dot(float2(vSinCos.y, -vSinCos.x), vPoint.xz), dot(vSinCos.xy, vPoint.xz));
}


///////////////////////////////////////////////////////////////////////  
//  RotationMatrix_xAxis
//
//  Constructs a X-axis rotation matrix

float3x3 RotationMatrix_xAxis(float fAngle)
{
    // compute sin/cos of fAngle
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float3x3(1.0f, 0.0f, 0.0f,
                    0.0f, vSinCos.y, -vSinCos.x,
                    0.0f, vSinCos.x, vSinCos.y);
}


///////////////////////////////////////////////////////////////////////  
//  Rotate_xAxis
//
//  Returns an updated .yz value

float2 Rotate_xAxis(float fAngle, float3 vPoint)
{
    float2 vSinCos;
    sincos(fAngle, vSinCos.x, vSinCos.y);
    
    return float2(dot(vSinCos.yx, vPoint.yz), dot(float2(-vSinCos.x, vSinCos.y), vPoint.yz));
}

