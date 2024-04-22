/*=============================================================================
	OpenGLWindowsLoader.h: Manual loading of OpenGL functions from DLL.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_OPENGLWINDOWSLOADER
#define _INC_OPENGLWINDOWSLOADER

#if _WINDOWS

#include "MinWindows.h"

/*
** Copyright 1996 Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
*/

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

/*************************************************************/

/* Version */
#define GL_VERSION_1_1                    1

/* AccumOp */
#define GL_ACCUM                          0x0100
#define GL_LOAD                           0x0101
#define GL_RETURN                         0x0102
#define GL_MULT                           0x0103
#define GL_ADD                            0x0104

/* AlphaFunction */
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

/* AttribMask */
#define GL_CURRENT_BIT                    0x00000001
#define GL_POINT_BIT                      0x00000002
#define GL_LINE_BIT                       0x00000004
#define GL_POLYGON_BIT                    0x00000008
#define GL_POLYGON_STIPPLE_BIT            0x00000010
#define GL_PIXEL_MODE_BIT                 0x00000020
#define GL_LIGHTING_BIT                   0x00000040
#define GL_FOG_BIT                        0x00000080
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_ACCUM_BUFFER_BIT               0x00000200
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_VIEWPORT_BIT                   0x00000800
#define GL_TRANSFORM_BIT                  0x00001000
#define GL_ENABLE_BIT                     0x00002000
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_HINT_BIT                       0x00008000
#define GL_EVAL_BIT                       0x00010000
#define GL_LIST_BIT                       0x00020000
#define GL_TEXTURE_BIT                    0x00040000
#define GL_SCISSOR_BIT                    0x00080000
#define GL_ALL_ATTRIB_BITS                0x000fffff

/* BeginMode */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006
#define GL_QUADS                          0x0007
#define GL_QUAD_STRIP                     0x0008
#define GL_POLYGON                        0x0009

/* BlendingFactorDest */
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305

/* BlendingFactorSrc */
/*      GL_ZERO */
/*      GL_ONE */
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308
/*      GL_SRC_ALPHA */
/*      GL_ONE_MINUS_SRC_ALPHA */
/*      GL_DST_ALPHA */
/*      GL_ONE_MINUS_DST_ALPHA */

/* Boolean */
#define GL_TRUE                           1
#define GL_FALSE                          0

/* ClearBufferMask */
/*      GL_COLOR_BUFFER_BIT */
/*      GL_ACCUM_BUFFER_BIT */
/*      GL_STENCIL_BUFFER_BIT */
/*      GL_DEPTH_BUFFER_BIT */

/* ClientArrayType */
/*      GL_VERTEX_ARRAY */
/*      GL_NORMAL_ARRAY */
/*      GL_COLOR_ARRAY */
/*      GL_INDEX_ARRAY */
/*      GL_TEXTURE_COORD_ARRAY */
/*      GL_EDGE_FLAG_ARRAY */

/* ClipPlaneName */
#define GL_CLIP_PLANE0                    0x3000
#define GL_CLIP_PLANE1                    0x3001
#define GL_CLIP_PLANE2                    0x3002
#define GL_CLIP_PLANE3                    0x3003
#define GL_CLIP_PLANE4                    0x3004
#define GL_CLIP_PLANE5                    0x3005

/* ColorMaterialFace */
/*      GL_FRONT */
/*      GL_BACK */
/*      GL_FRONT_AND_BACK */

/* ColorMaterialParameter */
/*      GL_AMBIENT */
/*      GL_DIFFUSE */
/*      GL_SPECULAR */
/*      GL_EMISSION */
/*      GL_AMBIENT_AND_DIFFUSE */

/* ColorPointerType */
/*      GL_BYTE */
/*      GL_UNSIGNED_BYTE */
/*      GL_SHORT */
/*      GL_UNSIGNED_SHORT */
/*      GL_INT */
/*      GL_UNSIGNED_INT */
/*      GL_FLOAT */
/*      GL_DOUBLE */

/* CullFaceMode */
/*      GL_FRONT */
/*      GL_BACK */
/*      GL_FRONT_AND_BACK */

/* DataType */
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_2_BYTES                        0x1407
#define GL_3_BYTES                        0x1408
#define GL_4_BYTES                        0x1409
#define GL_DOUBLE                         0x140A

/* DepthFunction */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* DrawBufferMode */
#define GL_NONE                           0
#define GL_FRONT_LEFT                     0x0400
#define GL_FRONT_RIGHT                    0x0401
#define GL_BACK_LEFT                      0x0402
#define GL_BACK_RIGHT                     0x0403
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_LEFT                           0x0406
#define GL_RIGHT                          0x0407
#define GL_FRONT_AND_BACK                 0x0408
#define GL_AUX0                           0x0409
#define GL_AUX1                           0x040A
#define GL_AUX2                           0x040B
#define GL_AUX3                           0x040C

/* Enable */
/*      GL_FOG */
/*      GL_LIGHTING */
/*      GL_TEXTURE_1D */
/*      GL_TEXTURE_2D */
/*      GL_LINE_STIPPLE */
/*      GL_POLYGON_STIPPLE */
/*      GL_CULL_FACE */
/*      GL_ALPHA_TEST */
/*      GL_BLEND */
/*      GL_INDEX_LOGIC_OP */
/*      GL_COLOR_LOGIC_OP */
/*      GL_DITHER */
/*      GL_STENCIL_TEST */
/*      GL_DEPTH_TEST */
/*      GL_CLIP_PLANE0 */
/*      GL_CLIP_PLANE1 */
/*      GL_CLIP_PLANE2 */
/*      GL_CLIP_PLANE3 */
/*      GL_CLIP_PLANE4 */
/*      GL_CLIP_PLANE5 */
/*      GL_LIGHT0 */
/*      GL_LIGHT1 */
/*      GL_LIGHT2 */
/*      GL_LIGHT3 */
/*      GL_LIGHT4 */
/*      GL_LIGHT5 */
/*      GL_LIGHT6 */
/*      GL_LIGHT7 */
/*      GL_TEXTURE_GEN_S */
/*      GL_TEXTURE_GEN_T */
/*      GL_TEXTURE_GEN_R */
/*      GL_TEXTURE_GEN_Q */
/*      GL_MAP1_VERTEX_3 */
/*      GL_MAP1_VERTEX_4 */
/*      GL_MAP1_COLOR_4 */
/*      GL_MAP1_INDEX */
/*      GL_MAP1_NORMAL */
/*      GL_MAP1_TEXTURE_COORD_1 */
/*      GL_MAP1_TEXTURE_COORD_2 */
/*      GL_MAP1_TEXTURE_COORD_3 */
/*      GL_MAP1_TEXTURE_COORD_4 */
/*      GL_MAP2_VERTEX_3 */
/*      GL_MAP2_VERTEX_4 */
/*      GL_MAP2_COLOR_4 */
/*      GL_MAP2_INDEX */
/*      GL_MAP2_NORMAL */
/*      GL_MAP2_TEXTURE_COORD_1 */
/*      GL_MAP2_TEXTURE_COORD_2 */
/*      GL_MAP2_TEXTURE_COORD_3 */
/*      GL_MAP2_TEXTURE_COORD_4 */
/*      GL_POINT_SMOOTH */
/*      GL_LINE_SMOOTH */
/*      GL_POLYGON_SMOOTH */
/*      GL_SCISSOR_TEST */
/*      GL_COLOR_MATERIAL */
/*      GL_NORMALIZE */
/*      GL_AUTO_NORMAL */
/*      GL_VERTEX_ARRAY */
/*      GL_NORMAL_ARRAY */
/*      GL_COLOR_ARRAY */
/*      GL_INDEX_ARRAY */
/*      GL_TEXTURE_COORD_ARRAY */
/*      GL_EDGE_FLAG_ARRAY */
/*      GL_POLYGON_OFFSET_POINT */
/*      GL_POLYGON_OFFSET_LINE */
/*      GL_POLYGON_OFFSET_FILL */

/* ErrorCode */
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
#define GL_OUT_OF_MEMORY                  0x0505

/* FeedBackMode */
#define GL_2D                             0x0600
#define GL_3D                             0x0601
#define GL_3D_COLOR                       0x0602
#define GL_3D_COLOR_TEXTURE               0x0603
#define GL_4D_COLOR_TEXTURE               0x0604

/* FeedBackToken */
#define GL_PASS_THROUGH_TOKEN             0x0700
#define GL_POINT_TOKEN                    0x0701
#define GL_LINE_TOKEN                     0x0702
#define GL_POLYGON_TOKEN                  0x0703
#define GL_BITMAP_TOKEN                   0x0704
#define GL_DRAW_PIXEL_TOKEN               0x0705
#define GL_COPY_PIXEL_TOKEN               0x0706
#define GL_LINE_RESET_TOKEN               0x0707

/* FogMode */
/*      GL_LINEAR */
#define GL_EXP                            0x0800
#define GL_EXP2                           0x0801


/* FogParameter */
/*      GL_FOG_COLOR */
/*      GL_FOG_DENSITY */
/*      GL_FOG_END */
/*      GL_FOG_INDEX */
/*      GL_FOG_MODE */
/*      GL_FOG_START */

/* FrontFaceDirection */
#define GL_CW                             0x0900
#define GL_CCW                            0x0901

/* GetMapTarget */
#define GL_COEFF                          0x0A00
#define GL_ORDER                          0x0A01
#define GL_DOMAIN                         0x0A02

/* GetPixelMap */
/*      GL_PIXEL_MAP_I_TO_I */
/*      GL_PIXEL_MAP_S_TO_S */
/*      GL_PIXEL_MAP_I_TO_R */
/*      GL_PIXEL_MAP_I_TO_G */
/*      GL_PIXEL_MAP_I_TO_B */
/*      GL_PIXEL_MAP_I_TO_A */
/*      GL_PIXEL_MAP_R_TO_R */
/*      GL_PIXEL_MAP_G_TO_G */
/*      GL_PIXEL_MAP_B_TO_B */
/*      GL_PIXEL_MAP_A_TO_A */

/* GetPointerTarget */
/*      GL_VERTEX_ARRAY_POINTER */
/*      GL_NORMAL_ARRAY_POINTER */
/*      GL_COLOR_ARRAY_POINTER */
/*      GL_INDEX_ARRAY_POINTER */
/*      GL_TEXTURE_COORD_ARRAY_POINTER */
/*      GL_EDGE_FLAG_ARRAY_POINTER */

/* GetTarget */
#define GL_CURRENT_COLOR                  0x0B00
#define GL_CURRENT_INDEX                  0x0B01
#define GL_CURRENT_NORMAL                 0x0B02
#define GL_CURRENT_TEXTURE_COORDS         0x0B03
#define GL_CURRENT_RASTER_COLOR           0x0B04
#define GL_CURRENT_RASTER_INDEX           0x0B05
#define GL_CURRENT_RASTER_TEXTURE_COORDS  0x0B06
#define GL_CURRENT_RASTER_POSITION        0x0B07
#define GL_CURRENT_RASTER_POSITION_VALID  0x0B08
#define GL_CURRENT_RASTER_DISTANCE        0x0B09
#define GL_POINT_SMOOTH                   0x0B10
#define GL_POINT_SIZE                     0x0B11
#define GL_POINT_SIZE_RANGE               0x0B12
#define GL_POINT_SIZE_GRANULARITY         0x0B13
#define GL_LINE_SMOOTH                    0x0B20
#define GL_LINE_WIDTH                     0x0B21
#define GL_LINE_WIDTH_RANGE               0x0B22
#define GL_LINE_WIDTH_GRANULARITY         0x0B23
#define GL_LINE_STIPPLE                   0x0B24
#define GL_LINE_STIPPLE_PATTERN           0x0B25
#define GL_LINE_STIPPLE_REPEAT            0x0B26
#define GL_LIST_MODE                      0x0B30
#define GL_MAX_LIST_NESTING               0x0B31
#define GL_LIST_BASE                      0x0B32
#define GL_LIST_INDEX                     0x0B33
#define GL_POLYGON_MODE                   0x0B40
#define GL_POLYGON_SMOOTH                 0x0B41
#define GL_POLYGON_STIPPLE                0x0B42
#define GL_EDGE_FLAG                      0x0B43
#define GL_CULL_FACE                      0x0B44
#define GL_CULL_FACE_MODE                 0x0B45
#define GL_FRONT_FACE                     0x0B46
#define GL_LIGHTING                       0x0B50
#define GL_LIGHT_MODEL_LOCAL_VIEWER       0x0B51
#define GL_LIGHT_MODEL_TWO_SIDE           0x0B52
#define GL_LIGHT_MODEL_AMBIENT            0x0B53
#define GL_SHADE_MODEL                    0x0B54
#define GL_COLOR_MATERIAL_FACE            0x0B55
#define GL_COLOR_MATERIAL_PARAMETER       0x0B56
#define GL_COLOR_MATERIAL                 0x0B57
#define GL_FOG                            0x0B60
#define GL_FOG_INDEX                      0x0B61
#define GL_FOG_DENSITY                    0x0B62
#define GL_FOG_START                      0x0B63
#define GL_FOG_END                        0x0B64
#define GL_FOG_MODE                       0x0B65
#define GL_FOG_COLOR                      0x0B66
#define GL_DEPTH_RANGE                    0x0B70
#define GL_DEPTH_TEST                     0x0B71
#define GL_DEPTH_WRITEMASK                0x0B72
#define GL_DEPTH_CLEAR_VALUE              0x0B73
#define GL_DEPTH_FUNC                     0x0B74
#define GL_ACCUM_CLEAR_VALUE              0x0B80
#define GL_STENCIL_TEST                   0x0B90
#define GL_STENCIL_CLEAR_VALUE            0x0B91
#define GL_STENCIL_FUNC                   0x0B92
#define GL_STENCIL_VALUE_MASK             0x0B93
#define GL_STENCIL_FAIL                   0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL        0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS        0x0B96
#define GL_STENCIL_REF                    0x0B97
#define GL_STENCIL_WRITEMASK              0x0B98
#define GL_MATRIX_MODE                    0x0BA0
#define GL_NORMALIZE                      0x0BA1
#define GL_VIEWPORT                       0x0BA2
#define GL_MODELVIEW_STACK_DEPTH          0x0BA3
#define GL_PROJECTION_STACK_DEPTH         0x0BA4
#define GL_TEXTURE_STACK_DEPTH            0x0BA5
#define GL_MODELVIEW_MATRIX               0x0BA6
#define GL_PROJECTION_MATRIX              0x0BA7
#define GL_TEXTURE_MATRIX                 0x0BA8
#define GL_ATTRIB_STACK_DEPTH             0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH      0x0BB1
#define GL_ALPHA_TEST                     0x0BC0
#define GL_ALPHA_TEST_FUNC                0x0BC1
#define GL_ALPHA_TEST_REF                 0x0BC2
#define GL_DITHER                         0x0BD0
#define GL_BLEND_DST                      0x0BE0
#define GL_BLEND_SRC                      0x0BE1
#define GL_BLEND                          0x0BE2
#define GL_LOGIC_OP_MODE                  0x0BF0
#define GL_INDEX_LOGIC_OP                 0x0BF1
#define GL_COLOR_LOGIC_OP                 0x0BF2
#define GL_AUX_BUFFERS                    0x0C00
#define GL_DRAW_BUFFER                    0x0C01
#define GL_READ_BUFFER                    0x0C02
#define GL_SCISSOR_BOX                    0x0C10
#define GL_SCISSOR_TEST                   0x0C11
#define GL_INDEX_CLEAR_VALUE              0x0C20
#define GL_INDEX_WRITEMASK                0x0C21
#define GL_COLOR_CLEAR_VALUE              0x0C22
#define GL_COLOR_WRITEMASK                0x0C23
#define GL_INDEX_MODE                     0x0C30
#define GL_RGBA_MODE                      0x0C31
#define GL_DOUBLEBUFFER                   0x0C32
#define GL_STEREO                         0x0C33
#define GL_RENDER_MODE                    0x0C40
#define GL_PERSPECTIVE_CORRECTION_HINT    0x0C50
#define GL_POINT_SMOOTH_HINT              0x0C51
#define GL_LINE_SMOOTH_HINT               0x0C52
#define GL_POLYGON_SMOOTH_HINT            0x0C53
#define GL_FOG_HINT                       0x0C54
#define GL_TEXTURE_GEN_S                  0x0C60
#define GL_TEXTURE_GEN_T                  0x0C61
#define GL_TEXTURE_GEN_R                  0x0C62
#define GL_TEXTURE_GEN_Q                  0x0C63
#define GL_PIXEL_MAP_I_TO_I               0x0C70
#define GL_PIXEL_MAP_S_TO_S               0x0C71
#define GL_PIXEL_MAP_I_TO_R               0x0C72
#define GL_PIXEL_MAP_I_TO_G               0x0C73
#define GL_PIXEL_MAP_I_TO_B               0x0C74
#define GL_PIXEL_MAP_I_TO_A               0x0C75
#define GL_PIXEL_MAP_R_TO_R               0x0C76
#define GL_PIXEL_MAP_G_TO_G               0x0C77
#define GL_PIXEL_MAP_B_TO_B               0x0C78
#define GL_PIXEL_MAP_A_TO_A               0x0C79
#define GL_PIXEL_MAP_I_TO_I_SIZE          0x0CB0
#define GL_PIXEL_MAP_S_TO_S_SIZE          0x0CB1
#define GL_PIXEL_MAP_I_TO_R_SIZE          0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE          0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE          0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE          0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE          0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE          0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE          0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE          0x0CB9
#define GL_UNPACK_SWAP_BYTES              0x0CF0
#define GL_UNPACK_LSB_FIRST               0x0CF1
#define GL_UNPACK_ROW_LENGTH              0x0CF2
#define GL_UNPACK_SKIP_ROWS               0x0CF3
#define GL_UNPACK_SKIP_PIXELS             0x0CF4
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_SWAP_BYTES                0x0D00
#define GL_PACK_LSB_FIRST                 0x0D01
#define GL_PACK_ROW_LENGTH                0x0D02
#define GL_PACK_SKIP_ROWS                 0x0D03
#define GL_PACK_SKIP_PIXELS               0x0D04
#define GL_PACK_ALIGNMENT                 0x0D05
#define GL_MAP_COLOR                      0x0D10
#define GL_MAP_STENCIL                    0x0D11
#define GL_INDEX_SHIFT                    0x0D12
#define GL_INDEX_OFFSET                   0x0D13
#define GL_RED_SCALE                      0x0D14
#define GL_RED_BIAS                       0x0D15
#define GL_ZOOM_X                         0x0D16
#define GL_ZOOM_Y                         0x0D17
#define GL_GREEN_SCALE                    0x0D18
#define GL_GREEN_BIAS                     0x0D19
#define GL_BLUE_SCALE                     0x0D1A
#define GL_BLUE_BIAS                      0x0D1B
#define GL_ALPHA_SCALE                    0x0D1C
#define GL_ALPHA_BIAS                     0x0D1D
#define GL_DEPTH_SCALE                    0x0D1E
#define GL_DEPTH_BIAS                     0x0D1F
#define GL_MAX_EVAL_ORDER                 0x0D30
#define GL_MAX_LIGHTS                     0x0D31
#define GL_MAX_CLIP_PLANES                0x0D32
#define GL_MAX_TEXTURE_SIZE               0x0D33
#define GL_MAX_PIXEL_MAP_TABLE            0x0D34
#define GL_MAX_ATTRIB_STACK_DEPTH         0x0D35
#define GL_MAX_MODELVIEW_STACK_DEPTH      0x0D36
#define GL_MAX_NAME_STACK_DEPTH           0x0D37
#define GL_MAX_PROJECTION_STACK_DEPTH     0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH        0x0D39
#define GL_MAX_VIEWPORT_DIMS              0x0D3A
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH  0x0D3B
#define GL_SUBPIXEL_BITS                  0x0D50
#define GL_INDEX_BITS                     0x0D51
#define GL_RED_BITS                       0x0D52
#define GL_GREEN_BITS                     0x0D53
#define GL_BLUE_BITS                      0x0D54
#define GL_ALPHA_BITS                     0x0D55
#define GL_DEPTH_BITS                     0x0D56
#define GL_STENCIL_BITS                   0x0D57
#define GL_ACCUM_RED_BITS                 0x0D58
#define GL_ACCUM_GREEN_BITS               0x0D59
#define GL_ACCUM_BLUE_BITS                0x0D5A
#define GL_ACCUM_ALPHA_BITS               0x0D5B
#define GL_NAME_STACK_DEPTH               0x0D70
#define GL_AUTO_NORMAL                    0x0D80
#define GL_MAP1_COLOR_4                   0x0D90
#define GL_MAP1_INDEX                     0x0D91
#define GL_MAP1_NORMAL                    0x0D92
#define GL_MAP1_TEXTURE_COORD_1           0x0D93
#define GL_MAP1_TEXTURE_COORD_2           0x0D94
#define GL_MAP1_TEXTURE_COORD_3           0x0D95
#define GL_MAP1_TEXTURE_COORD_4           0x0D96
#define GL_MAP1_VERTEX_3                  0x0D97
#define GL_MAP1_VERTEX_4                  0x0D98
#define GL_MAP2_COLOR_4                   0x0DB0
#define GL_MAP2_INDEX                     0x0DB1
#define GL_MAP2_NORMAL                    0x0DB2
#define GL_MAP2_TEXTURE_COORD_1           0x0DB3
#define GL_MAP2_TEXTURE_COORD_2           0x0DB4
#define GL_MAP2_TEXTURE_COORD_3           0x0DB5
#define GL_MAP2_TEXTURE_COORD_4           0x0DB6
#define GL_MAP2_VERTEX_3                  0x0DB7
#define GL_MAP2_VERTEX_4                  0x0DB8
#define GL_MAP1_GRID_DOMAIN               0x0DD0
#define GL_MAP1_GRID_SEGMENTS             0x0DD1
#define GL_MAP2_GRID_DOMAIN               0x0DD2
#define GL_MAP2_GRID_SEGMENTS             0x0DD3
#define GL_TEXTURE_1D                     0x0DE0
#define GL_TEXTURE_2D                     0x0DE1
#define GL_FEEDBACK_BUFFER_POINTER        0x0DF0
#define GL_FEEDBACK_BUFFER_SIZE           0x0DF1
#define GL_FEEDBACK_BUFFER_TYPE           0x0DF2
#define GL_SELECTION_BUFFER_POINTER       0x0DF3
#define GL_SELECTION_BUFFER_SIZE          0x0DF4
/*      GL_TEXTURE_BINDING_1D */
/*      GL_TEXTURE_BINDING_2D */
/*      GL_VERTEX_ARRAY */
/*      GL_NORMAL_ARRAY */
/*      GL_COLOR_ARRAY */
/*      GL_INDEX_ARRAY */
/*      GL_TEXTURE_COORD_ARRAY */
/*      GL_EDGE_FLAG_ARRAY */
/*      GL_VERTEX_ARRAY_SIZE */
/*      GL_VERTEX_ARRAY_TYPE */
/*      GL_VERTEX_ARRAY_STRIDE */
/*      GL_NORMAL_ARRAY_TYPE */
/*      GL_NORMAL_ARRAY_STRIDE */
/*      GL_COLOR_ARRAY_SIZE */
/*      GL_COLOR_ARRAY_TYPE */
/*      GL_COLOR_ARRAY_STRIDE */
/*      GL_INDEX_ARRAY_TYPE */
/*      GL_INDEX_ARRAY_STRIDE */
/*      GL_TEXTURE_COORD_ARRAY_SIZE */
/*      GL_TEXTURE_COORD_ARRAY_TYPE */
/*      GL_TEXTURE_COORD_ARRAY_STRIDE */
/*      GL_EDGE_FLAG_ARRAY_STRIDE */
/*      GL_POLYGON_OFFSET_FACTOR */
/*      GL_POLYGON_OFFSET_UNITS */

/* GetTextureParameter */
/*      GL_TEXTURE_MAG_FILTER */
/*      GL_TEXTURE_MIN_FILTER */
/*      GL_TEXTURE_WRAP_S */
/*      GL_TEXTURE_WRAP_T */
#define GL_TEXTURE_WIDTH                  0x1000
#define GL_TEXTURE_HEIGHT                 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT        0x1003
#define GL_TEXTURE_BORDER_COLOR           0x1004
#define GL_TEXTURE_BORDER                 0x1005
/*      GL_TEXTURE_RED_SIZE */
/*      GL_TEXTURE_GREEN_SIZE */
/*      GL_TEXTURE_BLUE_SIZE */
/*      GL_TEXTURE_ALPHA_SIZE */
/*      GL_TEXTURE_LUMINANCE_SIZE */
/*      GL_TEXTURE_INTENSITY_SIZE */
/*      GL_TEXTURE_PRIORITY */
/*      GL_TEXTURE_RESIDENT */

/* HintMode */
#define GL_DONT_CARE                      0x1100
#define GL_FASTEST                        0x1101
#define GL_NICEST                         0x1102

/* HintTarget */
/*      GL_PERSPECTIVE_CORRECTION_HINT */
/*      GL_POINT_SMOOTH_HINT */
/*      GL_LINE_SMOOTH_HINT */
/*      GL_POLYGON_SMOOTH_HINT */
/*      GL_FOG_HINT */
/*      GL_PHONG_HINT */

/* IndexPointerType */
/*      GL_SHORT */
/*      GL_INT */
/*      GL_FLOAT */
/*      GL_DOUBLE */

/* LightModelParameter */
/*      GL_LIGHT_MODEL_AMBIENT */
/*      GL_LIGHT_MODEL_LOCAL_VIEWER */
/*      GL_LIGHT_MODEL_TWO_SIDE */

/* LightName */
#define GL_LIGHT0                         0x4000
#define GL_LIGHT1                         0x4001
#define GL_LIGHT2                         0x4002
#define GL_LIGHT3                         0x4003
#define GL_LIGHT4                         0x4004
#define GL_LIGHT5                         0x4005
#define GL_LIGHT6                         0x4006
#define GL_LIGHT7                         0x4007

/* LightParameter */
#define GL_AMBIENT                        0x1200
#define GL_DIFFUSE                        0x1201
#define GL_SPECULAR                       0x1202
#define GL_POSITION                       0x1203
#define GL_SPOT_DIRECTION                 0x1204
#define GL_SPOT_EXPONENT                  0x1205
#define GL_SPOT_CUTOFF                    0x1206
#define GL_CONSTANT_ATTENUATION           0x1207
#define GL_LINEAR_ATTENUATION             0x1208
#define GL_QUADRATIC_ATTENUATION          0x1209

/* InterleavedArrays */
/*      GL_V2F */
/*      GL_V3F */
/*      GL_C4UB_V2F */
/*      GL_C4UB_V3F */
/*      GL_C3F_V3F */
/*      GL_N3F_V3F */
/*      GL_C4F_N3F_V3F */
/*      GL_T2F_V3F */
/*      GL_T4F_V4F */
/*      GL_T2F_C4UB_V3F */
/*      GL_T2F_C3F_V3F */
/*      GL_T2F_N3F_V3F */
/*      GL_T2F_C4F_N3F_V3F */
/*      GL_T4F_C4F_N3F_V4F */

/* ListMode */
#define GL_COMPILE                        0x1300
#define GL_COMPILE_AND_EXECUTE            0x1301

/* ListNameType */
/*      GL_BYTE */
/*      GL_UNSIGNED_BYTE */
/*      GL_SHORT */
/*      GL_UNSIGNED_SHORT */
/*      GL_INT */
/*      GL_UNSIGNED_INT */
/*      GL_FLOAT */
/*      GL_2_BYTES */
/*      GL_3_BYTES */
/*      GL_4_BYTES */

/* LogicOp */
#define GL_CLEAR                          0x1500
#define GL_AND                            0x1501
#define GL_AND_REVERSE                    0x1502
#define GL_COPY                           0x1503
#define GL_AND_INVERTED                   0x1504
#define GL_NOOP                           0x1505
#define GL_XOR                            0x1506
#define GL_OR                             0x1507
#define GL_NOR                            0x1508
#define GL_EQUIV                          0x1509
#define GL_INVERT                         0x150A
#define GL_OR_REVERSE                     0x150B
#define GL_COPY_INVERTED                  0x150C
#define GL_OR_INVERTED                    0x150D
#define GL_NAND                           0x150E
#define GL_SET                            0x150F

/* MapTarget */
/*      GL_MAP1_COLOR_4 */
/*      GL_MAP1_INDEX */
/*      GL_MAP1_NORMAL */
/*      GL_MAP1_TEXTURE_COORD_1 */
/*      GL_MAP1_TEXTURE_COORD_2 */
/*      GL_MAP1_TEXTURE_COORD_3 */
/*      GL_MAP1_TEXTURE_COORD_4 */
/*      GL_MAP1_VERTEX_3 */
/*      GL_MAP1_VERTEX_4 */
/*      GL_MAP2_COLOR_4 */
/*      GL_MAP2_INDEX */
/*      GL_MAP2_NORMAL */
/*      GL_MAP2_TEXTURE_COORD_1 */
/*      GL_MAP2_TEXTURE_COORD_2 */
/*      GL_MAP2_TEXTURE_COORD_3 */
/*      GL_MAP2_TEXTURE_COORD_4 */
/*      GL_MAP2_VERTEX_3 */
/*      GL_MAP2_VERTEX_4 */

/* MaterialFace */
/*      GL_FRONT */
/*      GL_BACK */
/*      GL_FRONT_AND_BACK */

/* MaterialParameter */
#define GL_EMISSION                       0x1600
#define GL_SHININESS                      0x1601
#define GL_AMBIENT_AND_DIFFUSE            0x1602
#define GL_COLOR_INDEXES                  0x1603
/*      GL_AMBIENT */
/*      GL_DIFFUSE */
/*      GL_SPECULAR */

/* MatrixMode */
#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701
#define GL_TEXTURE                        0x1702

/* MeshMode1 */
/*      GL_POINT */
/*      GL_LINE */

/* MeshMode2 */
/*      GL_POINT */
/*      GL_LINE */
/*      GL_FILL */

/* NormalPointerType */
/*      GL_BYTE */
/*      GL_SHORT */
/*      GL_INT */
/*      GL_FLOAT */
/*      GL_DOUBLE */

/* PixelCopyType */
#define GL_COLOR                          0x1800
#define GL_DEPTH                          0x1801
#define GL_STENCIL                        0x1802

/* PixelFormat */
#define GL_COLOR_INDEX                    0x1900
#define GL_STENCIL_INDEX                  0x1901
#define GL_DEPTH_COMPONENT                0x1902
#define GL_RED                            0x1903
#define GL_GREEN                          0x1904
#define GL_BLUE                           0x1905
#define GL_ALPHA                          0x1906
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE_ALPHA                0x190A

/* PixelMap */
/*      GL_PIXEL_MAP_I_TO_I */
/*      GL_PIXEL_MAP_S_TO_S */
/*      GL_PIXEL_MAP_I_TO_R */
/*      GL_PIXEL_MAP_I_TO_G */
/*      GL_PIXEL_MAP_I_TO_B */
/*      GL_PIXEL_MAP_I_TO_A */
/*      GL_PIXEL_MAP_R_TO_R */
/*      GL_PIXEL_MAP_G_TO_G */
/*      GL_PIXEL_MAP_B_TO_B */
/*      GL_PIXEL_MAP_A_TO_A */

/* PixelStore */
/*      GL_UNPACK_SWAP_BYTES */
/*      GL_UNPACK_LSB_FIRST */
/*      GL_UNPACK_ROW_LENGTH */
/*      GL_UNPACK_SKIP_ROWS */
/*      GL_UNPACK_SKIP_PIXELS */
/*      GL_UNPACK_ALIGNMENT */
/*      GL_PACK_SWAP_BYTES */
/*      GL_PACK_LSB_FIRST */
/*      GL_PACK_ROW_LENGTH */
/*      GL_PACK_SKIP_ROWS */
/*      GL_PACK_SKIP_PIXELS */
/*      GL_PACK_ALIGNMENT */

/* PixelTransfer */
/*      GL_MAP_COLOR */
/*      GL_MAP_STENCIL */
/*      GL_INDEX_SHIFT */
/*      GL_INDEX_OFFSET */
/*      GL_RED_SCALE */
/*      GL_RED_BIAS */
/*      GL_GREEN_SCALE */
/*      GL_GREEN_BIAS */
/*      GL_BLUE_SCALE */
/*      GL_BLUE_BIAS */
/*      GL_ALPHA_SCALE */
/*      GL_ALPHA_BIAS */
/*      GL_DEPTH_SCALE */
/*      GL_DEPTH_BIAS */

/* PixelType */
#define GL_BITMAP                         0x1A00
/*      GL_BYTE */
/*      GL_UNSIGNED_BYTE */
/*      GL_SHORT */
/*      GL_UNSIGNED_SHORT */
/*      GL_INT */
/*      GL_UNSIGNED_INT */
/*      GL_FLOAT */

/* PolygonMode */
#define GL_POINT                          0x1B00
#define GL_LINE                           0x1B01
#define GL_FILL                           0x1B02

/* ReadBufferMode */
/*      GL_FRONT_LEFT */
/*      GL_FRONT_RIGHT */
/*      GL_BACK_LEFT */
/*      GL_BACK_RIGHT */
/*      GL_FRONT */
/*      GL_BACK */
/*      GL_LEFT */
/*      GL_RIGHT */
/*      GL_AUX0 */
/*      GL_AUX1 */
/*      GL_AUX2 */
/*      GL_AUX3 */

/* RenderingMode */
#define GL_RENDER                         0x1C00
#define GL_FEEDBACK                       0x1C01
#define GL_SELECT                         0x1C02

/* ShadingModel */
#define GL_FLAT                           0x1D00
#define GL_SMOOTH                         0x1D01


/* StencilFunction */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* StencilOp */
/*      GL_ZERO */
#define GL_KEEP                           0x1E00
#define GL_REPLACE                        0x1E01
#define GL_INCR                           0x1E02
#define GL_DECR                           0x1E03
/*      GL_INVERT */

/* StringName */
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_EXTENSIONS                     0x1F03

/* TextureCoordName */
#define GL_S                              0x2000
#define GL_T                              0x2001
#define GL_R                              0x2002
#define GL_Q                              0x2003

/* TexCoordPointerType */
/*      GL_SHORT */
/*      GL_INT */
/*      GL_FLOAT */
/*      GL_DOUBLE */

/* TextureEnvMode */
#define GL_MODULATE                       0x2100
#define GL_DECAL                          0x2101
/*      GL_BLEND */
/*      GL_REPLACE */

/* TextureEnvParameter */
#define GL_TEXTURE_ENV_MODE               0x2200
#define GL_TEXTURE_ENV_COLOR              0x2201

/* TextureEnvTarget */
#define GL_TEXTURE_ENV                    0x2300

/* TextureGenMode */
#define GL_EYE_LINEAR                     0x2400
#define GL_OBJECT_LINEAR                  0x2401
#define GL_SPHERE_MAP                     0x2402

/* TextureGenParameter */
#define GL_TEXTURE_GEN_MODE               0x2500
#define GL_OBJECT_PLANE                   0x2501
#define GL_EYE_PLANE                      0x2502

/* TextureMagFilter */
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601

/* TextureMinFilter */
/*      GL_NEAREST */
/*      GL_LINEAR */
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703

/* TextureParameterName */
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
/*      GL_TEXTURE_BORDER_COLOR */
/*      GL_TEXTURE_PRIORITY */

/* TextureTarget */
/*      GL_TEXTURE_1D */
/*      GL_TEXTURE_2D */
/*      GL_PROXY_TEXTURE_1D */
/*      GL_PROXY_TEXTURE_2D */

/* TextureWrapMode */
#define GL_CLAMP                          0x2900
#define GL_REPEAT                         0x2901

/* VertexPointerType */
/*      GL_SHORT */
/*      GL_INT */
/*      GL_FLOAT */
/*      GL_DOUBLE */

/* ClientAttribMask */
#define GL_CLIENT_PIXEL_STORE_BIT         0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT        0x00000002
#define GL_CLIENT_ALL_ATTRIB_BITS         0xffffffff

/* polygon_offset */
#define GL_POLYGON_OFFSET_FACTOR          0x8038
#define GL_POLYGON_OFFSET_UNITS           0x2A00
#define GL_POLYGON_OFFSET_POINT           0x2A01
#define GL_POLYGON_OFFSET_LINE            0x2A02
#define GL_POLYGON_OFFSET_FILL            0x8037

/* texture */
#define GL_ALPHA4                         0x803B
#define GL_ALPHA8                         0x803C
#define GL_ALPHA12                        0x803D
#define GL_ALPHA16                        0x803E
#define GL_LUMINANCE4                     0x803F
#define GL_LUMINANCE8                     0x8040
#define GL_LUMINANCE12                    0x8041
#define GL_LUMINANCE16                    0x8042
#define GL_LUMINANCE4_ALPHA4              0x8043
#define GL_LUMINANCE6_ALPHA2              0x8044
#define GL_LUMINANCE8_ALPHA8              0x8045
#define GL_LUMINANCE12_ALPHA4             0x8046
#define GL_LUMINANCE12_ALPHA12            0x8047
#define GL_LUMINANCE16_ALPHA16            0x8048
#define GL_INTENSITY                      0x8049
#define GL_INTENSITY4                     0x804A
#define GL_INTENSITY8                     0x804B
#define GL_INTENSITY12                    0x804C
#define GL_INTENSITY16                    0x804D
#define GL_R3_G3_B2                       0x2A10
#define GL_RGB4                           0x804F
#define GL_RGB5                           0x8050
#define GL_RGB8                           0x8051
#define GL_RGB10                          0x8052
#define GL_RGB12                          0x8053
#define GL_RGB16                          0x8054
#define GL_RGBA2                          0x8055
#define GL_RGBA4                          0x8056
#define GL_RGB5_A1                        0x8057
#define GL_RGBA8                          0x8058
#define GL_RGB10_A2                       0x8059
#define GL_RGBA12                         0x805A
#define GL_RGBA16                         0x805B
#define GL_TEXTURE_RED_SIZE               0x805C
#define GL_TEXTURE_GREEN_SIZE             0x805D
#define GL_TEXTURE_BLUE_SIZE              0x805E
#define GL_TEXTURE_ALPHA_SIZE             0x805F
#define GL_TEXTURE_LUMINANCE_SIZE         0x8060
#define GL_TEXTURE_INTENSITY_SIZE         0x8061
#define GL_PROXY_TEXTURE_1D               0x8063
#define GL_PROXY_TEXTURE_2D               0x8064

/* texture_object */
#define GL_TEXTURE_PRIORITY               0x8066
#define GL_TEXTURE_RESIDENT               0x8067
#define GL_TEXTURE_BINDING_1D             0x8068
#define GL_TEXTURE_BINDING_2D             0x8069

/* vertex_array */
#define GL_VERTEX_ARRAY                   0x8074
#define GL_NORMAL_ARRAY                   0x8075
#define GL_COLOR_ARRAY                    0x8076
#define GL_INDEX_ARRAY                    0x8077
#define GL_TEXTURE_COORD_ARRAY            0x8078
#define GL_EDGE_FLAG_ARRAY                0x8079
#define GL_VERTEX_ARRAY_SIZE              0x807A
#define GL_VERTEX_ARRAY_TYPE              0x807B
#define GL_VERTEX_ARRAY_STRIDE            0x807C
#define GL_NORMAL_ARRAY_TYPE              0x807E
#define GL_NORMAL_ARRAY_STRIDE            0x807F
#define GL_COLOR_ARRAY_SIZE               0x8081
#define GL_COLOR_ARRAY_TYPE               0x8082
#define GL_COLOR_ARRAY_STRIDE             0x8083
#define GL_INDEX_ARRAY_TYPE               0x8085
#define GL_INDEX_ARRAY_STRIDE             0x8086
#define GL_TEXTURE_COORD_ARRAY_SIZE       0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE       0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE     0x808A
#define GL_EDGE_FLAG_ARRAY_STRIDE         0x808C
#define GL_VERTEX_ARRAY_POINTER           0x808E
#define GL_NORMAL_ARRAY_POINTER           0x808F
#define GL_COLOR_ARRAY_POINTER            0x8090
#define GL_INDEX_ARRAY_POINTER            0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER    0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER        0x8093
#define GL_V2F                            0x2A20
#define GL_V3F                            0x2A21
#define GL_C4UB_V2F                       0x2A22
#define GL_C4UB_V3F                       0x2A23
#define GL_C3F_V3F                        0x2A24
#define GL_N3F_V3F                        0x2A25
#define GL_C4F_N3F_V3F                    0x2A26
#define GL_T2F_V3F                        0x2A27
#define GL_T4F_V4F                        0x2A28
#define GL_T2F_C4UB_V3F                   0x2A29
#define GL_T2F_C3F_V3F                    0x2A2A
#define GL_T2F_N3F_V3F                    0x2A2B
#define GL_T2F_C4F_N3F_V3F                0x2A2C
#define GL_T4F_C4F_N3F_V4F                0x2A2D

/* Extensions */
#define GL_EXT_vertex_array               1
#define GL_EXT_bgra                       1
#define GL_EXT_paletted_texture           1
#define GL_WIN_swap_hint                  1
#define GL_WIN_draw_range_elements        1
// #define GL_WIN_phong_shading              1
// #define GL_WIN_specular_fog               1

/* EXT_vertex_array */
#define GL_VERTEX_ARRAY_EXT               0x8074
#define GL_NORMAL_ARRAY_EXT               0x8075
#define GL_COLOR_ARRAY_EXT                0x8076
#define GL_INDEX_ARRAY_EXT                0x8077
#define GL_TEXTURE_COORD_ARRAY_EXT        0x8078
#define GL_EDGE_FLAG_ARRAY_EXT            0x8079
#define GL_VERTEX_ARRAY_SIZE_EXT          0x807A
#define GL_VERTEX_ARRAY_TYPE_EXT          0x807B
#define GL_VERTEX_ARRAY_STRIDE_EXT        0x807C
#define GL_VERTEX_ARRAY_COUNT_EXT         0x807D
#define GL_NORMAL_ARRAY_TYPE_EXT          0x807E
#define GL_NORMAL_ARRAY_STRIDE_EXT        0x807F
#define GL_NORMAL_ARRAY_COUNT_EXT         0x8080
#define GL_COLOR_ARRAY_SIZE_EXT           0x8081
#define GL_COLOR_ARRAY_TYPE_EXT           0x8082
#define GL_COLOR_ARRAY_STRIDE_EXT         0x8083
#define GL_COLOR_ARRAY_COUNT_EXT          0x8084
#define GL_INDEX_ARRAY_TYPE_EXT           0x8085
#define GL_INDEX_ARRAY_STRIDE_EXT         0x8086
#define GL_INDEX_ARRAY_COUNT_EXT          0x8087
#define GL_TEXTURE_COORD_ARRAY_SIZE_EXT   0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE_EXT   0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE_EXT 0x808A
#define GL_TEXTURE_COORD_ARRAY_COUNT_EXT  0x808B
#define GL_EDGE_FLAG_ARRAY_STRIDE_EXT     0x808C
#define GL_EDGE_FLAG_ARRAY_COUNT_EXT      0x808D
#define GL_VERTEX_ARRAY_POINTER_EXT       0x808E
#define GL_NORMAL_ARRAY_POINTER_EXT       0x808F
#define GL_COLOR_ARRAY_POINTER_EXT        0x8090
#define GL_INDEX_ARRAY_POINTER_EXT        0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER_EXT 0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER_EXT    0x8093
#define GL_DOUBLE_EXT                     GL_DOUBLE

/* EXT_bgra */
#define GL_BGR_EXT                        0x80E0
#define GL_BGRA_EXT                       0x80E1

/* EXT_paletted_texture */

/* These must match the GL_COLOR_TABLE_*_SGI enumerants */
#define GL_COLOR_TABLE_FORMAT_EXT         0x80D8
#define GL_COLOR_TABLE_WIDTH_EXT          0x80D9
#define GL_COLOR_TABLE_RED_SIZE_EXT       0x80DA
#define GL_COLOR_TABLE_GREEN_SIZE_EXT     0x80DB
#define GL_COLOR_TABLE_BLUE_SIZE_EXT      0x80DC
#define GL_COLOR_TABLE_ALPHA_SIZE_EXT     0x80DD
#define GL_COLOR_TABLE_LUMINANCE_SIZE_EXT 0x80DE
#define GL_COLOR_TABLE_INTENSITY_SIZE_EXT 0x80DF

#define GL_COLOR_INDEX1_EXT               0x80E2
#define GL_COLOR_INDEX2_EXT               0x80E3
#define GL_COLOR_INDEX4_EXT               0x80E4
#define GL_COLOR_INDEX8_EXT               0x80E5
#define GL_COLOR_INDEX12_EXT              0x80E6
#define GL_COLOR_INDEX16_EXT              0x80E7

/* WIN_draw_range_elements */
#define GL_MAX_ELEMENTS_VERTICES_WIN      0x80E8
#define GL_MAX_ELEMENTS_INDICES_WIN       0x80E9

/* WIN_phong_shading */
#define GL_PHONG_WIN                      0x80EA 
#define GL_PHONG_HINT_WIN                 0x80EB 

/* WIN_specular_fog */
#define GL_FOG_SPECULAR_TEXTURE_WIN       0x80EC

/* For compatibility with OpenGL v1.0 */
#define GL_LOGIC_OP GL_INDEX_LOGIC_OP
#define GL_TEXTURE_COMPONENTS GL_TEXTURE_INTERNAL_FORMAT

/*************************************************************/

typedef void (APIENTRY * PFNGLACCUMPROC) (GLenum op, GLfloat value);
typedef void (APIENTRY * PFNGLALPHAFUNCPROC) (GLenum func, GLclampf ref);
typedef GLboolean (APIENTRY *PFNGLARETEXTURESRESIDENTPROC) (GLsizei n, const GLuint *textures, GLboolean *residences);
typedef void (APIENTRY * PFNGLARRAYELEMENTPROC) (GLint i);
typedef void (APIENTRY * PFNGLBEGINPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (APIENTRY * PFNGLBITMAPPROC) (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
typedef void (APIENTRY * PFNGLBLENDFUNCPROC) (GLenum sfactor, GLenum dfactor);
typedef void (APIENTRY * PFNGLCALLLISTPROC) (GLuint list);
typedef void (APIENTRY * PFNGLCALLLISTSPROC) (GLsizei n, GLenum type, const GLvoid *lists);
typedef void (APIENTRY * PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRY * PFNGLCLEARACCUMPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRY * PFNGLCLEARCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (APIENTRY * PFNGLCLEARDEPTHPROC) (GLclampd depth);
typedef void (APIENTRY * PFNGLCLEARINDEXPROC) (GLfloat c);
typedef void (APIENTRY * PFNGLCLEARSTENCILPROC) (GLint s);
typedef void (APIENTRY * PFNGLCLIPPLANEPROC) (GLenum plane, const GLdouble *equation);
typedef void (APIENTRY * PFNGLCOLOR3BPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (APIENTRY * PFNGLCOLOR3BVPROC) (const GLbyte *v);
typedef void (APIENTRY * PFNGLCOLOR3DPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (APIENTRY * PFNGLCOLOR3DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLCOLOR3FPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (APIENTRY * PFNGLCOLOR3FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLCOLOR3IPROC) (GLint red, GLint green, GLint blue);
typedef void (APIENTRY * PFNGLCOLOR3IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLCOLOR3SPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (APIENTRY * PFNGLCOLOR3SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLCOLOR3UBPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (APIENTRY * PFNGLCOLOR3UBVPROC) (const GLubyte *v);
typedef void (APIENTRY * PFNGLCOLOR3UIPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (APIENTRY * PFNGLCOLOR3UIVPROC) (const GLuint *v);
typedef void (APIENTRY * PFNGLCOLOR3USPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (APIENTRY * PFNGLCOLOR3USVPROC) (const GLushort *v);
typedef void (APIENTRY * PFNGLCOLOR4BPROC) (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
typedef void (APIENTRY * PFNGLCOLOR4BVPROC) (const GLbyte *v);
typedef void (APIENTRY * PFNGLCOLOR4DPROC) (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
typedef void (APIENTRY * PFNGLCOLOR4DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLCOLOR4FPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRY * PFNGLCOLOR4FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLCOLOR4IPROC) (GLint red, GLint green, GLint blue, GLint alpha);
typedef void (APIENTRY * PFNGLCOLOR4IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLCOLOR4SPROC) (GLshort red, GLshort green, GLshort blue, GLshort alpha);
typedef void (APIENTRY * PFNGLCOLOR4SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLCOLOR4UBPROC) (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
typedef void (APIENTRY * PFNGLCOLOR4UBVPROC) (const GLubyte *v);
typedef void (APIENTRY * PFNGLCOLOR4UIPROC) (GLuint red, GLuint green, GLuint blue, GLuint alpha);
typedef void (APIENTRY * PFNGLCOLOR4UIVPROC) (const GLuint *v);
typedef void (APIENTRY * PFNGLCOLOR4USPROC) (GLushort red, GLushort green, GLushort blue, GLushort alpha);
typedef void (APIENTRY * PFNGLCOLOR4USVPROC) (const GLushort *v);
typedef void (APIENTRY * PFNGLCOLORMASKPROC) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRY * PFNGLCOLORMATERIALPROC) (GLenum face, GLenum mode);
typedef void (APIENTRY * PFNGLCOLORPOINTERPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLCOPYPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
typedef void (APIENTRY * PFNGLCOPYTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (APIENTRY * PFNGLCOPYTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRY * PFNGLCOPYTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (APIENTRY * PFNGLCOPYTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY * PFNGLCULLFACEPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLDELETELISTSPROC) (GLuint list, GLsizei range);
typedef void (APIENTRY * PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef void (APIENTRY * PFNGLDEPTHFUNCPROC) (GLenum func);
typedef void (APIENTRY * PFNGLDEPTHMASKPROC) (GLboolean flag);
typedef void (APIENTRY * PFNGLDEPTHRANGEPROC) (GLclampd zNear, GLclampd zFar);
typedef void (APIENTRY * PFNGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRY * PFNGLDISABLECLIENTSTATEPROC) (GLenum array);
typedef void (APIENTRY * PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRY * PFNGLDRAWBUFFERPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (APIENTRY * PFNGLDRAWPIXELSPROC) (GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY * PFNGLEDGEFLAGPROC) (GLboolean flag);
typedef void (APIENTRY * PFNGLEDGEFLAGPOINTERPROC) (GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLEDGEFLAGVPROC) (const GLboolean *flag);
typedef void (APIENTRY * PFNGLENABLEPROC) (GLenum cap);
typedef void (APIENTRY * PFNGLENABLECLIENTSTATEPROC) (GLenum array);
typedef void (APIENTRY * PFNGLENDPROC) (void);
typedef void (APIENTRY * PFNGLENDLISTPROC) (void);
typedef void (APIENTRY * PFNGLEVALCOORD1DPROC) (GLdouble u);
typedef void (APIENTRY * PFNGLEVALCOORD1DVPROC) (const GLdouble *u);
typedef void (APIENTRY * PFNGLEVALCOORD1FPROC) (GLfloat u);
typedef void (APIENTRY * PFNGLEVALCOORD1FVPROC) (const GLfloat *u);
typedef void (APIENTRY * PFNGLEVALCOORD2DPROC) (GLdouble u, GLdouble v);
typedef void (APIENTRY * PFNGLEVALCOORD2DVPROC) (const GLdouble *u);
typedef void (APIENTRY * PFNGLEVALCOORD2FPROC) (GLfloat u, GLfloat v);
typedef void (APIENTRY * PFNGLEVALCOORD2FVPROC) (const GLfloat *u);
typedef void (APIENTRY * PFNGLEVALMESH1PROC) (GLenum mode, GLint i1, GLint i2);
typedef void (APIENTRY * PFNGLEVALMESH2PROC) (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
typedef void (APIENTRY * PFNGLEVALPOINT1PROC) (GLint i);
typedef void (APIENTRY * PFNGLEVALPOINT2PROC) (GLint i, GLint j);
typedef void (APIENTRY * PFNGLFEEDBACKBUFFERPROC) (GLsizei size, GLenum type, GLfloat *buffer);
typedef void (APIENTRY * PFNGLFINISHPROC) (void);
typedef void (APIENTRY * PFNGLFLUSHPROC) (void);
typedef void (APIENTRY * PFNGLFOGFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLFOGFVPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLFOGIPROC) (GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLFOGIVPROC) (GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLFRONTFACEPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLFRUSTUMPROC) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef GLuint (APIENTRY *PFNGLGENLISTSPROC) (GLsizei range);
typedef void (APIENTRY * PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef void (APIENTRY * PFNGLGETBOOLEANVPROC) (GLenum pname, GLboolean *params);
typedef void (APIENTRY * PFNGLGETCLIPPLANEPROC) (GLenum plane, GLdouble *equation);
typedef void (APIENTRY * PFNGLGETDOUBLEVPROC) (GLenum pname, GLdouble *params);
typedef GLenum (APIENTRY *PFNGLGETERRORPROC) (void);
typedef void (APIENTRY * PFNGLGETFLOATVPROC) (GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETINTEGERVPROC) (GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLGETLIGHTFVPROC) (GLenum light, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETLIGHTIVPROC) (GLenum light, GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLGETMAPDVPROC) (GLenum target, GLenum query, GLdouble *v);
typedef void (APIENTRY * PFNGLGETMAPFVPROC) (GLenum target, GLenum query, GLfloat *v);
typedef void (APIENTRY * PFNGLGETMAPIVPROC) (GLenum target, GLenum query, GLint *v);
typedef void (APIENTRY * PFNGLGETMATERIALFVPROC) (GLenum face, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETMATERIALIVPROC) (GLenum face, GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLGETPIXELMAPFVPROC) (GLenum map, GLfloat *values);
typedef void (APIENTRY * PFNGLGETPIXELMAPUIVPROC) (GLenum map, GLuint *values);
typedef void (APIENTRY * PFNGLGETPIXELMAPUSVPROC) (GLenum map, GLushort *values);
typedef void (APIENTRY * PFNGLGETPOINTERVPROC) (GLenum pname, GLvoid* *params);
typedef void (APIENTRY * PFNGLGETPOLYGONSTIPPLEPROC) (GLubyte *mask);
typedef const GLubyte * (APIENTRY *PFNGLGETSTRINGPROC) (GLenum name);
typedef void (APIENTRY * PFNGLGETTEXENVFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETTEXENVIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLGETTEXGENDVPROC) (GLenum coord, GLenum pname, GLdouble *params);
typedef void (APIENTRY * PFNGLGETTEXGENFVPROC) (GLenum coord, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETTEXGENIVPROC) (GLenum coord, GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLGETTEXIMAGEPROC) (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
typedef void (APIENTRY * PFNGLGETTEXLEVELPARAMETERFVPROC) (GLenum target, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETTEXLEVELPARAMETERIVPROC) (GLenum target, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLGETTEXPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLGETTEXPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (APIENTRY * PFNGLHINTPROC) (GLenum target, GLenum mode);
typedef void (APIENTRY * PFNGLINDEXMASKPROC) (GLuint mask);
typedef void (APIENTRY * PFNGLINDEXPOINTERPROC) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLINDEXDPROC) (GLdouble c);
typedef void (APIENTRY * PFNGLINDEXDVPROC) (const GLdouble *c);
typedef void (APIENTRY * PFNGLINDEXFPROC) (GLfloat c);
typedef void (APIENTRY * PFNGLINDEXFVPROC) (const GLfloat *c);
typedef void (APIENTRY * PFNGLINDEXIPROC) (GLint c);
typedef void (APIENTRY * PFNGLINDEXIVPROC) (const GLint *c);
typedef void (APIENTRY * PFNGLINDEXSPROC) (GLshort c);
typedef void (APIENTRY * PFNGLINDEXSVPROC) (const GLshort *c);
typedef void (APIENTRY * PFNGLINDEXUBPROC) (GLubyte c);
typedef void (APIENTRY * PFNGLINDEXUBVPROC) (const GLubyte *c);
typedef void (APIENTRY * PFNGLINITNAMESPROC) (void);
typedef void (APIENTRY * PFNGLINTERLEAVEDARRAYSPROC) (GLenum format, GLsizei stride, const GLvoid *pointer);
typedef GLboolean (APIENTRY *PFNGLISENABLEDPROC) (GLenum cap);
typedef GLboolean (APIENTRY *PFNGLISLISTPROC) (GLuint list);
typedef GLboolean (APIENTRY *PFNGLISTEXTUREPROC) (GLuint texture);
typedef void (APIENTRY * PFNGLLIGHTMODELFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLLIGHTMODELFVPROC) (GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLLIGHTMODELIPROC) (GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLLIGHTMODELIVPROC) (GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLLIGHTFPROC) (GLenum light, GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLLIGHTFVPROC) (GLenum light, GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLLIGHTIPROC) (GLenum light, GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLLIGHTIVPROC) (GLenum light, GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLLINESTIPPLEPROC) (GLint factor, GLushort pattern);
typedef void (APIENTRY * PFNGLLINEWIDTHPROC) (GLfloat width);
typedef void (APIENTRY * PFNGLLISTBASEPROC) (GLuint base);
typedef void (APIENTRY * PFNGLLOADIDENTITYPROC) (void);
typedef void (APIENTRY * PFNGLLOADMATRIXDPROC) (const GLdouble *m);
typedef void (APIENTRY * PFNGLLOADMATRIXFPROC) (const GLfloat *m);
typedef void (APIENTRY * PFNGLLOADNAMEPROC) (GLuint name);
typedef void (APIENTRY * PFNGLLOGICOPPROC) (GLenum opcode);
typedef void (APIENTRY * PFNGLMAP1DPROC) (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
typedef void (APIENTRY * PFNGLMAP1FPROC) (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
typedef void (APIENTRY * PFNGLMAP2DPROC) (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
typedef void (APIENTRY * PFNGLMAP2FPROC) (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
typedef void (APIENTRY * PFNGLMAPGRID1DPROC) (GLint un, GLdouble u1, GLdouble u2);
typedef void (APIENTRY * PFNGLMAPGRID1FPROC) (GLint un, GLfloat u1, GLfloat u2);
typedef void (APIENTRY * PFNGLMAPGRID2DPROC) (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
typedef void (APIENTRY * PFNGLMAPGRID2FPROC) (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
typedef void (APIENTRY * PFNGLMATERIALFPROC) (GLenum face, GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLMATERIALFVPROC) (GLenum face, GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLMATERIALIPROC) (GLenum face, GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLMATERIALIVPROC) (GLenum face, GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLMATRIXMODEPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLMULTMATRIXDPROC) (const GLdouble *m);
typedef void (APIENTRY * PFNGLMULTMATRIXFPROC) (const GLfloat *m);
typedef void (APIENTRY * PFNGLNEWLISTPROC) (GLuint list, GLenum mode);
typedef void (APIENTRY * PFNGLNORMAL3BPROC) (GLbyte nx, GLbyte ny, GLbyte nz);
typedef void (APIENTRY * PFNGLNORMAL3BVPROC) (const GLbyte *v);
typedef void (APIENTRY * PFNGLNORMAL3DPROC) (GLdouble nx, GLdouble ny, GLdouble nz);
typedef void (APIENTRY * PFNGLNORMAL3DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLNORMAL3FPROC) (GLfloat nx, GLfloat ny, GLfloat nz);
typedef void (APIENTRY * PFNGLNORMAL3FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLNORMAL3IPROC) (GLint nx, GLint ny, GLint nz);
typedef void (APIENTRY * PFNGLNORMAL3IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLNORMAL3SPROC) (GLshort nx, GLshort ny, GLshort nz);
typedef void (APIENTRY * PFNGLNORMAL3SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLNORMALPOINTERPROC) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLORTHOPROC) (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef void (APIENTRY * PFNGLPASSTHROUGHPROC) (GLfloat token);
typedef void (APIENTRY * PFNGLPIXELMAPFVPROC) (GLenum map, GLsizei mapsize, const GLfloat *values);
typedef void (APIENTRY * PFNGLPIXELMAPUIVPROC) (GLenum map, GLsizei mapsize, const GLuint *values);
typedef void (APIENTRY * PFNGLPIXELMAPUSVPROC) (GLenum map, GLsizei mapsize, const GLushort *values);
typedef void (APIENTRY * PFNGLPIXELSTOREFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLPIXELSTOREIPROC) (GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLPIXELTRANSFERFPROC) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLPIXELTRANSFERIPROC) (GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLPIXELZOOMPROC) (GLfloat xfactor, GLfloat yfactor);
typedef void (APIENTRY * PFNGLPOINTSIZEPROC) (GLfloat size);
typedef void (APIENTRY * PFNGLPOLYGONMODEPROC) (GLenum face, GLenum mode);
typedef void (APIENTRY * PFNGLPOLYGONOFFSETPROC) (GLfloat factor, GLfloat units);
typedef void (APIENTRY * PFNGLPOLYGONSTIPPLEPROC) (const GLubyte *mask);
typedef void (APIENTRY * PFNGLPOPATTRIBPROC) (void);
typedef void (APIENTRY * PFNGLPOPCLIENTATTRIBPROC) (void);
typedef void (APIENTRY * PFNGLPOPMATRIXPROC) (void);
typedef void (APIENTRY * PFNGLPOPNAMEPROC) (void);
typedef void (APIENTRY * PFNGLPRIORITIZETEXTURESPROC) (GLsizei n, const GLuint *textures, const GLclampf *priorities);
typedef void (APIENTRY * PFNGLPUSHATTRIBPROC) (GLbitfield mask);
typedef void (APIENTRY * PFNGLPUSHCLIENTATTRIBPROC) (GLbitfield mask);
typedef void (APIENTRY * PFNGLPUSHMATRIXPROC) (void);
typedef void (APIENTRY * PFNGLPUSHNAMEPROC) (GLuint name);
typedef void (APIENTRY * PFNGLRASTERPOS2DPROC) (GLdouble x, GLdouble y);
typedef void (APIENTRY * PFNGLRASTERPOS2DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLRASTERPOS2FPROC) (GLfloat x, GLfloat y);
typedef void (APIENTRY * PFNGLRASTERPOS2FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLRASTERPOS2IPROC) (GLint x, GLint y);
typedef void (APIENTRY * PFNGLRASTERPOS2IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLRASTERPOS2SPROC) (GLshort x, GLshort y);
typedef void (APIENTRY * PFNGLRASTERPOS2SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLRASTERPOS3DPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLRASTERPOS3DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLRASTERPOS3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLRASTERPOS3FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLRASTERPOS3IPROC) (GLint x, GLint y, GLint z);
typedef void (APIENTRY * PFNGLRASTERPOS3IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLRASTERPOS3SPROC) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY * PFNGLRASTERPOS3SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLRASTERPOS4DPROC) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * PFNGLRASTERPOS4DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLRASTERPOS4FPROC) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * PFNGLRASTERPOS4FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLRASTERPOS4IPROC) (GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRY * PFNGLRASTERPOS4IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLRASTERPOS4SPROC) (GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY * PFNGLRASTERPOS4SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLREADBUFFERPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLREADPIXELSPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
typedef void (APIENTRY * PFNGLRECTDPROC) (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
typedef void (APIENTRY * PFNGLRECTDVPROC) (const GLdouble *v1, const GLdouble *v2);
typedef void (APIENTRY * PFNGLRECTFPROC) (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
typedef void (APIENTRY * PFNGLRECTFVPROC) (const GLfloat *v1, const GLfloat *v2);
typedef void (APIENTRY * PFNGLRECTIPROC) (GLint x1, GLint y1, GLint x2, GLint y2);
typedef void (APIENTRY * PFNGLRECTIVPROC) (const GLint *v1, const GLint *v2);
typedef void (APIENTRY * PFNGLRECTSPROC) (GLshort x1, GLshort y1, GLshort x2, GLshort y2);
typedef void (APIENTRY * PFNGLRECTSVPROC) (const GLshort *v1, const GLshort *v2);
typedef GLint (APIENTRY *PFNGLRENDERMODEPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLROTATEDPROC) (GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLROTATEFPROC) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLSCALEDPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLSCALEFPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY * PFNGLSELECTBUFFERPROC) (GLsizei size, GLuint *buffer);
typedef void (APIENTRY * PFNGLSHADEMODELPROC) (GLenum mode);
typedef void (APIENTRY * PFNGLSTENCILFUNCPROC) (GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRY * PFNGLSTENCILMASKPROC) (GLuint mask);
typedef void (APIENTRY * PFNGLSTENCILOPPROC) (GLenum fail, GLenum zfail, GLenum zpass);
typedef void (APIENTRY * PFNGLTEXCOORD1DPROC) (GLdouble s);
typedef void (APIENTRY * PFNGLTEXCOORD1DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLTEXCOORD1FPROC) (GLfloat s);
typedef void (APIENTRY * PFNGLTEXCOORD1FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLTEXCOORD1IPROC) (GLint s);
typedef void (APIENTRY * PFNGLTEXCOORD1IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLTEXCOORD1SPROC) (GLshort s);
typedef void (APIENTRY * PFNGLTEXCOORD1SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLTEXCOORD2DPROC) (GLdouble s, GLdouble t);
typedef void (APIENTRY * PFNGLTEXCOORD2DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLTEXCOORD2FPROC) (GLfloat s, GLfloat t);
typedef void (APIENTRY * PFNGLTEXCOORD2FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLTEXCOORD2IPROC) (GLint s, GLint t);
typedef void (APIENTRY * PFNGLTEXCOORD2IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLTEXCOORD2SPROC) (GLshort s, GLshort t);
typedef void (APIENTRY * PFNGLTEXCOORD2SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLTEXCOORD3DPROC) (GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRY * PFNGLTEXCOORD3DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLTEXCOORD3FPROC) (GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRY * PFNGLTEXCOORD3FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLTEXCOORD3IPROC) (GLint s, GLint t, GLint r);
typedef void (APIENTRY * PFNGLTEXCOORD3IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLTEXCOORD3SPROC) (GLshort s, GLshort t, GLshort r);
typedef void (APIENTRY * PFNGLTEXCOORD3SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLTEXCOORD4DPROC) (GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRY * PFNGLTEXCOORD4DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLTEXCOORD4FPROC) (GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRY * PFNGLTEXCOORD4FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLTEXCOORD4IPROC) (GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRY * PFNGLTEXCOORD4IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLTEXCOORD4SPROC) (GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRY * PFNGLTEXCOORD4SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLTEXCOORDPOINTERPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLTEXENVFPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLTEXENVFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLTEXENVIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLTEXENVIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLTEXGENDPROC) (GLenum coord, GLenum pname, GLdouble param);
typedef void (APIENTRY * PFNGLTEXGENDVPROC) (GLenum coord, GLenum pname, const GLdouble *params);
typedef void (APIENTRY * PFNGLTEXGENFPROC) (GLenum coord, GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLTEXGENFVPROC) (GLenum coord, GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLTEXGENIPROC) (GLenum coord, GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLTEXGENIVPROC) (GLenum coord, GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLTEXIMAGE1DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY * PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY * PFNGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLTEXPARAMETERFVPROC) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (APIENTRY * PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRY * PFNGLTEXPARAMETERIVPROC) (GLenum target, GLenum pname, const GLint *params);
typedef void (APIENTRY * PFNGLTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY * PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY * PFNGLTRANSLATEDPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLTRANSLATEFPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLVERTEX2DPROC) (GLdouble x, GLdouble y);
typedef void (APIENTRY * PFNGLVERTEX2DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLVERTEX2FPROC) (GLfloat x, GLfloat y);
typedef void (APIENTRY * PFNGLVERTEX2FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLVERTEX2IPROC) (GLint x, GLint y);
typedef void (APIENTRY * PFNGLVERTEX2IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLVERTEX2SPROC) (GLshort x, GLshort y);
typedef void (APIENTRY * PFNGLVERTEX2SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLVERTEX3DPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLVERTEX3DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLVERTEX3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLVERTEX3FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLVERTEX3IPROC) (GLint x, GLint y, GLint z);
typedef void (APIENTRY * PFNGLVERTEX3IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLVERTEX3SPROC) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY * PFNGLVERTEX3SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLVERTEX4DPROC) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * PFNGLVERTEX4DVPROC) (const GLdouble *v);
typedef void (APIENTRY * PFNGLVERTEX4FPROC) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * PFNGLVERTEX4FVPROC) (const GLfloat *v);
typedef void (APIENTRY * PFNGLVERTEX4IPROC) (GLint x, GLint y, GLint z, GLint w);
typedef void (APIENTRY * PFNGLVERTEX4IVPROC) (const GLint *v);
typedef void (APIENTRY * PFNGLVERTEX4SPROC) (GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY * PFNGLVERTEX4SVPROC) (const GLshort *v);
typedef void (APIENTRY * PFNGLVERTEXPOINTERPROC) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);

extern PFNGLACCUMPROC glAccum;
extern PFNGLALPHAFUNCPROC glAlphaFunc;
extern PFNGLARETEXTURESRESIDENTPROC glAreTexturesResident;
extern PFNGLARRAYELEMENTPROC glArrayElement;
extern PFNGLBEGINPROC glBegin;
extern PFNGLBINDTEXTUREPROC glBindTexture;
extern PFNGLBITMAPPROC glBitmap;
extern PFNGLBLENDFUNCPROC glBlendFunc;
extern PFNGLCALLLISTPROC glCallList;
extern PFNGLCALLLISTSPROC glCallLists;
extern PFNGLCLEARPROC glClear;
extern PFNGLCLEARACCUMPROC glClearAccum;
extern PFNGLCLEARCOLORPROC glClearColor;
extern PFNGLCLEARDEPTHPROC glClearDepth;
extern PFNGLCLEARINDEXPROC glClearIndex;
extern PFNGLCLEARSTENCILPROC glClearStencil;
extern PFNGLCLIPPLANEPROC glClipPlane;
extern PFNGLCOLOR3BPROC glColor3b;
extern PFNGLCOLOR3BVPROC glColor3bv;
extern PFNGLCOLOR3DPROC glColor3d;
extern PFNGLCOLOR3DVPROC glColor3dv;
extern PFNGLCOLOR3FPROC glColor3f;
extern PFNGLCOLOR3FVPROC glColor3fv;
extern PFNGLCOLOR3IPROC glColor3i;
extern PFNGLCOLOR3IVPROC glColor3iv;
extern PFNGLCOLOR3SPROC glColor3s;
extern PFNGLCOLOR3SVPROC glColor3sv;
extern PFNGLCOLOR3UBPROC glColor3ub;
extern PFNGLCOLOR3UBVPROC glColor3ubv;
extern PFNGLCOLOR3UIPROC glColor3ui;
extern PFNGLCOLOR3UIVPROC glColor3uiv;
extern PFNGLCOLOR3USPROC glColor3us;
extern PFNGLCOLOR3USVPROC glColor3usv;
extern PFNGLCOLOR4BPROC glColor4b;
extern PFNGLCOLOR4BVPROC glColor4bv;
extern PFNGLCOLOR4DPROC glColor4d;
extern PFNGLCOLOR4DVPROC glColor4dv;
extern PFNGLCOLOR4FPROC glColor4f;
extern PFNGLCOLOR4FVPROC glColor4fv;
extern PFNGLCOLOR4IPROC glColor4i;
extern PFNGLCOLOR4IVPROC glColor4iv;
extern PFNGLCOLOR4SPROC glColor4s;
extern PFNGLCOLOR4SVPROC glColor4sv;
extern PFNGLCOLOR4UBPROC glColor4ub;
extern PFNGLCOLOR4UBVPROC glColor4ubv;
extern PFNGLCOLOR4UIPROC glColor4ui;
extern PFNGLCOLOR4UIVPROC glColor4uiv;
extern PFNGLCOLOR4USPROC glColor4us;
extern PFNGLCOLOR4USVPROC glColor4usv;
extern PFNGLCOLORMASKPROC glColorMask;
extern PFNGLCOLORMATERIALPROC glColorMaterial;
extern PFNGLCOLORPOINTERPROC glColorPointer;
extern PFNGLCOPYPIXELSPROC glCopyPixels;
extern PFNGLCOPYTEXIMAGE1DPROC glCopyTexImage1D;
extern PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
extern PFNGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D;
extern PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
extern PFNGLCULLFACEPROC glCullFace;
extern PFNGLDELETELISTSPROC glDeleteLists;
extern PFNGLDELETETEXTURESPROC glDeleteTextures;
extern PFNGLDEPTHFUNCPROC glDepthFunc;
extern PFNGLDEPTHMASKPROC glDepthMask;
extern PFNGLDEPTHRANGEPROC glDepthRange;
extern PFNGLDISABLEPROC glDisable;
extern PFNGLDISABLECLIENTSTATEPROC glDisableClientState;
extern PFNGLDRAWARRAYSPROC glDrawArrays;
extern PFNGLDRAWBUFFERPROC glDrawBuffer;
extern PFNGLDRAWELEMENTSPROC glDrawElements;
extern PFNGLDRAWPIXELSPROC glDrawPixels;
extern PFNGLEDGEFLAGPROC glEdgeFlag;
extern PFNGLEDGEFLAGPOINTERPROC glEdgeFlagPointer;
extern PFNGLEDGEFLAGVPROC glEdgeFlagv;
extern PFNGLENABLEPROC glEnable;
extern PFNGLENABLECLIENTSTATEPROC glEnableClientState;
extern PFNGLENDPROC glEnd;
extern PFNGLENDLISTPROC glEndList;
extern PFNGLEVALCOORD1DPROC glEvalCoord1d;
extern PFNGLEVALCOORD1DVPROC glEvalCoord1dv;
extern PFNGLEVALCOORD1FPROC glEvalCoord1f;
extern PFNGLEVALCOORD1FVPROC glEvalCoord1fv;
extern PFNGLEVALCOORD2DPROC glEvalCoord2d;
extern PFNGLEVALCOORD2DVPROC glEvalCoord2dv;
extern PFNGLEVALCOORD2FPROC glEvalCoord2f;
extern PFNGLEVALCOORD2FVPROC glEvalCoord2fv;
extern PFNGLEVALMESH1PROC glEvalMesh1;
extern PFNGLEVALMESH2PROC glEvalMesh2;
extern PFNGLEVALPOINT1PROC glEvalPoint1;
extern PFNGLEVALPOINT2PROC glEvalPoint2;
extern PFNGLFEEDBACKBUFFERPROC glFeedbackBuffer;
extern PFNGLFINISHPROC glFinish;
extern PFNGLFLUSHPROC glFlush;
extern PFNGLFOGFPROC glFogf;
extern PFNGLFOGFVPROC glFogfv;
extern PFNGLFOGIPROC glFogi;
extern PFNGLFOGIVPROC glFogiv;
extern PFNGLFRONTFACEPROC glFrontFace;
extern PFNGLFRUSTUMPROC glFrustum;
extern PFNGLGENLISTSPROC glGenLists;
extern PFNGLGENTEXTURESPROC glGenTextures;
extern PFNGLGETBOOLEANVPROC glGetBooleanv;
extern PFNGLGETCLIPPLANEPROC glGetClipPlane;
extern PFNGLGETDOUBLEVPROC glGetDoublev;
extern PFNGLGETERRORPROC glGetError;
extern PFNGLGETFLOATVPROC glGetFloatv;
extern PFNGLGETINTEGERVPROC glGetIntegerv;
extern PFNGLGETLIGHTFVPROC glGetLightfv;
extern PFNGLGETLIGHTIVPROC glGetLightiv;
extern PFNGLGETMAPDVPROC glGetMapdv;
extern PFNGLGETMAPFVPROC glGetMapfv;
extern PFNGLGETMAPIVPROC glGetMapiv;
extern PFNGLGETMATERIALFVPROC glGetMaterialfv;
extern PFNGLGETMATERIALIVPROC glGetMaterialiv;
extern PFNGLGETPIXELMAPFVPROC glGetPixelMapfv;
extern PFNGLGETPIXELMAPUIVPROC glGetPixelMapuiv;
extern PFNGLGETPIXELMAPUSVPROC glGetPixelMapusv;
extern PFNGLGETPOINTERVPROC glGetPointer;
extern PFNGLGETPOLYGONSTIPPLEPROC glGetPolygonStipple;
extern PFNGLGETSTRINGPROC glGetString;
extern PFNGLGETTEXENVFVPROC glGetTexEnvfv;
extern PFNGLGETTEXENVIVPROC glGetTexEnviv;
extern PFNGLGETTEXGENDVPROC glGetTexGendv;
extern PFNGLGETTEXGENFVPROC glGetTexGenfv;
extern PFNGLGETTEXGENIVPROC glGetTexGeniv;
extern PFNGLGETTEXIMAGEPROC glGetTexImage;
extern PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv;
extern PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv;
extern PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
extern PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
extern PFNGLHINTPROC glHint;
extern PFNGLINDEXMASKPROC glIndexMask;
extern PFNGLINDEXPOINTERPROC glIndexPointer;
extern PFNGLINDEXDPROC glIndexd;
extern PFNGLINDEXDVPROC glIndexdv;
extern PFNGLINDEXFPROC glIndexf;
extern PFNGLINDEXFVPROC glIndexfv;
extern PFNGLINDEXIPROC glIndexi;
extern PFNGLINDEXIVPROC glIndexiv;
extern PFNGLINDEXSPROC glIndexs;
extern PFNGLINDEXSVPROC glIndexsv;
extern PFNGLINDEXUBPROC glIndexub;
extern PFNGLINDEXUBVPROC glIndexubv;
extern PFNGLINITNAMESPROC glInitNames;
extern PFNGLINTERLEAVEDARRAYSPROC glInterleavedArrays;
extern PFNGLISENABLEDPROC glIsEnabled;
extern PFNGLISLISTPROC glIsList;
extern PFNGLISTEXTUREPROC glIsTexture;
extern PFNGLLIGHTMODELFPROC glLightModelf;
extern PFNGLLIGHTMODELFVPROC glLightModelfv;
extern PFNGLLIGHTMODELIPROC glLightModeli;
extern PFNGLLIGHTMODELIVPROC glLightModeliv;
extern PFNGLLIGHTFPROC glLightf;
extern PFNGLLIGHTFVPROC glLightfv;
extern PFNGLLIGHTIPROC glLighti;
extern PFNGLLIGHTIVPROC glLightiv;
extern PFNGLLINESTIPPLEPROC glLineStipple;
extern PFNGLLINEWIDTHPROC glLineWidth;
extern PFNGLLISTBASEPROC glListBase;
extern PFNGLLOADIDENTITYPROC glLoadIdentity;
extern PFNGLLOADMATRIXDPROC glLoadMatrixd;
extern PFNGLLOADMATRIXFPROC glLoadMatrixf;
extern PFNGLLOADNAMEPROC glLoadName;
extern PFNGLLOGICOPPROC glLogicOp;
extern PFNGLMAP1DPROC glMap1d;
extern PFNGLMAP1FPROC glMap1f;
extern PFNGLMAP2DPROC glMap2d;
extern PFNGLMAP2FPROC glMap2f;
extern PFNGLMAPGRID1DPROC glMapGrid1d;
extern PFNGLMAPGRID1FPROC glMapGrid1f;
extern PFNGLMAPGRID2DPROC glMapGrid2d;
extern PFNGLMAPGRID2FPROC glMapGrid2f;
extern PFNGLMATERIALFPROC glMaterialf;
extern PFNGLMATERIALFVPROC glMaterialfv;
extern PFNGLMATERIALIPROC glMateriali;
extern PFNGLMATERIALIVPROC glMaterialiv;
extern PFNGLMATRIXMODEPROC glMatrixMode;
extern PFNGLMULTMATRIXDPROC glMultMatrixd;
extern PFNGLMULTMATRIXFPROC glMultMatrixf;
extern PFNGLNEWLISTPROC glNewList;
extern PFNGLNORMAL3BPROC glNormal3b;
extern PFNGLNORMAL3BVPROC glNormal3bv;
extern PFNGLNORMAL3DPROC glNormal3d;
extern PFNGLNORMAL3DVPROC glNormal3dv;
extern PFNGLNORMAL3FPROC glNormal3f;
extern PFNGLNORMAL3FVPROC glNormal3fv;
extern PFNGLNORMAL3IPROC glNormal3i;
extern PFNGLNORMAL3IVPROC glNormal3iv;
extern PFNGLNORMAL3SPROC glNormal3s;
extern PFNGLNORMAL3SVPROC glNormal3sv;
extern PFNGLNORMALPOINTERPROC glNormalPointer;
extern PFNGLORTHOPROC glOrtho;
extern PFNGLPASSTHROUGHPROC glPassThrough;
extern PFNGLPIXELMAPFVPROC glPixelMapfv;
extern PFNGLPIXELMAPUIVPROC glPixelMapfuiv;
extern PFNGLPIXELMAPUSVPROC glPixelMapfusv;
extern PFNGLPIXELSTOREFPROC glPixelStoref;
extern PFNGLPIXELSTOREIPROC glPixelStorei;
extern PFNGLPIXELTRANSFERFPROC glPixelTransferf;
extern PFNGLPIXELTRANSFERIPROC glPixelTransferi;
extern PFNGLPIXELZOOMPROC glPixelZoom;
extern PFNGLPOINTSIZEPROC glPointSize;
extern PFNGLPOLYGONMODEPROC glPolygonMode;
extern PFNGLPOLYGONOFFSETPROC glPolygonOffset;
extern PFNGLPOLYGONSTIPPLEPROC glPolygonStipple;
extern PFNGLPOPATTRIBPROC glPopAttrib;
extern PFNGLPOPCLIENTATTRIBPROC glPopClientAttrib;
extern PFNGLPOPMATRIXPROC glPopMatrix;
extern PFNGLPOPNAMEPROC glPopName;
extern PFNGLPRIORITIZETEXTURESPROC glPrioritizeTextures;
extern PFNGLPUSHATTRIBPROC glPushAttrib;
extern PFNGLPUSHCLIENTATTRIBPROC glPushClientAttrib;
extern PFNGLPUSHMATRIXPROC glPushMatrix;
extern PFNGLPUSHNAMEPROC glPushName;
extern PFNGLRASTERPOS2DPROC glRasterPos2d;
extern PFNGLRASTERPOS2DVPROC glRasterPos2dv;
extern PFNGLRASTERPOS2FPROC glRasterPos2f;
extern PFNGLRASTERPOS2FVPROC glRasterPos2fv;
extern PFNGLRASTERPOS2IPROC glRasterPos2i;
extern PFNGLRASTERPOS2IVPROC glRasterPos2iv;
extern PFNGLRASTERPOS2SPROC glRasterPos2s;
extern PFNGLRASTERPOS2SVPROC glRasterPos2sv;
extern PFNGLRASTERPOS3DPROC glRasterPos3d;
extern PFNGLRASTERPOS3DVPROC glRasterPos3dv;
extern PFNGLRASTERPOS3FPROC glRasterPos3f;
extern PFNGLRASTERPOS3FVPROC glRasterPos3fv;
extern PFNGLRASTERPOS3IPROC glRasterPos3i;
extern PFNGLRASTERPOS3IVPROC glRasterPos3iv;
extern PFNGLRASTERPOS3SPROC glRasterPos3s;
extern PFNGLRASTERPOS3SVPROC glRasterPos3sv;
extern PFNGLRASTERPOS4DPROC glRasterPos4d;
extern PFNGLRASTERPOS4DVPROC glRasterPos4dv;
extern PFNGLRASTERPOS4FPROC glRasterPos4f;
extern PFNGLRASTERPOS4FVPROC glRasterPos4fv;
extern PFNGLRASTERPOS4IPROC glRasterPos4i;
extern PFNGLRASTERPOS4IVPROC glRasterPos4iv;
extern PFNGLRASTERPOS4SPROC glRasterPos4s;
extern PFNGLRASTERPOS4SVPROC glRasterPos4sv;
extern PFNGLREADBUFFERPROC glReadBuffer;
extern PFNGLREADPIXELSPROC glReadPixels;
extern PFNGLRECTDPROC glRectd;
extern PFNGLRECTDVPROC glRectdv;
extern PFNGLRECTFPROC glRectf;
extern PFNGLRECTFVPROC glRectfv;
extern PFNGLRECTIPROC glRecti;
extern PFNGLRECTIVPROC glRectiv;
extern PFNGLRECTSPROC glRects;
extern PFNGLRECTSVPROC glRectsv;
extern PFNGLRENDERMODEPROC glRenderMode;
extern PFNGLROTATEDPROC glRotated;
extern PFNGLROTATEFPROC glRotatef;
extern PFNGLSCALEDPROC glScaled;
extern PFNGLSCALEFPROC glScalef;
extern PFNGLSCISSORPROC glScissor;
extern PFNGLSELECTBUFFERPROC glSelectBuffer;
extern PFNGLSHADEMODELPROC glShadeModel;
extern PFNGLSTENCILFUNCPROC glStencilFunc;
extern PFNGLSTENCILMASKPROC glStencilMask;
extern PFNGLSTENCILOPPROC glStencilOp;
extern PFNGLTEXCOORD1DPROC glTexCoord1d;
extern PFNGLTEXCOORD1DVPROC glTexCoord1dv;
extern PFNGLTEXCOORD1FPROC glTexCoord1f;
extern PFNGLTEXCOORD1FVPROC glTexCoord1fv;
extern PFNGLTEXCOORD1IPROC glTexCoord1i;
extern PFNGLTEXCOORD1IVPROC glTexCoord1iv;
extern PFNGLTEXCOORD1SPROC glTexCoord1s;
extern PFNGLTEXCOORD1SVPROC glTexCoord1sv;
extern PFNGLTEXCOORD2DPROC glTexCoord2d;
extern PFNGLTEXCOORD2DVPROC glTexCoord2dv;
extern PFNGLTEXCOORD2FPROC glTexCoord2f;
extern PFNGLTEXCOORD2FVPROC glTexCoord2fv;
extern PFNGLTEXCOORD2IPROC glTexCoord2i;
extern PFNGLTEXCOORD2IVPROC glTexCoord2iv;
extern PFNGLTEXCOORD2SPROC glTexCoord2s;
extern PFNGLTEXCOORD2SVPROC glTexCoord2sv;
extern PFNGLTEXCOORD3DPROC glTexCoord3d;
extern PFNGLTEXCOORD3DVPROC glTexCoord3dv;
extern PFNGLTEXCOORD3FPROC glTexCoord3f;
extern PFNGLTEXCOORD3FVPROC glTexCoord3fv;
extern PFNGLTEXCOORD3IPROC glTexCoord3i;
extern PFNGLTEXCOORD3IVPROC glTexCoord3iv;
extern PFNGLTEXCOORD3SPROC glTexCoord3s;
extern PFNGLTEXCOORD3SVPROC glTexCoord3sv;
extern PFNGLTEXCOORD4DPROC glTexCoord4d;
extern PFNGLTEXCOORD4DVPROC glTexCoord4dv;
extern PFNGLTEXCOORD4FPROC glTexCoord4f;
extern PFNGLTEXCOORD4FVPROC glTexCoord4fv;
extern PFNGLTEXCOORD4IPROC glTexCoord4i;
extern PFNGLTEXCOORD4IVPROC glTexCoord4iv;
extern PFNGLTEXCOORD4SPROC glTexCoord4s;
extern PFNGLTEXCOORD4SVPROC glTexCoord4sv;
extern PFNGLTEXCOORDPOINTERPROC glTexCoordPointer;
extern PFNGLTEXENVFPROC glTexEnvf;
extern PFNGLTEXENVFVPROC glTexEnvfv;
extern PFNGLTEXENVIPROC glTexEnvi;
extern PFNGLTEXENVIVPROC glTexEnviv;
extern PFNGLTEXGENDPROC glTexGend;
extern PFNGLTEXGENDVPROC glTexGendv;
extern PFNGLTEXGENFPROC glTexGenf;
extern PFNGLTEXGENFVPROC glTexGenfv;
extern PFNGLTEXGENIPROC glTexGeni;
extern PFNGLTEXGENIVPROC glTexGeniv;
extern PFNGLTEXIMAGE1DPROC glTexImage1D;
extern PFNGLTEXIMAGE2DPROC glTexImage2D;
extern PFNGLTEXPARAMETERFPROC glTexParameterf;
extern PFNGLTEXPARAMETERFVPROC glTexParameterfv;
extern PFNGLTEXPARAMETERIPROC glTexParameteri;
extern PFNGLTEXPARAMETERIVPROC glTexParameteriv;
extern PFNGLTEXSUBIMAGE1DPROC glTexSubImage1D;
extern PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
extern PFNGLTRANSLATEDPROC glTranslated;
extern PFNGLTRANSLATEFPROC glTranslatef;
extern PFNGLVERTEX2DPROC glVertex2d;
extern PFNGLVERTEX2DVPROC glVertex2dv;
extern PFNGLVERTEX2FPROC glVertex2f;
extern PFNGLVERTEX2FVPROC glVertex2fv;
extern PFNGLVERTEX2IPROC glVertex2i;
extern PFNGLVERTEX2IVPROC glVertex2iv;
extern PFNGLVERTEX2SPROC glVertex2s;
extern PFNGLVERTEX2SVPROC glVertex2sv;
extern PFNGLVERTEX3DPROC glVertex3d;
extern PFNGLVERTEX3DVPROC glVertex3dv;
extern PFNGLVERTEX3FPROC glVertex3f;
extern PFNGLVERTEX3FVPROC glVertex3fv;
extern PFNGLVERTEX3IPROC glVertex3i;
extern PFNGLVERTEX3IVPROC glVertex3iv;
extern PFNGLVERTEX3SPROC glVertex3s;
extern PFNGLVERTEX3SVPROC glVertex3sv;
extern PFNGLVERTEX4DPROC glVertex4d;
extern PFNGLVERTEX4DVPROC glVertex4dv;
extern PFNGLVERTEX4FPROC glVertex4f;
extern PFNGLVERTEX4FVPROC glVertex4fv;
extern PFNGLVERTEX4IPROC glVertex4i;
extern PFNGLVERTEX4IVPROC glVertex4iv;
extern PFNGLVERTEX4SPROC glVertex4s;
extern PFNGLVERTEX4SVPROC glVertex4sv;
extern PFNGLVERTEXPOINTERPROC glVertexPointer;
extern PFNGLVIEWPORTPROC glViewport;

#include <GL/glext.h>
#include <GL/wglext.h>

// OpenGL 1.2
extern PFNGLBLENDCOLORPROC glBlendColor;
extern PFNGLBLENDEQUATIONPROC glBlendEquation;
extern PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
extern PFNGLTEXIMAGE3DPROC glTexImage3D;
extern PFNGLTEXSUBIMAGE3DPROC glTexSumImage3D;
extern PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;

// OpenGL 1.3
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLSAMPLECOVERAGEPROC glSampleCoverage;
extern PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
extern PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
extern PFNGLCOMPRESSEDTEXIMAGE1DPROC glCompressedTexImage1D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D;
extern PFNGLGETCOMPRESSEDTEXIMAGEPROC glGetCompressedTexImage;

// OpenGL 1.4
extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
extern PFNGLMULTIDRAWARRAYSPROC glMultiDrawArrays;
extern PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements;
extern PFNGLPOINTPARAMETERFPROC glPointParameterf;
extern PFNGLPOINTPARAMETERFVPROC glPointParameterfv;
extern PFNGLPOINTPARAMETERIPROC glPointParameteri;
extern PFNGLPOINTPARAMETERIVPROC glPointParameteriv;

// OpenGL 1.5
extern PFNGLGENQUERIESPROC glGenQueries;
extern PFNGLDELETEQUERIESPROC glDeleteQueries;
extern PFNGLISQUERYPROC glIsQuery;
extern PFNGLBEGINQUERYPROC glBeginQuery;
extern PFNGLENDQUERYPROC glEndQuery;
extern PFNGLGETQUERYIVPROC glGetQueryiv;
extern PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
extern PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLISBUFFERPROC glIsBuffer;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
extern PFNGLMAPBUFFERPROC glMapBuffer;
extern PFNGLUNMAPBUFFERPROC glUnmapBuffer;
extern PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
extern PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv;

// OpenGL 2.0
extern PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
extern PFNGLDRAWBUFFERSPROC glDrawBuffers;
extern PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
extern PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
extern PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLDETACHSHADERPROC glDetachShader;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
extern PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
extern PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLGETSHADERSOURCEPROC glGetShaderSource;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLGETUNIFORMFVPROC glGetUniformfv;
extern PFNGLGETUNIFORMIVPROC glGetUniformiv;
extern PFNGLGETVERTEXATTRIBDVPROC glGetVertexAttribdv;
extern PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv;
extern PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;
extern PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointer;
extern PFNGLISPROGRAMPROC glIsProgram;
extern PFNGLISSHADERPROC glIsShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM3FPROC glUniform3f;
extern PFNGLUNIFORM4FPROC glUniform4f;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM2IPROC glUniform2i;
extern PFNGLUNIFORM3IPROC glUniform3i;
extern PFNGLUNIFORM4IPROC glUniform4i;
extern PFNGLUNIFORM1FVPROC glUniform1fv;
extern PFNGLUNIFORM2FVPROC glUniform2fv;
extern PFNGLUNIFORM3FVPROC glUniform3fv;
extern PFNGLUNIFORM4FVPROC glUniform4fv;
extern PFNGLUNIFORM1IVPROC glUniform1iv;
extern PFNGLUNIFORM2IVPROC glUniform2iv;
extern PFNGLUNIFORM3IVPROC glUniform3iv;
extern PFNGLUNIFORM4IVPROC glUniform4iv;
extern PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
extern PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLVALIDATEPROGRAMPROC glValidateProgram;
extern PFNGLVERTEXATTRIB1DPROC glVertexAttrib1d;
extern PFNGLVERTEXATTRIB1DVPROC glVertexAttrib1dv;
extern PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
extern PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
extern PFNGLVERTEXATTRIB1SPROC glVertexAttrib1s;
extern PFNGLVERTEXATTRIB1SVPROC glVertexAttrib1sv;
extern PFNGLVERTEXATTRIB2DPROC glVertexAttrib2d;
extern PFNGLVERTEXATTRIB2DVPROC glVertexAttrib2dv;
extern PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;
extern PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
extern PFNGLVERTEXATTRIB2SPROC glVertexAttrib2s;
extern PFNGLVERTEXATTRIB2SVPROC glVertexAttrib2sv;
extern PFNGLVERTEXATTRIB3DPROC glVertexAttrib3d;
extern PFNGLVERTEXATTRIB3DVPROC glVertexAttrib3dv;
extern PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;
extern PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
extern PFNGLVERTEXATTRIB3SPROC glVertexAttrib3s;
extern PFNGLVERTEXATTRIB3SVPROC glVertexAttrib3sv;
extern PFNGLVERTEXATTRIB4NBVPROC glVertexAttrib4nbv;
extern PFNGLVERTEXATTRIB4NIVPROC glVertexAttrib4niv;
extern PFNGLVERTEXATTRIB4NSVPROC glVertexAttrib4nsv;
extern PFNGLVERTEXATTRIB4NUBPROC glVertexAttrib4nub;
extern PFNGLVERTEXATTRIB4NUBVPROC glVertexAttrib4nubv;
extern PFNGLVERTEXATTRIB4NUIVPROC glVertexAttrib4nuiv;
extern PFNGLVERTEXATTRIB4NUSVPROC glVertexAttrib4nusv;
extern PFNGLVERTEXATTRIB4BVPROC glVertexAttrib4bv;
extern PFNGLVERTEXATTRIB4DPROC glVertexAttrib4d;
extern PFNGLVERTEXATTRIB4DVPROC glVertexAttrib4dv;
extern PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;
extern PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
extern PFNGLVERTEXATTRIB4IVPROC glVertexAttrib4iv;
extern PFNGLVERTEXATTRIB4SPROC glVertexAttrib4s;
extern PFNGLVERTEXATTRIB4SVPROC glVertexAttrib4sv;
extern PFNGLVERTEXATTRIB4UBVPROC glVertexAttrib4ubv;
extern PFNGLVERTEXATTRIB4UIVPROC glVertexAttrib4uiv;
extern PFNGLVERTEXATTRIB4USVPROC glVertexAttrib4usv;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

// OpenGL 2.1
extern PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv;
extern PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv;
extern PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv;
extern PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv;
extern PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
extern PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv;

// GL_ARB_framebuffer_object
extern PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
extern PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;
extern PFNGLISFRAMEBUFFERPROC glIsFramebuffer;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
extern PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
extern PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
extern PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;

// GL_ARB_map_buffer_range
extern PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
extern PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange;

// GL_ARB_instanced_arrays
extern PFNGLVERTEXATTRIBDIVISORARBPROC glVertexAttribDivisorARB;

// GL_ARB_draw_instanced
extern PFNGLDRAWARRAYSINSTANCEDARBPROC glDrawArraysInstancedARB;
extern PFNGLDRAWELEMENTSINSTANCEDARBPROC glDrawElementsInstancedARB;

void LoadWindowsOpenGL();

#endif // _WINDOWS

#endif // _INC_OPENGLWINDOWSLOADER
