import gfx.core.UIComponent;
import gfx.controls.Button;
import gfx.utils.Constraints;
import gfx.utils.Delegate;

[InspectableList("title", "_formSource", "_formType", "allowResize", "_minWidth", "_maxWidth", "_minHeight", "_maxHeight", "_offsetTop", "_offsetBottom", "_offsetLeft", "_offsetRight")]
class com.scaleform.Window extends UIComponent {
	
// Public Properties: 

			
// Private Properties:
	[Inspectable(name="formType", enumeration="symbol,swf")]
	private var _formType:String = "symbol";
	[Inspectable(name="formSource", defaultValue="")]
	private var _formSource:String = "";
	private var _title:String = "Title";
	private var _allowResize:Boolean = true;
	private var constraints:Constraints;
	private var dragProps:Array;
	private var formCreated:Boolean = false;
	private var loader:MovieClipLoader;
	
	[Inspectable(name="minWidth")]
	private var _minWidth:Number;
	[Inspectable(name="maxWidth")]	
	private var _maxWidth:Number;
	[Inspectable(name="minHeight")]	
	private var _minHeight:Number;	
	[Inspectable(name="maxHeight")]	
	private var _maxHeight:Number;	

	[Inspectable(name="offsetTop", defaultValue=0)]
	private var _offsetTop:Number = 0;
	[Inspectable(name="offsetBottom", defaultValue=0)]
	private var _offsetBottom:Number = 0;
	[Inspectable(name="offsetLeft", defaultValue=0)]
	private var _offsetLeft:Number = 0;	
	[Inspectable(name="offsetRight", defaultValue=0)]
	private var _offsetRight:Number = 0;
	
// UI Elements:	

	public var titleBtn:Button;
	public var closeBtn:Button;
	public var resizeBtn:Button;
	public var background:MovieClip;
	public var hit:MovieClip;
	public var form:UIComponent;	

// Initialization:

	public function Window() {
		super();	
	}

// Public Methods:

	[Inspectable(defaultValue="Title")]
	public function get title():String { return _title; }
	public function set title(value:String):Void {
		_title = value;
		invalidate();
	}	

	[Inspectable(defaultValue="true")]
	public function get allowResize():Boolean { return _allowResize; }
	public function set allowResize(value:Boolean):Void {
		_allowResize = value;
		invalidate();
	}
	
	public function toString():String {
		return "[Scaleform Window " + _name + "]";
	}
	
// Private Methods:

	private function configUI():Void {
		background.hitTestDisable = true;
		
		hit.tabEnabled = hit.focusEnabled = false;
		hit.onPress = function() { this._parent.handleTitleDragStart(); }	
		hit.onRelease = function() { this._parent.handleTitleDragStop(); }
		
		super.configUI();										
						
		constraints = new Constraints(this);
		constraints.addElement(titleBtn, Constraints.LEFT | Constraints.RIGHT);
		constraints.addElement(closeBtn, Constraints.RIGHT);
		constraints.addElement(background, Constraints.ALL);
		constraints.addElement(hit, Constraints.ALL);
									
		titleBtn.addEventListener("press", this, "handleTitleDragStart");
		titleBtn.addEventListener("click", this, "handleTitleDragStop");
		
		resizeBtn.addEventListener("press", this, "handleResizeDragStart");
		resizeBtn.addEventListener("click", this, "handleResizeDragStop");
		resizeBtn.addEventListener("releaseOutside", this, "handleResizeDragStop");	
		
		closeBtn.addEventListener("click", this, "handleClose");
	}
	
	// We recurse the target to see if we are in the display hierarchy.
	private function onMouseDown() {
		var targetObj:Object = Mouse.getTopMostEntity(false);
		while (targetObj != null && targetObj != _root) {
			if (targetObj == this) {
				swapDepths(_parent.getNextHighestDepth());
				return;
			}
			targetObj = targetObj._parent;
		}
	}
	
	private function draw():Void {
		if (!formCreated) {
			// Store resizeBtn offsets
			resizeBtn["_ox"] = __width - resizeBtn._x;
			resizeBtn["_oy"] = __height - resizeBtn._y;
			formCreated = true;	
			this.visible = false;
			if (_formType == "swf") {
				if (loader) { delete loader; }
				loader = new MovieClipLoader();
				this.createEmptyMovieClip("form", this.getNextHighestDepth());
				loader.addListener(this);				
				loader.loadClip(_formSource, form);			
				// Defer form config until it has been completly loaded
			} else {
				this.attachMovie(_formSource, "form", this.getNextHighestDepth());
				onLoadComplete();
			}	
			return;			
		}		
		resizeBtn._visible = _allowResize;		
		resizeBtn._x = __width - resizeBtn["_ox"];
		resizeBtn._y = __height - resizeBtn["_oy"];		
		titleBtn.label = _title;
		constraints.update(__width, __height);
		if (form && form.validateNow) {	
			form.validateNow();
		}
	}
			
	// Used by both swf and symbol loading
	private function onLoadComplete():Void { 
		// Delay config by a frame to allow form dimensions to be propogated
		onEnterFrame = function() {
			configForm();
			this.visible = true;
			onEnterFrame = null;
		}
	}
			
	private function configForm():Void {		
		if (!form) { return; }
		// Set form position
		form._x = _offsetLeft;
		form._y = _offsetTop;
		// Autofit the window to the form (used for layout purposes only)
		setSize(
				form._x+form._width+_offsetRight, 
				form._y+form._height+_offsetBottom
		);
		// Update constraints before adding form to constraints			
		resizeBtn._x = resizeBtn._y = 0;
		constraints.update(__width, __height);
		constraints.addElement(form, Constraints.ALL);
		// Setup default minimum resize dimensions
		_minWidth = (!_minWidth) ? __width : Math.max(__width, _minWidth);
		_minHeight = (!_minHeight) ? __height : Math.max(__height, _minHeight);
		// Setup default maximum resize dimensions
		if (_maxWidth < 0) { _maxWidth = _minWidth; } 
		else { _maxWidth = (!_maxWidth) ? Number.POSITIVE_INFINITY : Math.max(_minWidth, _maxWidth); }
		if (_maxHeight < 0) { _maxHeight = _minHeight; }
		else { _maxHeight = (!_maxHeight) ? Number.POSITIVE_INFINITY : Math.max(_minHeight, _maxHeight); }
		// Set the final size
		setSize(_minWidth, _minHeight);		
	}
			
	function handleTitleDragStart() {
		startDrag(this, false);
	}
	function handleTitleDragStop() {
		stopDrag();
	}	
	
	function handleResizeDragStart() {
		dragProps = [_parent._xmouse-(this._x+this._width), _parent._ymouse-(this._y+this._height)];
		onMouseMove = handleResize;
	}
	function handleResizeDragStop() {
		onMouseMove = null;
		delete onMouseMove;
	}
	function handleResize() {			
		setSize(
				Math.max(_minWidth, Math.min(_maxWidth, _parent._xmouse-this._x-dragProps[0])),
				Math.max(_minHeight, Math.min(_maxHeight, _parent._ymouse-this._y-dragProps[1]))
		);
	}	
	
	function handleClose() {
		unloadMovie(this);
	}
}