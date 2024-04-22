/*=============================================================================
	UnScriptMacros.h: UnrealScript execution engine.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Macros.
-----------------------------------------------------------------------------*/

#define INIT_OPTX_EVAL GRuntimeUCFlags&=~RUC_SkippedOptionalParm;
#define ZERO_INIT(typ,var) appMemzero(&var,sizeof(typ));

//
// Macros for grabbing parameters for native functions.
//
#define P_GET_UBOOL(var)                   DWORD var=0;                                            Stack.Step( Stack.Object, &var    ); var = var ? TRUE : FALSE; // translate the bitfield into a UBOOL type for non-intel platforms
#define P_GET_UBOOL_OPTX(var,def)          DWORD var=def;                           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    ); var = var ? TRUE : FALSE; // translate the bitfield into a UBOOL type for non-intel platforms

#define P_GET_STRUCT(typ,var)              typ   var;                                              Stack.Step( Stack.Object, &var    );
#define P_GET_STRUCT_OPTX(typ,var,def)     typ   var;                               INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    ); if( (GRuntimeUCFlags&RUC_SkippedOptionalParm) != 0 ) {var=def;}
#define P_GET_STRUCT_REF(typ,var)          typ   var##T; GPropAddr=0;                              Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); typ*    p##var = (typ    *)GPropAddr; typ&     var = GPropAddr ? *(typ    *)GPropAddr:var##T;
#define P_GET_STRUCT_OPTX_REF(typ,var,def) typ   var##T; GPropAddr=0;               INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); typ*    p##var = (typ    *)GPropAddr; typ&     var = GPropAddr ? *(typ    *)GPropAddr:var##T; if( (GRuntimeUCFlags&RUC_SkippedOptionalParm) != 0 ) {var=def;}

#define P_GET_INT(var)                     INT   var=0;                                            Stack.Step( Stack.Object, &var    );
#define P_GET_INT_OPTX(var,def)            INT   var=def;                           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_INT_REF(var)                 INT   var##T=0; GPropAddr=0;                            Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); INT*     p##var = (INT    *)GPropAddr; INT&     var = GPropAddr ? *(INT    *)GPropAddr:var##T;
#define P_GET_INT_OPTX_REF(var,def)        INT   var##T=def; GPropAddr=NULL;        INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); INT*     p##var = (INT    *)GPropAddr; INT&     var = GPropAddr ? *(INT    *)GPropAddr:var##T;

#define P_GET_FLOAT(var)                   FLOAT var=0.f;                                          Stack.Step( Stack.Object, &var    );
#define P_GET_FLOAT_OPTX(var,def)          FLOAT var=def;                           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_FLOAT_REF(var)               FLOAT var##T=0.f; GPropAddr=0;                          Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); FLOAT*   p##var = (FLOAT  *)GPropAddr; FLOAT&   var = GPropAddr ? *(FLOAT  *)GPropAddr:var##T;
#define P_GET_FLOAT_OPTX_REF(var,def)      FLOAT var##T=def; GPropAddr=0;           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); FLOAT*   p##var = (FLOAT  *)GPropAddr; FLOAT&   var = GPropAddr ? *(FLOAT  *)GPropAddr:var##T;

#define P_GET_BYTE(var)                    BYTE  var=0;                                            Stack.Step( Stack.Object, &var    );
#define P_GET_BYTE_OPTX(var,def)           BYTE  var=def;                           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_BYTE_REF(var)                BYTE  var##T=0; GPropAddr=0;                            Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); BYTE*    p##var = (BYTE   *)GPropAddr; BYTE&    var = GPropAddr ? *(BYTE   *)GPropAddr:var##T;
#define P_GET_BYTE_OPTX_REF(var,def)       BYTE  var##T=def; GPropAddr=0;           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); BYTE*    p##var = (BYTE   *)GPropAddr; BYTE&    var = GPropAddr ? *(BYTE   *)GPropAddr:var##T;

#define P_GET_NAME(var)                    FName var=NAME_None;                                    Stack.Step( Stack.Object, &var    );
#define P_GET_NAME_OPTX(var,def)           FName var=def;                           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_NAME_REF(var)                FName var##T=NAME_None; GPropAddr=0;                    Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); FName*   p##var = (FName  *)GPropAddr; FName&   var = GPropAddr ? *(FName  *)GPropAddr:var##T;
#define P_GET_NAME_OPTX_REF(var,def)       FName var##T=def; GPropAddr=0;           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); FName*   p##var = (FName  *)GPropAddr; FName&   var = GPropAddr ? *(FName  *)GPropAddr:var##T;

#define P_GET_STR(var)                     FString var;                                            Stack.Step( Stack.Object, &var    );
#define P_GET_STR_OPTX(var,def)            FString var(def);                        INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_STR_REF(var)                 FString var##T; GPropAddr=0;                            Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); FString* p##var = (FString*)GPropAddr; FString& var = GPropAddr ? *(FString*)GPropAddr:var##T;
#define P_GET_STR_OPTX_REF(var,def)        FString var##T=def; GPropAddr=0;         INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); FString* p##var = (FString*)GPropAddr; FString& var = GPropAddr ? *(FString*)GPropAddr:var##T;

#define P_GET_OBJECT(cls,var)              cls*  var=NULL;                                         Stack.Step( Stack.Object, &var    );
#define P_GET_OBJECT_OPTX(cls,var,def)     cls*  var=def;                           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_OBJECT_REF(cls,var)          cls*  var##T=NULL; GPropAddr=0;                         Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); cls**    p##var = (cls   **)GPropAddr; cls*&    var = GPropAddr ? *(cls   **)GPropAddr:var##T;
#define P_GET_OBJECT_OPTX_REF(cls,var,def) cls* var##T=NULL; GPropAddr=0;           INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); cls**    p##var = (cls   **)GPropAddr; cls*&    var = GPropAddr ? *(cls   **)GPropAddr:var##T;

#define P_GET_ARRAY(typ,var)               typ   var[(MAX_VARIABLE_SIZE/sizeof(typ))+1];                               Stack.Step( Stack.Object,  var    );
#define P_GET_ARRAY_REF(typ,var)           typ var##T[(MAX_VARIABLE_SIZE/sizeof(typ))+1]; GPropAddr=0;  INIT_OPTX_EVAL Stack.Step( Stack.Object,  var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); typ*     var = GPropAddr ? (typ    *)GPropAddr:var##T;

#define P_GET_TARRAY(typ,var)              TArray<typ> var;                                        Stack.Step( Stack.Object, &var    );
#define P_GET_TARRAY_OPTX(typ,var,def)     TArray<typ> var;                         INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    ); if( (GRuntimeUCFlags&RUC_SkippedOptionalParm) != 0 ) {var=def;}
#define P_GET_TARRAY_REF(typ,var)          TArray<typ> var##T; GPropAddr=0;                        Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); TArray<typ>* p##var = (TArray<typ>*)GPropAddr; TArray<typ>& var = GPropAddr ? *(TArray<typ>*)GPropAddr:var##T;
#define P_GET_TARRAY_OPTX_REF(typ,var,def) TArray<typ> var##T; GPropAddr=0;         INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); TArray<typ>* p##var = (TArray<typ>*)GPropAddr; TArray<typ>& var = GPropAddr ? *(TArray<typ>*)GPropAddr:var##T; if( (GRuntimeUCFlags&RUC_SkippedOptionalParm) != 0 ) {var=def;}

#define P_GET_DELEGATE(var)                FScriptDelegate var(EC_EventParm);                      Stack.Step( Stack.Object, &var    );
#define P_GET_DELEGATE_OPTX(var,def)       FScriptDelegate var=def;                 INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    );
#define P_GET_DELEGATE_REF(var)            FScriptDelegate var##T(EC_EventParm); GPropAddr=0;      Stack.Step( Stack.Object, &var##T ); FScriptDelegate* p##var = (FScriptDelegate*)GPropAddr; FScriptDelegate& var = GPropAddr ? *(FScriptDelegate*)GPropAddr:var##T;
#define P_GET_DELEGATE_OPTX_REF(var,def)   FScriptDelegate var##T=def; GPropAddr=0; INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); FScriptDelegate* p##var = (FScriptDelegate*)GPropAddr; FScriptDelegate& var = GPropAddr ? *(FScriptDelegate*)GPropAddr:var##T;

#ifdef REQUIRES_ALIGNED_INT_ACCESS
#define P_GET_SKIP_OFFSET(var)             WORD var; {checkSlow(*Stack.Code==EX_Skip); Stack.Code++; appMemcpy(&var, Stack.Code, sizeof(WORD)); Stack.Code+=sizeof(WORD); }
#else
#define P_GET_SKIP_OFFSET(var)             WORD var; {checkSlow(*Stack.Code==EX_Skip); Stack.Code++; var=*(WORD*)Stack.Code; Stack.Code+=sizeof(WORD); }
#endif

#if CONSOLE // no script debugger on console
	#define P_FINISH                           Stack.Code++;
#else
	#define P_FINISH                           Stack.Code++; if ( *Stack.Code == EX_DebugInfo ) Stack.Step( Stack.Object, NULL );
#endif

// for retrieving structs which contain NoInit properties
#define P_GET_STRUCT_INIT(typ,var)              typ   var;                 ZERO_INIT(typ,var)                   Stack.Step( Stack.Object, &var    );
#define P_GET_STRUCT_INIT_OPTX(typ,var,def)     typ   var;                 ZERO_INIT(typ,var)    INIT_OPTX_EVAL Stack.Step( Stack.Object, &var    ); if( (GRuntimeUCFlags&RUC_SkippedOptionalParm) != 0 ) {var=def;}
#define P_GET_STRUCT_INIT_REF(typ,var)          typ   var##T; GPropAddr=0; ZERO_INIT(typ,var##T)                Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); typ*    p##var = (typ    *)GPropAddr; typ&     var = GPropAddr ? *(typ    *)GPropAddr:var##T;
#define P_GET_STRUCT_INIT_OPTX_REF(typ,var,def) typ   var##T; GPropAddr=0; ZERO_INIT(typ,var##T) INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); typ*    p##var = (typ    *)GPropAddr; typ&     var = GPropAddr ? *(typ    *)GPropAddr:var##T; if( (GRuntimeUCFlags&RUC_SkippedOptionalParm) != 0 ) {var=def;}

#define P_GET_INTERFACE(var)		           FScriptInterface var; Stack.Step(Stack.Object, &var);

#define P_GET_TINTERFACE(cls,var)		       TScriptInterface<cls> var;                                           Stack.Step(Stack.Object, &var);
#define P_GET_TINTERFACE_OPTX(cls,var,def)     TScriptInterface<cls> var(def,def);                   INIT_OPTX_EVAL Stack.Step(Stack.Object, &var);
#define P_GET_TINTERFACE_REF(cls,var)          TScriptInterface<cls> var##T;          GPropAddr=0;                  Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); TScriptInterface<cls>* p##var = (TScriptInterface<cls>*)GPropAddr; TScriptInterface<cls>& var = GPropAddr ? *(TScriptInterface<cls>*)GPropAddr:var##T;
#define P_GET_TINTERFACE_OPTX_REF(cls,var,def) TScriptInterface<cls> var##T(def,def); GPropAddr=0;   INIT_OPTX_EVAL Stack.Step( Stack.Object, &var##T ); if( GPropObject )GPropObject->NetDirty(GProperty); TScriptInterface<cls>* p##var = (TScriptInterface<cls>*)GPropAddr; TScriptInterface<cls>& var = GPropAddr ? *(TScriptInterface<cls>*)GPropAddr:var##T;

//
// Convenience macros.
//
#define P_GET_VECTOR2D(var)				P_GET_STRUCT(FVector2D,var)
#define P_GET_VECTOR2D_OPTX(var,def)	P_GET_STRUCT_OPTX(FVector2D,var,def)
#define P_GET_VECTOR2D_REF(var)			P_GET_STRUCT_REF(FVector2D,var)
#define P_GET_VECTOR2D_OPTX_REF(var,def)P_GET_STRUCT_OPTX_REF(FVector2D,var,def)
#define P_GET_VECTOR(var)				P_GET_STRUCT(FVector,var)
#define P_GET_VECTOR_OPTX(var,def)		P_GET_STRUCT_OPTX(FVector,var,def)
#define P_GET_VECTOR_REF(var)			P_GET_STRUCT_REF(FVector,var)
#define P_GET_VECTOR_OPTX_REF(var,def)  P_GET_STRUCT_OPTX_REF(FVector,var,def)
#define P_GET_ROTATOR(var)				P_GET_STRUCT(FRotator,var)
#define P_GET_ROTATOR_OPTX(var,def)		P_GET_STRUCT_OPTX(FRotator,var,def)
#define P_GET_ROTATOR_REF(var)			P_GET_STRUCT_REF(FRotator,var)
#define P_GET_ROTATOR_OPTX_REF(var,def) P_GET_STRUCT_OPTX_REF(FRotator,var,def)
#define P_GET_MATRIX(var)				P_GET_STRUCT(FMatrix,var)
#define P_GET_MATRIX_OPTX(var,def)		P_GET_STRUCT_OPTX(FMatrix,var,def)
#define P_GET_MATRIX_REF(var)			P_GET_STRUCT_REF(FMatrix,var)
#define P_GET_MATRIX_OPTX_REF(var,def)  P_GET_STRUCT_OPTX_REF(FMatrix,var,def)
#define P_GET_ACTOR(var)				P_GET_OBJECT(AActor,var)
#define P_GET_ACTOR_OPTX(var,def)		P_GET_OBJECT_OPTX(AActor,var,def)
#define P_GET_ACTOR_REF(var)			P_GET_OBJECT_REF(AActor,var)
#define P_GET_ACTOR_OPTX_REF(var,def)   P_GET_OBJECT_OPTX_REF(AActor,var,def)

//
// Iterator macros.
//
#define PRE_ITERATOR \
	INT wEndOffset = Stack.ReadWord(); \
	BYTE B=0; \
	DWORD Buffer[MAX_SIMPLE_RETURN_VALUE_SIZE_IN_DWORDS]; \
	BYTE *StartCode = Stack.Code; \
	do {
#define POST_ITERATOR \
		while( (B=*Stack.Code)!=EX_IteratorPop && B!=EX_IteratorNext ) \
			Stack.Step( Stack.Object, Buffer ); \
		if( *Stack.Code++==EX_IteratorNext ) \
			Stack.Code = StartCode; \
	} while( B != EX_IteratorPop );

// Exits an iterator within PRE/POST_ITERATOR
#define EXIT_ITERATOR \
	Stack.Code = &Stack.Node->Script(wEndOffset + 1);

// Skips iterator execution, without PRE/POST_ITERATOR
#define SKIP_ITERATOR \
	INT wEndOffset = Stack.ReadWord(); \
	Stack.Code = &Stack.Node->Script(wEndOffset + 1);
