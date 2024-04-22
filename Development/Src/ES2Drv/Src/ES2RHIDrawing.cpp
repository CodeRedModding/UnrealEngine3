/*=============================================================================
 ES2RHIImplementation.cpp: OpenGL ES 2.0 RHI definitions.
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Engine.h"

// By default, don't plan to time the next draw call. This can be set to TRUE
// after a shader compile so we can accurately measure performance of the
// warm up draw call, which is thought to "finish" the compile
STAT(UBOOL GES2TimeNextDrawCall = FALSE);

#include "ES2RHIPrivate.h"

#if WITH_ES2_RHI

#if _WINDOWS
	// @TODO: Clean up or remove
	extern PFNGLDEPTHRANGEPROC glDepthRangef;
	extern PFNGLCLEARDEPTHPROC glClearDepthf;
#endif

/**
 * Map the Unreal semantic to a hardcoded location that we will
 * bind it to in GL. They will map to:
 *		Position [0]
 *		BlendWeight [1]
 *		Normal [2]
 *		Color0 [3]
 *		Color1 [4]
 *		Tangent [5]
 *		Binormal [6]
 *		BlendIndices [7]
 *		TextureCoord0 [8]
 *		..
 *		TextureCoord7 [15]
 */
DWORD TranslateUnrealUsageToBindLocation(DWORD Usage)
{
	switch (Usage)
	{
	case VEU_Position:				return 0;
	case VEU_TextureCoordinate:		return 8;
	case VEU_BlendWeight:			return 1;
	case VEU_BlendIndices:			return 7;
	case VEU_Normal:				return 2;
	case VEU_Tangent:				return 5;
	case VEU_Binormal:				return 6;
	case VEU_Color:					return 3;
	default:						return 0;
	}
};

/**
 * Maps the unreal attribute type to how many floats it uses
 */
static inline DWORD TranslateUnrealTypeToCount(DWORD Type)
{
	switch (Type)
	{
	case VET_Float1:				return 1;
	case VET_Float2:				return 2;
	case VET_Float3:				return 3;
	case VET_Float4:				return 4;
	case VET_PackedNormal:			return 4;
	case VET_UByte4:				return 4;
	case VET_UByte4N:				return 4;
	case VET_Color:					return 4;
	case VET_Short2:				return 2;
	case VET_Short2N:				return 2;
	case VET_Half2:					return 2;
	default: appErrorf(TEXT("VertexElementType %d is not supported in ES2")); return 0;
	}
};

/**
 * Maps the unreal attribute type to the GL format it uses
 */
static inline GLint TranslateUnrealTypeToGLFormat(DWORD Type)
{
	switch (Type)
	{
	case VET_Float1:				return GL_FLOAT;
	case VET_Float2:				return GL_FLOAT;
	case VET_Float3:				return GL_FLOAT;
	case VET_Float4:				return GL_FLOAT;
	case VET_PackedNormal:			return GL_UNSIGNED_BYTE;
	case VET_UByte4:				return GL_UNSIGNED_BYTE;
	case VET_UByte4N:				return GL_UNSIGNED_BYTE;
	case VET_Color:					return GL_UNSIGNED_BYTE;
	case VET_Short2:				return GL_SHORT;
	case VET_Short2N:				return GL_SHORT;
	case VET_Half2:					return GL_HALF_FLOAT_OES;
	default: appErrorf(TEXT("VertexElementType %d is not supported in ES2")); return 0;
	}
}

/**
 * Maps the unreal attribute type to whether or not it needs normalization
 */
static inline GLint TranslateUnrealTypeToNormalization(DWORD Type)
{
	switch (Type)
	{
	case VET_Float1:				return GL_FALSE;
	case VET_Float2:				return GL_FALSE;
	case VET_Float3:				return GL_FALSE;
	case VET_Float4:				return GL_FALSE;
	case VET_PackedNormal:			return GL_FALSE;
	case VET_UByte4:				return GL_FALSE;
	case VET_UByte4N:				return GL_TRUE;
	case VET_Color:					return GL_TRUE;
	case VET_Short2:				return GL_FALSE;
	case VET_Short2N:				return GL_TRUE;
	case VET_Half2:					return GL_FALSE;
	default: appErrorf(TEXT("VertexElementType %d is not supported in ES2")); return 0;
	}
}


/**
 * Maps unreal primitive type to GL type
 */
static inline GLint TranslateUnrealPrimitiveTypeToGLType(DWORD Type)
{
	switch (Type)
	{
	case PT_TriangleList:			return GL_TRIANGLES;
	case PT_TriangleStrip:			return GL_TRIANGLE_STRIP;
	case PT_LineList:				return GL_LINES;
	default: appErrorf(TEXT("PrimitiveType %d is not supported in ES2")); return 0;
	}
};


// Converts from num-of-primitives to num-of-vertices or num-of-indices.
static inline void TranslateUnrealPrimitiveTypeToElementCount(DWORD Type, DWORD& ElementsPerVertex, DWORD& StartupElements)
{
	switch (Type)
	{
	case PT_TriangleList:			ElementsPerVertex = 3; StartupElements = 0; break;
	case PT_TriangleStrip:			ElementsPerVertex = 1; StartupElements = 2; break;
	case PT_LineList:				ElementsPerVertex = 2; StartupElements = 0; break;
	default: appErrorf(TEXT("PrimitiveType %d is not supported in ES2")); return;
	}

}

/**
 * Calculates the number of vertices or indices needed for the number of given primitives
 *
 * @param PrimitiveType Unreal primitive type
 * @param NumPrimitives Number of primitives to be rendered of PrimitiveType
 *
 * @return The total number of elements 
 */
inline DWORD CalcNumElements(DWORD PrimitiveType, DWORD NumPrimitives)
{
	if ( GThreeTouchMode == ThreeTouchMode_SingleTriangle ) 
	{
		return 3;
	}

	// figure out how many elements this primitive needs for the given number of primitives
	DWORD ElementsPerVertex, StartupElements;
	TranslateUnrealPrimitiveTypeToElementCount(PrimitiveType, ElementsPerVertex, StartupElements);
	return NumPrimitives * ElementsPerVertex + StartupElements;
}


#if FLASH
#include "AS3.h"
package_as3(
	"import flash.utils.ByteArray\n"
        "import C_Run.ram\n"
        );

//#define GLCOUNT(X) inline_as3("if(! \"" #X "\" in Console.es2api.counts) Console.es2api.counts[\"" #X "\"] = 0 else Console.es2api.counts[\"" #X "\"] = Console.es2api.counts[\"" #X "\"]+1\n")

#define GLCOUNT(X) {};




	void FES2RenderManager::RHIglClear(UINT value)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glClear(%0)\n" : : "r"(value));
	}

	void FES2RenderManager::RHIglClearColor(FLOAT r,FLOAT g,FLOAT b,FLOAT a )
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glClearColor(%0, %1, %2, %3)\n" : : "r"(r), "r"(g), "r"(b), "r"(a));
	}

	void FES2RenderManager::RHIglFinish()
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glFinish()\n" : : );
	}

	void FES2RenderManager::RHIglActiveTexture(UINT value)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glActiveTexture(%0)\n" : : "r"(value));
	}

	void FES2RenderManager::RHIglFlush()
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glFlush()\n" : : );
	}

	void FES2RenderManager::RHIglBindTexture(UINT type, UINT texture)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBindTexture(%0, %1)\n" : : "r"(type), "r"(texture));
	}
	
	void FES2RenderManager::RHIglEnable(UINT value)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glEnable(%0)\n" : : "r"(value));
	}

	void FES2RenderManager::RHIglDisable(UINT value)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDisable(%0)\n" : : "r"(value));
	}

	void FES2RenderManager::RHIglTexParameteri(UINT target,UINT pname,INT param)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glTexParameteri(%0, %1, %2)\n" : : "r"(target), "r"(pname), "r"(param));
	}

	void FES2RenderManager::RHIglTexImage2D(UINT target,INT level,INT intFormat,INT width,INT height,INT border,UINT format,UINT type,const void* data)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glTexImage2D(%0, %1, %2, %3, %4, %5, %6, %7, %8)\n" : : "r"(target), "r"(level), "r"(intFormat), "r"(width), "r"(height), "r"(border), "r"(format), "r"(type), "r"(data));
	}

	void FES2RenderManager::RHIglCompressedTexImage2D(UINT target,INT level,UINT intFormat,INT width,INT height,INT border,INT imgSize, const void* data)
	{
	    // Grab ATF texture format.
        uint8_t* ptr = (uint8_t*)data;
        uint8_t format = ptr[6];
        
        if (imgSize == 0)
            return;
        
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glCompressedTexImage2D(%0, %1, %2, %3, %4, %5, %6, %7)\n" : : "r"(target), "r"(level), "r"(format), "r"(width), "r"(height), "r"(border), "r"(imgSize), "r"(data));
	}

	void FES2RenderManager::RHIglGenTextures(INT length, UINT* data)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glGenTextures(%0, %1)\n" : : "r"(length), "r"(data));
	}
	void FES2RenderManager::RHIglDeleteTextures(INT length, UINT* data)
	{
		// TODO: Implement for real?
	}
    void FES2RenderManager::RHIglReadPixels(INT, INT, INT, INT, UINT, UINT, void*)
    {
        // TODO: Implement for real?
    }
    int FES2RenderManager::RHIglGetIntegerv(INT, INT*)
    {
        // TODO: Implement for real?
    }
	void FES2RenderManager::RHIglPixelStorei(UINT pname,INT param)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glPixelStorei(%0, %1)\n" : : "r"(pname), "r"(param));
	}
	void FES2RenderManager::RHIglGenBuffers(INT length, UINT* data)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glGenBuffers(%0, %1)\n" : : "r"(length), "r"(data));
	}
	void FES2RenderManager::RHIglDeleteBuffers(INT length, UINT* data)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglGenFramebuffers(INT length,UINT* data)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glGenFrameBuffers(%0, %1)\n" : : "r"(length), "r"(data));
	}
	void FES2RenderManager::RHIglGenRenderbuffers(INT length,UINT* data)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glGenRenderBuffers(%0, %1)\n" : : "r"(length), "r"(data));
	}
	void FES2RenderManager::RHIglDeleteRenderbuffers(INT length,UINT* data)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglRenderbufferStorage(UINT target, UINT format, INT width, INT height)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glRenderbufferStorage(%0, %1, %2, %3)\n" : : "r"(target), "r"(format), "r"(width), "r"(height));
	}

	static bool elementArrayBufferBound = false;
    static bool arrayBufferBound = false;

	void FES2RenderManager::RHIglBindBuffer(UINT target, UINT buffer)
	{
        if(target == GL_ELEMENT_ARRAY_BUFFER)
            arrayBufferBound = buffer != 0;
        else if(target == GL_ARRAY_BUFFER)
            elementArrayBufferBound = buffer != 0;

        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBindBuffer(%0, %1)\n" : : "r"(target), "r"(buffer));
	}
	void FES2RenderManager::RHIglBindFramebuffer(UINT target, UINT buffer)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBindFramebuffer(%0, %1)\n" : : "r"(target), "r"(buffer));
	}
	void FES2RenderManager::RHIglDeleteFramebuffers(INT length, UINT* targets)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDeleteFramebuffers(%0, %1)\n" : : "r"(length), "r"(targets));
	}
	void FES2RenderManager::RHIglFramebufferRenderbuffer(UINT target, UINT attachment, UINT renderbuffertarget, UINT renderbuffer)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glFramebufferRenderbuffer(%0, %1, %2, %3)\n" : : "r"(target), "r"(attachment), "r"(renderbuffertarget), "r"(renderbuffer));
	}
	void FES2RenderManager::RHIglFramebufferTexture2D(UINT target, UINT attachment, UINT textarget, UINT texture, INT level)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glFramebufferTexture2D(%0, %1, %2, %3, %4)\n" : : "r"(target), "r"(attachment), "r"(textarget), "r"(texture), "r"(level));
	}
	void FES2RenderManager::RHIglBindRenderbuffer(UINT target, UINT buffer)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBindRenderbuffer(%0, %1)\n" : : "r"(target), "r"(buffer));
	}
	void FES2RenderManager::RHIglBufferData(UINT target, UINT size, const void* data, UINT usage)
	{
        inline_as3( "import flash.utils.ByteArray\n"
                        "var msg:ByteArray = new ByteArray()\n"
                        "msg.endian = \"littleEndian\"\n"
                        "msg.writeBytes(ram, %0, %1)\n"
                        "msg.position = 0\n"
                        : : "r"(data), "r"(size)
                    );
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBufferData(%0, %1, msg)\n" : : "r"(target), "r"(usage));
                       
	}
	void FES2RenderManager::RHIglBufferSubData(UINT target, UINT offset, UINT size, const void* data)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglDepthMask(unsigned char flag)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDepthMask(%0)\n" : : "r"(flag));
	}
	void FES2RenderManager::RHIglStencilMask(UINT mask)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglStencilFunc(UINT,UINT,UINT)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglStencilOp(UINT,UINT,UINT)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglColorMask(unsigned char r, unsigned char g, unsigned char b, unsigned char a )
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glColorMask(%0, %1, %2, %3)\n" : : "r"(r), "r"(g), "r"(b), "r"(a));
	}
	void FES2RenderManager::RHIglDepthFunc(UINT func)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDepthFunc(%0)\n" : : "r"(func));
	}
	void FES2RenderManager::RHIglViewport(UINT x, UINT y, UINT width, UINT height)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glViewport(%0, %1, %2, %3)\n" : : "r"(x), "r"(y), "r"(width), "r"(height));
	}
	void FES2RenderManager::RHIglScissor(UINT x, UINT y, UINT width, UINT height)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glScissor(%0, %1, %2, %3)\n" : : "r"(x), "r"(y), "r"(width), "r"(height));
	}
	void FES2RenderManager::RHIglDepthRangef(FLOAT n,FLOAT f)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDepthRangef(%0, %1)\n" : : "r"(n), "r"(f));
	}
	void FES2RenderManager::RHIglClearDepthf(FLOAT depth)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glClearDepthf(%0)\n" : : "r"(depth));
	}
	void FES2RenderManager::RHIglFrontFace(UINT mode)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glFrontFace(%0)\n" : : "r"(mode));
	}
	void FES2RenderManager::RHIglClearStencil(INT s)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glClearStencil(%0)\n" : : "r"(s));
	}
	void FES2RenderManager::RHIglEnableVertexAttribArray(UINT index)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glEnableVertexAttribArray(%0)\n" : : "r"(index));
	}
	void FES2RenderManager::RHIglDisableVertexAttribArray(UINT index)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDisableVertexAttribArray(%0)\n" : : "r"(index));
	}

	static UINT currentProgram;
	void FES2RenderManager::RHIglUniformMatrix3fv(INT location,INT count,unsigned char transpose,const void* value)
	{
        GLCOUNT(RHIglUniformMatrix3fv);
        
        inline_as3("import com.adobe.flascc.Console;\n"
                       "if (%1 < 0) return; \n"
                       "var isFragment:Boolean = %1 >= 512;\n"
                       "var loc:uint = isFragment ? %1 - 512 : %1;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"
                       
                       "// Expand if necessary\n"
                       "if (loc + 16 >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + 16 + 1)/4)*4\n"
                       "vdata[loc+3] = 0\n"
                       "vdata[loc+7] = 0\n"
                       "vdata[loc+11] = 0\n"
                       "vdata[loc+15] = 0\n"
                       "vdata[loc+12] = 0\n"
                       "vdata[loc+13] = 0\n"
                       "vdata[loc+14] = 0\n"
                       : : "r"(value), "r"(location));
                       
        #ifdef FALCON
        inline_as3("import avm2.intrinsics.memory.*;\n"
                       "vdata[loc+0] = lf32(%0+0)\n"
                       "vdata[loc+4] = lf32(%0+4)\n"
                       "vdata[loc+8] = lf32(%0+8)\n"
                       
                       "vdata[loc+1] = lf32(%0+12)\n"
                       "vdata[loc+5] = lf32(%0+16)\n"
                       "vdata[loc+9] = lf32(%0+20)\n"
                       
                       "vdata[loc+2] = lf32(%0+24)\n"
                       "vdata[loc+6] = lf32(%0+28)\n"
                       "vdata[loc+10] = lf32(%0+32)\n"
                       : : "r"(value));
		#else
		inline_as3("import avm2.intrinsics.memory.*;\n"
                       "vdata[loc+0] = ((__xasm<Number>(push(%0+0), op(0x38))))\n"
                       "vdata[loc+4] = ((__xasm<Number>(push(%0+4), op(0x38))))\n"
                       "vdata[loc+8] = ((__xasm<Number>(push(%0+8), op(0x38))))\n"
                       
                       "vdata[loc+1] = ((__xasm<Number>(push(%0+12), op(0x38))))\n"
                       "vdata[loc+5] = ((__xasm<Number>(push(%0+16), op(0x38))))\n"
                       "vdata[loc+9] = ((__xasm<Number>(push(%0+20), op(0x38))))\n"
                       
                       "vdata[loc+2] = ((__xasm<Number>(push(%0+24), op(0x38))))\n"
                       "vdata[loc+6] = ((__xasm<Number>(push(%0+28), op(0x38))))\n"
                       "vdata[loc+10] = ((__xasm<Number>(push(%0+32), op(0x38))))\n"
                       : : "r"(value));
		#endif
                       
	}

	void FES2RenderManager::RHIglUniformMatrix4fv(INT location,INT count,unsigned char transpose,const void* value)
	{
        GLCOUNT(RHIglUniformMatrix4fv);
        
        inline_as3("import com.adobe.flascc.Console;\n"
                       "if (%1 < 0) return; \n"
                       "var isFragment:Boolean = %1 >= 512;\n"
                       "var loc:uint = isFragment ? %1 - 512 : %1;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"
                       
                       "// Expand if necessary\n"
                       "if (loc + 16 >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + 16 + 1)/4)*4\n"
                       : : "r"(value), "r"(location));
                       
		#ifdef FALCON
        inline_as3("import avm2.intrinsics.memory.*;\n"
                       "vdata[loc+0] = lf32(%0+0)\n"
                       "vdata[loc+4] = lf32(%0+4)\n"
                       "vdata[loc+8] = lf32(%0+8)\n"
                       "vdata[loc+12] = lf32(%0+12)\n"
                       
                       "vdata[loc+1] = lf32(%0+16)\n"
                       "vdata[loc+5] = lf32(%0+20)\n"
                       "vdata[loc+9] = lf32(%0+24)\n"
                       "vdata[loc+13] = lf32(%0+28)\n"
                       
                       "vdata[loc+2] = lf32(%0+32)\n"
                       "vdata[loc+6] = lf32(%0+36)\n"
                       "vdata[loc+10] = lf32(%0+40)\n"
                       "vdata[loc+14] = lf32(%0+44)\n"
                       
                       "vdata[loc+3] = lf32(%0+48)\n"
                       "vdata[loc+7] = lf32(%0+52)\n"
                       "vdata[loc+11] = lf32(%0+56)\n"
                       "vdata[loc+15] = lf32(%0+60)\n"
                       : : "r"(value));
		#else
		inline_as3("import avm2.intrinsics.memory.*;\n"
                       "vdata[loc+0] = ((__xasm<Number>(push(%0+0), op(0x38))))\n"
                       "vdata[loc+4] = ((__xasm<Number>(push(%0+4), op(0x38))))\n"
                       "vdata[loc+8] = ((__xasm<Number>(push(%0+8), op(0x38))))\n"
                       "vdata[loc+12] = ((__xasm<Number>(push(%0+12), op(0x38))))\n"
                       
                       "vdata[loc+1] = ((__xasm<Number>(push(%0+16), op(0x38))))\n"
                       "vdata[loc+5] = ((__xasm<Number>(push(%0+20), op(0x38))))\n"
                       "vdata[loc+9] = ((__xasm<Number>(push(%0+24), op(0x38))))\n"
                       "vdata[loc+13] = ((__xasm<Number>(push(%0+28), op(0x38))))\n"
                       
                       "vdata[loc+2] = ((__xasm<Number>(push(%0+32), op(0x38))))\n"
                       "vdata[loc+6] = ((__xasm<Number>(push(%0+36), op(0x38))))\n"
                       "vdata[loc+10] = ((__xasm<Number>(push(%0+40), op(0x38))))\n"
                       "vdata[loc+14] = ((__xasm<Number>(push(%0+44), op(0x38))))\n"
                       
                       "vdata[loc+3] = ((__xasm<Number>(push(%0+48), op(0x38))))\n"
                       "vdata[loc+7] = ((__xasm<Number>(push(%0+52), op(0x38))))\n"
                       "vdata[loc+11] = ((__xasm<Number>(push(%0+56), op(0x38))))\n"
                       "vdata[loc+15] = ((__xasm<Number>(push(%0+60), op(0x38))))\n"
                        : : "r"(value));
		#endif
	}

    void FES2RenderManager::RHIglUniform1i(INT location,INT v0)
    {
        GLCOUNT(RHIglUniform1i);
        
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glUniform1i(%0, %1)\n" : : "r"(location), "r"(v0));
   return;
    
        inline_as3("if (%0 < 0) return; \n"
                       
                       "var uniformName:String = Console.es2api.activeProgram.uniformNames[%0]\n"
                       
                       "// Track our samplers.\n"
                       "var programVar = Console.es2api.activeProgram.fragmentShaderVars[uniformName]\n"
                       "if (programVar && programVar.isSampler)\n"
                       "Console.es2api.activeProgram.activeSamplers[%1] = true\n"
                       
                       "var isFragment:Boolean = %0 >= 512;\n"
                       "var loc:uint = isFragment ? %0 - 512 : %0;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"

                       "// Expand if necessary\n"
                       "if (loc + 1 >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + 1 + 1)/4)*4\n"

                       "vdata[loc] = %1\n"
                       : : "r"(location), "r"(v0));
    }
    void FES2RenderManager::RHIglUniform1fv(INT location,INT count,const void* value)
    {
        GLCOUNT(RHIglUniform1fv);
        
        inline_as3("import com.adobe.flascc.Console;\n"
                       "if (%1 < 0) return; \n"
                       "var isFragment:Boolean = %1 >= 512;\n"
                       "var loc:uint = isFragment ? %1 - 512 : %1;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"
                       
                       "// Expand if necessary\n"
                       "if (loc + 1 >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + 1 + 1)/4)*4\n"
                       "var ptr:uint = %0\n"
                       : : "r"(value), "r"(location), "r"(count));


		#ifdef FALCON
        inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc] = lf32(ptr)\n"
                       "loc++\n"
                       "ptr += 4\n"
                       "}\n"
                       
                       : : "r"(count));
		#else
		inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc] = ((__xasm<Number>(push(ptr), op(0x38))))\n"
                       "loc++\n"
                       "ptr += 4\n"
                       "}\n"
                       
                       : : "r"(count));
		#endif
                       
    }

	void FES2RenderManager::RHIglUniform2fv(INT location,INT count,const void* value)
	{
        GLCOUNT(RHIglUniform2fv);
        
        inline_as3("if (%1 < 0) return; \n"
                       "var isFragment:Boolean = %1 >= 512;\n"
                       "var loc:uint = isFragment ? %1 - 512 : %1;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"
                       
                       "// Expand if necessary\n"
                       "if (loc + (2*%2) >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + (2*%2) + 1)/4)*4\n"
                       "var ptr:uint = %0\n"
                       : : "r"(value), "r"(location), "r"(count));

		#ifdef FALCON
        inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc+0] = lf32(ptr+0)\n"
                       "vdata[loc+1] = lf32(ptr+4)\n"
                       "loc += 2\n"
                       "ptr += 8\n"
                       "}\n"
                       : : "r"(count));
		#else
		inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc+0] = ((__xasm<Number>(push(ptr+0), op(0x38))))\n"
                       "vdata[loc+1] = ((__xasm<Number>(push(ptr+4), op(0x38))))\n"
                       "loc += 2\n"
                       "ptr += 8\n"
                       "}\n"
                       : : "r"(count));
		#endif
	}

	void FES2RenderManager::RHIglUniform3fv(INT location,INT count,const void* value)
	{
        GLCOUNT(RHIglUniform3fv);
        
        inline_as3("import com.adobe.flascc.Console;\n"
                       "if (%1 < 0) return; \n"
                       "var isFragment:Boolean = %1 >= 512;\n"
                       "var loc:uint = isFragment ? %1 - 512 : %1;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"
                       
                       "// Expand if necessary\n"
                       "if (loc + (3*%2) >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + (3*%2) + 1)/4)*4\n"
                       "var ptr:uint = %0\n"
                       : : "r"(value), "r"(location), "r"(count));

		#ifdef FALCON
        inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc+0] = lf32(ptr+0)\n"
                       "vdata[loc+1] = lf32(ptr+4)\n"
                       "vdata[loc+2] = lf32(ptr+8)\n"
                       "loc += 3\n"
                       "ptr += 12\n"
                       "}\n"
                       : : "r"(count));
		#else
		inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc+0] = ((__xasm<Number>(push(ptr+0), op(0x38))))\n"
                       "vdata[loc+1] = ((__xasm<Number>(push(ptr+4), op(0x38))))\n"
                       "vdata[loc+2] = ((__xasm<Number>(push(ptr+8), op(0x38))))\n"
                       "loc += 3\n"
                       "ptr += 12\n"
                       "}\n"
                       : : "r"(count));
		#endif
	}
	
	void FES2RenderManager::RHIglUniform4fv(INT location,INT count,const void* value)
	{
        GLCOUNT(RHIglUniform4fv);
        
        inline_as3("import com.adobe.flascc.Console;\n"
                       "if (%1 < 0) return; \n"
                       "var isFragment:Boolean = %1 >= 512;\n"
                       "var loc:uint = isFragment ? %1 - 512 : %1;\n"
                       "var vdata:Vector.<Number> = isFragment ? Console.es2api.activeProgram.fragmentConstantsData : Console.es2api.activeProgram.vertexConstantsData;\n"
                       
                       "if(isFragment)\n"
                       "Console.es2api.activeProgram.fragmentConstantsDirty = true\n"
                       "else\n"
                       "Console.es2api.activeProgram.vertexConstantsDirty = true\n"
                       
                       "// Expand if necessary\n"
                       "if (loc + (4*%2) >= vdata.length)\n"
                       "vdata.length = Math.ceil((loc + (4*%2) + 1)/4)*4\n"
                       "var ptr:uint = %0\n"
                       
                       : : "r"(value), "r"(location), "r"(count));

		#ifdef FALCON
        inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc+0] = lf32(ptr+0)\n"
                       "vdata[loc+1] = lf32(ptr+4)\n"
                       "vdata[loc+2] = lf32(ptr+8)\n"
                       "vdata[loc+3] = lf32(ptr+12)\n"
                       "loc += 4\n"
                       "ptr += 16\n"
                       "}\n"
                       : : "r"(count));
		#else
		inline_as3("import avm2.intrinsics.memory.*;\n"
                       "for (var c:int = 0; c < %0; c++)\n"
                       "{\n"
                       "vdata[loc+0] = ((__xasm<Number>(push(ptr+0), op(0x38))))\n"
                       "vdata[loc+1] = ((__xasm<Number>(push(ptr+4), op(0x38))))\n"
                       "vdata[loc+2] = ((__xasm<Number>(push(ptr+8), op(0x38))))\n"
                       "vdata[loc+3] = ((__xasm<Number>(push(ptr+12), op(0x38))))\n"
                       "loc += 4\n"
                       "ptr += 16\n"
                       "}\n"
                       : : "r"(count));
		#endif
	}

	void FES2RenderManager::RHIglDrawArrays(UINT mode, UINT first, UINT count)
	{
        GLCOUNT(RHIglDrawArrays);
        
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glDrawArrays(%0, %1, %2)\n" : : "r"(mode), "r"(first), "r"(count));
	}
	
	void FES2RenderManager::RHIglDrawElements(UINT mode,INT count,UINT type,const void* indices)
	{
	inline_as3("import com.adobe.flascc.Console;\n");
        GLCOUNT(RHIglDrawElements);
        
        GLboolean arrayBound = GL_FALSE;
        
        arrayBound = elementArrayBufferBound;
                
		const void* data = arrayBound ? &indices : indices;
		UINT dataCount = arrayBound ? 1 : count;
        
        if (type == GL_UNSIGNED_SHORT && !arrayBound)
			dataCount = data ? dataCount * 2 : 0;
		else if (type == GL_UNSIGNED_BYTE && !arrayBound)
			dataCount = data ? dataCount : 0;
		else
		{
			if (!arrayBound && !data)
				dataCount = 0;
			dataCount *= 4;
		}
        
        //debugf("RHIglDrawElements arrayBound: %d, indicies: %p, data: %p, dataCount: %d\n", arrayBound, indices, data, dataCount);
        
        if(arrayBound)
            inline_as3("Console.es2api.glDrawElements(%0, %1, %2, uint(%3), uint(%4))\n" : : "r"(mode), "r"(count), "r"(type), "r"((unsigned int)indices), "r"(0) );
        else
            inline_as3("Console.es2api.glDrawElements(%0, %1, %2, uint(%3), uint(%4))\n" : : "r"(mode), "r"(count), "r"(type), "r"((unsigned int)data), "r"(dataCount) );
        //else {
        //    inline_as3("trace(\"drawelements: \" + %0 + \" \" + %1)\n"
        //                   "import flash.utils.ByteArray\n"
        //                   "var msg:ByteArray = new ByteArray()\n"
        //                   "msg.endian = \"littleEndian\"\n"
        //                   "msg.writeBytes(ram, %0, %1)\n"
        //                   : : "r"(data), "r"(dataCount)
        //    );
        //    inline_as3("Console.es2api.glDrawElements(%0, %1, %2, msg)\n" : : "r"(mode), "r"(count), "r"(type));
        //}
	}

	void FES2RenderManager::RHIglVertexAttribPointer(UINT index,INT size,UINT type,unsigned char normalized,INT stride, const void* data, UINT dataLength)
	{
        GLCOUNT(RHIglVertexAttribPointer);
        
        GLboolean arrayBound = arrayBufferBound;
    
        //if(dataLength == 0) {
        inline_as3("Console.es2api.glVertexAttribPointer(%0, %1, %2, %3, %4, uint(%5), uint(%6))\n" : : "r"(index), "r"(size), "r"(type), "r"((bool)normalized), "r"(stride), "r"((unsigned int)data), "r"(dataLength) );
        //} else {
        //    inline_as3(//"trace(\"RHIglVertexAttribPointer: \" + %0 + \" \" + %1)\n"
        //               "import flash.utils.ByteArray\n"
        //               "var msg:ByteArray = new ByteArray()\n"
        //               "msg.endian = \"littleEndian\"\n"
        //               "msg.writeBytes(ram, %0, %1)\n"
        //               "msg.position = 0\n"
        //               : : "r"(data), "r"(dataLength)
        //               );
        //inline_as3("Console.es2api.glVertexAttribPointer(%0, %1, %2, %3, %4, msg)\n" : : "r"(index), "r"(size), "r"(type), "r"((bool)normalized), "r"(stride));
        //}
	}

	void FES2RenderManager::RHIglUseProgram(UINT program)
	{
        GLCOUNT(RHIglUseProgram);
        

		currentProgram = program;
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glUseProgram(%0)\n" : : "r"(program));
	}

	void FES2RenderManager::RHIglLinkProgram(UINT program)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glLinkProgram(%0)\n" : : "r"(program));
	}
	void FES2RenderManager::RHIglDeleteProgram(UINT program)
	{
		// TODO: Implement for real?
	}

	void FES2RenderManager::RHIglCompileShader(UINT shader)
	{
        //inline_as3("Console.es2api.glCompileShader(%0)\n" : : "r"(shader));
	}

	void FES2RenderManager::RHIglShaderSource(UINT shader,INT count,const char ** cstr,void* length)
	{
		FString Source;

		if(length == NULL) 
		{
			// string are null terminated
			for(int i=0; i<count; i++)
			{
				Source += cstr[i];
			}
		}
		else 
		{
			for(int i=0; i<count; i++)
			{
				Source += FString(cstr[i]).Left(((int*)length)[i]);
			}
		}
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("ram.position = %0\n"
                       "Console.es2api.glShaderSource(%1, ram.readUTFBytes(%2))\n" : : "r"(TCHAR_TO_ANSI(*Source)), "r"(shader), "r"(Source.Len()));
	}

	void FES2RenderManager::RHIglAttachShader(UINT program,UINT shader)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glAttachShader(%0, %1)\n" : : "r"(program), "r"(shader));
	}
	void FES2RenderManager::RHIglDetachShader(UINT program,UINT shader)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIglDeleteShader(UINT shader)
	{
		// TODO: Implement for real?
	}

	int FES2RenderManager::RHIglCreateProgram()
	{
            int result;
            inline_as3("import com.adobe.flascc.Console;\n");
            inline_as3("%0 = Console.es2api.glCreateProgram()\n" : "=r"(result));
            return result;
	}
	
	int FES2RenderManager::RHIglCreateShader(UINT shaderType)
	{
            int result;
            inline_as3("import com.adobe.flascc.Console;\n");
            inline_as3("%0 = Console.es2api.glCreateShader(%1)\n" : "=r"(result) : "r"(shaderType));
            return result;
	}
	
	int FES2RenderManager::RHIglGetAttribLocation(UINT program, const char * name)
	{
            int result;
            inline_as3("ram.position = %2\n"
                       "%0 = Console.es2api.glGetAttribLocation(%1, ram.readUTFBytes(%3))\n" : "=r"(result) : "r"(program), "r"(name), "r"(strlen(name)));
            return result;
	}
	
	int FES2RenderManager::RHIglGetUniformLocation(UINT program, const char * name)
	{
            int result;
            inline_as3("import com.adobe.flascc.Console;\n"
                           "ram.position = %2\n"
                           "%0 = Console.es2api.glGetUniformLocation(%1, ram.readUTFBytes(%3))\n" : "=r"(result) : "r"(program), "r"(name), "r"(strlen(name)));
            return result;
	}

	void FES2RenderManager::RHIglBlendColor(FLOAT red, FLOAT blue, FLOAT green, FLOAT alpha)
	{
		// TODO: Implement for real?
		//inline_as3("Console.es2api.glBlendColor(%0, %1, %2, %3)\n" : : "r"(red), "r"(green), "r"(blue), "r"(alpha));
	}
	void FES2RenderManager::RHIglBlendEquationSeparate(UINT modeRGB,UINT modeAlpha)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBlendEquationSeparate(%0, %1)\n" : : "r"(modeRGB), "r"(modeAlpha));
	}

	void FES2RenderManager::RHIglBlendFuncSeparate(UINT srcRGB,UINT dstRGB,UINT srcAlpha,UINT dstAlpha)
	{
		inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.glBlendFuncSeparate(%0, %1, %2, %3)\n" : : "r"(srcRGB), "r"(dstRGB), "r"(srcAlpha), "r"(dstAlpha));
	}

	void FES2RenderManager::RHIglStencilOpSeparate(UINT face, UINT sfail, UINT dfail, UINT dpass)
	{
		// TODO: Implement for real?
		//inline_as3("Console.es2api.glStencilOpSeparate(%0, %1, %2, %3)\n" : : "r"(face), "r"(sfail), "r"(dfail), "r"(dpass));
	}

	void FES2RenderManager::RHIglStencilFuncSeparate(UINT face, UINT func, UINT ref, UINT mask)
	{
		// TODO: Implement for real?
		//inline_as3("Console.es2api.glStencilFuncSeparate(%0, %1, %2, %3)\n" : : "r"(face), "r"(func), "r"(ref), "r"(mask));
	}

	void FES2RenderManager::RHIglPolygonOffset(GLfloat factor, GLfloat units)
	{
		// TODO: Implement for real?
	}
	void FES2RenderManager::RHIeglSwapBuffers()
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.eglSwapBuffers()\n");
	}

	void PlatformSwapBuffers(FES2Viewport* Viewport)
	{
        inline_as3("import com.adobe.flascc.Console;\n");
        inline_as3("Console.es2api.eglSwapBuffers()\n");
	}
#endif

/**
 *	Initialize the render manager
 */
void FES2RenderManager::InitRHI()
{
	// Default vertex scratch to 32kB if it is not in the ini
	VertexScratchBufferSize = GSystemSettings.MobileVertexScratchBufferSize * 1024;
	if (VertexScratchBufferSize == 0)
	{
		VertexScratchBufferSize = 32 * 1024;
	}
	// Default index scratch to (VertexScratch/64 * 2) if it is not in the ini
	IndexScratchBufferSize = GSystemSettings.MobileIndexScratchBufferSize * 1024;
	if (IndexScratchBufferSize == 0)
	{
		IndexScratchBufferSize = VertexScratchBufferSize / 32;
	}
#if IPHONE
	VertexScratchBuffer = (BYTE*)appMalloc(VertexScratchBufferSize, ScratchBufferAlignment);
	IndexScratchBuffer = (BYTE*)appMalloc(IndexScratchBufferSize, ScratchBufferAlignment);
#else
	VertexScratchBuffer = (BYTE*)appMalloc(VertexScratchBufferSize);
	IndexScratchBuffer = (BYTE*)appMalloc(IndexScratchBufferSize);
#endif

	// Make sure the memory is aligned as we want
	check(Align((PTRINT)VertexScratchBuffer, ScratchBufferAlignment) == (PTRINT)VertexScratchBuffer);
	check(Align((PTRINT)IndexScratchBuffer, ScratchBufferAlignment) == (PTRINT)IndexScratchBuffer);

	const INT NullElementCount = 64 * 1024;
	DWORD NullColorData[NullElementCount];

#if _WINDOWS
	// Set up the White Color Stream VBOs used for visual debugging
	{
		//default to this color buffer
		GNullColorVBOIndex = 0;

		//fill with black
		appMemzero(NullColorData, NullElementCount * sizeof(DWORD));
		GLCHECK(glGenBuffers(1, &(GNullColorVBOs[0])));
		GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, GNullColorVBOs[0]));
		GLCHECK(glBufferData(GL_ARRAY_BUFFER, NullElementCount * sizeof(DWORD), NullColorData, GL_STATIC_DRAW));

		//fill with white
		appMemset(NullColorData, 0xff, NullElementCount * sizeof(DWORD));
		GLCHECK(glGenBuffers(1, &(GNullColorVBOs[1])));
		GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, GNullColorVBOs[1]));
		GLCHECK(glBufferData(GL_ARRAY_BUFFER, NullElementCount * sizeof(DWORD), NullColorData, GL_STATIC_DRAW));
	}
#endif

	//to enable fracturables, we use a NULL bone weight array that happens to be the exact same data as the all black color vbo above
	for( INT NullColorIndex = 0; NullColorIndex < NullElementCount; NullColorIndex++ )
	{
		NullColorData[NullColorIndex] = 0xff0000ff;
	}
	GLCHECK(glGenBuffers(1, &GNullWeightVBO));
	GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, GNullWeightVBO));
	GLCHECK(glBufferData(GL_ARRAY_BUFFER, NullElementCount * sizeof(DWORD), NullColorData, GL_STATIC_DRAW));
	INC_TRACKED_OPEN_GL_BUFFER_MEM(NullElementCount * sizeof(DWORD));
	GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
}


/**
 * Called when shutting down the RHI
 */
void FES2RenderManager::ExitRHI()
{
	for( INT CurStreamIndex = 0; CurStreamIndex < MaxVertexStreams; ++CurStreamIndex )
	{
		FES2PendingStream& CurStream = PendingStreams[ CurStreamIndex ];
		if( IsValidRef( CurStream.VertexBuffer ) )
		{
			CurStream.VertexBuffer.SafeRelease();
		}
	}

	if( IsValidRef( PendingVertexDeclaration ) )
	{
		PendingVertexDeclaration.SafeRelease();
	}
}

/**
 * Clears out any GPU Resources used by Render Manager
 */
void FES2RenderManager::ClearGPUResources()
{
	// clear out refs to GPU data
	ExitRHI();		
	// set the streams dirty
	bArePendingStreamsDirty = TRUE;
	// make sure no attributes are set
	PrepareAttributes(0);
}

/**
 * Set the vertex attribute inputs, and set the current program
 *
 * @param InPrimitiveData For DrawPrimUP, this is the immediate vertex data
 * @param InVertexStride The size of each vertex in InPrimitiveData, if given
 */
UBOOL FES2RenderManager::UpdateAttributesAndProgram(const void* InPrimitiveData, INT InVertexStride, UINT InVertexDataSize)
{
	static FName SpriteParticle(TEXT("SpriteParticle"));
	static FName SubUVSpriteParticle(TEXT("SubUVSpriteParticle"));
	static FName BeamTrailParticle(TEXT("BeamTrailParticle"));
	static FName LensFlare(TEXT("LensFlare"));
	static FName Simple(TEXT("Simple"));
	static FName LandscapeName(TEXT("FLandscapeVertexFactoryMobile"));
	static FName DecalName(TEXT("Decal"));
	
	// helpers to figure out program to use
	FES2VertexDeclaration* VertexDecl = ES2CAST( FES2VertexDeclaration, PendingVertexDeclaration );
	UBOOL bUseSpriteParticles = VertexDecl->DeclName == SpriteParticle;
	UBOOL bUseSubUVSpriteParticles = VertexDecl->DeclName == SubUVSpriteParticle;
	UBOOL bUseBeamTrailParticles = VertexDecl->DeclName == BeamTrailParticle;
	UBOOL bUseLensFlare = VertexDecl->DeclName == LensFlare;		
	UBOOL bUseSimpleElements = VertexDecl->DeclName == Simple;
	UBOOL bUseGPUSkin = FALSE;
	UBOOL bHasTexCoord0 = FALSE;
	UBOOL bIsLandscape = VertexDecl->DeclName == LandscapeName;
	UBOOL bIsDecal = VertexDecl->DeclName == DecalName;

	UBOOL bWerePendingStreamsDirty = bArePendingStreamsDirty;

	// now we need to apply the stream sources
	if (bArePendingStreamsDirty)
	{
		// reset the flag
		bArePendingStreamsDirty = FALSE;

		// if we have UP data, unbind ARRAY_BUFFER
		if (InPrimitiveData != NULL)
		{
			GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
		}
	}

	{
		// get the elements in the current BoundShaderState
		const FVertexDeclarationElementList& VertexElements = VertexDecl->VertexElements;
		const INT NumElements = VertexElements.Num();

		// loop over the elements in the vertex declarations, looking for special cases
		for (INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
		{
			const FVertexElement& Element = VertexElements(ElementIndex);

			// for now only gpu skinning uses blendweight
			if (Element.Usage == VEU_BlendWeight)
			{
				bUseGPUSkin = TRUE;
			}

			// Keep track if whether or not we have a texture coordinate
			if( Element.Usage == VEU_TextureCoordinate && Element.UsageIndex == 0 )
			{
				bHasTexCoord0 = TRUE;
			}
		}
	}


	EMobilePrimitiveType Type = EPT_Default;
	EMobileGlobalShaderType GlobalShaderType = EGST_None;
	GShaderManager.ClearVertexFactoryFlags();		//default

	// determine the shader program to use
	if (bUseSpriteParticles || bUseSubUVSpriteParticles)
	{
		Type = EPT_Particle;
		if (bUseSubUVSpriteParticles)
		{
			//Turn on Particle Sub UV
			GShaderManager.SetVertexFactoryFlags(EShaderBaseFeatures::SubUVParticles);
		}
	}
	else if (bUseBeamTrailParticles)
	{
		Type = EPT_BeamTrailParticle;
	}
	else if (bUseLensFlare)
	{
		Type = EPT_LensFlare;
	}
	else if (InPrimitiveData)
	{
		GlobalShaderType = GShaderManager.GetNextDrawGlobalShaderAndReset();
		if ( GlobalShaderType != EGST_None )
		{
			//@TODO: Support various global shaders.
			Type = EPT_GlobalShader;
		}
		else if (bUseSimpleElements)
		{
			Type = EPT_Simple;
		}
	}
	else if ( bUseGPUSkin ) 
	{
		//Set GPU Skin on
		GShaderManager.SetVertexFactoryFlags(EShaderBaseFeatures::GPUSkinning);
	}
	else
	{
#if WITH_GFx
		GlobalShaderType = GShaderManager.GetNextDrawGlobalShader();
		if ( GlobalShaderType != EGST_None )
		{
			Type = EPT_GlobalShader;
		}
#endif
		if ( bIsLandscape )
		{
			//Set Landscape
			GShaderManager.SetVertexFactoryFlags(EShaderBaseFeatures::Landscape);
		}
		else if ( bIsDecal )
		{
			GShaderManager.SetVertexFactoryFlags(EShaderBaseFeatures::Decal);
		}

		if ( GShaderManager.HasHadLightmapSet() ) 
		{
			//Set lighting on
			GShaderManager.SetVertexFactoryFlags(EShaderBaseFeatures::Lightmap);

			if ( GShaderManager.HasHadDirectionalLightmapSet() )
			{
				GShaderManager.SetVertexFactoryFlags(EShaderBaseFeatures::DirectionalLightmap);
			}
		}
	}

	UBOOL bProgramChanged = GShaderManager.SetProgramByType(Type, GlobalShaderType);
	// TODO: For now, we'll just always assume this is true
	bProgramUpdateWasSuccessful = TRUE;

	extern GLint GMaxVertexAttribsGLSL;
	extern GLint GCurrentProgramUsedAttribMask;
	extern GLint* GCurrentProgramUsedAttribMapping;

	// now we need to apply the stream sources
	if (bWerePendingStreamsDirty || bProgramChanged)
	{
		// Reset this value each time we're going to refresh the attribute state
		bAttributeUpdateWasSuccessful = TRUE;

		//debugf( TEXT("CurrentProgram UsedAttribMask 0x%x, AttribMask 0x%x"), GCurrentProgramUsedAttribMask, AttribMask );
		// get the elements in the current BoundShaderState
		const FVertexDeclarationElementList& VertexElements = VertexDecl->VertexElements;
		const INT NumElements = VertexElements.Num();

		UINT NewAttribMask = 0;
		GLuint BoundArrayBuffer = ~0;
		FString MissingAttributeStreams;

		// loop over the elements in the vertex declarations, and hook up the streams
		for (INT ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
		{
			const FVertexElement& Element = VertexElements(ElementIndex);
			
			// calculate where we'll bind this attribute to
			const INT UE3AttributeIndex = TranslateUnrealUsageToBindLocation(Element.Usage) + Element.UsageIndex;
			const INT AttributeIndex = GCurrentProgramUsedAttribMapping[UE3AttributeIndex];
			if( AttributeIndex < 0 )
			{
				// This attribute is unused by the current shader, skip it
				continue;
			}

			//Default to no stream offset for DrawPrimitiveUP style dynamic data
			INT StreamOffset = 0;
			if ( GCurrentProgramUsedAttribMask & ( 1 << AttributeIndex ) )
			{
				// compute the vertex stride for DrimPrim vs DrawPrimUP
				INT VertexStride = InVertexStride;
				BYTE* AttribPointerBase = (BYTE*)InPrimitiveData;
				UBOOL bOriginalStreamActive = TRUE;

				// if the VertexStride wasn't passed in, then bind to the PendingStreams 
				if (VertexStride == -1)
				{
					FES2PendingStream& Stream = PendingStreams[Element.StreamIndex];

					if (!IsValidRef(Stream.VertexBuffer))
					{
						continue;
					}
					
					//store the pending stream offset
					StreamOffset = Stream.Offset;

					// set the vertex buffer for this stream as the active buffer
					GLuint NewArrayBuffer = ES2CAST( FES2VertexBuffer, Stream.VertexBuffer )->GetBufferName();
					VertexStride = Stream.Stride;
					// In the special case of a 0-stride stream, see if the stream is one that
					// we have a debugging replacement for, e.g. for the NULL Color Stream. On
					// a real device, we'll generally end up simply skipping the draw call in
					// this case, but in the simulator, it's easy to try to visualize.
					if( VertexStride == 0 )
					{
						if( Element.Usage == VEU_BlendWeight )
						{
							// Override both the VBO to bind and the stride, and indicate that the original stream isn't being used
							NewArrayBuffer = GNullWeightVBO;
							VertexStride = 4;
						}
#if _WINDOWS
						// For each known case, handle the missing stream
						else if( Element.Usage == VEU_Color &&
							Element.UsageIndex == 1 )
						{
							// Override both the VBO to bind and the stride, and indicate that the original stream isn't being used
							NewArrayBuffer = GNullColorVBOs[GNullColorVBOIndex];
							VertexStride = 4;
							bOriginalStreamActive = FALSE;
						}
						else
						{
							warnf(NAME_Warning, TEXT("ERROR: Unhandled zero-stride vertex attribute data!"));
						}

						// Keep track of the missing ones if we need to report them
						MissingAttributeStreams += FString::Printf(TEXT("ERROR:\t\tCurrent primitive missing vertex data: EVertexElementUsage = %d, UsageIndex = %d"), Element.Usage, Element.UsageIndex);
#endif
					}
					if ( BoundArrayBuffer != NewArrayBuffer )
					{
						BoundArrayBuffer = NewArrayBuffer;
						GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, BoundArrayBuffer));
					}
				}

				// UE3 always sets the stride to something meaningful, so check for 0
				// which is a special (and potentially dangerous) case for OpenGL
				if( VertexStride != 0 )
				{
					// Gather the values we'll use for this attribute
					const GLint VertexAttribCount = TranslateUnrealTypeToCount(Element.Type);
					const GLenum VertexAttribFormat = TranslateUnrealTypeToGLFormat(Element.Type);
					const GLboolean VertexAttribNormalize = TranslateUnrealTypeToNormalization(Element.Type);
					const GLsizei VertexAttribStride = VertexStride;
					const GLvoid* VertexAttribAddress = AttribPointerBase + StreamOffset + Element.Offset; // handle UP case and non-UP case

					// Check against the shadow state to avoid this call, if possible
					if( GStateShadow.ArrayBuffer[AttributeIndex] != BoundArrayBuffer ||
						GStateShadow.VertexAttribCount[AttributeIndex] != VertexAttribCount ||
						GStateShadow.VertexAttribFormat[AttributeIndex] != VertexAttribFormat ||
						GStateShadow.VertexAttribNormalize[AttributeIndex] != VertexAttribNormalize ||
						GStateShadow.VertexAttribStride[AttributeIndex] != VertexAttribStride ||
						GStateShadow.VertexAttribAddress[AttributeIndex] != VertexAttribAddress 
#if FLASH
                        || InPrimitiveData != NULL // Always invoke glVertexAttribPointer for Flash
#endif
						 )
					{
						GStateShadow.ArrayBuffer[AttributeIndex] = BoundArrayBuffer;
						GStateShadow.VertexAttribCount[AttributeIndex] = VertexAttribCount;
						GStateShadow.VertexAttribFormat[AttributeIndex] = VertexAttribFormat;
						GStateShadow.VertexAttribNormalize[AttributeIndex] = VertexAttribNormalize;
						GStateShadow.VertexAttribStride[AttributeIndex] = VertexAttribStride;
						GStateShadow.VertexAttribAddress[AttributeIndex] = VertexAttribAddress;

// @todo flash: Fix this up before merging code back to main
#if FLASH
						// bind that buffer to this attribute
						GLCHECK(glVertexAttribPointer(
							AttributeIndex,
							VertexAttribCount,
							VertexAttribFormat,
							VertexAttribNormalize,
							VertexAttribStride,
							(void*)(VertexAttribAddress),
							InPrimitiveData != NULL ? InVertexDataSize : 0
						));
#else
						// bind that buffer to this attribute
						GLCHECK(glVertexAttribPointer(
							AttributeIndex, 
							VertexAttribCount,
							VertexAttribFormat,
							VertexAttribNormalize,
							VertexAttribStride,
							(void*)(VertexAttribAddress)
						));
#endif
					}
					// Only update the new attribute mask if the original stream was used
					// so we can render with visual debugging enabled, but still catch
					// cases where we were missing attribute stream data.
					if( bOriginalStreamActive )
					{
						NewAttribMask |= ( 1 << AttributeIndex );
					}
				}
			}
		}
		// If the primitive didn't supply all of the required vertex attribute stream data, report and flag as an error
		if( (GCurrentProgramUsedAttribMask & ~NewAttribMask) != 0 )
		{
#if !FINAL_RELEASE || WITH_EDITOR
			warnf( NAME_Warning, TEXT( "ERROR: Invalid mesh detected! It's missing a vertex attribute (color?) that the material needs (%s)!" ), *GShaderManager.GetCurrentMaterialName() );
#else
			warnf( NAME_Warning, TEXT( "ERROR: Invalid mesh detected! It's missing a vertex attribute (color?) that the material needs!" ) );
#endif
			warnf( NAME_Warning, *MissingAttributeStreams );
			warnf( NAME_Warning, TEXT( "ERROR: \t\tThe mesh will be rendered using the fallback color (PINK)" ) );
			INC_DWORD_STAT( STAT_NumInvalidMeshes );
#if FINAL_RELEASE
			bProgramUpdateWasSuccessful = FALSE;
#else
 			GShaderManager.SetToUseFallbackStreamColor( TRUE );
 			bProgramChanged = GShaderManager.SetProgramByType(Type, GlobalShaderType);
 			GShaderManager.SetToUseFallbackStreamColor( FALSE );
#endif
		}

		// if we've enabled/disabled any attributes, update them now
		PrepareAttributes( NewAttribMask );
	}
	
	// mark that next time we set textures, reset if a lightmap has been set
	GShaderManager.ResetHasLightmapOnNextSetSamplerState();

	// Finally, update the success state of attributes and programs
	bAttributesAndProgramsAreValid = bAttributeUpdateWasSuccessful && bProgramUpdateWasSuccessful;

	return bAttributesAndProgramsAreValid;
}

/**
 * Set Active Vertex Attributes and corresponding mask
 *
 * @param NewAttribMask 
 */
void FES2RenderManager::PrepareAttributes( UINT NewAttribMask )
{
	extern GLint GMaxVertexAttribsGLSL;

		// if we've enabled/disabled any attributes, update them now
		UINT AttribChanged = AttribMask ^ NewAttribMask;
		if ( AttribChanged != 0 ) 
		{
			for (INT AttributeIndex = 0; AttributeIndex < GMaxVertexAttribsGLSL; AttributeIndex++)
			{
				UINT Bit = 1 << AttributeIndex;
				if ( AttribChanged & Bit )
				{
					if ( NewAttribMask & Bit )
					{
						GLCHECK(glEnableVertexAttribArray(AttributeIndex));
					}
					else
					{
						GLCHECK(glDisableVertexAttribArray(AttributeIndex));
					}
				}
			}
		}

		// remember which attributes are enabled
		AttribMask = NewAttribMask;
	}
	
/**
 * Retrieve scratch memory for dynamic draw data
 *
 * @param VertexDataSize Size in bytes of vertex data we need
 *
 * @return Pointer to start of properly aligned data
 */
BYTE* FES2RenderManager::AllocateVertexData(UINT VertexDataSize)
{
	// We make an assumption, which is safe today, that only one allocation is active out of this buffer at a time
	checkf(VertexScratchBufferRefcount == 0, TEXT("ERROR: Vertex scratch buffer data refcount is non-zero! Suggests multiple active allocations!"));

	// Align our size request to a multiple of the alignment, and check for space
	checkf(Align(VertexDataSize, ScratchBufferAlignment) <= VertexScratchBufferSize, TEXT("ERROR: Allocation too large for scratch buffer (%d, %d)"), VertexDataSize, VertexScratchBufferSize);

	static UINT MaxVertexDataSize = 0;
	MaxVertexDataSize = Max(MaxVertexDataSize, VertexDataSize);

	// Increment the refcount and return the buffer
	VertexScratchBufferRefcount++;
	return VertexScratchBuffer;
}
void FES2RenderManager::DeallocateVertexData()
{
	// Simply decrement the refcount
	VertexScratchBufferRefcount--;
}

/**
 * Retrieve scratch memory for dynamic draw data
 *
 * @param IndexDataSize Size in bytes of index data we need
 *
 * @return Pointer to start of properly aligned data
 */
BYTE* FES2RenderManager::AllocateIndexData(UINT IndexDataSize)
{
	// We make an assumption, which is safe today, that only one allocation is active out of this buffer at a time
	checkf(IndexScratchBufferRefcount == 0, TEXT("ERROR: Index scratch buffer data refcount is non-zero! Suggests multiple active allocations!"));

	// Align our size request to a multiple of the alignment, and check for space
	checkf(Align(IndexDataSize, ScratchBufferAlignment) <= IndexScratchBufferSize, TEXT("ERROR: Allocation too large for scratch buffer (%d, %d)"), IndexDataSize, IndexScratchBufferSize);

	static UINT MaxIndexDataSize = 0;
	MaxIndexDataSize = Max(MaxIndexDataSize, IndexDataSize);

	// Increment the refcount and return the buffer
	IndexScratchBufferRefcount++;
	return IndexScratchBuffer;
}
void FES2RenderManager::DeallocateIndexData()
{
	// Simply decrement the refcount
	IndexScratchBufferRefcount--;
}

	

/**
 * Single instance for the rendering manager to track streams, buffers, etc
 */
FES2RenderManager GRenderManager;


FVertexDeclarationRHIRef FES2RHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements) 
{
	return new FES2VertexDeclaration(Elements, NAME_None);
} 

FVertexDeclarationRHIRef FES2RHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements, FName DeclName) 
{
	return new FES2VertexDeclaration(Elements, DeclName); 
} 


/**
 * A holder struct that combines color and depth render targets into a GL Frame Buffer Object (fbo)
 */
FES2FrameBuffer::FES2FrameBuffer(FSurfaceRHIParamRef InColorRenderTarget, FSurfaceRHIParamRef InDepthRenderTarget)
	: ColorRenderTarget(InColorRenderTarget)
	, DepthRenderTarget(InDepthRenderTarget)
{
	// for the default ColorRenderTarget (who has a value of 0), we can just use the default
	// framebuffer (the main back buffer)
	FES2Surface* ES2ColorRenderTarget = ES2CAST( FES2Surface, ColorRenderTarget );
	FES2Surface* ES2DepthRenderTarget = ES2CAST( FES2Surface, DepthRenderTarget );

	// Is this the default color buffer?
	// Note: Default color and depth buffers can only be paired with each other.
	if (ES2ColorRenderTarget && ES2ColorRenderTarget->GetBackingRenderBuffer() == 0)
	{
		// Note: It can only be paired with the default depth buffer.
		FBO = 0;
		return;
	}

	GLCHECK(glGenFramebuffers(1, &FBO));
	GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, FBO));

	// bind the color target if it exists
	if (ES2ColorRenderTarget)
	{
		FTexture2DRHIRef ResolveTexture = ES2ColorRenderTarget->GetRenderTargetTexture();

		// bind to a render buffer if that's what it has
		if (ES2ColorRenderTarget->HasValidRenderBuffer())
		{
			GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ES2ColorRenderTarget->GetBackingRenderBuffer()));
		}
		else if (ResolveTexture)
		{
			FES2Texture2D* ES2ResolveTexture = ES2CAST( FES2Texture2D, ResolveTexture );
			GLCHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ES2ResolveTexture->GetTextureName(), 0));
		}
		else
		{
			appErrorf(TEXT("Currently only render buffer and texture2D resolve textures are supported for ES2FrameBuffer"));
		}
	}


	// bind the depth target if it exists and isn't a placeholder
	if (ES2DepthRenderTarget && !ES2DepthRenderTarget->IsAPlaceholderSurface())
	{
		FTexture2DRHIRef ResolveTexture = ES2DepthRenderTarget->GetRenderTargetTexture();

		// bind to a render buffer if that's what it has
		if (ES2DepthRenderTarget->HasValidRenderBuffer())
		{
			GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ES2DepthRenderTarget->GetBackingRenderBuffer()));
			GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ES2DepthRenderTarget->GetBackingStencilBuffer()));
		}
		else if (ResolveTexture)
		{
			FES2Texture2D* ES2ResolveTexture = ES2CAST( FES2Texture2D, ResolveTexture );
			GLCHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, ES2ResolveTexture->GetTextureName(), 0));

			// PF_ShadowDepth doesn't have a stencil component.
			if ( ES2ResolveTexture->GetFormat() != PF_ShadowDepth )
			{
				GLCHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ES2ResolveTexture->GetTextureName(), 0));
			}
		}
		else
		{
			appErrorf(TEXT("Currently only render buffer and texture2D resolve textures are supported for ES2FrameBuffer"));
		}
	}

// @todo flash: Is this #if needed? try removing it!
#if !FLASH
#if ANDROID
	// @hack some Android platforms do not detect valid combined depth/stencil buffers
	if (GMobileAllowFramebufferStatusCheck)
#endif
	{
		checkf(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, TEXT("FrameBuffer is not complete, error is %x"), glCheckFramebufferStatus(GL_FRAMEBUFFER));
	}
#endif
}

FES2FrameBuffer* FES2RenderManager::FindOrCreateFrameBuffer(FSurfaceRHIParamRef NewRenderTargetRHI,FSurfaceRHIParamRef NewDepthStencilTargetRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( Surface, NewRenderTarget )
	DYNAMIC_CAST_ES2RESOURCE( Surface, NewDepthStencilTarget )

	DWORD Key = 
		((NewRenderTarget		? NewRenderTarget->GetUniqueID()		: 0) <<  0) |
		((NewDepthStencilTarget	? NewDepthStencilTarget->GetUniqueID()	: 0) << 16);

	FES2FrameBuffer* FrameBuffer = FrameBuffers.Find(Key);

	// create it if needed
	if (!FrameBuffer)
	{
		FrameBuffer = &FrameBuffers.Set(Key, FES2FrameBuffer(NewRenderTargetRHI, NewDepthStencilTargetRHI));
	}

	return FrameBuffer;
}

void FES2RenderManager::RemoveFrameBufferReference(FSurfaceRHIParamRef RenderTargetRHI)
{
	DYNAMIC_CAST_ES2RESOURCE( Surface, RenderTarget )
	
	DWORD Key = RenderTarget->GetUniqueID();

	UBOOL bReferencesRemoved = true;
	while( bReferencesRemoved )
	{
		bReferencesRemoved = false;
		for( TMap<DWORD, FES2FrameBuffer>::TIterator Pair( FrameBuffers ); Pair; ++Pair )
		{
			DWORD SearchKey = Pair.Key();
			DWORD LoSearchKey = SearchKey & 0xffff;
			DWORD HiSearchKey = SearchKey >> 16;

			if( ( LoSearchKey == Key ) || ( HiSearchKey == Key ) )
			{
				FES2FrameBuffer* FrameBuffer = FrameBuffers.Find( SearchKey );
				GLCHECK( glDeleteFramebuffers( 1, &FrameBuffer->FBO ) );

				FrameBuffers.Remove( SearchKey );
				bReferencesRemoved = true;
				break;
			}
		}
	}
}

void FES2RHI::SetRenderTarget(FSurfaceRHIParamRef NewRenderTargetRHI,FSurfaceRHIParamRef NewDepthStencilTargetRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( Surface, NewRenderTarget );
	DYNAMIC_CAST_ES2RESOURCE( Surface, NewDepthStencilTarget );

	if ( NewRenderTarget )
	{
		GStateShadow.RenderTargetWidth = NewRenderTarget->GetWidth();
		GStateShadow.RenderTargetHeight = NewRenderTarget->GetHeight();
	}

	INT NewRenderTargetID = NewRenderTarget ? NewRenderTarget->GetUniqueID() : -1;
	INT NewDepthStencilTargetID = NewDepthStencilTarget ? NewDepthStencilTarget->GetUniqueID() : -1;

	// Check if we're setting a NULL depth buffer and we can override it with the previous depth buffer, to avoid a buffer flush & restore
	GStateShadow.bIsUsingDummyDepthStencilBuffer = FALSE;

	// @TODO cleanup: For now, disable this optimization as it conflicts with the fix for shadow rendering 

	extern UBOOL GMobileForceSetRenderTarget;
	FES2Surface* CurrentDepthTarget = ES2CAST( FES2Surface, GStateShadow.CurrentDepthTargetRHI );
	if ( !GMobileForceSetRenderTarget && NewRenderTarget && NewRenderTargetID == GStateShadow.CurrentRenderTargetID && NewDepthStencilTarget == NULL && CurrentDepthTarget )
	{
		if ( NewRenderTarget->GetWidth() == CurrentDepthTarget->GetWidth() &&
			 NewRenderTarget->GetHeight() == CurrentDepthTarget->GetHeight() )
		{
			NewDepthStencilTargetRHI = GStateShadow.CurrentDepthTargetRHI;
			NewDepthStencilTargetID = GStateShadow.CurrentDepthStencilTargetID;
			GStateShadow.bIsUsingDummyDepthStencilBuffer = TRUE;
		}
	}

	if ( NewRenderTargetID == GStateShadow.CurrentRenderTargetID && NewDepthStencilTargetID == GStateShadow.CurrentDepthStencilTargetID )
	{
		return;
	}

	GStateShadow.CurrentRenderTargetRHI			= NewRenderTargetRHI;
	GStateShadow.CurrentDepthTargetRHI 			= NewDepthStencilTargetRHI;
	GStateShadow.CurrentRenderTargetID			= NewRenderTargetID;
	GStateShadow.CurrentDepthStencilTargetID	= NewDepthStencilTargetID;

	if (NewRenderTarget == NULL && NewDepthStencilTarget == NULL)
	{
		// unset any framebuffer as much as we can
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		return;
	}

	if (NewRenderTarget == NULL)
	{
		return;
	}

	FES2FrameBuffer* FrameBuffer = GRenderManager.FindOrCreateFrameBuffer(NewRenderTargetRHI, NewDepthStencilTargetRHI);

	// make it the current framebuffer
	GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, FrameBuffer->FBO));

	// UE3 expects SetRenderTarget to reset the viewport
	GShaderManager.SetViewport(0, 0, GStateShadow.RenderTargetWidth, GStateShadow.RenderTargetHeight);
} 

void FES2RHI::SetViewport(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ) 
{ 
	if ( GThreeTouchMode != ThreeTouchMode_TinyViewport ) 
	{
		// @todo flash: Flash had different logic to never to the Height-MaxY thing
        FES2Surface* ColorRenderTarget = ES2CAST( FES2Surface, GStateShadow.CurrentRenderTargetRHI );
        if ( ColorRenderTarget && ColorRenderTarget->GetResolveTexture() && !ColorRenderTarget->HasValidRenderBuffer() )
        {
            GShaderManager.SetViewport( MinX, MinY, MaxX - MinX, MaxY - MinY );
        }
        else
        {
    		GShaderManager.SetViewport( MinX, GStateShadow.RenderTargetHeight - MaxY, MaxX - MinX, MaxY - MinY );
        }
	}
	GLCHECK( glDepthRangef( MinZ, MaxZ ) );
} 



void FES2RHI::BeginDrawingViewport(FViewportRHIParamRef ViewportRHI) 
{
	DYNAMIC_CAST_ES2RESOURCE(Viewport,Viewport);

	// switch to this viewport if needed
	FES2Core::MakeCurrent(Viewport);

	extern UBOOL GForceTextureBind;
	GForceTextureBind = FALSE;

	// Begin a new frame
	GShaderManager.NewFrame();
	GRenderManager.NewFrame();

#if IPHONE
	// If there is a pending request to toggle MSAA state, do it now
	if( GMSAAToggleRequest )
	{
		GMSAAEnabled = !GMSAAEnabled;
		// Now bind the set we want to use
		if( GMSAAEnabled )
		{
			RHISetRenderTarget(Viewport->ViewportBackBufferMSAA, Viewport->ViewportDepthBufferMSAA);
		}
		else
		{
			RHISetRenderTarget(Viewport->ViewportBackBuffer, Viewport->ViewportDepthBuffer);
		}
		GMSAAToggleRequest = FALSE;
	}
#endif

	GLCHECK(glEnable( GL_DEPTH_TEST ));

	if ( GThreeTouchMode == ThreeTouchMode_TinyViewport ) 
	{
		GLCHECK(glViewport(0, 0, 60, 60));
		GLCHECK(glScissor(0, 0, 60, 60));
		GLCHECK(glEnable( GL_SCISSOR_TEST ));		
	}
	else 
	{
		GLCHECK(glDisable( GL_SCISSOR_TEST ));
	}
} 

void FES2RHI::EndDrawingViewport(FViewportRHIParamRef ViewportRHI,UBOOL bPresent,UBOOL bLockToVsync) 
{
	DYNAMIC_CAST_ES2RESOURCE(Viewport,Viewport);

	extern UBOOL GForceTextureBind;
	GForceTextureBind = TRUE;

	if (bPresent)
	{
		FES2Core::SwapBuffers(Viewport);
	}

#if ENABLE_OFFSCREEN_RENDERING
	extern GLuint GOffScreenFrameBuffer;
	extern GLuint GOffScreenWidth;
	extern GLuint GOffScreenHeight;
	GLCHECK(glBindFramebuffer( GL_FRAMEBUFFER, GOffScreenFrameBuffer ));
	RHISetViewport( 0, 0, 0.0f, GOffScreenWidth, GOffScreenHeight, 1.0f );
#endif
} 

UBOOL FES2RHI::IsDrawingViewport() 
{ 
	// @todo: We could implement this in ES2Core most likely
	return FALSE; 
} 



void FES2RHI::SetStreamSource(UINT StreamIndex,FVertexBufferRHIParamRef VertexBuffer,UINT Stride,UINT Offset,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances) 
{
	GRenderManager.SetStream(StreamIndex, VertexBuffer, Stride, Offset);

	checkf(NumInstances <= 1, TEXT("Instanced rendering is not supported on iPhone yet"));
}



void FES2RHI::SetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( BoundShaderState, BoundShaderState );
	GRenderManager.SetVertexDeclaration(BoundShaderState->VertexDeclaration);
	GShaderManager.SetNextDrawGlobalShader(BoundShaderState->MobileGlobalShaderType);
}

inline void DrawElements(GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices) 
{
	STAT(if( !GES2TimeNextDrawCall ))
	{
		GLCHECK(glDrawElements( Mode, Count, Type, Indices ) );
	}
#if STATS
	else
	{
		DOUBLE ProfileDrawTime = 0;
		{
			SCOPE_SECONDS_COUNTER(ProfileDrawTime);
			GLCHECK(glDrawElements( Mode, Count, Type, Indices ) );
		}
		INC_FLOAT_STAT_BY(STAT_ES2ShaderCache1stDrawTime,(FLOAT)ProfileDrawTime);
		GES2TimeNextDrawCall = FALSE;
	}
#endif
}


void FES2RHI::DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives) 
{ 
	DYNAMIC_CAST_ES2RESOURCE( IndexBuffer, IndexBuffer );

	// set the attribute inputs
	UBOOL bUpdateWasSuccessful = GRenderManager.UpdateAttributesAndProgram();
	
	// set the index buffer, if needed
	if( GStateShadow.ElementArrayBuffer != IndexBuffer->GetBufferName() )
	{
		GStateShadow.ElementArrayBuffer = IndexBuffer->GetBufferName();
		GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer->GetBufferName()));
	}
	
	// calculate now much to draw
	DWORD NumPrimitivesToDraw = CalcNumElements(PrimitiveType, NumPrimitives);
	DWORD IndexBytes = IndexBuffer->GetStride();

	// Only issue the draw call if the update of the attributes and program was successful
	if (bUpdateWasSuccessful
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		&& (GShaderManager.IsCurrentPrimitiveTracked() || GIsDrawingStats)
#endif
	   )
	{
		INC_DWORD_STAT(STAT_DrawCalls);
		INC_DWORD_STAT_BY(STAT_PrimitivesDrawn, NumPrimitives);
		// draw it!
		DrawElements(TranslateUnrealPrimitiveTypeToGLType(PrimitiveType), NumPrimitivesToDraw, GL_UNSIGNED_SHORT, (void*)(StartIndex * IndexBytes));
	}
	GShaderManager.NextPrimitive();
} 


void FES2RHI::DrawIndexedPrimitive_PreVertexShaderCulling(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives,const FMatrix& LocalToWorld,const void* PlatformMeshData) 
{ 
	// we don't have prevertex shader culling, just call through to the standard
	RHIDrawIndexedPrimitive(IndexBuffer, PrimitiveType, BaseVertexIndex, MinIndex, NumVertices, StartIndex, NumPrimitives);
} 


void FES2RHI::BeginDrawIndexedPrimitiveUP(UINT PrimitiveType,UINT NumPrimitives,UINT NumVertices,UINT VertexDataStride,void*& OutVertexData,UINT MinVertexIndex,UINT NumIndices,UINT IndexDataStride,void*& OutIndexData) 
{ 
	check(IndexDataStride == 2);
	
	// figure out the size of the needed buffers
	UINT VertexDataSize = VertexDataStride * NumVertices;
	UINT IndexDataSize = IndexDataStride * NumIndices;
	
	// allocate space
	OutVertexData = GRenderManager.AllocateVertexData(VertexDataSize);
	OutIndexData = GRenderManager.AllocateIndexData(IndexDataSize);

	// remember required values for End time
	GRenderManager.CacheUPValues(PrimitiveType, VertexDataStride, NumPrimitives, OutVertexData, OutIndexData, VertexDataSize);
} 


void FES2RHI::EndDrawIndexedPrimitiveUP() 
{
	// set the attribute inputs to use the vertexdata passed in
	UBOOL bUpdateWasSuccessful = GRenderManager.UpdateAttributesAndProgram(GRenderManager.GetCachedVertexData(), GRenderManager.GetCachedVertexStride(), GRenderManager.GetCachedVertexDataSize());

	// unbind the index buffer object
	GStateShadow.ElementArrayBuffer = 0;
	GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

	const DWORD NumPrimitives = GRenderManager.GetCachedNumPrimitives();
	DWORD NumPrimitivesToDraw = CalcNumElements(GRenderManager.GetCachedPrimitiveType(), NumPrimitives);

	// Only issue the draw call if the update of the attributes and program was successful
	if (bUpdateWasSuccessful 
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		&& (GShaderManager.IsCurrentPrimitiveTracked() || GIsDrawingStats)
#endif
	   )
	{
#if STATS
		if( !GIsDrawingStats )
		{
			INC_DWORD_STAT(STAT_DrawCallsUP);
			INC_DWORD_STAT_BY(STAT_PrimitivesDrawnUP, NumPrimitives);
		}
#endif

		DrawElements(TranslateUnrealPrimitiveTypeToGLType(GRenderManager.GetCachedPrimitiveType()),
			NumPrimitivesToDraw, GL_UNSIGNED_SHORT, GRenderManager.GetCachedIndexData());
	}
	GShaderManager.NextPrimitive();

	// release the vertex and index data buffers we used
	GRenderManager.DeallocateVertexData();
	GRenderManager.DeallocateIndexData();
} 


void FES2RHI::DrawIndexedPrimitiveUP(UINT PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT NumPrimitives,const void* IndexData,UINT IndexDataStride,const void* VertexData,UINT VertexDataStride) 
{ 
	check(IndexDataStride == 2);

	// @todo es2: For non-static data passed in, we should maybe copy it to the ring buffer? Haven't seen any
	// issues however yet, so not worth the slow memcpy if not needed

    DWORD NumPrimitivesToDraw = CalcNumElements(PrimitiveType, NumPrimitives);
    
	// set the attribute inputs to use the vertexdata passed in
	UBOOL bUpdateWasSuccessful = GRenderManager.UpdateAttributesAndProgram(VertexData, VertexDataStride, NumPrimitivesToDraw * VertexDataStride);
	
	// unbind the index buffer object
	GStateShadow.ElementArrayBuffer = 0;
	GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));


	// Only issue the draw call if the update of the attributes and program was successful
	if (bUpdateWasSuccessful
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		&& (GShaderManager.IsCurrentPrimitiveTracked() || GIsDrawingStats)
#endif
	   )		
	{
		if( !GIsDrawingStats )
		{
			INC_DWORD_STAT(STAT_DrawCallsUP);
			INC_DWORD_STAT_BY(STAT_PrimitivesDrawnUP, NumPrimitives);
		}

		DrawElements(TranslateUnrealPrimitiveTypeToGLType(PrimitiveType), NumPrimitivesToDraw, GL_UNSIGNED_SHORT, IndexData);
	}
	GShaderManager.NextPrimitive();
} 


/**
 * Draw a point sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
void FES2RHI::DrawSpriteParticles(const FMeshBatch& Mesh) 
{ 
	// draw sprites as UP data (mostly copied from DX9 code)
	FDynamicSpriteEmitterData* SpriteData = (FDynamicSpriteEmitterData*)Mesh.DynamicVertexData;
	
	INT ParticleCount = SpriteData->Source.ActiveParticleCount;
	
	// clamp thenumber of particles drawn
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SpriteData->Source.MaxDrawCount >= 0) && (ParticleCount > SpriteData->Source.MaxDrawCount))
	{
		ParticleCount = SpriteData->Source.MaxDrawCount;
	}
	
	// render the particles as indexed tri-list
	void* OutVertexData = NULL;
	void* OutIndexData = NULL;
	
	RHIBeginDrawIndexedPrimitiveUP(PT_TriangleList, ParticleCount * 2, ParticleCount * 4, Mesh.DynamicVertexStride, 
								   OutVertexData, 0, ParticleCount * 6, sizeof(WORD), OutIndexData);
	
	if (OutVertexData && OutIndexData)
	{
		FParticleSpriteVertex* Vertices = (FParticleSpriteVertex*)OutVertexData;
		// todo : support batching
		SpriteData->GetVertexAndIndexData(Vertices, OutIndexData, (FParticleOrder*)(Mesh.Elements(0).DynamicIndexData));
		RHIEndDrawIndexedPrimitiveUP();
	}
}


/**
 * Draw a point sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
void FES2RHI::DrawPointSpriteParticles(const FMeshBatch& Mesh) 
{ 
	// Not implemented!
}

void FES2RHI::DrawSubUVParticles(const FMeshBatch& Mesh) 
{ 
	// draw sprites as UP data (mostly copied from DX9 code)
	FDynamicSubUVEmitterData* SubUVData = (FDynamicSubUVEmitterData*)Mesh.DynamicVertexData;
	
	INT ParticleCount = SubUVData->Source.ActiveParticleCount;
	
	// clamp thenumber of particles drawn
	INT StartIndex = 0;
	INT EndIndex = ParticleCount;
	if ((SubUVData->Source.MaxDrawCount >= 0) && (ParticleCount > SubUVData->Source.MaxDrawCount))
	{
		ParticleCount = SubUVData->Source.MaxDrawCount;
	}
	
	// render the particles as indexed tri-list
	void* OutVertexData = NULL;
	void* OutIndexData = NULL;
	
	RHIBeginDrawIndexedPrimitiveUP(PT_TriangleList, ParticleCount * 2, ParticleCount * 4, Mesh.DynamicVertexStride, 
								   OutVertexData, 0, ParticleCount * 6, sizeof(WORD), OutIndexData);
	
	if (OutVertexData && OutIndexData)
	{
		FParticleSpriteVertex* Vertices = (FParticleSpriteVertex*)OutVertexData;
		// todo : support batching
		SubUVData->GetVertexAndIndexData(Vertices, OutIndexData, (FParticleOrder*)(Mesh.Elements(0).DynamicIndexData));
		RHIEndDrawIndexedPrimitiveUP();
	}	
} 


void FES2RHI::Clear(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil) 
{
	// avoid stale state leaking into next frame as per apple suggestion
	GLCHECK(glUseProgram(0));

	extern void ResetCurrentProgram();
	ResetCurrentProgram();

	// Are we currently using a dummy depthstencil buffer (the user set NULL and we're overriding that)?
	if ( GStateShadow.bIsUsingDummyDepthStencilBuffer )
	{
		// Make sure we're not overwriting the dummy depthstencil buffer, in case we're going to use it later.
		bClearDepth = FALSE;
		bClearStencil = FALSE;
	}

	// figure out what needs clearing, and make sure the state is enabled to be able to clear it
	GLuint Mask = 0;
	if( bClearColor )
	{
		GLCHECK(glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE ));
		Mask |= GL_COLOR_BUFFER_BIT;
	}
	if ( bClearDepth )
	{
		GLCHECK(glDepthMask( GL_TRUE ));
		Mask |= GL_DEPTH_BUFFER_BIT;
	}
	if ( bClearStencil )
	{
		GLCHECK(glStencilMask( 0xFFFFFFFF ));
		Mask |= GL_STENCIL_BUFFER_BIT;
	}

	// do the clear
	GLCHECK(glClearColor( Color.R, Color.G, Color.B, Color.A ));
	GLCHECK(glClearDepthf( Depth ));
	GLCHECK(glClearStencil( Stencil ));
	GLCHECK(glClear( Mask ));
} 


void FES2RHI::DrawPrimitive(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives) 
{ 
	// set the attribute inputs
	UBOOL bUpdateWasSuccessful = GRenderManager.UpdateAttributesAndProgram();
	
	// calculate now much to draw
	DWORD NumPrimitivesToDraw = CalcNumElements(PrimitiveType, NumPrimitives);

	// Only issue the draw call if the update of the attributes and program was successful
	if (bUpdateWasSuccessful
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
		&& (GShaderManager.IsCurrentPrimitiveTracked() || GIsDrawingStats)
#endif
	   )
	{
		INC_DWORD_STAT(STAT_DrawCalls);
		INC_DWORD_STAT_BY(STAT_PrimitivesDrawn, NumPrimitives);
		GLCHECK(glDrawArrays(TranslateUnrealPrimitiveTypeToGLType(PrimitiveType), BaseVertexIndex, NumPrimitivesToDraw));
	}
	GShaderManager.NextPrimitive();
} 

void FES2RHI::BeginDrawPrimitiveUP(UINT PrimitiveType,UINT NumPrimitives,UINT NumVertices,UINT VertexDataStride,void*& OutVertexData) 
{ 
	// figure out the size of the needed buffers
	UINT VertexDataSize = VertexDataStride * NumVertices;
	
	// allocate space
	OutVertexData = GRenderManager.AllocateVertexData(VertexDataSize);
	
	// remember required values for End time
	GRenderManager.CacheUPValues(PrimitiveType, VertexDataStride, NumPrimitives, OutVertexData, NULL, VertexDataSize);
} 


void FES2RHI::EndDrawPrimitiveUP() 
{ 
	// set the attribute inputs to use the vertexdata passed in
	UBOOL bUpdateWasSuccessful = GRenderManager.UpdateAttributesAndProgram(GRenderManager.GetCachedVertexData(), GRenderManager.GetCachedVertexStride(), GRenderManager.GetCachedVertexDataSize());

	DWORD NumPrimitivesToDraw = CalcNumElements(GRenderManager.GetCachedPrimitiveType(), GRenderManager.GetCachedNumPrimitives());
	INC_DWORD_STAT(STAT_DrawCallsUP);
	INC_DWORD_STAT_BY(STAT_PrimitivesDrawnUP, NumPrimitivesToDraw);

	// Only issue the draw call if the update of the attributes and program was successful
	STAT(if( !GES2TimeNextDrawCall ))
	{
		GLCHECK(glDrawArrays(TranslateUnrealPrimitiveTypeToGLType(GRenderManager.GetCachedPrimitiveType()), 0, NumPrimitivesToDraw));
	}
#if STATS
	else
	{
		DOUBLE ProfileDrawTime = 0;
		{
			SCOPE_SECONDS_COUNTER(ProfileDrawTime);
			GLCHECK(glDrawArrays(TranslateUnrealPrimitiveTypeToGLType(GRenderManager.GetCachedPrimitiveType()), 0, NumPrimitivesToDraw));
		}
		INC_FLOAT_STAT_BY(STAT_ES2ShaderCache1stDrawTime,(FLOAT)ProfileDrawTime);
		GES2TimeNextDrawCall = FALSE;
	}
#endif

	// release the vertex data buffer we used
	GRenderManager.DeallocateVertexData();
} 


void FES2RHI::DrawPrimitiveUP(UINT PrimitiveType, UINT NumPrimitives, const void* VertexData,UINT VertexDataStride) 
{ 
    DWORD NumPrimitivesToDraw = CalcNumElements(PrimitiveType, NumPrimitives);
    
	// set the attribute inputs to use the vertexdata passed in
	UBOOL bUpdateWasSuccessful = GRenderManager.UpdateAttributesAndProgram(VertexData, VertexDataStride, NumPrimitivesToDraw * VertexDataStride);
	
	INC_DWORD_STAT(STAT_DrawCallsUP);
	INC_DWORD_STAT_BY(STAT_PrimitivesDrawnUP, NumPrimitives);

	// Only issue the draw call if the update of the attributes and program was successful
	STAT(if( !GES2TimeNextDrawCall ))
	{
		GLCHECK(glDrawArrays(TranslateUnrealPrimitiveTypeToGLType(PrimitiveType), 0, NumPrimitivesToDraw));
	}
#if STATS
	else
	{
		DOUBLE ProfileDrawTime = 0;
		{
			SCOPE_SECONDS_COUNTER(ProfileDrawTime);
			GLCHECK(glDrawArrays(TranslateUnrealPrimitiveTypeToGLType(PrimitiveType), 0, NumPrimitivesToDraw));
		}
		INC_FLOAT_STAT_BY(STAT_ES2ShaderCache1stDrawTime,(FLOAT)ProfileDrawTime);
		GES2TimeNextDrawCall = FALSE;
	}
#endif
	GShaderManager.NextPrimitive();
} 

void FES2RHI::ReadSurfaceData (FSurfaceRHIParamRef SurfaceRHI,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData, FReadSurfaceDataFlags InFlags) 
{ 
	DYNAMIC_CAST_ES2RESOURCE(Surface,Surface);

	// backup the old FBO as we need to change it
	GLint OldFBO;
	GLCHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &OldFBO));

	FES2FrameBuffer* FrameBuffer = GRenderManager.FindOrCreateFrameBuffer(SurfaceRHI, 0);

	// so the following glReadPixels reads from the right spot
	GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, FrameBuffer->FBO));

	OutData.Add(4 * ((1 + MaxX - MinX) * (1 + MaxY - MinY)) - OutData.Num());

	GLCHECK(glFinish());

	TArray<BYTE> TempMem;
	TempMem.Add(OutData.Num());
	GLCHECK(glReadPixels(MinX, MinY, MaxX + 1, MaxY + 1, GL_RGBA, GL_UNSIGNED_BYTE, TempMem.GetData()));

	BYTE* Source = TempMem.GetData();
	BYTE* Dest = OutData.GetData();
	
	for (INT Y = MaxY; Y >= (INT)MinY; Y--)
	{
		for (UINT X = MinX; X <= MaxX; X++)
		{
			UINT SourceTexelStart = (Y * (MaxX + 1) + X) * 4;
			UINT DestTexelStart = ((MaxY - Y) * (MaxX + 1) + X) * 4;

			// flip the image upside down, and swap red/blue
			Dest[DestTexelStart + 0] = Source[SourceTexelStart + 2];
			Dest[DestTexelStart + 1] = Source[SourceTexelStart + 1];
			Dest[DestTexelStart + 2] = Source[SourceTexelStart + 0];
			Dest[DestTexelStart + 3] = Source[SourceTexelStart + 3];
		}
	}

	// restore old FBO as we had to change it
	GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, OldFBO));
} 

#endif
