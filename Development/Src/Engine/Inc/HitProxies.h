/*=============================================================================
	HitProxies.h: Hit proxy definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * The priority a hit proxy has when choosing between several hit proxies near the point the user clicked.
 * HPP_World - this is the default priority
 * HPP_Wireframe - the priority of items that are drawn in wireframe, such as volumes
 * HPP_UI - the priority of the UI components such as the translation widget
 */
enum EHitProxyPriority
{
	HPP_World = 0,
	HPP_Wireframe = 1,
	HPP_Foreground = 2,
	HPP_UI = 3
};

/**
 * Represents a hit proxy class for runtime type checks.
 */
class HHitProxyType
{
public:
	HHitProxyType(HHitProxyType* InParent,const TCHAR* InName):
		Parent(InParent),
		Name(InName)
	{}
	HHitProxyType* GetParent() const { return Parent; }
	const TCHAR* GetName() const { return Name; }
private:
	HHitProxyType* Parent;
	const TCHAR* Name;
};

/**
 * A macro which creates a HHitProxyType for a HHitProxy-derived class.
 */
#define DECLARE_HIT_PROXY_BASE(TypeName,ParentType) \
	public: \
	static HHitProxyType* StaticGetType() \
	{ \
		static HHitProxyType StaticType(ParentType,TEXT(#TypeName)); \
		return &StaticType; \
	} \
	virtual HHitProxyType* GetType() const \
	{ \
		return StaticGetType(); \
	}

#define DECLARE_HIT_PROXY(TypeName,ParentTypeName) \
	DECLARE_HIT_PROXY_BASE(TypeName,ParentTypeName::StaticGetType())

/**
 * Encapsulates a hit proxy ID.
 */
class FHitProxyId
{
	friend class HHitProxy;
public:

	/** Default constructor. */
	FHitProxyId(): Index(INDEX_NONE) {}

	/** Color conversion constructor. */
	FHitProxyId(FColor Color);

	/**
	 * Maps the ID to a color which can be used to represent the ID.
	 */
	FColor GetColor() const;

	/**
	 * Maps a hit proxy ID to its hit proxy.  If the ID doesn't map to a valid hit proxy, NULL is returned.
	 * @param ID - The hit proxy ID to match.
	 * @return The hit proxy with matching ID, or NULL if no match.
	 */
	friend class HHitProxy* GetHitProxyById(FHitProxyId Id);

private:
	
	/** Initialization constructor. */
	FHitProxyId(INT InIndex): Index(InIndex) {}

	/** A uniquely identifying index for the hit proxy. */
	INT Index;
};

/**
 * Base class for detecting user-interface hits.
 */
class HHitProxy : public FRefCountedObject
{
	DECLARE_HIT_PROXY_BASE(HHitProxy,NULL)
public:

	/** The priority a hit proxy has when choosing between several hit proxies near the point the user clicked. */
	const EHitProxyPriority Priority;

	/** Used in the ortho views, defaults to the same value as Priority */
	const EHitProxyPriority OrthoPriority;

	/** The hit proxy's ID. */
	FHitProxyId Id;

	HHitProxy(EHitProxyPriority InPriority = HPP_World);
	HHitProxy(EHitProxyPriority InPriority, EHitProxyPriority InOrthoPriority);
	virtual ~HHitProxy();
	virtual void Serialize(FArchive& Ar) {}

	/**
	 * Determines whether the hit proxy is of the given type.
	 */
	UBOOL IsA(HHitProxyType* TestType) const;

	/**
		Override to change the mouse based on what it is hovering over.
	*/
	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Arrow;
	}

	/**
	 * Method that specifies whether the hit proxy *always* allows translucent primitives to be associated with it or not,
	 * regardless of any other engine/editor setting. For example, if translucent selection was disabled, any hit proxies
	 * returning TRUE would still allow translucent selection.
	 *
	 * @return	TRUE if translucent primitives are always allowed with this hit proxy; FALSE otherwise
	 */
	virtual UBOOL AlwaysAllowsTranslucentPrimitives() const
	{
		return FALSE;
	}

private:
	void InitHitProxy();
};

/**
 * Hit proxy class for UObject references.
 */
struct HObject : HHitProxy
{
	DECLARE_HIT_PROXY(HObject,HHitProxy);

	UObject*	Object;

	HObject(UObject* InObject): HHitProxy(HPP_UI), Object(InObject) {}
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Object;
	}
};

/**
 * An interface to a hit proxy consumer.
 */
class FHitProxyConsumer
{
public:

	/**
	 * Called when a new hit proxy is rendered.  The hit proxy consumer should keep a TRefCountPtr to the HitProxy to prevent it from being
	 * deleted before the rendered hit proxy map.
	 */
	virtual void AddHitProxy(HHitProxy* HitProxy) = 0;
};
