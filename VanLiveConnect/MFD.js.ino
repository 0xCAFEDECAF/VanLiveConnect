
char mfd_js[] PROGMEM = R"=====(

// Javascript functions to drive the browser-based Multi-Function Display (MFD)

// -----
// General

function addThousandsSeparator(x)
{
	return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
} // addThousandsSeparator

function clamp(num, min, max)
{
	return Math.min(Math.max(num, min), max);
} // clamp

// -----
// On-screen clocks

// Capitalize first letter
function CapFirstLetter(string)
{
	firstLetter = string.charAt(0);
	if (string.charCodeAt(0) == 8206)  // IE11-special
	{
		firstLetter = string.charAt(1);
		return firstLetter.toUpperCase() + string.slice(2);
	} // if
	return firstLetter.toUpperCase() + string.slice(1);
} // CapFirstLetter

// Show the current date and time
function updateDateTime()
{
	var locale = 
		localStorage.mfdLanguage === "set_language_french" ? 'fr' :
		localStorage.mfdLanguage === "set_language_german" ? 'de-DE' :
		localStorage.mfdLanguage === "set_language_spanish" ? 'es-ES' :
		localStorage.mfdLanguage === "set_language_italian" ? 'it' :
		localStorage.mfdLanguage === "set_language_dutch" ? 'nl' :
		'en-GB';

	var date = new Date().toLocaleDateString(locale, {weekday: 'short', day: 'numeric', month: 'short'});
	date = date.replace(/0(\d)/, "$1");  // IE11 incorrectly shows leading "0"
	date = date.replace(/,/, "");
	$("#date_small").text(CapFirstLetter(date));

	date = new Date().toLocaleDateString(locale, {weekday: 'long'});
	$("#date_weekday").text(CapFirstLetter(date) + ",");

	date = new Date().toLocaleDateString(locale, {day: 'numeric', month: 'long', year: 'numeric'});
	date = date.replace(/0(\d )/, "$1");  // IE11 incorrectly shows leading "0"
	$("#date").text(date);

	var time = new Date().toLocaleTimeString(
		[],  // When 12-hour time unit is chosen, 'a.m.' / 'p.m.' will be with dots, and in lower case
		{
			hour: "numeric",
			minute: "2-digit",
			hour12: localStorage.mfdTimeUnit === "set_units_12h"
		}
	);
	$("#time").text(time);
	$("#time_small").text(time.replace(/.m.$/, ""));  // Remove trailing '.m.' if present
} // updateDateTime

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
// Functions for parsing and handling JSON data as it comes in on a WebSocket

// Inspired by https://gist.github.com/ismasan/299789
var fancyWebSocket = function(url)
{
	var _url = url;
	var conn = new WebSocket(url);

	var callbacks = {};

	this.bind = function(event_name, callback)
	{
		callbacks[event_name] = callbacks[event_name] || [];
		callbacks[event_name].push(callback);
		return this;  // chainable
	}; // function

	this.send = function(data)
	{
		if (conn.readyState === 1) conn.send(data);
	}; // function

	// Dispatch to the right handlers
	conn.onmessage = function(evt)
	{
		var json = JSON.parse(evt.data)
		dispatch(json.event, json.data)
	}; // function

	conn.onopen = function()
	{
		console.log("// Connected to WebSocket '" + url + "'!");
		dispatch('open', null);
	}; // function

	conn.onclose = function(event)
	{
		console.log("// Connection to WebSocket '" + url + "' died!");
		dispatch('close', null);

		//conn = new WebSocket(_url);
		if (event.code == 3001)
		{
			console.log("// WebSocket '" + url + "' closed");
			//conn = null;
		}
		else
		{
			//conn = null;
			console.log("// WebSocket '" + url + "' connection error: " + event.code);
			connectToWebSocket();
		} // if
	}; // function

	conn.onerror = function(event)
	{
		if (conn.readyState == 1)
		{
			console.log("// WebSocket '" + url + "' normal error: " + event.type);
		} // if
	}; // function

	var dispatch = function(event_name, message)
	{
		var chain = callbacks[event_name];
		if (chain == undefined) return;  // No callbacks for this event
		for (var i = 0; i < chain.length; i++)
		{
			chain[i](message)
		} // for
	}; // function
}; // fancyWebSocket

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

function processJsonObject(item, jsonObject)
{
	// A double underscore ('__') is parsed as a "ditto mark". Example:
	//
	// {
	//   "event": "display",
	//   "data": {
	//     "satnav_fork_icon__take_right_exit":"OFF",
	//     "__keep_right":"OFF",
	//     "__take_left_exit":"OFF",
	//     "__keep_left":"OFF",
	//   }
	// }
	//
	// The above will update DOM items with id "satnav_fork_icon_take_right_exit", "satnav_fork_icon_keep_right", etc.
	//
	// Note: double underscores ('__') are translated to single ones ('_').

	// Recognize the string before a double underscore ('__') as a prefix for subsequent items
	var r = /^(.*)__.+/.exec(item);
	if (! r) processJsonObject.prefix = ""; else if (r[1]) processJsonObject.prefix = r[1];

	// Pre-pend saved prefix to items starting with double underscore ('__', "ditto mark")
	var domItem = item;
	if (domItem.match(/^__.+/)) domItem = processJsonObject.prefix + domItem;

	domItem = domItem.replace("__", "_"); 

	// In demo mode, skip certain updates and just call the handler
	if (inDemoMode && fixFieldsOnDemo.indexOf(domItem) >= 0)
	{
		handleItemChange(domItem, jsonObject);
		return;
	} // if

	var selectorById = '#' + domItem;  // Select by 'id' attribute (must be unique in the DOM)
	var selectorByAttr = '[gid="' + domItem + '"]';  // Select also by custom attribute 'gid' (need not be unique)

	// jQuery-style loop over merged, unique-element array
	$.each($.unique($.merge($(selectorByAttr), $(selectorById))), function (index, selector)
	{
		if ($(selector).length <= 0) return;  // Go to next iteration in .each()

		if (Array.isArray(jsonObject))
		{
			// Handling of multi-line DOM objects to show lists. Example:
			//
			// {
			//   "event": "display",
			//   "data": {
			//     "alarm_list": [
			//       "Tyre pressure low",
			//       "Door open",
			//       "Water temperature too high",
			//       "Oil level too low"
			//     ]
			//   }
			// }

			$(selector).html(jsonObject.join('<br />'));
		}
		else if (!!jsonObject && typeof(jsonObject) === "object")
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

			var attributeObj = jsonObject;
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
			var itemText = jsonObject;
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
	handleItemChange(domItem, jsonObject);
} // processJsonObject

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
		if (elapsed > 20000) elapsed = 20000;  // During replay, don't wait longer than 20 seconds
		console.log("await sleep(" + elapsed + "); //");
	} // if
	console.log("writeToDom(" + JSON.stringify(jsonObj) + "); //");

	writeToDom.lastInvoked = now;

	for (var item in jsonObj)
	{
		// A nested object with an item name that ends with '_' indicates namespace. For example:
		//
		// {
		//   "event": "display",
		//   "data": {
		//     "satnav_fork_icon_": {
		//       "take_right_exit":"OFF",
		//       "keep_right":"OFF",
		//       "take_left_exit":"OFF",
		//       "keep_left":"OFF"
		//     }
		//   }
		// }
		//
		// The above update DOM items with id "satnav_fork_icon_take_right_exit", "satnav_fork_icon_keep_right", etc.
		//
		if (item.endsWith('_') && !!jsonObj[item] && typeof(jsonObj[item]) === "object")
		{
			var subObj = jsonObj[item];
			for (var subItem in subObj) processJsonObject(item + subItem, subObj[subItem]);  // Use 'item' as prefix
		}
		else
		{
			processJsonObject(item, jsonObj[item]);
		} // if
	} // for
} // writeToDom

var webSocketServerHost = window.location.hostname;
// Send some data so that a webSocket event triggers when the connection has failed
function keepAliveWebSocket()
{
	webSocket.send("keepalive");
} // keepAliveWebSocket

var webSocket;
var keepAliveWebSocketTimer = null;
var websocketPreventConnect = false;

// Prevent double re-connecting when user reloads the page
window.onbeforeunload = function() { websocketPreventConnect = true; };

function connectToWebSocket()
{
	if (websocketPreventConnect) return;

	var wsUrl = "ws://" + webSocketServerHost + ":81/";
	console.log("// Connecting to WebSocket '" + wsUrl + "'");

	// Create WebSocket class instance
	webSocket = new fancyWebSocket(wsUrl);

	// Bind WebSocket to server events
	webSocket.bind('display', function(data)
		{
			// Re-start the "keep alive" timer
			clearInterval(keepAliveWebSocketTimer);
			keepAliveWebSocketTimer = setInterval(keepAliveWebSocket, 5000);

			writeToDom(data);
		}
	);
	webSocket.bind('open', function ()
		{
			webSocket.send("mfd_language:" + localStorage.mfdLanguage);
			webSocket.send("mfd_distance_unit:" + localStorage.mfdDistanceUnit);
			webSocket.send("mfd_temperature_unit:" + localStorage.mfdTemperatureUnit);
			webSocket.send("mfd_time_unit:" + localStorage.mfdTimeUnit);

			// (Re-)start the "keep alive" timer
			clearInterval(keepAliveWebSocketTimer);
			keepAliveWebSocketTimer = setInterval(keepAliveWebSocket, 5000);
		}
	);
	webSocket.bind('close', function ()
		{
			clearInterval(keepAliveWebSocketTimer);
			// demoMode();
		}
	);
} // connectToWebSocket

// -----
// Handling of vehicle data

var economyMode= "";
var contactKeyPosition;
var engineRpm = 0;
var engineRunning = "";  // Either "" (unknown), "YES" or "NO"
var engineCoolantTemperature = 80;  // In degrees Celsius
var vehicleSpeed = 0;
var icyConditions = false;
var wasRiskOfIceWarningShown = false;

var suppressClimateControlPopup = null;

function changeToInstrumentsScreen()
{
	changeLargeScreenTo("instruments");

	// Suppress climate control popup during the next 2 seconds
	clearTimeout(suppressClimateControlPopup);
	suppressClimateControlPopup = setTimeout(function () { suppressClimateControlPopup = null; }, 2000);
} // changeToInstrumentsScreen

// -----
// Functions for navigating through the screens and their subscreens/elements

// Open a "trip_info" tab in the small (left) information panel
function openTripInfoTab(evt, tabName)
{
	$("#trip_info .tabContent").hide();
	$("#trip_info .tablinks").removeClass("active");
	$("#" + tabName).show();
	evt.currentTarget.className += " active";
} // openTripInfoTab

 // Set visibility of an element by ID, together with all its parents in the 'div' hierarchy
function setVisibilityOfElementAndParents(id, value)
{
	var el = document.getElementById(id);
	while (el && el.tagName === "DIV")
	{
		el.style.display = value;
		el = el.parentNode;
	} // while
} // setVisibilityOfElementAndParents

var currentLargeScreenId = "clock";  // Currently shown large (right) screen. Initialize to the first screen visible.
var lastScreenChangedAt = 0;  // Last time the large screen changed

// Switch to a specific screen on the right hand side of the display
function changeLargeScreenTo(id)
{
	if (id === currentLargeScreenId) return;

	hidePopup("audio_popup");

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

	// Report back to server (ESP) that user is browsing the menus
	if (! inMenu() && typeof webSocket !== "undefined") webSocket.send("in_menu:NO");
} // changeLargeScreenTo

var menuStack = [];

var mfdLargeScreen = "CLOCK";  // Screen currently shown in the "large" (right hand side) area on the original MFD

function selectDefaultScreen(audioSource)
{
	// Ignore invocation during engine start
	if (currentLargeScreenId === "pre_flight" && contactKeyPosition === "START") return;

	// We're no longer in any menu
	menuStack = [];
	currentMenu = undefined;

	var selectedScreenId = "";

	// Explicitly passed a value for 'audioSource'?
	if (typeof audioSource !== "undefined")
	{
		selectedScreenId =
			audioSource === "TUNER" ? "tuner" :
			audioSource === "TAPE" ? "tape" :
			audioSource === "CD" || audioSource === "INTERNAL_CD_OR_TAPE" ? "cd_player" :
			audioSource === "CD_CHANGER" ? "cd_changer" :
			"";
	} // if

	if (selectedScreenId === "")
	{
		if (satnavMode === "IN_GUIDANCE_MODE") selectedScreenId = "satnav_guidance";
	} // if

	if (selectedScreenId === "")
	{
		if (typeof audioSource === "undefined") audioSource = $("#audio_source").text();

		selectedScreenId =
			audioSource === "TUNER" ? "tuner" :
			audioSource === "TAPE" ? "tape" :
			audioSource === "CD" || audioSource === "INTERNAL_CD_OR_TAPE" ? "cd_player" :
			audioSource === "CD_CHANGER" ? "cd_changer" :
			"";
	} // if

	// Show current street, if known
	if (selectedScreenId === "" && satnavCurrentStreet !== "")
	{
		// But don't switch away from instrument screen, if the engine is running
		if (currentLargeScreenId === "instruments")
		{
			if (engineRunning !== "NO" && engineRpm !== 0 && contactKeyPosition === "ON") return;
		} // if

		selectedScreenId = "satnav_current_location";
	} // if

	// Fall back to instrument screen if engine is running
	if (selectedScreenId === "" && engineRunning === "YES") selectedScreenId = "instruments";

	// Final fallback screen...
	if (selectedScreenId === "") selectedScreenId = "clock";

	changeLargeScreenTo(selectedScreenId);
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
	// In "demo mode" we can cycle through all the screens, otherwise to a limited set of screens
	if (inDemoMode)
	{
		nextLargeDemoScreen();
		return;
	} // if

	if (inMenu()) return;  // Don't cycle through menu screens

	// The IDs of the screens ("divs") that will be cycled through.
	// Important: list only leaf divs here!
	var screenIds =
	[
		"clock",
		"satnav_guidance",
		"tuner",
		"tape",
		"cd_player",
		"cd_changer",
		"instruments",
		"satnav_current_location",
	];

	var idIndex = screenIds.indexOf(currentLargeScreenId);  // -1 if not found

	idIndex = (idIndex + 1) % screenIds.length;  // ID of the next screen in the sequence

	// Skip the "satnav_guidance" screen if the guidance is not active
	if (idIndex === screenIds.indexOf("satnav_guidance"))
	{
		if (satnavMode !== "IN_GUIDANCE_MODE") idIndex = (idIndex + 1) % screenIds.length;
	} // if

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

	changeLargeScreenTo(screenIds[idIndex]);
} // nextLargeScreen

function changeToTripCounter(id)
{
	// Simulate a "tab click" event
	var event = { currentTarget: document.getElementById(id + "_button")};
	openTripInfoTab(event, id);
} // changeToTripCounter

function tripComputerShortStr(tripComputerStr)
{
	return tripComputerStr === "TRIP_INFO_1" ? "TR1" :
		tripComputerStr === "TRIP_INFO_2" ? "TR2" :
		tripComputerStr === "GPS_INFO" ? "GPS" :
		tripComputerStr === "FUEL_CONSUMPTION" ? "FUE" :
		"??";
} // tripComputerShortStr

// Change to the correct screen and trip counter, given the reported value of "trip_computer_screen_tab"
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
	} // switch
} // gotoSmallScreen

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
			"trip_computer_popup_fuel_data"; // Tab chosen if the small screen was showing GPS data
	$("#" + selectedId).show();

	$("#" + selectedId + "_button").addClass("tabActive");
} // initTripComputerPopup

// Unselect any tab in the trip computer popup
function resetTripComputerPopup()
{
	$("#trip_computer_popup .tabContent").hide();
	$("#trip_computer_popup .tabLeft").removeClass("tabActive");
} // resetTripComputerPopup

// In the "trip_computer_popup", select a specific tab
function selectTabInTripComputerPopup(index)
{
	// Unselect the current tab
	resetTripComputerPopup();

	// Retrieve all tab buttons and content elements
	var tabs = $("#trip_computer_popup").find(".tabContent");
	var tabButtons = $("#trip_computer_popup").find(".tabLeft");

	// Select the specified tab
	$(tabs[index]).show();
	$(tabButtons[index]).addClass("tabActive");
} // selectTabInTripComputerPopup

// Toggle full-screen mode
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
function toggleFullScreen()
{
	if (! document.fullscreenElement && ! document.mozFullScreenElement && ! document.webkitFullscreenElement && ! document.msFullscreenElement)
	{
		// Enter full screen mode
		var elem = document.documentElement;
		if (elem.requestFullscreen) elem.requestFullscreen();
		else if (elem.mozRequestFullScreen) elem.mozRequestFullScreen(); // Firefox
		else if (elem.webkitRequestFullscreen) elem.webkitRequestFullscreen(); // Chrome and Safari
		else if (elem.msRequestFullscreen) elem.msRequestFullscreen(); // IE
	}
	else
	{
		// Exit full screen mode
		if (document.exitFullscreen) document.exitFullscreen();
		else if (document.msExitFullscreen) document.msExitFullscreen();
		else if (document.mozCancelFullScreen) document.mozCancelFullScreen();
		else if (document.webkitExitFullscreen) document.webkitExitFullscreen();
	} // if
} // toggleFullScreen

// -----
// Functions for popups

// Associative array, using the popup element ID as key
var popupTimer = {};

// Show the specified popup, with an optional timeout
function showPopup(id, msec)
{
	var popup = $("#" + id);
	if (! popup.is(":visible"))
	{
		popup.show();

		// A popup can appear under another. In that case, don't register the time.
		var topLevelPopup = $(".notificationPopup:visible").slice(-1)[0];
		if (id === topLevelPopup.id) lastScreenChangedAt = Date.now();

		// Perform "on_enter" action, if specified
		var onEnter = popup.attr("on_enter");
		if (onEnter) eval(onEnter);
	} // if

	if (! msec) return;

	// Hide popup after specified milliseconds
	clearTimeout(popupTimer[id]);
	popupTimer[id] = setTimeout(function () { hidePopup(id); }, msec);
} // showPopup

// Show a popup and send an update on the web socket, The software on the ESP needs to know when a popup is showing,
// e.g. to know when to ignore a "MOD" button press from the IR remote control.
function showPopupAndNotifyServer(id, msec, message)
{
	showPopup(id, msec);

	var messageText = typeof message !== "undefined" ? (" \"" + message.replace(/<[^>]*>/g, ' ') + "\"") : "";
	webSocket.send("mfd_popup_showing:" + (msec === 0 ? 0xFFFFFFFF : msec) + " " + id + messageText);

	$("#original_mfd_popup").text("N_POP");  // "Notification popup" - Debug info
} // showPopupAndNotifyServer

// Hide the specified or the current (top) popup
function hidePopup(id)
{
	var popup;
	if (id) popup = $("#" + id); else popup = $(".notificationPopup:visible").last();
	if (popup.length === 0 || ! popup.is(":visible")) return false;

	popup.hide();

	if (webSocket) webSocket.send("mfd_popup_showing:NO");
	$("#original_mfd_popup").empty();  // Debug info

	clearTimeout(popupTimer[id]);

	// Perform "on_exit" action, if specified
	var onExit = popup.attr("on_exit");
	if (onExit) eval(onExit);

	return true;
} // hidePopup

// Hide all visible popups. Optionally, pass the ID of a popup not keep visible.
function hideAllPopups(except)
{
	allPopups = $(".notificationPopup:visible");

	$.each(allPopups, function (index, selector)
	{
		if ($(selector).attr("id") !== except) hidePopup($(selector).attr("id"));
	});
} // hideAllPopups

// Show the notification popup (with icon) with a message and an optional timeout. The shown icon is either "info"
// (isWarning == false, default) or "warning" (isWarning == true).
function showNotificationPopup(message, msec, isWarning)
{
	if (isWarning === undefined) isWarning = false;  // IE11 does not support default parameters

	// Show the required icon: "information" or "warning"
	$("#notification_icon_info").toggle(! isWarning);
	$("#notification_icon_warning").toggle(isWarning);

	$("#last_notification_message_on_mfd").html(message);  // Set the notification text
	showPopupAndNotifyServer("notification_popup", msec, message);  // Show the notification popup
} // showNotificationPopup

// Show a simple status popup (no icon) with a message and an optional timeout
function showStatusPopup(message, msec)
{
	$("#status_popup_text").html(message);  // Set the popup text
	showPopupAndNotifyServer("status_popup", msec);  // Show the popup
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
	showPopupAndNotifyServer("audio_popup", 8000);
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
		"satnav_current_location",
		"satnav_guidance", "satnav_curr_turn_icon"
	];

	return currentMenu !== null
		&& mainScreenIds.indexOf(currentLargeScreenId) < 0;  // And not in one of the "main" screens?
} // inMenu

// TODO - there are now three algorithms for selecting the first menu item resp. button:
// 1. in function selectFirstMenuItem: just selects the first found (even if disabled)
// 2. in function gotoMenu: selects the first button, thereby skipping disabled buttons
// 3. in function selectButton: selects the specified button, thereby skipping disabled buttons, following
//	"DOWN_BUTTON" / "RIGHT_BUTTON" attributes if and where present
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
	if (menu === undefined) menu = "main_menu";

	// Report back that user is not browsing the menus
	if (typeof webSocket !== "undefined") webSocket.send("in_menu:YES");

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

		if (! id || id === currentButtonId) return;  // No further button in that direction? Or went all the way round?

		selectedButton = $("#" + id);
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

	if (currentButton.length !== 1) return;  // Only select if exactly one selected button was found

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

	gotoMenu(goto_id);  // Change to the menu screen with that ID
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
	$("#" + id).parent().parent().find(".tickBox").each(function() { $(this).empty() } );

	$("#" + id).html("<b>&#10004;</b>");
} // setTick

function toggleTick(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	// In a group, only one tick box can be ticked at a time
	$("#" + id).parent().parent().find(".tickBox").each(function() { $(this).empty() } );

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

// As inspired by https://stackoverflow.com/a/2771544
function getTextWidth(selector)
{
	var htmlOrg = $(selector).html();
	$(selector).html('<span>' + htmlOrg + '</span>');
	var width = $(selector).find('span:first').width();
	$(selector).html(htmlOrg);
	return width;
} // getTextWidth

// Associative array, using the button element ID as key
var buttonOriginalWidths = {};

// Handle an arrow key press in a screen with buttons
function navigateButtons(key)
{
	// Retrieve the currently selected button
	var selected = findSelectedButton();
	if (selected === undefined) return;

	var screen = selected.screen;
	var currentButton = selected.button;
	var currentButtonId = currentButton.attr("id");

	// Retrieve the attribute that matches the pressed key ("UP_BUTTON", "DOWN_BUTTON", "LEFT_BUTTON", "RIGHT_BUTTON")
	var gotoButtonId = currentButton.attr(key);
	var gotoButton;

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

		if (nextIdx === currIdx) return;  // Return if nothing changed

		gotoButton = $(allButtons[nextIdx]);
		gotoButtonId = gotoButton.attr("id");
	}
	else
	{
		// Keep track of the buttons checked
		var checkedButtons = {};

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

		if (gotoButtonId === currentButtonId) return;  // Return if nothing changed

		gotoButton = $("#" + gotoButtonId);
	} // if

	if (typeof currentButtonId !== "undefined" && typeof buttonOriginalWidths[currentButtonId] !== "undefined")
	{
		currentButton.width(buttonOriginalWidths[currentButtonId]);  // Reset width of button to original size
		currentButton.css({ 'marginLeft': '' });  // Place back in original position
		currentButton.css('z-index', '');  // No longer bring to front
	} // if

	// Perform "on_exit" action, if specified
	var onExit = currentButton.attr("on_exit");
	if (onExit) eval(onExit);

	currentButton.removeClass("buttonSelected");
	gotoButton.addClass("buttonSelected");

	if (typeof gotoButtonId !== "undefined")
	{
		var widthAtLeast = getTextWidth(gotoButton);
		if (gotoButton.width() < widthAtLeast)
		{
			buttonOriginalWidths[gotoButtonId] = gotoButton.width();  // Save original width of button

			// Move left a bit if necessary
			var right = gotoButton.position().left + widthAtLeast;
			var moveLeft = right - 910;
			if (moveLeft > 0) gotoButton.css({ 'marginLeft': '-=' + moveLeft + 'px' });

			gotoButton.width(widthAtLeast);  // Resize button to fit text
			gotoButton.css('z-index', 1);  // Bring to front
		} // if
	} // if

	// Perform "on_enter" action, if specified
	var onEnter = gotoButton.attr("on_enter");
	if (onEnter) eval(onEnter);
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
		index = clamp (index, 0, text.length);
		highlightIndexes[id] = index;
	} // if

	if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0;

	// Don't go before first letter or past last
	highlightIndexes[id] = clamp(highlightIndexes[id], 0, text.length - 1);

	text = text.substr(0, highlightIndexes[id])
		+ "<span class='invertedText'>" + text[highlightIndexes[id]] + "</span>"
		+ text.substr(highlightIndexes[id] + 1, text.length);
	$("#" + id).html(text);
} // highlightLetter

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

	$("#" + id).scrollTop(0);  // Make sure the top line is visible
} // highlightFirstLine

function highlightNextLine(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	unhighlightLine(id);

	if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0; else highlightIndexes[id]++;

	highlightLine(id);

	// Scroll along if necessary

	var topOfHighlightedLine = Math.ceil($("#" + id + " .invertedText").position().top);
	var heightOfHighlightedLine = $("#" + id + " .invertedText").height();
	var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));
	var heightOfBox = $("#" + id).height();

	var scrollBy = (topOfHighlightedLine + heightOfHighlightedLine) - (heightOfBox - heightOfUnhighlightedLine)
	if (scrollBy > 0)
	{
		// Try to keep at least one next line visible
		$("#" + id).scrollTop($("#" + id).scrollTop() + scrollBy);
	} // if
} // highlightNextLine

function highlightPreviousLine(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	unhighlightLine(id);

	var lines = splitIntoLines(id);

	if (highlightIndexes[id] === undefined) highlightIndexes[id] = lines.length - 1; else highlightIndexes[id]--;

	highlightLine(id);

	// Scroll along if necessary

	var topOfHighlightedLine = Math.ceil($("#" + id + " .invertedText").position().top);
	var heightOfHighlightedLine = $("#" + id + " .invertedText").height();
	var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));
	var heightOfBox = $("#" + id).height();

	if (topOfHighlightedLine < heightOfUnhighlightedLine - 1)
	{
		// Try to keep at least one previous line visible
		$("#" + id).scrollTop($("#" + id).scrollTop() - heightOfUnhighlightedLine);
	}
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
//
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
	clearTimeout(audioSettingsPopupShowTimer);
	clearTimeout(audioSettingsPopupHideTimer);
	updatingAudioVolume = false;
	isAudioMenuVisible = false;

	if (! $("#audio_settings_popup").is(":visible")) return false;
	hidePopup("audio_settings_popup");
	return true;
} // hideAudioSettingsPopup

function hideAudioSettingsPopupAfter(msec)
{
	// (Re-)arm the timer to hide the popup after the specified number of milliseconds
	clearTimeout(audioSettingsPopupHideTimer);
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
	clearTimeout(handleItemChange.tunerPresetsPopupTimer);
	handleItemChange.tunerPresetsPopupTimer = setTimeout(hideTunerPresetsPopup, 9000);
} // showTunerPresetsPopup

// -----
// Functions for selecting MFD language

var notDigitizedAreaText = "Not digitized area";
var cityCenterText = "City centre";
var changeText = "Change";
var noNumberText = "No number";
var stopGuidanceText = "Stop guidance";
var resumeGuidanceText = "Resume guidance";
var streetNotListedText = "Street not listed";

// Find the ticked button and select it
function languageSelectTickedButton()
{
	var foundTick = false;

	$("#set_language").find(".tickBox").each(
		function()
		{
			var isTicked = $(this).text().trim() !== "";
			$(this).toggleClass("buttonSelected", isTicked);
			if (isTicked) foundTick = true;
		}
	);

	// If nothing is ticked, retrieve from local store
	if (! foundTick)
	{
		var lang = localStorage.mfdLanguage;
		var id = "set_language_english";  // Default

		// Puppy powerrrrrr :-)
		if ($("#set_language").find(".tickBox").map(function() { return this.id; }).get().indexOf(lang) >= 0)
		{
			id = lang;
		}
		else
		{
			localStorage.mfdLanguage = id;
		} // if

		setTick(id);
		$("#" + id).addClass("buttonSelected");
	} // if

	$("#set_language_validate_button").removeClass("buttonSelected");
} // languageSelectTickedButton

function languageValidate()
{
	var newLanguage = $("#set_language").find(".tickBox").filter(function() { return $(this).text(); }).attr("id");
	setLanguage(newLanguage);
	webSocket.send("mfd_language:" + newLanguage);

	// TODO - the original MFD sometimes shows a popup when the language is changed

	exitMenu();
	exitMenu();
	exitMenu();
} // languageValidate

// -----
// Functions for selecting MFD formats and units

function unitsSelectTickedButtons()
{
	$("#set_units").find(".tickBox").removeClass("buttonSelected");

	var distUnit = localStorage.mfdDistanceUnit;
	var id = "set_units_km_h";  // Default

	if ($("#set_distance_unit").find(".tickBox").map(function() { return this.id; }).get().indexOf(distUnit) >= 0)
	{
		id = distUnit;
	}
	else
	{
		localStorage.mfdDistanceUnit = id;
	} // if

	setTick(id);
	$("#" + id).addClass("buttonSelected");

	var tempUnit = localStorage.mfdTemperatureUnit;
	var id = "set_units_deg_celsius";  // Default

	if ($("#set_temperature_unit").find(".tickBox").map(function() { return this.id; }).get().indexOf(tempUnit) >= 0)
	{
		id = tempUnit;
	}
	else
	{
		localStorage.mfdTemperatureUnit = id;
	} // if

	setTick(id);


	var timeUnit = localStorage.mfdTimeUnit;
	id = "set_units_24h";  // Default

	if ($("#set_time_unit").find(".tickBox").map(function() { return this.id; }).get().indexOf(timeUnit) >= 0)
	{
		id = timeUnit;
	}
	else
	{
		localStorage.mfdTimeUnit = id;
	} // if

	setTick(id);

	$("#set_units_validate_button").removeClass("buttonSelected");
} // unitsSelectTickedButtons

function unitsValidate()
{
	// Save selected ids in local (persistent) store
	localStorage.mfdDistanceUnit =
		$("#set_distance_unit").find(".tickBox").filter(function() { return $(this).text(); }).attr("id");
	localStorage.mfdTemperatureUnit =
		$("#set_temperature_unit").find(".tickBox").filter(function() { return $(this).text(); }).attr("id");
	localStorage.mfdTimeUnit =
		$("#set_time_unit").find(".tickBox").filter(function() { return $(this).text(); }).attr("id");

	setUnits(localStorage.mfdDistanceUnit, localStorage.mfdTemperatureUnit, localStorage.mfdTimeUnit);

	webSocket.send("mfd_distance_unit:" + localStorage.mfdDistanceUnit);
	webSocket.send("mfd_temperature_unit:" + localStorage.mfdTemperatureUnit);
	webSocket.send("mfd_time_unit:" + localStorage.mfdTimeUnit);

	exitMenu();
	exitMenu();
	exitMenu();
} // unitsValidate

// -----
// Functions for satellite navigation menu and screen handling

var satnavInitialized = false;

// Current sat nav mode, saved as last reported in item "satnav_status_2"
var satnavMode = "INITIALIZING";

var satnavRouteComputed = false;
var satnavDisclaimerAccepted = false;
var satnavLastEnteredChar = null;
var satnavToMfdResponse;
var satnavDestinationReachable = true;
var satnavGpsFix = false;
var satnavOnMap = false;
var satnavStatus1 = "";
var satnavDestinationAccessible = true;
var satnavComputingRoute = false;

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

	if (satnavStatus1.match(/NO_DISC/) || $("#satnav_disc_recognized").hasClass("ledOff"))
	{
		showStatusPopup(
			localStorage.mfdLanguage === "set_language_french" ? "Le CD-ROM de navigation<br />est absent" :
			localStorage.mfdLanguage === "set_language_german" ? "Die Navigations CD-ROM<br />fehlt" :
			localStorage.mfdLanguage === "set_language_spanish" ? "No hay CD-ROM de<br />navegaci&oacute;n" :
			localStorage.mfdLanguage === "set_language_italian" ? "Il CD-ROM di navigazione<br />&egrave; assente" :
			localStorage.mfdLanguage === "set_language_dutch" ? "De navigatie CD-rom is<br />niet aanwezig" :
			"Navigation CD-ROM is<br />missing",
			8000
		);
		return;
	} // if

	if (satnavStatus1.match(/DISC_UNREADABLE/))
	{
		showStatusPopup(
			localStorage.mfdLanguage === "set_language_french" ? "Le CD-ROM de navigation<br />est illisible" :
			localStorage.mfdLanguage === "set_language_german" ? "Die Navigations CD-ROM<br />is unleserlich" :
			localStorage.mfdLanguage === "set_language_spanish" ? "No se puede leer<br />CD-ROM de navegaci&oacute;n" :
			localStorage.mfdLanguage === "set_language_italian" ? "Il CD-ROM di navigazione<br />&egrave; illeggibile" :
			localStorage.mfdLanguage === "set_language_dutch" ? "De navigatie CD-rom is<br />onleesbaar" :
			"Navigation CD-ROM<br />is unreadable",
			10000
		);

		return;
	} // if

	// Change to the sat nav main menu with an exit to the general main manu
	menuStack = [ "main_menu" ];
	currentMenu = "satnav_main_menu";
	changeLargeScreenTo(currentMenu);

	selectFirstMenuItem(currentMenu);  // Select the top button

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

	if (handleItemChange.mfdToSatnavRequestType === "REQ_ITEMS"
		|| handleItemChange.mfdToSatnavRequestType === "REQ_N_ITEMS")
	{
		switch(handleItemChange.mfdToSatnavRequest)
		{
			// Pre-fill with what we previously received

			case "service":
				$("#satnav_choice_list").html(satnavServices.join('<br />'));
				break;

			case "personal_address_list":
				$("#satnav_choice_list").html(satnavPersonalDirectoryEntries.join('<br />'));
				break;

			case "professional_address_list":
				$("#satnav_choice_list").html(satnavProfessionalDirectoryEntries.join('<br />'));
				break;

			case "enter_city":
			case "enter_street":
				$("#satnav_choice_list").empty();
				break;
		} // switch
	} // if

	if ($("#satnav_choice_list").text() === "")
	{
		// Show the spinning disc after a while
		clearTimeout(satnavGotoListScreen.showSpinningDiscTimer);
		satnavGotoListScreen.showSpinningDiscTimer = setTimeout
		(
			function () { $("#satnav_choose_from_list_spinning_disc").show(); },
			3500
		);
	} // if

	// We could get here via "Esc" and then the currently selected line must be highlighted
	highlightLine("satnav_choice_list");
} // satnavGotoListScreen

function satnavGotoListScreenEmpty()
{
	satnavGotoListScreen();
	$("#satnav_choice_list").empty();
	highlightFirstLine("satnav_choice_list");
} // satnavGotoListScreenEmpty

function satnavGotoListScreenServiceList()
{
	handleItemChange.mfdToSatnavRequestType = "REQ_N_ITEMS";
	handleItemChange.mfdToSatnavRequest = "service";
	satnavGotoListScreen();
	highlightFirstLine("satnav_choice_list");
} // satnavGotoListScreenServiceList

function satnavGotoListScreenPersonalAddressList()
{
	handleItemChange.mfdToSatnavRequestType = "REQ_N_ITEMS";
	handleItemChange.mfdToSatnavRequest = "personal_address_list";
	satnavGotoListScreen();
	highlightFirstLine("satnav_choice_list");
} // satnavGotoListScreenPersonalAddressList

function satnavGotoListScreenProfessionalAddressList()
{
	handleItemChange.mfdToSatnavRequestType = "REQ_N_ITEMS";
	handleItemChange.mfdToSatnavRequest = "professional_address_list";
	satnavGotoListScreen();
	highlightFirstLine("satnav_choice_list");
} // satnavGotoListScreenProfessionalAddressList

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

function satnavEnterStreetCharactersScreen()
{
	highlightFirstLine("satnav_choice_list");
	$("#satnav_to_mfd_show_characters_spinning_disc").hide();
	$("#satnav_enter_characters_validate_button").addClass("buttonDisabled");
} // satnavEnterStreetCharactersScreen

var satnavAvailableCharactersStack = [];

// Puts the "Enter city" screen into the mode where the user simply has to confirm the current destination city
function satnavConfirmCityMode()
{
	// Clear the character-by-character entry string
	$("#satnav_entered_string").empty();
	satnavLastEnteredChar = null;

	satnavAvailableCharactersStack = [];

	// No entered characters, so disable the "Correction" button
	$("#satnav_enter_characters_correction_button").addClass("buttonDisabled");

	$("#satnav_current_destination_city").show();

	// If a current destination city is known, enable the "Validate" button
	$("#satnav_enter_characters_validate_button").toggleClass(
		"buttonDisabled",
		$("#satnav_current_destination_city").text() === ""
	);

	// Set button text
	$("#satnav_enter_characters_change_or_city_center_button").text(changeText);

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
	$("#satnav_enter_characters_change_or_city_center_button").text(
		currentMenu === "satnav_enter_street_characters" ? cityCenterText : changeText);

	satnavSelectFirstAvailableCharacter();
} // satnavEnterByLetterMode

var userHadOpportunityToEnterStreet = false;

// Puts the "Enter street" screen into the mode where the user simply has to confirm the current destination street
function satnavConfirmStreetMode()
{
	// Clear the character-by-character entry string
	$("#satnav_entered_string").empty();
	satnavLastEnteredChar = null;

	satnavAvailableCharactersStack = [];

	// No entered characters, so disable the "Correction" button
	$("#satnav_enter_characters_correction_button").addClass("buttonDisabled");

	$("#satnav_current_destination_street").show();

	// If a current destination street is known, enable the "Validate" button
	$("#satnav_enter_characters_validate_button").toggleClass(
		"buttonDisabled",
		$("#satnav_current_destination_street").text() === ""
	);

	// Set button text
	$("#satnav_enter_characters_change_or_city_center_button").text(cityCenterText);

	satnavSelectFirstAvailableCharacter();

	userHadOpportunityToEnterStreet = false;
} // satnavConfirmStreetMode

// Toggle between character-by-character entry mode, and confirming the current destination city
function satnavToggleCityEntryMode()
{
	if ($("#satnav_current_destination_city").is(":visible")) satnavEnterByLetterMode(); else satnavConfirmCityMode();
} // satnavToggleCityEntryMode

// The right-most button in the "Enter city/street" screens is either "Change" (for "Enter city") or
// "City centre" for ("Enter street"). Depending on the button text, perform the appropriate action.
function satnavEnterCharactersChangeOrCityCenterButtonPress()
{
	if ($("#satnav_enter_characters_change_or_city_center_button").text().replace(/\s*/g, "") === changeText)
	{
		// Entering city: toggle to the entry mode
		satnavToggleCityEntryMode();
	}
	else
	{
		// Entering street and chosen "City centre": directly go to the "Programmed destination" screen
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
	$("#satnav_current_destination_city").empty();

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
// "City centre" for ("Enter street"). Depending on the button text, perform the appropriate action.
function satnavGotoEnterStreetOrNumber()
{
	if ($("#satnav_enter_characters_change_or_city_center_button").text().replace(/\s*/g, "") === changeText)
	{
		gotoMenu("satnav_enter_street_characters");
		satnavConfirmStreetMode();
	}
	else
	{
		if ($("#satnav_current_destination_street").text() === "") satnavGotoListScreen();
		else gotoMenu("satnav_current_destination_house_number");
	} // if
} // satnavGotoEnterStreetOrNumber

// Scrape the entered character from the screen (from the DOM). Used as a last-resort if the
// "mfd_to_satnav_enter_character" packet is missed.
function satnavGrabSelectedCharacter()
{
	if (currentMenu !== "satnav_enter_city_characters" && currentMenu !== "satnav_enter_street_characters") return;

	var selectedChar = $("#satnav_to_mfd_show_characters_line_1 .invertedText").text();
	if (! selectedChar) selectedChar = $("#satnav_to_mfd_show_characters_line_2 .invertedText").text();
	if (selectedChar) satnavLastEnteredChar = selectedChar;
} // satnavGrabSelectedCharacter

function satnavEnterCharacter()
{
	if (satnavLastEnteredChar === null) return;

	// The user is pressing "Val" on the remote control, to enter the chosen character.
	// Or the MFD itself is confirming the character, if there is only one character to choose from.

	$("#satnav_entered_string").append(satnavLastEnteredChar);  // Append the entered character
	$("#satnav_enter_characters_correction_button").removeClass("buttonDisabled");  // Enable the "Correction" button
	$("#satnav_enter_characters_validate_button").addClass("buttonDisabled");  // Disable the "Validate" button
	highlightFirstLine("satnav_choice_list");  // Go to the first line in the "satnav_choice_list" screen
} // satnavEnterCharacter

var showAvailableCharactersTimer = null;

// Handler for the "Correction" button in the "satnav_enter_characters" screen.
// Removes the last entered character.
function satnavRemoveEnteredCharacter()
{
	var currentText = $("#satnav_entered_string").text();
	currentText = currentText.slice(0, -1);
	$("#satnav_entered_string").text(currentText);

	//satnavLastEnteredChar = null;

	// Just in case we miss the "satnav_to_mfd_show_characters" packet
	if (satnavAvailableCharactersStack.length >= 2)
	{
		var availableCharacters = satnavAvailableCharactersStack[satnavAvailableCharactersStack.length - 2];

		clearTimeout(showAvailableCharactersTimer);
		showAvailableCharactersTimer = setTimeout
		(
			function () { writeToDom({"satnav_to_mfd_show_characters":availableCharacters}); },
			1500
		);
	} // if

	// If the entered string has become empty, disable the "Correction" button
	$("#satnav_enter_characters_correction_button").toggleClass("buttonDisabled", currentText.length === 0);

	// Jump back to the characters
	if (currentText.length === 0) satnavSelectFirstAvailableCharacter();
} // satnavRemoveEnteredCharacter

// Boolean to indicate if the user has pressed 'Esc' within a list of cities or streets; in that case the entered
// characters are removed until there is more than one character to choose from.
var satnavRollingBackEntryByCharacter = false;

// State while entering sat nav destination in character-by-character mode
// 0 = "mfd_to_satnav_instruction=Val" packet received
// 1 = "satnav_to_mfd_show_characters" packet received
// 2 = "mfd_to_satnav_enter_character" packet received
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
	// then add "City centre" at the top of the choice list
	if (
		$("#satnav_choose_from_list").is(":visible")
		&& handleItemChange.mfdToSatnavRequest === "enter_street"
		//&& $("#satnav_entered_string").text() === ""
		&& ! userHadOpportunityToEnterStreet
	   )
	{
		$("#satnav_choice_list").html(cityCenterText + "<br />" + $("#satnav_choice_list").html());
	} // if
} // satnavCheckIfCityCenterMustBeAdded

function satnavListItemClicked()
{
	// Clear the character-by-character entry string
	$("#satnav_entered_string").empty();

	// When choosing a city from the list, hide the current destination street, so that the entry format ("") will
	// be shown.
	if (handleItemChange.mfdToSatnavRequest === "enter_city") $("#satnav_current_destination_street").empty();

	// When choosing a city or street from the list, hide the current destination house number, so that the
	// entry format ("_ _ _") will be shown.
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
		showStatusPopup(
			localStorage.mfdLanguage === "set_language_french" ? "Ce num&eacute;ro n'est<br />pas r&eacute;pertori&eacute;<br />dans cette voie" :
			localStorage.mfdLanguage === "set_language_german" ? "Die Nummer ist<br />f&uuml;r diese Stra&szlig;e<br />nicht eingetragen" :
			localStorage.mfdLanguage === "set_language_spanish" ? "N&uacute;mero no<br />identificado<br />para esta v&iacute;a" :
			localStorage.mfdLanguage === "set_language_italian" ? "Questo numero<br />non &egrave; indicato<br />in questa via" :
			localStorage.mfdLanguage === "set_language_dutch" ? "Straatnummer onbekend" :
			"This number is<br />not listed for<br />this street",
			7000);
		return;
	} // if

	// Already copy the house number into the "satnav_show_current_destination" screen, in case the
	// "satnav_report" packet is missed
	$("#satnav_current_destination_house_number_shown").html(enteredNumber === "" ? noNumberText : enteredNumber);

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
	if (selectedEntry === 1) $("#satnav_service_address_previous_button").removeClass("buttonSelected");
	if (selectedEntry === lastEntry) $("#satnav_service_address_next_button").removeClass("buttonSelected");

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
	if (newEntryName === "") return;
	satnavPersonalDirectoryEntries.push(newEntryName);

	// Save in local (persistent) store
	localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);

	showPopup("satnav_input_stored_in_personal_dir_popup", 7000);
} // satnavArchiveInDirectoryCreatePersonalEntry

function satnavArchiveInDirectoryCreateProfessionalEntry()
{
	// Add the new entry to the list of known entries
	var newEntryName = $("#satnav_archive_in_directory_entry").text().replace(/-+$/, "");
	if (newEntryName === "") return;
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
	$(".satNavEntryExistsTag" ).toggle(entryExists);
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

function satnavSwitchToGuidanceScreen()
{
	hidePopup("satnav_computing_route_popup");
	menuStack = [];
	currentMenu = "satnav_guidance";
	changeLargeScreenTo(currentMenu);
} // satnavSwitchToGuidanceScreen

function satnavGuidanceSetPreference(value)
{
	if (typeof value === "undefined" || value === "---") return;

	// Copy the correct text into the sat nav guidance preference popup
	var satnavGuidancePreferenceText =
		value === "FASTEST_ROUTE" ? $("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").text() :
		value === "SHORTEST_DISTANCE" ? $("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").text() :
		value === "AVOID_HIGHWAY" ? $("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").text() :
		value === "COMPROMISE_FAST_SHORT" ? $("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").text() :
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
		setTick("satnav_guidance_preference_fastest");
		$("#satnav_guidance_preference_fastest").addClass("buttonSelected");
	} // if

	$("#satnav_guidance_preference_menu_validate_button").removeClass("buttonSelected");
} // satnavGuidancePreferenceSelectTickedButton

function satnavGuidancePreferencePopupYesButton()
{
	hidePopup('satnav_guidance_preference_popup');
	if (! $('#satnav_guidance').is(':visible')) satnavCalculatingRoute();
} // satnavGuidancePreferencePopupYesButton

function satnavGuidancePreferencePopupNoButton()
{
	hidePopup('satnav_guidance_preference_popup');
	hidePopup('satnav_computing_route_popup');
	changeLargeScreenTo('satnav_guidance_preference_menu');
	satnavGuidancePreferenceSelectTickedButton();
} // satnavGuidancePreferencePopupNoButton

function satnavGuidancePreferenceValidate()
{
	if (currentMenu === "satnav_guidance")
	{
		// Return to the guidance screen (bit clumsy)
		exitMenu();
		if (satnavComputingRoute) satnavCalculatingRoute(); else showDestinationNotAccessiblePopupIfApplicable();
	}
	else
	{
		satnavSwitchToGuidanceScreen();
	} // if
} // satnavGuidancePreferenceValidate

function satnavCalculatingRoute()
{
	localStorage.askForGuidanceContinuation = "NO";

	// Don't pop up in the guidance preference screen
	if (currentLargeScreenId === "satnav_guidance_preference_menu") return;

	// No popups while driving. Note: original MFD does seem to show this popup (in some cases) during driving.
	if (satnavVehicleMoving()) return;

	// If the result of the calculation is "Destination is not accessible by road", show that popup once, at the
	// start, but not any more during the guidance.
	satnavDestinationNotAccessibleByRoadPopupShown = false;

	showPopupAndNotifyServer("satnav_computing_route_popup", 30000);
} // satnavCalculatingRoute

// Show the "Destination is not accessible by road" popup, if applicable
function showDestinationNotAccessiblePopupIfApplicable()
{
	if (satnavDestinationAccessible) return false;

	// Don't show this popup while still in the guidance preference menu
	if ($("#satnav_guidance_preference_menu").is(":visible")) return false;

	// Show this popup only once at start of guidance or after recalculation
	if (satnavDestinationNotAccessibleByRoadPopupShown) return true;

	// But only if the current location is known (to emulate behaviour of MFD)
	//if (satnavCurrentStreet !== "")
	//if (satnavOnMap)
	if (satnavGpsFix)
	{
		hidePopup();
		showStatusPopup(
			localStorage.mfdLanguage === "set_language_french" ? "La destination n'est<br />pas accessible par<br />voie routi&egrave;re" :
			localStorage.mfdLanguage === "set_language_german" ? "Das Ziel ist<br />per Stra%szlig;e nicht<br />zu erreichen" :
			localStorage.mfdLanguage === "set_language_spanish" ? "Destino inaccesible<br />por carratera" :
			localStorage.mfdLanguage === "set_language_italian" ? "La destinazione non<br />&egrave; accessibile<br />mediante strada" :
			localStorage.mfdLanguage === "set_language_dutch" ? "De bestemming is niet<br />via de weg bereikbaar" :
			"Destination is not<br />accessible by road",
			8000
		);
	} // if

	satnavDestinationNotAccessibleByRoadPopupShown = true;

	return true;
} // showDestinationNotAccessiblePopupIfApplicable

function showOrTimeoutDestinationNotAccessiblePopup()
{
	if (showDestinationNotAccessiblePopupIfApplicable()) return;

	// The popup may be displayed during the next 10 seconds, not any more after that
	clearTimeout(showOrTimeoutDestinationNotAccessiblePopup.timer);
	showOrTimeoutDestinationNotAccessiblePopup.timer = setTimeout
	(
		function () { satnavDestinationNotAccessibleByRoadPopupShown = true; },
		10000
	);
} // showOrTimeoutDestinationNotAccessiblePopup 

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
	else if (unit === "yd" && +distance >= 880)
	{
		// Reported in yards, and 880 yards or more: show in miles, with decimal notation. Round upwards.
		return [ ((+distance + 87) / 1760).toFixed(1), "mi" ];  // Original MFD shows "ml"
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
		clearTimeout(suppressScreenChangeToAudio);
		suppressScreenChangeToAudio = setTimeout(function () { suppressScreenChangeToAudio = null; }, 400);
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
		clearTimeout(suppressScreenChangeToAudio);
		suppressScreenChangeToAudio = setTimeout(function () { suppressScreenChangeToAudio = null; }, 400);
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

function satnavStopGuidance()
{
	localStorage.askForGuidanceContinuation = 'NO';
	selectDefaultScreen();
} // satnavStopGuidance

function satnavStopOrResumeGuidance()
{
	if ($("#satnav_navigation_options_menu_stop_guidance_button").html() === stopGuidanceText) 
	{
		satnavStopGuidance();
	}
	else
	{
		satnavSwitchToGuidanceScreen();
	} // if
} // satnavStopOrResumeGuidance

function satnavPoweringOff()
{
	// If in guidance mode while turning off contact key, remember to show popup
	// "Continue guidance to destination?" the next time the contact key is turned "ON"
	if (satnavMode === "IN_GUIDANCE_MODE")
	{
		localStorage.askForGuidanceContinuation = "YES";
		webSocket.send("ask_for_guidance_continuation:YES");
	} // if

	satnavInitialized = false;
	$("#satnav_disc_recognized").addClass("ledOff");
	handleItemChange.nSatNavDiscUnreadable = 1;
	satnavDisclaimerAccepted = false;
	satnavDestinationNotAccessibleByRoadPopupShown = false;
} // function

function showPowerSavePopup()
{
	showNotificationPopup(
		localStorage.mfdLanguage === "set_language_german" ? "Wechsel in<br />Energiesparmodus" :
		localStorage.mfdLanguage === "set_language_dutch" ? "Omschakeling naar<br />energiebesparingsmodus" :
		"Changing to<br />power-save mode",
		15000);
} // showPowerSavePopup

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

var isDoorOpen = {};  // Associative array, using the door ID as key, mapping to true (open) or false (closed)
var isBootOpen = false;

var doorOpenText = "Door open";
var doorsOpenText = "Doors open";
var bootOpenText = "Boot open";
var bootAndDoorOpenText = "Boot and door open";
var bootAndDoorsOpenText = "Boot and doors open";

// -----
// Handling of changed items

function handleItemChange(item, value)
{
	switch(item)
	{
		case "volume_update":
		{
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

			// In the audio settings popup, unhighlight any audio setting
			$("#" + highlightIds[audioSettingHighlightIndex]).hide();

			// Don't pop up when user is browsing the menus
			if (inMenu()) break;

			if ($("#contact_key_position").text() != "" && $("#contact_key_position").text() !== "OFF")
			{
				// Show the audio settings popup
				$("#audio_settings_popup").show();
			}
			else
			{
				// With no contact key in place, we get a spurious "volume_update" message when turning off the
				// head unit. To prevent the audio settings popup to flash up and disappear, show it only after
				// 100 msec in this specific situation.
				clearTimeout(audioSettingsPopupShowTimer);
				audioSettingsPopupShowTimer = setTimeout(function () { $("#audio_settings_popup").show(); }, 100);
			} // if
			updatingAudioVolume = true;

			// Hide popup after 4 seconds
			hideAudioSettingsPopupAfter(4000);
		} // case
		break;

		case "audio_menu":
		{
			if (value !== "CLOSED") break;

			// Bug in original MFD: if source is cd-changer, audio_menu is always "OPEN"... So ignore for now.

			// Ignore when audio volume is being set
			if (updatingAudioVolume) break;

			// Hide the audio popup now
			hideAudioSettingsPopup();
		} // case
		break;

		case "head_unit_button_pressed":
		{
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
					showStatusPopup("No CD", 3000);  // Show "No CD" popup for a few seconds
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
					showStatusPopup("No CD", 3000);  // Show "No CD" popup for a few seconds
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
				if (inMenu()) break;  // No screen change while browsing the menus
				selectDefaultScreen(value);
				break;
			} // if

			// Check for button press of "1" ... "6" (set tuner preset, or select disc on CD changer).
			// Ignore variants like "1 (released)" or "5 (held)".
			if (! value.match(/^[1-6]$/)) break;

			if (audioSource === "CD_CHANGER")
			{
				if (currentLargeScreenId === "satnav_guidance") showAudioPopup();

				var selector = "#cd_changer_disc_" + value + "_present";
				if ($(selector).hasClass("ledOn")) break;

				// Show "NO CD X" text for a few seconds
				$("#cd_changer_current_disc").hide();
				$("#cd_changer_disc_not_present").show();
				$("#cd_changer_selected_disc").text(value);
				$("#cd_changer_selected_disc").show();

				// After 5 seconds, go back to showing the CD currently playing
				clearTimeout(handleItemChange.noCdPresentTimer);
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

			if (currentLargeScreenId === "satnav_guidance") showAudioPopup();
		} // case
		break;

		case "tape_status":
		{
			// Has anything changed?
			if (value === handleItemChange.tapeStatus) break;
			handleItemChange.tapeStatus = value;

			hideAudioSettingsPopup();

			if (currentLargeScreenId === "satnav_guidance") showAudioPopup();
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

			if (currentLargeScreenId === "satnav_guidance") showAudioPopup();
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

			// Un-highlight any previous entry in the "tuner_presets_popup"
			$('div[id^=presets_memory_][id$=_select]').hide()

			// Make sure the audio settings popup is hidden
			hideAudioSettingsPopup();

			// Changed to a non-preset frequency? Or just changed audio source? Then suppress the tuner presets popup.
			if (value === "-" || handleItemChange.hideHeadUnitPopupsTimer != null)
			{
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

			hideAudioSettingsPopup();
			hideTunerPresetsPopup();

			if (currentLargeScreenId === "satnav_guidance") showAudioPopup();

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
				if (currentLargeScreenId === "satnav_guidance") showAudioPopup();
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
				if (currentLargeScreenId === "satnav_guidance") showAudioPopup();
			} // if
		} // case
		break;

		case "cd_changer_current_disc":
		{
			// Has anything changed?
			if (value === cdChangerCurrentDisc) break;

			if (currentLargeScreenId === "satnav_guidance") showAudioPopup();

			if (cdChangerCurrentDisc != null && cdChangerCurrentDisc.match(/^[1-6]$/))
			{
				var selector = "#cd_changer_disc_" + cdChangerCurrentDisc + "_present";
				$(selector).removeClass("ledActive");
				$(selector).css({marginTop: '+=25px'});
			} // if

			// Only if valid value (can be "--" if cartridge is not present)
			if (value.match(/^[1-6]$/))
			{
				// The LED of the current disc is shown a bit larger than the others
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

			// When in a menu, a change in the audio source does not imply any further action
			if (inMenu()) break;

			// Suppress timer still running?
			if (suppressScreenChangeToAudio != null) break;

			// Hide the audio settings or tuner presets popup (if visible)
			hideAudioSettingsPopup();
			hideTunerPresetsPopup();

			// Suppress the audio settings popup for the next 0.4 seconds (bit clumsy, but effective)
			clearTimeout(handleItemChange.hideHeadUnitPopupsTimer);
			handleItemChange.hideHeadUnitPopupsTimer = setTimeout
			(
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
			engineRpm = parseInt(value) || 0;
			if (value === "---") break;

			// If more than 3500 rpm or less than 500 rpm (but > 0), add glow effect

			// Threshold of 3500 rpm is lowered with engine coolant temperature, but always at least 1700 rpm
			var thresholdRpm = 3500;
			if (engineCoolantTemperature < 80) thresholdRpm = 3500 - (80 - engineCoolantTemperature) * 30;
			if (thresholdRpm < 1700) thresholdRpm = 1700;

			$('[gid="engine_rpm"]').toggleClass("glow",
				(parseInt(value) < 500 && parseInt(value) > 0) || parseInt(value) > thresholdRpm);

			if (contactKeyPosition === "START" && parseInt(value) > 150)
			{
				changeToInstrumentsScreen();
			} // if
		} // case
		break;

		case "vehicle_speed":
		{
			if (value === "--") break;

			vehicleSpeed = parseInt(value);

			var isDriving = vehicleSpeed >= 5;
			var isDrivingFast = vehicleSpeed >= 130;

			if (localStorage.mfdDistanceUnit === "set_units_mph")
			{
				isDriving = vehicleSpeed >= 3;
				isDrivingFast = vehicleSpeed >= 85;
			} // if

			// If 130 km/h (85 mph) or more, add glow effect
			$("#oil_level_raw").toggleClass("glow", isDrivingFast);

			if (icyConditions && ! wasRiskOfIceWarningShown && isDriving)
			{
				// Show warning popup
				showNotificationPopup("Risk of ice!", 10000);
				wasRiskOfIceWarningShown = true;
			} // if
		} // case
		break;

		case "fuel_level_filtered":
		{
			var level = parseFloat(value);

			var lowFuelCondition = false;
			if (localStorage.mfdDistanceUnit === "set_units_mph") lowFuelCondition = level <= 1;  // 1 gallon left
			else lowFuelCondition = level <= 5;  // 5 litres left

			$('[gid="fuel_level_filtered"]').toggleClass("glow", lowFuelCondition);
		} // case
		break;

		case "coolant_temp":
		{
			var temp = parseFloat(value);

			if (localStorage.mfdTemperatureUnit === "set_units_deg_fahrenheit")
			{
				// If more than 230 degrees, add glow effect
				$("#coolant_temp").toggleClass("glow", temp > 230);

				// If less than 160 degrees, add "ice glow" effect
				$("#coolant_temp").toggleClass("glowIce", temp < 160);
			}
			else
			{
				// If more than 110 degrees, add glow effect
				$("#coolant_temp").toggleClass("glow", temp > 110);

				// If less than 70, add "ice glow" effect
				$("#coolant_temp").toggleClass("glowIce", temp < 70);
			} // if
		} // case
		break;

		case "distance_to_empty":
		{
			var dist = parseFloat(value);

			if (localStorage.mfdDistanceUnit === "set_units_mph")
			{
				// If less than 60 miles, add glow effect
				$('[gid="distance_to_empty"]').toggleClass("glow", dist < 60);
			}
			else
			{
				// If less than 90 kms, add glow effect
				$('[gid="distance_to_empty"]').toggleClass("glow", dist < 90);
			} // if
		} // case
		break;

		case "exterior_temp":
		{
			$("#splash_text").hide();

			var temp = parseFloat(value);

			if (localStorage.mfdTemperatureUnit === "set_units_deg_fahrenheit") 
			{
				// Apply hysteresis
				if (! icyConditions && temp >= 26 && temp <= 38) icyConditions = true;
				else if (icyConditions && (temp <= 24 || temp >= 40)) icyConditions = false;
			}
			else
			{
				// Apply hysteresis
				if (! icyConditions && temp >= -3.0 && temp <= 3.0) icyConditions = true;
				else if (icyConditions && (temp <= -4.0 || temp >= 4.0)) icyConditions = false;
			} // if

			$('[gid="exterior_temp_shown"]').html(
				localStorage.mfdTemperatureUnit === "set_units_deg_fahrenheit" ?
					value + " &deg;F" : value + " &deg;C"
				);

			// If icy conditions, add "ice glow" effect
			$('[gid="exterior_temp_shown"]').toggleClass("glowIce", icyConditions);

			if (icyConditions)
			{
				if (! wasRiskOfIceWarningShown && vehicleSpeed >= 5)
				{
					// Show warning popup
					showNotificationPopup("Risk of ice!", 10000);
					wasRiskOfIceWarningShown = true;
				} // if
			}
			else
			{
				wasRiskOfIceWarningShown = false;
			} // if
		} // case
		break;

		case "oil_level_raw":
		{
			// If very low, add glow effect
			$("#oil_level_raw").toggleClass("glow", parseInt(value) <= 11);
		} // case
		break;

		case "distance_to_service":
		{
			$("#distance_to_service").text(addThousandsSeparator(value));  // Improve readability

			// If zero or negative, add glow effect
			$("#distance_to_service").toggleClass("glow", parseInt(value) <= 0);
		} // case
		break;

		case "odometer_1":
		{
			$("#odometer_1").text(addThousandsSeparator(value));  // Improve readability
		} // case
		break;

		case "economy_mode":
		{
			// Check if anything actually changed
			if (value === economyMode) break;
			economyMode = value;

			if (value !== "ON") break;
			if (currentLargeScreenId === "pre_flight") break;

			showPowerSavePopup();
		} // case
		break;

		case "hazard_lights":
		{
			$("#hazard_lights").removeClass("ledOn");
			$("#hazard_lights").removeClass("ledOff");
			$("#hazard_lights").toggleClass("glow", value === "ON");  // If on, add glow effect
		} // case
		break;

		case "door_open":
		{
			// Note: If the system is in power save mode, "door_open" always reports "NO", even if a door is open.
			// That situation is handled in the next case clause, below.

			// If on, add glow effect
			$("#door_open").removeClass("ledOn");
			$("#door_open").removeClass("ledOff");
			$("#door_open").toggleClass("glow", value === "YES");

			// Show or hide the "door open" popup, but not in the "pre_flight" screen
			if (value === "YES" && currentLargeScreenId !== "pre_flight")
			{
				$("#door_open_popup_text").html(doorOpenText);
				showPopupAndNotifyServer("door_open_popup", 8000);
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

			if (! change) break;  // Only continue if anything changed

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
			var popupText =
				isBootOpen && nDoorsOpen > 1 ? bootAndDoorsOpenText :
				isBootOpen && nDoorsOpen == 1 ? bootAndDoorOpenText :
				isBootOpen && nDoorsOpen == 0 ? bootOpenText :
				nDoorsOpen > 1 ? doorsOpenText :
				doorOpenText;

			// Set the correct text and show the "door open" popup
			$("#door_open_popup_text").html(popupText);
			showPopupAndNotifyServer("door_open_popup", 8000);
		} // case
		break;

		case "engine_running":
		{
			// Has anything changed?
			if (value === engineRunning) return;
			engineRunning = value;

			// Engine just started?
			if (engineRunning === "YES") changeToInstrumentsScreen();

			// If engine just stopped, and currently in "instruments" screen, then switch to default screen
			if (engineRunning === "NO" && $("#instruments").is(":visible")) selectDefaultScreen();
		} // case
		break;

		case "contact_key_position":
		{
			// Has anything changed?
			if (value === contactKeyPosition) break;
			contactKeyPosition = value;

			// On engine START, add glow effect
			$("#contact_key_position").toggleClass("glow", value === "START");

			switch(value)
			{
				case "ON":
				{
					clearTimeout(handleItemChange.contactKeyOffTimer);

					if (inMenu()) break;

					// Show "Pre-flight checks" screen if contact key is in "ON" position, without engine running
					if (engineRunning === "NO" && engineRpm === 0)
					{
						changeLargeScreenTo("pre_flight");

						// Hide all popups except "satnav_continue_guidance_popup"
						hideAllPopups("satnav_continue_guidance_popup");
					} // if

					if (satnavInitialized && localStorage.askForGuidanceContinuation === "YES" && economyMode === "OFF")
					{
						// Show popup "Continue guidance to destination? [Yes]/No"
						showPopup("satnav_continue_guidance_popup", 15000);

						localStorage.askForGuidanceContinuation = "NO";
					} // if
				} // case
				break;

				case "START":
				{
					clearTimeout(handleItemChange.contactKeyOffTimer);
					wasRiskOfIceWarningShown = false;
				} // case
				break;

				case "ACC":
				{
					clearTimeout(handleItemChange.contactKeyOffTimer);
				} // case
				break;

				case "OFF":
				{
					// "OFF" position can be very short between any of the other positions, so first wait a bit
					clearTimeout(handleItemChange.contactKeyOffTimer);
					handleItemChange.contactKeyOffTimer = setTimeout
					(
						function ()
						{
							handleItemChange.contactKeyOffTimer = null;
							selectDefaultScreen();  // Even when in a menu: the original MFD switches off
						},
						500
					);
				} // case
				break;
			} // switch
		} // case
		break;

		case "notification_message_on_mfd":
		{
			if (value === "") break;

			var isWarning = value.slice(-1) === "!";
			var message = isWarning ? value.slice(0, -1) : value;
			showNotificationPopup(message, 8000, isWarning);
		} // case
		break;

		case "satnav_status_1":
		{
			satnavStatus1 = value;

			// No further handling when in power-save mode
			if (economyMode === "ON") break;

			if (satnavStatus1.match(/ARRIVED_AT_DESTINATION_AUDIO/))
			{
				showPopup("satnav_reached_destination_popup", 10000);
			} // if

			if (satnavStatus1.match(/STOPPING_GUIDANCE/)
				// "0 m", "0 yd": we've reached the destination
				&& handleItemChange.satnavDistanceToDestViaRoad.match(/^0\s/)
				)
			{
				showPopup("satnav_reached_destination_popup", 10000);
			} // if

			if (satnavStatus1.match(/AUDIO/))
			{
				// Turn on or off "satnav_audio" LED
				var playingAudio = satnavStatus1.match(/START/) !== null;
				$("#satnav_audio").toggleClass("ledOn", playingAudio);
				$("#satnav_audio").toggleClass("ledOff", ! playingAudio);

				clearTimeout(handleItemChange.showSatnavAudioLed);
				if (playingAudio)
				{
					// Set timeout on LED, in case the "AUDIO OFF" packet is missed
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
		} // case
		break;

		case "satnav_status_2":
		{
			if (value === "") break;  // Only if valid

			// Has anything changed?
			if (value === satnavMode) break;
			var previousSatnavMode = satnavMode;
			satnavMode = value;

			// In case the ESP boots after the moment the sat nav has reported its "satnav_system_id"
			// (i.e., reports it is initialized)
			satnavInitialized = value !== "INITIALIZING";
			if (satnavInitialized) hidePopup("satnav_initializing_popup");

			// Button texts in menus
			$("#satnav_navigation_options_menu_stop_guidance_button").html(
				value === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);
			// $("#satnav_tools_menu_stop_guidance_button").text(
				// value === "IN_GUIDANCE_MODE" ? "Stop guidance" : "Resume guidance");

			// Just entered guidance mode?
			if (value === "IN_GUIDANCE_MODE")
			{
				// Reset the trip computer popup contents
				resetTripComputerPopup();

				if ($("#satnav_guidance_preference_menu").is(":visible")) break;
				satnavSwitchToGuidanceScreen();

				break;
			} // if

			// Just left guidance mode?
			if (previousSatnavMode === "IN_GUIDANCE_MODE")
			{
				if ($("#satnav_guidance").is(":visible")) selectDefaultScreen();
			} // if

			if (value === "IDLE")
			{
				satnavRouteComputed = false;
			} // if
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
			else if (value === "COMPUTING_ROUTE")
			{
				satnavComputingRoute = true;
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
			if (value.match(/COMPUTING_ROUTE/) && value.match(/DISC_PRESENT/))
			{
				satnavComputingRoute = true;
				satnavCalculatingRoute();
			}
			else
			{
				satnavComputingRoute = false;
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

			if (satnavMode === "IN_GUIDANCE_MODE")
			{
				if ($("#satnav_guidance").is(":visible"))
				{
					if (! satnavRouteComputed)
					{
						// TODO - check translations from English
						$("#satnav_guidance_next_street").html(
							localStorage.mfdLanguage === "set_language_french" ? "Chercher instruction" :
							localStorage.mfdLanguage === "set_language_german" ? "Anweisung suchen" :
							localStorage.mfdLanguage === "set_language_spanish" ? "Buscar instrucci&oacute;n" :
							localStorage.mfdLanguage === "set_language_italian" ? "Cercare istruzione" :
							localStorage.mfdLanguage === "set_language_dutch" ? "Opzoeken instructie" :
							"Retrieving next instruction"
						);
					}
				}
				else
				{
					// Don't change screen while driving or while the guidance preference menu is showing
					if (inMenu() && ! $("#satnav_guidance_preference_menu").is(":visible"))
					{
						satnavSwitchToGuidanceScreen();
					} // if
				} // if
			} // if
		} // case
		break;

		case "satnav_destination_reachable":
		{
			satnavDestinationReachable = value === "YES";
		} // case
		break;

		case "satnav_gps_fix":
		{
			satnavGpsFix = value === "YES";
		} // case
		break;

		case "satnav_on_map":
		{
			satnavOnMap = value === "YES";

			if (value === "NO" && satnavCurrentStreet !== "")
			{
				satnavCurrentStreet = "";
				$('[gid="satnav_curr_street_shown"]').html(notDigitizedAreaText);
			} // if
		} // case
		break;

		case "satnav_disc_recognized":
		{
			if (handleItemChange.nSatNavDiscUnreadable === undefined) handleItemChange.nSatNavDiscUnreadable = 0;

			// No further handling when in power-save mode
			if (economyMode === "ON") break;

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
			if (! $("#satnav_enter_city_characters").is(":visible")) break;

			$("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");
		} // case
		break;

		case "satnav_current_destination_street":
		{
			$("#satnav_current_destination_street_shown").html(value === "" ? cityCenterText : value);

			if (! $("#satnav_enter_street_characters").is(":visible")) break;

			$("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");
		} // case
		break;

		case "satnav_last_destination_city":
		{
			if (satnavStatus1.match(/ARRIVED_AT_DESTINATION_POPUP/))
			{
				showPopup("satnav_reached_destination_popup", 10000);
			} // if
		} // case
		break;

		case "satnav_personal_address_street":
		case "satnav_professional_address_street":
		{
			$("#" + item + "_shown").html(value === "" ? cityCenterText : value);
		} // case
		break;

		case "satnav_last_destination_street":
		{
			$('[gid="satnav_last_destination_street_shown"]').html(value === "" ? cityCenterText : value);
		} // case
		break;

		case "satnav_current_destination_house_number":
		case "satnav_personal_address_house_number":
		case "satnav_professional_address_house_number":
		{
			$("#" + item + "_shown").html(value === "" || value === "0" ? noNumberText : value);
		} // case
		break;

		case "satnav_last_destination_house_number":
		{
			$('[gid="satnav_last_destination_house_number_shown"]').html(value === "" ? noNumberText : value);
		} // case
		break;

		case "satnav_curr_street":
		{
			satnavCurrentStreet = value;

			// Show the first three letters in the debug box. TODO - remove
			$("#original_mfd_curr_street").text(satnavCurrentStreet.substring(0, 3));

			if (satnavMode === "IN_GUIDANCE_MODE")
			{
				// In the guidance screen, show "Street (City)", otherwise "Street not listed"
				$("#satnav_guidance_curr_street").html(value.match(/\(.*\)/) ? value : streetNotListedText);
			} // if

			// In the current location screen, show "City", or "Street (City)", otherwise "Street not listed"
			// Note: when driving off the map, the current location screen will start showing "Not digitized area";
			// this is handled by 'case "satnav_on_map"' above.
			$('[gid="satnav_curr_street_shown"]').html(value !== "" ? value : streetNotListedText);

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
			var entryOnDisc = value.match(/&#x24E7;$/) === null;

			if ($("#satnav_personal_address_manage_buttons").is(":visible"))
			{
				var title = $("#satnav_rename_entry_in_directory_title").text().replace(/\s+\(.*\)/, "");
				$("#satnav_rename_entry_in_directory_title").text(title + " (" + value + ")");
				$("#satnav_delete_directory_entry_in_popup").text(value);

				$("#satnav_manage_personal_address_rename_button").toggleClass("buttonDisabled", ! entryOnDisc);
				$("#satnav_manage_personal_address_rename_button").toggleClass("buttonSelected", entryOnDisc);
				$("#satnav_manage_personal_address_delete_button").toggleClass("buttonSelected", ! entryOnDisc);
			}
			else
			{
				$("#satnav_show_personal_address_validate_button").toggleClass("buttonDisabled", ! entryOnDisc);
				$("#satnav_show_personal_address_validate_button").toggleClass("buttonSelected", entryOnDisc);
			} // if
		} // case
		break;

		case "satnav_professional_address_entry":
		{
			satnavDirectoryEntry = value;

			// The "Rename" resp. "Validate" buttons are disabled if the entry is not on the current disc
			// (value ends with "&#x24E7;" (X))
			var entryOnDisc = value.match(/&#x24E7;$/) === null;

			if ($("#satnav_professional_address_manage_buttons").is(":visible"))
			{
				var title = $("#satnav_rename_entry_in_directory_title").text().replace(/\s+\(.*\)/, "");
				$("#satnav_rename_entry_in_directory_title").text(title + " (" + value + ")");
				$("#satnav_delete_directory_entry_in_popup").text(value);

				$("#satnav_manage_professional_address_rename_button").toggleClass("buttonDisabled", ! entryOnDisc);
				$("#satnav_manage_professional_address_rename_button").toggleClass("buttonSelected", entryOnDisc);
				$("#satnav_manage_professional_address_delete_button").toggleClass("buttonSelected", ! entryOnDisc);
			}
			else
			{
				$("#satnav_show_professional_address_validate_button").toggleClass("buttonDisabled", ! entryOnDisc);
				$("#satnav_show_professional_address_validate_button").toggleClass("buttonSelected", entryOnDisc);
			} // if
		} // case
		break;

		case "mfd_to_satnav_enter_character":
		{
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

			// Has anything changed?
			if (value === currentLargeScreenId) break;

			if (value === "satnav_choose_from_list")
			{
				if ($("#satnav_enter_street_characters").is(":visible")
					&& $("#satnav_enter_characters_list_button").hasClass("buttonSelected"))
				{
					userHadOpportunityToEnterStreet = true;
				} // if

				satnavGotoListScreen();
				break;
			} // if

			if (value === "satnav_enter_city_characters")
			{
				// Switching (back) into the "satnav_enter_city_characters" screen

				if (currentMenu === "satnav_current_destination_house_number") exitMenu();
				else if (currentMenu === "satnav_enter_street_characters") exitMenu();
				else if (currentMenu === "satnav_show_last_destination") satnavEnterNewCityForService();
				else gotoMenu(value);

				break;
			} // if

			if (value === "satnav_enter_street_characters")
			{
				// Switching (back) into the "satnav_enter_street_characters" screen

				if (currentMenu === "satnav_current_destination_house_number") exitMenu();
				else satnavGotoEnterStreetOrNumber();

				break;
			} // if

			if (value === "satnav_current_destination_house_number")
			{
				satnavLastEnteredChar = null;
				satnavClearEnteredNumber();  // Clear the entered house number
				gotoMenu(value);
				break;
			} // if

			if (value === "satnav_show_current_destination"
				|| value === "satnav_show_personal_address"
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

			// Show or hide the appropriate tag
			$("#satnav_tag_city_list").toggle(value === "enter_city");
			$("#satnav_tag_street_list").toggle(value === "enter_street");
			$("#satnav_tag_service_list").toggle(value === "service");
			$("#satnav_tag_personal_address_list").toggle(value === "personal_address_list");
			$("#satnav_tag_professional_address_list").toggle(value === "professional_address_list");
		} // case
		break;

		case "mfd_to_satnav_request_type":
		{
			handleItemChange.mfdToSatnavRequestType = value;

			if (value !== "REQ_N_ITEMS") break;

			switch (handleItemChange.mfdToSatnavRequest)
			{
				case "enter_city":
				case "enter_street":
				{
					$("#satnav_to_mfd_show_characters_line_1").empty();
					$("#satnav_to_mfd_show_characters_line_2").empty();

					// Show the spinning disc after a second or so
					clearTimeout(handleItemChange.showCharactersSpinningDiscTimer);
					handleItemChange.showCharactersSpinningDiscTimer = setTimeout
					(
						function () { $("#satnav_to_mfd_show_characters_spinning_disc").show(); },
						1500
					);
				} // case
				break;
			} // switch
		} // case
		break;

		case "mfd_to_satnav_selection":
		{
			if (handleItemChange.mfdToSatnavRequest === "enter_street"
				&& handleItemChange.mfdToSatnavRequestType === "SELECT")
			{
				// Already copy the selected street into the "satnav_show_current_destination" screen, in case the
				// "satnav_report" packet is missed

				var lines = splitIntoLines("satnav_choice_list");

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
			satnavToMfdResponse = value;
		} // case
		break;

		case "satnav_to_mfd_list_size":
		{
			// Hide the spinning disc
			clearTimeout(handleItemChange.showCharactersSpinningDiscTimer);
			$("#satnav_to_mfd_show_characters_spinning_disc").hide();

			if (satnavToMfdResponse === "service")
			{
				// Sat nav menu item "Select a service": enable if sat nav responds with a list size value > 0
				if (parseInt(value) > 0) $("#satnav_main_menu_select_a_service_button").removeClass("buttonDisabled");
			} // if

			// "List XXX" button: only enable if less than 80 items in list
			$("#satnav_enter_characters_list_button").toggleClass("buttonDisabled", parseInt(value) > 80);

			if ($("#satnav_choose_from_list").is(":visible") && value == 0)
			{
				if (handleItemChange.mfdToSatnavRequest.match(/^service/))
				{
					// User selected a service which has no address entries for the specified location
					showStatusPopup("This service is<br />not available for<br />this location", 8000);
				}
				else if (handleItemChange.mfdToSatnavRequest === "personal_address_list")
				{
					exitMenu();
					showStatusPopup("Personal directory<br />is empty", 8000);
				}
				else if (handleItemChange.mfdToSatnavRequest === "professional_address_list")
				{
					exitMenu();
					showStatusPopup("Professional directory<br />is empty", 8000);
				} // if
			} // if
		} // case
		break;

		case "satnav_to_mfd_list_2_size":
		{
			// Sometimes there is a second list size. "satnav_to_mfd_list_size" (above) is the number of items
			// *containing* the selected characters; "satnav_to_mfd_list_2_size" (this one) is the number of items
			// *starting* with the selected characters.

			// If there is a second list size, enable the "Validate" button
			if (value !== "" && value !== "0")
			{
				$("#satnav_enter_characters_validate_button").removeClass("buttonDisabled");
			} // if
		} // case
		break;

		case "satnav_list":
		{
			// Hide the spinning disc
			clearTimeout(satnavGotoListScreen.showSpinningDiscTimer);
			$("#satnav_choose_from_list_spinning_disc").hide();

			switch(handleItemChange.mfdToSatnavRequest)
			{
				case "enter_city":
				case "enter_street":
					// These entries are received in chunks of 20

					if (handleItemChange.mfdToSatnavOffset == 0 || $("#satnav_choice_list").text() == "")
					{
						// First chunk
						$("#satnav_choice_list").html(value.join('<br />'));
					}
					else
					{
						// Follow-up chunk
						//unhighlightLine("satnav_choice_list");

						var lines = splitIntoLines("satnav_choice_list");
						lines = lines.slice(0, handleItemChange.mfdToSatnavOffset);
						lines = lines.concat(value);
						$("#satnav_choice_list").html(lines.join('<br />'));

						highlightNextLine("satnav_choice_list");
					} // if

					break;

				default:
					$("#satnav_choice_list").html(value.join('<br />'));

			} // switch

			satnavCheckIfCityCenterMustBeAdded();

			// Highlight the current line (or the first, if no line is currently highlighted)
			highlightLine("satnav_choice_list");

			if (handleItemChange.mfdToSatnavRequest === "service")
			{
				satnavServices = value;

				// Save in local (persistent) store
				localStorage.satnavServices = JSON.stringify(satnavServices);
			}
			else if (handleItemChange.mfdToSatnavRequest === "personal_address_list")
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
			else if (handleItemChange.mfdToSatnavRequest === "professional_address_list")
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
		} // case
		break;

		case "mfd_to_satnav_offset":
		{
			if (value === "") break;

			handleItemChange.mfdToSatnavOffset = value;

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
		{
			if (value !== "YES") break;
		} // case
		// Fallthrough intended

		case "satnav_system_id":
		{
			// As soon as any value is received for "satnav_system_id", the sat nav is considered "initialized"
			satnavInitialized = true;

			// Hide any popup "Navigation system being initialized" (if shown)
			hidePopup("satnav_initializing_popup");

			// Show "Continue guidance? (yes/no)" popup if applicable
			if (contactKeyPosition === "ON" && localStorage.askForGuidanceContinuation === "YES")
			{
				// Show popup "Continue guidance to destination? [Yes]/No"
				showPopup("satnav_continue_guidance_popup", 15000);

				localStorage.askForGuidanceContinuation = "NO";
			} // if
		} // case
		break;

		case "satnav_distance_to_dest_via_road":
		{
			handleItemChange.satnavDistanceToDestViaRoad = value;
		}
		// No 'break': fallthrough intended
		case "satnav_service_address_distance":
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
				$("#satnav_entered_string").empty();  // Clear the character-by-character entry string
				satnavLastEnteredChar = null;
			}
			else if (value === "Val")
			{
				satnavEnterOrDeleteCharacter(0);
			} // if

			handleItemChange.mfdToSatnavInstruction = value;
		} // case
		break;

		case "satnav_to_mfd_show_characters":
		{
			if (value === "") break;

			clearTimeout(showAvailableCharactersTimer);

			if ($("#satnav_enter_characters").is(":visible"))
			{
				satnavEnterOrDeleteCharacter(1);

				if (satnavRollingBackEntryByCharacter) satnavAvailableCharactersStack.pop()
				else satnavAvailableCharactersStack.push(value);
			} // if

			// if (value.length > 1)
			// {
				// satnavRollingBackEntryByCharacter = false;
			// } // if

			if (value.length == 1)
			{
				// Save for later: this character can be automatically selected by the MFD without the user explicitly
				// pressing the "Val" button on the remote control. See code at 'case "mfd_to_satnav_instruction"'
				// above.
				satnavLastEnteredChar = value;
			} // if

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
					|| $("#satnav_enter_characters_list_button").hasClass("buttonSelected")
					|| $("#satnav_enter_characters_validate_button").hasClass("buttonSelected"))
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

				$("#satnav_guidance_next_street").html(
					localStorage.mfdLanguage === "set_language_french" ? "Suivez le cap indiqu&eacute;" :
					localStorage.mfdLanguage === "set_language_german" ? "Folgen Sie dem Pfeil" :
					localStorage.mfdLanguage === "set_language_spanish" ? "Siga el rumbo indicado" :
					localStorage.mfdLanguage === "set_language_italian" ? "Sequire la direzione" :
					localStorage.mfdLanguage === "set_language_dutch" ? "Volg deze richting" :
					"Follow the heading"
				);

				//satnavCurrentStreet = ""; // Bug: original MFD clears currently known street in this situation...

				// To replicate a bug in the original MFD; in fact the current street is usually known
				$("#satnav_guidance_curr_street").html(notDigitizedAreaText);
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
			if (mfdLargeScreen === "TRIP_COMPUTER") break;

			// Will either start showing the popup, or restart its timer if it is already showing
			showPopup("trip_computer_popup", 8000);
		} // case
		break;

		case "trip_computer_screen_tab":
		{
			// Has anything changed?
			if (value === localStorage.smallScreen) break;
			localStorage.smallScreen = value;

			// Regardless where the original MFD is showing its trip computer data (small screen or large screen),
			// we show it always in the small screen.
			gotoSmallScreen(value);

			if (mfdLargeScreen === "TRIP_COMPUTER")
			{
				// Show the first few letters of the screen name plus the first few letters of the trip computer tab
				$("#original_mfd_large_screen").text(
					mfdLargeScreen.substring(0, 4) + " " + tripComputerShortStr(localStorage.smallScreen)
				);
			}
		} // case
		break;

		case "trip_computer_popup_tab":
		{
			// Apply mapping from string to index
			var tabIndex =
				value === "TRIP_INFO_1" ? 1 :
				value === "TRIP_INFO_2" ? 2 : 0;
			selectTabInTripComputerPopup(tabIndex);

			$("#original_mfd_popup").text(tripComputerShortStr(value));  // Debug info
		} // case
		break;

		case "small_screen":
		{
			// Show three letters of the screen name
			$("#original_mfd_small_screen").text(tripComputerShortStr(value));
		} // case
		break;

		case "large_screen":
		{
			mfdLargeScreen = value;

			if (mfdLargeScreen === "TRIP_COMPUTER")
			{
				// Show the first few letters of the screen name plus the first few letters of the trip computer tab
				$("#original_mfd_large_screen").text(
					mfdLargeScreen.substring(0, 4) + " " + tripComputerShortStr(localStorage.smallScreen)
				);
			}
			else
			{
				// Show the first few letters of the screen name
				$("#original_mfd_large_screen").text(mfdLargeScreen.substring(0, 6));
			} // if
		} // case
		break;

		case "van_bus_overrun":
		{
			// Indicate bus overrun by changing color of comms_led icon to red
			$("#comms_led").css('background-color', 'red');

			// After 10 seconds, return to original color
			clearTimeout(handleItemChange.revertCommsLedColorTimer);
			handleItemChange.revertCommsLedColorTimer = setTimeout
			(
				function () { $("#comms_led").css('background-color', ''); },
				10000
			);
		} // case
		break;

		case "mfd_remote_control":
		{
			// Ignore remote control buttons if contact key is off
			if ($("#contact_key_position").text() === "OFF") break;

			if (economyMode === "ON") break;  // Ignore also when in power-save mode

			var parts = value.split(" ");
			var button = parts[0];

			// Show the first three letters of the button just pressed
			$("#ir_button_pressed").text(button.substring(0, 3));

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
				// if (Date.now() - lastScreenChangedAt < 250)
				// {
					// console.log("// mfd_remote_control - ESC_BUTTON: ignored");
					// break;
				// } // if

				// If a popup is showing, hide it and break
				if (hideTunerPresetsPopup()) break;
				if (hideAudioSettingsPopup()) break;

				if (hidePopup()) break;

				// Ignore the "Esc" button when guidance screen is showing
				if ($("#satnav_guidance").is(":visible")) break;

				// Very special situation... Escaping out of list of services after entering a new "Select a Service"
				// address means escaping back all the way to the to the "Select a Service" address screen.
				if (currentMenu === "satnav_choose_from_list"
					&& handleItemChange.mfdToSatnavRequest === "service"
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
				satnavGrabSelectedCharacter();
			}
			else if (button === "VAL_BUTTON")
			{
				// Ignore if just changed screen; sometimes the screen change is triggered by the system, not the
				// remote control. If the user wanted pressed a button on the previous screen, it will end up
				// being handled by the current screen, which is confusing.
				if (Date.now() - lastScreenChangedAt < 250)
				{
					console.log("// mfd_remote_control - VAL_BUTTON: ignored");
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
				if (hidePopup("climate_control_popup")) break;

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
				if (inMenu()) break;  // Ignore when user is browsing the menus

				nextLargeScreen();
			} // if
		} // case
		break;

		case "mfd_status":
		{
			if (value === "MFD_SCREEN_ON")
			{
				if (economyMode === "ON" && currentLargeScreenId !== "pre_flight" && engineRpm === 0) showPowerSavePopup();
			}
			else if (value === "MFD_SCREEN_OFF")
			{
				hidePopup();
			} // if
		} // case
		break;

		case "mfd_popup":
		{
			$("#original_mfd_popup").text(value);  // Debug info
		} // case
		break;

		case "set_fan_speed":
		case "rear_heater_2":
		case "ac_enabled":
		{
			// Has anything changed?
			if (value === handleItemChange[item]) break;
			handleItemChange[item] = value;

			if (item === "set_fan_speed")
			{
				var on = value !== "0";
				$("#fan_icon").toggleClass("ledOn", on);
				$("#fan_icon").toggleClass("ledOff", ! on);
			} // if

			if (inMenu() || currentLargeScreenId === "pre_flight") break;  // Not in menu or "pre_flight" screen
			if (engineRunning !== "YES") break;  // Only when engine running
			if (suppressClimateControlPopup != null) break;  // Not when suppress timer is running

			showPopup("climate_control_popup", 5000);
		} // case
		break;

		default:
		break;
	} // switch
} // handleItemChange

// -----
// Functions for localization (language, units)

function setLanguage(language)
{
	var languageSelections =
		"<span class='languageIcon'>D</span>" +
		"<span class='languageIcon'>E</span>" +
		"<span class='languageIcon'>F</span>" +
		"<span class='languageIcon'>GB</span>" +
		"<span class='languageIcon'>NL</span>" +
		"<span class='languageIcon'>I</span>";

	switch(language)
	{
		case "set_language_english":
		{
			doorOpenText = "Door open";
			doorsOpenText = "Doors open";
			bootOpenText = "Boot open";
			bootAndDoorOpenText = "Boot and door open";
			bootAndDoorsOpenText = "Boot and doors open";

			$("#main_menu .menuTitleLine").html("Main menu<br />");
			$("#main_menu_goto_satnav_button").html("Navigation / Guidance");
			$("#main_menu_goto_screen_configuration_button").html("Configure display");

			$("#screen_configuration_menu .menuTitleLine").html("Configure display<br />");
			$("#screen_configuration_menu .button:eq(0)").html("Set brightness");
			$("#screen_configuration_menu .button:eq(1)").html("Set date and time");
			$("#screen_configuration_menu .button:eq(2)").html("Select a language " + languageSelections);
			$("#screen_configuration_menu .button:eq(3)").html("Set format and units");

			$("#set_screen_brightness .menuTitleLine").html("Set brightness<br />");
			$("#set_date_time .menuTitleLine").html("Set date and time<br />");
			$("#set_language .menuTitleLine").html("Select a language<br />");
			$("#set_units .menuTitleLine").html("Set format and units<br />");

			$("#satnav_initializing_popup .messagePopupArea").html("Navigation system<br>being initialised");
			$("#satnav_input_stored_in_personal_dir_popup .messagePopupArea").html("\
				Input has been stored in<br />the personal<br />directory");
			$("#satnav_input_stored_in_professional_dir_popup .messagePopupArea").html("\
				Input has been stored in<br />the professional<br />directory");
			$("#satnav_computing_route_popup .messagePopupArea").html("Computing route<br />in progress");

			$("#satnav_disclaimer_text").html("\
				Navigation is an electronic help system. It<br>\
				cannot replace the driver's decisions. All<br>\
				guidance instructions must be carefully<br>\
				checked by the user.<br>");

			notDigitizedAreaText = "Not digitized area";
			cityCenterText = "City centre";
			changeText = "Change";
			noNumberText = "No number";
			stopGuidanceText = "Stop guidance";
			resumeGuidanceText = "Resume guidance";
			streetNotListedText = "Street not listed";

			$("#satnav_main_menu .menuTitleLine").html("Navigation / Guidance<br />");
			$("#satnav_main_menu .button:eq(0)").html("Enter new destination");
			$("#satnav_main_menu .button:eq(1)").html("Select a service");
			$("#satnav_main_menu .button:eq(2)").html("Select destination from memory");
			$("#satnav_main_menu .button:eq(3)").html("Navigation options");

			$("#satnav_select_from_memory_menu .menuTitleLine").html("Select from memory<br />");
			$("#satnav_select_from_memory_menu .button:eq(0)").html("Personal directory");
			$("#satnav_select_from_memory_menu .button:eq(1)").html("Professional directory");

			$("#satnav_navigation_options_menu .menuTitleLine").html("Navigation options<br />");
			$("#satnav_navigation_options_menu .button:eq(0)").html("Directory management");
			$("#satnav_navigation_options_menu .button:eq(1)").html("Vocal synthesis volume");
			$("#satnav_navigation_options_menu .button:eq(2)").html("Delete directories");
			$("#satnav_navigation_options_menu .button:eq(3)").html(
				satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);

			$("#satnav_directory_management_menu .menuTitleLine").html("Directory management<br />");
			$("#satnav_directory_management_menu .button:eq(0)").html("Personal directory");
			$("#satnav_directory_management_menu .button:eq(1)").html("Professional directory");

			$("#satnav_guidance_tools_menu .menuTitleLine").html("Guidance tools<br />");
			$("#satnav_guidance_tools_menu .button:eq(0)").html("Guidance criteria");
			$("#satnav_guidance_tools_menu .button:eq(1)").html("Programmed destination");
			$("#satnav_guidance_tools_menu .button:eq(2)").html("Vocal synthesis volume");
			$("#satnav_guidance_tools_menu .button:eq(3)").html(stopGuidanceText);

			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").html("Fastest route<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").html("Shortest route<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").html("Avoid highway route<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").html("Fast/short compromise route<br />");

			$("#satnav_vocal_synthesis_level .menuTitleLine").html("Vocal synthesis level<br />");
			$("#satnav_vocal_synthesis_level .tag").html("Level");

			$("#satnav_enter_city_characters .tag").html("Enter city");
			$("#satnav_enter_street_characters .tag").html("Enter street");

			$("#satnav_enter_characters .tag:eq(0)").html("Choose next letter");
			$("#satnav_enter_characters .tag:eq(1)").html("Enter city");
			$("#satnav_enter_characters .tag:eq(2)").html("Enter street");

			$("#satnav_tag_city_list").html("Choose city");
			$("#satnav_tag_street_list").html("Choose street");
			$("#satnav_tag_service_list").html("Service list");
			$("#satnav_tag_personal_address_list").html("Personal address list");
			$("#satnav_tag_professional_address_list").html("Professional address list");

			$("#satnav_enter_house_number .tag").html("Enter number");

			$("#satnav_show_personal_address .tag:eq(0)").html("City");
			$("#satnav_show_personal_address .tag:eq(1)").html("Street");
			$("#satnav_show_personal_address .tag:eq(2)").html("Number");

			$("#satnav_show_professional_address .tag:eq(0)").html("City");
			$("#satnav_show_professional_address .tag:eq(1)").html("Street");
			$("#satnav_show_professional_address .tag:eq(3)").html("Number");

			$("#satnav_show_service_address .tag:eq(0)").html("City");
			$("#satnav_show_service_address .tag:eq(1)").html("Street");

			$("#satnav_show_current_destination .tag:eq(0)").html("Programmed destination");
			$("#satnav_show_current_destination .tag:eq(1)").html("City");
			$("#satnav_show_current_destination .tag:eq(2)").html("Street");
			$("#satnav_show_current_destination .tag:eq(3)").html("Number");

			$("#satnav_show_programmed_destination .tag:eq(0)").html("Programmed destination");
			$("#satnav_show_programmed_destination .tag:eq(1)").html("City");
			$("#satnav_show_programmed_destination .tag:eq(2)").html("Street");
			$("#satnav_show_programmed_destination .tag:eq(3)").html("Number");

			$("#satnav_show_last_destination .tag:eq(0)").html("Select a service");
			$("#satnav_show_last_destination .tag:eq(1)").html("City");
			$("#satnav_show_last_destination .tag:eq(2)").html("Street");
			$("#satnav_show_last_destination .button:eq(2)").html("Current location");

			$("#satnav_archive_in_directory_title").html("Archive in directory");
			$("#satnav_archive_in_directory .satNavEntryNameTag").html("Name");
			$("#satnav_archive_in_directory .button:eq(0)").html("Correction");
			$("#satnav_archive_in_directory .button:eq(1)").html("Personal dir");
			$("#satnav_archive_in_directory .button:eq(2)").html("Professional d");

			$("#satnav_rename_entry_in_directory_title").html("Rename entry");
			$("#satnav_rename_entry_in_directory .satNavEntryNameTag").html("Name");
			$("#satnav_rename_entry_in_directory .button:eq(1)").html("Correction");

			$("#satnav_reached_destination_popup_title").html("Destination reached");
			$("#satnav_delete_item_popup_title").html("Delete item ?<br />");
			$("#satnav_guidance_preference_popup_title").html("Keep criteria");
			$("#satnav_delete_directory_data_popup_title").html("This will delete all data in directories");
			$("#satnav_continue_guidance_popup_title").html("Continue guidance to destination ?");

			$(".yesButton").html("Yes");
			$(".noButton").html("No");

			$(".validateButton").html("Validate");
			$("#satnav_disclaimer_validate_button").html("Validate");
			$("#satnav_manage_personal_address_rename_button").html("Rename");
			$("#satnav_manage_personal_address_delete_button").html("Delete");
			$("#satnav_manage_professional_address_rename_button").html("Rename");
			$("#satnav_manage_professional_address_delete_button").html("Delete");
			$("#satnav_service_address_previous_button").html("Previous");
			$("#satnav_service_address_next_button").html("Next");

			$(".correctionButton").html(changeText);
			$("#satnav_enter_characters_correction_button").html("Correction");
			$("#satnav_to_mfd_list_tag").html("List");
			$("#satnav_store_entry_in_directory").html("Store");

			$(".satNavEntryExistsTag").html("This entry already exists");
		} // case
		break;

		case "set_language_french":
		{
			doorOpenText = "Porte ouverte";
			doorsOpenText = "Portes ouverts";
			bootOpenText = "Coffre ouvert";
			bootAndDoorOpenText = "Porte et coffre ouverts";
			bootAndDoorsOpenText = "Portes et coffre ouverts";

			$("#main_menu .menuTitleLine").html("Menu g&eacute;n&eacute;ral<br />");
			$("#main_menu_goto_satnav_button").html("Navigation / Guidage");
			$("#main_menu_goto_screen_configuration_button").html("Configuration afficheur");

			$("#screen_configuration_menu .menuTitleLine").html("Configuration afficheur<br />");
			$("#screen_configuration_menu .button:eq(0)").html("R&eacute;glage luminosit&eacute;");
			$("#screen_configuration_menu .button:eq(1)").html("R&eacute;glage de date et heure");
			$("#screen_configuration_menu .button:eq(2)").html("Choix de la langue " + languageSelections);
			$("#screen_configuration_menu .button:eq(3)").html("R&eacute;glage des formats et unit&eacute;s");

			$("#set_screen_brightness .menuTitleLine").html("R&eacute;glage luminosit&eacute;<br />");
			$("#set_date_time .menuTitleLine").html("R&eacute;glage de date et heure<br />");
			$("#set_language .menuTitleLine").html("Choix de la langue<br />");
			$("#set_units .menuTitleLine").html("R&eacute;glage des formats et unit&eacute;s<br />");

			// $("#satnav_initializing_popup .messagePopupArea").html("Navigation system<br>being initialised");
			$("#satnav_input_stored_in_personal_dir_popup .messagePopupArea").html("\
				L'entre&eacute;e a &eacute;t&eacute; archiv&eacute;e<br />dans le r&eacute;pertoire<br />personnel");
			$("#satnav_input_stored_in_professional_dir_popup .messagePopupArea").html("\
				L'entre&eacute;e a &eacute;t&eacute; archiv&eacute;e<br />dans le r&eacute;pertoire<br />professionel");
			$("#satnav_computing_route_popup .messagePopupArea").html("Le calcul d'itin&eacute;raire<br />est en cours");

			$("#satnav_disclaimer_text").html("\
				La navigation est un syst&egrave;me &eacute;lectronique<br>\
				d'assistance. Il ne peut en aucun cas se<br>\
				substituer &agrave; l'analyse du conducteur.<br>\
				Soi te consigne de guidance est &agrave; v&eacute;rifier<br>\
				scrupuleusement par l'utilisateur.<br>");

			notDigitizedAreaText = "Zone non cartographi&eacute;e";
			cityCenterText = "Centre-ville";
			changeText = "Changer";
			noNumberText = "Pas de num&eacute;ro";
			stopGuidanceText = "Arr&ecirc;ter le guidage";
			resumeGuidanceText = "Reprendre le guidage";
			streetNotListedText = "Rue non r&eacute;pertori&eacute;e";  // TODO - check

			$("#satnav_main_menu .menuTitleLine").html("Navigation / Guidance<br />");
			$("#satnav_main_menu .button:eq(0)").html("Saisie d'une nouvelle destination");
			$("#satnav_main_menu .button:eq(1)").html("Choix d'un service");
			$("#satnav_main_menu .button:eq(2)").html("Choix d'une destination archiv&eacute;e");
			$("#satnav_main_menu .button:eq(3)").html("Options de navigation");

			$("#satnav_select_from_memory_menu .menuTitleLine").html("Choix destination archiv&eacute;e<br />");
			$("#satnav_select_from_memory_menu .button:eq(0)").html("R&eacute;pertoire personnel");
			$("#satnav_select_from_memory_menu .button:eq(1)").html("R&eacute;pertoire professionnel");

			$("#satnav_navigation_options_menu .menuTitleLine").html("Options de navigation<br />");
			$("#satnav_navigation_options_menu .button:eq(0)").html("Gestion des r&eacute;pertoires");
			$("#satnav_navigation_options_menu .button:eq(1)").html("Volume synth&egrave;se vocale");
			$("#satnav_navigation_options_menu .button:eq(2)").html("Effacement des r&eacute;pertoires");
			$("#satnav_navigation_options_menu .button:eq(3)").html(
				satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);

			$("#satnav_directory_management_menu .menuTitleLine").html("Gestion des r&eacute;pertoires<br />");
			$("#satnav_directory_management_menu .button:eq(0)").html("R&eacute;pertoire personnel");
			$("#satnav_directory_management_menu .button:eq(1)").html("R&eacute;pertoire professionnel");

			$("#satnav_guidance_tools_menu .menuTitleLine").html("Outils de guidage<br />");
			$("#satnav_guidance_tools_menu .button:eq(0)").html("Crit&egrave;res de guidage");
			$("#satnav_guidance_tools_menu .button:eq(1)").html("Destination programm&eacute;e");
			$("#satnav_guidance_tools_menu .button:eq(2)").html("Volume synth&egrave;se vocale");
			$("#satnav_guidance_tools_menu .button:eq(3)").html(stopGuidanceText);

			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").html("Trajet le plus rapide<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").html("Trajet le plus court<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").html("Itin&eacute;raire sans voie rapide<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").html("Compromis rapidit&eacute;/distance<br />");

			$("#satnav_vocal_synthesis_level .menuTitleLine").html("Volume synth&egrave;se vocale<br />");
			$("#satnav_vocal_synthesis_level .tag").html("Volume");

			$("#satnav_enter_city_characters .tag").html("Saisie de la ville");
			$("#satnav_enter_street_characters .tag").html("Saisie de la voie");

			$("#satnav_enter_characters .tag:eq(0)").html("Choisis une lettre");  // TODO - check
			$("#satnav_enter_characters .tag:eq(1)").html("Saisie de la ville");
			$("#satnav_enter_characters .tag:eq(2)").html("Saisie de la voie");

			$("#satnav_tag_city_list").html("Saisie de la ville");
			$("#satnav_tag_street_list").html("Saisie de la voie");
			$("#satnav_tag_service_list").html("Choix d'un service");  // TODO - check
			$("#satnav_tag_personal_address_list").html("R&eacute;pertoire personnel");  // TODO - check
			$("#satnav_tag_professional_address_list").html("R&eacute;pertoire professionnel");  // TODO - check

			$("#satnav_enter_house_number .tag").html("Saisie du num&eacute;ro");

			$("#satnav_show_personal_address .tag:eq(0)").html("Ville");
			$("#satnav_show_personal_address .tag:eq(1)").html("Voie");
			$("#satnav_show_personal_address .tag:eq(2)").html("Num&eacute;ro");

			$("#satnav_show_professional_address .tag:eq(0)").html("Ville");
			$("#satnav_show_professional_address .tag:eq(1)").html("Voie");
			$("#satnav_show_professional_address .tag:eq(3)").html("Num&eacute;ro");

			$("#satnav_show_service_address .tag:eq(0)").html("Ville");
			$("#satnav_show_service_address .tag:eq(1)").html("Voie");

			$("#satnav_show_current_destination .tag:eq(0)").html("Destination programm&eacute;e");
			$("#satnav_show_current_destination .tag:eq(1)").html("Ville");
			$("#satnav_show_current_destination .tag:eq(2)").html("Voie");
			$("#satnav_show_current_destination .tag:eq(3)").html("Num&eacute;ro");

			$("#satnav_show_programmed_destination .tag:eq(0)").html("Destination programm&eacute;e");
			$("#satnav_show_programmed_destination .tag:eq(1)").html("Ville");
			$("#satnav_show_programmed_destination .tag:eq(2)").html("Voie");
			$("#satnav_show_programmed_destination .tag:eq(3)").html("Num&eacute;ro");

			$("#satnav_show_last_destination .tag:eq(0)").html("Choix d'un service");
			$("#satnav_show_last_destination .tag:eq(1)").html("Ville");
			$("#satnav_show_last_destination .tag:eq(2)").html("Voie");
			$("#satnav_show_last_destination .button:eq(2)").html("Position actuelle");  // TODO - check

			$("#satnav_archive_in_directory_title").html("Archiver dans r&eacute;pertoire");
			$("#satnav_archive_in_directory .satNavEntryNameTag").html("Libell&eacute;");
			$("#satnav_archive_in_directory .button:eq(0)").html("Corriger");
			$("#satnav_archive_in_directory .button:eq(1)").html("R&eacute;p. personnel");
			$("#satnav_archive_in_directory .button:eq(2)").html("R&eacute;p. professionel");

			// $("#satnav_rename_entry_in_directory_title").html("Rename entry");
			$("#satnav_rename_entry_in_directory .satNavEntryNameTag").html("Libell&eacute;");
			$("#satnav_rename_entry_in_directory .button:eq(1)").html("Corriger");

			$("#satnav_reached_destination_popup_title").html("Destination atteinte");  // TODO - check
			$("#satnav_delete_item_popup_title").html("Voulez-vous supprimer<br />la fiche ?<br />");
			$("#satnav_guidance_preference_popup_title").html("Conserver le crit&egrave;re");
			// $("#satnav_delete_directory_data_popup_title").html("This will delete all data in directories");
			$("#satnav_continue_guidance_popup_title").html("Continuer la navigation?");  // TODO - check

			$(".yesButton").html("Oui");
			$(".noButton").html("Non");

			$(".validateButton").html("Valider");
			$("#satnav_disclaimer_validate_button").html("Valider");
			$("#satnav_manage_personal_address_rename_button").html("Renommer");
			$("#satnav_manage_personal_address_delete_button").html("Supprimer");
			$("#satnav_manage_professional_address_rename_button").html("Renommer");
			$("#satnav_manage_professional_address_delete_button").html("Supprimer");
			$("#satnav_service_address_previous_button").html("Pr&eacute;c&eacute;dent");
			$("#satnav_service_address_next_button").html("Suivant");

			$(".correctionButton").html(changeText);
			$("#satnav_enter_characters_correction_button").html("Corriger");
			$("#satnav_to_mfd_list_tag").html("Liste");
			$("#satnav_store_entry_in_directory").html("Archiver");

			$(".satNavEntryExistsTag").html("Libell&eacute; existe d&eacute;j&agrave;");  // TODO - check
		} // case
		break;

		case "set_language_german":
		{
			doorOpenText = "T&uuml;r offen";
			doorsOpenText = "T&uuml;ren offen";
			bootOpenText = "Kofferraum offen";
			bootAndDoorOpenText = "T&uuml;r und Kofferraum offen";
			bootAndDoorsOpenText = "T&uuml;ren und Kofferraum offen";

			$("#main_menu .menuTitleLine").html("Hauptmen&uuml;<br />");
			$("#main_menu_goto_satnav_button").html("Navigation / F&uuml;hrung");
			$("#main_menu_goto_screen_configuration_button").html("Display konfigurieren");

			$("#screen_configuration_menu .menuTitleLine").html("Display konfigurieren<br />");
			$("#screen_configuration_menu .button:eq(0)").html("Helligkeit einstellen");
			$("#screen_configuration_menu .button:eq(1)").html("Datum und Uhrzeit einstellen");
			$("#screen_configuration_menu .button:eq(2)").html("Sprache w&auml;hlen " + languageSelections);
			$("#screen_configuration_menu .button:eq(3)").html("Einstellen der Einheiten");

			$("#set_screen_brightness .menuTitleLine").html("Helligkeit einstellen<br />");
			$("#set_date_time .menuTitleLine").html("Datum und Uhrzeit einstellen<br />");
			$("#set_language .menuTitleLine").html("Sprache w&auml;hlen<br />");
			$("#set_units .menuTitleLine").html("Einstellen der Einheiten<br />");

			$("#satnav_initializing_popup .messagePopupArea").html("Das Navigationssystem<br>wird initialisiert");
			$("#satnav_input_stored_in_personal_dir_popup .messagePopupArea").html("\
				Die Eingabe wurde im<br />pers&ouml;nlichen Verzeichnis<br />gespeichert");
			$("#satnav_input_stored_in_professional_dir_popup .messagePopupArea").html("\
				Die Eingabe wurde im<br />beruflichen Verzeichnis<br />gespeichert");
			$("#satnav_computing_route_popup .messagePopupArea").html("Berechnung der<br />Fahrstrecke l&auml;uft");

			$("#satnav_disclaimer_text").html("\
				Die Navigator ist ein elektronisches<br>\
				Hilfssystem. Es kann auf keinen Fall die<br>\
				Analyse des Fahrers ersetzen. Jede<br>\
				F&uuml;hrungsanweisung mu&szlig; vom Benutzer<br>\
				sofgf&auml;ltig gepr&uuml;ft werden");

			notDigitizedAreaText = "Nicht kartographiert";
			cityCenterText = "Stadtmitte";
			changeText = "Ändern";  // TODO - &Auml;nderen
			noNumberText = "Keine Nummer";
			stopGuidanceText = "F&uuml;hrung abbrechen";
			resumeGuidanceText = "F&uuml;hrung wieder aufnehmen";
			streetNotListedText = "Stra&szlig;e unbekannt";  // TODO - check

			$("#satnav_main_menu .menuTitleLine").html("Navigation / F&uuml;hrung<br />");
			$("#satnav_main_menu .button:eq(0)").html("Neues Ziel eingeben");
			$("#satnav_main_menu .button:eq(1)").html("Einen Dienst w&auml;hlen");
			$("#satnav_main_menu .button:eq(2)").html("Gespeichertes Ziel W&auml;hlen");
			$("#satnav_main_menu .button:eq(3)").html("Navigationsoptionen");

			$("#satnav_select_from_memory_menu .menuTitleLine").html("Gespeichertes Ziel w&auml;hlen<br />");
			$("#satnav_select_from_memory_menu .button:eq(0)").html("Pers&ouml;nliches Zielverzeichnis");
			$("#satnav_select_from_memory_menu .button:eq(1)").html("Berufliches Zielverzeichnis");

			$("#satnav_navigation_options_menu .menuTitleLine").html("Navigationsoptionen<br />");
			$("#satnav_navigation_options_menu .button:eq(0)").html("Verwalben der Verzeichnisse");
			$("#satnav_navigation_options_menu .button:eq(1)").html("Lautst. der Synthesestimme");
			$("#satnav_navigation_options_menu .button:eq(2)").html("L&ouml;schen der Verzeichnisse");
			$("#satnav_navigation_options_menu .button:eq(3)").html(
				satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);

			$("#satnav_directory_management_menu .menuTitleLine").html("Verwalben der Verzeichnisse<br />");
			$("#satnav_directory_management_menu .button:eq(0)").html("Pers&ouml;nliches Zielverzeichnis");
			$("#satnav_directory_management_menu .button:eq(1)").html("Berufliches Zielverzeichnis");

			$("#satnav_guidance_tools_menu .menuTitleLine").html("F&uuml;hrungshilfen<br />");
			$("#satnav_guidance_tools_menu .button:eq(0)").html("F&uuml;hrungskriterien");
			$("#satnav_guidance_tools_menu .button:eq(1)").html("Programmiertes Ziel");
			$("#satnav_guidance_tools_menu .button:eq(2)").html("Lautst. der Synthesestimme");
			$("#satnav_guidance_tools_menu .button:eq(3)").html(stopGuidanceText);

			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").html("Die schnellste Route<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").html("Die k&uuml;rzeste Route<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").html("Autobahn meiden<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").html("Vergleich Fahrzeit/Routenl&auml;nge<br />");

			$("#satnav_vocal_synthesis_level .menuTitleLine").html("Lautst. der Synthesestimme<br />");
			$("#satnav_vocal_synthesis_level .tag").html("Lautst.");

			$("#satnav_enter_city_characters .tag").html("Stadt eingeben");
			$("#satnav_enter_street_characters .tag").html("Stra&szlig;e eingeben");

			$("#satnav_enter_characters .tag:eq(0)").html("Buchstabe w&auml;hlen");  // TODO - check
			$("#satnav_enter_characters .tag:eq(1)").html("Stadt eingeben");
			$("#satnav_enter_characters .tag:eq(2)").html("Stra&szlig;e eingeben");

			$("#satnav_tag_city_list").html("Stadt eingeben");
			$("#satnav_tag_street_list").html("Stra&szlig;e eingeben");
			$("#satnav_tag_service_list").html("Einen Dienst w&auml;hlen");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Pers&ouml;nliches Zielverzeichnis");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Berufliches Zielverzeichnis");  // TODO - check

			$("#satnav_enter_house_number .tag").html("Hausnummer eingeben");

			$("#satnav_show_personal_address .tag:eq(0)").html("Stadt");
			$("#satnav_show_personal_address .tag:eq(1)").html("Stra&szlig;e");
			$("#satnav_show_personal_address .tag:eq(2)").html("Nummer");

			$("#satnav_show_professional_address .tag:eq(0)").html("Stadt");
			$("#satnav_show_professional_address .tag:eq(1)").html("Stra&szlig;e");
			$("#satnav_show_professional_address .tag:eq(3)").html("Nummer");

			$("#satnav_show_service_address .tag:eq(0)").html("Stadt");
			$("#satnav_show_service_address .tag:eq(1)").html("Stra&szlig;e");

			$("#satnav_show_current_destination .tag:eq(0)").html("Programmiertes Ziel");
			$("#satnav_show_current_destination .tag:eq(1)").html("Stadt");
			$("#satnav_show_current_destination .tag:eq(2)").html("Stra&szlig;e");
			$("#satnav_show_current_destination .tag:eq(3)").html("Nummer");

			$("#satnav_show_programmed_destination .tag:eq(0)").html("Programmiertes Ziel");
			$("#satnav_show_programmed_destination .tag:eq(1)").html("Stadt");
			$("#satnav_show_programmed_destination .tag:eq(2)").html("Stra&szlig;e");
			$("#satnav_show_programmed_destination .tag:eq(3)").html("Nummer");

			$("#satnav_show_last_destination .tag:eq(0)").html("Einen Dienst w&auml;hlen");
			$("#satnav_show_last_destination .tag:eq(1)").html("Stadt");
			$("#satnav_show_last_destination .tag:eq(2)").html("Stra&szlig;e");
			$("#satnav_show_last_destination .button:eq(2)").html("Aktueller Standort");  // TODO - check

			$("#satnav_archive_in_directory_title").html("Im Verzeichnis speichern");
			$("#satnav_archive_in_directory .satNavEntryNameTag").html("Name");
			$("#satnav_archive_in_directory .button:eq(0)").html("Korrigieren");
			$("#satnav_archive_in_directory .button:eq(1)").html("Pers&ouml;nl. Speic");
			$("#satnav_archive_in_directory .button:eq(2)").html("Berufl. Speich");

			$("#satnav_rename_entry_in_directory_title").html("Umbenennen");
			$("#satnav_rename_entry_in_directory .satNavEntryNameTag").html("Name");
			$("#satnav_rename_entry_in_directory .button:eq(1)").html("Korrigieren");

			$("#satnav_reached_destination_popup_title").html("Ziel erreicht");  // TODO - check
			$("#satnav_delete_item_popup_title").html("M&ouml;chten Sie Diese<br />Position l&ouml;schen ?<br />");
			$("#satnav_guidance_preference_popup_title").html("Beibehalten Routenart");
			$("#satnav_delete_directory_data_popup_title").html("Hiermit werden alle Daten der Verzeichnisse gel&ouml;scht");
			$("#satnav_continue_guidance_popup_title").html("Navigation fortsetzen?");  // TODO - check

			$(".yesButton").html("Ja");
			$(".noButton").html("Nein");

			$(".validateButton").html("Best&auml;tigen");
			$("#satnav_disclaimer_validate_button").html("Best&auml;tigen");
			$("#satnav_manage_personal_address_rename_button").html("Umbenennen");
			$("#satnav_manage_personal_address_delete_button").html("L&ouml;schen");
			$("#satnav_manage_professional_address_rename_button").html("Umbenennen");
			$("#satnav_manage_professional_address_delete_button").html("L&ouml;schen");
			$("#satnav_service_address_previous_button").html("Z&uuml;r&uuml;ck");
			$("#satnav_service_address_next_button").html("Weiter");

			$(".correctionButton").html(changeText);
			$("#satnav_enter_characters_correction_button").html("Korrigieren");
			$("#satnav_to_mfd_list_tag").html("Liste");
			$("#satnav_store_entry_in_directory").html("Speichern");

			$(".satNavEntryExistsTag").html("Name existiert bereits");  // TODO - check
		} // case
		break;

		case "set_language_spanish":
		{
			doorOpenText = "Puerta abierta";
			doorsOpenText = "Puertas abiertas";
			bootOpenText = "Maletero abierto";
			bootAndDoorOpenText = "Puerta y maletero abiertos";
			bootAndDoorsOpenText = "Puertas y maletero abiertos";

			$("#main_menu .menuTitleLine").html("Men&uacute; general<br />");
			$("#main_menu_goto_satnav_button").html("Navegaci&oacute;n / Guiado");
			$("#main_menu_goto_screen_configuration_button").html("Configuraci&oacute;n de pantalla");

			$("#screen_configuration_menu .menuTitleLine").html("Configuraci&oacute;n de pantalla<br />");
			$("#screen_configuration_menu .button:eq(0)").html("Ajustar luminosidad");
			$("#screen_configuration_menu .button:eq(1)").html("Ajustar hora y fecha");
			$("#screen_configuration_menu .button:eq(2)").html("Seleccionar idioma " + languageSelections);
			$("#screen_configuration_menu .button:eq(3)").html("Ajustar formatos y unidades");

			$("#set_screen_brightness .menuTitleLine").html("Ajustar luminosidad<br />");
			$("#set_date_time .menuTitleLine").html("Ajustar hora y fecha<br />");
			$("#set_language .menuTitleLine").html("Seleccionar idioma<br />");
			$("#set_units .menuTitleLine").html("Ajustar formatos y unidades<br />");

			// $("#satnav_initializing_popup .messagePopupArea").html("Navigation system<br>being initialised");
			$("#satnav_input_stored_in_personal_dir_popup .messagePopupArea").html("\
				Datos memorizados en el<br />directorio personal");
			$("#satnav_input_stored_in_professional_dir_popup .messagePopupArea").html("\
				Datos memorizados en el<br />directorio profesional");
			$("#satnav_computing_route_popup .messagePopupArea").html("C&aacute;lculo de itinerario<br />en proceso");

			$("#satnav_disclaimer_text").html("\
				La navegaci&oacute;n es un sistema electr&oacute;nico<br>\
				de asistencia que nunca se sustiuye al<br>\
				an&aacute;lisis del conductor quien debe<br>\
				comprobar todos las instrucciones de<br>\
				guiado y respetar las se&ntilde;ales de<br>\
				carreteras.<br>");

			notDigitizedAreaText = "Zona no cartografiada";
			cityCenterText = "Centro de la ciudad";
			changeText = "Cambiar";
			noNumberText = "Sin n&uacute;mero";
			stopGuidanceText = "Interrumpir guiado";
			resumeGuidanceText = "Reanudar el guiado";
			streetNotListedText = "Calle no listada"; // TODO - check

			$("#satnav_main_menu .menuTitleLine").html("Navegaci&oacute;n / Guiado<br />");
			$("#satnav_main_menu .button:eq(0)").html("Introducir nuevo destino");
			$("#satnav_main_menu .button:eq(1)").html("Seleccionar servicio");
			$("#satnav_main_menu .button:eq(2)").html("Seleccionar destino archivado");
			$("#satnav_main_menu .button:eq(3)").html("Opciones de navegaci&oacute;n");

			$("#satnav_select_from_memory_menu .menuTitleLine").html("Seleccionar destino archivado<br />");
			$("#satnav_select_from_memory_menu .button:eq(0)").html("Directorio personal");
			$("#satnav_select_from_memory_menu .button:eq(1)").html("Directorio profesional");

			$("#satnav_navigation_options_menu .menuTitleLine").html("Opciones de navegaci&oacute;n<br />");
			$("#satnav_navigation_options_menu .button:eq(0)").html("Gesti&oacute;n de directorios");
			$("#satnav_navigation_options_menu .button:eq(1)").html("Volumen de s&iacute;ntesis vocal");
			$("#satnav_navigation_options_menu .button:eq(2)").html("Borrado de directorios");
			$("#satnav_navigation_options_menu .button:eq(3)").html(
				satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);

			$("#satnav_directory_management_menu .menuTitleLine").html("Gesti&oacute;n de directorios<br />");
			$("#satnav_directory_management_menu .button:eq(0)").html("Directorio personal");
			$("#satnav_directory_management_menu .button:eq(1)").html("Directorio profesional");

			$("#satnav_guidance_tools_menu .menuTitleLine").html("Herramientas de guiado<br />");
			$("#satnav_guidance_tools_menu .button:eq(0)").html("Criterios de guiado");
			$("#satnav_guidance_tools_menu .button:eq(1)").html("Destino programado");
			$("#satnav_guidance_tools_menu .button:eq(2)").html("Volumen de s&iacute;ntesis vocal");
			$("#satnav_guidance_tools_menu .button:eq(3)").html(stopGuidanceText);

			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").html("M&aacute;s r&aacute;pido<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").html("M&aacute;s corto<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").html("Sin autopistas<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").html("Compromiso r&aacute;pido/corto<br />");

			$("#satnav_vocal_synthesis_level .menuTitleLine").html("Volumen de s&iacute;ntesis vocal<br />");
			$("#satnav_vocal_synthesis_level .tag").html("Volumen");

			$("#satnav_enter_city_characters .tag").html("Introducir la ciudad");
			$("#satnav_enter_street_characters .tag").html("Introducir la calle");

			$("#satnav_enter_characters .tag:eq(0)").html("Elegir letra");  // TODO - check
			$("#satnav_enter_characters .tag:eq(1)").html("Introducir la ciudad");
			$("#satnav_enter_characters .tag:eq(2)").html("Introducir la calle");

			$("#satnav_tag_city_list").html("Introducir la ciudad");
			$("#satnav_tag_street_list").html("Introducir la calle");
			$("#satnav_tag_service_list").html("Seleccionar servicio");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Directorio personal");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Directorio profesional");  // TODO - check

			$("#satnav_enter_house_number .tag").html("Introducir el n&uacute;mero");

			$("#satnav_show_personal_address .tag:eq(0)").html("Ciudad");
			$("#satnav_show_personal_address .tag:eq(1)").html("Calle");
			$("#satnav_show_personal_address .tag:eq(2)").html("N&uacute;mero");

			$("#satnav_show_professional_address .tag:eq(0)").html("Ciudad");
			$("#satnav_show_professional_address .tag:eq(1)").html("Calle");
			$("#satnav_show_professional_address .tag:eq(3)").html("N&uacute;mero");

			$("#satnav_show_service_address .tag:eq(0)").html("Ciudad");
			$("#satnav_show_service_address .tag:eq(1)").html("Calle");

			$("#satnav_show_current_destination .tag:eq(0)").html("Destino programado");
			$("#satnav_show_current_destination .tag:eq(1)").html("Ciudad");
			$("#satnav_show_current_destination .tag:eq(2)").html("Calle");
			$("#satnav_show_current_destination .tag:eq(3)").html("N&uacute;mero");

			$("#satnav_show_programmed_destination .tag:eq(0)").html("Destino programado");
			$("#satnav_show_programmed_destination .tag:eq(1)").html("Ciudad");
			$("#satnav_show_programmed_destination .tag:eq(2)").html("Calle");
			$("#satnav_show_programmed_destination .tag:eq(3)").html("N&uacute;mero");

			$("#satnav_show_last_destination .tag:eq(0)").html("Seleccionar servicio");
			$("#satnav_show_last_destination .tag:eq(1)").html("Ciudad");
			$("#satnav_show_last_destination .tag:eq(2)").html("Calle");
			$("#satnav_show_last_destination .button:eq(2)").html("Ubicaci&oacute;n actual");  // TODO - check

			$("#satnav_archive_in_directory_title").html("Archivar en directorio");
			$("#satnav_archive_in_directory .satNavEntryNameTag").html("Denominaci&oacute;n");
			$("#satnav_archive_in_directory .button:eq(0)").html("Corregir");
			$("#satnav_archive_in_directory .button:eq(1)").html("Directorio personal");
			$("#satnav_archive_in_directory .button:eq(2)").html("Directorio profesional");

			$("#satnav_rename_entry_in_directory_title").html("Cambiar");
			$("#satnav_rename_entry_in_directory .satNavEntryNameTag").html("Denominaci&oacute;n");
			$("#satnav_rename_entry_in_directory .button:eq(1)").html("Corregir");

			$("#satnav_reached_destination_popup_title").html("Destino alcanzado");  // TODO - check
			$("#satnav_delete_item_popup_title").html("&iquest;Desea suprimir<br />la ficha?");
			// $("#satnav_guidance_preference_popup_title").html("Keep criteria");
			$("#satnav_delete_directory_data_popup_title").html("Se borrar&aacute;n todos los<br />catos de los directorios");
			$("#satnav_continue_guidance_popup_title").html("&iquest;Continuar la navegaci&oacute;n?");  // TODO - check

			$(".yesButton").html("S&iacute;");
			$(".noButton").html("No");

			$(".validateButton").html("Aceptar");
			$("#satnav_disclaimer_validate_button").html("Aceptar");
			$("#satnav_manage_personal_address_rename_button").html("Cambiar");
			$("#satnav_manage_personal_address_delete_button").html("Suprimir");
			$("#satnav_manage_professional_address_rename_button").html("Cambiar");
			$("#satnav_manage_professional_address_delete_button").html("Suprimir");
			$("#satnav_service_address_previous_button").html("Anterior");
			$("#satnav_service_address_next_button").html("Siguiente");

			$(".correctionButton").html(changeText);
			$("#satnav_enter_characters_correction_button").html("Corregir");
			$("#satnav_to_mfd_list_tag").html("Lista");
			$("#satnav_store_entry_in_directory").html("Memorizar");

			$(".satNavEntryExistsTag").html("Denominaci&oacute;n ya existe");  // TODO - check
		} // case
		break;

		case "set_language_italian":
		{
			doorOpenText = "Portiera aperta";
			doorsOpenText = "Portiere aperte";
			bootOpenText = "Cofano aperto";
			bootAndDoorOpenText = "Portiera e cofano aperti";
			bootAndDoorsOpenText = "Portiere e cofano aperti";

			$("#main_menu .menuTitleLine").html("Men&ugrave; generale<br />");
			$("#main_menu_goto_satnav_button").html("Navigazione/Guida");
			$("#main_menu_goto_screen_configuration_button").html("Configurazione monitor");

			$("#screen_configuration_menu .menuTitleLine").html("Configurazione monitor<br />");
			$("#screen_configuration_menu .button:eq(0)").html("Regolazione luminosita");
			$("#screen_configuration_menu .button:eq(1)").html("Regolazione date e ora");
			$("#screen_configuration_menu .button:eq(2)").html("Scelta della lingua " + languageSelections);
			$("#screen_configuration_menu .button:eq(3)").html("Regolazione formato e unita");

			$("#set_screen_brightness .menuTitleLine").html("Regolazione luminosita<br />");
			$("#set_date_time .menuTitleLine").html("Regolazione hora y fecha<br />");
			$("#set_language .menuTitleLine").html("Scelta della lingua<br />");
			$("#set_units .menuTitleLine").html("Regolazione formato e unita<br />");

			// $("#satnav_initializing_popup .messagePopupArea").html("Navigation system<br>being initialised");
			$("#satnav_input_stored_in_personal_dir_popup .messagePopupArea").html("\
				L'informazione &egrave; stata<br />salvata nella rubrica<br />personale");
			$("#satnav_input_stored_in_professional_dir_popup .messagePopupArea").html("\
				L'informazione &egrave; stata<br />salvata nella rubrica<br />professionale");
			$("#satnav_computing_route_popup .messagePopupArea").html("Calcolo dell'itinerario<br />in corso");

			$("#satnav_disclaimer_text").html("\
				La navigazione fornisce un'assistenza<br>\
				elettronica. In nessun caso pu&ograve; sostituirsi<br>\
				a l'analisi del conducente. Qualsiasi<br>\
				istruzione deve essere attentamente<br>\
				verificata dall'utente<br>");

			notDigitizedAreaText = "Area non mappata";
			cityCenterText = "Centro della citt&agrave;";
			changeText = "Modificare";
			noNumberText = "Nessun numero";
			stopGuidanceText = "Interrompere la guida";
			resumeGuidanceText = "Riprendere la guida";
			streetNotListedText = "Strada non elencata";  // TODO - check

			$("#satnav_main_menu .menuTitleLine").html("Navigazione/Guida<br />");
			$("#satnav_main_menu .button:eq(0)").html("Inserire una nuova destinazione");
			$("#satnav_main_menu .button:eq(1)").html("Scelta un servizio");
			$("#satnav_main_menu .button:eq(2)").html("Scelta una destinazione salvata");
			$("#satnav_main_menu .button:eq(3)").html("Opzioni di navigazione");

			$("#satnav_select_from_memory_menu .menuTitleLine").html("Scelta una destinazione salvata<br />");
			$("#satnav_select_from_memory_menu .button:eq(0)").html("Rub. personale");
			$("#satnav_select_from_memory_menu .button:eq(1)").html("Rub. professionale");

			$("#satnav_navigation_options_menu .menuTitleLine").html("Opzioni di navigazione<br />");
			$("#satnav_navigation_options_menu .button:eq(0)").html("Gestione rubrica");
			$("#satnav_navigation_options_menu .button:eq(1)").html("Volume navigazione");
			$("#satnav_navigation_options_menu .button:eq(2)").html("Cancella rubrica");
			$("#satnav_navigation_options_menu .button:eq(3)").html(
				satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);

			$("#satnav_directory_management_menu .menuTitleLine").html("Gestione rubrica<br />");
			$("#satnav_directory_management_menu .button:eq(0)").html("Rub. personale");
			$("#satnav_directory_management_menu .button:eq(1)").html("Rub. professionale");

			$("#satnav_guidance_tools_menu .menuTitleLine").html("Strumenti de guida<br />");
			$("#satnav_guidance_tools_menu .button:eq(0)").html("Criteri di guida");
			$("#satnav_guidance_tools_menu .button:eq(1)").html("Destinazione programmata");
			$("#satnav_guidance_tools_menu .button:eq(2)").html("Volume navigazione");
			$("#satnav_guidance_tools_menu .button:eq(3)").html(stopGuidanceText);

			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").html("Percorso pi&ugrave; rapido<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").html("Percorso pi&ugrave; corto<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").html("Esclusione autostrada<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").html("Compromesso rapidit&agrave;/distanza<br />");

			$("#satnav_vocal_synthesis_level .menuTitleLine").html("Volume navigazione<br />");
			$("#satnav_vocal_synthesis_level .tag").html("Volume");

			$("#satnav_enter_city_characters .tag").html("Selezionare citt&agrave;");
			$("#satnav_enter_street_characters .tag").html("Selezionare via");

			$("#satnav_enter_characters .tag:eq(0)").html("Scegli lettera");  // TODO - check
			$("#satnav_enter_characters .tag:eq(1)").html("Selezionare citt&agrave;");
			$("#satnav_enter_characters .tag:eq(2)").html("Selezionare via");

			$("#satnav_tag_city_list").html("Selezionare citt&agrave;");
			$("#satnav_tag_street_list").html("Selezionare via");
			$("#satnav_tag_service_list").html("Scelta un servizio");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Rubrica personale");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Rubrica professionale");  // TODO - check

			$("#satnav_enter_house_number .tag").html("Selezionare numero civico");

			$("#satnav_show_personal_address .tag:eq(0)").html("Citt&agrave;");
			$("#satnav_show_personal_address .tag:eq(1)").html("Via");
			$("#satnav_show_personal_address .tag:eq(2)").html("Numero");

			$("#satnav_show_professional_address .tag:eq(0)").html("Citt&agrave;");
			$("#satnav_show_professional_address .tag:eq(1)").html("Via");
			$("#satnav_show_professional_address .tag:eq(3)").html("Numero");

			$("#satnav_show_service_address .tag:eq(0)").html("Citt&agrave;");
			$("#satnav_show_service_address .tag:eq(1)").html("Via");

			$("#satnav_show_current_destination .tag:eq(0)").html("Destinazione programmata");
			$("#satnav_show_current_destination .tag:eq(1)").html("Citt&agrave;");
			$("#satnav_show_current_destination .tag:eq(2)").html("Via");
			$("#satnav_show_current_destination .tag:eq(3)").html("Numero");

			$("#satnav_show_programmed_destination .tag:eq(0)").html("Destinazione programmata");
			$("#satnav_show_programmed_destination .tag:eq(1)").html("Citt&agrave;");
			$("#satnav_show_programmed_destination .tag:eq(2)").html("Via");
			$("#satnav_show_programmed_destination .tag:eq(3)").html("Numero");

			$("#satnav_show_last_destination .tag:eq(0)").html("Scelta un servizio");
			$("#satnav_show_last_destination .tag:eq(1)").html("Citt&agrave;");
			$("#satnav_show_last_destination .tag:eq(2)").html("Via");
			$("#satnav_show_last_destination .button:eq(2)").html("Posizione attuale");  // TODO - check

			$("#satnav_archive_in_directory_title").html("Salvare nella rubrica");
			$("#satnav_archive_in_directory .satNavEntryNameTag").html("Denominazione");
			$("#satnav_archive_in_directory .button:eq(0)").html("Correggere");
			$("#satnav_archive_in_directory .button:eq(1)").html("Rub. personale");
			$("#satnav_archive_in_directory .button:eq(2)").html("Rub. professionale");

			$("#satnav_rename_entry_in_directory_title").html("Rinominare");
			$("#satnav_rename_entry_in_directory .satNavEntryNameTag").html("Denominazione");
			$("#satnav_rename_entry_in_directory .button:eq(1)").html("Correggere");

			$("#satnav_reached_destination_popup_title").html("Destinazione raggiunta");  // TODO - check
			$("#satnav_delete_item_popup_title").html("Cancellare la scheda?<br />");
			$("#satnav_guidance_preference_popup_title").html("Conservare il criterio");
			$("#satnav_delete_directory_data_popup_title").html("I dati delle rubrica<br />verranno cancellati");
			$("#satnav_continue_guidance_popup_title").html("Continuare la navigazione?");  // TODO - check

			$(".yesButton").html("S&igrave;");
			$(".noButton").html("No");

			$(".validateButton").html("Convalidare");
			$("#satnav_disclaimer_validate_button").html("Convalidare");
			$("#satnav_manage_personal_address_rename_button").html("Rinominare");
			$("#satnav_manage_personal_address_delete_button").html("Eliminare");
			$("#satnav_manage_professional_address_rename_button").html("Rinominare");
			$("#satnav_manage_professional_address_delete_button").html("Eliminare");
			$("#satnav_service_address_previous_button").html("Precedente");
			$("#satnav_service_address_next_button").html("Seguente");

			$(".correctionButton").html(changeText);
			$("#satnav_enter_characters_correction_button").html("Correggere");
			$("#satnav_to_mfd_list_tag").html("Lista");
			$("#satnav_store_entry_in_directory").html("Memorizzare");

			$(".satNavEntryExistsTag").html("Denominazione esiste gi&agrave;");  // TODO - check
		} // case
		break;

		case "set_language_dutch":
		{
			doorOpenText = "Deur open";
			doorsOpenText = "Deuren open";
			bootOpenText = "Kofferbak open";
			bootAndDoorOpenText = "Deur en kofferbak open";
			bootAndDoorsOpenText = "Deuren en kofferbak open";

			$("#main_menu .menuTitleLine").html("Hoofdmenu<br />");
			$("#main_menu_goto_satnav_button").html("Navigatie / Begeleiding");
			$("#main_menu_goto_screen_configuration_button").html("Beeldschermconfiguratie");

			$("#screen_configuration_menu .menuTitleLine").html("Beeldschermconfiguratie<br />");
			$("#screen_configuration_menu .button:eq(0)").html("Instelling helderheid");
			$("#screen_configuration_menu .button:eq(1)").html("Instelling datum en tijd");
			$("#screen_configuration_menu .button:eq(2)").html("Taalkeuze " + languageSelections);
			$("#screen_configuration_menu .button:eq(3)").html("Instelling van eenheden");

			$("#set_screen_brightness .menuTitleLine").html("Instelling helderheid<br />");
			$("#set_date_time .menuTitleLine").html("Instelling datum en tijd<br />");
			$("#set_language .menuTitleLine").html("Taalkeuze<br />");
			$("#set_units .menuTitleLine").html("Instelling van eenheden<br />");

			$("#satnav_initializing_popup .messagePopupArea").html("Initialisering navigator");
			$("#satnav_input_stored_in_personal_dir_popup .messagePopupArea").html("\
				Gegevens opgeslagen in<br />priv&eacute;-bestand");
			$("#satnav_input_stored_in_professional_dir_popup .messagePopupArea").html("\
				Gegevens opgeslagen in<br />zaken-bestand");
			$("#satnav_computing_route_popup .messagePopupArea").html("Routeberekening");

			$("#satnav_disclaimer_text").html("\
				De navigator is een elektronisch<br>\
				hulpsysteem dat de beslissing van de<br>\
				bestuurder niet kan vervangen.<br>\
				De gebruiker moet elke aanwijzing<br>\
				nauwkeurig controleren.<br>");

			notDigitizedAreaText = "Zone niet in kaart";
			cityCenterText = "Stadscentrum";
			changeText = "Wijzigen";
			noNumberText = "Geen nummer";
			stopGuidanceText = "Stop navigatie";
			resumeGuidanceText = "Laatste bestemming";
			streetNotListedText = "Straat onbekend";  // TODO - check

			$("#satnav_main_menu .menuTitleLine").html("Navigatie/ Begeleiding<br />");
			$("#satnav_main_menu .button:eq(0)").html("Nieuwe bestemming invoeren");
			$("#satnav_main_menu .button:eq(1)").html("Informatie-dienstverlening");
			$("#satnav_main_menu .button:eq(2)").html("Bestemming uit archief kiezen");
			$("#satnav_main_menu .button:eq(3)").html("Navigatiekeuze");

			$("#satnav_select_from_memory_menu .menuTitleLine").html("Bestemming uit archief kiezen<br />");
			$("#satnav_select_from_memory_menu .button:eq(0)").html("Priv&eacute;-adressen");
			$("#satnav_select_from_memory_menu .button:eq(1)").html("Zaken-adressen");

			$("#satnav_navigation_options_menu .menuTitleLine").html("Navigatiekeuze<br />");
			$("#satnav_navigation_options_menu .button:eq(0)").html("Adressenbestand");
			$("#satnav_navigation_options_menu .button:eq(1)").html("Geluidsvolume");
			$("#satnav_navigation_options_menu .button:eq(2)").html("Bestanden wissen");
			$("#satnav_navigation_options_menu .button:eq(3)").html(
				satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);

			$("#satnav_directory_management_menu .menuTitleLine").html("Adressenbestand<br />");
			$("#satnav_directory_management_menu .button:eq(0)").html("Priv&eacute;-adressen");
			$("#satnav_directory_management_menu .button:eq(1)").html("Zaken-adressen");

			$("#satnav_guidance_tools_menu .menuTitleLine").html("Navigatiehulp<br />");
			$("#satnav_guidance_tools_menu .button:eq(0)").html("Navigatiecriteria");
			$("#satnav_guidance_tools_menu .button:eq(1)").html("Geprogrammeerde bestemming");
			$("#satnav_guidance_tools_menu .button:eq(2)").html("Geluidsvolume");
			$("#satnav_guidance_tools_menu .button:eq(3)").html(stopGuidanceText);

			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(0)").html("Snelste weg<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(1)").html("Kortste afstand<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(2)").html("Traject zonder snelweg<br />");
			$("#satnav_guidance_preference_menu .tickBoxLabel:eq(3)").html("Compromis snelheid/afstand<br />");

			$("#satnav_vocal_synthesis_level .menuTitleLine").html("Geluidsvolume<br />");
			$("#satnav_vocal_synthesis_level .tag").html("Volume");

			$("#satnav_enter_city_characters .tag").html("Stad invoeren");
			$("#satnav_enter_street_characters .tag").html("Straat invoeren");

			$("#satnav_enter_characters .tag:eq(0)").html("Kies letter");
			$("#satnav_enter_characters .tag:eq(1)").html("Stad invoeren");
			$("#satnav_enter_characters .tag:eq(2)").html("Straat invoeren");

			$("#satnav_tag_city_list").html("Kies stad");
			$("#satnav_tag_street_list").html("Kies straat");
			$("#satnav_tag_service_list").html("Kies dienst");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Kies priv&eacute;-adres");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Kies zaken-adres");  // TODO - check

			$("#satnav_enter_house_number .tag").html("Nummer invoeren");

			$("#satnav_show_personal_address .tag:eq(0)").html("Stad");
			$("#satnav_show_personal_address .tag:eq(1)").html("Straat");
			$("#satnav_show_personal_address .tag:eq(2)").html("Nummer");

			$("#satnav_show_professional_address .tag:eq(0)").html("Stad");
			$("#satnav_show_professional_address .tag:eq(1)").html("Straat");
			$("#satnav_show_professional_address .tag:eq(3)").html("Nummer");

			$("#satnav_show_service_address .tag:eq(0)").html("Stad");
			$("#satnav_show_service_address .tag:eq(1)").html("Straat");

			$("#satnav_show_current_destination .tag:eq(0)").html("Geprogrammeerde bestemming");
			$("#satnav_show_current_destination .tag:eq(1)").html("Stad");
			$("#satnav_show_current_destination .tag:eq(2)").html("Straat");
			$("#satnav_show_current_destination .tag:eq(3)").html("Nummer");

			$("#satnav_show_programmed_destination .tag:eq(0)").html("Geprogrammeerde bestemming");
			$("#satnav_show_programmed_destination .tag:eq(1)").html("Stad");
			$("#satnav_show_programmed_destination .tag:eq(2)").html("Straat");
			$("#satnav_show_programmed_destination .tag:eq(3)").html("Nummer");

			$("#satnav_show_last_destination .tag:eq(0)").html("Informatie-dienstverlening");
			$("#satnav_show_last_destination .tag:eq(1)").html("Stad");
			$("#satnav_show_last_destination .tag:eq(2)").html("Straat");
			$("#satnav_show_last_destination .button:eq(2)").html("Huidige locatie");

			$("#satnav_archive_in_directory_title").html("Opslaan in adressenbestand");  // TODO - check
			$("#satnav_archive_in_directory .satNavEntryNameTag").html("Naam");
			$("#satnav_archive_in_directory .button:eq(0)").html("Verbeteren");
			$("#satnav_archive_in_directory .button:eq(1)").html("Priv&eacute;-adressen");
			$("#satnav_archive_in_directory .button:eq(2)").html("Zaken-adressen");

			$("#satnav_rename_entry_in_directory_title").html("Nieuwe naam");
			$("#satnav_rename_entry_in_directory .satNavEntryNameTag").html("Naam");
			$("#satnav_rename_entry_in_directory .button:eq(1)").html("Verbeteren");

			$("#satnav_reached_destination_popup_title").html("Bestemming bereikt");  // TODO - check
			$("#satnav_delete_item_popup_title").html("Wilt u dit gegeven<br />wissen?<br />");
			$("#satnav_guidance_preference_popup_title").html("Bewaar de gegevens");
			$("#satnav_delete_directory_data_popup_title").html("Pas op : alle gegevens<br />worden gewist");
			$("#satnav_continue_guidance_popup_title").html("Navigatie voortzetten?");  // TODO - check

			$(".yesButton").html("Ja");
			$(".noButton").html("Nee");

			$(".validateButton").html("Bevestigen");
			$("#satnav_disclaimer_validate_button").html("Bevestigen");
			$("#satnav_manage_personal_address_rename_button").html("Nieuwe naam");
			$("#satnav_manage_personal_address_delete_button").html("Wissen");
			$("#satnav_manage_professional_address_rename_button").html("Nieuwe naam");
			$("#satnav_manage_professional_address_delete_button").html("Wissen");
			$("#satnav_service_address_previous_button").html("Vorige");
			$("#satnav_service_address_next_button").html("Volgende");

			$(".correctionButton").html(changeText);
			$("#satnav_enter_characters_correction_button").html("Verbeteren");
			$("#satnav_to_mfd_list_tag").html("Lijst");
			$("#satnav_store_entry_in_directory").html("Bewaren");

			$(".satNavEntryExistsTag").html("Deze naam bestaat al");
		} // case
		break;
	} // switch

	localStorage.mfdLanguage = language;

	$("#door_open_popup_text").html(doorOpenText);
	satnavGuidanceSetPreference(localStorage.satnavGuidancePreference);
} // setLanguage

function setUnits(distanceUnit, temperatureUnit, timeUnit)
{
	if (distanceUnit === "set_units_mph")
	{
		$('[gid="fuel_level_filtered_unit"]').text("gl");
		$('[gid="fuel_consumption_unit"]').text("mpg");
		$("#fuel_consumption_unit_sm").text("mpg");
		$('[gid="speed_unit"]').text("mph");
		$('[gid="distance_unit"]').text("mi");
	}
	else
	{
		$('[gid="fuel_level_filtered_unit"]').text("lt");
		$('[gid="fuel_consumption_unit"]').text("l/100 km");
		$("#fuel_consumption_unit_sm").text("/100");
		$('[gid="speed_unit"]').text("km/h");
		$('[gid="distance_unit"]').text("km");
	} // if

	if (temperatureUnit === "set_units_deg_fahrenheit")
	{
		$("#coolant_temp_unit").html("&deg;F");
		$("#climate_control_popup .tag:eq(1)").html("&deg;F");
		$("#climate_control_popup .tag:eq(3)").html("&deg;F");
	}
	else
	{
		$("#coolant_temp_unit").html("&deg;C");
		$("#climate_control_popup .tag:eq(1)").html("&deg;C");
		$("#climate_control_popup .tag:eq(3)").html("&deg;C");
	} // if
} // setUnits

// To be called by the body "onload" event
function htmlBodyOnLoad()
{
	setUnits(localStorage.mfdDistanceUnit, localStorage.mfdTemperatureUnit, localStorage.mfdTimeUnit);
	setLanguage(localStorage.mfdLanguage);

	gotoSmallScreen(localStorage.smallScreen);

	// Update time now and every next second
	updateDateTime();
	setInterval(updateDateTime, 1000);

	connectToWebSocket();
} // htmlBodyOnLoad

)=====";
