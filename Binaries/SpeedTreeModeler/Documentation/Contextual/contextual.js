var ie6 = (navigator.userAgent.indexOf('MSIE 6') != -1 || navigator.userAgent.indexOf('MSIE 5') != -1);
var iMinHeight, bNodePage, bFixedMode, strLastHash, 
	arrGroupLabels, arrTopics, arrAnchors, arrBtns;


function getElementsByClass(searchClass, tagName) {
	if (tagName == null) tagName = '*';
	var arrClass = new Array();
	var tags = document.getElementsByTagName(tagName);
	
	for (i=0,j=0; i<tags.length; i++) {
		var test = tags[i].className;
		if (test.indexOf(searchClass) != -1 && test.indexOf(searchClass + '_') == -1) arrClass[j++] = tags[i];
	}
	return arrClass;
}


function autoSize(fixSize, forceThisHash) {
	if (arrTopics.length > 0) {
		if (fixSize) bFixedMode = false;
		iMinHeight = document.documentElement.clientHeight;	
		if (bFixedMode) {
			var t = arrTopics.length;
			do arrTopics[t - 1].style.height = 'auto';
			while (--t);
			
			var b = arrBtns.length;
			do { 
				var bClass = arrBtns[b-1].className;
				switch(bClass) {
				case 'button-auto' :	
					arrBtns[b-1].style.display = 'none';
					break;
				case 'button-fixed' :	
					arrBtns[b-1].style.display = 'inline';
					break;
				}
			} while (--b);		

			if (document.getElementById('container')) {
				document.getElementById('container').style.overflow = 'auto';
				document.getElementById('container').style.height = 'auto';
			}
			if (document.getElementById('object_label'))
				document.getElementById('object_label').style.height = '21px';
			bFixedMode = false;
			
		} else {
			var t = arrTopics.length;
			do arrTopics[t - 1].style.height = iMinHeight - 27 + 'px';
			while (--t);
			
			var b = arrBtns.length;
			do { 
				var bClass = arrBtns[b-1].className;
				switch(bClass) {
				case 'button-auto' :	
					arrBtns[b-1].style.display = 'inline';
					break;
				case 'button-fixed' :	
					arrBtns[b-1].style.display = 'none';
					break;
				}
			} while (--b);
			
			if (document.getElementById('container')) {
				document.getElementById('container').style.overflow = 'hidden';
				document.getElementById('container').style.height = iMinHeight;
			}	
			if (document.getElementById('object_label'))
				document.getElementById('object_label').style.height = document.documentElement.clientHeight + 'px';
			bFixedMode = true;
		}
		
		switch(forceThisHash) {
		case undefined :
		  	window.location.hash = '#' + strLastHash;
			break;
		default : 
			window.location.hash = '#' + forceThisHash;
			break;
		}		
	}
}


function enableGroups() {
	var g = arrGroupLabels.length;
	do {
		arrGroupLabels[g - 1].onclick = function() { 	
			var elGroup = this.parentNode.nextSibling;
			while (elGroup.nodeType == 3) elGroup = elGroup.nextSibling;
			
			if (window.getComputedStyle) {
				var groupStyle = document.defaultView.getComputedStyle(elGroup, '');
				var groupDisplay = groupStyle.getPropertyValue('display');
			} else if (elGroup.currentStyle)
				var groupDisplay = elGroup.currentStyle.display;

			switch(groupDisplay) {
			case 'none' :	
				elGroup.style.display = 'block';
				break;
			default :	
				elGroup.style.display = 'none';
				break;
			}
		};		
	} while (--g);
}


function forceExpand() {
	var g = arrGroupLabels.length;
	do	arrGroupLabels[g-1].parentNode.nextSibling.style.display = 'block';
	while (--g);
}

var date1, milliseconds1, date2, milliseconds2, difference;
function startTimer() {
	date1 = new Date();
	milliseconds1 = date1.getTime();	
}
function endTimer() {
	date2 = new Date();
	milliseconds2 = date2.getTime();
	difference = milliseconds2 - milliseconds1;
	alert(difference); 	
}


function initPage() {
	bNodePage = (document.title.indexOf('Node') != -1);
	var freshLoad = false;
	if (location.hash == '' || location.hash == '#reserved.this') freshLoad = true;
	
	if (freshLoad && !bNodePage && document.getElementById('object_label')) {
		var cHeight = document.documentElement.clientHeight;
		var labelHeight = document.getElementById('object_label').clientHeight;	
		document.getElementById('object_label').style.height = Math.max(labelHeight,cHeight) + 'px';	
		var arrObjectLabelBtns = document.getElementById('object_label').getElementsByTagName('B');
		if (arrObjectLabelBtns.length > 1) {
			arrObjectLabelBtns[0].title = document.anchors[1].name;
			arrObjectLabelBtns[0].onclick = function() { assignEvents(true); window.location.hash = document.anchors[1].name; };
			arrObjectLabelBtns[1].onclick = function() { assignEvents(false); };
		}

		document.getElementById('container').style.overflow = 'hidden';
		document.getElementById('container').style.height = cHeight;	
		document.getElementById('container').onscroll = function() {
				assignEvents(true);
				this.onscroll = '';
		};	
	} else assignEvents(true);
}


function assignEvents(doSize) {
	iMinHeight = document.documentElement.clientHeight - 26;
	if (bNodePage && document.getElementById('instructions')) document.getElementById('instructions').style.height = (iMinHeight - 18) + 'px';
	else {
		bFixedMode = true;
		arrGroupLabels = document.getElementsByTagName('H1');
		enableGroups();
	}
	arrBtns = document.getElementsByTagName('B');
	arrTopics = getElementsByClass('property', 'DIV');
	strLastHash = 'reserved.this'; 
	if (location.hash.length > 1) strLastHash = location.hash.substring(1);
	arrAnchors = new Array();
	arrAnchors[0] = document.anchors[0];
	var elPrevParent = document.anchors[0].parentNode;
	var a = i = 1;
	do { 
		if (document.anchors[a].parentNode != elPrevParent) {
			arrAnchors[i] =  document.anchors[a];
			elPrevParent = arrAnchors[i].parentNode;
			++i;
		}
		++a;
	} while (a < document.anchors.length);
		
	// button actions //
	var iAnchor = b = 0;
	if (bNodePage) iAnchor += 1;
	do{ var bClass = arrBtns[b].className;
		switch(bClass) {
		case 'button-prev' :
			arrBtns[b].onclick = function() { if (this.title != '') window.location.hash = this.title; };
			if (iAnchor - 1 >= 0) arrBtns[b].title = arrAnchors[iAnchor - 1].name;
			break;
		case 'button-next' :
			arrBtns[b].onclick = function() { if (this.title != '') window.location.hash = this.title; };			
			if (iAnchor + 1 < arrAnchors.length) arrBtns[b].title = arrAnchors[iAnchor + 1].name;
			iAnchor = iAnchor + 1;
			break;
		case 'button-auto' :
			arrBtns[b].onclick = function() { autoSize(false, this.parentNode.name); };
			break;
		case 'button-fixed' :
			arrBtns[b].onclick = function() { autoSize(true, this.parentNode.name); };
			break;
		case 'button-help' :
			arrBtns[b].onclick = function() {				
				var pageParent = this.parentNode.parentNode.nextSibling;
				while (pageParent.nodeType == 3) pageParent = pageParent.nextSibling;
				if (pageParent.getElementsByTagName('A').length > 0)
					window.location = pageParent.getElementsByTagName('A')[0].href;
			};
			break;
		}		
		arrBtns[b].onmousedown = function() {
			if (this.className.indexOf('down') == -1)
				this.className = this.className + '_down'; 
		};
		arrBtns[b].onmouseup = function() {
			if (this.className.indexOf('down') != -1)
				this.className = this.className.substring(0,(this.className.length -5));
		}
		++b;
	} while (b < arrBtns.length);
	

	var elContainer = document.getElementById('container');
	elContainer.style.height = document.documentElement.clientHeight;
	elContainer.unselectable = "on";
	elContainer.style.cursor = "default";
	elContainer.onselectstart = function() { return false; };
	elContainer.oncontextmenu = function() { return false; };
	
	window.onresize = function() { 
		document.body.style.height = document.documentElement.clientHeight; 
		if (bNodePage && document.getElementById('instructions')) 
			document.getElementById('instructions').style.height = parseInt(document.documentElement.clientHeight) - 44 + 'px';
	};
	
	switch (doSize) {
	case true :
		autoSize(true);
		break;
	case false :
		autoSize(false);
		break;
	}
}

window.onload = function() {
	initPage();
}
	