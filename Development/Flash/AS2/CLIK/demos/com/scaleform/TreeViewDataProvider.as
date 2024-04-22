import gfx.events.EventDispatcher;

import com.scaleform.TreeViewConstants;

class com.scaleform.TreeViewDataProvider extends EventDispatcher {
	
	public var length:Number = 0;

	private var root:Object;
	
	public function TreeViewDataProvider(r:Object) { 
		super();		
		root = r;
		preProcessRoot();
	}
	
	public function initialize(list:Object):Void
	{
		invalidate();
	}
		
	public function requestItemRange(startIndex:Number, endIndex:Number, scope:Object, callBack:String):Array
	{
		// create data array
		var data:Array = [];
		// find position and load data
		loadObjects(data, findObjectByIndex(startIndex), endIndex - startIndex);
		// pass data to list
		scope[callBack].call(scope, data);
		return null;
	}
	
	public function invalidate(length:Number):Void
	{
		dispatchEvent({type:"change"});
	}

	public function validateLength():Void {
		length = computeLength(root);
	}
	
	private function loadObjects(data:Array, currObj:Object, reqLen:Number) {		
		// add current object
		data.push(currObj);
		reqLen--;
		// load the rest
		while (currObj && reqLen >= 0) {
			// find next in sequence
			var lastObj:Object = currObj;
			if (lastObj.type == TreeViewConstants.TYPE_OPEN && lastObj.nodes) { currObj = lastObj.nodes[0]; }
			else { currObj = lastObj.nextSibling; }
			while (!currObj && lastObj.parent) {
				currObj = lastObj.parent.nextSibling; 
				lastObj = lastObj.parent;
			}
			if (currObj) {
				data.push(currObj);
				reqLen--;
			}
		}
		// fill nulls for rest
		while (reqLen >= 0) {
			data.push(null);
			reqLen--;
		}
	}
	
	private function computeLength(obj:Object):Number {
		var currObj:Object = obj;
		var endObj:Object = (obj.nextSibling?obj.nextSibling:obj.parent);
		var count:Number = 0;
		while (currObj && currObj!=endObj) {
			count++;			
			var lastObj:Object = currObj;
			if (lastObj.type == TreeViewConstants.TYPE_OPEN && lastObj.nodes) { currObj = lastObj.nodes[0]; }
			else { currObj = lastObj.nextSibling; }
			while (!currObj && lastObj.parent) {
				currObj = lastObj.parent.nextSibling; 
				lastObj = lastObj.parent;
			}
		}
		return count;
	}
	
	private function findObjectByIndex(index:Number):Object {
		var treeStack:Array = [root];
		var currIndex:Number = 0;
		var currObj:Object = root;
		for (var node:Object=treeStack.pop(); node; node=treeStack.pop()) {
			currObj = node;
			if (currIndex == index) { break; }
			var nodes:Array = node.nodes;
			if (nodes && node.type == TreeViewConstants.TYPE_OPEN) {
				for (var i:Number=nodes.length-1; i > -1; i--) {
					treeStack.push(nodes[i]); 
				}
			}
			currIndex++;			
		}
		return currObj;
	}
	
	private function preProcessRoot():Void {
		preProcess(root);
		root.isRoot = true;
		validateLength();
	}
	
	private function preProcess(node:Object, parent:Object, depth:Number, depthIcons:Array):Void {
		// init tree params
		node.depthIcons = depthIcons ? depthIcons : [];
		node.parent = parent ? parent : null;
		node.type = (node.nodes) ? TreeViewConstants.TYPE_CLOSED : TreeViewConstants.TYPE_LEAF;
		var nodes:Array = node.nodes;
		if (nodes) {
			var childDepthIcons:Array = new Array();
			for (var i=0; i<node.depthIcons.length; i++) { childDepthIcons.push(node.depthIcons[i]); }
			childDepthIcons.push( (node.nextSibling)?1:0 );
			for (var i:Number=0; i < nodes.length; i++) {
				nodes[i].nextSibling = nodes[i+1];							
				preProcess(nodes[i], node, depth?depth+1:1, childDepthIcons);
			}
		}
	}
	
	public function toString():String {
		var treeStack:Array = [root];
		var retStr:String = "";
		for (var node:Object=treeStack.pop(); node; node=treeStack.pop()) {
			for (var d:Number=node.depth; d>0; d--) { retStr += "  "; }
			retStr += node.label+" (numChildren:"+(node.nodes?node.nodes.length:0)+", flags:"+(node.type&0x0f)+"|"+(node.state>>4&0x0f)+")\n";
			var nodes:Array = node.nodes;
			if (nodes) {
				for (var i:Number=nodes.length-1; i > -1; i--) {
					treeStack.push(nodes[i]); 
				}
			}
		}
		return retStr;
	}
}