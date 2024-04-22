/* Functions
-----------------------------------------------------------*/

var popupVisible = false;


function promoteLabel(command, label) {
    ($.ajax({
        url: "/Promote/PromoteLabel/" + command + "/" + label,
        async: true
    }));

    window.alert("A request to promote this build to label '" + label + "' has been submitted and will be processed momentarily.");
}

function stopCommand(id) {
    ($.ajax({
        url: "/Build/Stop/" + id,
        async: true
    }));

    window.alert("A request to stop this command has been submitted.");
}

function triggerCommand (id) {
    ($.ajax({
        url: "/Build/Trigger/" + id,
        async: true
    }));

    window.alert("A request to trigger this command has been submitted.");
}

function centerPopup(width, height) {
    var h = $(window).height();
    var w = $(window).width();

    $("#popupWindow").css({
        "height": height * h,
        "width": width * w
    });

    $("#popupWindow").css('top', h / 2 - $("#popupWindow").height() / 2);
    $("#popupWindow").css('left', w / 2 - $("#popupWindow").width() / 2);

    $("#popupContent").css('height', $("#popupWindow").height() - 1.5 * $("#popupClose").height());

    // hack for IE6
    $("#popupBg").css({
        "height": document.documentElement.clientHeight
    });
}

function hidePopup () {
    if (popupVisible) {
        $("#popupBg").fadeOut("slow");
        $("#popupWindow").fadeOut("slow");

        popupVisible = false;
    }
}

function showPopup (url, width, height) {
    if (!popupVisible) {
        $("#popupBg").css({
            "opacity" : "0.7"
        });

        $("#popupBg").fadeIn("slow");
        $("#popupWindow").fadeIn("slow");
        $("#popupContent").attr("src", url);

        centerPopup(width, height);

        popupVisible = true;
    }
}


/* Popups
-----------------------------------------------------------*/

var heightP = 0.7;
var widthP = 0.7;

function popupLog(branch, path) {
    showPopup("file://epicgames.net/Root/UE3/Builder/BuilderFailedLogs/" + branch + "/" + path, widthP, heightP);
}


function popupPage (url) {
    showPopup(url, widthP, heightP);
}


$(window).resize(function () {
    centerPopup(widthP, heightP);
});


/* Initialization
-----------------------------------------------------------*/

$(document).ready(function () {
    $("#popupBg").click(function () {
        hidePopup();
    });

    $("#popupClose").click(function () {
        hidePopup();
    });

    $(document).keypress(function (e) {
        if (popupVisible && (e.keyCode == 27)) {
            hidePopup();
        }
    });
});