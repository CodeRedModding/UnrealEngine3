/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// it doesn't seem worthwhile to risk changing Cast<>/IsA() to work with interfaces at this time,
// so we'll just make sure it fails... spectacularly
#define CATCH_UNSUPPORTED_INTERFACECAST(TheType) checkAtCompileTime((TheType::StaticClassFlags & CLASS_Interface) == 0, __CASTING_TO_INTERFACE_TYPE_##TheType##_IS_UNSUPPORTED)

// Dynamically cast an object type-safely.
template< typename T > T* Cast( UObject* Src )
{
	CATCH_UNSUPPORTED_INTERFACECAST(T);

	return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}
template< class T > FORCEINLINE T* ExactCast( UObject* Src )
{
	return Src && (Src->GetClass() == T::StaticClass()) ? (T*)Src : NULL;
}
template< class T > FORCEINLINE const T* ExactCastConst( const UObject* Src )
{
	return Src && (Src->GetClass() == T::StaticClass()) ? (T*)Src : NULL;
}

template< class T, class U > T* CastChecked( U* Src )
{
	CATCH_UNSUPPORTED_INTERFACECAST(T);

#if !FINAL_RELEASE
	if( !Src || !Src->IsA(T::StaticClass()) )
	{
		appErrorf( TEXT("Cast of %s to %s failed"), Src ? *Src->GetFullName() : TEXT("NULL"), *T::StaticClass()->GetName() );
	}
#endif
	return (T*)Src;
}
template< class T > const T* ConstCast( const UObject* Src )
{
	CATCH_UNSUPPORTED_INTERFACECAST(T);

	return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}

template< class T, class U > const T* ConstCastChecked( const U* Src )
{
	CATCH_UNSUPPORTED_INTERFACECAST(T);

#if !FINAL_RELEASE
	if( !Src || !Src->IsA(T::StaticClass()) )
	{
		appErrorf( TEXT("Cast of %s to %s failed"), Src ? *Src->GetFullName() : TEXT("NULL"), *T::StaticClass()->GetName() );
	}
#endif
	return (T*)Src;
}

/**
 * Dynamically casts an object pointer to an interface pointer.  InterfaceType must correspond to a native interface class.
 *
 * @param	Src		the object to be casted
 *
 * @return	a pointer to the interface type specified or NULL if the object doesn't support for specified interface
 */
template<typename InterfaceType>
FORCEINLINE InterfaceType* InterfaceCast( UObject* Src )
{
	return Src != NULL
		? static_cast<InterfaceType*>(Src->GetInterfaceAddress(InterfaceType::UClassType::StaticClass()))
		: NULL;
}
template<typename InterfaceType>
FORCEINLINE const InterfaceType* ConstInterfaceCast( const UObject* Src )
{
	return Src != NULL
		? static_cast<const InterfaceType*>(const_cast<UObject*>(Src)->GetInterfaceAddress(InterfaceType::UClassType::StaticClass()))
		: NULL;
}
template<typename InterfaceType>
TScriptInterface<InterfaceType> ScriptInterfaceCast( UObject* Src )
{
	TScriptInterface<InterfaceType> Result;
	InterfaceType* ISrc = InterfaceCast<InterfaceType>(Src);
	if ( ISrc != NULL )
	{
		Result.SetObject(Src);
		Result.SetInterface(ISrc);
	}
	return Result;
}


// specializations of cast methods
#if FINAL_RELEASE

#define DECLARE_CAST_BY_FLAG(ClassName) \
class ClassName; \
template<> FORCEINLINE ClassName* Cast( UObject* Src ) \
{ \
	return Src && Src->GetClass()->HasAnyCastFlag(CASTCLASS_##ClassName) ? (ClassName*)Src : NULL; \
} \
template<> FORCEINLINE const ClassName* ConstCast( const UObject* Src ) \
{ \
	return Src && Src->GetClass()->HasAnyCastFlag(CASTCLASS_##ClassName) ? (ClassName*)Src : NULL; \
} \
template<> FORCEINLINE ClassName* CastChecked( UObject* Src ) \
{ \
	return Cast<ClassName>(Src); \
} \
template<> FORCEINLINE const ClassName* ConstCastChecked( const UObject* Src ) \
{ \
	return ConstCast<ClassName>(Src); \
}

#else

#define DECLARE_CAST_BY_FLAG(ClassName) \
class ClassName; \
template<> FORCEINLINE ClassName* Cast( UObject* Src ) \
{ \
	return Src && Src->GetClass()->HasAnyCastFlag(CASTCLASS_##ClassName) ? (ClassName*)Src : NULL; \
} \
template<> FORCEINLINE const ClassName* ConstCast( const UObject* Src ) \
{ \
	return Src && Src->GetClass()->HasAnyCastFlag(CASTCLASS_##ClassName) ? (ClassName*)Src : NULL; \
} \
template<> FORCEINLINE ClassName* CastChecked( UObject* Src ) \
{ \
	if( !Src || !Src->GetClass()->HasAnyCastFlag(CASTCLASS_##ClassName) ) \
		appErrorf( TEXT("Cast of %s to ") TEXT(#ClassName) TEXT(" failed"), Src ? *Src->GetFullName() : TEXT("NULL")); \
	return (ClassName*)Src; \
} \
template<> FORCEINLINE const ClassName* ConstCastChecked( const UObject* Src ) \
{ \
	if( !Src || !Src->GetClass()->HasAnyCastFlag(CASTCLASS_##ClassName) ) \
		appErrorf( TEXT("Cast of %s to ") TEXT(#ClassName) TEXT(" failed"), Src ? *Src->GetFullName() : TEXT("NULL")); \
	return (ClassName*)Src; \
}

#endif

DECLARE_CAST_BY_FLAG(UField)
DECLARE_CAST_BY_FLAG(UConst)
DECLARE_CAST_BY_FLAG(UEnum)
DECLARE_CAST_BY_FLAG(UStruct)
DECLARE_CAST_BY_FLAG(UScriptStruct)
DECLARE_CAST_BY_FLAG(UState)
DECLARE_CAST_BY_FLAG(UClass)
DECLARE_CAST_BY_FLAG(UProperty)
DECLARE_CAST_BY_FLAG(UObjectProperty)
DECLARE_CAST_BY_FLAG(UBoolProperty)
DECLARE_CAST_BY_FLAG(UFunction)
DECLARE_CAST_BY_FLAG(UStructProperty)
DECLARE_CAST_BY_FLAG(UByteProperty)
DECLARE_CAST_BY_FLAG(UIntProperty)
DECLARE_CAST_BY_FLAG(UFloatProperty)
DECLARE_CAST_BY_FLAG(UComponentProperty)
DECLARE_CAST_BY_FLAG(UClassProperty)
DECLARE_CAST_BY_FLAG(UInterfaceProperty)
DECLARE_CAST_BY_FLAG(UNameProperty)
DECLARE_CAST_BY_FLAG(UStrProperty)
DECLARE_CAST_BY_FLAG(UArrayProperty)
DECLARE_CAST_BY_FLAG(UMapProperty)
DECLARE_CAST_BY_FLAG(UDelegateProperty)
DECLARE_CAST_BY_FLAG(UComponent)


template< class T > FORCEINLINE T* Cast( UObject* Src, EClassFlags Flag )
{
	CATCH_UNSUPPORTED_INTERFACECAST(T);
	return Cast<T>(Src);
}
template< class T > FORCEINLINE const T* ConstCast( const UObject* Src, EClassFlags Flag )
{
	CATCH_UNSUPPORTED_INTERFACECAST(T);
	return ConstCast<T>(Src);
}

