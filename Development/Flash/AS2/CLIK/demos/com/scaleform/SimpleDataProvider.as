import flash.external.ExternalInterface; 
import gfx.events.EventDispatcher;

/**
 * Defines the methods that all DataProviders should expose. Note that this interface is not implemented by the existing components, and does not need to be implemented, it is just a reference. There are additional properties and getter/setters defined in the interface, which are commented out for compiler compatibility.
 */
class com.scaleform.SimpleDataProvider extends EventDispatcher {
	
	public var length = 0;
	
	public function SimpleDataProvider() { 
		super();		
	}
	
	public function initialize(list:Object):Void {
		ExternalInterface.call("Simple.requestLength", this, "invalidate");		
	}
	
	public function requestItemRange(startIndex:Number, endIndex:Number, scope:Object, callBack:String):Array
	{
		ExternalInterface.call("Simple.requestItemRange", startIndex, endIndex, scope, callBack);
		return null;
	}
	
	public function invalidate(length:Number):Void
	{
		this.length = length;
		dispatchEvent({type:"change"});
	}

}