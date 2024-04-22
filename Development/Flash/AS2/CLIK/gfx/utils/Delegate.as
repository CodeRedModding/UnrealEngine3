/** 
 * The Delegate helps resolve function callbacks when no scope can be passed in. Currently, all component callbacks include a scope, so this class may be deprecated.
 */

/**************************************************************************

Filename    :   Delegate.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

class gfx.utils.Delegate extends Object {
	
// Public Methods
	/**
	 * Creates a function wrapper for the original function so that it runs in the provided context.
	 * @parameter obj Context in which to run the function.
	 * @paramater func Function to run.
	 * @return A wrapper function that when called will make the appropriate scoped callback.
	*/
	public static function create(obj:Object, func:Function):Function {
		var f = function() {
			var target = arguments.callee.target;
			var _func = arguments.callee.func;
			return _func.apply(target, arguments);
		};
		f.target = obj;
		f.func = func;
		return f;
	}

}