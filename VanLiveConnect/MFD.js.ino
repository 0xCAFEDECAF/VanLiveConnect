
char mfd_js[] PROGMEM = R"=====(

// Javascript functions to drive the browser-based Multi-Function Display (MFD)

// -----
// General

function numberWithSpaces(x)
{
    return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, " ");
} // numberWithSpaces

// -----
// On-screen clocks

// Retrieve and show the current date and time
function updateDateTime()
{
    var date = new Date().toLocaleDateString
    (
        'en-gb',
        {
            weekday: 'short',
            day: 'numeric',
            month: 'short'
        }
    );
    date = date.replace(/,\s*0/, ", ");  // IE11 incorrectly shows leading "0"
    $("#date_small").text(date);

    var date = new Date().toLocaleDateString
    (
        'en-gb',
        {
            weekday: 'long',
        }
    );
    $("#date_weekday").text(date + ",");

    date = new Date().toLocaleDateString
    (
        'en-gb',
        {
            day: 'numeric',
            month: 'long',
            year: 'numeric'
        }
    );
    date = date.replace(/^.0/, "");  // IE11 incorrectly shows leading "0"
    $("#date").text(date);

    var time = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    $("#time").text(time);
    $("#time_small").text(time);
} // updateDateTime

// Update now and every next second
updateDateTime();
setInterval(updateDateTime, 1000);

// -----
// System settings

function showViewportSizes()
{
    $("#viewport_width").text($(window).width()); // width of browser viewport
    $("#screen_width").text(window.screen.width);
    $("#window_width").text(window.innerWidth);

    $("#viewport_height").text($(window).height());  // height of browser viewport
    $("#screen_height").text(window.screen.height);
    $("#window_height").text(window.innerHeight);
} // showViewportSizes

showViewportSizes();

// -----
// Functions for parsing and handling JSON data as it comes in on a websocket

// Inspired by https://gist.github.com/ismasan/299789
var FancyWebSocket = function(url)
{
    var conn = new WebSocket(url);

    var callbacks = {};

    this.bind = function(event_name, callback)
    {
        callbacks[event_name] = callbacks[event_name] || [];
        callbacks[event_name].push(callback);
        return this; // chainable
    };  // function

    // Dispatch to the right handlers
    conn.onmessage = function(evt)
    {
        var json = JSON.parse(evt.data)
        dispatch(json.event, json.data)
    };  // function

    conn.onclose = function() { dispatch('close', null); }
    conn.onopen = function() { dispatch('open', null); }

    var dispatch = function(event_name, message)
    {
        var chain = callbacks[event_name];
        if (chain == undefined) return; // No callbacks for this event
        for (var i = 0; i < chain.length; i++)
        {
            chain[i](message)
        } // for
    } // function
}; // FancyWebSocket

// In "demo mode", specific fields will not be updated
var inDemoMode = false;

var fixFieldsOnDemo =
[
    // During demo, don't show the real VIN
    "vin",

    // During demo, don't show where we are
    "satnav_curr_street_shown",
    "satnav_guidance_curr_street",
    "satnav_guidance_next_street",
    "satnav_distance_to_dest_via_road_number",
    "satnav_distance_to_dest_via_road_unit"
];

function writeToDom(jsonObj)
{
    // The following log entries can be used to literally re-play a session; simply copy-paste these lines into the
    // console window of the web-browser. Also it can be really useful to copy and save these lines into a text file
    // for later re-playing at your desk.

    var now = Date.now();
    if (typeof writeToDom.lastInvoked === "undefined")
    {
        // First time: print the sleep function for easy copy-pasting. Intervals will be at most 300 milliseconds.
        // Note: some events need a minimum amount of time between them, e.g. the pressing of the same button on
        // the remote control, or pressing a button on the remote control after changing to another screen.
        console.log("function sleep(ms) { return new Promise(resolve => setTimeout(resolve, Math.min(ms, 300))); } //");

        // Alternative, with actual intervals
        //console.log("function sleep(ms) { return new Promise(resolve => setTimeout(resolve, ms)); } //");
    }
    else
    {
        var elapsed = now - writeToDom.lastInvoked;

        // Don't let them wait longer than 20 seconds:
        if (elapsed > 20000) elapsed = 20000;

        console.log("await sleep(" + elapsed + "); //");
    } // if
    console.log("writeToDom(" + JSON.stringify(jsonObj) + "); //");

    writeToDom.lastInvoked = now;

    for (var item in jsonObj)
    {
        // In demo mode, skip certain updates and just call the handler
        if (inDemoMode && fixFieldsOnDemo.indexOf(item) >= 0)
        {
            handleItemChange(item, jsonObj[item]);
            continue;
        } // if

        // Select by 'id' attribute (must be unique in the DOM)
        var selectorById = '#' + item;

        // Select also by custom attribute 'gid' (need not be unique)
        var selectorByAttr = '[gid="' + item + '"]';

        // jQuery-style loop over merged, unique-element array (this is cool, looks like Perl! :-) )
        $.each($.unique($.merge($(selectorByAttr), $(selectorById))), function (index, selector)
        {
            if ($(selector).length <= 0) return; // go to next iteration in .each()

            if (Array.isArray(jsonObj[item]))
            {
                // Handling of multi-line DOM objects to show lists. Example:
                //
                // {
                //   "event": "display",
                //   "data": {
                //     "alarm_list":
                //     [
                //       "Tyre pressure low",
                //       "Door open",
                //       "Water temperature too high",
                //       "Oil level too low"
                //     ]
                //   }
                // }

                // Remove current lines
                $(selector).empty();

                var len = jsonObj[item].length;
                for (var i = 0; i < len; i++) $(selector).append(jsonObj[item][i] + (i < len - 1 ? "<br />" : ""));
            }
            else if (!!jsonObj[item] && typeof(jsonObj[item]) === "object")
            {
                // Handling of "change attribute" events. Examples:
                //
                // {
                //   "event": "display",
                //   "data": {
                //     "satnav_curr_heading": { "transform": "rotate(247.5)" }
                //   }
                // }
                //
                // {
                //   "event": "display",
                //   "data": {
                //     "notification_on_mfd": {
                //       "style": { "display": "block" }
                //     }
                //   }
                // }

                var attributeObj = jsonObj[item];
                for (var attribute in attributeObj)
                {
                    // Deeper nesting?
                    if (typeof(attributeObj) === "object")
                    {
                        var propertyObj = attributeObj[attribute];
                        for (var property in propertyObj)
                        {
                            var value = propertyObj[property];
                            $(selector).get(0)[attribute][property] = value;
                        } // for
                    }
                    else
                    {
                        var attrValue = attributeObj[attribute];
                        $(selector).attr(attribute, attrValue);
                    } // if
                } // for
            }
            else
            {
                if ($(selector).hasClass("led"))
                {
                    // Handle "led" class objects: no text copy, just turn ON or OFF
                    var on = jsonObj[item].toUpperCase() === "ON" || jsonObj[item].toUpperCase() === "YES";

                    $(selector).toggleClass("ledOn", on);
                    $(selector).toggleClass("ledOff", ! on);
                }
                else if ($(selector).hasClass("icon") || $(selector).get(0) instanceof SVGElement)
                {
                    // Handle SVG elements and "icon" class objects: no text copy, just show or hide.
                    // From https://stackoverflow.com/questions/27950151/hide-show-element-with-a-boolean :
                    $(selector).toggle(jsonObj[item].toUpperCase() === "ON" || jsonObj[item].toUpperCase() === "YES");
                }
                else
                {
                    // Handle simple "text" objects
                    $(selector).html(jsonObj[item]);
                } // if
            } // if
        }); // each

        // Check if a handler must be invoked
        handleItemChange(item, jsonObj[item]);
    } // for
} // writeToDom

var websocketServerHost = window.location.hostname;

// TODO - hard coded "172.217.28", better to use AP_IP from Config.h
if (! websocketServerHost.match(/^detectportal/) && ! websocketServerHost.match(/^172\.217\.28/))
{
    //websocketServerHost = "VanLiveConnect.lan";
    //websocketServerHost = "192.168.43.2";  // For Android Wi-Fi hotspot
    websocketServerHost = "192.168.137.2";  // For Windows Wi-Fi hotspot
} // if

// For stability, connect to the web socket server only after a few seconds.
// Background: VAN bus receiver is started on first connection of client. Web server may be serving files from
// SPIFFS at the same time. SPIFFS file access and VAN bus receiver cannot operate at the same time. Use timing
// to separate the activities (poor man's solution...)
// TODO - improve
var connectToWebsocketTimer = setTimeout(
    function ()
    {
        console.log("// Connecting to websocket 'ws://" + websocketServerHost + ":81/'");

        // WebSocket class instance
        var socket = new FancyWebSocket("ws://" + websocketServerHost + ":81/");

        // Bind to WebSocket to server events
        socket.bind('display', function(data) { writeToDom(data); });
        //socket.bind('open', function () { inDemoMode = false; });
        //socket.bind('close', function () { demoMode(); });
    }, // function
    5000
);

// -----
// Functions for navigating through the screens and their subscreens/elements

// Open a tab
function openTab(evt, tabName)
{
    var i;
    var tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++)
    {
        tabcontent[i].style.display = "none";
    } // for
    var tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++)
    {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    } // for
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.className += " active";
} // openTab

 // Set display style value of an element by ID, together with all its parents in the 'div' hierarchy
function setVisibilityOfElementAndParents(id, value)
{
    var el = document.getElementById(id);
    while (el && el.tagName === "DIV")
    {
        el.style.display = value;
        el = el.parentNode;
    } // while
} // setVisibilityOfElementAndParents

// Currently shown large screen
var currentLargeScreenId = "clock";  // Make sure this is the first screen visible

var lastScreenChangedAt;  // Last time the large screen changed

// Switch to a specific screen on the right hand side of the display
function changeLargeScreenTo(id)
{
    if (id === currentLargeScreenId) return;

    //console.log("// changeLargeScreenTo('" + id + "')");

    // Perform current screen's "on_exit" action, if specified
    var onExit = $("#" + currentLargeScreenId).attr("on_exit");
    if (onExit) eval(onExit);

    setVisibilityOfElementAndParents(currentLargeScreenId, "none");
    setVisibilityOfElementAndParents(id, "block");

    // A screen can change under a popup. In that case, don't register the time
    if ($(".notificationPopup:visible").length === 0) lastScreenChangedAt = Date.now();

    currentLargeScreenId = id;

    // Perform new screen's "on_enter" action, if specified
    var onEnter = $("#" + currentLargeScreenId).attr("on_enter");
    if (onEnter) eval(onEnter);
} // changeLargeScreenTo

// Keep track of "engine running" condition
var engineRunning = "";  // Either "" (unknown), "YES" or "NO"

var menuStack = [];

function selectDefaultScreen(audioSource)
{
    // We're no longer in any menu
    menuStack = [];
    currentMenu = undefined;

    var selectedScreen = "";

    // Explicitly passed a value for 'audioSource'?
    if (typeof audioSource !== "undefined")
    {
        // TODO - do we ever get audioSource "INTERNAL_CD_OR_TAPE"?
        selectedScreen =
            audioSource === "TUNER" ? "tuner" :
            audioSource === "TAPE" ? "tape" :
            audioSource === "CD" ? "cd_player" :
            audioSource === "CD_CHANGER" ? "cd_changer" :
            "";
    } // if

    if (selectedScreen === "")
    {
        if (satnavMode === "IN_GUIDANCE_MODE") selectedScreen = "satnav_guidance";
    } // if

    if (selectedScreen === "")
    {
        if (typeof audioSource === "undefined") audioSource = $("#audio_source").text();

        selectedScreen =
            audioSource === "TUNER" ? "tuner" :
            audioSource === "TAPE" ? "tape" :
            audioSource === "CD" ? "cd_player" :
            audioSource === "CD_CHANGER" ? "cd_changer" :
            "";
    } // if

    // Show current street, if known
    if (selectedScreen === "" && satnavCurrentStreet !== "") selectedScreen = "satnav_current_location";

    // Fall back to instrument screen if engine is running
    if (selectedScreen === "" && engineRunning === "YES") selectedScreen = "instruments";

    // Final fallback screen...
    if (selectedScreen === "") selectedScreen = "clock";

    changeLargeScreenTo(selectedScreen);
} // selectDefaultScreen

// Keep track of currently shown small screen
var currentSmallScreenId = "trip_info";  // Make sure this is the first screen visible

// Switch to a specific screen on the left hand side of the display
function changeSmallScreenTo(id)
{
    if (currentSmallScreenId === id) return;

    setVisibilityOfElementAndParents(currentSmallScreenId, "none");
    setVisibilityOfElementAndParents(id, "block");
    currentSmallScreenId = id;
} // changeSmallScreenTo

// Cycle through the large screens on the right hand side of the display
function nextLargeScreen()
{
    // The IDs of the screens ("divs") that will be cycled through.
    // Important: list only leaf divs here!
    var screenIds =
    [
        "clock",
        "tuner",
        "tape",
        "cd_player",
        "cd_changer",
        "pre_flight",  // Only in demo mode
        "instruments",
        "satnav_current_location",

        "main_menu",  // Only in demo mode
        "screen_configuration_menu",  // Only in demo mode
        "set_screen_brightness",  // Only in demo mode
        "set_date_time",  // Only in demo mode
        "set_language",  // Only in demo mode
        "set_units",  // Only in demo mode

        "satnav_main_menu",  // Only in demo mode
        "satnav_disclaimer",  // Only in demo mode
        "satnav_select_from_memory_menu",  // Only in demo mode
        "satnav_navigation_options_menu",  // Only in demo mode
        "satnav_directory_management_menu",  // Only in demo mode
        "satnav_guidance_tools_menu",  // Only in demo mode
        "satnav_guidance_preference_menu",  // Only in demo mode
        "satnav_vocal_synthesis_level",  // Only in demo mode

        "satnav_enter_city_characters",  // Only in demo mode
        "satnav_enter_street_characters",  // Only in demo mode
        "satnav_choose_from_list",  // Only in demo mode
        "satnav_enter_house_number",  // Only in demo mode

        "satnav_show_personal_address",  // Only in demo mode
        "satnav_show_professional_address",  // Only in demo mode
        "satnav_show_service_address",  // Only in demo mode
        "satnav_show_current_destination",  // Only in demo mode
        "satnav_show_last_destination",  // Only in demo mode

        "satnav_guidance",
        "satnav_curr_turn_icon",  // Only in demo mode

        "system"  // Only in demo mode
    ]; 

    // Will become -1 if not found
    var idIndex = screenIds.indexOf(currentLargeScreenId);

    // Get the ID of the next screen in the sequence
    idIndex = (idIndex + 1) % screenIds.length;

    // In "demo mode" we can cycle through all the screens, otherwise to a limited set of screens
    if (inDemoMode)
    {
        if (idIndex === screenIds.indexOf("satnav_guidance"))
        {
            // Select the "satnav_curr_turn_icon" which will be shown in the "satnav_guidance" screen
            idIndex = (idIndex + 1) % screenIds.length;
        } // if
    }
    else
    {
        var audioSource = $("#audio_source").text();

        // Skip the "tuner" screen if the radio is not the current source
        if (idIndex === screenIds.indexOf("tuner"))
        {
            if (audioSource !== "TUNER") idIndex = (idIndex + 1) % screenIds.length;
        } // if

        // Skip the "tape" screen if the cassette player is not the current source
        if (idIndex === screenIds.indexOf("tape"))
        {
            if (audioSource !== "TAPE") idIndex = (idIndex + 1) % screenIds.length;
        } // if

        // Skip the "cd_player" screen if the CD player is not the current source
        if (idIndex === screenIds.indexOf("cd_player"))
        {
            if (audioSource !== "CD") idIndex = (idIndex + 1) % screenIds.length;
        } // if

        // Skip the "cd_changer" screen if the CD changer is not the current source
        if (idIndex === screenIds.indexOf("cd_changer"))
        {
            if (audioSource !== "CD_CHANGER") idIndex = (idIndex + 1) % screenIds.length;
        } // if

        // Skip the "pre_flight" screen; it is only visible at engine start
        if (idIndex === screenIds.indexOf("pre_flight"))
        {
            idIndex = (idIndex + 1) % screenIds.length;
        } // if

        // Skip the "instruments" screen if the engine is not running
        if (idIndex === screenIds.indexOf("instruments"))
        {
            if (engineRunning !== "YES") idIndex = (idIndex + 1) % screenIds.length;
        } // if

        // Skip the "satnav_current_location" screen if in guidance mode, or if the current street is empty
        if (idIndex === screenIds.indexOf("satnav_current_location"))
        {
            if (satnavMode === "IN_GUIDANCE_MODE")
            {
                idIndex = screenIds.indexOf("satnav_guidance");
            }
            else
            {
                if (satnavCurrentStreet === "") idIndex = 0;  // Go back to the "clock" screen
            } // if
        } // if

        // After the "satnav_current_location" screen, go back to the "clock" screen
        if (idIndex === screenIds.indexOf("satnav_current_location") + 1)
        {
            idIndex = 0;  // Go back to the "clock" screen
        } // if

        // After the "satnav_guidance" screen, go back to the "clock" screen
        if (idIndex === screenIds.indexOf("satnav_guidance") + 1)
        {
            idIndex = 0;  // Go back to the "clock" screen
        } // if
    } // if

    changeLargeScreenTo(screenIds[idIndex]);
} // nextLargeScreen

function changeToTripCounter(id)
{
    // Simulate a "tab click" event
    var event =
    {
        currentTarget: document.getElementById(id + "_button")
    };
    openTab(event, id);
} // changeToTripCounter

// Change to the correct screen and trip counter, given the reported value of "small_screen"
function gotoSmallScreen(smallScreenName)
{
    switch(smallScreenName)
    {
        case "TRIP_INFO_1":
        {
            changeSmallScreenTo("trip_info");
            changeToTripCounter("trip_1");
        } // case
        break;

        case "TRIP_INFO_2":
        {
            changeSmallScreenTo("trip_info");
            changeToTripCounter("trip_2");
        } // case
        break;

        case "GPS_INFO":
        {
            changeSmallScreenTo("gps_info");
        } // case
        break;

        case "FUEL_CONSUMPTION":
        {
            changeSmallScreenTo("instrument_small");
        } // case
        break;

        default:
        break;
    } // switch
} // gotoSmallScreen

gotoSmallScreen(localStorage.smallScreen);

// Cycle through the small screens on the left hand side of the display
function nextSmallScreen()
{
    // The IDs of the screens ("divs") that will be cycled through
    var screenIds = ["trip_info", "gps_info", "instrument_small"]; 

    // Will become -1 if not found
    var idIndex = screenIds.indexOf(currentSmallScreenId);

    if (idIndex == screenIds.indexOf("trip_info") && $("#trip_1").is(":visible"))
    {
        // Change to the second trip counter
        changeToTripCounter("trip_2");
    }
    else
    {
        // Change to the first trip counter?
        if (idIndex == screenIds.indexOf("instrument_small")) changeToTripCounter("trip_1");

        // Get the ID of the next screen in the sequence
        idIndex = (idIndex + 1) % screenIds.length;

        changeSmallScreenTo(screenIds[idIndex]);
    } // if
} // nextSmallScreen

// Go full-screen
//
// Note: when going full-screen, Android Chrome no longer respects the view port, i.e zooms in. This seems
// to be a known issue; see also:
// - https://stackoverflow.com/questions/39236875/fullscreen-api-on-androind-chrome-moble-disregards-meta-viewport-scale-setting?rq=1
//   ("the scale is hard fixed to 1 when you enter fullscreen.")
// - https://stackoverflow.com/questions/47954761/the-values-of-meta-viewport-attribute-are-not-reflected-when-in-full-screen-mode
//   ("When in fullscreen, the meta viewport values are ignored by mobile browsers.")
// - https://github.com/whatwg/fullscreen/issues/111
//
// Note that Firefox for Android (version >= 68.11.0) *does* work fine when going full-screen.
function goFullScreen()
{
    //document.documentElement.webkitRequestFullScreen(); // Might work
    document.body.requestFullscreen(); // Works, but not in IE11

    // Doesn't work
    //var wscript = new ActiveXObject("Wscript.shell");
    //wscript.SendKeys("{F11}");
} // goFullScreen

// Show the specified popup, with an optional timeout
function showPopup(id, msec)
{
    //console.log("// showPopup('" + id + "'," + msec + ")");

    var popup = $("#" + id);
    popup.show();

    // A popup can appear under another. In that case, don't register the time
    var topLevelPopup = $(".notificationPopup:visible").slice(-1)[0];
    if (id === topLevelPopup.id) lastScreenChangedAt = Date.now();

    // Perform "on_enter" action, if specified
    var onEnter = popup.attr("on_enter");
    if (onEnter) eval(onEnter);

    if (! msec) return;

    // Hide popup after "msec" milliseconds
    clearInterval(showPopup.popupTimer);
    showPopup.popupTimer = setTimeout(function () { hidePopup(id); }, msec);
} // showPopup

// Hide the specified popup. If not specified, hide the current
function hidePopup(id)
{
    var popup;
    if (id) popup = $("#" + id); else popup = $(".notificationPopup:visible");
    if (popup.length === 0) return false;
    if (! popup.is(":visible")) return false;

    //console.log("// hidePopup('" + id + "')");

    popup.hide();

    clearInterval(showPopup.popupTimer);

    // Perform "on_exit" action, if specified
    var onExit = popup.attr("on_exit");
    if (onExit) eval(onExit);

    return true;
} // hidePopup

// Show the notification popup (with icon) with a message and an optional timeout. The shown icon is either "info"
// (isWarning == false, default) or "warning" (isWarning == true).
function showNotificationPopup(message, msec, isWarning)
{
    // IE11 does not support default parameters
    if (isWarning === undefined) isWarning = false;

    // Show the required icon: "information" or "warning"
    $("#notification_icon_info").toggle(! isWarning);
    $("#notification_icon_warning").toggle(isWarning);

    // Set the notification text
    $("#last_notification_message_on_mfd").html(message);

    // Show the notification popup
    showPopup("notification_popup", msec);
} // showNotificationPopup

// Show a simple status popup (no icon) with a message and an optional timeout
function showStatusPopup(message, msec)
{
    // Set the popup text
    $("#status_popup_text").html(message);

    //console.log("showStatusPopup('" + $("#status_popup_text").text() + "')");

    // Show the popup
    showPopup("status_popup", msec);
} // showStatusPopup

// -----
// Functions for navigating through button sets and menus

var currentMenu;

function inMenu()
{
    var notInMenu =

        // Not in any menu?
        currentMenu == null

        // Or in one of the "main" screens (not a menu)?
        // TODO - maybe better to select by matching the string "menu" in the value (ID) of 'currentMenu'
        || currentLargeScreenId === "clock"
        || currentLargeScreenId === "instruments" || currentLargeScreenId === "pre_flight"
        || currentLargeScreenId === "tuner" || currentLargeScreenId === "cd_player" || currentLargeScreenId === "cd_changer"
        || currentLargeScreenId === "satnav_guidance" || currentLargeScreenId === "satnav_curr_turn_icon";

    return ! notInMenu;
} // inMenu

// TODO - there are now three algorithms for selecting the first menu item resp. button:
// 1. in function selectFirstMenuItem: just selects the first found (even if disabled)
// 2. in function gotoMenu: selects the first button, thereby skipping disabled buttons
// 3. in function selectButton: selects the specified button, thereby skipping disabled buttons, following
//    "DOWN_BUTTON" / "RIGHT_BUTTON" attributes if and where present
// --> Let's choose a single algorithm and stick to that.

// Select the first menu item
function selectFirstMenuItem(id)
{
    var allButtons = $("#" + id).find(".button");
    $(allButtons[0]).addClass("buttonSelected");
    allButtons.slice(1).removeClass("buttonSelected");
} // selectFirstMenuItem

// Enter a specific menu
function gotoMenu(menu)
{
    // Change screen, but only if not already there
    if (currentMenu !== menu)
    {
        if (currentMenu) menuStack.push(currentMenu);
        currentMenu = menu;
        changeLargeScreenTo(currentMenu);
    } // if

    // Retrieve all the buttons in the currently shown menu
    var buttons = $("#" + currentMenu).find(".button");
    if (buttons.length === 0) return;

    // Move the "buttonSelected" class to the first enabled button in the list

    var selectedButton = $("#" + currentMenu).find(".buttonSelected");
    var currIdx = buttons.index(selectedButton);  // will be -1 if no buttons are selected
    var nextIdx = 0;

    // Skip disabled buttons
    do
    {
        isButtonEnabled = $(buttons[nextIdx]).is(":visible") && ! $(buttons[nextIdx]).hasClass("buttonDisabled");
        if (isButtonEnabled) break;
        nextIdx = (nextIdx + 1) % buttons.length;
    } // while
    while (nextIdx !== 0);

    // Found an enabled button, which is not the current?
    if (isButtonEnabled && nextIdx !== currIdx)
    {
        if (currIdx >= 0) $(buttons[currIdx]).removeClass("buttonSelected");
        $(buttons[nextIdx]).addClass("buttonSelected");
    } // if

    // Perform menu's "on_goto" action, if specified
    var onGoto = $("#" + currentMenu).attr("on_goto");
    if (onGoto) eval(onGoto);
} // gotoMenu

function gotoTopLevelMenu(menu)
{
    hidePopup();

    menuStack = [];

    if (menu === undefined)
    {
        menu = "main_menu";
        currentMenu = currentLargeScreenId;
    }
    else
    {
        // This is the screen we want to go back to when pressing "Esc" on the remote control
        // TODO - always? I.e. not just in the 'else' clause?
        currentMenu = currentLargeScreenId;
    } // if

    gotoMenu(menu);
} // gotoTopLevelMenu

// Make the indicated button selected, or, if disabled, the first button below or to the right (wrap if necessary).
// Also de-select all other buttons within the same div 
function selectButton(id)
{
    var selectedButton = $("#" + id);

    // De-select all buttons within the same div
    var allButtons = selectedButton.parent().find(".button");
    allButtons.removeClass("buttonSelected");

    var buttonOrientation = selectedButton.parent().attr("button_orientation");
    var buttonForNext = "DOWN_BUTTON";
    if (buttonOrientation === "horizontal") buttonForNext = "RIGHT_BUTTON";

    // Skip disabled buttons
    var nButtons = allButtons.length;
    var currentButtonId = id;
    while (selectedButton.hasClass("buttonDisabled"))
    {
        var gotoButtonId = selectedButton.attr(buttonForNext);

        // Nothing specified?
        if (! gotoButtonId)
        {
            var currIdx = allButtons.index(selectedButton);
            var nextIdx = (currIdx + 1) % nButtons;
            id = $(allButtons[nextIdx]).attr("id");
        }
        else
        {
            id = selectedButton.attr("RIGHT_BUTTON");
        } // if

        selectedButton = $("#" + id);

        // No further button in that direction? Or went all the way round?
        if (! id || id === currentButtonId) return;
    } // while

    selectedButton.addClass("buttonSelected");
} // selectButton

// Retrieve the currently selected button and its screen
function findSelectedButton()
{
    var currentButton;
    var screen;

    // If a popup is showing, get the selected button from that
    // TODO - there could be multiple popups showing (one over the other). The following code may have trouble with that.
    var popup = $(".notificationPopup:visible");
    if (popup.length >= 1)
    {
        screen = popup;
        currentButton = popup.find(".buttonSelected");
        if (currentButton.length === 0) return;
        return {
            screen: popup,
            button: currentButton
        };
    } // if

    // No popup is showing: query the current large screen
    screen = $("#" + currentLargeScreenId);
    currentButton = screen.find(".buttonSelected");

    // No selected button found? Then go the extra mile and also query the parent element.
    if (currentButton.length === 0)
    {
        screen = $("#" + currentLargeScreenId).parent();
        currentButton = screen.find(".buttonSelected");
    } // if

    // Only select if exactly one selected button was found
    if (currentButton.length !== 1) return;

    return {
        screen: screen,
        button: currentButton
    };
} // findSelectedButton

// Handle the 'val' (enter) button
function buttonClicked()
{
    var selected = findSelectedButton();
    if (selected === undefined) return;

    var currentButton = selected.button;

    // console.log("buttonClicked: '" + currentButton.attr("id") + "' in '" +
        // (currentButton.parent().attr("id") ||
        // currentButton.parent().parent().attr("id") ||
        // currentButton.parent().parent().parent().attr("id")) + "'");

    var onClick = currentButton.attr("on_click");
    if (onClick) eval (onClick);

    var goto_id = currentButton.attr("goto_id");
    if (! goto_id) return;

    // Change to the menu screen with that ID
    gotoMenu(goto_id);
} // buttonClicked

function exitMenu()
{
    // If specified, perform the current screen's "on_esc" action instead of the default "menu-up" action
    var onEsc = $("#" + currentLargeScreenId).attr("on_esc");
    if (onEsc)
    {
        eval(onEsc);
        return;
    } // if

    currentMenu = menuStack.pop();
    if (currentMenu) changeLargeScreenTo(currentMenu); else selectDefaultScreen();
} // exitMenu

// Returns the passed id or, if undefined, the id of the button that has focus (has class "selectedButton")
function getFocusId(id)
{
    if (id === undefined)
    {
        var selected = findSelectedButton();
        if (selected === undefined) return id;
        id = selected.button.attr("id");
    } // if

    return id;
} // getFocusId

function setTick(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    // In a group, only one tick box can be ticked at a time
    $("#" + id).parent().find(".tickBox").each(function() { $(this).text("") } );

    $("#" + id).html("<b>&#10004;</b>");
} // setTick

function toggleTick(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    // In a group, only one tick box can be ticked at a time
    $("#" + id).parent().find(".tickBox").each(function() { $(this).text("") } );

    $("#" + id).html($("#" + id).text() === "" ? "<b>&#10004;</b>" : "");
} // toggleTick

// Handle an arrow key press in a screen with buttons
function keyPressed(key)
{
    // Retrieve the currently selected button 
    var selected = findSelectedButton();
    if (selected === undefined) return;

    var screen = selected.screen;
    var currentButton = selected.button;

    // Retrieve the specified action, like e.g. on_left_button="doSomething();"
    var action = currentButton.attr("on_" + key.toLowerCase());
    if (action)
    {
        eval(action);
        return;
    } // if

    navigateButtons(key);
} // keyPressed

// Handle an arrow key press in a screen with buttons
function navigateButtons(key)
{
    // Retrieve the currently selected button 
    var selected = findSelectedButton();
    if (selected === undefined) return;

    var screen = selected.screen;
    var currentButton = selected.button;

    // Retrieve the attribute that matches the pressed key ("UP_BUTTON", "DOWN_BUTTON", "LEFT_BUTTON", "RIGHT_BUTTON")
    var gotoButtonId = currentButton.attr(key);

    // Nothing specified for the pressed key?
    if (! gotoButtonId)
    {
        var buttonOrientation = currentButton.parent().attr("button_orientation");
        var buttonForNext = buttonOrientation === "horizontal" ? "RIGHT_BUTTON" : "DOWN_BUTTON";
        var buttonForPrevious = buttonOrientation === "horizontal" ? "LEFT_BUTTON" : "UP_BUTTON";

        if (key !== buttonForNext && key !== buttonForPrevious) return;

        // Depending on the orientation of the buttons, the "DOWN_BUTTON"/"UP_BUTTON" resp.
        // "RIGHT_BUTTON"/"LEFT_BUTTON" simply walks the list, i.e. selects the next resp. previous button
        // in the list.
        var allButtons = screen.find(".button");
        var nButtons = allButtons.length;
        if (nButtons === 0) return;

        var currIdx = allButtons.index(currentButton);
        var nextIdx = currIdx;

        // Select the next button, thereby skipping disabled or invisible buttons
        do
        {
            nextIdx = (nextIdx + (key === buttonForNext ? 1 : nButtons - 1)) % nButtons;
            if (nextIdx === currIdx) break;

            var isButtonEnabled =
                $(allButtons[nextIdx]).is(":visible")
                && ! $(allButtons[nextIdx]).hasClass("buttonDisabled");

            if (isButtonEnabled) break;
        }
        while (true);

        // Only if anything changed
        if (nextIdx !== currIdx)
        {
            // Perform "on_exit" action, if specified
            var onExit = currentButton.attr("on_exit");
            if (onExit) eval(onExit);

            $(allButtons[currIdx]).removeClass("buttonSelected");
            $(allButtons[nextIdx]).addClass("buttonSelected");

            // Perform "on_enter" action, if specified
            var onEnter = $(allButtons[nextIdx]).attr("on_enter");
            if (onEnter) eval(onEnter);
        } // if
    }
    else
    {
        // Keep track of the buttons checked
        var checkedButtons = {};

        var currentButtonId = currentButton.attr("id");
        checkedButtons[currentButtonId] = true;

        var allButtons = screen.find(".button");
        var nButtons = allButtons.length;

        var gotoNext = key === "DOWN_BUTTON" || key === "RIGHT_BUTTON";

        // Skip disabled buttons, following the orientation of the buttons
        while ($("#" + gotoButtonId).hasClass("buttonDisabled"))
        {
            checkedButtons[gotoButtonId] = true;

            var buttonForNext = key;
            var buttonOrientation = $("#" + gotoButtonId).parent().attr("button_orientation");
            if (buttonOrientation === "horizontal")
            {
                buttonForNext =
                    key === "DOWN_BUTTON" || key === "RIGHT_BUTTON"
                        ? "RIGHT_BUTTON"
                        : "LEFT_BUTTON";
            } // if

            var nextButtonId = $("#" + gotoButtonId).attr(buttonForNext);

            if (nextButtonId)
            {
                gotoButtonId = nextButtonId;
            }
            else
            {
                // Nothing specified? Then select the next resp. previous button in the list.
                var currIdx = allButtons.index($("#" + gotoButtonId));
                var nextIdx = (currIdx + (gotoNext ? 1 : nButtons - 1)) % nButtons;
                gotoButtonId = $(allButtons[nextIdx]).attr("id");
            } // if

            // No further button in that direction? Or went all the way around? Then give up.
            if (! gotoButtonId || checkedButtons[gotoButtonId]) return;
        } // while

        // Only if anything changed
        if (gotoButtonId !== currentButtonId)
        {
            // Perform "on_exit" action, if specified
            var onExit = $("#" + currentButtonId).attr("on_exit");
            if (onExit) eval(onExit);

            $("#" + currentButtonId).removeClass("buttonSelected");
            $("#" + gotoButtonId).addClass("buttonSelected");

            // Perform "on_enter" action, if specified
            var onEnter = $("#" + gotoButtonId).attr("on_enter");
            if (onEnter) eval(onEnter);
        } // if
    } // if
} // navigateButtons

// -----
// Functions for navigating through letters in a string

// Associative array, using the element ID as key, and the highlighted index as value
var highlightIndexes = {};

// Highlight a letter in a text element
function highlightLetter(id, index)
{
    id = getFocusId(id);
    if (id === undefined) return;

    var text = $("#" + id).text();

    // Nothing to highlight?
    if (text.length === 0) return;

    if (typeof index !== "undefined")
    {
        if (index > text.length) index = text.length;
        if (index < 0) index = 0;
        highlightIndexes[id] = index;
    } // if

    if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0;

    // Don't go before first letter or past last
    if (highlightIndexes[id] < 0) highlightIndexes[id] = 0;
    if (highlightIndexes[id] >= text.length) highlightIndexes[id] = text.length - 1;

    text = text.substr(0, highlightIndexes[id])
        + "<span class='invertedText'>" + text[highlightIndexes[id]] + "</span>"
        + text.substr(highlightIndexes[id] + 1, text.length);
    $("#" + id).html(text);
} // highlightLetter

function highlightLetterAtSamePosition(id)
{
    highlightLetter(undefined, highlightIndexes[id]);
} // highlightLetterAtSamePosition

// Remove any highlight from a text element
function unhighlightLetter(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    var text = $("#" + id).text();
    $("#" + id).html(text);
} // unhighlightLetter

function highlightFirstLetter(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    highlightLetter(id, 0);
} // highlightFirstLetter

function highlightNextLetter(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0; else highlightIndexes[id]++;
    if (highlightIndexes[id] >= $("#" + id).text().length) highlightIndexes[id] = 0;  // Roll over
    highlightLetter(id);
} // highlightNextLetter

function highlightPreviousLetter(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    var last = $("#" + id).text().length - 1;
    if (highlightIndexes[id] === undefined) highlightIndexes[id] = last; else highlightIndexes[id]--;
    if (highlightIndexes[id] < 0) highlightIndexes[id] = last;  // Roll over
    highlightLetter(id);
} // highlightPreviousLetter

// -----
// Functions for navigating through lines in a list

// Split the lines of an element to an array
function splitIntoLines(id)
{
    // Puppy powerrrrrr :-)
    var html = $("#" + id).html();
    var brExp = /<br\s*\/?>/i;
    return html.split(brExp);
} // splitIntoLines

// Highlight a line in an element with lines
function highlightLine(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    var lines = splitIntoLines(id);

    // Nothing to highlight?
    if (lines.length === 0) return;

    if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0;

    // Don't go before first line or past last
    if (highlightIndexes[id] < 0) highlightIndexes[id] = 0;
    if (highlightIndexes[id] >= lines.length) highlightIndexes[id] = lines.length - 1;

    // Add class 'invertedText' to specific line
    lines[highlightIndexes[id]] = "<span class='invertedText'>" + lines[highlightIndexes[id]] + "</span>";

    $("#" + id).html(lines.join('<br />'));

    // A highlighted line is a little bit higher than a normal line. If necessary, shrink the box a bit such that the
    // bottom line is not shown partially.
    var heightOfHighlightedLine = $("#" + id + " .invertedText").height();
    var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));

    // Sometimes 'heightOfHighlightedLine' is 0 ??
    if (heightOfHighlightedLine < heightOfUnhighlightedLine) return;

    var heightOfBox = $("#" + id).height();
    var nVisibleLines = (heightOfBox - heightOfHighlightedLine) / heightOfUnhighlightedLine + 1;
    var wantNVisibleLines = Math.floor(nVisibleLines);
    var tooManyPixels = (nVisibleLines - wantNVisibleLines) * heightOfUnhighlightedLine;
    if (tooManyPixels > 0) $("#" + id).height($("#" + id).height() - tooManyPixels); // Shrink a bit
} // highlightLine

// Remove any highlight from element with lines
function unhighlightLine(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    var lines = splitIntoLines(id);

    // Nothing to unhighlight?
    if (lines.length === 0) return;
    if (highlightIndexes[id] === undefined) return;
    if (lines[highlightIndexes[id]] === undefined) return;

    // Remove the class from the highlighed line
    lines[highlightIndexes[id]] = lines[highlightIndexes[id]].replace(/<[^>]*>/g, '');

    $("#" + id).html(lines.join('<br />'));
/*
    // Resize box such that a whole number of lines is shown
    var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));
    var heightOfBox = $("#" + id).height();
    var nVisibleLines = heightOfBox / heightOfUnhighlightedLine;
    var wantNVisibleLines = Math.ceil(nVisibleLines);
    var tooLittlePixels = (wantNVisibleLines - nVisibleLines) * heightOfUnhighlightedLine;
    if (tooLittlePixels > 0)
    {
        // Expand a bit
        $("#" + id).height($("#" + id).height() + tooLittlePixels);
    } // if
*/
} // unhighlightLine

function highlightFirstLine(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    unhighlightLine(id);
    highlightIndexes[id] = 0;
    highlightLine(id);

    // Make sure the top line is visible
    $("#" + id).scrollTop(0);
} // highlightFirstLine

function highlightNextLine(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    unhighlightLine(id);

    if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0; else highlightIndexes[id]++;

    // Uncomment to have roll-over behaviour
    //var lines = splitIntoLines(id);
    //if (highlightIndexes[id] >= lines.length) highlightIndexes[id] = 0;  // Roll over

    highlightLine(id);

    // Scroll along if necessary

    var topOfHighlightedLine = Math.ceil($("#" + id + " .invertedText").position().top);
    var heightOfHighlightedLine = $("#" + id + " .invertedText").height();
    var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));
    var heightOfBox = $("#" + id).height();

    if (topOfHighlightedLine + heightOfHighlightedLine > heightOfBox - heightOfUnhighlightedLine)
    {
        // Try to keep at least one next line visible
        $("#" + id).scrollTop($("#" + id).scrollTop() + heightOfUnhighlightedLine);
    } // if
} // highlightNextLine

function highlightPreviousLine(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    unhighlightLine(id);

    var lines = splitIntoLines(id);

    if (highlightIndexes[id] === undefined) highlightIndexes[id] = lines.length - 1; else highlightIndexes[id]--;

    // Uncomment to have roll-over behaviour
    //if (highlightIndexes[id] < 0) highlightIndexes[id] = lines.length - 1;  // Roll over

    highlightLine(id);

    // Scroll along if necessary

    var topOfHighlightedLine = Math.ceil($("#" + id + " .invertedText").position().top);
    var heightOfHighlightedLine = $("#" + id + " .invertedText").height();
    var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));
    var heightOfBox = $("#" + id).height();

    if (topOfHighlightedLine < heightOfUnhighlightedLine)
    {
        // Try to keep at least one previous line visible
        $("#" + id).scrollTop($("#" + id).scrollTop() - heightOfUnhighlightedLine);
    }
    // else if (topOfHighlightedLine + heightOfHighlightedLine > heightOfBox - heightOfUnhighlightedLine)
    // {
        // // Try to keep at least one next line visible
        // $("#" + id).scrollTop($("#" + id).scrollTop() + heightOfUnhighlightedLine);
    // }
    // else
    // {
        // // Prevent spontaneous jumps
        // $("#" + id).scrollTop(scrollPositions[id]);
    // } // if
} // highlightPreviousLine

// -----
// Functions for drawing arcs (circle segments). Used for sat nav guidance on/near roundabouts.
//
// Inspired by (or: shamelessly copied from):
// https://stackoverflow.com/questions/5736398/how-to-calculate-the-svg-path-for-an-arc-of-a-circle
//
// Usage:
//
// - In SVG element, write:
//   <path id="arc1" fill="none" stroke="#446688" stroke-width="20" />
//
// - For a right-hand roundabout, write the following in code Javascript:
//   document.getElementById("arc1").setAttribute("d", describeArc(200, 400, 100, <angle_to_go_round> - 180, 180));

function polarToCartesian(centerX, centerY, radius, angleInDegrees)
{
    var angleInRadians = (angleInDegrees-90) * Math.PI / 180.0;

    return {
        x: centerX + (radius * Math.cos(angleInRadians)),
        y: centerY + (radius * Math.sin(angleInRadians))
    };
} // polarToCartesian

function describeArc(x, y, radius, startAngle, endAngle)
{
    var start = polarToCartesian(x, y, radius, endAngle);
    var end = polarToCartesian(x, y, radius, startAngle);

    var largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";

    var d = 
    [
        "M", start.x, start.y, 
        "A", radius, radius, 0, largeArcFlag, 0, end.x, end.y
    ].join(" ");

    return d;       
} // describeArc

// -----
// Audio settings and tuner presets popups

var updatingAudioVolume = false;
var isAudioMenuVisible = false;
var audioSettingsPopupHideTimer = null;
var audioSettingsPopupShowTimer = null;
var cdChangerCurrentDisc = "";
var tunerSearchMode = "";

function hideAudioSettingsPopup()
{
    clearInterval(audioSettingsPopupShowTimer);
    clearInterval(audioSettingsPopupHideTimer);
    updatingAudioVolume = false;
    isAudioMenuVisible = false;

    if (! $("#audio_settings_popup").is(":visible")) return false;
    $("#audio_settings_popup").hide();
    return true;
} // hideAudioSettingsPopup

function hideAudioSettingsPopupAfter(msec)
{
    // (Re-)arm the timer to hide the popup after the specified number of milliseconds
    clearInterval(audioSettingsPopupHideTimer);
    audioSettingsPopupHideTimer = setTimeout(hideAudioSettingsPopup, msec);
} // hideAudioSettingsPopupAfter

// The IDs of the audio settings highlight boxes ("divs") to be cycled through
var highlightIds =
[
    "bass_select",
    "treble_select",
    "loudness_select",
    "fader_select",
    "balance_select",
    "auto_volume_select"
]; 
var audioSettingHighlightIndex = 0; // Current audio settings highlight box index

function highlightAudioSetting(goToFirst)
{
    // IE11 does not support default parameters
    if (goToFirst === undefined) goToFirst = false;

    // Hide the current box
    $("#" + highlightIds[audioSettingHighlightIndex]).hide();

    // Either reset to the first ID, or get the ID of the next box
    audioSettingHighlightIndex = goToFirst ? 0 : audioSettingHighlightIndex + 1;

    // If going past the last setting, highlight nothing
    if (audioSettingHighlightIndex >= highlightIds.length)
    {
        audioSettingHighlightIndex = 0;

        // Also remove the audio settings popup
        hideAudioSettingsPopup();

        return;
    } // if

    // Show the next highlight box
    $("#" + highlightIds[audioSettingHighlightIndex]).show();
} // highlightAudioSetting

function hideTunerPresetsPopup()
{
    return hidePopup("tuner_presets_popup");
} // hideTunerPresetsPopup

// Show the audio settings popup
function showAudioSettingsPopup(button)
{
    if (button === "AUDIO")
    {
        // If the tuner presets popup is visible, hide it
        hideTunerPresetsPopup();

        // Highlight the first audio setting ("bass") if just popped up, else highlight next audio setting
        highlightAudioSetting(! isAudioMenuVisible);

        // Show the audio settings popup
        $("#audio_settings_popup").show();

        updatingAudioVolume = false;
        isAudioMenuVisible = true;
    } // if

    // (Re-)start the timer to hide the popup after.
    // Note: set to at least 12 seconds, otherwise the popup will appear again just before it is being force-closed.
    hideAudioSettingsPopupAfter(12000);
} // showAudioSettingsPopup

// Show the tuner presets popup
function showTunerPresetsPopup()
{
    $("#tuner_presets_popup").show();

    // Hide the popup after 9 seconds
    clearInterval(handleItemChange.tunerPresetsPopupTimer);
    handleItemChange.tunerPresetsPopupTimer = setTimeout(hideTunerPresetsPopup, 9000);
} // showTunerPresetsPopup

// -----
// Functions for satellite navigation menu and screen handling

// Current sat nav mode, saved as last reported in item "satnav_status_2"
var satnavMode = localStorage.satnavMode || "INITIALIZING";

// Check if the "Navigation/Guidance" button must be enabled in the main menu
if (typeof localStorage.satnavMode !== "undefined")
{
    $("#main_menu_goto_satnav_button").removeClass("buttonDisabled");
} // if

var satnavRouteComputed = false;
var satnavDisclaimerAccepted = false;
var satnavLastEnteredChar = null;
var satnavToMfdResponse;
var satnavStatus1 = "";
var satnavDestinationNotAccessibleByRoadPopupShown = false;  // Show this popup only once at start of guidance
var satnavCurrentStreet = "";
var satnavDirectoryEntry = "";
var satnavPersonalDirectoryEntries = JSON.parse(localStorage.satnavPersonalDirectoryEntries || "[]");
var satnavProfessionalDirectoryEntries = JSON.parse(localStorage.satnavProfessionalDirectoryEntries || "[]");

function satnavShowDisclaimer()
{
    if (satnavDisclaimerAccepted) return;

    currentMenu = "satnav_disclaimer";
    changeLargeScreenTo("satnav_disclaimer");
} // satnavShowDisclaimer

function satnavGotoMainMenu()
{
    if (satnavStatus1.match(/NO_DISC/))
    {
        showStatusPopup("Navigation CD-ROM is<br />missing", 8000);
        return;
    } // if

    if ($("#satnav_disc_recognized").hasClass("ledOff"))
    {
        showStatusPopup("Navigation CD-ROM<br />is unreadable", 10000);
        return;
    } // if

    // Show popup "Initializing navigator" as long as mode is "INITIALIZING"
    if (satnavMode === "INITIALIZING")
    {
        showPopup("satnav_initializing_popup", 20000);
        return;
    } // if

    // Change to the sat nav main menu with an exit to the general main manu
    //menuStack.push("main_menu");
    menuStack = [ "main_menu" ];
    currentMenu = "satnav_main_menu";
    changeLargeScreenTo("satnav_main_menu");

    // Select the top button
    selectFirstMenuItem("satnav_main_menu");

    satnavShowDisclaimer();
} // satnavGotoMainMenu

function satnavGotoListScreen()
{
    // In the menu stack, there is never more than one "list screen" stacked. So check if there is an existing list
    // screen in the current menu stack and, if so, go back to it.
    var idx = menuStack.indexOf("satnav_choose_from_list");

    if (idx >= 0)
    {
        menuStack.length = idx;  // Remove trailing elements
        currentMenu = null;
    } // if

    satnavLastEnteredChar = null;

    gotoMenu("satnav_choose_from_list");

    // Clear the list box
    $("#satnav_list").text("");
    $('[gid="satnav_to_mfd_list_size"]').text("");
    highlightFirstLine("satnav_list");
} // satnavGotoListScreen

// Select the first line of available characters and highlight the first letter in the "satnav_enter_characters" screen
function satnavSelectFirstAvailableCharacter()
{
    // None of the buttons is selected
    $("#satnav_enter_characters").find(".button").removeClass("buttonSelected");

    // Select the first letter in the first row
    $("#satnav_to_mfd_show_characters_line_1").addClass("buttonSelected");
    highlightFirstLetter("satnav_to_mfd_show_characters_line_1");

    $("#satnav_to_mfd_show_characters_line_2").removeClass("buttonSelected");
    unhighlightLetter("satnav_to_mfd_show_characters_line_2");
} // satnavSelectFirstAvailableCharacter

// Puts the "Enter city" screen into the mode where the user simply has to confirm the current destination city
function satnavConfirmCityMode()
{
    // Clear the character-by-character entry string
    $("#satnav_entered_string").text("");
    satnavLastEnteredChar = null;

    // No entered characters, so disable the "Correction" button
    $("#satnav_enter_characters_correction_button").addClass("buttonDisabled");

    $("#satnav_current_destination_city").show();

    // If a current destination city is shown, enable the "Validate" button
    $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", $("#satnav_current_destination_city").text() === "");

    // Set button text
    $('#satnav_enter_characters_change_or_city_center_button').text('Change');

    satnavSelectFirstAvailableCharacter();
} // satnavConfirmCityMode

// Puts the "Enter city" and "Enter street" screen into the mode where the user is entering a new city resp. street
// character by character
function satnavEnterByLetterMode()
{
    $("#satnav_current_destination_city").hide();
    $("#satnav_current_destination_street").hide();

    $("#satnav_enter_characters_validate_button").addClass("buttonDisabled");

    // Set button text
    $('#satnav_enter_characters_change_or_city_center_button').text('Change');

    satnavSelectFirstAvailableCharacter();
} // satnavEnterByLetterMode

var userHadOpportunityToEnterStreet = false;

// Puts the "Enter street" screen into the mode where the user simply has to confirm the current destination street
function satnavConfirmStreetMode()
{
    // Clear the character-by-character entry string
    $("#satnav_entered_string").text("");
    satnavLastEnteredChar = null;

    // No entered characters, so disable the "Correction" button
    $("#satnav_enter_characters_correction_button").addClass("buttonDisabled");

    $("#satnav_current_destination_street").show();

    // If a current destination street is shown, enable the "Validate" button
    $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", $("#satnav_current_destination_street").text() === "");

    // Set button text
    // TODO - set full text, make button auto-expand when selected
    $('#satnav_enter_characters_change_or_city_center_button').text('City Ce');

    satnavSelectFirstAvailableCharacter();

    userHadOpportunityToEnterStreet = false;
} // satnavConfirmStreetMode

// Toggle between character-by-character entry mode, and confirming the current destination city
function satnavToggleCityEntryMode()
{
    if ($("#satnav_current_destination_city").is(":visible")) satnavEnterByLetterMode(); else satnavConfirmCityMode();
} // satnavToggleCityEntryMode

// The right-most button in the "Enter city/street" screens is either "Change" (for "Enter city") or
// "City Center" for ("Enter street"). Depending on the button text, perform the appropriate action.
function satnavEnterCharactersChangeOrCityCenterButtonPress()
{
    if ($("#satnav_enter_characters_change_or_city_center_button").text().replace(/\s*/g, "") === "Change")
    {
        // Entering city: toggle to the entry mode
        satnavToggleCityEntryMode();
    }
    else
    {
        // Entering street and chosen "City center": directly go to the "Programmed destination" screen
        gotoMenu("satnav_show_current_destination");
    } // if
} // satnavEnterCharactersChangeOrCityCenterButtonPress

function satnavEnterCity()
{
    gotoMenu("satnav_enter_city_characters");
    satnavConfirmCityMode();
} // satnavEnterCity

function satnavEnterNewCity()
{
    gotoMenu("satnav_enter_city_characters");
    satnavEnterByLetterMode();
} // satnavEnterNewCity

function satnavGotoEnterCity()
{
    // Hard-code the menu stack; we could get here from deep inside like the "Change" button in the
    // "satnav_show_current_destination" screen. Pressing "Esc" on the remote control must bring us back to the
    // "satnav_main_menu".
    var topLevel = menuStack[0];
    menuStack = [ "main_menu", "satnav_main_menu" ];
    if (topLevel !== "main_menu") menuStack.unshift(topLevel); // Keep the top level if it was not "main_menu"
    currentMenu = undefined;

    satnavEnterCity();
} // satnavGotoEnterCity

// The right-most button in the "Enter city/street" screens is either "Change" (for "Enter city") or
// "City Center" for ("Enter street"). Depending on the button text, perform the appropriate action.
function satnavGotoEnterStreetOrNumber()
{
    if ($("#satnav_enter_characters_change_or_city_center_button").text().replace(/\s*/g, "") === "Change")
    {
        gotoMenu("satnav_enter_street_characters");
        satnavConfirmStreetMode();
    }
    else
    {
        gotoMenu("satnav_current_destination_house_number");
    } // if
} // satnavGotoEnterStreetOrNumber

// Boolean to indicate if the user has pressed 'Esc' within a list of cities or streets; in that case the entered
// characters are removed until there is more than one character to choose from.
var satnavRollingBackEntryByCharacter = false;

// Handler for the "Correction" button in the "satnav_enter_characters" screen.
// Removes the last entered character.
function satnavRemoveEnteredCharacter()
{
    var currentText = $("#satnav_entered_string").text();
    currentText = currentText.slice(0, -1);
    $("#satnav_entered_string").text(currentText);

    satnavLastEnteredChar = null;

    // If the entered string has become empty, disable the "Correction" button ...
    $("#satnav_enter_characters_correction_button").toggleClass("buttonDisabled", currentText.length === 0);

    // ... and jump back to the characters
    if (currentText.length === 0) satnavSelectFirstAvailableCharacter();
} // satnavRemoveEnteredCharacter

function satnavCheckIfCityCenterMustBeAdded()
{
    // If changed to screen "satnav_choose_from_list" while entering the street with no street entered,
    // then add "City center" at the top of the list
    if (
        $("#satnav_choose_from_list").is(":visible")
        && $("#mfd_to_satnav_request").text() === "Enter street"  // TODO - other language?
        //&& $("#satnav_entered_string").text() === ""
        && ! userHadOpportunityToEnterStreet
       )
    {
        $("#satnav_list").html("City center<br />" + $("#satnav_list").html());
    } // if
} // satnavCheckIfCityCenterMustBeAdded

function satnavListItemClicked()
{
    // Clear the character-by-character entry string
    $("#satnav_entered_string").text("");

    // When choosing a city or street from the list, hide the current destination house number, so that the
    // entry format ("_ _ _") will be shown. If however, the current city and street are confirmed, then
    // the current destination house number must be shown, so that it can also be confirmed.
    $("#satnav_current_destination_house_number").text("");

    var comingFromMenu = menuStack[menuStack.length - 1];
    if (comingFromMenu === "satnav_enter_city_characters" || comingFromMenu === "satnav_enter_street_characters")
    {
        // Pop away this screen, so that escaping from the next screen leads to the previous
        currentMenu = menuStack.pop();
    } // if
} // satnavListItemClicked

// A new digit has been entered for the house number
function satnavEnterDigit(enteredNumber)
{
    $("#satnav_current_destination_house_number").hide();

    if (enteredNumber === undefined) enteredNumber = $("#satnav_house_number_show_characters .invertedText").text();

    // Replace the '_' characters one by one, right to left
    var s = $("#satnav_entered_number").text();
    s = s.replace(/_ /, "");  // Strip off leading "_ " (if present)
    s = s + enteredNumber + " ";
    $("#satnav_entered_number").text(s);

    $("#satnav_entered_number").show();

    // Enable the "Change" button ("Validate" is always enabled)
    $("#satnav_enter_house_number_change_button").removeClass("buttonDisabled");

    // If the maximum number of digits is entered, then directly jump down to the "Validate" button
    s = s.replace(/[\s_]/g, "");  // Strip the entry formatting: remove spaces and underscores
    var nDigitsEntered = s.toString().length;
    if (nDigitsEntered === highestHouseNumberInRange.toString().length) keyPressed("DOWN_BUTTON");
} // satnavEnterDigit

function satnavConfirmHouseNumber()
{
    var haveHouseNumber = $("#satnav_current_destination_house_number").text() !== "";

    $("#satnav_current_destination_house_number").toggle(haveHouseNumber);
    $("#satnav_entered_number").toggle(! haveHouseNumber);

    // Enable or disable the "Change" button
    // Note: "Validate" button is always enabled
    $("#satnav_enter_house_number_change_button").toggleClass("buttonDisabled", ! haveHouseNumber);

    // Highlight the first number ("1")
    $("#satnav_house_number_show_characters").addClass("buttonSelected");
    highlightFirstLetter("satnav_house_number_show_characters");

    // None of the buttons is selected
    $("#satnav_enter_house_number_validate_button").removeClass("buttonSelected");
    $("#satnav_enter_house_number_change_button").removeClass("buttonSelected");
} // satnavConfirmHouseNumber

var lowestHouseNumberInRange = 0;
var highestHouseNumberInRange = 0;

// Present the entry format for the entering of a house number
function satnavClearEnteredNumber()
{
    var nDigits = highestHouseNumberInRange.toString().length;
    $("#satnav_entered_number").text("_ ".repeat(nDigits));
} // satnavClearEnteredNumber

function satnavHouseNumberEntryMode()
{
    $("#satnav_current_destination_house_number").hide();
    satnavClearEnteredNumber();
    $("#satnav_entered_number").show();

    // Navigate back up to the list of numbers and highlight the first
    keyPressed("UP_BUTTON");

    //highlightFirstLetter("satnav_house_number_show_characters");  // This is already done in "on_enter"
    $("#satnav_house_number_show_characters").addClass("buttonSelected");

    // No entered number, so disable the "Change" button
    $("#satnav_enter_house_number_change_button").addClass("buttonDisabled");
} // satnavHouseNumberEntryMode

// Checks if the chosen or entered house number is valid. If not valid, shows a popup to indicate this. If valid,
// proceeds to the next screen: "satnav_show_current_destination".
function satnavCheckEnteredHouseNumber()
{
    var ok = false;

    // When the current destination house number is confirmed, it is always ok
    if ($("#satnav_current_destination_house_number").is(":visible")) ok = true;

    if (! ok)
    {
        // Strip the entry formatting: remove spaces and underscores
        var enteredNumber = $("#satnav_entered_number").text().replace(/[\s_]/g, "");

        // Selecting "No number" is also always ok
        if (enteredNumber === "") ok = true;

        // Check if the entered number is between the lowest and highest
        if (! ok &&
            (lowestHouseNumberInRange <= 0 || +enteredNumber >= lowestHouseNumberInRange) &&
            (highestHouseNumberInRange <= 0 || +enteredNumber <= highestHouseNumberInRange)
           )
        {
            ok = true;
        } // if
    } // if

    if (ok)
    {
        gotoMenu("satnav_show_current_destination");
    }
    else
    {
        satnavHouseNumberEntryMode();
        showStatusPopup("This number is<br />not listed for<br />this street", 7000);
    } // if
} // satnavCheckEnteredHouseNumber

// Enable/disable the buttons in the "satnav_show_service_address" screen. Also select the
// appropriate button.
function satnavServiceAddressSetButtons(selectedEntry)
{
    $("#satnav_service_address_entry_number").text(selectedEntry);

    // Enable the "previous" and "next" buttons, except if at first resp. last entry
    var lastEntry = + $("#satnav_to_mfd_list_size").text();

    $("#satnav_service_address_previous_button").toggleClass("buttonDisabled", selectedEntry === 1);
    $("#satnav_service_address_next_button").toggleClass("buttonDisabled", selectedEntry === lastEntry);

    // Select the appropriate button
    if (selectedEntry === 1)
    {
        $("#satnav_service_address_previous_button").removeClass("buttonSelected");
    }
    if (selectedEntry === lastEntry)
    {
        $("#satnav_service_address_next_button").removeClass("buttonSelected");
    } // if

    if (selectedEntry === 1 && selectedEntry === lastEntry)
    {
        $("#satnav_service_address_validate_button").addClass("buttonSelected");
    }
    else if (selectedEntry === 1)
    {
        $("#satnav_service_address_validate_button").removeClass("buttonSelected");
        $("#satnav_service_address_next_button").addClass("buttonSelected");
    }
    else if (selectedEntry === lastEntry)
    {
        $("#satnav_service_address_validate_button").removeClass("buttonSelected");
        $("#satnav_service_address_previous_button").addClass("buttonSelected");
    } // if
} // satnavServiceAddressSetButtons

function satnavArchiveInDirectoryCreatePersonalEntry()
{
    // Add the new entry to the list of known entries
    var newEntryName = $("#satnav_archive_in_directory_entry").text().replace(/-+$/, "");
    satnavPersonalDirectoryEntries.push(newEntryName);

    // Save in local (persistent) store
    localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);

    showPopup("satnav_input_stored_in_personal_dir_popup", 7000);
} // satnavArchiveInDirectoryCreatePersonalEntry

function satnavArchiveInDirectoryCreateProfessionalEntry()
{
    // Add the new entry to the list of known entries
    var newEntryName = $("#satnav_archive_in_directory_entry").text().replace(/-+$/, "");
    satnavProfessionalDirectoryEntries.push(newEntryName);

    // Save in local (persistent) store
    localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);

    showPopup("satnav_input_stored_in_professional_dir_popup", 7000);
} // satnavArchiveInDirectoryCreateProfessionalEntry

function satnavSetDirectoryAddressScreenMode(mode)
{
    $("#satnav_show_personal_address").find(".button").removeClass("buttonSelected");
    $("#satnav_show_professional_address").find(".button").removeClass("buttonSelected");

    // Make the appropriate set of buttons visible
    if (mode === "SELECT")
    {
        $("#satnav_personal_address_validate_buttons").show();
        $("#satnav_personal_address_manage_buttons").hide();

        $("#satnav_professional_address_validate_buttons").show();
        $("#satnav_professional_address_manage_buttons").hide();
    }
    else
    {
        // mode === "MANAGE"

        $("#satnav_personal_address_validate_buttons").hide();
        $("#satnav_personal_address_manage_buttons").show();
        $("#satnav_manage_personal_address_rename_button").addClass("buttonSelected");

        $("#satnav_professional_address_validate_buttons").hide();
        $("#satnav_professional_address_manage_buttons").show();
        $("#satnav_manage_professional_address_rename_button").addClass("buttonSelected");
    } // if
} // satnavSetDirectoryAddressScreenMode

// Select the first letter in the first row
function satnavSelectFirstLineOfDirectoryEntryScreen(screenId)
{
    var line1id = screenId + "_characters_line_1";
    var line2id = screenId + "_characters_line_2";

    $("#" + line1id).addClass("buttonSelected");
    highlightFirstLetter(line1id);

    $("#" + line2id).removeClass("buttonSelected");
    unhighlightLetter(line2id);
} // satnavSelectFirstLineOfDirectoryEntryScreen

function satnavEnterArchiveInDirectoryScreen()
{
    satnavLastEnteredChar = null;

    $("#satnav_archive_in_directory_entry").text("----------------");

    // Select the first letter in the first row
    satnavSelectFirstLineOfDirectoryEntryScreen("satnav_archive_in_directory");

    // On entry into this screen, all buttons are disabled
    $("#satnav_archive_in_directory").find(".button").addClass("buttonDisabled");
    $("#satnav_archive_in_directory").find(".button").removeClass("buttonSelected");
} // satnavEnterArchiveInDirectoryScreen

function satnavEnterRenameDirectoryEntryScreen()
{
    satnavLastEnteredChar = null;

    $("#satnav_rename_entry_in_directory_entry").text("----------------");

    // Select the first letter in the first row
    satnavSelectFirstLineOfDirectoryEntryScreen("satnav_rename_entry_in_directory");

    // On entry into this screen, all buttons are disabled
    $("#satnav_rename_entry_in_directory").find(".button").addClass("buttonDisabled");
    $("#satnav_rename_entry_in_directory").find(".button").removeClass("buttonSelected");
} // satnavEnterRenameDirectoryEntryScreen

// A new character is entered in the "satnav_rename_entry_in_directory" or "satnav_archive_in_directory" screen
function satnavDirectoryEntryEnterCharacter(screenId, availableCharacters)
{
    var screenSelector = "#" + screenId;
    var entrySelector = screenSelector + "_entry";

    // On the original MFD, there is only room for 24 characters. If there are more characters, spill over
    // to the next line.
    var line_1 = availableCharacters.substr(0, 24);
    var line_2 = availableCharacters.substr(24);

    $("#" + screenId + "_characters_line_1").text(line_1);
    $("#" + screenId + "_characters_line_2").text(line_2);

    // Enable the second line only if it contains something
    $("#" + screenId + "_characters_line_2").toggleClass("buttonDisabled", line_2.length === 0);

    if (satnavLastEnteredChar === "Esc")
    {
        // Remove the last character and replace it by "-"
        var s = $(entrySelector).text();
        s = s.replace(/([^-]*)[^-](-*)/, "$1-$2");
        $(entrySelector).text(s);

        // Was the last character removed?
        if ($(entrySelector).text()[0] === "-")
        {
            // Select the first letter in the first row
            satnavSelectFirstLineOfDirectoryEntryScreen(screenId);
            $(screenSelector).find(".button").removeClass("buttonSelected");
        } // if
    }
    else
    {
        if (satnavLastEnteredChar != null)
        {
            // Replace the '-' characters one by one, left to right
            var s = $(entrySelector).text();
            s = s.replace("-", satnavLastEnteredChar);
            $(entrySelector).text(s);
        } // if

        // Select the first letter in the first row
        satnavSelectFirstLineOfDirectoryEntryScreen(screenId);
        $(screenSelector).find(".button").removeClass("buttonSelected");
    } // if

    // Enable or disable buttons, depending on if there is anything in the entry field
    $(screenSelector).find(".button").toggleClass("buttonDisabled", $(entrySelector).text()[0] === "-");

    // Enable or disable "Personal dir", "Professional d" and Validate" button if entered name already exists
    var enteredEntryName = $(entrySelector).text().replace(/-+$/, "");
    var personalEntryExists = satnavPersonalDirectoryEntries.indexOf(enteredEntryName) >= 0;
    var professionalEntryExists = satnavProfessionalDirectoryEntries.indexOf(enteredEntryName) >= 0;
    var entryExists = personalEntryExists || professionalEntryExists;

    $("#satnav_rename_entry_in_directory_validate_button").toggleClass("buttonDisabled", entryExists);
    $("#satnav_archive_in_directory_personal_button").toggleClass("buttonDisabled", entryExists);
    $("#satnav_archive_in_directory_professional_button").toggleClass("buttonDisabled", entryExists);
    $("#satnav_rename_entry_in_directory_entry_exists").toggle(entryExists);
    $("#satnav_archive_in_directory_entry_exists").toggle(entryExists);
} // satnavDirectoryEntryEnterCharacter

// Handles pressing the "Validate" button in the sat nav directory rename entry screen
function satnavDirectoryEntryRenameValidateButton()
{
    satnavLastEnteredChar = null;

    // Replace the old entry name by the new one
    var newEntryName = $("#satnav_rename_entry_in_directory_entry").text().replace(/-+$/, "");
    var index = satnavPersonalDirectoryEntries.indexOf(satnavDirectoryEntry);
    if (index >= 0)
    {
        satnavPersonalDirectoryEntries[index] = newEntryName;

        // Save in local (persistent) store
        localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);
    }
    else
    {
        index = satnavProfessionalDirectoryEntries.indexOf(satnavDirectoryEntry);
        if (index >= 0)
        {
            satnavProfessionalDirectoryEntries[index] = newEntryName;

            // Save in local (persistent) store
            localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);
        } // if
    } // if

    // Go all the way back to the "Directory management" menu
    menuStack.pop();
    menuStack.pop();
    currentMenu = menuStack.pop();
    changeLargeScreenTo(currentMenu);
} // satnavDirectoryEntryRenameValidateButton

function satnavDeleteDirectoryEntry()
{
    // Remove the entry from the list of known entries
    var index = satnavPersonalDirectoryEntries.indexOf(satnavDirectoryEntry);
    if (index >= 0)
    {
        satnavPersonalDirectoryEntries.splice(index, 1);

        // Save in local (persistent) store
        localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);
    }
    else
    {
        index = satnavProfessionalDirectoryEntries.indexOf(satnavDirectoryEntry);
        if (index >= 0)
        {
            satnavProfessionalDirectoryEntries.splice(index, 1);

            // Save in local (persistent) store
            localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);
        } // if
    } // if

    hidePopup();
    exitMenu();
    exitMenu();
} // satnavDeleteDirectoryEntry

// Handles pressing "UP" on the "Correction" button in the sat nav directory entry screens
function satnavDirectoryEntryMoveUpFromCorrectionButton(screenId)
{
    var line2id = screenId + "_characters_line_2";

    if (typeof highlightIndexes[line2id] === "undefined")
    {
        // In this case, "UP" moves the cursor to the first letter of in the first line
        satnavSelectFirstLineOfDirectoryEntryScreen(screenId);
        $("#" + screenId + "_correction_button").removeClass('buttonSelected');
    }
    else
    {
        // In this case, "UP" simply moves back to the second line
        navigateButtons('UP_BUTTON');
    } // if
} // satnavDirectoryEntryMoveUpFromCorrectionButton

function satnavDeleteDirectories()
{
    hidePopup('satnav_delete_directory_data_popup');

    satnavPersonalDirectoryEntries = [];
    satnavProfessionalDirectoryEntries = [];

    // Save in local (persistent) store
    localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);
    localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);
} // satnavDeleteDirectories

function satnavGuidanceSetPreference(value)
{
    if (typeof value === "undefined" || value === "---") return;

    // Set the correct text in the sat nav guidance preference popup
    var satnavGuidancePreferenceText =
        value === "FASTEST_ROUTE" ? "Fastest route" :
        value === "SHORTEST_DISTANCE" ? "Shortest route" :
        value === "AVOID_HIGHWAY" ? "Avoid highway route" :
        value === "COMPROMISE_FAST_SHORT" ? "Fast/short compromise route" :
        "??";
    $("#satnav_guidance_current_preference_text").text(satnavGuidancePreferenceText);

    // Also set the correct tick box in the sat nav guidance preference menu
    var satnavGuidancePreferenceTickBoxId =
        value === "FASTEST_ROUTE" ? "satnav_guidance_preference_fastest" :
        value === "SHORTEST_DISTANCE" ? "satnav_guidance_preference_shortest" :
        value === "AVOID_HIGHWAY" ? "satnav_guidance_preference_avoid_highway" :
        value === "COMPROMISE_FAST_SHORT" ? "satnav_guidance_preference_compromise" :
        undefined;
    setTick(satnavGuidancePreferenceTickBoxId);
} // satnavGuidanceSetPreference

satnavGuidanceSetPreference(localStorage.satnavGuidancePreference);

// Find the ticked button and select it
function satnavGuidancePreferenceSelectTickedButton()
{
    var foundTick = false;

    $("#satnav_guidance_preference_menu").find(".tickBox").each(
        function()
        {
            var isTicked = $(this).text() !== "";
            $(this).toggleClass("buttonSelected", isTicked);
            if (isTicked) foundTick = true;
        }
    );

    // If nothing is ticked, select the top one
    if (! foundTick)
    {
        $("#satnav_guidance_preference_fastest").html("<b>&#10004;</b>");
        $("#satnav_guidance_preference_fastest").addClass("buttonSelected");
    } // if

    $("#satnav_guidance_preference_menu_validate_button").removeClass("buttonSelected");
} // satnavGuidancePreferenceSelectTickedButton

function satnavGuidancePreferenceValidate()
{
    if (currentMenu !== "satnav_guidance")
    {
        satnavSwitchToGuidanceScreen();
        //showPopup("satnav_calculating_route_popup", 30000);
    }
    else
    {
        // Return to the guidance screen (bit clumsy)
        exitMenu();
    } // if
} // satnavGuidancePreferenceValidate

function satnavSwitchToGuidanceScreen()
{
    //console.log("satnavSwitchToGuidanceScreen()");

    menuStack = [];
    currentMenu = "satnav_guidance";
    changeLargeScreenTo("satnav_guidance");

    hidePopup("satnav_calculating_route_popup");
} // satnavSwitchToGuidanceScreen

// Format a string like "45 km" or "7000 m" or "60 m". Return an array [distance, unit]
function satnavFormatDistance(distanceStr)
{
    // Want compatibility with IE11 so cannot assign result array directly to variables
    var parts = distanceStr.split(" ");
    var distance = parts[0];
    var unit = parts [1];

    if (unit === "m" && +distance >= 1000)
    {
        // Reported in metres, and 1000 metres or more: show in kilometres, with decimal notation.
        // Round upwards, e.g. 6901 metres must be shown as "7.0 km", but 6900 metres must be shown as "6.9 km".
        // Note that 'toFixed' also performs rounding. So we need to add only 49.
        return [ ((+distance + 49) / 1000).toFixed(1), "km" ];
    }
    else
    {
        return [ distance, unit ];
    } // if
} // satnavFormatDistance

var suppressScreenChangeToAudio = null;

function satnavValidateVocalSynthesisLevel()
{
    if (satnavMode === "IN_GUIDANCE_MODE")
    {
        // When head unit is playing audio (tuner, CD, CD changer), the "audio_source" will change to it after
        // setting the vocal synthesis level. In that situation, ignore that change during a short period.
        clearInterval(suppressScreenChangeToAudio);
        suppressScreenChangeToAudio = setTimeout(
            function () { suppressScreenChangeToAudio = null; },
            400
        );
    } // if

    var comingFromMenu = menuStack[menuStack.length - 1];
    if (comingFromMenu === "satnav_guidance_tools_menu")
    {
        // In nav guidance tools (context) menu

        // Go back to guidance screen
        //selectDefaultScreen();
        //if (satnavMode === "IN_GUIDANCE_MODE") satnavSwitchToGuidanceScreen();
        exitMenu();
        exitMenu();
    }
    else
    {
        // In main menu

        // When the "Validate" button is pressed, all menus are exited up to top level. IMHO that is a bug in the MFD.
        exitMenu();
        exitMenu();
        exitMenu();
        exitMenu();
    } // if
} // satnavValidateVocalSynthesisLevel

function satnavEscapeVocalSynthesisLevel()
{
    if (satnavMode === "IN_GUIDANCE_MODE")
    {
        // When head unit is playing audio (tuner, CD, CD changer), the "audio_source" will change to it after
        // setting the vocal synthesis level. In that situation, ignore that change during a short period.
        clearInterval(suppressScreenChangeToAudio);
        suppressScreenChangeToAudio = setTimeout(
            function () { suppressScreenChangeToAudio = null; },
            400
        );
    } // if

    var comingFromMenu = menuStack[menuStack.length - 1];
    if (comingFromMenu === "satnav_guidance_tools_menu")
    {
        // Go back to guidance screen
        //selectDefaultScreen();
        //if (satnavMode === "IN_GUIDANCE_MODE") satnavSwitchToGuidanceScreen();
        satnavSwitchToGuidanceScreen();
    }
    else
    {
        // In main menu
        currentMenu = menuStack.pop();
        changeLargeScreenTo(currentMenu);
    } // if
} // satnavEscapeVocalSynthesisLevel

// -----
// Handling of 'system' screen

function gearIconAreaClicked()
{
    if ($("#clock").is(":visible")) changeLargeScreenTo("system");
    else changeLargeScreenTo("clock");
} // gearIconAreaClicked

// -----
// Handling of doors and boot status

// Associative array, using the door ID as key, mapping to true (open) or false (closed)
var isDoorOpen = {};

var isBootOpen = false;

// -----
// Handling of changed items

function handleItemChange(item, value)
{
    switch(item)
    {
        case "volume_update":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            if (value !== "YES") break;

            // Just changed audio source? Then suppress the audio settings popup (which would otherwise pop up every
            // time the audio source changes).
            if (handleItemChange.hideHeadUnitPopupsTimer != null) break;

            // Bail out if no audio is playing
            var audioSource = $("#audio_source").text();
            if (audioSource !== "TUNER"
                && audioSource !== "TAPE"
                && audioSource !== "CD"
                && audioSource !== "CD_CHANGER")
                //&& audioSource !== "NAVIGATION")
            {
                break;
            } // if

            // Audio popup already visible due to display of audio settings?
            if (isAudioMenuVisible) break;

            // If the tuner presets popup is visible, hide it
            hideTunerPresetsPopup();

            // Show the audio settings popup to display the current volume
            $("#" + highlightIds[audioSettingHighlightIndex]).hide();

            // Don't pop up when user is browsing the menus
            if (inMenu()) break;

            // Show the audio settings popup 
            if ($("#contact_key_position").text() != "" && $("#contact_key_position").text() !== "OFF")
            {
                $("#audio_settings_popup").show();
            }
            else
            {
                // With no contact key in place, we get a spurious "volume_update" message when turning off the
                // head unit. To prevent the audio settings popup to flash up and disappear, show it only after
                // 100 msec in this specific situation.
                clearInterval(audioSettingsPopupShowTimer);
                audioSettingsPopupShowTimer = setTimeout(function () { $("#audio_settings_popup").show(); }, 100);
            } // if
            updatingAudioVolume = true;

            // Hide popup after 4 seconds
            hideAudioSettingsPopupAfter(4000);
        } // case
        break;

        case "audio_menu":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            if (value !== "CLOSED") break;

            // Bug: if source is cd-changer, audio_menu is always "OPEN"... So ignore for now.

            // Ignore when audio volume is being set
            if (updatingAudioVolume) break;

            // Hide the audio popup now
            hideAudioSettingsPopup();
        } // case
        break;

        case "head_unit_button_pressed":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Check for audio button press or release
            if (value.substring(0, 5) === "AUDIO")
            {
                // Make sure the audio settings popup is shown. If so, take care to restart the time-out
                showAudioSettingsPopup(value);

                break;
            } // if

            // Any other button than "AUDIO" or a variant like "CD_CHANGER (released)"
            if (updatingAudioVolume) hideAudioSettingsPopup();

            var audioSource = $("#audio_source").text();

            // If "audio_source" changed to "CD", directly show the "searching CD" icon
            if (audioSource !== "CD" && value === "CD")
            {
                if (! $("#cd_present").hasClass("ledOn"))
                {
                    // Show "No CD" popup for a few seconds
                    showStatusPopup("No CD", 3000);

                    break;
                } // if

                // Show the "searching" icon
                $("#cd_status_searching").show();
                $("#cd_status_pause").hide();
                $("#cd_status_play").hide();
                $("#cd_status_fast_forward").hide();
                $("#cd_status_rewind").hide();
            }

            // If "audio_source" changed to "CD_CHANGER", directly show the "searching CD" icon
            else if (audioSource !== "CD_CHANGER" && value === "CD_CHANGER")
            {
                // Cartridge is not present?
                if (! $("#cd_changer_cartridge_present").hasClass("ledOn"))
                {
                    // Show "No CD" popup for a few seconds
                    showStatusPopup("No CD", 3000);
                    break;
                } // if

                // Cartridge is present

                // Not busy loading, and cartridge contains no discs?
                if (
                     $("#cd_changer_status_loading").css("display") !== "block"
                     && (cdChangerCurrentDisc == null || ! cdChangerCurrentDisc.match(/^[1-6]$/))
                   )
                {
                    // Show "No CD" popup for a few seconds
                    showStatusPopup("No CD", 3000);

                    break;
                } // if

                // Show the "loading" icon
                $("#cd_changer_status_pause").hide();
                $("#cd_changer_status_play").hide();
                $("#cd_changer_status_loading").show();
                $("#cd_changer_status_fast_forward").hide();
                $("#cd_changer_status_rewind").hide();
                //$("#cd_changer_status_next_track").hide();
                //$("#cd_changer_status_previous_track").hide();
            } // if

            /*
            // If "audio_source" is "CD_CHANGER", we set the icon to "next_track" or "previous_track" if the
            // "SEEK_FORWARD" resp. "SEEK_BACKWARD" head unit button is pressed
            //
            // Note: this works only for the CD changer. For the head unit internal CD player, we don't see the
            // "head_unit_button_pressed" events "SEEK_FORWARD" resp. "SEEK_BACKWARD", unfortunately...
            else if (audioSource === "CD_CHANGER")
            {
                if (value == "SEEK_FORWARD")
                {
                    $("#cd_changer_status_pause").hide();
                    $("#cd_changer_status_play").hide();
                    $("#cd_changer_status_fast_forward").hide();
                    $("#cd_changer_status_rewind").hide();
                    $("#cd_changer_status_next_track").show();
                    $("#cd_changer_status_previous_track").hide();
                }
                else if (value == "SEEK_BACKWARD")
                {
                    $("#cd_changer_status_pause").hide();
                    $("#cd_changer_status_play").hide();
                    $("#cd_changer_status_fast_forward").hide();
                    $("#cd_changer_status_rewind").hide();
                    $("#cd_changer_status_next_track").hide();
                    $("#cd_changer_status_previous_track").show();
                } // if
            } // if
            */

            if (! value.match(/^[1-6]/)) hideTunerPresetsPopup();

            // Check for audio source button press
            // Note: ignore values like "CD (released)", "TUNER (held)" and whatever other variants
            if (value === "TUNER" || value === "TAPE" || value === "CD" || value === "CD_CHANGER")
            {
                // Don't change screen when user is browsing the menus
                //if (currentMenu && currentMenu !== "satnav_guidance") break;
                if (inMenu()) break;

                selectDefaultScreen(value);
                break;
            } // if

            // Check for button press of "1" ... "6" (set tuner preset, or select disc on CD changer).
            // Ignore variants like "1 (released)" or "5 (held)".
            if (! value.match(/^[1-6]$/)) break;

            if (audioSource === "CD_CHANGER")
            {
                var selector = "#cd_changer_disc_" + value + "_present";
                if ($(selector).hasClass("ledOn")) break;

                // Show "NO CD X" text for a few seconds
                $("#cd_changer_current_disc").hide();
                $("#cd_changer_disc_not_present").show();
                $("#cd_changer_selected_disc").text(value);
                $("#cd_changer_selected_disc").show();

                // After 5 seconds, go back to showing the CD currently playing
                clearInterval(handleItemChange.noCdPresentTimer);
                handleItemChange.noCdPresentTimer = setTimeout
                (
                    function ()
                    {
                        $("#cd_changer_disc_not_present").hide();
                        $("#cd_changer_selected_disc").hide();
                        $("#cd_changer_current_disc").show();

                        handleItemChange.noCdPresentTimer = null;
                    },
                    5000
                );

                break;
            } // if

            // What follows is only for when "audio_source" is "TUNER"
            if (audioSource !== "TUNER") break;

            // Ignore if audio menu is visible (the head unit ignores the button press in that case)
            if (isAudioMenuVisible) break;

            // Show the tuner presets popup
            showTunerPresetsPopup();
        } // case
        break;

        case "tape_status":
        {
            // Has anything changed?
            if (value === handleItemChange.tapeStatus) break;
            handleItemChange.tapeStatus = value;

            hideAudioSettingsPopup();
        } // case
        break;

        case "tuner_band":
        {
            // Has anything changed?
            if (value === handleItemChange.tunerBand) break;
            handleItemChange.tunerBand = value;

            // Hide the audio and tuner presets popup when changing band
            hideTunerPresetsPopup();
            hideAudioSettingsPopup();
        } // case
        break;

        case "fm_band":
        {
            // FM tuner data is visible only if FM band is selected
            $("#fm_tuner_data").toggle(value === "ON");
            $("#fm_tuner_data_small").toggle(value === "ON");
        } // case
        break;

        // Switch the "tuner_presets_popup" to display the correct tuner band
        case "fm_band_1":
        {
            $("#presets_fm_1").toggle(value === "ON");
        } // case
        break;

        case "fm_band_2":
        {
            $("#presets_fm_2").toggle(value === "ON");
        } // case
        break;

        case "fm_band_ast":
        {
            $("#presets_fm_ast").toggle(value === "ON");
        } // case
        break;

        case "am_band":
        {
            // Has anything changed?
            if (value === handleItemChange.amBand) break;
            handleItemChange.amBand = value;

            $("#presets_am").toggle(value === "ON");

            // In "AM" mode, show tuner frequency large
            $("#frequency_data_large").toggle(value === "ON");
            $("#frequency_data_small").toggle(value !== "ON");
        } // case
        break;

        case "tuner_memory":
        {
            // Check for legal value
            if (value !== "-" && (value < "1" || value > "6")) break;

            // Switch to head unit display if applicable
            if ($("#clock").is(":visible")) selectDefaultScreen();

            // Has anything changed?
            if (value === handleItemChange.currentTunerPresetMemoryValue) break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            if (handleItemChange.currentTunerPresetMemoryValue >= "1"
                && handleItemChange.currentTunerPresetMemoryValue <= "6")
            {
                // Un-highlight the previous entry in the "tuner_presets_popup"
                var highlightId = "presets_memory_" + handleItemChange.currentTunerPresetMemoryValue + "_select";
                $("#" + highlightId).hide();
            } // if

            // Make sure the audio settings popup is hidden
            hideAudioSettingsPopup();

            // Changed to a non-preset frequency? Or just changed audio source? Then suppress the tuner presets popup.
            if (value === "-" || handleItemChange.hideHeadUnitPopupsTimer != null)
            {
                // Make sure the tuner presets popup is hidden
                hideTunerPresetsPopup();
            }
            else
            {
                // Highlight the corresponding entry in the "tuner_presets_popup", in case that is currently visible
                // due to a recent preset button press on the head unit
                var highlightId = "presets_memory_" + value + "_select";
                $("#" + highlightId).show();
            } // if

            handleItemChange.currentTunerPresetMemoryValue = value;
        } // case
        break;

        case "rds_text":
        {
            // Has anything changed?
            if (value === handleItemChange.rdsText) break;
            handleItemChange.rdsText = value;

            //console.log("Item '" + item + "' set to '" + value + "'");

            var showRdsText = value !== "" && tunerSearchMode !== "MANUAL_TUNING";

            // If RDS text, show tuner frequency small, and RDS text large
            $("#frequency_data_small").toggle(showRdsText);
            $("#rds_text").toggle(showRdsText);

            // If no RDS text, show tuner frequency large
            $("#frequency_data_large").toggle(! showRdsText);
        } // case
        break;

        case "search_mode":
        {
            // Has anything changed?
            if (value === tunerSearchMode) break;
            tunerSearchMode = value;

            //console.log("Item '" + item + "' set to '" + value + "'");

            hideAudioSettingsPopup();
            hideTunerPresetsPopup();

            var rdsText = $("#rds_text").text();
            var showRdsText = $("#fm_band").hasClass("ledOn") && rdsText !== "" && value !== "MANUAL_TUNING";

            // If there is RDS text, and no manual frequency search is busy, show tuner frequency small,
            // and RDS text large
            $("#frequency_data_small").toggle(showRdsText);
            $("#rds_text").toggle(showRdsText);

            // If there is no RDS text, or a manual frequency search is busy, show tuner frequency large
            // (on the place where normally the RDS text is shown)
            $("#frequency_data_large").toggle(! showRdsText);
        } // case
        break;

        case "pty_standby_mode":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Show this LED only when in "PTY standby mode"
            $("#pty_standby_mode").toggle(value === "YES");
        } // case
        break;

        case "pty_selection_menu":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Show this LED only when in "PTY selection mode"
            $("#pty_selection_menu").toggle(value === "ON");

            // Show the selected PTY instead of that of the current station
            $("#pty_16").toggle(value !== "ON");
            $("#selected_pty_16").toggle(value === "ON");
        } // case
        break;

        case "info_traffic":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            if (typeof handleItemChange.infoTraffic === "undefined") handleItemChange.infoTraffic = "NO";

            // Don't pop up when user is browsing the menus
            //if (currentMenu) break;
            if (inMenu()) break;

            // Has anything changed?
            if (value === handleItemChange.infoTraffic) break;
            handleItemChange.infoTraffic = value;

            if (value === "YES") showNotificationPopup("Info Traffic!", 120000);  // At most 2 minutes
            else hidePopup("notification_popup");
        } // case
        break;

        case "cd_status":
        {
            $("#cd_status_error").toggle(value === "ERROR");
            if (value === "ERROR") showNotificationPopup("Disc unreadable<br />(upside down?)", 6000)
        } // case
        break;

        case "cd_status_fast_forward":
        case "cd_status_rewind":
        case "cd_status_searching":
        {
            if (value === "ON") hideAudioSettingsPopup();
        } // case
        break;

        case "cd_changer_status":
        {
            $("#cd_changer_status_error").toggle(value === "ERROR");
            if (value === "ERROR") showNotificationPopup("Disc unreadable<br />(wrong side?)", 6000)
        } // case
        break;

        case "cd_changer_current_disc":
        {
            // Has anything changed?
            if (value === cdChangerCurrentDisc) break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            if (cdChangerCurrentDisc != null && cdChangerCurrentDisc.match(/^[1-6]$/))
            {
                var selector = "#cd_changer_disc_" + cdChangerCurrentDisc + "_present";
                $(selector).removeClass("ledActive");
                $(selector).css({marginTop: '+=25px'});
            } // if

            // Only if valid value (can be "--" if cartridge is not present)
            if (value.match(/^[1-6]$/))
            {
                var selector = "#cd_changer_disc_" + value + "_present";
                $(selector).addClass("ledActive");
                $(selector).css({marginTop: '-=25px'});
            } // if

            cdChangerCurrentDisc = value;
        } // case
        break;

        case "audio_source":
        {
            // Sat nav audio (e.g. guidance instruction or welcome message) needs no further action
            if (value === "NAVIGATION") break;

            // Has anything changed?
            if (value === handleItemChange.currentAudioSource) break;
            handleItemChange.currentAudioSource = value;

            //console.log("Item '" + item + "' set to '" + value + "'");

            // When in a menu, a change in the audio source does not imply any further action
            //if (currentMenu) break;
            if (inMenu()) break;

            // Suppress?
            if (suppressScreenChangeToAudio != null) break;

            // Hide the audio settings or tuner presets popup (if visible)
            hideAudioSettingsPopup();
            hideTunerPresetsPopup();

            // Suppress the audio settings popup for the next 0.4 seconds (bit clumsy, but it is effective)
            clearInterval(handleItemChange.hideHeadUnitPopupsTimer);
            handleItemChange.hideHeadUnitPopupsTimer = setTimeout(
                function () { handleItemChange.hideHeadUnitPopupsTimer = null; },
                400
            );

            if (value === "CD")
            {
                // Hide all icons except "searching" or "loading"
                $("#cd_status_pause").hide();
                $("#cd_status_play").hide();
                $("#cd_status_fast_forward").hide();
                $("#cd_status_rewind").hide();
            }
            else if (value === "CD_CHANGER")
            {
                // Show the "loading" icon
                $("#cd_changer_status_pause").hide();
                $("#cd_changer_status_play").hide();
                $("#cd_changer_status_loading").show();
                $("#cd_changer_status_fast_forward").hide();
                $("#cd_changer_status_rewind").hide();
                //$("#cd_changer_status_next_track").hide();
                //$("#cd_changer_status_previous_track").hide();
            } // if

            selectDefaultScreen(value);
        } // case
        break;

        case "remaining_km_to_service":
        case "odometer_1":
        {
            // Add a space between hundreds and thousands for better readability
            $("#" + item).text(numberWithSpaces(value));
        } // case
        break;

        case "economy_mode":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Check if anything actually changed
            if (value === handleItemChange.economyMode) break;
            handleItemChange.economyMode = value;

            if (value !== "ON") break;

            if (currentLargeScreenId !== "pre_flight") showNotificationPopup("Changing to<br />power-save mode", 15000);
        } // case
        break;

        case "door_open":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Show or hide the "door open" popup
            if (value === "YES")
            {
                $("#door_open_popup_text").text("Door open");
                showPopup("door_open_popup", 8000);
            }
            else
            {
                hidePopup("door_open_popup");
            } // if
        } // case
        break;

        case "door_front_right":
        case "door_front_left":
        case "door_rear_right":
        case "door_rear_left":
        case "door_boot":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Show or hide the door SVG element
            document.getElementById(item).style.display = value === "OPEN" ? "block" : "none";

            var isOpen = value === "OPEN";

            var change = false;
            if (item === "door_boot")
            {
                if (isBootOpen !== isOpen) change = true;
                isBootOpen = isOpen;
            }
            else
            {
                if (isDoorOpen[item] !== isOpen) change = true;
                isDoorOpen[item] = isOpen;
            } // if

            // Only continue if anything changed
            if (! change) break;

            var nDoorsOpen = 0;
            for (var id in isDoorOpen) { if (isDoorOpen[id]) nDoorsOpen++; }

            // All closed?
            if (! isBootOpen && nDoorsOpen == 0)
            {
                hidePopup("door_open_popup");
                break;
            } // if

            // Compose popup text
            var popupText = " open";
            if (isBootOpen) popupText = "boot" + popupText;
            if (isBootOpen && nDoorsOpen > 0) popupText = " and " + popupText;
            if (nDoorsOpen > 1) popupText = "s" + popupText;
            if (nDoorsOpen > 0) popupText = "door" + popupText;

            // Capitalize first letter
            popupText = popupText.charAt(0).toUpperCase() + popupText.slice(1);

            // Set the correct text and show the "door open" popup
            $("#door_open_popup_text").text(popupText);
            showPopup("door_open_popup", 8000);
        } // case
        break;

        case "engine_running":
        {
            // Has anything changed?
            if (value === engineRunning) break;
            engineRunning = value;

            // If engine just started, and currently in "pre_flight" screen, then switch to "instruments"
            if (engineRunning === "YES" && $("#pre_flight").is(":visible")) changeLargeScreenTo("instruments");
        } // case
        break;

        case "contact_key_position":
        {
            // Has anything changed?
            if (value === handleItemChange.contactKeyPosition) break;
            handleItemChange.contactKeyPosition = value;

            //console.log("Item '" + item + "' set to '" + value + "'");

            // Show "Pre-flight checks" screen if contact key is in "ON" position, without engine running
            if (value === "ON" && engineRunning === "NO")
            {
                clearInterval(handleItemChange.contactKeyOffTimer);
                changeLargeScreenTo("pre_flight");
                hidePopup();
            }
            else if (value === "ACC")
            {
                clearInterval(handleItemChange.contactKeyOffTimer);
            }
            else if (value === "OFF")
            {
                // After 2 seconds, show the audio screen (or fallback to current location or clock)
                clearInterval(handleItemChange.contactKeyOffTimer);
                handleItemChange.contactKeyOffTimer = setTimeout(
                    function ()
                    {
                        selectDefaultScreen();
                        handleItemChange.contactKeyOffTimer = null;
                    },
                    2000
                );
            } // if
        } // case
        break;

        case "notification_message_on_mfd":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Only if not empty
            if (value === "") break;

            var message = value.slice(0, -1);
            var isWarning = value.slice(-1) === "!";
            showNotificationPopup(message, 10000, isWarning);
        } // case
        break;

        case "satnav_status_1":
        {
            satnavStatus1 = value;

            if (satnavStatus1.match(/AUDIO/))
            {
                // Turn on or off "satnav_audio" LED 
                var playingAudio = satnavStatus1.match(/START/) !== null;
                $("#satnav_audio").toggleClass("ledOn", playingAudio);
                $("#satnav_audio").toggleClass("ledOff", ! playingAudio);
            } // if

            // Show one or the other
            $("#satnav_distance_to_dest_via_road_visible").toggle(satnavStatus1 !== "DESTINATION_NOT_ON_MAP");
            $("#satnav_distance_to_dest_via_straight_line_visible").toggle(satnavStatus1 === "DESTINATION_NOT_ON_MAP");

            if (satnavStatus1 === "DESTINATION_NOT_ON_MAP")
            {
                if ($("#satnav_guidance").is(":visible") && $(".notificationPopup:visible").length === 0)
                {
                    if (! satnavDestinationNotAccessibleByRoadPopupShown)
                    {
                        // Show only at start of guidance
                        showStatusPopup("Destination is not<br />accessible by road", 8000);
                        satnavDestinationNotAccessibleByRoadPopupShown = true;
                    } // if
                } // if
            }
            else if (satnavStatus1.match(/NO_DISC/))
            {
                showStatusPopup("Navigation CD-ROM is<br />missing", 5000);
            } // if
            else if (satnavStatus1.match(/DISC_UNREADABLE/))
            {
                showStatusPopup("Navigation CD-ROM<br />is unreadable", 5000);
            } // if
        } // case
        break;

        case "satnav_status_2":
        {
            // Only if valid
            if (value === "") break;

            // As soon as we receive anything from the sat nav, enable the "Navigation/Guidance" button in the main menu
            // TODO - if this actually enables the button, also select it?
            $("#main_menu_goto_satnav_button").removeClass("buttonDisabled");

            // Has anything changed?
            if (value === satnavMode) break;
            var previousSatnavMode = satnavMode;
            satnavMode = value;
            localStorage.satnavMode = value;

            // Button texts in menus
            $("#satnav_navigation_options_menu_stop_guidance_button").text(
                value === "IN_GUIDANCE_MODE" ? "Stop guidance" : "Resume guidance");
            $("#satnav_tools_menu_stop_guidance_button").text(
                value === "IN_GUIDANCE_MODE" ? "Stop guidance" : "Resume guidance");

            if (value === "IN_GUIDANCE_MODE")
            {
                // Just entered guidance mode

                if ($("#satnav_guidance_preference_menu").is(":visible")) break;
                satnavSwitchToGuidanceScreen();

                // If the small screen (left) is currently "gps_info", then go to the next screen ("instrument_small").
                // When the system is in guidance mode, the right stalk button does not cycle through "gps_info".
                if (currentSmallScreenId === "gps_info") changeSmallScreenTo("instrument_small");

                break;
            }

            if (previousSatnavMode === "IN_GUIDANCE_MODE")
            {
                // Just left guidance mode
                if ($("#satnav_guidance").is(":visible")) selectDefaultScreen();
            } // if

            if (value === "IDLE")
            {
                satnavRouteComputed = false;
            } // if

            if (value === "INITIALIZING")
            {
                if (currentLargeScreenId === "satnav_main_menu" || currentLargeScreenId === "satnav_disclaimer")
                {
                    // In these screens, show the "Initializing navigator" popup as long as the mode is "INITIALIZING"
                    showPopup("satnav_initializing_popup", 20000);
                } // if
            }
            else
            {
                // Hide any popup "Navigation system being initialized" if no longer "INITIALIZING"
                hidePopup("satnav_initializing_popup");
            } // if
        } // case
        break;

        case "satnav_status_3":
        {
            if (value === "STOPPING_NAVIGATION") satnavDestinationNotAccessibleByRoadPopupShown = false;

            if (value === "POWERING_OFF")
            {
                $("#main_menu_goto_satnav_button").addClass("buttonDisabled");
                $("#satnav_disc_recognized").addClass("ledOff");
                handleItemChange.nSatNavDiscUnreadable = 1;
                satnavDisclaimerAccepted = false;
            }
            else if (value === "CALCULATING_ROUTE")
            {
                // TODO - necessary?
                //showPopup("satnav_calculating_route_popup", 30000);
            } // if
        } // case
        break;

        case "satnav_guidance_status":
        {
            if (value.match(/CALCULATING_ROUTE/)) showPopup("satnav_calculating_route_popup", 30000);
            else hidePopup("satnav_calculating_route_popup");
        } // case
        break;

        case "satnav_route_computed":
        {
            var newValue = value === "YES" && satnavMode !== "IDLE";

            // Has anything changed?
            if (newValue === satnavRouteComputed) break;
            satnavRouteComputed = newValue;

            //console.log("Item '" + item + "' set to '" + value + "'");

            if (satnavMode === "IN_GUIDANCE_MODE")
            {
                if ($("#satnav_guidance").is(":visible"))
                {
                    if (! satnavRouteComputed) $("#satnav_guidance_next_street").text("Retrieving next instruction");
                }
                else
                {
                    if (! $("#satnav_guidance_preference_menu").is(":visible")) satnavSwitchToGuidanceScreen();
                } // if
            } // if
        } // case
        break;

        case "satnav_disc_recognized":
        {
            if (handleItemChange.nSatNavDiscUnreadable === undefined) handleItemChange.nSatNavDiscUnreadable = 0;

            if (value === "YES") handleItemChange.nSatNavDiscUnreadable = 0;
            else handleItemChange.nSatNavDiscUnreadable++;

            // At the 4-th and 14-th time of consequently reporting "NO", shortly show a status popup
            if (handleItemChange.nSatNavDiscUnreadable == 4 || handleItemChange.nSatNavDiscUnreadable == 14)
            {
                showStatusPopup("Navigation CD-ROM<br />is unreadable", 4000);
            } // if
        } // case
        break;

        case "satnav_current_destination_city":
        {
            if (! $("#satnav_enter_city_characters").is(":visible")) break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");

            $("#satnav_entered_string").text("");
            satnavLastEnteredChar = null;
        } // case
        break;

        case "satnav_current_destination_street":
        {
            $("#satnav_current_destination_street_shown").text(value === "" ? "City centre" : value);

            if (! $("#satnav_enter_street_characters").is(":visible")) break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");

            $("#satnav_entered_string").text("");
            satnavLastEnteredChar = null;
        } // case
        break;

        case "satnav_personal_address_street":
        case "satnav_professional_address_street":
        {
            $("#" + item + "_shown").text(value === "" ? "City centre" : value);
        } // case
        break;

        case "satnav_last_destination_street":
        {
            $('[gid="satnav_last_destination_street_shown"]').text(value === "" ? "City centre" : value);
        } // case
        break;

        case "satnav_current_destination_house_number":
        case "satnav_personal_address_house_number":
        case "satnav_professional_address_house_number":
        {
            $("#" + item + "_shown").text(value === "" || value === "0" ? "No number" : value);
        } // case
        break;

        case "satnav_last_destination_house_number":
        {
            $('[gid="satnav_last_destination_house_number_shown"]').text(value === "" ? "No number" : value);
        } // case
        break;

        case "satnav_curr_street":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            satnavCurrentStreet = value;

            if (satnavMode === "IN_GUIDANCE_MODE")
            {
                $("#satnav_guidance_curr_street").text(value !== "" ? value : "Street not listed");
            } // if

            $('[gid="satnav_curr_street_shown"]').text(value !== "" ? value : "Not on map"); // TODO - find exact text

            if (value === "") break;

            // Only if the clock is currently showing, i.e. don't move away from the Tuner or CD player screen
            if (! $("#clock").is(":visible")) break;

            // Show the current location (street and city)
            changeLargeScreenTo("satnav_current_location");
        } // case
        break;

        case "satnav_next_street":
        {
            // No data means: stay on current street.
            // But if current street is empty (""), then just keep the current text.
            if (value !== "") $("#satnav_guidance_next_street").text(value)
            else if (satnavCurrentStreet !== "") $("#satnav_guidance_next_street").text(satnavCurrentStreet);
        } // case
        break;

        case "satnav_personal_address_entry":
        {
            satnavDirectoryEntry = value;

            // The "Rename" resp. "Validate" buttons are disabled if the entry is not on the current disc
            // (value ends with "&#x24E7;" (X))
            var entryNotOnDisc = value.match(/&#x24E7;$/) !== null;

            if ($("#satnav_personal_address_manage_buttons").is(":visible"))
            {
                $("#satnav_rename_entry_in_directory_title").text("Rename (" + value + ")");
                $("#satnav_delete_directory_entry_in_popup").text(value);

                $("#satnav_manage_personal_address_rename_button").toggleClass("buttonDisabled", entryNotOnDisc);
                $("#satnav_manage_personal_address_rename_button").toggleClass("buttonSelected", ! entryNotOnDisc);
                $("#satnav_manage_personal_address_delete_button").toggleClass("buttonSelected", entryNotOnDisc);
            }
            else
            {
                $("#satnav_show_personal_address_validate_button").toggleClass("buttonDisabled", entryNotOnDisc);
                $("#satnav_show_personal_address_validate_button").toggleClass("buttonSelected", ! entryNotOnDisc);
            } // if
        } // case
        break;

        case "satnav_professional_address_entry":
        {
            satnavDirectoryEntry = value;

            // The "Rename" resp. "Validate" buttons are disabled if the entry is not on the current disc
            // (value ends with "&#x24E7;" (X))
            var entryNotOnDisc = value.match(/&#x24E7;$/) !== null;

            if ($("#satnav_professional_address_manage_buttons").is(":visible"))
            {
                $("#satnav_rename_entry_in_directory_title").text("Rename (" + value + ")");
                $("#satnav_delete_directory_entry_in_popup").text(value);

                $("#satnav_manage_professional_address_rename_button").toggleClass("buttonDisabled", entryNotOnDisc);
                $("#satnav_manage_professional_address_rename_button").toggleClass("buttonSelected", ! entryNotOnDisc);
                $("#satnav_manage_professional_address_delete_button").toggleClass("buttonSelected", entryNotOnDisc);
            }
            else
            {
                $("#satnav_show_professional_address_validate_button").toggleClass("buttonDisabled", entryNotOnDisc);
                $("#satnav_show_professional_address_validate_button").toggleClass("buttonSelected", ! entryNotOnDisc);
            } // if
        } // case
        break;

        case "mfd_to_satnav_enter_character":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            if (value === "" || value === "?")
            {
                satnavLastEnteredChar = null;
                break;
            } // if

            // Save for later; the same character can be repeated by the user pressing the "Val" button on the
            // IR remote control. In that case we do not get another "mfd_to_satnav_enter_character" value.
            // See code at 'case "mfd_to_satnav_instruction"'.
            satnavLastEnteredChar = value;

            if (value === "Esc")
            {
                // Going into "character roll-back" mode
                satnavRollingBackEntryByCharacter = true;
                break;
            } // if

            satnavRollingBackEntryByCharacter = false;

            satnavEnterByLetterMode();
        } // case
        break;

        case "mfd_to_satnav_go_to_screen":
        {
            // A VAN packet indicates we must switch to a specific screen

            if (value === "") break;

            // Has anything changed?
            if (value === currentLargeScreenId) break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            if (value === "satnav_choose_from_list")
            {
                if ($("#satnav_enter_street_characters").is(":visible")
                    && $("#satnav_enter_characters_list_button").hasClass("buttonSelected"))
                {
                    userHadOpportunityToEnterStreet = true;
                } // if

                satnavGotoListScreen();

                // Also make the list box invisible so that the spinning disk icon behind it appears
                // TODO - make this work
                //document.getElementById("satnav_list").style.display = "none";
                break;
            }

            if (value === "satnav_enter_city_characters")
            {
                // Switching (back) into the "satnav_enter_city_characters" screen

                if (currentMenu === "satnav_current_destination_house_number") exitMenu();

                if (currentMenu === "satnav_enter_street_characters") exitMenu();
                else gotoMenu(value);

                break;
            }

            if (value === "satnav_enter_street_characters")
            {
                // Switching (back) into the "satnav_enter_street_characters" screen

                if (currentMenu === "satnav_current_destination_house_number") exitMenu();
                else satnavGotoEnterStreetOrNumber();

                break;
            }

            if (value === "satnav_current_destination_house_number")
            {
                satnavLastEnteredChar = null;

                // Clear the entered house number
                satnavClearEnteredNumber();

                gotoMenu(value);
                break;
            } // if

            if (value === "satnav_show_current_destination")
            {
                gotoMenu(value);
                break;
            } // if

            if (value === "satnav_show_personal_address"
                || value === "satnav_show_professional_address"
                || value === "satnav_show_service_address")
            {
                gotoMenu(value);
                break;
            } // if

            changeLargeScreenTo(value);
        } // case
        break;

        case "mfd_to_satnav_request":
        {
            handleItemChange.mfdToSatnavRequest = value;
        } // case
        break;

        case "mfd_to_satnav_request_type":
        {
            if (value !== "REQ_N_ITEMS") break;

            switch(handleItemChange.mfdToSatnavRequest)
            {
                case "Enter city":
                case "Enter street":
                {
                    $("#satnav_to_mfd_show_characters_line_1").text("");
                    $("#satnav_to_mfd_show_characters_line_2").text("");

                    // Show the spinning disc after a second or so
                    clearInterval(handleItemChange.showCharactersSpinningDiscTimer);
                    handleItemChange.showCharactersSpinningDiscTimer = setTimeout(
                        function () { $("#satnav_to_mfd_show_characters_spinning_disc").show(); },
                        1500);
                } // case
                break;

                // case "Service":
                // {
                    // // Clear the list box
                    // $("#satnav_list").text("");
                    // highlightIndexes["satnav_list"] = 0;
                // } // case
                // break;

                default:
                break;
            } // switch
        } // case
        break;

        case "satnav_to_mfd_response":
        {
            // Remember the last response topic
            if (value === satnavToMfdResponse) break;
            satnavToMfdResponse = value;
        } // case
        break;

        case "satnav_to_mfd_list_size":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Hide the spinning disc
            clearInterval(handleItemChange.showCharactersSpinningDiscTimer);
            $("#satnav_to_mfd_show_characters_spinning_disc").hide();

            // "Select a service" menu item: enable if sat nav responds with a list size (any will do)
            if (satnavToMfdResponse === "Service")
            {
                // Clear the list box
                // $("#satnav_list").text("");
                // highlightIndexes["satnav_list"] = 0;

                if (parseInt(value) > 0) $("#satnav_main_menu_select_a_service_button").removeClass("buttonDisabled");
            } // if

            // "List XXX" button: only enable if less than 80 items in list
            $("#satnav_enter_characters_list_button").toggleClass("buttonDisabled", parseInt(value) > 80);

            if ($("#mfd_to_satnav_request").is(":visible") && value == 0)
            {
                var title = $("#mfd_to_satnav_request").text();
                if (title.match(/^Service/))
                {
                    // User selected a service which has no address entries for the specified location
                    showStatusPopup("This service is<br />not available for<br />this location", 8000);
                }
                else if (title === "Personal address list")
                {
                    exitMenu();
                    showStatusPopup("Personal directory<br />is empty", 8000);
                }
                else if (title === "Professional address list")
                {
                    exitMenu();
                    showStatusPopup("Professional directory<br />is empty", 8000);
                } // if
            } // if
        } // case
        break;

        case "satnav_list":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            satnavCheckIfCityCenterMustBeAdded();

            // Highlight the current line (or the first, if no line is currently highlighted)
            highlightLine("satnav_list");

            var title = $("#mfd_to_satnav_request").text();
            if (title === "Personal address list")
            {
                // Store the list of entries. When the user tries to create an entry with an existing name,
                // the "Validate" button must be disabled.
                satnavPersonalDirectoryEntries = value;

                // Remove invalid characters
                satnavPersonalDirectoryEntries = satnavPersonalDirectoryEntries.map(function(entry)
                {
                    entry = entry.replace(/&.*?;/g, "");
                    entry = entry.replace(/\?/g, "");
                    entry = entry.trim();
                    return entry;
                });

                // Save in local (persistent) store
                localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);
            }
            else if (title === "Professional address list")
            {
                // Store the list of entries. When the user tries to create an entry with an existing name,
                // the "Validate" button must be disabled.
                satnavProfessionalDirectoryEntries = value;

                // Remove invalid characters
                satnavProfessionalDirectoryEntries = satnavProfessionalDirectoryEntries.map(function(entry)
                {
                    entry = entry.replace(/&.*?;/g, "");
                    entry = entry.replace(/\?/g, "");
                    entry = entry.trim();
                    return entry;
                });

                // Save in local (persistent) store
                localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);
            } // if

            // TODO - make this work
            // Make the list box visible so that the spinning disk icon disappears behind it
            //document.getElementById("satnav_list").style.display = "block";
        } // case
        break;

        case "mfd_to_satnav_offset":
        {
            if (value === "") break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            if (! $("#satnav_show_service_address").is(":visible")) break;

            satnavServiceAddressSetButtons(parseInt(value) + 1);
        } // case
        break;

        case "satnav_guidance_preference":
        {
            if (value === "---") break;

            satnavGuidanceSetPreference(value);

            // Save also in local (persistent) store
            localStorage.satnavGuidancePreference = value;
        } // case
        break;

        case "satnav_service_address_distance":
        case "satnav_distance_to_dest_via_road":
        case "satnav_distance_to_dest_via_straight_line":
        case "satnav_turn_at":
        {
            var a = satnavFormatDistance(value);
            var distance = a[0];
            var unit = a[1];
            $("#" + item + "_number").text(distance);
            $("#" + item + "_unit").text(unit);
        } // case
        break;

        case "satnav_heading_on_roundabout_as_text":
        {
            // Show or hide the roundabout
            $("#satnav_curr_turn_roundabout").toggle(value !== "---");

            // TODO - do we ever need to make "satnav_next_turn_roundabout" visible?
        } // case
        break;

        case "satnav_curr_turn_icon_direction_as_text":
        {
            if (value === "") break;

            // Break out if current turn roundabout is not visible
            if (! $("#satnav_curr_turn_roundabout").is(":visible")) break;

            // Has anything changed?
            if (value === handleItemChange.currentTurnDirection) break;
            handleItemChange.currentTurnDirection = value;

            // Draw the small arc inside the roundabout
            document.getElementById("satnav_curr_turn_icon_direction_on_roundabout").setAttribute
            (
                "d",
                describeArc(150, 150, 30, value - 180, 180)
            );
        } // case
        break;

        case "satnav_next_turn_icon_direction_as_text":
        {
            if (value === "") break;

            // Break out if next turn roundabout is not visible
            if (! $("#satnav_next_turn_roundabout").is(":visible")) break;

            // Has anything changed?
            if (value === handleItemChange.nextTurnDirection) break;
            handleItemChange.nextTurnDirection = value;

            // Draw the small arc inside the roundabout
            document.getElementById("satnav_next_turn_icon_direction_on_roundabout").setAttribute
            (
                "d",
                describeArc(50, 150, 30, value - 180, 180)
            );
        } // case
        break;

        case "mfd_to_satnav_instruction":
        {
            if (value === "Selected_city_from_list")
            {
                // Clear the character-by-character entry string
                $("#satnav_entered_string").text("");
                satnavLastEnteredChar = null;
            } //if

            if (value !== "Val") break;

            if (satnavRollingBackEntryByCharacter)
            {
                // In "character roll-back" mode, remove the last entered character
                satnavRemoveEnteredCharacter();
            }
            else
            {
                if (satnavLastEnteredChar == null) break;

                // The user is pressing "Val" on the remote control, to enter the chosen letter

                // Append the entered character
                $("#satnav_entered_string").append(satnavLastEnteredChar);

                // Enable the "Correction" button
                $("#satnav_enter_characters_correction_button").removeClass("buttonDisabled");

                // Disable the "Validate" button
                $("#satnav_enter_characters_validate_button").addClass("buttonDisabled");

                // Go to the first line in the "satnav_list" screen
                highlightFirstLine("satnav_list");
            } // if
        } // case
        break;

        case "satnav_to_mfd_show_characters":
        {
            if (value === "") break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            // New set of available characters coming in means: reset all highlight positions
            highlightIndexes["satnav_rename_entry_in_directory_characters_line_1"] = 0;
            highlightIndexes["satnav_rename_entry_in_directory_characters_line_2"] = undefined;
            highlightIndexes["satnav_archive_in_directory_characters_line_1"] = 0;
            highlightIndexes["satnav_archive_in_directory_characters_line_2"] = undefined;
            highlightIndexes["satnav_to_mfd_show_characters_line_1"] = 0;
            highlightIndexes["satnav_to_mfd_show_characters_line_2"] = undefined;

            if ($("#satnav_rename_entry_in_directory_entry").is(":visible"))
            {
                // New set of available characters coming in indicates that the last chosen character has been entered
                satnavDirectoryEntryEnterCharacter("satnav_rename_entry_in_directory", value);
            }
            else if ($("#satnav_archive_in_directory_entry").is(":visible"))
            {
                // New set of available characters coming in indicates that the last chosen character has been entered
                satnavDirectoryEntryEnterCharacter("satnav_archive_in_directory", value);
            }
            else
            {
                // On the original MFD, there is only room for 24 characters. If there are more characters, spill
                // over to the next line.
                var line_1 = value.substr(0, 24);
                var line_2 = value.substr(24);

                $("#satnav_to_mfd_show_characters_line_1").text(line_1);
                $("#satnav_to_mfd_show_characters_line_2").text(line_2);

                // Enable the second line only if it contains something
                $("#satnav_to_mfd_show_characters_line_2").toggleClass("buttonDisabled", line_2.length === 0);

                // When a new list of available characters comes in, the highlight always moves back to the first
                // character.
                if ($("#satnav_to_mfd_show_characters_line_1").hasClass("buttonSelected")
                    || $("#satnav_to_mfd_show_characters_line_2").hasClass("buttonSelected")
                    || $("#satnav_enter_characters_list_button").hasClass("buttonSelected"))
                {
                    satnavSelectFirstAvailableCharacter();
                }
                else if ($("#satnav_enter_characters_correction_button").hasClass("buttonSelected"))
                {
                    // All entered characters have been removed?
                    if ($("#satnav_entered_string").text() === "") satnavSelectFirstAvailableCharacter();
                } // if
            } // if

            if (value.length === 1)
            {
                // Save for later: this character can be automatically selected by the MFD without the user explicitly
                // pressing the "Val" button on the remote control. See code at 'case "mfd_to_satnav_instruction"'.
                satnavLastEnteredChar = value;
            } // if
        } // case
        break;

        case "satnav_house_number_range":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Store the lowest and highest number
            lowestHouseNumberInRange = parseInt(value.replace(/From (.*) to .*/, "$1"));
            highestHouseNumberInRange = parseInt(value.replace(/From .* to (.*)/, "$1"));

            if (lowestHouseNumberInRange == 0 && highestHouseNumberInRange == 0)
            {
                // No house number can be entered: go directly to next screen 
                gotoMenu("satnav_show_current_destination");
            }
            else
            {
                changeLargeScreenTo("satnav_current_destination_house_number");

                // The following will get the number of digits in the highest number, and show that amount of '_'
                // characters on the place to enter the number
                satnavClearEnteredNumber();
            } // if
        } // case
        break;

        case "satnav_follow_road_icon":
        case "satnav_turn_around_if_possible_icon":
        case "satnav_fork_icon_keep_right":
        case "satnav_fork_icon_keep_left":
        case "satnav_fork_icon_take_right_exit":
        case "satnav_fork_icon_take_left_exit":
        case "satnav_curr_turn_icon":
        case "satnav_next_turn_icon":
        {
            // Any of these icons becoming visible means we can hide the "Computing route in progress" popup
            if (value === "ON") hidePopup('satnav_calculating_route_popup');
        } // case
        break;

        case "satnav_not_on_map_icon":
        {
            if (value === "ON")
            {
                // Hide the "Computing route in progress" popup, if showing
                hidePopup("satnav_calculating_route_popup");

                $("#satnav_guidance_next_street").text("Follow the heading");
                $("#satnav_guidance_curr_street").text("Not digitized area");
            } // if

            $("#satnav_turn_at_indication").toggle(value !== "ON");
        } // case
        break;

        case "satnav_curr_heading_compass_needle_rotation":
        {
            // Place the "N" just in front of the arrow point in the compass
            $("#satnav_curr_heading_compass_tag").css(
                "transform",
                "translate(" +
                    42 * Math.sin(value * Math.PI / 180) + "px," +
                    -42 * Math.cos(value * Math.PI / 180) + "px)"
                );

            // Enable elements as soon as a value is received
            $("#satnav_curr_heading_compass_tag").find("*").removeClass('satNavInstructionDisabledIconText');
            $("#satnav_curr_heading_compass_needle").find("*").removeClass('satNavInstructionDisabledIcon');
        } // case
        break;

        case "satnav_heading_to_dest_pointer_rotation":
        {
            // Place the "D" just in front of the arrow point
            $("#satnav_heading_to_dest_tag").css(
                "transform",
                "translate(" +
                    70 * Math.sin(value * Math.PI / 180) + "px," +
                    -70 * Math.cos(value * Math.PI / 180) + "px)"
                );

            // Enable elements as soon as a value is received
            $("#satnav_heading_to_dest_tag").find("*").removeClass('satNavInstructionDisabledIconText');
            $("#satnav_heading_to_dest_pointer").find("*").removeClass('satNavInstructionDisabledIcon');
        } // case
        break;

        case "small_screen":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Has anything changed?
            if (value === localStorage.smallScreen) break;
            localStorage.smallScreen = value;

            gotoSmallScreen(value);
        } // case
        break;

        case "mfd_remote_control":
        {
            // Ignore remote control buttons if contact key is off
            var contactKeyPosition = $("#contact_key_position").text();
            if (contactKeyPosition === "OFF") break;

            // Ignore remote control buttons when in power-save mode
            if (handleItemChange.economyMode === "ON") break;

            // Ignore same button press within 150 ms
            if (value === handleItemChange.lastIrCode && Date.now() - handleItemChange.lastIrCodeProcessed < 150)
            {
                break;
            } // if

            handleItemChange.lastIrCode = value;
            handleItemChange.lastIrCodeProcessed = Date.now();

            //console.log("Item '" + item + "' set to '" + value + "'");

            if (value === "MENU_BUTTON")
            {
                // Directly hide any visible audio popup
                hideAudioSettingsPopup();
                hideTunerPresetsPopup();

                gotoTopLevelMenu();
            }
            else if (value === "ESC_BUTTON")
            {
                // If a status popup is showing, hide it and break
                if (hidePopup("status_popup")) break;
                if (hidePopup("notification_popup")) break;

                // Also close these, if currently shown
                if (hideTunerPresetsPopup()) break;
                if (hideAudioSettingsPopup()) break;
                if (hidePopup("satnav_initializing_popup")) break;
                if (hidePopup("satnav_input_stored_in_personal_dir_popup")) break;
                if (hidePopup("satnav_input_stored_in_professional_dir_popup")) break;
                if (hidePopup("satnav_delete_item_popup")) break;
                if (hidePopup("satnav_delete_directory_data_popup")) break;

                // Hiding this popup might make the underlying "Computing route in progress" popup visible
                if (hidePopup("satnav_guidance_preference_popup")) break;

                // Ignore the "Esc" button when in guidance mode
                if ($("#satnav_guidance").is(":visible")) break;

                exitMenu();
            }
            else if (value === "UP_BUTTON"
                    || value === "DOWN_BUTTON"
                    || value === "LEFT_BUTTON"
                    || value === "RIGHT_BUTTON")
            {
                // Entering a character in the "satnav_enter_street_characters" screen?
                if ($("#satnav_enter_street_characters").is(":visible"))
                {
                    userHadOpportunityToEnterStreet = true;
                } // if

                keyPressed(value);
            }
            else if (value === "ENTER_BUTTON")
            {
                // Ignore if just changed screen; sometimes the screen change is triggered by the system, not the
                // remote control. If the user wanted pressed a button on the previous screen, it will end up
                // being handled by the current screen, which is confusing.
                if (Date.now() - lastScreenChangedAt < 250)
                {
                    console.log("// mfd_remote_control - ENTER_BUTTON: ignored");
                    break;
                } // if

                // If a status popup is showing, hide it and break
                if (hidePopup("status_popup")) break;

                // Also close these, if currently shown
                if (hideTunerPresetsPopup()) break;
                if (hideAudioSettingsPopup()) break;
                if (hidePopup("satnav_initializing_popup")) break;
                if (hidePopup("satnav_input_stored_in_personal_dir_popup")) break;
                if (hidePopup("satnav_input_stored_in_professional_dir_popup")) break;
                if (hidePopup("satnav_delete_directory_data_popup")) break;

                // Entering a character in the "satnav_enter_street_characters" screen?
                if ($("#satnav_enter_street_characters").is(":visible")) userHadOpportunityToEnterStreet = true;

                // In sat nav guidance mode, clicking "Val" shows the "Guidance tools" menu
                if (satnavMode === "IN_GUIDANCE_MODE"  // In guidance mode?
                    && $(".notificationPopup:visible").length === 0  // No popup showing?
                    && ! inMenu())  // Not in any menu?
                {
                    gotoTopLevelMenu("satnav_guidance_tools_menu");
                }
                else
                {
                    buttonClicked();
                } // if
            }
            else if (value === "MODE_BUTTON")
            {
                // Ignore when user is browsing the menus
                if (inMenu()) break;

                nextLargeScreen();
            } // if
        } // case
        break;

        case "mfd_status":
        {
            if (value === "MFD_SCREEN_ON" && handleItemChange.economyMode === "ON")
            {
                showNotificationPopup("Changing to<br />power-save mode", 15000);
            }
            else if (value === "MFD_SCREEN_OFF")
            {
                hidePopup();
            } // if
        } // case
        break;

        default:
        break;
    } // switch
} // handleItemChange
)=====";