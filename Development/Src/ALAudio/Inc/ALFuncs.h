/*=============================================================================
	ALFuncs.h: OpenAL function-declaration macros.

	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
		* Ported to Linux by Ryan C. Gordon.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Standard OpenAL functions.
-----------------------------------------------------------------------------*/

AL_EXT(_AL)

// OpenAL.
AL_PROC(_AL,ALvoid,alEnable,( ALenum capability ))
AL_PROC(_AL,ALvoid,alDisable,( ALenum capability ))
AL_PROC(_AL,ALboolean,alIsEnabled,( ALenum capability ))
//AL_PROC(_AL,ALvoid,alHint,( ALenum target, ALenum mode ))
AL_PROC(_AL,ALboolean,alGetBoolean,( ALenum param ))
AL_PROC(_AL,ALint,alGetInteger,( ALenum param ))
AL_PROC(_AL,ALfloat,alGetFloat,( ALenum param ))
AL_PROC(_AL,ALdouble,alGetDouble,( ALenum param ))
AL_PROC(_AL,ALvoid,alGetBooleanv,( ALenum param, ALboolean* data ))
AL_PROC(_AL,ALvoid,alGetIntegerv,( ALenum param, ALint* data ))
AL_PROC(_AL,ALvoid,alGetFloatv,( ALenum param, ALfloat* data ))
AL_PROC(_AL,ALvoid,alGetDoublev,( ALenum param, ALdouble* data ))
AL_PROC(_AL,ALubyte*,alGetString,( ALenum param ))
AL_PROC(_AL,ALenum,alGetError,( ALvoid ))
AL_PROC(_AL,ALboolean,alIsExtensionPresent,( ALubyte* fname ))
AL_PROC(_AL,ALvoid*,alGetProcAddress,( ALubyte* fname ))
AL_PROC(_AL,ALenum,alGetEnumValue,( ALubyte* ename ))
AL_PROC(_AL,ALvoid,alListeneri,( ALenum param, ALint value ))
AL_PROC(_AL,ALvoid,alListenerf,( ALenum param, ALfloat value ))
AL_PROC(_AL,ALvoid,alListener3f,( ALenum param, ALfloat v1, ALfloat v2, ALfloat v3 ))
AL_PROC(_AL,ALvoid,alListenerfv,( ALenum param, ALfloat* values ))
AL_PROC(_AL,ALvoid,alGetListeneri,( ALenum param, ALint* value ))
AL_PROC(_AL,ALvoid,alGetListenerf,( ALenum param, ALfloat* value ))
AL_PROC(_AL,ALvoid,alGetListener3f,( ALenum param, ALfloat* v1, ALfloat* v2, ALfloat* v3 ))
AL_PROC(_AL,ALvoid,alGetListenerfv,( ALenum param, ALfloat* values ))
AL_PROC(_AL,ALvoid,alGenSources,( ALsizei n, ALuint* sources )) 
AL_PROC(_AL,ALvoid,alDeleteSources,( ALsizei n, ALuint* sources ))
AL_PROC(_AL,ALboolean,alIsSource,( ALuint id )) 
AL_PROC(_AL,ALvoid,alSourcei,( ALuint source, ALenum param, ALint value )) 
AL_PROC(_AL,ALvoid,alSourcef,( ALuint source, ALenum param, ALfloat value )) 
AL_PROC(_AL,ALvoid,alSource3f,( ALuint source, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3 ))
AL_PROC(_AL,ALvoid,alSourcefv,( ALuint source, ALenum param, ALfloat* values )) 
AL_PROC(_AL,ALvoid,alGetSourcei,( ALuint source,  ALenum param, ALint* value ))
AL_PROC(_AL,ALvoid,alGetSourcef,( ALuint source,  ALenum param, ALfloat* value ))
//AL_PROC(_AL,ALvoid,alGetSource3f,( ALuint source,  ALenum param, ALfloat* v1, ALfloat* v2, ALfloat* v3 ))
AL_PROC(_AL,ALvoid,alGetSourcefv,( ALuint source, ALenum param, ALfloat* values ))
AL_PROC(_AL,ALvoid,alSourcePlayv,( ALsizei n, ALuint *sources ))
AL_PROC(_AL,ALvoid,alSourcePausev,( ALsizei n, ALuint *sources ))
AL_PROC(_AL,ALvoid,alSourceStopv,( ALsizei n, ALuint *sources ))
AL_PROC(_AL,ALvoid,alSourceRewindv,(ALsizei n,ALuint *sources))
AL_PROC(_AL,ALvoid,alSourcePlay,( ALuint source ))
AL_PROC(_AL,ALvoid,alSourcePause,( ALuint source ))
AL_PROC(_AL,ALvoid,alSourceStop,( ALuint source ))
AL_PROC(_AL,ALvoid,alSourceRewind,( ALuint source ))
AL_PROC(_AL,ALvoid,alGenBuffers,( ALsizei n, ALuint* buffers ))
AL_PROC(_AL,ALvoid,alDeleteBuffers,( ALsizei n, ALuint* buffers ))
AL_PROC(_AL,ALboolean,alIsBuffer,( ALuint buffer ))
AL_PROC(_AL,ALvoid,alBufferData,( ALuint   buffer, ALenum   format, ALvoid*  data, ALsizei  size, ALsizei  freq ))
AL_PROC(_AL,ALvoid,alGetBufferi,( ALuint buffer, ALenum param, ALint*   value ))
AL_PROC(_AL,ALvoid,alGetBufferf,( ALuint buffer, ALenum param, ALfloat* value ))
AL_PROC(_AL,ALvoid,alSourceQueueBuffers,( ALuint source, ALsizei n, ALuint* buffers ))
AL_PROC(_AL,ALvoid,alSourceUnqueueBuffers,( ALuint source, ALsizei n, ALuint* buffers ))
AL_PROC(_AL,ALvoid,alDistanceModel,( ALenum value ))
AL_PROC(_AL,ALvoid,alDopplerFactor,( ALfloat value ))
AL_PROC(_AL,ALvoid,alDopplerVelocity,( ALfloat value ))

AL_PROC(_AL,ALubyte*,alcGetString,(ALCdevice *device,ALenum param))
AL_PROC(_AL,ALvoid,alcGetIntegerv,(ALCdevice *device,ALenum param,ALsizei size,ALint *data))
AL_PROC(_AL,ALCdevice*,alcOpenDevice,(ALubyte *deviceName))
AL_PROC(_AL,ALvoid,alcCloseDevice,(ALCdevice *device))
AL_PROC(_AL,ALCcontext*,alcCreateContext,(ALCdevice *device,ALint *attrList))
AL_PROC(_AL,ALboolean,alcMakeContextCurrent,(ALCcontext *context))
AL_PROC(_AL,ALvoid,alcProcessContext,(ALCcontext *context))
AL_PROC(_AL,ALCcontext*,alcGetCurrentContext,(ALvoid))
AL_PROC(_AL,ALCdevice*,alcGetContextsDevice,(ALCcontext *context))
AL_PROC(_AL,ALvoid,alcSuspendContext,(ALCcontext *context))
AL_PROC(_AL,ALvoid,alcDestroyContext,(ALCcontext *context))
AL_PROC(_AL,ALenum,alcGetError,(ALCdevice *device))
//AL_PROC(_AL,ALboolean,alcIsExtensionPresent,(ALCdevice *device,ALubyte *extName))
//AL_PROC(_AL,ALvoid*,alcGetProcAddress,(ALCdevice *device,ALubyte *funcName))
//AL_PROC(_AL,ALenum,alcGetEnumValue,(ALCdevice *device,ALubyte *enumName))


/*-----------------------------------------------------------------------------
	OpenAL extensions.
-----------------------------------------------------------------------------*/

#if 0
//@todo audio: EAX here
// Paletted textures.
AL_EXT(_GL_EXT_paletted_texture)
AL_PROC(_GL_EXT_paletted_texture,void,glColorTableEXT,(GLenum target,GLenum internalFormat,GLsizei width,GLenum format,GLenum type,const void *data))
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

