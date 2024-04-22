package scaleform.clik.controls {
    
    import flash.display.DisplayObject;
    import flash.events.Event;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;
    import flash.utils.getDefinitionByName;
    
    import scaleform.clik.constants.DirectionMode;
    import scaleform.clik.constants.InvalidationType;
    import scaleform.clik.constants.InputValue;
    import scaleform.clik.constants.NavigationCode;
    import scaleform.clik.constants.WrappingMode;
    import scaleform.clik.controls.ScrollBar;
    import scaleform.clik.data.ListData;
    import scaleform.clik.events.InputEvent;
    import scaleform.clik.interfaces.IScrollBar;
    import scaleform.clik.interfaces.IListItemRenderer;
    import scaleform.clik.ui.InputDetails;
    import scaleform.clik.utils.Padding;
    
    [Event(name="change", type="flash.events.Event")]
    [Event(name="itemClick", type="scaleform.clik.events.ListEvent")]
    [Event(name="itemPress", type="scaleform.clik.events.ListEvent")]
    [Event(name="itemRollOver", type="scaleform.clik.events.ListEvent")]
    [Event(name="itemRollOut", type="scaleform.clik.events.ListEvent")]
    [Event(name="itemDoubleClick", type="scaleform.clik.events.ListEvent")]
    
    public class TileList extends CoreList {
        
    // @TODO: ScrollBar's max and min seemed to be mapped to rowHeight rather than itemsPerSet. 
        
    // Constants:
        
    // Public Properties:
        /** 
         * Determines how focus "wraps" when the end or beginning of the component is reached.
            <ul>
                <li>WrappingMode.NORMAL: The focus will leave the component when it reaches the end of the data</li>
                <li>WrappingMode.WRAP: The selection will wrap to the beginning or end.</li>
                <li>WrappingMode.STICK: The selection will stop when it reaches the end of the data.</li>
            </ul>
         */
        [Inspectable(defaultValue="normal", enumeration="normal,wrap,stick")]
        public var wrapping:String = WrappingMode.NORMAL;
        
        /** ScrollIndicator thumb offset for top and bottom. Passed through to ScrollIndicator. This property has no effect if the list does not automatically create a scrollbar instance. */
        public var thumbOffset:Object;
        /** Page size factor for the scrollbar thumb. A value greater than 1.0 will increase the thumb size by the given factor. This positive value has no effect if a scrollbar is not attached to the list. */
        public var thumbSizeFactor:Number = 1; // @TODO: Currently unimplemented.
        
        /** The number of columns to use when setting external item renderers */
        [Inspectable(defaultValue="0")]
        public var externalColumnCount:Number = 0;
        
    // Protected Properties:
        // The height of the rows, if it was defined by the user.
        protected var _rowHeight:Number = NaN;
        // The height of the rows, calculated automatically if a user defined rowHeight was not provided.
        protected var _autoRowHeight:Number = NaN;
        // The total number of rows available (calculated based on the height).
        protected var _totalRows:Number = 0;
        // The width of the columns, if it was defined by the user.
        protected var _columnWidth:Number = NaN;
        // The width of the columns, calculated automatically if a user defined columnWidth was not provided.
        protected var _autoColumnWidth:Number = NaN; 
        // The total number of rows available (calculated based on the width).
        protected var _totalColumns:uint = 0;
        // The current scroll position for the TileList.
        protected var _scrollPosition:uint = 0;
        // true if this component generated its own ScrollBar; false otherwise.
        protected var _autoScrollBar:Boolean = false;
        // Some reference to the ScrollBar (class name, instance name, direct ref, etc...).
        protected var _scrollBarValue:Object;
        // The margin between the boundary of the list component and the list items created internally.
        protected var _margin:Number = 0;
        // Extra padding at the top, bottom, left, and right for the list items.
        protected var _padding:Padding;
        // The direction of the TileList.
        protected var _direction:String = DirectionMode.HORIZONTAL;
        // The width of the ScrollBar before it was rotated.
        protected var _siWidth:Number = 0;
        
    // UI Elements:
        protected var _scrollBar:IScrollBar;
        
    // Initialization:
        public function TileList() {
            super();
        }
        
        override protected function initialize():void {
            super.initialize();
        }
        
        /**
         * The component to use to scroll the list. The {@code scrollBar} can be set as a library linkage ID,
         * an instance name on the stage relative to the component, or a reference to an existing ScrollBar 
         * elsewhere in the application. The automatic behaviour in this component only supports a vertical 
         * scrollBar, positioned on the top right, the entire height of the component.
         * @see ScrollBar
         * @see ScrollIndicator
         */
        [Inspectable(type="String")]
        public function get scrollBar():Object { return _scrollBar; }
        public function set scrollBar(value:Object):void {
            _scrollBarValue = value;
            invalidate(InvalidationType.SCROLL_BAR);
        }
        
         /**
         * The height of each item in the list.  When set to {@code null} or 0, the default height of the
         * renderer symbol is used.
         */
        [Inspectable(defaultValue="0")]
        public function get rowHeight():Number { return isNaN(_autoRowHeight) ? _rowHeight : _autoRowHeight; }
        public function set rowHeight(value:Number):void {
            if (value == 0) {
                value = NaN;
                if (_inspector){ return; }
            }
            _rowHeight = value;
            _autoRowHeight = NaN;
            invalidateSize();
        }
        
        /**
         * Set the width of each column.  By default, the width is 0, and determined by an itemRenderer instance.
         */
        [Inspectable(defaultValue="0")]
        public function get columnWidth():Number { return isNaN(_autoColumnWidth) ? _columnWidth : _autoColumnWidth;  }
        public function set columnWidth(value:Number):void {
            if (value == 0) {
                value = NaN;
                if (_inspector){ return; }
            }
            _columnWidth = value;
            _autoColumnWidth = NaN;
            invalidateSize();
        }
        
        /**
         * The amount of visible rows.  Setting this property will immediately change the height of the component
         * to accomodate the specified amount of rows. The {@code rowCount} property is not stored or maintained.
         */
        public function get rowCount():uint { return _totalRenderers; }
        public function set rowCount(value:uint):void {
            var h:Number = rowHeight;
            if (isNaN(h)) { 
                calculateRendererTotal(availableWidth, availableHeight); 
                h = rowHeight;
            }
            height = (h * value) + (margin * 2) + padding.vertical + ((_direction == DirectionMode.HORIZONTAL && _autoScrollBar) ? Math.round(_siWidth) : 0);
        }
        
        
        /**
         * Set the width of the component to accommodate the number of columns specified.
         */
        public function get columnCount():uint { return _totalColumns; }
        public function set columnCount(value:uint):void {
            var w:Number = columnWidth;
            if (isNaN(w)) { 
                calculateRendererTotal(availableWidth, availableHeight); 
                w = columnWidth
            }
            
            width = (w * value) + (margin * 2) + padding.horizontal + ((_direction == DirectionMode.VERTICAL && _autoScrollBar) ? Math.round(_siWidth) : 0);
        }
        
        /**
         * Set the scrolling direction of the TileList. The {@code direction} can be set to "horizontal" or "vertical". TileLists can only scroll in one direction at a time.
         */
        [Inspectable(type="List", enumeration="horizontal,vertical", defaultValue="horizontal")]
        public function get direction():String { return _direction; }
        public function set direction(value:String):void {
            if (value == _direction) { return; }
            _direction = value;
            invalidate(); // Invalidate everything. Renderers, data, and Scro
        }
        
        /**
         * The selected index of the {@code dataProvider}.  The {@code itemRenderer} at the {@code selectedIndex}
         * will be set to {@code selected=true}.
         */
        override public function set selectedIndex(value:int):void {
            if (value == _selectedIndex || value == _newSelectedIndex) { return; }
            _newSelectedIndex = value;
            invalidateSelectedIndex();
        }
        
        /**
         * The margin between the boundary of the list component and the list items created internally. Does not affects any automatically generated ScrollBars.
         * This value has no effect if the rendererInstanceName property is set. 
         */
        [Inspectable(defaultValue="0")]
        public function get margin():Number { return _margin; }
        public function set margin(value:Number):void {
            _margin = value;
            invalidateSize();
        }
        
        /** 
         * Extra padding at the top, bottom, left, and right for the list items. Also affects any automatically generated ScrollBars.
         * This value has no effect if the rendererInstanceName property is set. 
         */
        public function get padding():Padding { return _padding; }
        public function set padding(value:Padding):void {
            _padding = value;
            invalidateSize();
        }
        
        /** @exclude */
        [Inspectable(name="padding", defaultValue="top:0,right:0,bottom:0,left:0")]
        public function set inspectablePadding(value:Object):void {
            if (!componentInspectorSetting) { return; }
            padding = new Padding(value.top, value.right, value.bottom, value.left);
        }
        
        /** Retireve the available width of the component. */
        override public function get availableWidth():Number {
            return Math.round(_width) - (margin * 2) - ((_direction == DirectionMode.VERTICAL && _autoScrollBar) ? Math.round(_siWidth) : 0);
        }
        
        /** Retrieve the available height of the component. */
        override public function get availableHeight():Number {
            return Math.round(_height) - (margin * 2) - ((_direction == DirectionMode.HORIZONTAL && _autoScrollBar) ? Math.round(_siWidth) : 0);
        }
        
        /** The scroll position of the list. */
        public function get scrollPosition():Number { return _scrollPosition; }
        public function set scrollPosition(value:Number):void {
            var maxScrollPosition:Number = Math.ceil((_dataProvider.length - _totalRows * _totalColumns) / (_direction == DirectionMode.HORIZONTAL ? _totalRows : _totalColumns)); // @TODO: Seems like this can be cached on invalidate.
            value = Math.max(0, Math.min(maxScrollPosition, Math.round(value)));
            if (_scrollPosition == value) { return; }
            _scrollPosition = value;
            invalidateData();
        }
        
        /** 
         * Retrieve a reference to an IListItemRenderer of the List.
         * @param index The index of the renderer.
         * @param offset An offset from the original scrollPosition (normally, the scrollPosition itself).
         */
        override public function getRendererAt(index:uint, offset:int=0):IListItemRenderer {
            if (_renderers == null) { return null; }
            var rendererIndex:uint = index - offset * (_direction == DirectionMode.HORIZONTAL ? _totalRows : _totalColumns);
            if (rendererIndex >= _renderers.length) { return null; }
            return _renderers[rendererIndex] as IListItemRenderer;
        }
        
        /**
         * Scroll the list to the specified index.  If the index is currently visible, the position will not change. The scroll position will only change the minimum amount it has to to display the item at the specified index.
         * @param index The index to scroll to.
         */
        override public function scrollToIndex(index:uint):void {
            if (_totalRenderers == 0) { return; }
            var factor:Number = (_direction == DirectionMode.HORIZONTAL ? _totalRows : _totalColumns);
            var startIndex:Number = _scrollPosition * factor;
            if (factor == 0) { return; }
            if (index >= startIndex && index < startIndex + (_totalRows * _totalColumns)) {
                return;
            } else if (index < startIndex) {
                scrollPosition = (index / factor >> 0);
            } else {
                scrollPosition = Math.floor(index / factor) - (_direction == DirectionMode.HORIZONTAL ? _totalColumns: _totalRows) + 1;
            }
        }
        
        /** @exclude */
        override public function handleInput(event:InputEvent):void {
            if (event.handled) { return; }
            
            // Pass on to selected renderer first
            var renderer:IListItemRenderer = getRendererAt(_selectedIndex, _scrollPosition);
            if (renderer != null) {
                renderer.handleInput(event); // Since we are just passing on the event, it won't bubble, and should properly stopPropagation.
                if (event.handled) { return; }
            }
            
            // Only allow actions on key down, but still set handled=true when it would otherwise be handled.
            var details:InputDetails = event.details;
            var keyPress:Boolean = (details.value == InputValue.KEY_DOWN || details.value == InputValue.KEY_HOLD);
            var nextIndex:int = NaN;
            var nav:String = details.navEquivalent;
            
            // Directional navigation commands differ depending on layout direction.
            if (_direction == DirectionMode.HORIZONTAL) {
                switch (nav) {
                    case NavigationCode.RIGHT:
                        nextIndex = _selectedIndex + _totalRows;
                        break;
                    case NavigationCode.LEFT:
                        nextIndex = _selectedIndex - _totalRows;
                        break;
                    case NavigationCode.UP:
                        nextIndex = _selectedIndex - 1;
                        break;
                    case NavigationCode.DOWN:
                        nextIndex = _selectedIndex + 1;
                        break;
                }
            }
            else {
                switch (nav) {
                    case NavigationCode.DOWN:
                        nextIndex = _selectedIndex + _totalColumns;
                        break;
                    case NavigationCode.UP:
                        nextIndex = _selectedIndex - _totalColumns;
                        break;
                    case NavigationCode.LEFT:
                        nextIndex = _selectedIndex - 1;
                        break;
                    case NavigationCode.RIGHT:
                        nextIndex = _selectedIndex + 1;
                        break;
                }
            }
            
            if (isNaN(nextIndex)) {
                // These navigation commands don't change depending on direction.
                switch (nav) {
                    case NavigationCode.HOME:
                        nextIndex = 0;
                        break;
                    case NavigationCode.END:
                        nextIndex = _dataProvider.length - 1;
                        break;
                    case NavigationCode.PAGE_DOWN:
                        nextIndex = Math.min(_dataProvider.length - 1, _selectedIndex + _totalColumns * _totalRows);
                        break;
                    case NavigationCode.PAGE_UP:
                        nextIndex = Math.max(0, _selectedIndex - _totalColumns * _totalRows);
                        break;
                }
            }

            if (!isNaN(nextIndex)) {
                if (!keyPress) { 
                    event.handled = true;
                    return; // If the event is a Key.UP, bail now to avoid changing the selectedIndex.
                }
                if (nextIndex >= 0 && nextIndex < dataProvider.length) { // Out-of-range items do NOT get rounded. We just don't do anything.
                    selectedIndex = Math.max(0, Math.min(_dataProvider.length-1, nextIndex));
                    event.handled = true;
                }
                else if (wrapping == WrappingMode.STICK) {
                    // Stick to top or bottom.
                    nextIndex = Math.max(0, Math.min(_dataProvider.length-1, nextIndex));
                    if (selectedIndex != nextIndex) { 
                        selectedIndex = nextIndex; 
                    }
                    event.handled = true;
                } 
                else if (wrapping == WrappingMode.WRAP) {
                    selectedIndex = (nextIndex < 0) ? _dataProvider.length-1 : (selectedIndex < _dataProvider.length-1) ? _dataProvider.length-1 : 0;
                    event.handled = true;
                }
            }
        }
        
        /** @exclude */
        override public function toString():String {
            return "[CLIK TileList "+ name +"]";
        }
        
    // Protected Functions:
        override protected function configUI():void {
            super.configUI();
            if (padding == null) { padding = new Padding(); }
            if (_itemRenderer == null && !_usingExternalRenderers) { itemRendererName = _itemRendererName }
        }
        
        override protected function draw():void {
            if (isInvalid(InvalidationType.SCROLL_BAR)) {
                createScrollBar();
            }
            
            if (isInvalid(InvalidationType.RENDERERS)) {
                _autoRowHeight = NaN;
                _autoColumnWidth = NaN;
                
                if (_usingExternalRenderers) {
                    _totalColumns = (externalColumnCount == 0) ? 1 : externalColumnCount; // Defaults to 1 if its not set.
                    _totalRows = Math.ceil(_renderers.length / _totalColumns);
                }
            }
            
            super.draw();
            
            if (isInvalid(InvalidationType.DATA)) {
                updateScrollBar();
            }
        }
        
        protected function createScrollBar():void {
            if (_scrollBar) {
                _scrollBar.removeEventListener(Event.SCROLL, handleScroll);
                _scrollBar.removeEventListener(Event.CHANGE, handleScroll);
                _scrollBar.focusTarget = null;
                if (container.contains(_scrollBar as DisplayObject)) { container.removeChild(_scrollBar as DisplayObject); }
                _scrollBar = null;
            }

            if (!_scrollBarValue || _scrollBarValue == "") { return; }
            
            _autoScrollBar = false; // Reset
            
            var sb:IScrollBar;
            if (_scrollBarValue is String) {
                if (parent != null) {
                    sb = parent.getChildByName(_scrollBarValue.toString()) as IScrollBar;
                }
                if (sb == null) {
                    var classRef:Class = getDefinitionByName(_scrollBarValue.toString()) as Class;
                    if (classRef) { 
                        sb = new classRef() as IScrollBar; 
                    }
                    if (sb) {
                        _autoScrollBar = true;
                        var sbInst:Object = sb as Object;
                        if (sbInst && thumbOffset) {
                            sbInst.offsetTop = thumbOffset.top;
                            sbInst.offsetBottom = thumbOffset.bottom;
                        }
                        sb.addEventListener(MouseEvent.MOUSE_WHEEL, blockMouseWheel, false, 0, true); // Prevent duplicate scroll events
                        //if (sb.scale9Grid == null) { sb.scale9Grid = new Rectangle(0,0,1,1); } // Prevent scaling
                        container.addChild(sb as DisplayObject);
                    }
                }
            } else if (_scrollBarValue is Class) {
                sb = new (_scrollBarValue as Class)() as IScrollBar;
                sb.addEventListener(MouseEvent.MOUSE_WHEEL, blockMouseWheel, false, 0, true);
                if (sb != null) {
                    _autoScrollBar = true;
                    (sb as Object).offsetTop = thumbOffset.top;
                    (sb as Object).offsetBottom = thumbOffset.bottom;
                    container.addChild(sb as DisplayObject);
                }
            } else {
                sb = _scrollBarValue as IScrollBar;
            }
            _scrollBar = sb;
            _siWidth = _scrollBar.width; // Store the width of the ScrollIndicator in case it is rotated later.
            
            invalidateSize(); // Redraw to reset scrollbar bounds, even if there is no scrollBar.
            
            if (_scrollBar == null) { return; }
            // Now that we have a scrollBar, lets set it up.
            _scrollBar.addEventListener(Event.SCROLL, handleScroll, false, 0, true);
            _scrollBar.addEventListener(Event.CHANGE, handleScroll, false, 0, true);
            _scrollBar.focusTarget = this;
            _scrollBar.tabEnabled = false;
        }
        
        override protected function calculateRendererTotal(width:Number, height:Number):uint {
            var invalidRowHeight:Boolean = isNaN(_rowHeight) && isNaN(_autoRowHeight);
            var invalidColumnWidth:Boolean = isNaN(_columnWidth) && isNaN(_autoColumnWidth);
            
            if (invalidRowHeight || invalidColumnWidth) {
                var renderer:IListItemRenderer = createRenderer(0);
                if (invalidRowHeight) { _autoRowHeight = renderer.height; }
                if (invalidColumnWidth) { _autoColumnWidth = renderer.width; }
                cleanUpRenderer(renderer);
            }
            
            _totalRows = availableHeight / rowHeight >> 0;
            _totalColumns = availableWidth / columnWidth >> 0;
            _totalRenderers = _totalRows * _totalColumns;
            
            return _totalRenderers;
        }
        
        override protected function updateSelectedIndex():void {
            if (_selectedIndex == _newSelectedIndex) { return; }
            if (_totalRenderers == 0) { return; } // Return if there are no renderers
            
            var renderer:IListItemRenderer = getRendererAt(_selectedIndex, scrollPosition);
            if (renderer != null) {
                renderer.selected = false; // Only reset items in range
                renderer.validateNow();
            }
            
            super.selectedIndex = _newSelectedIndex; // Reset the new selected index value if we found a renderer instance
            if (_selectedIndex < 0 || _selectedIndex >= _dataProvider.length) { return; }
            
            renderer = getRendererAt(_selectedIndex, _scrollPosition);
            if (renderer != null) {
                renderer.selected = true; // Item is in range. Just set it.
                renderer.validateNow();
            } else {
                scrollToIndex(_selectedIndex); // Will redraw
                renderer = getRendererAt(_selectedIndex, scrollPosition);
                renderer.selected = true; // Item is in range. Just set it.
                renderer.validateNow();
            }
        }
        
    // Protected Functions:
        override protected function refreshData():void {
            // Keep the items in range (in case the component grows larger than the number of renderers)
            var itemsPerSet:Number = (_direction == DirectionMode.HORIZONTAL ? _totalRows : _totalColumns); // Number of items in each set (column or row)
            var numberOfSets:Number = Math.ceil(_dataProvider.length / itemsPerSet); // Number of sets
            var maxScrollPosition:Number = numberOfSets - (_direction == DirectionMode.HORIZONTAL ? _totalColumns : _totalRows);
            _scrollPosition = Math.max(0, Math.min(maxScrollPosition, _scrollPosition));
            
            var startIndex:Number = _scrollPosition * itemsPerSet;
            var endIndex:Number = startIndex + (_totalColumns * _totalRows) -1;
            
            selectedIndex = Math.min(_dataProvider.length - 1,  _selectedIndex);
            updateSelectedIndex();
            
            _dataProvider.requestItemRange(startIndex, endIndex, populateData);
        }
        
        override protected function drawLayout():void {
            var l:uint = _renderers.length;
            var h:Number = rowHeight;
            var w:Number = columnWidth;
            var rx:Number = margin + padding.left;
            var ry:Number = margin + padding.top;
            var dataWillChange:Boolean = isInvalid(InvalidationType.DATA);
            
            for (var i:uint = 0; i < l; i++) {
                var renderer:IListItemRenderer = getRendererAt(i);
                
                if (direction == DirectionMode.HORIZONTAL) {
                    renderer.y = (i % _totalRows) * h + margin;
                    renderer.x = (i / _totalRows >> 0) * w + margin;
                } else {
                    renderer.x = (i % _totalColumns) * w + margin;
                    renderer.y = (i / _totalColumns >> 0) * h + margin;
                }
                
                renderer.width = w;
                renderer.height = h;
                
                if (!dataWillChange) { renderer.validateNow(); }
            }
            
            drawScrollBar();
        }
        
        override protected function changeFocus():void {
            super.changeFocus();
            var renderer:IListItemRenderer = getRendererAt(_selectedIndex, _scrollPosition);
            if (renderer != null) {
                renderer.displayFocus = (focused > 0);
                renderer.validateNow();
            }
        }
        
        protected function populateData(data:Array):void {
            var dl:uint = data.length;
            var l:uint = _renderers.length;
            for (var i:uint = 0; i < l; i++) {
                
                var renderer:IListItemRenderer = getRendererAt(i);
                var index:uint = _scrollPosition * ((_direction == DirectionMode.HORIZONTAL) ? _totalRows : _totalColumns) + i;
                var listData:ListData = new ListData(index, itemToLabel(data[i]), _selectedIndex == index);
                renderer.enabled = (i >= dl) ? false : true;
                renderer.setListData(listData);
                renderer.setData(data[i]);
                renderer.validateNow();
            }
        }
        
        protected function drawScrollBar():void {
            if (!_autoScrollBar) { return; }
            
            var sb:ScrollIndicator = _scrollBar as ScrollIndicator;
            sb.direction = _direction;
            if (_direction == DirectionMode.VERTICAL) {
                sb.rotation = 0;
                sb.x = _width - sb.width + margin;
                sb.y = margin;
                sb.height = availableHeight;
            } else {
                sb.rotation = -90;
                sb.x = margin;
                sb.y = _height - margin; // @TODO: No need for _scrollBar.height here?
                sb.width = availableWidth; // When the ScrollBar is rotated, we can set its width instead.
            }
            
            _scrollBar.validateNow();
        }
        
        protected function updateScrollBar():void {
            if (_scrollBar == null) { return; }

            var max:Number;
            if (direction == DirectionMode.HORIZONTAL) {
                max = Math.ceil(_dataProvider.length / _totalRows) - _totalColumns;
            } else {
                max = Math.ceil(_dataProvider.length / _totalColumns) - _totalRows;
            }
            
            if (_scrollBar is ScrollIndicator) {
                var scrollIndicator:ScrollIndicator = _scrollBar as ScrollIndicator;
                scrollIndicator.setScrollProperties( (_direction == DirectionMode.HORIZONTAL ? _totalColumns : _totalRows), 0, max);
            }
            
            _scrollBar.position = _scrollPosition;
            _scrollBar.validateNow();
        }
        
        protected function handleScroll(event:Event):void {
            scrollPosition = _scrollBar.position;
        }
    
        override protected function scrollList(delta:int):void {
            scrollPosition -= delta;
        }
        
        protected function blockMouseWheel(event:MouseEvent):void {
            event.stopPropagation();
        }
    }
}
