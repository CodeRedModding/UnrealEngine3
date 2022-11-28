/*=============================================================================
	Windows.h: Misc window classes

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/**
 * This class provides common registration for serialization during garbage
 * collection. It is an abstract base class requiring you to implement the
 * Serialize() method.
 *
 * @author Joe Graf
 */
class WxSerializableWindow
{
public:
	/**
	 * This nested class is used to provide a UObject interface between the
	 * inheriting windows and the UObject system. It handles forwarding all
	 * calls of Serialize() to windows that register with it.
	 */
	class UWxWindowSerializer : public UObject
	{
		/**
		 * This is the list of windows that are requesting serialization events
		 */
		TArray<WxSerializableWindow*> SerializableWindows;

	public:
		DECLARE_CLASS(UWxWindowSerializer,UObject,CLASS_Transient,UnrealEd);

		/**
		 * Adds a window to the serialize list
		 *
		 * @param Window The window to add to the list
		 */
		void AddWindow(WxSerializableWindow* Window)
		{
			check(Window);
			// Make sure there are no duplicates. Should be impossible...
			SerializableWindows.AddUniqueItem(Window);
		}

		/**
		 * Removes a window from the list so it won't receive serialization events
		 *
		 * @param Window The window to remove from the list
		 */
		void RemoveWindow(WxSerializableWindow* Window)
		{
			check(Window);
			SerializableWindows.RemoveItem(Window);
		}

		/**
		 * Forwards this call to all registered windows so they can serialize
		 * any UObjects they depend upon
		 *
		 * @param Ar The archive to serialize with
		 */
		virtual void Serialize(FArchive& Ar)
		{
			Super::Serialize(Ar);
			// Let each registered window handle its serialization
			for (INT Index = 0; Index < SerializableWindows.Num(); Index++)
			{
				check(SerializableWindows(Index));
				SerializableWindows(Index)->Serialize(Ar);
			}
		}
	};

	/**
	 * The static window serializer object that is shared across all
	 * serializable windows
	 */
	static UWxWindowSerializer* GWxWindowSerializer;

	/**
	 * Initializes the global window serializer and adds it to the root set
	 */
	static void StaticInit(void)
	{
		if (GWxWindowSerializer == NULL)
		{
			GWxWindowSerializer = new UWxWindowSerializer();
			GWxWindowSerializer->AddToRoot();
		}
	}

	/**
	 * Tells the global object that forwards serialization calls on to windows
	 * that a new window is requiring serialization.
	 */
	WxSerializableWindow(void)
	{
		StaticInit();
		check(GWxWindowSerializer);
		// Add this instance to the serializer's list
		GWxWindowSerializer->AddWindow(this);
	}

	/**
	 * Removes this instance from the global serializer's list
	 */
	virtual ~WxSerializableWindow(void)
	{
		check(GWxWindowSerializer);
		// Remove this instance from the serializer's list
		GWxWindowSerializer->RemoveWindow(this);
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. Use this
	 * method to serialize any UObjects contained in windows that you wish to
	 * keep around.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar) = 0;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
