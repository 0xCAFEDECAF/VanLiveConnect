
char mfd_js[] PROGMEM = R"=====(

// Javascript functions to drive the browser-based Multi-Function Display (MFD)

// -----
// General

function numberWithSpaces(x)
{
    return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, " ");
} // numberWithSpaces

function clamp(num, min, max)
{
    return Math.min(Math.max(num, min), max);
} // clamp

// -----
// On-screen clocks

// Show the current date and time
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
    $("#viewport_width").text($(window).width());  // width of browser viewport
    $("#screen_width").text(window.screen.width);
    $("#window_width").text(window.innerWidth);

    $("#viewport_height").text($(window).height());  // height of browser viewport
    $("#screen_height").text(window.screen.height);
    $("#window_height").text(window.innerHeight);
} // showViewportSizes

// -----
// Functions for parsing and handling JSON data as it comes in on a websocket

// Inspired by https://gist.github.com/ismasan/299789
var FancyWebSocket = function(url)
{
    var _url = url;
    var conn = new WebSocket(url);
    var retryTimerId = null;

    var callbacks = {};

    this.bind = function(event_name, callback)
    {
        callbacks[event_name] = callbacks[event_name] || [];
        callbacks[event_name].push(callback);
        return this;  // chainable
    }; // function

    // Dispatch to the right handlers
    conn.onmessage = function(evt)
    {
        var json = JSON.parse(evt.data)
        dispatch(json.event, json.data)
    };  // function

    conn.onopen = function()
    {
        clearInterval(retryTimerId);
        console.log("// Connected to websocket '" + url + "'!");
        dispatch('open', null);
    };

    conn.onclose = function(event)
    {
        console.log("// Connection to websocket '" + url + "' died!");
        dispatch('close', null);
        // TODO - uncomment the following line. Currently it will crash the ESP.
        //retryTimerId = setInterval(() => { conn = new WebSocket(_url); }, 2000);
    };

    var dispatch = function(event_name, message)
    {
        var chain = callbacks[event_name];
        if (chain == undefined) return;  // No callbacks for this event
        for (var i = 0; i < chain.length; i++)
        {
            chain[i](message)
        } // for
    }; // function
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
    // Toggle the "comms_led" to indicate communication activity
    $("#comms_led").toggleClass("ledOn");
    $("#comms_led").toggleClass("ledOff");

    // The following log entries can be used to literally re-play a session; simply copy-paste these lines into the
    // console area of the web-browser. Also it can be really useful to copy and save these lines into a text file
    // for later re-playing at your desk.

    var now = Date.now();
    if (typeof writeToDom.lastInvoked === "undefined")
    {
        // First time: print the sleep function for easy copy-pasting into the browser console.
        // Intervals will be at most 300 milliseconds.
        // Note: some events need a minimum amount of time between them, e.g. pressing a button on the remote control
        // after changing to another screen.
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
            if ($(selector).length <= 0) return;  // Go to next iteration in .each()

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

                $(selector).html(jsonObj[item].join('<br />'));
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
                var itemText = jsonObj[item];
                if ($(selector).hasClass("led"))
                {
                    // Handle "led" class objects: no text copy, just turn ON or OFF
                    var itemTextUc = itemText.toUpperCase();
                    var on = itemTextUc === "ON" || itemTextUc === "YES";
                    $(selector).toggleClass("ledOn", on);
                    $(selector).toggleClass("ledOff", ! on);
                }
                else if ($(selector).hasClass("icon") || $(selector).get(0) instanceof SVGElement)
                {
                    // Handle SVG elements and "icon" class objects: no text copy, just show or hide
                    var itemTextUc = itemText.toUpperCase();
                    var on = itemTextUc === "ON" || itemTextUc === "YES";
                    $(selector).toggle(on);
                }
                else
                {
                    // Handle simple "text" objects
                    $(selector).html(itemText);
                } // if
            } // if
        }); // each

        // Check if a handler must be invoked
        handleItemChange(item, jsonObj[item]);
    } // for
} // writeToDom

var websocketServerHost = window.location.hostname;

// For stability, connect to the web socket server only after a few seconds
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
    2000
);

// -----
// Functions for navigating through the screens and their subscreens/elements

// Open a "trip_info" tab in the small information panel
function openTripInfoTab(evt, tabName)
{
    $("#trip_info .tabContent").hide();
    $("#trip_info .tablinks").removeClass("active");
    $("#" + tabName).show();
    evt.currentTarget.className += " active";
} // openTripInfoTab

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

    hidePopup("audio_popup");

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

var engineCoolantTemperature = 80;  // In degrees Celsius

var menuStack = [];

var mfdLargeScreen = "CLOCK";  // Screen currently shown in the "large" (right hand side) area on the original MFD

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
    if (selectedScreen === "" && satnavCurrentStreet !== "")
    {
        selectedScreen = "satnav_current_location";

        // But don't switch away from instrument screen
        if (currentLargeScreenId === "instruments") return;
    } // if

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
        // Don't cycle through menu screens
        if (inMenu()) return;

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
        if (idIndex === screenIds.indexOf("satnav_current_location") + 1) idIndex = 0;

        // After the "satnav_guidance" screen, go back to the "clock" screen
        if (idIndex === screenIds.indexOf("satnav_guidance") + 1) idIndex = 0;
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
    openTripInfoTab(event, id);
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
            // Fuel consumption data is already permanently visible in the status bar. Show this screen instead.
            changeSmallScreenTo("instrument_small");
        } // case
        break;

        default:
        break;
    } // switch
} // gotoSmallScreen

gotoSmallScreen(localStorage.smallScreen);

// In the "trip_computer_popup", cycle to the next tab
function cycleTabInTripComputerPopup()
{
    // Retrieve all tab buttons and content elements
    var tabs = $("#trip_computer_popup").find(".tabContent");
    var tabButtons = $("#trip_computer_popup").find(".tabLeft");

    // Retrieve current tab button and content element
    var currActiveTab = $("#trip_computer_popup .tabContent:visible");
    var currActiveButton = $("#trip_computer_popup .tabLeft.tabActive");

    var currTabIdx = tabs.index(currActiveTab);
    var currButtonIdx = tabButtons.index(currActiveButton);
    var nTabs = tabButtons.length;

    // Cycle to the next tab
    currActiveTab.hide();
    currActiveButton.removeClass("tabActive");
    $(tabs[(currTabIdx + 1) % nTabs]).show();
    $(tabButtons[(currButtonIdx + 1) % nTabs]).addClass("tabActive");
} // cycleTabInTripComputerPopup

// If the "trip_computer_popup" is has no tab selected, select that of the current small screen
function initTripComputerPopup()
{
    // Retrieve all tab buttons and content elements
    var tabs = $("#trip_computer_popup").find(".tabContent");
    var tabButtons = $("#trip_computer_popup").find(".tabLeft");

    // Retrieve current tab button and content element
    var currActiveTab = $("#trip_computer_popup .tabContent:visible");
    var currActiveButton = $("#trip_computer_popup .tabLeft.tabActive");

    var currButtonIdx = tabButtons.index(currActiveButton);

    if (currButtonIdx >= 0) return;

    // No tab selected

    var selectedId =
            localStorage.smallScreen === "TRIP_INFO_1" ? "trip_computer_popup_trip_1" :
            localStorage.smallScreen === "TRIP_INFO_2" ? "trip_computer_popup_trip_2" :
            "trip_computer_popup_fuel_data"; // This tab is chosen if the small screen was showing GPS data
    $("#" + selectedId).show();

    var selectedButtonId = selectedId + "_button";
    $("#" + selectedButtonId).addClass("tabActive");
} // initTripComputerPopup

// Unselect any tab in the trip computer popup
function resetTripComputerPopup()
{
    $("#trip_computer_popup .tabContent").hide();
    $("#trip_computer_popup .tabLeft").removeClass("tabActive");
} // resetTripComputerPopup

// // Cycle through the small screens on the left hand side of the display
// function nextSmallScreen()
// {
    // // The IDs of the screens ("divs") that will be cycled through
    // var screenIds = [ "trip_info", "gps_info", "instrument_small", "tuner_small" ];

    // // Will become -1 if not found
    // var idIndex = screenIds.indexOf(currentSmallScreenId);

    // if (idIndex === screenIds.indexOf("trip_info") && $("#trip_1").is(":visible"))
    // {
        // // Change to the second trip counter
        // changeToTripCounter("trip_2");
    // }
    // else
    // {
        // // Cycle to the next screen in the sequence
        // idIndex = (idIndex + 1) % screenIds.length;

        // // Changing to the "trip_info" screen?
        // if (idIndex === screenIds.indexOf("trip_info")) changeToTripCounter("trip_1");

        // changeSmallScreenTo(screenIds[idIndex]);
    // } // if
// } // nextSmallScreen

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
    //document.documentElement.webkitRequestFullScreen();  // Might work
    document.body.requestFullscreen();  // Works, but not in IE11

    // Doesn't work
    //var wscript = new ActiveXObject("Wscript.shell");
    //wscript.SendKeys("{F11}");
} // goFullScreen

// -----
// Functions for popups

// Show the specified popup, with an optional timeout
function showPopup(id, msec)
{
    //console.log("// showPopup('" + id + "'," + msec + ")");

    var popup = $("#" + id);
    if (! popup.is(":visible"))
    {
        popup.show();

        // A popup can appear under another. In that case, don't register the time
        var topLevelPopup = $(".notificationPopup:visible").slice(-1)[0];
        if (id === topLevelPopup.id) lastScreenChangedAt = Date.now();

        // Perform "on_enter" action, if specified
        var onEnter = popup.attr("on_enter");
        if (onEnter) eval(onEnter);
    } // if

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

function showAudioPopup(id)
{
    if (typeof id === "undefined")
    {
        var audioSource = $("#audio_source").text();
        id =
            audioSource === "TUNER" ? "tuner_popup" :
            audioSource === "TAPE" ? "tape_popup" :
            audioSource === "CD" ? "cd_player_popup" :
            audioSource === "CD_CHANGER" ? "cd_changer_popup" :
            "";
    } // if

    if (! id) return;

    if (! $("#audio_popup").is(":visible")) hidePopup();  // Hide other popup, if showing
    showPopup("audio_popup", 7000);
    $("#" + id).siblings("div[id$=popup]").hide();
    $("#" + id).show();
} // showAudioPopup

// -----
// Functions for navigating through button sets and menus

var currentMenu;

function inMenu()
{
    var mainScreenIds =
    [
        "clock",
        "instruments", "pre_flight",
        "tuner", "cd_player", "cd_changer",
        "satnav_guidance", "satnav_curr_turn_icon"
    ];

    var notInMenu =
        currentMenu == null  // Not in any menu?
        || mainScreenIds.indexOf(currentLargeScreenId) >= 0;  // Or in one of the "main" screens (not a menu)?

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
    } // if

    // This is the screen we want to go back to when pressing "Esc" on the remote control inside the top level menu
    currentMenu = currentLargeScreenId;

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
    $("#" + id).parent().parent().find(".tickBox").each(function() { $(this).text("") } );

    $("#" + id).html("<b>&#10004;</b>");
} // setTick

function toggleTick(id)
{
    id = getFocusId(id);
    if (id === undefined) return;

    // In a group, only one tick box can be ticked at a time
    $("#" + id).parent().parent().find(".tickBox").each(function() { $(this).text("") } );

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

        // Select the next button
        do
        {
            nextIdx = (nextIdx + (key === buttonForNext ? 1 : nButtons - 1)) % nButtons;
            if (nextIdx === currIdx) break;

            // Skip disabled or invisible buttons

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
        // if (index > text.length) index = text.length;
        // if (index < 0) index = 0;
        index = clamp (index, 0, text.length);
        highlightIndexes[id] = index;
    } // if

    if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0;

    // Don't go before first letter or past last
    // if (highlightIndexes[id] < 0) highlightIndexes[id] = 0;
    // if (highlightIndexes[id] >= text.length) highlightIndexes[id] = text.length - 1;
    highlightIndexes[id] = clamp(highlightIndexes[id], 0, text.length - 1);

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
    highlightIndexes[id] = clamp(highlightIndexes[id], 0, lines.length - 1);

    // If present, remove the 'invertedText' class from the highlighed line
    lines[highlightIndexes[id]] = lines[highlightIndexes[id]].replace(/<[^>]*>/g, '');

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
    if (tooManyPixels > 0) $("#" + id).height($("#" + id).height() - tooManyPixels);  // Shrink a bit
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

    // Remove the 'invertedText' class from the highlighed line
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
var audioSettingHighlightIndex = 0;  // Current audio settings highlight box index

function highlightAudioSetting(goToFirst)
{
    // IE11 does not support default parameters
    if (goToFirst === undefined) goToFirst = false;

    $("#" + highlightIds[audioSettingHighlightIndex]).hide();  // Hide the current box

    // Either reset to the first ID, or get the ID of the next box
    audioSettingHighlightIndex = goToFirst ? 0 : audioSettingHighlightIndex + 1;

    // If going past the last setting, highlight nothing
    if (audioSettingHighlightIndex >= highlightIds.length)
    {
        audioSettingHighlightIndex = 0;
        hideAudioSettingsPopup();  // Also hide the audio settings popup
        return;
    } // if

    $("#" + highlightIds[audioSettingHighlightIndex]).show();  // Show the next highlight box
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
        // Note: on the original MFD, the tuner presets popup stays in the background, showing up again as soon
        // as the volume disapears. But that would become ugly here, because the tuner presets popup is not in
        // the same position as the audio settings popup.
        hideTunerPresetsPopup();

        // Show the audio settings popup
        $("#audio_settings_popup").show();

        // Highlight the first audio setting ("bass") if just popped up, else highlight next audio setting
        highlightAudioSetting(! isAudioMenuVisible);

        updatingAudioVolume = false;
        isAudioMenuVisible = true;
    } // if

    // (Re-)start the popup visibility timer.
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

var satnavInitialized = false;

// Current sat nav mode, saved as last reported in item "satnav_status_2"
var satnavMode = "INITIALIZING";

var satnavRouteComputed = false;
var satnavDisclaimerAccepted = false;
var satnavLastEnteredChar = null;
var satnavToMfdResponse;
var satnavStatus1 = "";
var satnavDestinationAccessible = true;

// Show this popup only once at start of guidance or after recalculation
var satnavDestinationNotAccessibleByRoadPopupShown = false;

var satnavCurrentStreet = "";
var satnavDirectoryEntry = "";

var satnavServices = JSON.parse(localStorage.satnavServices || "[]");
var satnavPersonalDirectoryEntries = JSON.parse(localStorage.satnavPersonalDirectoryEntries || "[]");
var satnavProfessionalDirectoryEntries = JSON.parse(localStorage.satnavProfessionalDirectoryEntries || "[]");

function satnavShowDisclaimer()
{
    if (satnavDisclaimerAccepted) return;

    currentMenu = "satnav_disclaimer";
    changeLargeScreenTo(currentMenu);
} // satnavShowDisclaimer

// When vehicle is moving, certain menus are disabled
function satnavVehicleMoving()
{
    var gpsSpeed = parseInt($("#satnav_gps_speed").text() || 0);
    return gpsSpeed >= 2;
} // satnavVehicleMoving

function satnavGotoMainMenu()
{
    // Show popup "Initializing navigator" as long as sat nav is not initialized
    if (! satnavInitialized)
    {
        showPopup("satnav_initializing_popup", 20000);
        return;
    } // if

    if (satnavStatus1.match(/DISC_UNREADABLE/))
    {
        showStatusPopup("Navigation CD-ROM<br />is unreadable", 10000);
        return;
    } // if

    if (satnavStatus1.match(/NO_DISC/) || $("#satnav_disc_recognized").hasClass("ledOff"))
    {
        showStatusPopup("Navigation CD-ROM is<br />missing", 8000);
        return;
    } // if

    // Change to the sat nav main menu with an exit to the general main manu
    menuStack = [ "main_menu" ];
    currentMenu = "satnav_main_menu";
    changeLargeScreenTo(currentMenu);

    // Select the top button
    selectFirstMenuItem(currentMenu);

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

    if (handleItemChange.mfdToSatnavRequestType === "REQ_ITEMS")
    {
        switch(handleItemChange.mfdToSatnavRequest)
        {
            // Pre-fill with what we previously received

            case "Service":
                $("#satnav_list").html(satnavServices.join('<br />'));
                break;

            case "Personal address list":
                $("#satnav_list").html(satnavPersonalDirectoryEntries.join('<br />'));
                break;

            case "Professional address list":
                $("#satnav_list").html(satnavProfessionalDirectoryEntries.join('<br />'));
                break;

            case "Enter city":
            case "Enter street":
                $("#satnav_list").empty();
                break;
        } // switch
    } // if

    // We could get here via "Esc" and then the currently selected line must be highlighted
    //unhighlightLine("satnav_list");
    highlightLine("satnav_list");
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

    // If a current destination city is known, enable the "Validate" button
    $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", $("#satnav_current_destination_city").text() === "");

    // Set button text
    $("#satnav_enter_characters_change_or_city_center_button").text("Change");

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
    // TODO - set full text ("City Center"), make button auto-expand when selected
    $("#satnav_enter_characters_change_or_city_center_button").text(
        currentMenu === "satnav_enter_street_characters" ? "City Ce" : "Change");

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

    // If a current destination street is known, enable the "Validate" button
    $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", $("#satnav_current_destination_street").text() === "");

    // Set button text
    // TODO - set full text ("City Center"), make button auto-expand when selected
    $("#satnav_enter_characters_change_or_city_center_button").text("City Ce");

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

function satnavEnterNewCityForService()
{
    $("#satnav_current_destination_city").text("");

    // Clear also the character-by-character entry string
    $("#satnav_entered_string").empty();

    satnavLastEnteredChar = null;

    satnavEnterNewCity();
} // satnavEnterNewCityForService

function satnavGotoEnterCity()
{
    // Hard-code the menu stack; we could get here from deep inside like the "Change" button in the
    // "satnav_show_current_destination" screen. Pressing "Esc" on the remote control must bring us back to the
    // "satnav_main_menu".
    var topLevel = menuStack[0];
    menuStack = [ "main_menu", "satnav_main_menu" ];
    if (topLevel !== "main_menu") menuStack.unshift(topLevel);  // Keep the top level if it was not "main_menu"
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

function satnavGetSelectedCharacter()
{
    if (currentMenu !== "satnav_enter_city_characters" && currentMenu !== "satnav_enter_street_characters") return;

    var selectedChar = $("#satnav_to_mfd_show_characters_line_1 .invertedText").text();
    if (! selectedChar) selectedChar = $("#satnav_to_mfd_show_characters_line_2 .invertedText").text();
    if (selectedChar) satnavLastEnteredChar = selectedChar;
} // satnavGetSelectedCharacter

function satnavEnterCharacter()
{
    clearInterval(handleItemChange.enterCharacterTimer);

    if (satnavLastEnteredChar === null) return;

    // The user is pressing "Val" on the remote control, to enter the chosen character.
    // Or the MFD itself is confirming the character, if there is only one character to choose from.

    $("#satnav_entered_string").append(satnavLastEnteredChar);  // Append the entered character
    $("#satnav_enter_characters_correction_button").removeClass("buttonDisabled");  // Enable the "Correction" button
    $("#satnav_enter_characters_validate_button").addClass("buttonDisabled");  // Disable the "Validate" button
    highlightFirstLine("satnav_list");  // Go to the first line in the "satnav_list" screen
} // satnavEnterCharacter

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

    // If the entered string has become empty, disable the "Correction" button
    $("#satnav_enter_characters_correction_button").toggleClass("buttonDisabled", currentText.length === 0);

    // Jump back to the characters
    if (currentText.length === 0) satnavSelectFirstAvailableCharacter();
} // satnavRemoveEnteredCharacter

// State while entering sat nav destination in character-by-character mode
// 0 = "Val" button pressed
// 1 = "satnav_to_mfd_show_characters"
// 2 = "mfd_to_satnav_enter_character"
var satnavEnterOrDeleteCharacterExpectedState = 2;

function satnavEnterOrDeleteCharacter(newState)
{
    if (newState <= satnavEnterOrDeleteCharacterExpectedState)
    {
        if (satnavRollingBackEntryByCharacter)
        {
            // In "character roll-back" mode, remove the last entered character
            satnavRemoveEnteredCharacter();
        }
        else
        {
            satnavEnterCharacter();
        } // if
    } // if

    satnavEnterOrDeleteCharacterExpectedState = newState;
} // satnavEnterOrDeleteCharacter

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
    //$("#satnav_entered_string").text("");
    $("#satnav_entered_string").empty();

    // When choosing a city from the list, hide the current destination street, so that the entry format ("") will
    // be shown.
    //if ($("#mfd_to_satnav_request").text() === "Enter city") $("#satnav_current_destination_street").text("");
    if ($("#mfd_to_satnav_request").text() === "Enter city") $("#satnav_current_destination_street").empty();

    // When choosing a city or street from the list, hide the current destination house number, so that the
    // entry format ("_ _ _") will be shown.
    //$("#satnav_current_destination_house_number").text("");
    $("#satnav_current_destination_house_number").empty();

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
    s = s.replace(/_ /, "");  // Strip off leading "_ ", if present
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

        // Selecting "No number" is always ok
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

    if (! ok)
    {
        satnavHouseNumberEntryMode();
        showStatusPopup("This number is<br />not listed for<br />this street", 7000);
        return;
    } // if

    // Already copy the house number into the "satnav_show_current_destination" screen, in case the
    // "satnav_report" packet is missed
    $("#satnav_current_destination_house_number_shown").text(enteredNumber === "" ? "No number" : enteredNumber);

    gotoMenu("satnav_show_current_destination");
} // satnavCheckEnteredHouseNumber

function satnavClearLastDestination()
{
    $('[gid="satnav_last_destination_city"]').empty();
    $('[gid="satnav_last_destination_street_shown"]').empty();
    $('[gid="satnav_last_destination_house_number_shown"]').empty();
} // satnavClearLastDestination

// Enable/disable the buttons in the "satnav_show_service_address" screen. Also select the appropriate button.
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
    if (currentMenu === "satnav_guidance")
    {
        // Return to the guidance screen (bit clumsy)
        exitMenu();
        showDestinationNotAccessiblePopupIfApplicable();
    }
    else
    {
        satnavSwitchToGuidanceScreen();
    } // if
} // satnavGuidancePreferenceValidate

function satnavCalculatingRoute()
{
    localStorage.askForGuidanceContinuation = "NO";

    // No popups while driving
    if (satnavVehicleMoving()) return;

    // If the result of the calculation is "Destination is not accessible by road", show that popup once, at the
    // start, but not any more during the guidance.
    satnavDestinationNotAccessibleByRoadPopupShown = false;

    // Don't pop up in the guidance preference screen
    if (currentLargeScreenId === "satnav_guidance_preference_menu") return;

    showPopup("satnav_computing_route_popup", 30000);
} // satnavCalculatingRoute

// Show the "Destination is not accessible by road" popup, if applicable
function showDestinationNotAccessiblePopupIfApplicable()
{
    if (satnavDestinationAccessible) return false;

    // Don't show this popup while still in the guidance preference menu
    if ($("#satnav_guidance_preference_menu").is(":visible")) return false;

    // Show this popup only once at start of guidance or after recalculation
    if (satnavDestinationNotAccessibleByRoadPopupShown) return true;

    // But only if the curent location is known (to emulate behaviour of MFD)
    if (satnavCurrentStreet !== "")
    {
        hidePopup();
        showStatusPopup("Destination is not<br />accessible by road", 8000);
    } // if

    satnavDestinationNotAccessibleByRoadPopupShown = true;

    return true;
} // showDestinationNotAccessiblePopupIfApplicable

function showOrTimeoutDestinationNotAccessiblePopup()
{
    if (showDestinationNotAccessiblePopupIfApplicable()) return;

    // The popup may be displayed during the next 10 seconds, not any more after that
    clearInterval(showOrTimeoutDestinationNotAccessiblePopup.timer);
    showOrTimeoutDestinationNotAccessiblePopup.timer = setTimeout(
        function () { satnavDestinationNotAccessibleByRoadPopupShown = true; },
        10000
    );
} // showOrTimeoutDestinationNotAccessiblePopup 

function satnavSwitchToGuidanceScreen()
{
    //console.log("// satnavSwitchToGuidanceScreen()");

    menuStack = [];
    currentMenu = "satnav_guidance";
    changeLargeScreenTo(currentMenu);

    hidePopup("satnav_computing_route_popup");
} // satnavSwitchToGuidanceScreen

// Format a string like "45 km" or "7000 m" or "60 m". Return an array [distance, unit]
function satnavFormatDistance(distanceStr)
{
    // We want compatibility with IE11 so cannot assign result array directly to variables
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
        exitMenu();
        exitMenu();
    }
    else
    {
        // In main menu

        // When the "Validate" button is pressed, all menus are exited up to top level.
        // IMHO that is a bug in the original MFD.
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
        satnavSwitchToGuidanceScreen();
    }
    else
    {
        // In main menu
        currentMenu = menuStack.pop();
        changeLargeScreenTo(currentMenu);
    } // if
} // satnavEscapeVocalSynthesisLevel

function satnavPoweringOff()
{
    satnavInitialized = false;
    $("#satnav_disc_recognized").addClass("ledOff");
    handleItemChange.nSatNavDiscUnreadable = 1;
    satnavDisclaimerAccepted = false;
    satnavDestinationNotAccessibleByRoadPopupShown = false;
} // function

// -----
// Handling of 'system' screen

function gearIconAreaClicked()
{
    if ($("#clock").is(":visible"))
    {
        changeLargeScreenTo("system");
        showViewportSizes();
    }
    else
    {
        changeLargeScreenTo("clock");
    } // if
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
                $('[gid="cd_status_searching"]').show();
                $('[gid="cd_status_pause"]').hide();
                $('[gid="cd_status_play"]').hide();
                $('[gid="cd_status_fast_forward"]').hide();
                $('[gid="cd_status_rewind"]').hide();
            }

            // If "audio_source" changed to "CD_CHANGER", directly show the "searching CD" icon
            else if (audioSource !== "CD_CHANGER" && value === "CD_CHANGER")
            {
                // Cartridge is not present?
                if (! $("#cd_changer_cartridge_present").hasClass("ledOn"))
                {
                    // Show "No Cartridge" (original MFD says: "No Mag" (magazine)) popup for a few seconds
                    showStatusPopup("No Cartridge", 3000);
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
                $('[gid="cd_changer_status_pause"]').hide();
                $('[gid="cd_changer_status_play"]').hide();
                $('[gid="cd_changer_status_loading"]').show();
                $('[gid="cd_changer_status_fast_forward"]').hide();
                $('[gid="cd_changer_status_rewind"]').hide();
            } // if

            if (! value.match(/^[1-6]/)) hideTunerPresetsPopup();

            // Change screen when audio source button is pressed
            // Note: ignore values like "CD (released)", "TUNER (held)" and whatever other variants
            if (value === "TUNER" || value === "TAPE" || value === "CD" || value === "CD_CHANGER")
            {
                // No screen change while browsing the menus
                if (inMenu()) break;

                selectDefaultScreen(value);

                break;
            } // if

            // Check for button press of "1" ... "6" (set tuner preset, or select disc on CD changer).
            // Ignore variants like "1 (released)" or "5 (held)".
            if (! value.match(/^[1-6]$/)) break;

            if (audioSource === "CD_CHANGER")
            {
                if (! $("#cd_changer").is(":visible")) showAudioPopup("cd_changer_popup");

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

        case "tape_side":
        {
            // Has anything changed?
            if (value === handleItemChange.tapeSide) break;
            handleItemChange.tapeSide = value;

            hideAudioSettingsPopup();

            if ($("#audio_source").text() === "TAPE" && ! $("#tape").is(":visible")) showAudioPopup("tape_popup");
        } // case
        break;

        case "tape_status":
        {
            // Has anything changed?
            if (value === handleItemChange.tapeStatus) break;
            handleItemChange.tapeStatus = value;

            hideAudioSettingsPopup();

            if ($("#audio_source").text() === "TAPE" && ! $("#tape").is(":visible")) showAudioPopup("tape_popup");
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

            if ($("#audio_source").text() === "TUNER" && ! $("#tuner").is(":visible")) showAudioPopup("tuner_popup");
        } // case
        break;

        case "fm_band":
        {
            // FM tuner data is visible only if FM band is selected
            $('[gid="fm_tuner_data"]').toggle(value === "ON");
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
            $('[gid="frequency_data_large"]').toggle(value === "ON");
            $('[gid="frequency_data_small"]').toggle(value !== "ON");

            $('[gid="ta_selected"]').toggle(value !== "ON");
            $('[gid="ta_not_available"]').toggle(value !== "ON");
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

            // Un-highlight any previous entry in the "tuner_presets_popup"
            $('div[id^=presets_memory_][id$=_select]').hide()

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
            $('[gid="frequency_data_small"]').toggle(showRdsText);
            $('[gid="rds_text"]').toggle(showRdsText);

            // If no RDS text, show tuner frequency large
            $('[gid="frequency_data_large"]').toggle(! showRdsText);
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

            if ($("#audio_source").text() === "TUNER" && ! $("#tuner").is(":visible")) showAudioPopup("tuner_popup");

            var rdsText = $("#rds_text").text();
            var showRdsText = $("#fm_band").hasClass("ledOn") && rdsText !== "" && value !== "MANUAL_TUNING";

            // If there is RDS text, and no manual frequency search is busy, show tuner frequency small,
            // and RDS text large
            $('[gid="frequency_data_small"]').toggle(showRdsText);
            $('[gid="rds_text"]').toggle(showRdsText);

            // If there is no RDS text, or a manual frequency search is busy, show tuner frequency large
            // (on the place where normally the RDS text is shown)
            $('[gid="frequency_data_large"]').toggle(! showRdsText);
        } // case
        break;

        case "pty_standby_mode":
        {
            // Show this LED only when in "PTY standby mode"
            $("#pty_standby_mode").toggle(value === "YES");
        } // case
        break;

        case "pty_selection_menu":
        {
            // Show this LED only when in "PTY selection mode"
            $("#pty_selection_menu").toggle(value === "ON");

            // Show the selected PTY instead of that of the current station
            $("#pty_16").toggle(value !== "ON");
            $("#selected_pty_16").toggle(value === "ON");
        } // case
        break;

        case "info_traffic":
        {
            if (typeof handleItemChange.infoTraffic === "undefined") handleItemChange.infoTraffic = "NO";

            // Don't pop up when browsing the menus, or in the sat nav guidance screen
            if (inMenu() || $("#satnav_guidance").is(":visible")) break;

            // Has anything changed?
            if (value === handleItemChange.infoTraffic) break;
            handleItemChange.infoTraffic = value;

            if (value === "YES") showNotificationPopup("Info Traffic!", 120000);  // At most 2 minutes
            else hidePopup("notification_popup");
        } // case
        break;

        case "cd_status":
        {
            // Has anything changed?
            if (value === handleItemChange.cdPlayerStatus) break;
            handleItemChange.cdPlayerStatus = value;

            $("#cd_status_error").toggle(value === "ERROR");

            if (value === "ERROR")
            {
                showNotificationPopup("Disc unreadable<br />(upside down?)", 6000);
            }
            else if
                (
                    value === "PAUSE" || value === "PLAY" || value === "PLAY-SEARCHING"
                    || value ===  "FAST_FORWARD" || value === "REWIND"
                )
            {
                if ($("#audio_source").text() === "CD" && ! $("#cd_player").is(":visible")) showAudioPopup();
            } // if
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
            // Has anything changed?
            if (value === handleItemChange.cdChangerStatus) break;
            handleItemChange.cdChangerStatus = value;

            $("#cd_changer_status_error").toggle(value === "ERROR");

            if (value === "ERROR")
            {
                showNotificationPopup("Disc unreadable<br />(wrong side?)", 6000);
            }
            else if
                (
                    value === "LOADING"
                    || value === "PAUSE" || value === "PLAY" || value === "PLAY-SEARCHING"
                    || value ===  "FAST_FORWARD" || value === "REWIND"
                )
            {
                if ($("#audio_source").text() === "CD_CHANGER" && ! $("#cd_changer").is(":visible")) showAudioPopup();
            } // if
        } // case
        break;

        case "cd_changer_current_disc":
        {
            // Has anything changed?
            if (value === cdChangerCurrentDisc) break;

            //console.log("Item '" + item + "' set to '" + value + "'");

            if ($("#audio_source").text() === "CD_CHANGER" && ! $("#cd_changer").is(":visible")) showAudioPopup();

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
            if (inMenu()) break;

            // Suppress timer still running?
            if (suppressScreenChangeToAudio != null) break;

            // Hide the audio settings or tuner presets popup (if visible)
            hideAudioSettingsPopup();
            hideTunerPresetsPopup();

            // Suppress the audio settings popup for the next 0.4 seconds (bit clumsy, but effective)
            clearInterval(handleItemChange.hideHeadUnitPopupsTimer);
            handleItemChange.hideHeadUnitPopupsTimer = setTimeout(
                function () { handleItemChange.hideHeadUnitPopupsTimer = null; },
                400
            );

            if (value === "CD")
            {
                // Hide all icons except "searching" or "loading"
                $('[gid="cd_status_pause"]').hide();
                $('[gid="cd_status_play"]').hide();
                $('[gid="cd_status_fast_forward"]').hide();
                $('[gid="cd_status_rewind"]').hide();
                $('[gid="cd_status_eject"]').hide();
            }
            else if (value === "CD_CHANGER")
            {
                // Show the "loading" icon
                $("#cd_changer .iconLarge").hide();
                $("#cd_changer_popup .iconLarge").hide();
                $('[gid="cd_changer_status_loading"]').show();
            } // if

            selectDefaultScreen(value);
        } // case
        break;

        case "mute":
        {
            // Has anything changed?
            if (value === handleItemChange.headUnitMute) break;
            handleItemChange.headUnitMute = value;

            if (value !== "ON") break;

            if (
                  $("#audio_source").text() !== "NONE"
                  && $("#audio_source").text() !== "NAVIGATION"
                  && ! $("#audio").is(":visible")
                )
            {
                showAudioPopup();
            } // if

            if (handleItemChange.currentAudioSource === "CD_CHANGER")
            {
                // Show the "pause" icon
                $("#cd_changer .iconLarge").hide();
                $("#cd_changer_popup .iconLarge").hide();
                $('[gid="cd_changer_status_pause"]').show();
            } // if
        } // case
        break;

        case "engine_rpm":
        {
            //if (value === "---" && $('[gid="vehicle_speed"]').first().text() === "--")
            if (value === "---")
            {
                // Engine just stopped. If currently in "instruments" screen, then switch to default screen.
                if ($("#instruments").is(":visible")) selectDefaultScreen();
                break;
            } // if

            // If more than 3500 rpm or less than 500 rpm (but > 0), add glow effect

            // Threshold of 3500 rpm is lowered with engine coolant temperature, but always at least 1700 rpm
            var thresholdRpm = 3500;
            if (engineCoolantTemperature < 80) thresholdRpm = 3500 - (80 - engineCoolantTemperature) * 30;
            if (thresholdRpm < 1700) thresholdRpm = 1700;

            $('[gid="' + item + '"]').toggleClass("glow",
                (parseInt(value) < 500 && parseInt(value) > 0) || parseInt(value) > thresholdRpm);
        } // case
        break;

        case "vehicle_speed":
        {
            if (value === "---") break;

            // If 130 km/h or more, add glow effect
            $('[gid="' + item + '"]').toggleClass("glow", parseInt(value) >= 130);
        } // case
        break;

        case "fuel_level_filtered":
        {
            // If less than 5 litres left, add glow effect
            $('[gid="' + item + '"]').toggleClass("glow", parseInt(value) < 5);
        } // case
        break;

        case "water_temp":
        {
            engineCoolantTemperature = parseFloat(value);

            // If more than 110 degrees Celsius, add glow effect
            $('[gid="' + item + '"]').toggleClass("glow", engineCoolantTemperature > 110);

            // If less than 70 degrees, add "ice glow" effect
            $('[gid="' + item + '"]').toggleClass("glowIce", engineCoolantTemperature < 70);
        } // case
        break;

        case "exterior_temperature":
        {
            $("#splash_text").hide();

            // If between -3.0 and +3.0 degrees Celsius, add "ice glow" effect
            var temp10 = parseFloat(value) * 10;
            $('[gid="' + item + '"]').toggleClass("glowIce", temp10 >= -30 && temp10 <= 30);
        } // case
        break;

        case "oil_level_raw":
        {
            // If very low, add glow effect
            $("#" + item).toggleClass("glow", parseInt(value) <= 11);
        } // case
        break;

        case "remaining_km_to_service":
        {
            // Add a space between hundreds and thousands for better readability
            $("#" + item).text(numberWithSpaces(value));

            // If zero or negative, add glow effect
            $("#" + item).toggleClass("glow", parseInt(value) <= 0);
        } // case
        break;

        case "odometer_1":
        {
            // Add a space between hundreds and thousands for better readability
            $("#" + item).text(numberWithSpaces(value));
        } // case
        break;

        case "economy_mode":
        {
            // Check if anything actually changed
            if (value === handleItemChange.economyMode) break;
            handleItemChange.economyMode = value;

            if (value !== "ON") break;

            if (currentLargeScreenId !== "pre_flight") showNotificationPopup("Changing to<br />power-save mode", 15000);
        } // case
        break;

        case "warning_led":
        {
            // If on, add glow effect
            $("#" + item).removeClass("ledOn");
            $("#" + item).removeClass("ledOff");
            $("#" + item).toggleClass("glow", value === "ON");
        } // case
        break;

        case "door_open":
        {
            // Note: If the system is in power save mode, "door_open" always reports "NO", even if a door is open.
            // That situation is handled below (around line 3329).

            // If on, add glow effect
            $("#" + item).removeClass("ledOn");
            $("#" + item).removeClass("ledOff");
            $("#" + item).toggleClass("glow", value === "YES");

            // Show or hide the "door open" popup, but not in the "pre_flight" screen
            if (value === "YES" && currentLargeScreenId !== "pre_flight")
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

            // If on, add glow effect to "door_open" icon in the "pre_flight" screen
            $("#door_open").toggleClass("glow", isBootOpen || nDoorsOpen > 0);

            // All closed, or in the "pre_flight" screen?
            if ((! isBootOpen && nDoorsOpen == 0) || currentLargeScreenId === "pre_flight")
            {
                hidePopup("door_open_popup");
                break;
            } // if

            // Compose gold-plated popup text

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

            // If engine just stopped, and currently in "instruments" screen, then switch to default screen
            if (engineRunning === "NO" && $("#instruments").is(":visible")) selectDefaultScreen();
        } // case
        break;

        case "contact_key_position":
        {
            // Has anything changed?
            if (value === handleItemChange.contactKeyPosition) break;
            handleItemChange.contactKeyPosition = value;

            // On engine START, add glow effect
            $("#" + item).toggleClass("glow", value === "START");

            // Show "Pre-flight checks" screen if contact key is in "ON" position, without engine running
            if (value === "ON" && ! inMenu())
            {
                if (engineRunning === "NO")
                {
                    clearInterval(handleItemChange.contactKeyOffTimer);
                    changeLargeScreenTo("pre_flight");
                    hidePopup();
                } // if

                if (satnavInitialized && localStorage.askForGuidanceContinuation === "YES")
                {
                    // Show popup "Continue guidance to destination? [Yes]/No"
                    showPopup("satnav_continue_guidance_popup", 15000);

                    localStorage.askForGuidanceContinuation = "NO";
                } // if
            }
            else if (value === "ACC")
            {
                clearInterval(handleItemChange.contactKeyOffTimer);
            }
            else if (value === "OFF")
            {
                // "OFF" position can be very short between "ACC" and "ON", so first wait a bit
                clearInterval(handleItemChange.contactKeyOffTimer);
                handleItemChange.contactKeyOffTimer = setTimeout(
                    function ()
                    {
                        // If in guidance mode while turning off contact key, remember to show popup
                        // "Continue guidance to destination?" the next time the contact key is turned "ON"
                        if (satnavMode === "IN_GUIDANCE_MODE") localStorage.askForGuidanceContinuation = "YES";

                        satnavPoweringOff();

                        // Show the audio screen (or fallback to current location or clock)
                        selectDefaultScreen();
                        handleItemChange.contactKeyOffTimer = null;
                    },
                    500
                );
            } // if
        } // case
        break;

        case "notification_message_on_mfd":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            // Only if not empty
            if (value === "") break;

            var isWarning = value.slice(-1) === "!";
            var message = isWarning ? value.slice(0, -1) : value;
            showNotificationPopup(message, 10000, isWarning);
        } // case
        break;

        case "satnav_status_1":
        {
            satnavStatus1 = value;

            // No further handling when in power-save mode
            if (handleItemChange.economyMode === "ON") break;

            if (satnavStatus1.match(/AUDIO/))
            {
                // Turn on or off "satnav_audio" LED
                var playingAudio = satnavStatus1.match(/START/) !== null;
                $("#satnav_audio").toggleClass("ledOn", playingAudio);
                $("#satnav_audio").toggleClass("ledOff", ! playingAudio);

                clearInterval(handleItemChange.showSatnavAudioLed);
                if (playingAudio)
                {
                    // Set timeout on LED, in case the "AUDIO OFF" packet is missed (yes, that does happen)
                    handleItemChange.showSatnavAudioLed = setTimeout
                    (
                        function ()
                        {
                            $("#satnav_audio").removeClass("ledOn");
                            $("#satnav_audio").addClass("ledOff");
                        },
                        5000
                    );
                } // if
            } // if

            // Show one or the other
            satnavDestinationAccessible = satnavStatus1.match(/DESTINATION_NOT_ON_MAP/) === null;
            $("#satnav_distance_to_dest_via_road_visible").toggle(satnavDestinationAccessible);
            $("#satnav_distance_to_dest_via_straight_line_visible").toggle(! satnavDestinationAccessible);

            if (! satnavDestinationAccessible)
            {
                // Note: this popup must be shown just after the "Computing route in progress" popup closes, not
                // during navigation
                showDestinationNotAccessiblePopupIfApplicable();
            }
            else if (satnavStatus1.match(/NO_DISC/))
            {
                showStatusPopup("Navigation CD-ROM is<br />missing", 5000);
            } // if
            else if (satnavStatus1.match(/DISC_UNREADABLE/))
            {
                // TODO - show popup not directly, but only after a few times this status
                showStatusPopup("Navigation CD-ROM<br />is unreadable", 5000);
            } // if
            else if (satnavStatus1.match(/ARRIVED_AT_DESTINATION/))
            {
                showPopup("satnav_reached_destination_popup", 10000);
            } // if
        } // case
        break;

        case "satnav_status_2":
        {
            // Only if valid
            if (value === "") break;

            // Has anything changed?
            if (value === satnavMode) break;
            var previousSatnavMode = satnavMode;
            satnavMode = value;

            // Button texts in menus
            $("#satnav_navigation_options_menu_stop_guidance_button").text(
                value === "IN_GUIDANCE_MODE" ? "Stop guidance" : "Resume guidance");
            $("#satnav_tools_menu_stop_guidance_button").text(
                value === "IN_GUIDANCE_MODE" ? "Stop guidance" : "Resume guidance");

            // Just entered guidance mode?
            if (value === "IN_GUIDANCE_MODE")
            {
                // Reset the trip computer popup contents
                resetTripComputerPopup();

                if ($("#satnav_guidance_preference_menu").is(":visible")) break;
                satnavSwitchToGuidanceScreen();

                // If the small screen (left) is currently "gps_info", then go to the next screen ("instrument_small").
                // When the system is in guidance mode, the right stalk button does not cycle through "gps_info".
                //if (currentSmallScreenId === "gps_info") changeSmallScreenTo("instrument_small");

                break;
            }

            // Just left guidance mode?
            if (previousSatnavMode === "IN_GUIDANCE_MODE")
            {
                if ($("#satnav_guidance").is(":visible")) selectDefaultScreen();
            } // if

            if (value === "IDLE")
            {
                satnavRouteComputed = false;
            } // if

            // if (value === "INITIALIZING")
            // {
                // if (currentLargeScreenId === "satnav_main_menu" || currentLargeScreenId === "satnav_disclaimer")
                // {
                    // // In these screens, show the "Initializing navigator" popup as long as the mode is "INITIALIZING"
                    // showPopup("satnav_initializing_popup", 20000);
                // } // if
            // }
            // else
            // {
                // // Hide any popup "Navigation system being initialized" if no longer "INITIALIZING"
                // hidePopup("satnav_initializing_popup");
            // } // if
        } // case
        break;

        case "satnav_status_3":
        {
            if (value === "STOPPING_NAVIGATION")
            {
                satnavDestinationNotAccessibleByRoadPopupShown = false;
            }
            else if (value === "POWERING_OFF")
            {
                satnavPoweringOff();
            }
            else if (value === "CALCULATING_ROUTE")
            {
                satnavCalculatingRoute();
            } // if
        } // case
        break;

        case "satnav_gps_speed":
        {
            var driving = satnavVehicleMoving();

            // While driving, disable "Navigation / Guidance" button in main menu
            $("#main_menu_goto_satnav_button").toggleClass("buttonDisabled", driving);
            if (driving)
            {
                $("#main_menu_goto_satnav_button").removeClass("buttonSelected");
                $("#main_menu_goto_screen_configuration_button").addClass("buttonSelected");
            } // if
        } // case
        break;

        case "satnav_guidance_status":
        {
            if (value.match(/CALCULATING_ROUTE/) && value.match(/DISC_PRESENT/))
            {
                satnavCalculatingRoute();
            }
            else
            {
                hidePopup("satnav_computing_route_popup");
            } // if
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

            // No further handling when in power-save mode
            if (handleItemChange.economyMode === "ON") break;

            if (value === "YES") handleItemChange.nSatNavDiscUnreadable = 0;
            else handleItemChange.nSatNavDiscUnreadable++;

            // At the 6-th and 14-th time of consequently reporting "NO", shortly show a status popup
            if (handleItemChange.nSatNavDiscUnreadable == 6 || handleItemChange.nSatNavDiscUnreadable == 14)
            {
                showStatusPopup("Navigation CD-ROM<br />is unreadable", 4000);
            } // if
        } // case
        break;

        case "satnav_equipment_present":
        {
            // Enable or disable the "Navigation/Guidance" button in the main menu
            $("#main_menu_goto_satnav_button").toggleClass("buttonDisabled", value === "NO");

            if (value === "NO")
            {
                // Select the 'Configure display' (bottom) button
                $("#main_menu_goto_satnav_button").removeClass("buttonSelected");
                $("#main_menu_goto_screen_configuration_button").addClass("buttonSelected");
            } // if
        } // case
        break;

        case "satnav_current_destination_city":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            if (! $("#satnav_enter_city_characters").is(":visible")) break;

            $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");
        } // case
        break;

        case "satnav_current_destination_street":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            $("#satnav_current_destination_street_shown").text(value === "" ? "City centre" : value);

            if (! $("#satnav_enter_street_characters").is(":visible")) break;

            $("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");
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
            satnavCurrentStreet = value;

            if (satnavMode === "IN_GUIDANCE_MODE")
            {
                // In the guidance screen, show "Street (City)", otherwise "Street not listed"
                $("#satnav_guidance_curr_street").text(value.match(/\(.*\)/) ? value : "Street not listed");
            } // if

            // In the current location screen, show "City", or "Street (City)", otherwise "Street not listed"
            // TODO - do we ever see "Not digitized area" in the current location screen?
            $('[gid="satnav_curr_street_shown"]').text(value !== "" ? value : "Street not listed");

            // Only if the clock is currently showing, i.e. don't move away from the Tuner or CD player screen
            if (! $("#clock").is(":visible")) break;

            // Show the current location
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
            satnavEnterOrDeleteCharacterExpectedState = 2;

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

            // Has anything changed?
            if (value === currentLargeScreenId) break;

            if (value === "satnav_enter_city_characters")
            {
                // Switching (back) into the "satnav_enter_city_characters" screen

                if (currentMenu === "satnav_current_destination_house_number") exitMenu();
                else if (currentMenu === "satnav_enter_street_characters") exitMenu();
                else if (currentMenu === "satnav_show_last_destination") satnavEnterNewCityForService();
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
                satnavClearEnteredNumber();  // Clear the entered house number
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
            if (value === handleItemChange.mfdToSatnavRequest) break;
            handleItemChange.mfdToSatnavRequest = value;
        } // case
        break;

        case "mfd_to_satnav_request_type":
        {
            handleItemChange.mfdToSatnavRequestType = value;

            if (value !== "REQ_N_ITEMS") break;

            switch (handleItemChange.mfdToSatnavRequest)
            {
                case "Enter city":
                case "Enter street":
                {
                    $("#satnav_to_mfd_show_characters_line_1").text("");
                    $("#satnav_to_mfd_show_characters_line_2").text("");

                    // Show the spinning disc after a second or so
                    clearInterval(handleItemChange.showCharactersSpinningDiscTimer);
                    handleItemChange.showCharactersSpinningDiscTimer = setTimeout
                    (
                        function () { $("#satnav_to_mfd_show_characters_spinning_disc").show(); },
                        1500
                    );
                } // case
                break;

                default:
                break;
            } // switch
        } // case
        break;

        case "mfd_to_satnav_selection":
        {
            if (handleItemChange.mfdToSatnavRequest === "Enter street"
                && handleItemChange.mfdToSatnavRequestType === "SELECT")
            {
                // Already copy the selected street into the "satnav_show_current_destination" screen, in case the
                // "satnav_report" packet is missed

                var lines = splitIntoLines("satnav_list");

                if (lines[value] === undefined) break;

                var selectedLine = lines[value];
                var selectedStreetAndCity = selectedLine.replace(/<[^>]*>/g, '');  // Remove HTML formatting
                var selectedStreet = selectedStreetAndCity.replace(/ -.*-/g, '');  // Keep only the street; discard city

                $("#satnav_current_destination_street_shown").text(selectedStreet);
            } // if
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

            if (satnavToMfdResponse === "Service")
            {
                // Sat nav menu item "Select a service": enable the button if sat nav responds with a
                // list size value > 0
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
            if (title === "Service")
            {
                satnavServices = value;

                // Save in local (persistent) store
                localStorage.satnavServices = JSON.stringify(satnavServices);
            }
            else if (title === "Personal address list")
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
            if (! $("#satnav_show_service_address").is(":visible")) break;
            satnavServiceAddressSetButtons(parseInt(value) + 1);
        } // case
        break;

        case "satnav_guidance_preference":
        {
            if (value === "---") break;
            satnavGuidanceSetPreference(value);
            localStorage.satnavGuidancePreference = value;  // Save also in local (persistent) store
        } // case
        break;

        case "satnav_initialized":
        case "satnav_system_id":
        {
            // console.log("Item '" + item + "' set to '" + value + "'");

            if (item === "satnav_initialized" && value !== "YES") break;

            // As soon as any value is received for "satnav_system_id", the sat nav is considered "initialized"
            satnavInitialized = true;

            // Hide any popup "Navigation system being initialized" (if shown)
            hidePopup("satnav_initializing_popup");

            // Show "Continue guidance? (yes/no)" popup if applicable
            if (handleItemChange.contactKeyPosition === "ON" && localStorage.askForGuidanceContinuation === "YES")
            {
                // Show popup "Continue guidance to destination? [Yes]/No"
                showPopup("satnav_continue_guidance_popup", 15000);

                localStorage.askForGuidanceContinuation = "NO";
            } // if
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
                describeArc(150, 150, 30, Math.max(value, 0.1) - 180, 180)
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
                describeArc(50, 150, 30, Math.max(value, 0.1) - 180, 180)
            );
        } // case
        break;

        case "mfd_to_satnav_instruction":
        {
            if (value === "Selected_city_from_list")
            {
                // Clear the character-by-character entry string
                //$("#satnav_entered_string").text("");
                $("#satnav_entered_string").empty();
                satnavLastEnteredChar = null;
            } //if

            else if (value === "Val")
            {
                satnavEnterOrDeleteCharacter(0);
            } // if

            handleItemChange.mfdToSatnavInstruction = value;
        } // case
        break;

        case "satnav_to_mfd_show_characters":
        {
            //console.log("Item '" + item + "' set to '" + value + "'");

            satnavEnterOrDeleteCharacter(1);

            if (value.length > 1)
            {
                satnavRollingBackEntryByCharacter = false;
            } // if

            if (value.length == 1)
            {
                // Save for later: this character can be automatically selected by the MFD without the user explicitly
                // pressing the "Val" button on the remote control. See code at 'case "mfd_to_satnav_instruction"'.
                satnavLastEnteredChar = value;
            } // if

            if (value === "") break;

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
            if (value === "ON") hidePopup("satnav_computing_route_popup");
        } // case
        break;

        case "satnav_not_on_map_icon":
        {
            if (value === "ON")
            {
                // Hide the "Computing route in progress" popup, if showing
                hidePopup("satnav_computing_route_popup");

                //satnavCurrentStreet = "";

                $("#satnav_guidance_next_street").text("Follow the heading");

                // To replicate a bug in the original MFD; in fact the current street is usually known
                $("#satnav_guidance_curr_street").text("Not digitized area");

                //$('[gid="satnav_curr_street_shown"]').text("Not digitized area");
            } // if

            $("#satnav_turn_at_indication").toggle(value !== "ON");
        } // case
        break;

        case "satnav_curr_heading_compass_needle_rotation":
        {
            // Place the "N" (North) just in front of the arrow point in the compass
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
            // Place the "D" (Destination) just in front of the arrow point
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

        case "right_stalk_button":
        {
            if (value !== "PRESSED") break;
            if (satnavMode !== "IN_GUIDANCE_MODE") break;

            var isTripComputerPopupAlreadyVisible = $("#trip_computer_popup").is(":visible");
            if (isTripComputerPopupAlreadyVisible) cycleTabInTripComputerPopup();

            // Will either start showing the popup, or restart its timer if it is already showing
            showPopup("trip_computer_popup", 8000);

            if (! isTripComputerPopupAlreadyVisible)
            {
                // If the original MFD is showing the trip computer in the large (right hand side) screen, the first
                // stalk button press cycles to the next trip counter tab, instead of triggering the popup.
                if (mfdLargeScreen === "TRIP_COMPUTER") cycleTabInTripComputerPopup();
            } // if
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

        case "large_screen":
        {
            mfdLargeScreen = value;
        } // case
        break;

        case "mfd_remote_control":
        {
            // Ignore remote control buttons if contact key is off
            var contactKeyPosition = $("#contact_key_position").text();
            if (contactKeyPosition === "OFF") break;

            // Ignore remote control buttons when in power-save mode
            if (handleItemChange.economyMode === "ON") break;

            var parts = value.split(" ");
            var button = parts[0];

            //console.log("// Item '" + item + "' set to '" + button + "'");

            if (button === "MENU_BUTTON")
            {
                // Directly hide any visible audio popup
                hideAudioSettingsPopup();
                hideTunerPresetsPopup();

                gotoTopLevelMenu();
            }
            else if (button === "ESC_BUTTON")
            {
                // Ignore if just changed screen; sometimes the screen change is triggered by the system, not the
                // remote control. If the user wanted pressed a button on the previous screen, it will end up
                // being handled by the current screen, which is confusing.
                if (Date.now() - lastScreenChangedAt < 250)
                {
                    console.log("// mfd_remote_control - ESC_BUTTON: ignored");
                    break;
                } // if

                // If a popup is showing, hide it and break
                if (hideTunerPresetsPopup()) break;
                if (hideAudioSettingsPopup()) break;
                if (hidePopup("status_popup")) break;
                if (hidePopup("satnav_continue_guidance_popup")) break;
                if (hidePopup("satnav_delete_directory_data_popup")) break;

                // Hiding this popup might make the underlying "Computing route in progress" popup visible
                if (hidePopup("satnav_guidance_preference_popup")) break;

                if (hidePopup("satnav_delete_item_popup")) break;
                if (hidePopup("satnav_input_stored_in_professional_dir_popup")) break;
                if (hidePopup("satnav_input_stored_in_personal_dir_popup")) break;
                if (hidePopup("satnav_initializing_popup")) break;
                if (hidePopup("notification_popup")) break;
                if (hidePopup("trip_computer_popup")) break;
                if (hidePopup("satnav_reached_destination_popup")) break;
                if (hidePopup("audio_popup")) break;

                // Ignore the "Esc" button when guidance screen is showing
                if ($("#satnav_guidance").is(":visible")) break;

                // Very special situation... Escaping out of list of services after entering a new "Select a Service"
                // address means escaping back all the way to the to the "Select a Service" address screen.
                if (currentMenu === "satnav_choose_from_list"
                    && $("#mfd_to_satnav_request").text() === "Service"  // TODO - other language?
                    && menuStack.indexOf("satnav_show_last_destination") >= 0
                    && menuStack[menuStack.length - 1] !== "satnav_show_last_destination")
                {
                    exitMenu();
                    exitMenu();
                } // if

                exitMenu();
            }
            else if (button === "UP_BUTTON"
                    || button === "DOWN_BUTTON"
                    || button === "LEFT_BUTTON"
                    || button === "RIGHT_BUTTON")
            {
                // While driving, it is not possible to navigate the main sat nav menu
                // (but "satnav_guidance_tools_menu" can be navigated)
                if (currentMenu === "satnav_main_menu" || menuStack[1] === "satnav_main_menu")
                {
                    if (satnavVehicleMoving()) break;
                } // if

                // Entering a character in the "satnav_enter_street_characters" screen?
                if ($("#satnav_enter_street_characters").is(":visible"))
                {
                    userHadOpportunityToEnterStreet = true;
                } // if

                keyPressed(button);
                satnavGetSelectedCharacter();
            }
            else if (button === "ENTER_BUTTON")
            {
                // "Val" button pressed on IR controller

                // Ignore if just changed screen; sometimes the screen change is triggered by the system, not the
                // remote control. If the user wanted pressed a button on the previous screen, it will end up
                // being handled by the current screen, which is confusing.
                if (Date.now() - lastScreenChangedAt < 250)
                {
                    console.log("// mfd_remote_control - ENTER_BUTTON: ignored");
                    break;
                } // if

                // If a popup is showing, hide it and break
                if (hideTunerPresetsPopup()) break;
                if (hideAudioSettingsPopup()) break;
                if (hidePopup("status_popup")) break;
                if (hidePopup("satnav_delete_directory_data_popup")) break;
                if (hidePopup("satnav_input_stored_in_professional_dir_popup")) break;
                if (hidePopup("satnav_input_stored_in_personal_dir_popup")) break;
                if (hidePopup("satnav_initializing_popup")) break;
                if (hidePopup("trip_computer_popup")) break;
                if (hidePopup("satnav_reached_destination_popup")) break;
                if (hidePopup("audio_popup")) break;

                // In sat nav guidance mode, clicking "Val" shows the "Guidance tools" menu
                if (satnavMode === "IN_GUIDANCE_MODE"  // In guidance mode?
                    && $(".notificationPopup:visible").length === 0  // And no popup showing?
                    && ! inMenu())  // And not in any menu?
                {
                    gotoTopLevelMenu("satnav_guidance_tools_menu");
                    break;
                } // if

                // While driving, it is not possible to navigate the main sat nav menu
                // (but "satnav_guidance_tools_menu" can be navigated)
                if (currentMenu === "satnav_main_menu" || menuStack[1] === "satnav_main_menu")
                {
                    if (satnavVehicleMoving()) break;
                } // if

                // Entering a character in the "satnav_enter_street_characters" screen?
                if ($("#satnav_enter_street_characters").is(":visible")) userHadOpportunityToEnterStreet = true;

                if (currentMenu === "satnav_enter_city_characters" || currentMenu === "satnav_enter_street_characters")
                {
                    // In case both the "Val" packet and the "End_of_button_press" packet were missed...
                    var selectedChar = $("#satnav_to_mfd_show_characters_line_1 .invertedText").text();
                    if (! selectedChar) selectedChar = $("#satnav_to_mfd_show_characters_line_2 .invertedText").text();
                    if (selectedChar) satnavLastEnteredChar = selectedChar;
                } // if

                buttonClicked();
            }
            else if (button === "MODE_BUTTON")
            {
                // Ignore when user is browsing the menus
                if (inMenu()) break;

                nextLargeScreen();
            } // if
        } // case
        break;

        case "mfd_status":
        {
            if (value === "MFD_SCREEN_ON")
            {
                if (handleItemChange.economyMode === "ON" && currentLargeScreenId !== "pre_flight")
                {
                    showNotificationPopup("Changing to<br />power-save mode", 15000);
                } // if
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

// -----
// Functions for demo

// Head unit OFF
function headUnitOff()
{
    writeToDom
    (
        {
            "head_unit_power":"OFF",
            "tape_present":"NO",
            "cd_present":"YES",
            "audio_source":"NONE",
            "ext_mute":"OFF",
            "mute":"OFF",
            "volume":"0",
            "volume_update":"NO",
            "volume_perc":{"style":{"transform":"scaleX(0.00)"}},
            "audio_menu":"CLOSED",
            "bass":"+4",
            "bass_update":"NO",
            "treble":"+4",
            "treble_update":"NO",
            "loudness":"ON",
            "fader":"+0",
            "fader_update":"NO",
            "balance":"+0",
            "balance_update":"NO",
            "auto_volume":"ON"
        }
    );
} // headUnitOff

// Handle key down events in demo mode
function onKeyDown(event)
{
    // Do nothing if the event was already processed
    if (event.defaultPrevented) return;

    switch (event.key)
    {
        case "m":
        case "M":
        {
            writeToDom({"mfd_remote_control":"MENU_BUTTON"});
        } // case
        break;

        case "Down": // IE/Edge specific value
        case "ArrowDown":
        {
            writeToDom({"mfd_remote_control":"DOWN_BUTTON"});
            break;
        } // case

        case "Up": // IE/Edge specific value
        case "ArrowUp":
        {
            writeToDom({"mfd_remote_control":"UP_BUTTON"});
            break;
        } // case

        case "Left": // IE/Edge specific value
        case "ArrowLeft":
        {
            writeToDom({"mfd_remote_control":"LEFT_BUTTON"});
            break;
        } // case

        case "Right": // IE/Edge specific value
        case "ArrowRight":
        {
            writeToDom({"mfd_remote_control":"RIGHT_BUTTON"});
            break;
        } // case

        case "Tab":
        {
            var wasInDemoMode = inDemoMode;
            inDemoMode = false;  // To cycle through the screens like the remote control "MOD" button would
            writeToDom({"mfd_remote_control":"MODE_BUTTON"});
            inDemoMode = wasInDemoMode;
            break;
        } // case

        case "Enter":
        {
            writeToDom({"mfd_remote_control":"ENTER_BUTTON"});
            break;
        } // case

        case "Esc": // IE/Edge specific value
        case "Escape":
        {
            writeToDom({"mfd_remote_control":"ESC_BUTTON"});
            break;
        } // case

        case "n":
        case "N":
        {
            if (satnavMode !== "IN_GUIDANCE_MODE") satnavMode = "IN_GUIDANCE_MODE";
            else satnavMode = "IDLE";

            console.log("// satnavMode set to '" + satnavMode + "'");

            break;
        } // case

        case "t":
        case "T":
        {
            if ($("#audio_source").text() !== "TUNER")
            {
                // Tuner ON
                writeToDom
                (
                    {
                        "head_unit_power":"ON",
                        "tape_present":"NO",
                        "cd_present":"YES",
                        "audio_source":"TUNER",
                        "ext_mute":"OFF",
                        "mute":"OFF",
                        "volume":"0",
                        "volume_update":"NO",
                        "volume_perc":{"style":{"transform":"scaleX(0.00)"}},
                        "audio_menu":"CLOSED",
                        "bass":"+4",
                        "bass_update":"NO",
                        "treble":"+4",
                        "treble_update":"NO",
                        "loudness":"ON",
                        "fader":"+0",
                        "fader_update":"NO",
                        "balance":"+0",
                        "balance_update":"NO",
                        "auto_volume":"ON"
                    }
                );
                writeToDom
                (
                    {
                        "tuner_band":"FM1",
                        "fm_band":"ON",
                        "fm_band_1":"ON",
                        "fm_band_2":"OFF",
                        "fm_band_ast":"OFF",
                        "am_band":"OFF",
                        "tuner_memory":"5",
                        "frequency":"100.1",
                        "frequency_h":"0",
                        "frequency_unit":"MHz",
                        "frequency_khz":"OFF",
                        "frequency_mhz":"ON",
                        "signal_strength":"8",
                        "search_mode":"NOT_SEARCHING",
                        "search_manual":"OFF",
                        "search_sensitivity":"",
                        "search_sensitivity_lo":"OFF",
                        "search_sensitivity_dx":"OFF",
                        "search_direction":"",
                        "search_direction_up":"OFF",
                        "search_direction_down":"OFF",
                        "pty_selection_menu":"OFF",
                        "selected_pty_8":"Pop M",
                        "selected_pty_16":"Pop Music",
                        "selected_pty_full":"Pop Music",
                        "pty_standby_mode":"NO",
                        "pty_match":"NO",
                        "pty_8":"News",
                        "pty_16":"News",
                        "pty_full":"News",
                        "pi_code":"83D1",
                        "pi_country":"NL",
                        "pi_area_coverage":"supra-regional",
                        "regional":"OFF",
                        "ta_selected":"YES",
                        "ta_not_available":"NO",
                        "rds_selected":"YES",
                        "rds_not_available":"NO",
                        "rds_text":"BNR     ",
                        "info_traffic":"NO"
                    }
                );
            }
            else
            {
                headUnitOff();
            } // if
        } // case
        break;

        case "i":
        case "I":
        {
            if ($("#audio_source").text() !== "CD")
            {
                // CD player ON
                writeToDom
                (
                    {
                        "head_unit_power":"ON",
                        "tape_present":"NO",
                        "cd_present":"YES",
                        "audio_source":"CD",
                        "ext_mute":"OFF",
                        "mute":"OFF",
                        "volume":"1",
                        "volume_update":"NO",
                        "volume_perc":{"style":{"transform":"scaleX(0.03)"}},
                        "audio_menu":"CLOSED",
                        "bass":"+3",
                        "bass_update":"NO",
                        "treble":"+3",
                        "treble_update":"NO",
                        "loudness":"ON",
                        "fader":"+0",
                        "fader_update":"NO",
                        "balance":"+0",
                        "balance_update":"NO",
                        "auto_volume":"ON"
                    }
                );
                writeToDom
                (
                    {
                        "cd_status":"PLAY",
                        "cd_status_loading":"OFF",
                        "cd_status_eject":"OFF",
                        "cd_status_pause":"OFF",
                        "cd_status_play":"ON",
                        "cd_status_fast_forward":"OFF",
                        "cd_status_rewind":"OFF",
                        "cd_status_searching":"OFF",
                        "cd_track_time":"3:08",
                        "cd_current_track":"9",
                        "cd_total_tracks":"15",
                        "cd_total_time":"--:--",
                        "cd_random":"OFF"
                    }
                );
            }
            else
            {
                headUnitOff();
            } // if
        } // case
        break;

        case "c":
        case "C":
        {
            if ($("#audio_source").text() !== "CD_CHANGER")
            {
                // CD player ON
                writeToDom
                (
                    {
                        "head_unit_power":"ON",
                        "tape_present":"NO",
                        "cd_present":"YES",
                        "audio_source":"CD_CHANGER",
                        "ext_mute":"OFF",
                        "mute":"OFF",
                        "volume":"1",
                        "volume_update":"NO",
                        "volume_perc":{"style":{"transform":"scaleX(0.03)"}},
                        "audio_menu":"CLOSED",
                        "bass":"+3",
                        "bass_update":"NO",
                        "treble":"+3",
                        "treble_update":"NO",
                        "loudness":"ON",
                        "fader":"+0",
                        "fader_update":"NO",
                        "balance":"+0",
                        "balance_update":"NO",
                        "auto_volume":"ON"
                    }
                );
                writeToDom
                (
                    {
                        "cd_changer_present":"YES",
                        "cd_changer_status_loading":"NO",
                        "cd_changer_status_eject":"NO",
                        "cd_changer_status_operational":"YES",
                        "cd_changer_status_searching":"ON",
                        "cd_changer_status":"PLAY-SEARCHING",
                        "cd_changer_status_pause":"OFF",
                        "cd_changer_status_play":"OFF",
                        "cd_changer_status_fast_forward":"OFF",
                        "cd_changer_status_rewind":"OFF",
                        "cd_changer_cartridge_present":"YES",
                        "cd_changer_track_time":"--:--",
                        "cd_changer_current_track":"13",
                        "cd_changer_total_tracks":"15",
                        "cd_changer_disc_1_present":"YES",
                        "cd_changer_disc_2_present":"YES",
                        "cd_changer_disc_3_present":"YES",
                        "cd_changer_disc_4_present":"YES",
                        "cd_changer_disc_5_present":"NO",
                        "cd_changer_disc_6_present":"NO",
                        "cd_changer_current_disc":cdChangerCurrentDisc,
                        "cd_changer_random":"OFF"
                    }
                );
            }
            else
            {
                headUnitOff();
            } // if
        } // case
        break;

        default:
            return;  // Quit when this doesn't handle the key event
    } // switch

    // Cancel the default action to avoid it being handled twice
    event.preventDefault();
} // onKeyDown

function demoMode()
{
    // Set "demo mode" on to prevent certain fields from being overwritten; fill DOM elements with demo values
    inDemoMode = true;

    // In demo mode, we can type keys to simulate remote control buttons
    window.addEventListener("keydown", onKeyDown, true);

    $("#splash_text").hide();

    // Small information panel (left)
    $("#trip_1_button").addClass("active");
    $("#trip_2_button").removeClass("active");
    $("#trip_1").show();
    $("#trip_2").hide();
    $('[gid="avg_consumption_lt_100_1"]').text("5.7");
    $('[gid="avg_speed_1"]').text("63");
    $('[gid="distance_1"]').text("2370");
    $('[gid="avg_consumption_lt_100_2"]').text("6.3");
    $('[gid="avg_speed_2"]').text("72");
    $('[gid="distance_2"]').text("1325");

    // Large information panel (right)

    // Multimedia
    $("#power").removeClass("ledOff");
    $("#power").addClass("ledOn");
    $("#cd_present").removeClass("ledOff");
    $("#cd_present").addClass("ledOn");
    $("#tape_present").removeClass("ledOn");
    $("#tape_present").addClass("ledOff");
    $("#cd_changer_cartridge_present").removeClass("ledOff");
    $("#cd_changer_cartridge_present").addClass("ledOn");

    $("#info_traffic").removeClass("ledOn");
    $("#info_traffic").addClass("ledOff");
    $('[gid="ext_mute"]').removeClass("ledOn");
    $('[gid="ext_mute"]').addClass("ledOff");
    $('[gid="mute"]').removeClass("ledOn");
    $('[gid="mute"]').addClass("ledOff");

    // Tuner
    $('[gid="ta_selected"]').removeClass("ledOff");
    $('[gid="ta_selected"]').addClass("ledOn");
    $('[gid="ta_not_available"]').hide();
    $('[gid="tuner_band"]').text("FM1");
    $('[gid="tuner_memory"]').text("5");
    $('[gid="frequency"]').text("102.1");
    $('[gid="frequency_h"]').text("0");
    $('[gid="frequency_mhz"]').removeClass("ledOff");
    $('[gid="frequency_mhz"]').addClass("ledOn");
    $('[gid="rds_text"]').text("RADIO 538");
    $("#pty_8").text("Pop M");
    $("#pty_16").text("Pop Music");
    $('[gid="pi_country"]').text("NL");
    $("#search_sensitivity_dx").removeClass("ledOff");
    $("#search_sensitivity_dx").addClass("ledOn");
    $("#signal_strength").text("12");
    $("#regional").removeClass("ledOff");
    $("#regional").addClass("ledOn");
    $('[gid="rds_selected"]').removeClass("ledOff");
    $('[gid="rds_selected"]').addClass("ledOn");
    $('[gid="rds_not_available"]').hide();

    // Audio settings popup
    $('[gid="volume"]').text("12");
    $("#bass").text("+4");
    $("#treble").text("+3");
    $("#fader").text("-1");
    $("#balance").text("0");
    $("#loudness").removeClass("ledOn");
    $("#loudness").addClass("ledOff");
    $("#auto_volume").removeClass("ledOff");
    $("#auto_volume").addClass("ledOn");
    $("#bass_select").show();

    // Tape player
    $('[gid="tape_side"]').text("2");
    $('[gid="tape_status_play"]').show();

    // Internal CD player
    $('[gid="audio_source"]').text("CD");
    $('[gid="cd_track_time"]').text("2:17");
    $('[gid="cd_status_play"]').show();
    $("#cd_total_time").text("43:16");
    $('[gid="cd_current_track"]').text("3");
    $('[gid="cd_total_tracks"]').text("15");

    // CD changer
    cdChangerCurrentDisc = "3";
    $('[gid="cd_changer_track_time"]').text("4:51");
    $('[gid="cd_changer_status_play"]').show();
    $('[gid="cd_changer_current_disc"]').text(cdChangerCurrentDisc);
    $('[gid="cd_changer_current_track"]').text("6");
    $('[gid="cd_changer_total_tracks"]').text("11");
    $("#cd_changer_disc_1_present").removeClass("ledOff");
    $("#cd_changer_disc_1_present").addClass("ledOn");
    $("#cd_changer_disc_2_present").removeClass("ledOff");
    $("#cd_changer_disc_2_present").addClass("ledOn");
    $("#cd_changer_disc_3_present").removeClass("ledOff");
    $("#cd_changer_disc_3_present").addClass("ledOn");
    $("#cd_changer_disc_4_present").removeClass("ledOff");
    $("#cd_changer_disc_4_present").addClass("ledOn");
    $("#cd_changer_disc_5_present").removeClass("ledOn");
    $("#cd_changer_disc_5_present").addClass("ledOff");
    $("#cd_changer_disc_6_present").removeClass("ledOn");
    $("#cd_changer_disc_6_present").addClass("ledOff");
    $("#cd_changer_disc_3_present").addClass("ledActive");
    $("#cd_changer_disc_3_present").css({marginTop: '-=25px'});

    // Status bar
    $('[gid="inst_consumption_lt_100"]').text("6.6");
    $('[gid="distance_to_empty"]').text("750");
    $('[gid="exterior_temperature"]').html("12.5 &deg;C");

    // Pre-flight checks
    $('[gid="fuel_level_filtered"]').text("51.6");
    $('[gid="fuel_level_filtered_perc"]').css("transform", "scaleX(0.645)");
    $('[gid="water_temp"]').text("65.0");
    $('[gid="water_temp_perc"]').css("transform", "scaleX(0.5)");
    $("#oil_level_raw").text("80");
    $("#oil_level_raw_perc").css("transform", "scaleX(0.94)");
    $("#remaining_km_to_service").text("27 500");
    $("#remaining_km_to_service_perc").css("transform", "scaleX(0.917)");
    $("#contact_key_position").text("START");
    $("#dashboard_programmed_brightness").text("14");
    $("#diesel_glow_plugs").addClass("ledOn");
    $("#diesel_glow_plugs").removeClass("ledOff");
    $("#vin").text("VF38FRHZF80371XXX");

    // Instrument cluster
    $('[gid="vehicle_speed"]').text("95");
    $('[gid="engine_rpm"]').text("1873");
    $("#odometer_1").text("65 803.2");
    $("#delivered_power").text("30.7");
    $("#delivered_torque").text("23.1");

    // Sat nav
    satnavInitialized = true;
    $("#main_menu_goto_satnav_button").removeClass("buttonDisabled");
    $("#main_menu_goto_satnav_button").addClass("buttonSelected");
    $("#main_menu_goto_screen_configuration_button").removeClass("buttonSelected");
    $("#satnav_main_menu_select_a_service_button").removeClass("buttonDisabled");
    $("#satnav_disc_recognized").addClass("ledOn");
    $("#satnav_disc_recognized").removeClass("ledOff");
    $("#satnav_gps_scanning").addClass("ledOff");
    $("#satnav_gps_scanning").removeClass("ledOn");
    $("#satnav_gps_fix_lost").addClass("ledOff");
    $("#satnav_gps_fix_lost").removeClass("ledOn");
    $('[gid="satnav_gps_fix"]').addClass("ledOn");
    $('[gid="satnav_gps_fix"]').removeClass("ledOff");
    $("#satnav_gps_speed").text("0");

    // Compass
    var compass_heading = 135;
    $("#satnav_curr_heading_compass_needle").css("transform", "rotate(" + compass_heading + "deg)");
    $("#satnav_curr_heading_compass_tag").css(
        "transform",
        "translate(" +
            42 * Math.sin(compass_heading * Math.PI / 180) + "px," +
            -42 * Math.cos(compass_heading * Math.PI / 180) + "px)"
        );
    $("#satnav_curr_heading_compass_tag").find("*").removeClass('satNavInstructionDisabledIconText');
    $("#satnav_curr_heading_compass_needle").find("*").removeClass('satNavInstructionDisabledIcon');

    // Sat nav current location
    $('[gid="satnav_curr_street_shown"]').text("Keizersgracht (Amsterdam)");

    // Sat nav guidance preference
    $("#satnav_guidance_preference_fastest").html("");
    $("#satnav_guidance_preference_shortest").html("<b>&#10004;</b>");
    satnavGuidancePreferenceSelectTickedButton();

    // Sat nav enter destination city
    $("#satnav_current_destination_city").text("Amsterdam");
    $("#satnav_enter_characters_validate_button").removeClass("buttonDisabled");
    $("#satnav_enter_characters_validate_button").addClass("buttonSelected");
    $('[gid="satnav_to_mfd_list_size"]').text("35");
    $("#satnav_current_destination_street").text("Keizersgracht");

    // Sat nav choose destination street from list
    $("#mfd_to_satnav_request").text("Enter street");
    var lines =
    [
        "AADIJK -ALMELO- -AADORP-",
        "AADORPWEG -ALMELO- -AADORP-",
        "ACACIALAAN -ALMELO- -AADORP-",
        "ALBARDASTRAAT -ALMELO- -AADORP-",
        "ALMELOSEWEG -ALMELO- -AADORP-",
        "BEDRIJVENPARKSINGEL -ALMELO- -AADORP-",
        "BEDRIJVENPARK TWENTE -ALMELO- -AADORP-",
        "BERKENLAAN -ALMELO- -AADORP-",
        "BEUKENLAAN -ALMELO- -AADORP-",
        "BRUGLAAN -ALMELO- -AADORP-",
        "DENNENLAAN -ALMELO- -AADORP-",
        "SCHOUT DODDESTRAAT -ALMELO- -AADORP-",
        "EIKENLAAN -ALMELO- -AADORP-",
        "GRAVENWEG -ALMELO- -AADORP-",
        "HEUVELTJESLAAN -ALMELO- -AADORP-",
        "IEPENWEG NOORD -ALMELO- -AADORP-",
        "IEPENWEG ZUID -ALMELO- -AADORP-",
        "KANAALWEG ZUID -ALMELO- -AADORP-",
        "KASTANJELAAN -ALMELO- -AADORP-",
        "KERKWEG -ALMELO- -AADORP-"
    ];
    $("#satnav_list").html(lines.join('<br />'));
    highlightFirstLine("satnav_list");
    highlightNextLine("satnav_list");

    // Sat nav enter destination house number
    $("#satnav_house_number_range").text("From 1 to 812");
    $("#satnav_entered_number").text("6 0 9 ");
    highlightFirstLetter("satnav_house_number_show_characters");
    highlightPreviousLetter("satnav_house_number_show_characters");
    highlightPreviousLetter("satnav_house_number_show_characters");

    // Sat nav private address entry
    $("#satnav_personal_address_entry").text("HOME");
    $("#satnav_personal_address_city").text("AMSTELVEEN");
    $("#satnav_personal_address_street_shown").text("SPORTLAAN");
    $("#satnav_personal_address_house_number_shown").text("23");

    // Sat nav business address entry
    $("#satnav_professional_address_entry").text("WORK");
    $("#satnav_professional_address_city").text("Schiphol");
    $("#satnav_professional_address_street_shown").text("Westzijde");
    $("#satnav_professional_address_house_number_shown").text("1118");

    // Sat nav place of interest address entry
    $("#satnav_service_address_entry").text("P1 Short-term parking");
    $("#satnav_service_address_city").text("Schiphol");
    $("#satnav_service_address_street").text("Boulevard");
    $("#satnav_service_address_distance_number").text("17");
    $("#satnav_service_address_distance_unit").text("km");
    $("#satnav_service_address_entry_number").text("3");
    $("#satnav_to_mfd_list_size").text("17");

    // Sat nav programmed (current) destination
    $('[gid="satnav_current_destination_city"]').text("Amsterdam");
    $("#satnav_current_destination_street_shown").text("Keizersgracht");
    $("#satnav_current_destination_house_number_shown").text("609");

    // Sat nav last destination
    $('[gid="satnav_last_destination_city"]').text("Schiphol");
    $('[gid="satnav_last_destination_street_shown"]').text("Evert van de Beekstraat");
    $('[gid="satnav_last_destination_house_number_shown"]').text("31");

    // Sat nav guidance
    $("#satnav_distance_to_dest_via_road_number").text("45");
    $("#satnav_distance_to_dest_via_road_unit").text("km");
    $("#satnav_guidance_curr_street").text("Evert van de Beekstraat");
    $("#satnav_guidance_next_street").text("Handelskade");
    $("#satnav_turn_at_number").text("200");
    $("#satnav_turn_at_unit").text("m");
    $("#satnav_next_turn_icon").show();

    // Heading to destination
    var heading_to_dest = 292.5;
    $("#satnav_heading_to_dest_pointer").css("transform", "rotate(" + heading_to_dest + "deg)");
    $("#satnav_heading_to_dest_tag").css(
        "transform",
        "translate(" +
            70 * Math.sin(heading_to_dest * Math.PI / 180) + "px," +
            -70 * Math.cos(heading_to_dest * Math.PI / 180) + "px)"
        );
    $("#satnav_heading_to_dest_tag").find("*").removeClass('satNavInstructionDisabledIconText');
    $("#satnav_heading_to_dest_pointer").find("*").removeClass('satNavInstructionDisabledIcon');

    // Current turn is a roundabout with five legs; display the legs not to take
    $("#satnav_curr_turn_icon").show();
    $("#satnav_curr_turn_roundabout").show();
    $("#satnav_curr_turn_icon_leg_90_0").show();
    $("#satnav_curr_turn_icon_leg_157_5").show();
    $("#satnav_curr_turn_icon_leg_202_5").show();

    // Next turn is a junction four legs; display the legs not to take
    $("#satnav_next_turn_icon_leg_112_5").show();
    $("#satnav_next_turn_icon_leg_202_5").show();

    // Show some "no entry" icons
    $("#satnav_curr_turn_icon_no_entry_157_5").show();
    $("#satnav_next_turn_icon_no_entry_202_5").show();

    // Set the "direction" arrows
    var currTurnAngle = 247.5;
    document.getElementById("satnav_curr_turn_icon_direction").style.transform = "rotate(" + currTurnAngle + "deg)";
    document.getElementById("satnav_curr_turn_icon_direction_on_roundabout").setAttribute(
        "d", describeArc(150, 150, 30, currTurnAngle - 180, 180)
    );
    var nextTurnAngle = 292.5;
    document.getElementById("satnav_next_turn_icon_direction").style.transform = "rotate(" + nextTurnAngle + "deg)";
    document.getElementById("satnav_next_turn_icon_direction_on_roundabout").setAttribute(
        "d", describeArc(150, 150, 30, nextTurnAngle - 180, 180)
    );

    // Popup warning or information
    $("#notification_icon_warning").show();
    $("#notification_icon_info").hide();
    $("#last_notification_message_on_mfd").text("Oil pressure too low");

    // Door open popup
    $("#door_front_left").show();
    $("#door_front_right").show();

    // Tuner presets popup
    $("#presets_fm_1").show();
    $("#radio_preset_FM1_1").text("NPO 1");
    $("#radio_preset_FM1_2").text("Classic NL");
    $("#radio_preset_FM1_3").text("3FM");
    $("#radio_preset_FM1_4").text("BNR");
    $("#radio_preset_FM1_5").text("Radio 538");
    $("#radio_preset_FM1_6").text("Veronica");
    $("#presets_memory_5_select").show();
} // demoMode

//demoMode();
)=====";
