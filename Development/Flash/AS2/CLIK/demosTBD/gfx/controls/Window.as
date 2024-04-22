/**
	A Basic Window class
 
 	The Base Window includes support for optional UI buttons including:
	<ul>
 	<li>Close Button: An instance named "closeBtn" that will trigger a "close" event when it is clicked.</li>
	<li>Maximize Button: An instance named "maxBtn" that will trigger a "maximize" event when it is clicked.</li>
	<li>Minimize Button: An instance named "minBtn" that will trigger a "minimize" event when it is clicked.</li>
	<li>Resize Button: An instance namd "resizeBtn" that will trigger a "resize" event when the user clicks & drags.</li>
	<li>Drag Bar: An instance named "dragBar" that allows the window to be moved by click & drags.</li>
	</ul>
	@todo Add vertical & horizontal scrolling - for views that do not fit inside the window perfectly & remove scaling of the view when resizing the window
*/

import gfx.controls.Button;
import gfx.core.UIComponent;
import gfx.utils.Delegate;
import gfx.utils.Constraints;

class gfx.controls.Window extends UIComponent
{
// Private Properties:
	
	private var maxWidth:Number = -1;
	private var maxHeight:Number = -1;
	private var minWidth:Number = -1;
	private var currentWidth:Number = -1;
	private var currentHeight:Number = -1;
	private var currentXPos:Number = -1;
	private var currentYPos:Number = -1;
	private var c:Constraints;
		
// Public Properties:

	/** Window states */
	public var bOpen:Boolean = false;
	public var bMinimized:Boolean = false;
	public var bMaximized:Boolean = false;
	/** The linkage identifier of a form to be displayed inside the window */
	public var view:String;

// UI Elements:

	/** An optional Button UI element used as a "close" button. When clicked, a "close" event is dispatched. */
	public var closeBtn:Button;
	/** An optional Button UI element used as a "minimize" button. When clicked, a "minimize" event is dispatched. */
	public var minBtn:Button;
	/** An optional Button UI element used as a "maximize" button. When clicked, a "maximize" event is dispatched. */
	public var maxBtn:Button;
	/** An optional Button UI element used to drag the Window around the stage. */
	public var dragBar:MovieClip;
	/** An optional Button UI element used to manually resize the Window. */
	public var resizeBtn:MovieClip;
	public var winBg:MovieClip;
	public var winTitleBar:MovieClip;
	/** The optional title of a dialog. Only defined when it is passed to {@code Window.addView()}. */
	public var winTitleFld:TextField;
	/** Empty movieClip which will hold the form */
	public var viewMc:MovieClip;
	
// Initialization:
	
	/**
	 * The constructor is called when a Window or a sub-class of Window is instatiated on stage or by using {@code attachMovie()} in ActionScript. This component can <b>not</b> be instantiated using {@code new} syntax. When creating new components that extend Window, ensure that a {@code super()} call is made first in the constructor.
	 */

	public function Window() {
		super();
	}
	
// Public Methods:

	public function toString():String {
		return "[Scaleform Window " + _name + "]";
	}
	
	/**
	 * Creates a view within the window using a form.  The form is a movieClip containing a group related objects, controls, etc.
	 * @param view The library linkage to attach as a form within the window
	 * @param title The name of the window passed as a string
	*/

	public function setView(view:String, title:String):Void {
		viewMc = attachMovie(view, "winView", 1);
		viewMc._y += dragBar._height;
		var w:Number = viewMc._width;
		var h:Number = viewMc._height + dragBar._height;
		winTitleFld.text = title;
		setSize(w,h);
		c.addElement(viewMc, Constraints.TOP | Constraints.BOTTOM | Constraints.LEFT | Constraints.RIGHT);
	}
		
// Private Methods:
		
	private function configUI():Void {
		super.configUI();
		currentWidth = this.__width;
		currentHeight = this.__height;
		currentXPos = this._x;
		currentYPos = this._y;
		winTitleFld.text = "";
		
		// minWidth ensures the window is never resized to less than the title bar elements total width
		minWidth = winTitleFld._width + closeBtn._width + minBtn._width + maxBtn._width;
		
		// window element constraints
		c = new Constraints(this);
		c.addElement(closeBtn, Constraints.TOP | Constraints.RIGHT); 
		c.addElement(minBtn, Constraints.TOP | Constraints.RIGHT);
		c.addElement(maxBtn, Constraints.TOP | Constraints.RIGHT);
		c.addElement(resizeBtn, Constraints.BOTTOM | Constraints.RIGHT);
		c.addElement(dragBar, Constraints.TOP | Constraints.LEFT | Constraints.RIGHT);
		c.addElement(winBg, Constraints.TOP | Constraints.BOTTOM | Constraints.LEFT | Constraints.RIGHT);
		c.addElement(winTitleBar, Constraints.TOP | Constraints.LEFT | Constraints.RIGHT);
		c.addElement(winTitleFld, Constraints.TOP | Constraints.LEFT);
		
		// add event listeners to window controls
		if (closeBtn != null) { 
		closeBtn.addEventListener("click", this, "handleClose"); }
		if (minBtn != null) { minBtn.addEventListener("click", this, "handleMinimize"); }
		if (maxBtn != null) { maxBtn.addEventListener("click", this, "handleMaximize"); }
		if (dragBar != null) { 
			dragBar.tabEnabled = false;
			dragBar.onPress = Delegate.create(this, startDrag);
			dragBar.onRelease = Delegate.create(this, stopDrag);
		}
		if (resizeBtn != null) {
			resizeBtn.tabEnabled = false;
			resizeBtn.onPress = Delegate.create(this, startResize);
		}
		dispatchEvent({type: "init"});
	}
	
	private function draw():Void {
		c.update(__width, __height);
	}
	
	// window controls
	 
	private function minWindow():Void {
		currentXPos = this._x;
		currentYPos = this._y;
		viewMc._visible = winBg._visible = resizeBtn._visible = false;
		dragBar._visible = true;
		setSize(minWidth,dragBar._height);
		dispatchEvent({type: "resize", width:currentWidth, height:dragBar._height});
		bMinimized = true;
		bMaximized = false;
	}
	
	private function maxWindow():Void {
		currentXPos = this._x;
		currentYPos = this._y;
		viewMc._visible = winBg._visible = true;
		resizeBtn._visible = dragBar._visible = false;
		var w:Number = (maxWidth == -1) ? Stage.width : maxWidth;
		var h:Number = (maxHeight == -1) ? Stage.height : maxHeight;
		setSize(w,h);
		this._x = this._y = 0;
		dispatchEvent({type: "resize", width:w, height:h});
		bMaximized = true;
		bMinimized = false;
	}
	
	private function restoreWindow():Void {
		setSize(currentWidth,currentHeight);
		if (bMaximized) {
			this._x = currentXPos;
			this._y = currentYPos;
		}
		if (bMinimized) { viewMc._visible = winBg._visible = true; }
		resizeBtn._visible = dragBar._visible = true;
		dispatchEvent({type: "resize", width:currentWidth, height:currentHeight});
		bMaximized = bMinimized = false;
	}
	
	// window resize methods
	
	private function startResize():Void {
		onMouseUp = stopResize;
		onMouseMove = doResize;
	}
	
	private function doResize():Void {
		if (_parent._xmouse - this._x > minWidth) { currentWidth = _parent._xmouse - this._x + resizeBtn._width; }
		if (_parent._ymouse - this._y > minWidth) { currentHeight = _parent._ymouse - this._y + resizeBtn._height; }
		bMaximized = bMinimized = false;
		setSize(currentWidth, currentHeight);
		dispatchEvent({type: "resize", width:currentWidth, height:currentHeight});
		validateNow();
	}
	
	private function stopResize():Void {
		delete onMouseMove;
		delete onMouseUp;
	}
	
	// event handlers
	
	/**
	 * Dispatches a "close" event. Objects creating Windows can also listen for this event to handle it manually, however the Window will automatically close when the close event is dispatched.
	 */
	 
	private function handleClose(event:Object):Void {
		dispatchEvent({type: "close"});
		bOpen = bMaximized = bMinimized = false;
		this.removeMovieClip();
	}
	
	/**
	 * Dispatches a "minimize" event if the window is not already minimzied, otherwise dispatches a "restore" event.
	 *
	 */
	private function handleMinimize(event:Object):Void {
		if (!bMinimized) {
			dispatchEvent({type: "minimize"});
			minWindow();
		}
		else {
			dispatchEvent({type: "restore"});
			restoreWindow();
		}
	}
	
	/**
	 * Dispatches a "maximize" event if the window is not already maximized, otherwise dispatches a "restore" event.
	 *
	 */
	private function handleMaximize(event:Object):Void {
		if (!bMaximized) {
			dispatchEvent({type: "maximize"});
			maxWindow();
		}
		else { 
			dispatchEvent({type: "restore"});
			restoreWindow();
		}
	}
}