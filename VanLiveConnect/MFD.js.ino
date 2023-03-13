
char mfd_js[] PROGMEM = R"=====(

// Javascript functions to drive the browser-based Multi-Function Display (MFD)

// -----
// General

var thousandsSeparator = ",";

function addThousandsSeparator(x)
{
	return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, thousandsSeparator);
}

function clamp(num, min, max)
{
	return Math.min(Math.max(num, min), max);
}

function incModulo(i, mod)
{
	return (i + 1) % mod;
}

// -----
// On-screen clocks

// Capitalize first letter
function CapFirstLetter(string)
{
	var firstLetter = string.charAt(0);
	if (string.charCodeAt(0) == 8206)  // IE11-special
	{
		firstLetter = string.charAt(1);
		return firstLetter.toUpperCase() + string.slice(2);
	} // if
	return firstLetter.toUpperCase() + string.slice(1);
}

// Show the current date and time
function updateDateTime()
{
	var locales =
	{
		"set_language_french": "fr",
		"set_language_german": "de-DE",
		"set_language_spanish": "es-ES",
		"set_language_italian": "it",
		"set_language_dutch": "nl"
	};
	var locale = locales[localStorage.mfdLanguage] || "en-GB";

	var date = new Date().toLocaleDateString(locale, {weekday: 'short', day: 'numeric', month: 'short'});
	date = date.replace(/0(\d)/, "$1");  // No leading "0"
	date = date.replace(/,/, "");
	$("#date_small").text(CapFirstLetter(date));

	date = new Date().toLocaleDateString(locale, {weekday: 'long'});
	$("#date_weekday").text(CapFirstLetter(date) + ",");

	date = new Date().toLocaleDateString(locale, {day: 'numeric', month: 'long', year: 'numeric'});
	date = date.replace(/0(\d )/, "$1");  // No leading "0"
	$("#date").text(date);

	var time = new Date().toLocaleTimeString(
		[],  // When 12-hour time unit is chosen, 'a.m.' / 'p.m.' will be with dots, and in lower case
		{
			hour: "numeric",
			minute: "2-digit",
			hour12: localStorage.mfdTimeUnit === "set_units_12h"
		}
	);
	if (locale === "nl" || locale === "es-ES") time = time.replace(/0(\d:)/, "$1");  // No leading "0"
	$("#time").text(time);
	$("#time_small").text(time.replace(/.m.$/, ""));  // No trailing '.m.'
}

// -----
// System

document.addEventListener("visibilitychange", function()
{
	if (document.visibilityState === 'visible') connectToWebSocket(); else webSocket.close("browser tab no longer active");
});

function showViewportSizes()
{
	$("#viewport_width").text($(window).width());  // width of browser viewport
	$("#screen_width").text(window.screen.width);
	$("#window_width").text(window.innerWidth);

	$("#viewport_height").text($(window).height());  // height of browser viewport
	$("#screen_height").text(window.screen.height);
	$("#window_height").text(window.innerHeight);
}

// -----
// IR remote control

var ignoringIrCommands = false;

// Normally, holding the "VAL" button on the IR remote control will not cause repetition.
// The only exceptions are the '+' and '-' buttons in specific menu screens.
var acceptingHeldValButton = false;

// -----
// Functions for parsing and handling VAN bus packet data as received in JSON format

function processJsonObject(item, jsonObject)
{
	var selectorById = '#' + item;  // Select by 'id' attribute (must be unique in the DOM)
	var selectorByAttr = '[gid="' + item + '"]';  // Select also by custom attribute 'gid' (need not be unique)

	// jQuery-style loop over merged, unique-element array
	$.each($.unique($.merge($(selectorByAttr), $(selectorById))), function (index, selector)
	{
		if ($(selector).length <= 0) return;  // Go to next iteration in .each()

		if (Array.isArray(jsonObject))
		{
			// Handling of multi-line DOM objects to show lists
			$(selector).html(jsonObject.join('<br />'));
		}
		else if (!!jsonObject && typeof(jsonObject) === "object")
		{
			// Handling of "change attribute" events
			let attributeObj = jsonObject;
			for (let attribute in attributeObj)
			{
				// Deeper nesting?
				if (typeof(attributeObj) === "object")
				{
					let propertyObj = attributeObj[attribute];
					for (let property in propertyObj)
					{
						let value = propertyObj[property];
						$(selector).get(0)[attribute][property] = value;
					} // for
				}
				else
				{
					let attrValue = attributeObj[attribute];
					$(selector).attr(attribute, attrValue);
				} // if
			} // for
		}
		else
		{
			let itemText = jsonObject;
			if ($(selector).hasClass("led"))
			{
				// Handle "led" class objects: no text copy, just turn ON or OFF
				let on = itemText === "ON" || itemText === "YES";
				$(selector).toggleClass("ledOn", on);
				$(selector).toggleClass("ledOff", ! on);
			}
			else if ($(selector).hasClass("icon") || $(selector).get(0) instanceof SVGElement)
			{
				// Handle SVG elements and "icon" class objects: no text copy, just show or hide
				let on = itemText === "ON" || itemText === "YES";
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
	handleItemChange(item, jsonObject);
}

function writeToDom(jsonObj)
{
	// The following log entries can be used to literally re-play a session; simply copy-paste these lines into the
	// console area of the web-browser. Also it can be really useful to copy and save these lines into a text file
	// for later re-playing at your desk.

	var now = Date.now();
	if (writeToDom.lastInvoked === undefined)
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
		let elapsed = now - writeToDom.lastInvoked;
		if (elapsed > 20000) elapsed = 20000;  // During replay, don't wait longer than 20 seconds
		console.log("await sleep(" + elapsed + "); //");
	} // if
	console.log("writeToDom(" + JSON.stringify(jsonObj) + "); //");

	writeToDom.lastInvoked = now;

	for (let item in jsonObj)
	{
		// A nested object with an item name that ends with '_' indicates namespace
		if (item.slice(-1) === "_" && !!jsonObj[item] && typeof(jsonObj[item]) === "object")
		{
			let subObj = jsonObj[item];
			for (let subItem in subObj) processJsonObject(item + subItem, subObj[subItem]);  // Use 'item' as prefix
		}
		else
		{
			processJsonObject(item, jsonObj[item]);
		} // if
	} // for
}

// -----
// Functions for handling the WebSocket

// Inspired by https://gist.github.com/ismasan/299789
var fancyWebSocket = function(url)
{
	var conn = new WebSocket(url);

	var callbacks = {};

	this.bind = function(event_name, callback)
	{
		callbacks[event_name] = callbacks[event_name] || [];
		callbacks[event_name].push(callback);
		return this;  // chainable
	}; // function

	this.close = function(msg)
	{
		console.log("// Closing websocket'" + url + (msg ? "', reason: " + msg : ""));
		conn.close();
	}; // function

	this.send = function(data)
	{
		if (conn.readyState === 1) conn.send(data);
	}; // function

	// Dispatch to the right handlers
	conn.onmessage = function(evt)
	{
		try
		{
			var json = JSON.parse(evt.data);
		}
		catch(e)
		{
			console.log("// Error parsing json data '" + evt.data + "'!");
		}

		dispatch(json.event, json.data);
	}; // function

	conn.onopen = function()
	{
		console.log("// Connected to WebSocket '" + url + "'!");
		dispatch('open', null);
	}; // function

	conn.onclose = function(event)
	{
		dispatch('close', null);

		if (event.code == 3001)
		{
			console.log("// WebSocket '" + url + "' closed");
		}
		else
		{
			console.log("// WebSocket '" + url + "' connection error: " + event.code);
			connectToWebSocket();
		} // if
	}; // function

	conn.onerror = function(event)
	{
		if (conn.readyState == 1) console.log("// WebSocket '" + url + "' normal error: " + event.type);
	}; // function

	var dispatch = function(event_name, message)
	{
		var chain = callbacks[event_name];
		if (chain === undefined) return;  // No callbacks for this event
		for (let i = 0; i < chain.length; i++) chain[i](message);
	}; // function
}; // fancyWebSocket

var webSocketServerHost = window.location.hostname;

var webSocket;

// Send some data so that a webSocket event triggers when the connection has failed
var keepAliveWebSocketTimer;
function keepAliveWebSocket()
{
	webSocket.send("keepalive");
}

// Prevent double re-connecting when user reloads the page
var websocketPreventConnect = false;
window.onbeforeunload = function() { websocketPreventConnect = true; };

function connectToWebSocket()
{
	if (websocketPreventConnect || document.hidden) return;

	var wsUrl = "ws://" + webSocketServerHost + ":81/";
	console.log("// Connecting to WebSocket '" + wsUrl + "'");

	// Create WebSocket class instance
	webSocket = new fancyWebSocket(wsUrl);

	// Bind WebSocket to server events
	webSocket.bind
	(
		'display', function(data)
		{
			// Re-start the "keep alive" timer
			clearInterval(keepAliveWebSocketTimer);
			keepAliveWebSocketTimer = setInterval(keepAliveWebSocket, 6000);

			writeToDom(data);
		}
	);
	webSocket.bind
	(
		'open', function ()
		{
			webSocket.send("mfd_language:" + localStorage.mfdLanguage);
			webSocket.send("mfd_distance_unit:" + localStorage.mfdDistanceUnit);
			webSocket.send("mfd_temperature_unit:" + localStorage.mfdTemperatureUnit);
			webSocket.send("mfd_time_unit:" + localStorage.mfdTimeUnit);

			// Send current date-time
			webSocket.send("time_offset:" + new Date().getTimezoneOffset() * -1);
			webSocket.send("date_time:" + Date.now());  // Unix epoch, in milliseconds

			// (Re-)start the "keep alive" timer
			clearInterval(keepAliveWebSocketTimer);
			keepAliveWebSocketTimer = setInterval(keepAliveWebSocket, 6000);
		}
	);
	webSocket.bind
	(
		'close', function ()
		{
			clearInterval(keepAliveWebSocketTimer);
		}
	);
}

// -----
// Functions for popups

// Associative array, using the popup element ID as key
var popupTimer = {};

// Hide the specified or the current (top) popup
function hidePopup(id)
{
	var popup;
	if (id) popup = $("#" + id); else popup = $(".notificationPopup:visible").last();
	if (popup.length === 0 || ! popup.is(":visible")) return false;

	popup.hide();

	if (webSocket) webSocket.send("mfd_popup_showing:NO");

	clearTimeout(popupTimer[id]);
	popupTimer[id] = undefined;

	// Perform "on_exit" action, if specified
	var onExit = popup.attr("on_exit");
	if (onExit) eval(onExit);

	return true;
}

// Show the specified popup, with an optional timeout
function showPopup(id, msec)
{
	var popup = $("#" + id);
	if (! popup.is(":visible"))
	{
		popup.show();

		// A popup can appear under another. In that case, don't register the time.
		let topLevelPopup = $(".notificationPopup:visible").slice(-1)[0];
		if (id === topLevelPopup.id) lastScreenChangedAt = Date.now();

		// Perform "on_enter" action, if specified
		let onEnter = popup.attr("on_enter");
		if (onEnter) eval(onEnter);
	} // if

	if (! msec) return;

	// Hide popup after specified milliseconds
	clearTimeout(popupTimer[id]);
	popupTimer[id] = setTimeout(function () { hidePopup(id); }, msec);
	return popupTimer[id];
}

function NotifyServerAboutPopup(id, msec, message)
{
	if (webSocket)
	{
		var messageText = message !== undefined ? (" \"" + message.replace(/<[^>]*>/g, ' ') + "\"") : "";
		webSocket.send("mfd_popup_showing:" + (msec === 0 ? 0xFFFFFFFF : msec) + " " + id + messageText);
	} // if
}

// Show a popup and send an update on the web socket, The software on the ESP needs to know when a popup is showing,
// e.g. to know when to ignore a "MOD" button press from the IR remote control.
function showPopupAndNotifyServer(id, msec, message)
{
	var timer = showPopup(id, msec);
	NotifyServerAboutPopup(id, msec, message);
	return timer;
}

// Hide all visible popups. Optionally, pass the ID of a popup not keep visible.
function hideAllPopups(except)
{
	var allPopups = $(".notificationPopup:visible");

	$.each(allPopups, function (index, selector)
	{
		if ($(selector).attr("id") !== except) hidePopup($(selector).attr("id"));
	});
}

// Hide the notification popup, but only if it is the specified one
function hideNotificationPopup(timer)
{
	if (timer === undefined) return;
	var popupTimer = popupTimer["notification_popup"];
	if (popupTimer === undefined) return;
	if (timer === popupTimer) hidePopup("notification_popup");
}

// Show the notification popup (with icon) with a message and an optional timeout. The shown icon is either "info"
// (isWarning == false, default) or "warning" (isWarning == true).
function showNotificationPopup(message, msec, isWarning)
{
	if (isWarning === undefined) isWarning = false;  // IE11 does not support default parameters

	// Show the required icon: "information" or "warning"
	$("#notification_icon_info").toggle(! isWarning);
	$("#notification_icon_warning").toggle(isWarning);

	// TODO - if popup is already showing with this message, return here

	hideAllPopups();

	$("#last_notification_message_on_mfd").html(message);  // Set the notification text
	return showPopupAndNotifyServer("notification_popup", msec, message);
}

// Show a simple status popup (no icon) with a message and an optional timeout
function showStatusPopup(message, msec)
{
	$("#status_popup_text").html(message);  // Set the popup text
	showPopupAndNotifyServer("status_popup", msec);  // Show the popup
}

function showAudioPopup(id)
{
	// This popup only appears in the following screens:
	if (currentLargeScreenId !== "satnav_guidance" && currentLargeScreenId !== "instruments") return;

	if (id === undefined)
	{
		var map =
		{
			"TUNER": "tuner_popup",
			"TAPE": "tape_popup",
			"CD": "cd_player_popup",
			"CD_CHANGER": "cd_changer_popup"
		};

		var audioSource = $("#audio_source").text();
		id = map[audioSource] || "";
	} // if

	if (! id) return;

	if (! $("#audio_popup").is(":visible")) hidePopup();  // Hide other popup, if showing
	showPopupAndNotifyServer("audio_popup", 8000);
	$("#" + id).siblings("div[id$=popup]").hide();
	$("#" + id).show();
}

// If the "trip_computer_popup" is has no tab selected, select that of the current small screen
function initTripComputerPopup()
{
	// Retrieve all tab buttons
	var tabButtons = $("#trip_computer_popup .tabLeft");

	// Retrieve current tab button
	var currActiveButton = $("#trip_computer_popup .tabLeft.tabActive");

	var currButtonIdx = tabButtons.index(currActiveButton);
	if (currButtonIdx >= 0) return;

	// No tab selected

	var mapping =
	{
		"TRIP_INFO_1": "trip_computer_popup_trip_1",
		"TRIP_INFO_2": "trip_computer_popup_trip_2"
	};
	var selectedId = mapping[localStorage.smallScreen] || "trip_computer_popup_fuel_data";
	$("#" + selectedId).show();
	$("#" + selectedId + "_button").addClass("tabActive");
}

// Unselect any tab in the trip computer popup
function resetTripComputerPopup()
{
	$("#trip_computer_popup .tabContent").hide();
	$("#trip_computer_popup .tabLeft").removeClass("tabActive");
}

// In the "trip_computer_popup", select a specific tab
function selectTabInTripComputerPopup(index)
{
	// Unselect the current tab
	resetTripComputerPopup();

	// Retrieve all tab buttons and content elements
	var tabs = $("#trip_computer_popup .tabContent");
	var tabButtons = $("#trip_computer_popup .tabLeft");

	// Select the specified tab
	$(tabs[index]).show();
	$(tabButtons[index]).addClass("tabActive");
}

// -----
// Functions for navigating through the screens and their subscreens/elements

function isScreenFullSize()
{
	return document.fullscreenElement || document.mozFullScreenElement || document.webkitFullscreenElement || document.msFullscreenElement;
}

function resizeScreenToFit()
{
	let isMobile = window.matchMedia("(any-pointer:coarse)").matches;

	if (! isMobile)
	{
		$(":root").css("--scale-factor", (window.innerWidth - 10) / 1350 * window.devicePixelRatio);
		$("#body").css("background-size", window.devicePixelRatio * 100 + "%");
	}
	else
	{
		$(":root").css("--scale-factor", (window.innerWidth - 10) / 1350);
	} // if

	if (isScreenFullSize()) $("#full_screen_button").removeClass("fa-expand").addClass("fa-compress");
	else $("#full_screen_button").removeClass("fa-compress").addClass("fa-expand");
}

// Toggle full-screen mode
function toggleFullScreen()
{
	if (isScreenFullSize())
	{
		// Exit full screen mode
		if (document.exitFullscreen) document.exitFullscreen();
		else if (document.msExitFullscreen) document.msExitFullscreen();
		else if (document.mozCancelFullScreen) document.mozCancelFullScreen();
		else if (document.webkitExitFullscreen) document.webkitExitFullscreen();
	}
	else
	{
		// Enter full screen mode
		let elem = document.documentElement;
		if (elem.requestFullscreen) elem.requestFullscreen();
		else if (elem.mozRequestFullScreen) elem.mozRequestFullScreen(); // Firefox
		else if (elem.webkitRequestFullscreen) elem.webkitRequestFullscreen(); // Chrome and Safari
		else if (elem.msRequestFullscreen) elem.msRequestFullscreen(); // IE
	} // if
}

// Set visibility of an element by ID, together with all its parents in the 'div' hierarchy
function setVisibilityOfElementAndParents(id, value)
{
	var el = document.getElementById(id);
	while (el && el.tagName === "DIV")
	{
		el.style.display = value;
		el = el.parentNode;
	} // while
}

// Functions setting the large screen (right hand side of the display)

var currentLargeScreenId = "clock";  // Currently shown large screen. Initialize to the first screen visible.
var lastScreenChangedAt = 0;  // Last time the large screen changed

// Switch to a specific large screen
function changeLargeScreenTo(id)
{
	if (id === undefined || id === currentLargeScreenId) return;

	if ($("#" + id).length === 0) return alert("Oops: screen '" + id + "'does not exist!!");

	// Notify ESP so that it can use a slightly quicker repeat rate for the IR controller buttons
	var irFastRepeat =
		id === "satnav_choose_from_list" &&
		(
			handleItemChange.mfdToSatnavRequest === "service"
			|| handleItemChange.mfdToSatnavRequest === "personal_address_list"
			|| handleItemChange.mfdToSatnavRequest === "professional_address_list"
		);
	webSocket.send("ir_button_faster_repeat:" + (irFastRepeat ? "YES" : "NO"));

	hidePopup("audio_popup");

	// Perform current screen's "on_exit" action, if specified
	var onExit = $("#" + currentLargeScreenId).attr("on_exit");
	if (onExit) eval(onExit);

	setVisibilityOfElementAndParents(currentLargeScreenId, "none");
	setVisibilityOfElementAndParents(id, "block");

	// A screen can change under a popup. In that case, don't register the time
	if ($(".notificationPopup:visible").length === 0) lastScreenChangedAt = Date.now();

	currentLargeScreenId = id;

	if ($("#" + currentLargeScreenId + " .buttonBar").length > 0) restoreButtonSizes();

	// Perform new screen's "on_enter" action, if specified
	var onEnter = $("#" + currentLargeScreenId).attr("on_enter");
	if (onEnter) eval(onEnter);

	// Report back to server (ESP) that user is not browsing the menus
	if (! inMenu() && webSocket !== undefined) webSocket.send("in_menu:NO");
}

var changeBackScreenId;
var changeBackScreenTimer;
var changeBackScreenTimerEnd = 0;
var preventTemporaryScreenChangeTimer;

// (Re-)arm the timer to change back to the previous screen
function changeBackLargeScreenAfter(msec)
{
	clearTimeout(changeBackScreenTimer);
	changeBackScreenTimer = undefined;

	if (! changeBackScreenId) return;

	changeBackScreenTimer = setTimeout
	(
		function ()
		{
			changeBackScreenTimer = undefined;

			if (! changeBackScreenId) return;

			if (inMenu())
			{
				if (menuStack.length >= 1) menuStack[0] = changeBackScreenId;
			}
			else
			{
				changeLargeScreenTo(changeBackScreenId);
			} // if

			changeBackScreenId = undefined;
		},
		msec
	);

	changeBackScreenTimerEnd = Date.now() + msec;
}

// Temporarily switch to a specific screen on the right hand side of the display
function temporarilyChangeLargeScreenTo(id, msec)
{
	if (preventTemporaryScreenChangeTimer
		|| currentLargeScreenId === "pre_flight"
		|| currentLargeScreenId === "system")
	{
		return;
	} // if

	if (inMenu())
	{
		if (menuStack.length <= 0) return;
		if (! changeBackScreenId)
		{
			if (id === menuStack[0]) return;
			changeBackScreenId = menuStack[0];
		} // if
		menuStack[0] = id;
	}
	else
	{
		if (! changeBackScreenId)
		{
			if (id === currentLargeScreenId) return;
			changeBackScreenId = currentLargeScreenId;
		} // if
		changeLargeScreenTo(id);
	} // if

	// If timer is running and requested less than 10 seconds, do not shorten remaining time
	if (changeBackScreenTimer !== undefined && msec < 10000)
	{
		let msToEnd = changeBackScreenTimerEnd - Date.now();  // ms left to timeout
		if (msec < msToEnd) return;
	} // if

	changeBackLargeScreenAfter(msec);
}

function cancelChangeBackScreenTimer()
{
	clearTimeout(changeBackScreenTimer);
	changeBackScreenTimer = undefined;
	changeBackScreenId = undefined;
}

function preventTemporaryScreenChange(msec)
{
	cancelChangeBackScreenTimer();

	clearTimeout(preventTemporaryScreenChangeTimer);
	preventTemporaryScreenChangeTimer = setTimeout
	(
		function () { preventTemporaryScreenChangeTimer = null; },
		msec
	);
}

// Vehicle data
var contactKeyPosition;
var engineRpm = -1;
var engineRunning;

var menuStack = [];

function selectDefaultScreen(audioSource)
{
	// Ignore invocation during engine start
	if (currentLargeScreenId === "pre_flight" && contactKeyPosition === "START") return;

	preventTemporaryScreenChange(3000);  // No autonomous screen change for the next 3 seconds

	// We're no longer in any menu
	menuStack = [];
	currentMenu = undefined;

	var mapping =
	{
		"TUNER": "tuner",
		"TAPE": "tape",
		"CD": "cd_player",
		"INTERNAL_CD_OR_TAPE": "cd_player",
		"CD_CHANGER": "cd_changer"
	};

	var selectedScreenId = "";

	// Explicitly passed a value for 'audioSource'?
	if (audioSource !== undefined) selectedScreenId = mapping[audioSource] || "";

	if (! selectedScreenId && satnavMode === "IN_GUIDANCE_MODE") selectedScreenId = "satnav_guidance";

	if (! selectedScreenId)
	{
		audioSource = $("#audio_source").text();
		selectedScreenId = mapping[audioSource] || "";
	} // if

	// Show instrument screen if engine is running
	if (! selectedScreenId && engineRunning === "YES" && contactKeyPosition !== "OFF" && engineRpm >= 0)
	{
		selectedScreenId = "instruments";
	} // if

	// Show current street, if known
	if (! selectedScreenId && satnavCurrentStreet !== "") selectedScreenId = "satnav_current_location";

	// Final fallback screen...
	if (! selectedScreenId) selectedScreenId = "clock";

	changeLargeScreenTo(selectedScreenId);
}

// Cycle through the large screens (right hand side of the display)
function nextLargeScreen()
{

	if (inMenu()) return;  // Don't cycle through menu screens

	preventTemporaryScreenChange(10000);  // No autonomous screen change for the next 10 seconds

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
		"satnav_current_location"
	];

	var currIdx = screenIds.indexOf(currentLargeScreenId);  // -1 if not found
	var i = currIdx;
	var n = screenIds.length;

	i = incModulo(i, n);  // ID of the next screen in the sequence

	// Skip the "satnav_guidance" screen if the guidance is not active
	if (i === screenIds.indexOf("satnav_guidance") && satnavMode !== "IN_GUIDANCE_MODE") i = incModulo(i, n);

	var audioSource = $("#audio_source").text();

	// Skip the "tuner" screen if the radio is not the current source
	if (i === screenIds.indexOf("tuner") && audioSource !== "TUNER") i = incModulo(i, n);

	// Skip the "tape" screen if the cassette player is not the current source
	if (i === screenIds.indexOf("tape") && audioSource !== "TAPE") i = incModulo(i, n);

	// Skip the "cd_player" screen if the CD player is not the current source
	if (i === screenIds.indexOf("cd_player") && audioSource !== "CD") i = incModulo(i, n);

	// Skip the "cd_changer" screen if the CD changer is not the current source
	if (i === screenIds.indexOf("cd_changer") && audioSource !== "CD_CHANGER") i = incModulo(i, n);

	// Skip the "instruments" screen if the engine is not running
	if (i === screenIds.indexOf("instruments") && engineRunning !== "YES") i = incModulo(i, n);

	// Skip the "satnav_current_location" screen if in guidance mode, or if the current street is empty
	if (i === screenIds.indexOf("satnav_current_location"))
	{
		if (satnavMode === "IN_GUIDANCE_MODE") i = screenIds.indexOf("satnav_guidance");
		else if (satnavCurrentStreet === "")
		{
			let instrIdx = screenIds.indexOf("instruments")

			if (engineRunning == "YES" && currIdx !== instrIdx) i = instrIdx;
			else i = 0;  // Go back to the "clock" screen
		} // if
	} // if

	// After the "satnav_current_location" screen, go back to the "clock" screen
	if (i >= screenIds.length) i = 0;

	changeLargeScreenTo(screenIds[i]);
}

// Functions setting the small screen (left hand side of the display)

var currentSmallScreenId = "trip_info";  // Currently shown small screen. Initialize to the first screen visible.

// Switch to a specific small screen
function changeSmallScreenTo(id)
{
	if (currentSmallScreenId === id) return;

	setVisibilityOfElementAndParents(currentSmallScreenId, "none");
	setVisibilityOfElementAndParents(id, "block");
	currentSmallScreenId = id;
}

// Open a "trip_info" tab in the small (left) information panel
function openTripInfoTab(evt, tabName)
{
	$("#trip_info .tabContent").hide();
	$("#trip_info .tablinks").removeClass("active");
	$("#" + tabName).show();
	evt.currentTarget.className += " active";
}

function changeToTripCounter(id)
{
	// Simulate a "tab click" event
	var event = { currentTarget: document.getElementById(id + "_button")};
	openTripInfoTab(event, id);
}

// Only for debugging
function tripComputerShortStr(tripComputerStr)
{
	var mapping =
	{
		"TRIP_INFO_1": "TR1",
		"TRIP_INFO_2": "TR2",
		"GPS_INFO": "GPS",
		"FUEL_CONSUMPTION": "FUE"
	};
	return mapping[tripComputerStr] || "??";
}

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
			// Original MFD shows fuel consumption, but we already show that data permanently in the status bar.
			// Instead, show this screen:
			changeSmallScreenTo("instrument_small");
		} // case
		break;
	} // switch
}

// -----
// Functions for navigating through button sets and menus

var currentMenu;

function inMenu()
{
	var mainScreenIds =
	[
		"clock",
		"instruments", "pre_flight",
		"tuner", "tape", "cd_player", "cd_changer",
		"satnav_current_location",
		"satnav_guidance", "satnav_curr_turn_icon"
	];

	return currentMenu !== undefined
		&& mainScreenIds.indexOf(currentLargeScreenId) < 0;  // And not in one of the "main" screens?
}

// Select the first menu item (even if disabled)
function selectFirstMenuItem(id)
{
	var allButtons = $("#" + id).find(".button");
	$(allButtons[0]).addClass("buttonSelected");
	allButtons.slice(1).removeClass("buttonSelected");
}

// Enter a specific menu. Selects the first button, thereby skipping disabled buttons.
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
		var isButtonEnabled = $(buttons[nextIdx]).is(":visible") && ! $(buttons[nextIdx]).hasClass("buttonDisabled");
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
}

function gotoTopLevelMenu(menu)
{
	hidePopup();
	menuStack = [];
	if (menu === undefined) menu = "main_menu";

	// Report back to server (ESP) that user is browsing the menus
	if (webSocket !== undefined) webSocket.send("in_menu:YES");

	// This is the screen we want to go back to when pressing "Esc" on the remote control inside the top level menu
	currentMenu = currentLargeScreenId;

	gotoMenu(menu);
}

// Selects the specified button, thereby skipping disabled buttons, following "DOWN_BUTTON" / "RIGHT_BUTTON"
// attributes if and where present. Wraps if necessary. Also de-selects all other buttons within the same div.
function selectButton(id)
{
	var selectedButton = $("#" + id);

	// De-select all buttons within the same div
	var allButtons = selectedButton.parent().find(".button");
	allButtons.removeClass("buttonSelected");
	selectedButton.addClass("buttonSelected");
}

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
}

// Handle the 'Val' (enter) button
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
}

function upMenu()
{
	currentMenu = menuStack.pop();
	if (currentMenu) changeLargeScreenTo(currentMenu);
}

function exitMenu()
{
	// If specified, perform the current screen's "on_esc" action instead of the default "menu-up" action
	var onEsc = $("#" + currentLargeScreenId).attr("on_esc");
	if (onEsc) return eval(onEsc);
	upMenu();
}

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
}

function setTick(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	// In a group, only one tick box can be ticked at a time
	$("#" + id).parent().parent().find(".tickBox").each(function() { $(this).empty() } );

	$("#" + id).html("<b>&#10004;</b>");
}

function isValidTickId(groupId, valueId)
{
	return $("#" + groupId).find(".tickBox").map(function() { return this.id; }).get().indexOf(valueId) >= 0;
}

function getTickedId(groupId)
{
	return $("#" + groupId).find(".tickBox").filter(function() { return $(this).text(); }).attr("id");
}

function toggleTick(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	// In a group, only one tick box can be ticked at a time
	$("#" + id).parent().parent().find(".tickBox").each(function() { $(this).empty() } );

	$("#" + id).html($("#" + id).text() === "" ? "<b>&#10004;</b>" : "");
}

// Handle an arrow key press in a screen with buttons
function keyPressed(key)
{
	// Retrieve the currently selected button
	var selected = findSelectedButton();
	if (selected === undefined) return;

	var currentButton = selected.button;

	// Retrieve the specified action, like e.g. on_left_button="doSomething();"
	var action = currentButton.attr("on_" + key.toLowerCase());
	if (action) return eval(action);

	navigateButtons(key);
}

// As inspired by https://stackoverflow.com/a/2771544
function getTextWidth(selector)
{
	var htmlOrg = $(selector).html();
	$(selector).html('<span>' + htmlOrg + '</span>');
	var width = $(selector).find('span:first').width();
	$(selector).html(htmlOrg);
	return width;
}

// Associative array, using the button element ID as key
var buttonOriginalWidths = {};

function resizeButton(id)
{
	if (id === undefined) return;

	var button = $("#" + id);

	var widthAtLeast = getTextWidth(button);
	if (button.width() < widthAtLeast)
	{
		buttonOriginalWidths[id] = button.width();  // Save original width of button

		// Move left a bit if necessary
		let right = button.position().left + widthAtLeast;
		let moveLeft = right - 910;
		if (moveLeft > 0) button.css({ 'marginLeft': '-=' + moveLeft + 'px' });

		button.width(widthAtLeast);  // Resize button to fit text
		button.css('z-index', 1);  // Bring to front
	} // if
}

function restoreButtonSize(id)
{
	if (! (id in buttonOriginalWidths)) return;

	var elem = $("#" + id);
	elem.width(buttonOriginalWidths[id]);  // Reset width of button to original size
	elem.css({ 'marginLeft': '' });  // Place back in original position
	elem.css('z-index', '');  // No longer bring to front
}

function restoreButtonSizes()
{
	for (let id in buttonOriginalWidths) restoreButtonSize(id);
}

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
		let buttonOrientation = currentButton.parent().attr("button_orientation");
		let buttonForNext = buttonOrientation === "horizontal" ? "RIGHT_BUTTON" : "DOWN_BUTTON";
		let buttonForPrevious = buttonOrientation === "horizontal" ? "LEFT_BUTTON" : "UP_BUTTON";

		if (key !== buttonForNext && key !== buttonForPrevious) return;

		// Depending on the orientation of the buttons, the "DOWN_BUTTON"/"UP_BUTTON" resp.
		// "RIGHT_BUTTON"/"LEFT_BUTTON" simply walks the list, i.e. selects the next resp. previous button
		// in the list.
		let allButtons = screen.find(".button");
		let nButtons = allButtons.length;
		if (nButtons === 0) return;

		let currIdx = allButtons.index(currentButton);
		let nextIdx = currIdx;

		// Select the next button
		do
		{
			nextIdx = (nextIdx + (key === buttonForNext ? 1 : nButtons - 1)) % nButtons;
			if (nextIdx === currIdx) break;

			// Skip disabled or invisible buttons
			let isButtonEnabled =
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
		let checkedButtons = {};

		checkedButtons[currentButtonId] = true;

		let allButtons = screen.find(".button");
		let nButtons = allButtons.length;

		let gotoNext = key === "DOWN_BUTTON" || key === "RIGHT_BUTTON";

		// Skip disabled buttons, following the orientation of the buttons
		while ($("#" + gotoButtonId).hasClass("buttonDisabled"))
		{
			checkedButtons[gotoButtonId] = true;

			let buttonForNext = key;
			let buttonOrientation = $("#" + gotoButtonId).parent().attr("button_orientation");
			if (buttonOrientation === "horizontal")
			{
				buttonForNext = key === "DOWN_BUTTON" || key === "RIGHT_BUTTON" ? "RIGHT_BUTTON" : "LEFT_BUTTON";
			} // if

			let nextButtonId = $("#" + gotoButtonId).attr(buttonForNext);

			if (nextButtonId)
			{
				gotoButtonId = nextButtonId;
			}
			else
			{
				// Nothing specified? Then select the next resp. previous button in the list.
				let currIdx = allButtons.index($("#" + gotoButtonId));
				let nextIdx = (currIdx + (gotoNext ? 1 : nButtons - 1)) % nButtons;
				gotoButtonId = $(allButtons[nextIdx]).attr("id");
			} // if

			// No further button in that direction? Or went all the way around? Then give up.
			if (! gotoButtonId || checkedButtons[gotoButtonId]) return;
		} // while

		if (gotoButtonId === currentButtonId) return;  // Return if nothing changed

		gotoButton = $("#" + gotoButtonId);
	} // if

	restoreButtonSize(currentButtonId);

	// Perform "on_exit" action, if specified
	var onExit = currentButton.attr("on_exit");
	if (onExit) eval(onExit);

	currentButton.removeClass("buttonSelected");
	gotoButton.addClass("buttonSelected");

	resizeButton(gotoButtonId);

	// Perform "on_enter" action, if specified
	var onEnter = gotoButton.attr("on_enter");
	if (onEnter) eval(onEnter);
}

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
	if (text.length === 0) return;  // Nothing to highlight?

	if (index !== undefined)
	{
		index = clamp(index, 0, text.length);
		highlightIndexes[id] = index;
	} // if

	if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0;

	// Don't go before first letter or past last
	highlightIndexes[id] = clamp(highlightIndexes[id], 0, text.length - 1);

	text = text.substr(0, highlightIndexes[id])
		+ "<span class='invertedText'>" + text[highlightIndexes[id]] + "</span>"
		+ text.substr(highlightIndexes[id] + 1, text.length);
	$("#" + id).html(text);
}

// Remove any highlight from a text element
function unhighlightLetter(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	var text = $("#" + id).text();
	$("#" + id).html(text);
}

function highlightFirstLetter(id)
{
	highlightLetter(id, 0);
}

function highlightNextLetter(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	if (highlightIndexes[id] === undefined) highlightIndexes[id] = 0; else highlightIndexes[id]++;
	if (highlightIndexes[id] >= $("#" + id).text().length) highlightIndexes[id] = 0;  // Roll over
	highlightLetter(id);
}

function highlightPreviousLetter(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	var last = $("#" + id).text().length - 1;
	if (highlightIndexes[id] === undefined) highlightIndexes[id] = last; else highlightIndexes[id]--;
	if (highlightIndexes[id] < 0) highlightIndexes[id] = last;  // Roll over
	highlightLetter(id);
}

// -----
// Functions for navigating through lines in a list

// Split the lines of an element to an array
function splitIntoLines(id)
{
	// Puppy powerrrrrr :-)
	var html = $("#" + id).html();
	var brExp = /<br\s*\/?>/i;
	return html.split(brExp);
}

// Highlight a line in an element with lines
function highlightLine(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	var lines = splitIntoLines(id);
	if (lines.length === 0) return;  // Nothing to highlight?

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
}

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
}

function highlightFirstLine(id)
{
	id = getFocusId(id);
	if (id === undefined) return;

	unhighlightLine(id);
	highlightIndexes[id] = 0;
	highlightLine(id);

	$("#" + id).scrollTop(0);  // Make sure the top line is visible
}

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

	var scrollBy = (topOfHighlightedLine + heightOfHighlightedLine) - (heightOfBox - heightOfUnhighlightedLine);
	if (scrollBy > 0)
	{
		// Try to keep at least one next line visible
		$("#" + id).scrollTop($("#" + id).scrollTop() + scrollBy);
	} // if
}

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
	var heightOfUnhighlightedLine = parseFloat($("#" + id).css('line-height'));

	if (topOfHighlightedLine < heightOfUnhighlightedLine - 1)
	{
		// Try to keep at least one previous line visible
		$("#" + id).scrollTop($("#" + id).scrollTop() - heightOfUnhighlightedLine);
	}
}

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
}

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
}

// -----
// Audio settings and tuner presets popups

var updatingAudioVolume = false;
var isAudioMenuVisible = false;
var audioSettingsPopupHideTimer;
var audioSettingsPopupShowTimer;
var cdChangerCurrentDisc = "";
var tunerSearchMode = "";

function hideAudioSettingsPopup()
{
	clearTimeout(audioSettingsPopupShowTimer);
	clearTimeout(audioSettingsPopupHideTimer);
	updatingAudioVolume = false;
	isAudioMenuVisible = false;

	return hidePopup("audio_settings_popup");
}

function hideAudioSettingsPopupAfter(msec)
{
	// (Re-)arm the timer to hide the popup after the specified number of milliseconds
	clearTimeout(audioSettingsPopupHideTimer);
	audioSettingsPopupHideTimer = setTimeout(hideAudioSettingsPopup, msec);
}

// The IDs of the audio settings highlight boxes to be cycled through
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

	// Either reset to the first ID, or get the ID of the next box to highlight
	audioSettingHighlightIndex = goToFirst ? 0 : audioSettingHighlightIndex + 1;

	// Going past the last setting?
	if (audioSettingHighlightIndex >= highlightIds.length)
	{
		audioSettingHighlightIndex = 0;
		hideAudioSettingsPopup();
		return;
	} // if

	$("#" + highlightIds[audioSettingHighlightIndex]).show();  // Show the next highlight box

	updatingAudioVolume = false;
	isAudioMenuVisible = true;
}

function hideTunerPresetsPopup()
{
	return hidePopup("tuner_presets_popup");
}

// Show the audio volume popup
function showAudioVolumePopup()
{
	// Audio popup already visible due to display of audio settings?
	if (isAudioMenuVisible) return hideAudioSettingsPopupAfter(11500);

	hideTunerPresetsPopup();  // If the tuner presets popup is visible, hide it
	hidePopup("trip_computer_popup");
	hidePopup("audio_popup");

	// In the audio settings popup, unhighlight any audio setting
	$("#" + highlightIds[audioSettingHighlightIndex]).hide();

	if (inMenu()) return;  // Don't pop up when user is browsing the menus

	if ($("#contact_key_position").text() !== "" && $("#contact_key_position").text() !== "OFF")
	{
		$("#audio_settings_popup").show();
		NotifyServerAboutPopup("audio_settings_popup", 4000);
	}
	else
	{
		// With no contact key in place, we get a spurious "volume_update" message when turning off the
		// head unit. To prevent the audio settings popup to flash up and disappear, show it only after
		// 100 msec in this specific situation.
		clearTimeout(audioSettingsPopupShowTimer);
		audioSettingsPopupShowTimer = setTimeout(function ()
		{
			$("#audio_settings_popup").show();
			NotifyServerAboutPopup("audio_settings_popup", 4000);
		},
		100);
	} // if
	updatingAudioVolume = true;

	hideAudioSettingsPopupAfter(4000);  // Hide popup after 4 seconds
}

// Show the audio settings popup
function showAudioSettingsPopup(button)
{
	if (button === "AUDIO")
	{
		// If the tuner presets popup is visible, hide it
		// Note: on the original MFD, the tuner presets popup stays in the background, showing up again as soon
		// as the volume disappears. But that would become ugly here, because the tuner presets popup is not in
		// the same position as the audio settings popup.
		hideTunerPresetsPopup();

		// Show the audio settings popup
		$("#audio_settings_popup").show();
		NotifyServerAboutPopup("audio_settings_popup", 7500);

		// Highlight the first audio setting ("bass") if just popped up, else highlight next audio setting
		highlightAudioSetting(! isAudioMenuVisible);
	} // if

	// (Re-)start the popup visibility timer.
	if (isAudioMenuVisible) hideAudioSettingsPopupAfter(7500);
}

// Show the tuner presets popup
function showTunerPresetsPopup()
{
	$("#tuner_presets_popup").show();
	NotifyServerAboutPopup("tuner_presets_popup", 8500);

	// Hide the popup after 9 seconds
	clearTimeout(handleItemChange.tunerPresetsPopupTimer);
	handleItemChange.tunerPresetsPopupTimer = setTimeout(hideTunerPresetsPopup, 8500);
}

// -----
// Color theme and dimming

var headlightStatus = "";

function setColorTheme(theme)
{
	$(":root").css("--main-color", theme === "set_light_theme" ? "#271b42" : "hsl(215,42%,91%)");
	$(":root").css("--background-color", theme === "set_light_theme" ? "hsl(240,6%,91%)" : "rgb(8,7,19)");
	$(":root").css("--led-off-color", theme === "set_light_theme" ? "rgb(189,189,189)" : "rgb(25,31,40)");
	$(":root").css("--notification-color", theme === "set_light_theme" ? "rgba(205,209,213,0.95)" : "rgba(15,19,23,0.95)");
	$(":root").css("--highlight-color", theme === "set_light_theme" ? "rgba(84,101,125,0.4)" : "rgba(223,231,242,0.4)");
	$(":root").css("--selected-element-color", theme === "set_light_theme" ? "rgb(207,205,217)" : "rgb(41,55,74)");
	$(":root").css("--disabled-element-color", theme === "set_light_theme" ? "rgb(172,183,202)" : "rgb(67,82,105)");

	// Different background image (if available) for light theme
	$("body").css("background-image", theme === "set_light_theme" ? "url('background_light.jpg')" : "url('background.jpg')");

	setDimLevel(headlightStatus, theme);  // Will adjust main color (dark theme) or background color (light theme)
}

function colorThemeTickSet()
{
	toggleTick();
	setColorTheme(getTickedId("set_screen_brightness"));
}

function colorThemeSelectTickedButton()
{
	$("#set_screen_brightness .tickBox").removeClass("buttonSelected");

	var theme = localStorage.colorTheme;
	var id = "set_dark_theme";  // Default
	if (isValidTickId("set_screen_brightness", theme)) id = theme; else localStorage.colorTheme = id;
	setTick(id);
	$("#" + id).addClass("buttonSelected");  // On entry into units screen, select this button
}

function setLuminosity(luminosity, theme)
{
	luminosity = clamp(luminosity, 63, 91);
	if (theme === undefined)
	{
		theme = localStorage.colorTheme;
		if ($("#set_screen_brightness").is(":visible")) theme = getTickedId("set_screen_brightness");
	} // if

	if (theme === "set_light_theme") $(":root").css("--background-color", "hsl(240,6%," + luminosity + "%)");
	else $(":root").css("--main-color", "hsl(215,42%," + luminosity + "%)");

	return luminosity;
}

function initDimLevel()
{
	// Default dim level luminosity values
	if (! (localStorage.dimLevelReduced >= 0 && localStorage.dimLevelReduced <= 14)) localStorage.dimLevelReduced = 0;
	if (! (localStorage.dimLevel >= 0 && localStorage.dimLevel <= 14)) localStorage.dimLevel = 14;

	$("#display_brightness_level").text(localStorage.dimLevel);
	$("#display_reduced_brightness_level").text(localStorage.dimLevelReduced);
}

function setDimLevel(headlightStatus, theme)
{
	var headlightsOn = headlightStatus.match(/BEAM/) !== null;
	var id = headlightsOn ? "display_reduced_brightness_level" : "display_brightness_level";
	var dimLevel = parseInt($("#" + id).text());
	var luminosity = dimLevel * 2 + 63;
	setLuminosity(luminosity, theme);

	$("#display_brightness_level").toggle(! headlightsOn);
	$("#display_reduced_brightness_level").toggle(headlightsOn);
}

function adjustDimLevel(headlightStatus, button)
{
	// IMHO, it is better to trigger on the beam lights. Field "dash_light" triggers on any lights, including
	// parking lights, which is not really used when dark.
	var id = headlightStatus.match(/BEAM/) ? "display_reduced_brightness_level" : "display_brightness_level";

	// Luminosity is adjustable in 15 levels from 63 ... 91
	var luminosity = (parseInt($("#" + id).text()) * 2) + 63 + (button === "UP_BUTTON" ? 2 : -2);
	luminosity = setLuminosity(luminosity, getTickedId("set_screen_brightness"));

	$("#" + id).text((+luminosity - 63) / 2);

	return id;
}

function setBrightnessEscape()
{
	// Go back to saved settings
	initDimLevel();
	setColorTheme(localStorage.colorTheme);

	upMenu();
}

function setBrightnessValidate()
{
	localStorage.colorTheme = getTickedId("set_screen_brightness");
	localStorage.dimLevelReduced = $("#display_reduced_brightness_level").text();
	localStorage.dimLevel = $("#display_brightness_level").text();

	upMenu();
	upMenu();
	upMenu();
}

// -----
// Functions for selecting MFD language

var notDigitizedAreaText = "Not digitized area";
var cityCenterText = "City centre";
var changeText = "Change";
var noNumberText = "No number";
var stopGuidanceText = "Stop guidance";
var resumeGuidanceText = "Resume guidance";
var streetNotListedText = "Street not listed";

function setLanguageSelectTickedButton()
{
	$("#set_language .tickBox").removeClass("buttonSelected");

	var lang = localStorage.mfdLanguage;
	var id = "set_language_english";  // Default
	if (isValidTickId("set_language", lang)) id = lang; else localStorage.mfdLanguage = id;
	setTick(id);
	$("#" + id).addClass("buttonSelected");

	$("#set_language_validate_button").removeClass("buttonSelected");
}

function setLanguageTickSet()
{
	toggleTick();
	setLanguage(getTickedId("set_language"));
}

function setLanguageValidate()
{
	localStorage.mfdLanguage = getTickedId("set_language");
	webSocket.send("mfd_language:" + localStorage.mfdLanguage);

	// TODO - the original MFD sometimes shows a popup when the language is changed

	upMenu();
	upMenu();
	upMenu();
}

// -----
// Functions for selecting MFD formats and units

function invalidateAllDistanceFields()
{
	$('[gid="fuel_level"]').text("--.-");
	$('[gid="avg_consumption_1"]').text("--.-");
	$('[gid="avg_consumption_2"]').text("--.-");
	$('[gid="inst_consumption"]').text("--.-");
	$('[gid="avg_speed_1"]').text("--");
	$('[gid="avg_speed_2"]').text("--");
	$('[gid="distance_1"]').text("--");
	$('[gid="distance_2"]').text("--");
	$("#distance_to_service").text("---");
	$("#odometer_1").text("--.-");
	$('[gid="distance_to_empty"]').text("--");
}

function unitsSelectTickedButtons()
{
	$("#set_units .tickBox").removeClass("buttonSelected");

	var distUnit = localStorage.mfdDistanceUnit;
	var id = "set_units_km_h";  // Default
	if (isValidTickId("set_distance_unit", distUnit)) id = distUnit; else localStorage.mfdDistanceUnit = id;
	setTick(id);
	$("#" + id).addClass("buttonSelected");  // On entry into units screen, select this button

	var tempUnit = localStorage.mfdTemperatureUnit;
	id = "set_units_deg_celsius";  // Default
	if (isValidTickId("set_temperature_unit", tempUnit)) id = tempUnit; else localStorage.mfdTemperatureUnit = id;
	setTick(id);

	var timeUnit = localStorage.mfdTimeUnit;
	id = "set_units_24h";  // Default
	if (isValidTickId("set_time_unit", timeUnit)) id = timeUnit; else localStorage.mfdTimeUnit = id;
	setTick(id);

	$("#set_units_validate_button").removeClass("buttonSelected");
}

function unitsValidate()
{
	var newDistanceUnit = getTickedId("set_distance_unit");
	var newTemperatureUnit = getTickedId("set_temperature_unit");
	var newTimeUnit = getTickedId("set_time_unit");

	if (newDistanceUnit !== localStorage.mfdDistanceUnit) invalidateAllDistanceFields();

	setUnits(newDistanceUnit, newTemperatureUnit, newTimeUnit);

	webSocket.send("mfd_distance_unit:" + newDistanceUnit);
	webSocket.send("mfd_temperature_unit:" + newTemperatureUnit);
	webSocket.send("mfd_time_unit:" + newTimeUnit);

	upMenu();
	upMenu();
	upMenu();
}

var mfdLargeScreen = "CLOCK";  // Screen currently shown in the "large" (right hand side) area on the original MFD

var suppressClimateControlPopup = null;

function changeToInstrumentsScreen()
{
	// Suppress climate control popup during the next 2 seconds
	clearTimeout(suppressClimateControlPopup);
	suppressClimateControlPopup = setTimeout(function () { suppressClimateControlPopup = null; }, 2000);

	if (inMenu()) return;  // No screen change while browsing the menus

	changeLargeScreenTo("instruments");
}

// -----
// Functions for satellite navigation menu and screen handling

var satnavEquipmentPresent = false;
var satnavInitialized = false;

// Current sat nav mode, saved as last reported in item "satnav_status_2"
var satnavMode = "INITIALIZING";

var satnavRouteComputed = false;
var satnavDisclaimerAccepted = false;
var satnavLastEnteredChar = null;
var satnavToMfdResponse;
var satnavDestinationReachable = false;
var satnavGpsFix = false;
var satnavOnMap = false;
var satnavStatus1 = "";
var satnavStatus3 = "";
var satnavDestinationNotAccessible = false;
var satnavComputingRoute = false;
var satnavDisplayCanBeDimmed = true;

// Show this popup only once at start of guidance or after recalculation
var satnavDestinationNotAccessibleByRoadPopupShown = false;

var satnavCurrentStreet = "";
var satnavNextStreet = "";
var satnavDirectoryEntry = "";

var satnavServices = JSON.parse(localStorage.satnavServices || "[]");
var satnavPersonalDirectoryEntries = JSON.parse(localStorage.satnavPersonalDirectoryEntries || "[]");
var satnavProfessionalDirectoryEntries = JSON.parse(localStorage.satnavProfessionalDirectoryEntries || "[]");

var satnavDiscUnreadbleText = "Navigation CD-ROM<br />is unreadable";
var satnavDiscMissingText = "Navigation CD-ROM is<br />missing";

function satnavShowDisclaimer()
{
	if (satnavDisclaimerAccepted) return;

	currentMenu = "satnav_disclaimer";
	changeLargeScreenTo(currentMenu);
}

// When vehicle is moving, certain menus are disabled

// Override to true by defining MFD_DISABLE_NAVIGATION_MENU_WHILE_DRIVING in Config.h
var mfdDisableNavigationMenuWhileDriving = false;

function satnavVehicleMoving()
{
	var gpsSpeed = parseInt($("#satnav_gps_speed").text() || 0);  // Always reported in km/h
	return gpsSpeed >= 2;
}

function satnavCutoffBottomLines(selector)
{
	// More lines than fit?
	if (selector.height() > selector.parent().height() + 5)
	{
		// Cut off the bottom line(s)
		selector.css("top", "unset");
		selector.css("transform", "unset");
	}
	else
	{
		// Back to original formatting
		var styleObject = selector.prop('style'); 
		styleObject.removeProperty('top');
		styleObject.removeProperty('transform');
	} // if
}

function satnavGotoMainMenu()
{
	// Show popup "Initializing navigator" as long as sat nav is not initialized
	if (! satnavInitialized) return showPopupAndNotifyServer("satnav_initializing_popup", 20000);

	hidePopup("satnav_initializing_popup");

	if (satnavStatus1.match(/NO_DISC/)) return showStatusPopup(satnavDiscMissingText, 8000);

	if (satnavStatus1.match(/DISC_UNREADABLE/) || $("#satnav_disc_recognized").hasClass("ledOff"))
	{
		showStatusPopup(satnavDiscUnreadbleText, 10000);
		return;
	} // if

	// We can get here from various screens. Rewrite the menu stack for direct exit to main menu.
	menuStack = [ menuStack[0] ];
	gotoMenu("satnav_main_menu");

	satnavShowDisclaimer();
}

function satnavGotoListScreen()
{
	// In the menu stack, there is never more than one "list screen" stacked. So check if there is an existing list
	// screen in the current menu stack and, if so, go back to it.
	var idx = menuStack.indexOf("satnav_choose_from_list");

	if (idx >= 0)
	{
		menuStack.length = idx;  // Remove trailing elements
		currentMenu = undefined;
	} // if

	satnavLastEnteredChar = null;

	gotoMenu("satnav_choose_from_list");

	var reqType = handleItemChange.mfdToSatnavRequestType;
	if (reqType === "REQ_ITEMS" || reqType === "REQ_N_ITEMS")
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
}

function satnavGotoListScreenEmpty()
{
	satnavGotoListScreen();
	$("#satnav_choice_list").empty();
	highlightFirstLine("satnav_choice_list");
}

function satnavGotoListScreenServiceList()
{
	handleItemChange.mfdToSatnavRequestType = "REQ_N_ITEMS";
	handleItemChange.mfdToSatnavRequest = "service";
	satnavGotoListScreen();
	highlightFirstLine("satnav_choice_list");
}

function satnavGotoListScreenPersonalAddressList()
{
	handleItemChange.mfdToSatnavRequestType = "REQ_N_ITEMS";
	handleItemChange.mfdToSatnavRequest = "personal_address_list";
	satnavGotoListScreen();
	highlightFirstLine("satnav_choice_list");
}

function satnavGotoListScreenProfessionalAddressList()
{
	handleItemChange.mfdToSatnavRequestType = "REQ_N_ITEMS";
	handleItemChange.mfdToSatnavRequest = "professional_address_list";
	satnavGotoListScreen();
	highlightFirstLine("satnav_choice_list");
}

// Select the first line of available characters and highlight the first letter in the "satnav_enter_characters" screen
function satnavSelectFirstAvailableCharacter()
{
	// None of the buttons is selected
	$("#satnav_enter_characters .button").removeClass("buttonSelected");

	// Select the first letter in the first row
	$("#satnav_to_mfd_show_characters_line_1").addClass("buttonSelected");
	highlightFirstLetter("satnav_to_mfd_show_characters_line_1");

	$("#satnav_to_mfd_show_characters_line_2").removeClass("buttonSelected");
	unhighlightLetter("satnav_to_mfd_show_characters_line_2");
}

function satnavEnterCityCharactersScreen()
{
	highlightFirstLine('satnav_choice_list');
	$('#satnav_to_mfd_show_characters_spinning_disc').hide();
	$("#satnav_enter_characters_validate_button").addClass("buttonDisabled");  // TODO - check
	$("#satnav_enter_characters_validate_button").removeClass("buttonSelected");  // TODO - check
	restoreButtonSizes();
	satnavSelectFirstAvailableCharacter();  // TODO - check
}

function satnavEnterStreetCharactersScreen()
{
	highlightFirstLine("satnav_choice_list");
	$("#satnav_to_mfd_show_characters_spinning_disc").hide();
	$("#satnav_enter_characters_validate_button").addClass("buttonDisabled");
	$("#satnav_enter_characters_validate_button").removeClass("buttonSelected");
	restoreButtonSizes();
	satnavSelectFirstAvailableCharacter();  // TODO - check
}

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
	$("#satnav_enter_characters_validate_button").toggleClass
	(
		"buttonDisabled",
		$("#satnav_current_destination_city").text() === ""
	);

	// Set button text
	$("#satnav_enter_characters_change_or_city_center_button").text(changeText);

	satnavSelectFirstAvailableCharacter();
}

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
	$("#satnav_enter_characters_validate_button").toggleClass
	(
		"buttonDisabled",
		$("#satnav_current_destination_street").text() === ""
	);

	// Set button text
	$("#satnav_enter_characters_change_or_city_center_button").text(cityCenterText);

	satnavSelectFirstAvailableCharacter();

	userHadOpportunityToEnterStreet = false;
}

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
}

// Currently entering destination city?
function satnavEnteringCity()
{
	// The right-most button in the "Enter city/street" screens is "Change" when entering city
	// ("City centre" when entering street)
	return $("#satnav_enter_characters_change_or_city_center_button").text().replace(/\s*/g, "") === changeText;
}

// Currently entering destination city in "character-by-character entry" mode?
function satnavEnteringCityByLetter()
{
	return $("#satnav_current_destination_city").is(":hidden");
}

// Currently entering destination street in "character-by-character entry" mode?
function satnavEnteringStreetByLetter()
{
	return $("#satnav_current_destination_street").is(":hidden");
}

// Currently entering destination in "character-by-character entry" mode?
function satnavEnteringByLetter()
{
	if (satnavEnteringCity()) return satnavEnteringCityByLetter();
	return satnavEnteringStreetByLetter();
}

// Toggle between "character-by-character entry" mode, and "confirm current destination city" mode
function satnavToggleCityEntryMode()
{
	if (satnavEnteringCityByLetter()) satnavConfirmCityMode(); else satnavEnterByLetterMode();
}

function satnavEnterCharactersChangeOrCityCenterButtonPress()
{
	if (satnavEnteringCity())
	{
		// Entering city and chosen "Change" button: toggle to the entry mode
		satnavToggleCityEntryMode();
	}
	else
	{
		// Entering street and chosen "City centre" button: directly go to the "Programmed destination" screen
		gotoMenu("satnav_show_current_destination");
	} // if
}

function satnavEnterCity()
{
	gotoMenu("satnav_enter_city_characters");
	satnavConfirmCityMode();
}

function satnavEnterNewCity()
{
	gotoMenu("satnav_enter_city_characters");
	satnavEnterByLetterMode();
}

function satnavEnterNewCityForService()
{
	$("#satnav_current_destination_city").empty();

	// Clear also the character-by-character entry string
	$("#satnav_entered_string").empty();

	satnavLastEnteredChar = null;

	satnavEnterNewCity();
}

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
}

function satnavGotoEnterStreetOrNumber()
{
	if (satnavEnteringCity())
	{
		if ($("#satnav_entered_string").text() === "")
		{
			// "Validate" clicked in "confirm city" mode
			gotoMenu("satnav_enter_street_characters");
			satnavConfirmStreetMode();
		}
		else
		{
			// "Validate" clicked in "enter by letter" mode, after having entered a few characters
			satnavGotoListScreenEmpty();
		} // if
	}
	else
	{
		if ($("#satnav_current_destination_street").text() === "") satnavGotoListScreen();
		else gotoMenu("satnav_current_destination_house_number");
	} // if
}

// Scrape the entered character from the screen (from the DOM). Used as a last-resort if the
// "mfd_to_satnav_enter_character" packet is missed.
function satnavGrabSelectedCharacter()
{
	if (currentMenu !== "satnav_enter_city_characters" && currentMenu !== "satnav_enter_street_characters") return;

	var selectedChar = $("#satnav_to_mfd_show_characters_line_1 .invertedText").text();
	if (! selectedChar) selectedChar = $("#satnav_to_mfd_show_characters_line_2 .invertedText").text();
	if (selectedChar) satnavLastEnteredChar = selectedChar;
}

function satnavEnterCharacter()
{
	if (satnavLastEnteredChar === null) return;

	// The user is pressing "Val" on the remote control, to enter the chosen character.
	// Or the MFD itself is confirming the character, if there is only one character to choose from.

	$("#satnav_entered_string").append(satnavLastEnteredChar);  // Append the entered character
	$("#satnav_enter_characters_correction_button").removeClass("buttonDisabled");  // Enable the "Correction" button
	$("#satnav_enter_characters_validate_button").addClass("buttonDisabled");  // Disable the "Validate" button
	highlightFirstLine("satnav_choice_list");
}

var showAvailableCharactersTimer = null;

// Handler for the "Correction" button in the "satnav_enter_characters" screen.
// Removes the last entered character.
function satnavRemoveEnteredCharacter(newState)
{
	var currentText = $("#satnav_entered_string").text();
	currentText = currentText.slice(0, -1);
	$("#satnav_entered_string").text(currentText);

	//satnavLastEnteredChar = null;

	// Just in case we miss the "satnav_to_mfd_show_characters" packet
	if (newState == 0 && satnavAvailableCharactersStack.length >= 2)
	{
		let availableCharacters = satnavAvailableCharactersStack[satnavAvailableCharactersStack.length - 2];

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
}

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
		if (satnavRollingBackEntryByCharacter) satnavRemoveEnteredCharacter(newState); else satnavEnterCharacter();
	} // if

	satnavEnterOrDeleteCharacterExpectedState = newState;
}

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
}

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
}

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
}

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
}

var lowestHouseNumberInRange = 0;
var highestHouseNumberInRange = 0;

// Present the entry format for the entering of a house number
function satnavClearEnteredNumber()
{
	var nDigits = highestHouseNumberInRange.toString().length;
	$("#satnav_entered_number").text("_ ".repeat(nDigits));
}

function satnavHouseNumberEntryMode()
{
	$("#satnav_current_destination_house_number").hide();
	satnavClearEnteredNumber();
	$("#satnav_entered_number").show();

	// Navigate back up to the list of numbers and highlight the first
	keyPressed("UP_BUTTON");

	$("#satnav_house_number_show_characters").addClass("buttonSelected");

	// No entered number, so disable the "Change" button
	$("#satnav_enter_house_number_change_button").addClass("buttonDisabled");
}

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

		let translations =
		{
			"set_language_french": "Ce num&eacute;ro n'est<br />pas r&eacute;pertori&eacute;<br />dans cette voie",
			"set_language_german": "Die Nummer ist<br />f&uuml;r diese Stra&szlig;e<br />nicht eingetragen",
			"set_language_spanish": "N&uacute;mero no<br />identificado<br />para esta v&iacute;a",
			"set_language_italian": "Questo numero<br />non &egrave; indicato<br />in questa via",
			"set_language_dutch": "Straatnummer onbekend"
		};
		showStatusPopup(translations[localStorage.mfdLanguage] || "This number is<br />not listed for<br />this street", 7000);

		return;
	} // if

	// Already copy the house number into the "satnav_show_current_destination" screen, in case the
	// "satnav_report" packet is missed
	$("#satnav_current_destination_house_number_shown").html(enteredNumber === "" ? noNumberText : enteredNumber);

	gotoMenu("satnav_show_current_destination");
}

function satnavClearLastDestination()
{
	$('[gid="satnav_last_destination_city"]').empty();
	$('[gid="satnav_last_destination_street_shown"]').empty();
	$("#satnav_last_destination_house_number_shown").empty();
	$("#satnav_reached_destination_house_number").empty();
}

function satnavChangeProgrammedDestination()
{
	menuStack.pop();

	currentMenu = satnavVehicleMoving()
		&& mfdDisableNavigationMenuWhileDriving ?  // TODO - not sure
		'satnav_select_from_memory_menu' : 'satnav_main_menu';
	changeLargeScreenTo(currentMenu);
	selectFirstMenuItem(currentMenu);
}

function satnavEnterShowServiceAddress()
{
	// Empty all fields except 'satnav_to_mfd_list_size'
	$("#satnav_show_service_address .dots:not(#satnav_to_mfd_list_size)").empty();

	$("#satnav_service_address_entry_number").text("1");
}

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
}

function satnavArchiveInDirectoryCreatePersonalEntry()
{
	// Add the new entry to the list of known entries
	var newEntryName = $("#satnav_archive_in_directory_entry").text().replace(/-+$/, "");
	if (newEntryName === "") return;
	satnavPersonalDirectoryEntries.push(newEntryName);

	// Save in local (persistent) store
	localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);

	showPopup("satnav_input_stored_in_personal_dir_popup", 7000);
}

function satnavArchiveInDirectoryCreateProfessionalEntry()
{
	// Add the new entry to the list of known entries
	var newEntryName = $("#satnav_archive_in_directory_entry").text().replace(/-+$/, "");
	if (newEntryName === "") return;
	satnavProfessionalDirectoryEntries.push(newEntryName);

	// Save in local (persistent) store
	localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);

	showPopup("satnav_input_stored_in_professional_dir_popup", 7000);
}

function satnavSetDirectoryAddressScreenMode(mode)
{
	$("#satnav_show_personal_address .button").removeClass("buttonSelected");
	$("#satnav_show_professional_address .button").removeClass("buttonSelected");

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

		$("#satnav_professional_address_validate_buttons").hide();
		$("#satnav_professional_address_manage_buttons").show();

		$("#satnav_manage_personal_address_rename_button").addClass("buttonSelected");
		resizeButton("satnav_manage_personal_address_rename_button");

		$("#satnav_manage_professional_address_rename_button").addClass("buttonSelected");
		resizeButton("satnav_manage_professional_address_rename_button");
	} // if
}

// Select the first letter in the first row
function satnavSelectFirstLineOfDirectoryEntryScreen(screenId)
{
	var line1id = screenId + "_characters_line_1";
	var line2id = screenId + "_characters_line_2";

	$("#" + line1id).addClass("buttonSelected");
	highlightFirstLetter(line1id);

	$("#" + line2id).removeClass("buttonSelected");
	unhighlightLetter(line2id);
}

function satnavEnterArchiveInDirectoryScreen()
{
	satnavLastEnteredChar = null;

	$("#satnav_archive_in_directory_entry").text("----------------");

	// Select the first letter in the first row
	satnavSelectFirstLineOfDirectoryEntryScreen("satnav_archive_in_directory");

	// On entry into this screen, all buttons are disabled
	$("#satnav_archive_in_directory .button").addClass("buttonDisabled");
	$("#satnav_archive_in_directory .button").removeClass("buttonSelected");
}

function satnavEnterRenameDirectoryEntryScreen()
{
	satnavLastEnteredChar = null;

	$("#satnav_rename_entry_in_directory_entry").text("----------------");

	// Select the first letter in the first row
	satnavSelectFirstLineOfDirectoryEntryScreen("satnav_rename_entry_in_directory");

	// On entry into this screen, all buttons are disabled
	$("#satnav_rename_entry_in_directory .button").addClass("buttonDisabled");
	$("#satnav_rename_entry_in_directory .button").removeClass("buttonSelected");
}

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
		let s = $(entrySelector).text();
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
		if (satnavLastEnteredChar !== null)
		{
			// Replace the '-' characters one by one, left to right
			let s = $(entrySelector).text();
			s = s.replace("-", satnavLastEnteredChar);
			$(entrySelector).text(s);
		} // if

		// Select the first letter in the first row
		satnavSelectFirstLineOfDirectoryEntryScreen(screenId);
		$(screenSelector).find(".button").removeClass("buttonSelected");
	} // if

	// Enable or disable buttons, depending on if there is anything in the entry field
	$(screenSelector).find(".button").toggleClass("buttonDisabled", $(entrySelector).text()[0] === "-");

	// Nothing in the entry field?
	if ($(entrySelector).text()[0] === "-")
	{
		$(".satNavEntryExistsTag").hide();
		return;
	} // if

	// Enable or disable "Personal dir", "Professional dir" and Validate" button if entered name already exists
	var enteredEntryName = $(entrySelector).text().replace(/-+$/, "");
	var personalEntryExists = satnavPersonalDirectoryEntries.indexOf(enteredEntryName) >= 0;
	var professionalEntryExists = satnavProfessionalDirectoryEntries.indexOf(enteredEntryName) >= 0;
	var entryExists = personalEntryExists || professionalEntryExists;

	$("#satnav_rename_entry_in_directory_validate_button").toggleClass("buttonDisabled", entryExists);
	$("#satnav_archive_in_directory_personal_button").toggleClass("buttonDisabled", entryExists);
	$("#satnav_archive_in_directory_professional_button").toggleClass("buttonDisabled", entryExists);
	$(".satNavEntryExistsTag").toggle(entryExists);
}

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
}

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

	// Go all the way back to the "Directory management" menu
	upMenu();
	upMenu();
}

// Handles pressing "UP" on the "Correction" button in the sat nav directory entry screens
function satnavDirectoryEntryMoveUpFromCorrectionButton(screenId)
{
	var line2id = screenId + "_characters_line_2";

	if (highlightIndexes[line2id] === undefined)
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
}

function satnavDeleteDirectories()
{
	hidePopup('satnav_delete_directory_data_popup');

	satnavPersonalDirectoryEntries = [];
	satnavProfessionalDirectoryEntries = [];

	// Save in local (persistent) store
	localStorage.satnavPersonalDirectoryEntries = JSON.stringify(satnavPersonalDirectoryEntries);
	localStorage.satnavProfessionalDirectoryEntries = JSON.stringify(satnavProfessionalDirectoryEntries);
}

function satnavSwitchToGuidanceScreen()
{
	hidePopup("satnav_computing_route_popup");
	menuStack = [];
	currentMenu = "satnav_guidance";
	changeLargeScreenTo(currentMenu);
}

// Show the "Destination is not accessible by road" popup, if applicable.
// Returns 'false' if it is not (yet) the right moment to check if the popup must be shown.
function showDestinationNotAccessiblePopupIfApplicable()
{
	if (! satnavDestinationNotAccessible) return false;

	// Don't show this popup while still in the guidance preference popup or menu
	if ($("#satnav_guidance_preference_popup").is(":visible")) return false;
	if ($("#satnav_guidance_preference_menu").is(":visible")) return false;

	// Show this popup only if the current location is known (to emulate behaviour of MFD).
	// This seems to be the criterion used by the original MFD.
	if (satnavCurrentStreet === "") return false;
	if ($("#satnav_not_on_map_icon").is(':visible')) return false;

	// Show popup only once at start of guidance or after recalculation
	if (satnavDestinationNotAccessibleByRoadPopupShown) return true;

	if (! satnavDestinationReachable && satnavOnMap)
	{
		hidePopup();

		let translations =
		{
			"set_language_french": "La destination n'est<br />pas accessible par<br />voie routi&egrave;re",
			"set_language_german": "Das Ziel ist<br />per Stra%szlig;e nicht<br />zu erreichen",
			"set_language_spanish": "Destino inaccesible<br />por carratera",
			"set_language_italian": "La destinazione non<br />&egrave; accessibile<br />mediante strada",
			"set_language_dutch": "De bestemming is niet<br />via de weg bereikbaar"
		};
		showStatusPopup(translations[localStorage.mfdLanguage] || "Destination is not<br />accessible by road", 8000);
	} // if

	satnavDestinationNotAccessibleByRoadPopupShown = true;

	return true;
}

function satnavGuidanceSetPreference(value)
{
	if (value === undefined || value === "---") return;

	// Copy the correct text into the guidance preference popup
	var menuId = "#satnav_guidance_preference_menu";
	var preferenceText =
		value === "FASTEST_ROUTE" ? $(menuId + " .tickBoxLabel:eq(0)").text() :
		value === "SHORTEST_DISTANCE" ? $(menuId + " .tickBoxLabel:eq(1)").text() :
		value === "AVOID_HIGHWAY" ? $(menuId + " .tickBoxLabel:eq(2)").text() :
		value === "COMPROMISE_FAST_SHORT" ? $(menuId + " .tickBoxLabel:eq(3)").text() :
		"??";
	$("#satnav_guidance_current_preference_text").text(preferenceText.toLowerCase());

	// Also set the correct tick box in the guidance preference menu
	var preferenceTickBoxId =
		value === "FASTEST_ROUTE" ? "fastest" :
		value === "SHORTEST_DISTANCE" ? "shortest" :
		value === "AVOID_HIGHWAY" ? "avoid_highway" :
		value === "COMPROMISE_FAST_SHORT" ? "compromise" :
		undefined;
	setTick("satnav_guidance_preference_" + preferenceTickBoxId);
}

function satnavGuidancePreferenceSelectTickedButton()
{
	satnavGuidanceSetPreference(localStorage.satnavGuidancePreference);

	// Also select the ticked button

	var foundTick = false;

	$("#satnav_guidance_preference_menu .tickBox").each
	(
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
}

function satnavGuidancePreferencePopupYesButton()
{
	hidePopup('satnav_guidance_preference_popup');

	if (showDestinationNotAccessiblePopupIfApplicable()) return;

	if (! $('#satnav_guidance').is(':visible')) satnavCalculatingRoute();
}

function satnavGuidancePreferencePopupNoButton()
{
	hidePopup('satnav_guidance_preference_popup');
	hidePopup('satnav_computing_route_popup');
	changeLargeScreenTo('satnav_guidance_preference_menu');
	satnavGuidancePreferenceSelectTickedButton();
}

function satnavGuidancePreferenceEscape()
{
	if (menuStack[0])
	{
		changeLargeScreenTo(menuStack[0]);
		menuStack = [];
		currentMenu = undefined;
	}
	else
	{
		selectDefaultScreen();
	} // if
}

function satnavGuidancePreferenceValidate()
{
	if (currentMenu === "satnav_guidance")
	{
		// Return to the guidance screen
		satnavSwitchToGuidanceScreen();
		if (satnavComputingRoute) satnavCalculatingRoute(); else showDestinationNotAccessiblePopupIfApplicable();
	}
	else
	{
		satnavSwitchToGuidanceScreen();
	} // if
}

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
}

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
}

// Format a string like "45 km", "15 mi", "7000 m", "880 yd" or "60 m". Return an array [distance, unit]
function satnavFormatDistance(distanceStr)
{
	// We want compatibility with IE11 so cannot assign result array directly to variables
	var parts = distanceStr.split(" ");
	var distance = parts[0];
	var unit = parts[1];

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
}

function satnavSetAudioLed(playingAudio)
{
	$("#satnav_audio").toggleClass("ledOn", playingAudio);
	$("#satnav_audio").toggleClass("ledOff", ! playingAudio);

	clearTimeout(satnavSetAudioLed.showSatnavAudioLed);
	if (playingAudio)
	{
		// Don't let traffic info or any other popup eclipse the guidance screen at this moment
		hidePopup("notification_popup");

		if (satnavMode === "IN_GUIDANCE_MODE" && ! inMenu())
		{
			temporarilyChangeLargeScreenTo("satnav_guidance", 5000);
		} // if

		// Set timeout on LED, in case the "AUDIO OFF" packet is missed
		satnavSetAudioLed.showSatnavAudioLed = setTimeout
		(
			function ()
			{
				$("#satnav_audio").removeClass("ledOn");
				$("#satnav_audio").addClass("ledOff");
			},
			5000
		);
	} // if
}

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

		upMenu();
		upMenu();
	}
	else
	{
		// In main menu

		// When the "Validate" button is pressed, all menus are exited up to top level.
		upMenu();
		upMenu();
		upMenu();
		upMenu();
	} // if
}

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
	upMenu();
	if (comingFromMenu === "satnav_guidance_tools_menu") upMenu();
}

function satnavStopGuidance()
{
	// Try to reproduce a bug in the original MFD: if sat nav guidance is stopped while not on map,
	// it will ask for guidance continuation later. So only clear if "on map".
	if (satnavOnMap) localStorage.askForGuidanceContinuation = "NO";
	selectDefaultScreen();
}

function satnavStopOrResumeGuidance()
{
	if (satnavMode === "IN_GUIDANCE_MODE") satnavStopGuidance(); else satnavSwitchToGuidanceScreen();
}

function satnavPoweringOff(satnavMode)
{
	// If in guidance mode while turning off contact key, remember to show popup
	// "Continue guidance to destination?" the next time the contact key is turned "ON"
	if (satnavMode === "IN_GUIDANCE_MODE")
	{
		localStorage.askForGuidanceContinuation = "YES";
		webSocket.send("ask_for_guidance_continuation:YES");
	} // if

	satnavInitialized = false;
	// $("#satnav_disc_recognized").addClass("ledOff");
	handleItemChange.nSatNavDiscUnreadable = 1;
	satnavDisclaimerAccepted = false;
	satnavDestinationNotAccessibleByRoadPopupShown = false;
}

function satnavNoAudioIcon()
{
	clearTimeout(handleItemChange.audioSourceTimer);
	handleItemChange.audioSourceTimer = undefined;
	if (handleItemChange.headUnitLastSwitchedTo === "NAVIGATION") 
	{
		$("#satnav_no_audio_icon").toggle($("#volume").text() === "0");
	} // if
}

function satnavRetrievingInstruction()
{
	// TODO - check translations from English
	let translations =
	{
		"set_language_french": "Chercher instruction",
		"set_language_german": "Anweisung suchen",
		"set_language_spanish": "Buscar instrucci&oacute;n",
		"set_language_italian": "Cercare istruzione",
		"set_language_dutch": "Opzoeken instructie"
	};
	let content = translations[localStorage.mfdLanguage] || "Retrieving next instruction";
	$("#satnav_guidance_next_street").html(content);
	satnavCutoffBottomLines($("#satnav_guidance_next_street"));
}

function showPowerSavePopup()
{
	var translations =
	{
		"set_language_french": "Passer en mode<br />d'&eacute;conomie d'&eacute;nergie",  // TODO - check
		"set_language_german": "Wechsel in<br />Energiesparmodus",
		"set_language_spanish": "Cambiar al modo<br />de ahorro de energ&iacute;a",  // TODO - check
		"set_language_italian": "Passaggio alla modalit&agrave;<br />di risparmio energetico",  // TODO - check
		"set_language_dutch": "Omschakeling naar<br />energiebesparingsmodus"
	};
	showNotificationPopup(translations[localStorage.mfdLanguage] || "Changing to<br />power-save mode", 15000);
}

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
}

// -----
// Handling of doors and boot status

var isDoorOpen = {};  // Maps door ID to either true (open) or false (closed)
var isBootOpen = false;

var doorOpenText = "Door open";
var doorsOpenText = "Doors open";
var bootOpenText = "Boot open";
var bootAndDoorOpenText = "Boot and door open";
var bootAndDoorsOpenText = "Boot and doors open";

// -----
// Handling of changed items

// Vehicle data
var economyMode;
var engineCoolantTemperature;  // In degrees Celsius
var vehicleSpeed;
var icyConditions = false;
var wasRiskOfIceWarningShown = false;
var riskOfIceText = "Risk of ice!";

function handleItemChange(item, value)
{
	switch(item)
	{
		case "mfd_language":
		{
			let map =
			{
				"FRENCH": "set_language_french",
				"ENGLISH": "set_language_english",
				"GERMAN": "set_language_german",
				"SPANISH": "set_language_spanish",
				"ITALIAN": "set_language_italian",
				"DUTCH": "set_language_dutch"
			};

			let newLanguage = map[value] || "";
			if (! newLanguage) break;
			setLanguage(newLanguage);
			localStorage.mfdLanguage = newLanguage;
		} // case
		break;

		case "mfd_temperature_unit":
		{
			let map =
			{
				"FAHRENHEIT": "set_units_deg_fahrenheit",
				"CELSIUS": "set_units_deg_celsius"
			};

			let newUnit = map[value] || "";
			if (! newUnit) break;
			if (newUnit === localStorage.mfdTemperatureUnit) break;
			setUnits(localStorage.mfdDistanceUnit, newUnit, localStorage.mfdTimeUnit);
		} // case
		break;

		case "mfd_distance_unit":
		{
			let map =
			{
				"MILES_YARDS": "set_units_mph",
				"KILOMETRES_METRES": "set_units_km_h"
			};

			let newUnit = map[value] || "";
			if (! newUnit) break;
			if (newUnit === localStorage.mfdDistanceUnit) break;
			setUnits(newUnit, localStorage.mfdTemperatureUnit, localStorage.mfdTimeUnit);
			invalidateAllDistanceFields();
		} // case
		break;

		case "mfd_time_unit":
		{
			let map =
			{
				"24_H": "set_units_24h",
				"12_H": "set_units_12h"
			};

			let newUnit = map[value] || "";
			if (! newUnit) break;
			if (newUnit === localStorage.mfdTimeUnit) break;
			setUnits(localStorage.mfdDistanceUnit, localStorage.mfdTemperatureUnit, newUnit);
		} // case
		break;

		case "volume_update":
		{
			if (value !== "YES") break;

			// Just changed audio source? Then suppress the audio settings popup (which would otherwise pop up every
			// time the audio source changes).
			if (handleItemChange.hideHeadUnitPopupsTimer !== null) break;

			// Keep the popup for at least another 4 seconds, if already showing
			hideAudioSettingsPopupAfter(4000);

			// Bail out if no audio is playing
			let audioSource = $("#audio_source").text();
			if (audioSource !== "TUNER"
				&& audioSource !== "TAPE"
				&& audioSource !== "CD"
				&& audioSource !== "CD_CHANGER"
				)
			{
				break;
			} // if

			showAudioVolumePopup();
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

			let audioSource = $("#audio_source").text();

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
					// Original MFD says: "No Mag" ("Mag" means: "magazine")
					showStatusPopup("No CD cartridge present", 3000);
					break;
				} // if

				// Cartridge is present

				// Not busy loading, and cartridge contains no discs?
				if (
					 $("#cd_changer_status_loading").css("display") !== "block"
					 && (cdChangerCurrentDisc === null || ! cdChangerCurrentDisc.match(/^[1-6]$/))
				   )
				{
					// Original MFD says: "No CD"
					showStatusPopup("No CD present", 3000);
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
				showAudioPopup();

				let selector = "#cd_changer_disc_" + value + "_present";
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

			showTunerPresetsPopup();
		} // case
		break;

		case "tape_side":
		{
			// Has anything changed?
			if (value === handleItemChange.tapeSide) break;
			handleItemChange.tapeSide = value;

			hideAudioSettingsPopup();

			showAudioPopup();
		} // case
		break;

		case "tape_status":
		{
			// Has anything changed?
			if (value === handleItemChange.tapeStatus) break;
			handleItemChange.tapeStatus = value;

			hideAudioSettingsPopup();
			showAudioPopup();
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

			showAudioPopup();
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
			//if (value === handleItemChange.currentTunerPresetMemoryValue) break;

			// Un-highlight any previous entry in the "tuner_presets_popup"
			$('div[id^=presets_memory_][id$=_select]').hide();

			// Make sure the audio settings popup is hidden
			hideAudioSettingsPopup();

			// Changed to a non-preset frequency? Or just changed audio source? Then suppress the tuner presets popup.
			if (value === "-" || handleItemChange.hideHeadUnitPopupsTimer !== null)
			{
				hideTunerPresetsPopup();
			}
			else
			{
				// Highlight the corresponding entry in the "tuner_presets_popup", in case that is currently visible
				// due to a recent preset button press on the head unit
				let highlightId = "presets_memory_" + value + "_select";
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

			let showRdsText = value !== "" && tunerSearchMode !== "MANUAL_TUNING";

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

			showAudioPopup();

			let rdsText = $("#rds_text").text();
			let showRdsText = $("#fm_band").hasClass("ledOn") && rdsText !== "" && value !== "MANUAL_TUNING";

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
			if (handleItemChange.infoTraffic === undefined) handleItemChange.infoTraffic = "NO";

			if (inMenu() || $("#audio_settings_popup").is(":visible")) break;

			// Has anything changed?
			if (value === handleItemChange.infoTraffic) break;
			handleItemChange.infoTraffic = value;

			if (value === "YES")
			{
				let translations =
				{
					"set_language_french": "Info Traffic!",
					"set_language_german": "Verkehrsinformationen",
					"set_language_spanish": "Informaci&oacute;n de tr&aacute;fico",
					"set_language_italian": "Informazioni sul traffico",
					"set_language_dutch": "Verkeersinformatie"
				};
				handleItemChange.infoTrafficPopupTimer =
					showNotificationPopup(translations[localStorage.mfdLanguage] || "Traffic information",
						$("#satnav_guidance").is(":visible") ? 5000 : 10000);
			}
			else
			{
				hideNotificationPopup(handleItemChange.infoTrafficPopupTimer);
			} // if
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
				showAudioPopup();
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

		case "cd_total_time":
		{
			$("#cd_total_time_tag").toggle(value !== "--:--");
			$("#cd_total_time").toggle(value !== "--:--");
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
				showAudioPopup();
			} // if
		} // case
		break;

		case "cd_changer_current_disc":
		{
			// Has anything changed?
			if (value === cdChangerCurrentDisc) break;

			showAudioPopup();

			if (cdChangerCurrentDisc !== null && cdChangerCurrentDisc.match(/^[1-6]$/))
			{
				let selector = "#cd_changer_disc_" + cdChangerCurrentDisc + "_present";
				$(selector).removeClass("ledActive");
				$(selector).css({marginTop: '+=25px'});
			} // if

			// Only if valid value (can be "--" if cartridge is not present)
			if (value.match(/^[1-6]$/))
			{
				// The LED of the current disc is shown a bit larger than the others
				let selector = "#cd_changer_disc_" + value + "_present";
				$(selector).addClass("ledActive");
				$(selector).css({marginTop: '-=25px'});
			} // if

			cdChangerCurrentDisc = value;
		} // case
		break;

		case "audio_source":
		{
			if (handleItemChange.currentAudioSource !== value) hideAudioSettingsPopup();
			handleItemChange.currentAudioSource = value;

			// Sat nav audio (e.g. guidance instruction or welcome message) needs no further action
			if (value === "NAVIGATION") break;

			// Has main audio source changed?
			if (value === handleItemChange.mainAudioSource) break;
			handleItemChange.mainAudioSource = value;

			// When in a menu, a change in the audio source does not imply any further action
			if (inMenu()) break;

			// Suppress timer still running?
			if (suppressScreenChangeToAudio !== null) break;

			// Hide the audio settings and tuner presets popup (if visible)
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

			// If head unit is turned off, only change screen if currently showing head unit
			if (value === "NONE" && ! $("#audio").is(":visible"))
			{
				cancelChangeBackScreenTimer();
				break;
			} // if

			selectDefaultScreen(value);
		} // case
		break;

		case "head_unit_update_switch_to":
		{
			handleItemChange.headUnitLastSwitchedTo = value;
			if ($("#audio_source").text() === "NAVIGATION")
			{
				clearTimeout(handleItemChange.audioSourceTimer);
				handleItemChange.audioSourceTimer = setTimeout(satnavNoAudioIcon, 500);
			} // if
		} // case
		break;

		case "volume":
		{
			if (handleItemChange.headUnitLastSwitchedTo === "NAVIGATION")
			{
				// Wait for the audio source to stabilize
				clearTimeout(handleItemChange.audioSourceTimer);
				handleItemChange.audioSourceTimer = setTimeout(satnavNoAudioIcon, 500);
			} // if
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

			if (handleItemChange.mainAudioSource === "CD_CHANGER")
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
			engineRpm = parseInt(value);
			if (isNaN(engineRpm))
			{
				engineRpm = -1;
				if (currentLargeScreenId === "instruments") selectDefaultScreen();
				cancelChangeBackScreenTimer();
				break;
			} // if

			// If more than 3500 rpm or less than 500 rpm (but > 0), add glow effect

			// Threshold of 3500 rpm is lowered with engine coolant temperature, but always at least 1700 rpm
			let thresholdRpm = 3500;
			if (engineCoolantTemperature < 80) thresholdRpm = 3500 - (80 - engineCoolantTemperature) * 30;
			if (thresholdRpm < 1700) thresholdRpm = 1700;

			$('[gid="engine_rpm"]').toggleClass("glow", (engineRpm < 500 && engineRpm > 0) || engineRpm > thresholdRpm);

			if (contactKeyPosition === "START" && engineRpm > 150) changeToInstrumentsScreen();

			// Deduce the chosen gear
			if (vehicleSpeed === undefined || vehicleSpeed < 2 || engineRpm < 915)
			{
				$("#chosen_gear").text("-");
				break;
			} // if
			let speedKmh = vehicleSpeed;
			if (localStorage.mfdDistanceUnit === "set_units_mph") speedKmh = vehicleSpeed * 1.609;
			let n = engineRpm / speedKmh;

			// Note: ratio numbers are for 5-speed manual, 2.0 HDI. Ratio numbers may vary per vehicle configuration.
			if (n >= 110 && n <= 170) $("#chosen_gear").text("1");
			else if (n >= 57 && n <= 75) $("#chosen_gear").text("2");
			else if (n >= 37 && n <= 42) $("#chosen_gear").text("3");
			else if (n >= 26 && n <= 29) $("#chosen_gear").text("4");
			else if (n >= 20 && n <= 22) $("#chosen_gear").text("5");
			else $("#chosen_gear").text("-");
		} // case
		break;

		case "vehicle_speed":
		{
			if (value === "--") break;

			vehicleSpeed = parseInt(value);

			let isDriving = vehicleSpeed >= 5;
			let isDrivingFast = vehicleSpeed >= 140;

			if (localStorage.mfdDistanceUnit === "set_units_mph")
			{
				isDriving = vehicleSpeed >= 3;
				isDrivingFast = vehicleSpeed >= 90;
			} // if

			// If driving fast, add glow effect
			$('[gid="vehicle_speed"]').toggleClass("glow", isDrivingFast);

			if (icyConditions && ! wasRiskOfIceWarningShown && isDriving)
			{
				showNotificationPopup(riskOfIceText, 10000);
				wasRiskOfIceWarningShown = true;
			} // if
		} // case
		break;

		case "fuel_level":
		{
			let level = parseFloat(value);

			// 5.5 litres or 1.5 gallons left?
			let lowFuelCondition = level <= (localStorage.mfdDistanceUnit === "set_units_mph" ? 1.5 : 5.5);

			// 10 percent fuel left?
			if ($('[gid="fuel_level_unit"]').first().text() === "%") lowFuelCondition === level <= 10;

			$('[gid="fuel_level"]').toggleClass("glow", lowFuelCondition);
		} // case
		break;

		case "coolant_temp":
		{
			let temp = parseFloat(value);

			if (localStorage.mfdTemperatureUnit === "set_units_deg_fahrenheit")
			{
				engineCoolantTemperature = (temp - 32) * 5 / 9;  // convert deg F to deg C

				$("#coolant_temp").toggleClass("glow", temp > 240);  // If high, add glow effect
				$("#coolant_temp").toggleClass("glowIce", temp < 160);  // If low, add "ice glow" effect
			}
			else
			{
				engineCoolantTemperature = temp;

				$("#coolant_temp").toggleClass("glow", temp > 115);  // If high, add glow effect
				$("#coolant_temp").toggleClass("glowIce", temp < 70); // If low, add "ice glow" effect
			} // if
		} // case
		break;

		case "distance_to_empty":
		{
			// If less than 90 kms or 60 miles, add glow effect
			let dist = parseFloat(value);
			let unit = localStorage.mfdDistanceUnit;
			$('[gid="distance_to_empty"]').toggleClass("glow", dist < (unit === "set_units_mph" ? 60 : 90));
		} // case
		break;

		case "exterior_temp":
		{
			let temp = parseFloat(value);

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

			// If icy conditions, add "ice glow" effect
			$('[gid="exterior_temp_shown"]').toggleClass("glowIce", icyConditions);

			if (icyConditions)
			{
				if (! wasRiskOfIceWarningShown && vehicleSpeed >= 5)
				{
					showNotificationPopup(riskOfIceText, 10000);
					wasRiskOfIceWarningShown = true;
				} // if
			}
			else
			{
				wasRiskOfIceWarningShown = false;
			} // if
		} // case
		break;

		case "exterior_temp_loc":
		{
			$("#splash_text").hide();

			$('[gid="exterior_temp_shown"]').html(
				localStorage.mfdTemperatureUnit === "set_units_deg_fahrenheit" ?
					value + " &deg;F" : value + " &deg;C"
				);
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

		case "lights":
		{
			headlightStatus = value;
			setDimLevel(value);

			let indicatorLeft = value.match(/INDICATOR_LEFT/) !== null;
			$("#left_indicator").toggleClass("ledOnGreen", indicatorLeft).toggleClass("ledOff", ! indicatorLeft);

			let indicatorRight = value.match(/INDICATOR_RIGHT/) !== null;
			$("#right_indicator").toggleClass("ledOnGreen", indicatorRight).toggleClass("ledOff", ! indicatorRight);

			// Set timeout on indicator LEDs, in case the "OFF" packet is missed
			clearTimeout(handleItemChange.indicatorOffTimer);
			handleItemChange.indicatorOffTimer = setTimeout
			(
				function () { $("#left_indicator,#right_indicator").removeClass("ledOnGreen").addClass("ledOff"); },
				1000
			);

			let dippedBeam = value.match(/DIPPED_BEAM/) !== null;
			$("#dipped_beam").toggleClass("ledOnGreen", dippedBeam).toggleClass("ledOff", ! dippedBeam);

			let highBeam = value.match(/HIGH_BEAM/) !== null;
			$("#high_beam").toggleClass("ledOnBlue", highBeam).toggleClass("ledOff", ! highBeam);
		} // case
		break;

		case "door_open":
		{
			// Note: If the system is in power save mode, "door_open" always reports "NO", even if a door is open.
			// That situation is handled in the next case clause, below.

			$("#door_open_led").toggleClass("glow", value === "YES");  // If on, add glow effect

			// Always hide the "door open" popup if in the "pre_flight" screen or while browsing the menus.
			if (value !== "YES" || currentLargeScreenId === "pre_flight" || inMenu()) hidePopup("door_open_popup");
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

			let isOpen = value === "OPEN";

			let change = false;
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

			if (! change) break;

			let nDoorsOpen = 0;
			for (let id in isDoorOpen) { if (isDoorOpen[id]) nDoorsOpen++; }

			// If on, add glow effect to "door_open" LED in the "pre_flight" screen
			$("#door_open_led").toggleClass("glow", isBootOpen || nDoorsOpen > 0);

			// All closed, or in the "pre_flight" or a menu screen?
			if ((! isBootOpen && nDoorsOpen == 0) || currentLargeScreenId === "pre_flight" || inMenu())
			{
				hidePopup("door_open_popup");
				break;
			} // if

			// Compose gold-plated popup text
			let popupText =
				isBootOpen && nDoorsOpen > 1 ? bootAndDoorsOpenText :
				isBootOpen && nDoorsOpen == 1 ? bootAndDoorOpenText :
				isBootOpen && nDoorsOpen == 0 ? bootOpenText :
				nDoorsOpen > 1 ? doorsOpenText :
				doorOpenText;

			hideTunerPresetsPopup();

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

			// Engine just stopped, and currently in "instruments" screen?
			else if (currentLargeScreenId === "instruments") selectDefaultScreen();
		} // case
		break;

		case "contact_key_position":
		{
			if (localStorage.askForGuidanceContinuation === "YES"
				&& !inMenu()
				&& value === "ON"
				&& satnavInitialized
				&& economyMode !== "ON"
				)
			{
				// Show popup "Continue guidance to destination? [Yes]/No"
				showPopup("satnav_continue_guidance_popup", 15000);

				localStorage.askForGuidanceContinuation = "NO";
			} // if

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
					if (engineRunning !== "YES" && engineRpm <= 0)
					{
						changeLargeScreenTo("pre_flight");

						// Hide all popups except "satnav_continue_guidance_popup"
						hideAllPopups("satnav_continue_guidance_popup");
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
					let savedSatnavMode = satnavMode;
					handleItemChange.contactKeyOffTimer = setTimeout
					(
						function ()
						{
							handleItemChange.contactKeyOffTimer = null;
							satnavPoweringOff(savedSatnavMode);
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
			if (handleItemChange.mfdNotificationMsg === undefined) handleItemChange.mfdNotificationMsg = "";

			// Has anything changed?
			if (value === handleItemChange.mfdNotificationMsg) break;
			handleItemChange.mfdNotificationMsg = value;

			if (value === "")
			{
				hideNotificationPopup(handleItemChange.notificationMessagePopupTimer);
				break;
			} // if

			let isWarning = value.slice(-1) === "!";
			let message = isWarning ? value.slice(0, -1) : value;
			handleItemChange.notificationMessagePopupTimer = showNotificationPopup(message, 8000, isWarning);
		} // case
		break;

		case "doors_locked":
		{
			// Has anything changed?
			if (value === handleItemChange.doorsLocked) break;
			handleItemChange.doorsLocked = value;

			let translations =
			{
				"set_language_french": "Portes verrouill&eacutees;",
				"set_language_german": "T&uuml;ren verschlossen",
				"set_language_spanish": "Puertas cerradas",
				"set_language_italian": "Porte chiuse",
				"set_language_dutch": "Deuren op slot"
			};
			let content = translations[localStorage.mfdLanguage] || "Doors locked";

			if (value === "YES") showNotificationPopup(content, 4000);
		} // case
		break;

		case "satnav_destination_not_accessible":
		{
			satnavDestinationNotAccessible = value === "YES";

			if (satnavDestinationNotAccessible) showDestinationNotAccessiblePopupIfApplicable();
		} // case
		break;

		case "satnav_arrived_at_destination":
		{
			if (value !== "YES") break;

			showPopup("satnav_reached_destination_popup", 10000);
		} // case
		break;

		case "satnav_new_guidance_instruction":
		{
			if (satnavMode !== "IN_GUIDANCE_MODE") break;
			if (! satnavDisplayCanBeDimmed) break;

			if (value === "YES") temporarilyChangeLargeScreenTo("satnav_guidance", 15000);
			else changeBackLargeScreenAfter(2000);
		} // case
		break;

		case "satnav_guidance_display_can_be_dimmed":
		{
			if (satnavMode !== "IN_GUIDANCE_MODE") break;

			satnavDisplayCanBeDimmed = value === "YES";

			if (value === "NO" && currentLargeScreenId !== "satnav_vocal_synthesis_level")
			{
				temporarilyChangeLargeScreenTo("satnav_guidance", 300000);
			}
			else if (value === "YES")
			{
				changeBackLargeScreenAfter(2000);
			} // if
		} // case
		break;

		case "satnav_calculating_route":
		{
			if (value === "YES") satnavRetrievingInstruction();
		} // case
		break;

		case "satnav_audio_start":
		{
			if (value === "YES") satnavSetAudioLed(true);
		} // case
		break;

		case "satnav_audio_end":
		{
			if (value === "YES") satnavSetAudioLed(false);
		} // case
		break;

		case "satnav_status_1":
		{
			satnavStatus1 = value;

			if (economyMode === "ON") break;

			if (satnavStatus1.match(/NO_DISC/))
			{
				hidePopup("satnav_initializing_popup");
				showStatusPopup(satnavDiscMissingText, 5000);
			} // if
			else if (satnavStatus1.match(/DISC_UNREADABLE/))
			{
				hidePopup("satnav_initializing_popup");

				// TODO - show popup not directly, but only after a few times this status
				showStatusPopup(satnavDiscUnreadbleText, 5000);
			} // if
		} // case
		break;

		case "satnav_status_2":
		{
			if (value === "") break;  // Only if valid

			// Has anything changed?
			if (value === satnavMode) break;
			let previousSatnavMode = satnavMode;
			satnavMode = value;

			// In case the ESP boots after the moment the sat nav has reported its "satnav_system_id"
			// (i.e., reports it is initialized)
			satnavInitialized = value !== "INITIALIZING";
			if (satnavInitialized) hidePopup("satnav_initializing_popup");

			let text = value === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText;
			$("#satnav_navigation_options_menu .button:eq(3)").html(text);
			$("#satnav_guidance_tools_menu .button:eq(3)").html(text);

			// Just entered guidance mode?
			if (value === "IN_GUIDANCE_MODE")
			{
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

			if (value === "IDLE") satnavRouteComputed = false;
		} // case
		break;

		case "satnav_status_3":
		{
			satnavStatus3 = value;

			if (value === "STOPPING_NAVIGATION") satnavDestinationNotAccessibleByRoadPopupShown = false;
			else if (value === "POWERING_OFF") satnavPoweringOff(satnavMode);
			else if (value === "COMPUTING_ROUTE") satnavComputingRoute = true;
			else if (value === "VOCAL_SYNTHESIS_LEVEL_SETTING_VIA_HEAD_UNIT") showAudioVolumePopup();
		} // case
		break;

		case "satnav_gps_speed":
		{
			if (! mfdDisableNavigationMenuWhileDriving) break;

			let driving = satnavVehicleMoving();

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
			let newValue = value === "YES" && satnavMode !== "IDLE";

			// Has anything changed?
			if (newValue === satnavRouteComputed) break;
			satnavRouteComputed = newValue;

			if (satnavMode !== "IN_GUIDANCE_MODE") break;
			if (! $("#satnav_guidance").is(":visible")) break;

			satnavRetrievingInstruction();
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

			if (value === "NO" && satnavEquipmentPresent) handleItemChange.nSatNavDiscUnreadable++;
			else handleItemChange.nSatNavDiscUnreadable = 0;

			// At the 6-th and 14-th time of consequently reporting "NO", shortly show a status popup
			if (handleItemChange.nSatNavDiscUnreadable == 6 || handleItemChange.nSatNavDiscUnreadable == 14)
			{
				hidePopup("satnav_initializing_popup");
				showStatusPopup(satnavDiscUnreadbleText, 4000);
			} // if
		} // case
		break;

		case "satnav_equipment_present":
		{
			satnavEquipmentPresent = value === "YES";

			if (! satnavEquipmentPresent) handleItemChange.nSatNavDiscUnreadable = 0;

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
			$("#satnav_current_destination_street_shown").text(value === "" ? cityCenterText : value);

			if (! $("#satnav_enter_street_characters").is(":visible")) break;

			$("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "");
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
			$("#satnav_last_destination_house_number_shown").html(value === "" ? noNumberText : value);
			$("#satnav_reached_destination_house_number").html(value);  // If empty, don't say "No number"
		} // case
		break;

		case "satnav_curr_street":
		{
			satnavCurrentStreet = value;

			if (satnavMode === "IN_GUIDANCE_MODE")
			{
				// Last received value empty ("")? Then copy current street into next street (if not empty)
				if (satnavNextStreet === "" && value !== "")
				{
					$("#satnav_guidance_next_street").html(value);
					satnavCutoffBottomLines($("#satnav_guidance_next_street"));
				} // if

				// In the guidance screen, show "Street (City)", otherwise "Street not listed"
				let selector = $("#satnav_guidance_curr_street");
				selector.html(value.match(/\(.*\)/) ? value : streetNotListedText);
				satnavCutoffBottomLines(selector);

				showDestinationNotAccessiblePopupIfApplicable();
			} // if

			// In the current location screen, show "City", or "Street (City)", otherwise "Street not listed"
			// Note: when driving off the map, the current location screen will start showing "Not digitized area";
			// this is handled by 'case "satnav_on_map"' above.
			$('[gid="satnav_curr_street_shown"]').html(value !== "" ? value : streetNotListedText);

			satnavCutoffBottomLines($('#satnav_curr_street_small [gid="satnav_curr_street_shown"]'));

			// Only if the clock is currently showing, i.e. don't move away from the Tuner or CD player screen
			if ($("#clock").is(":visible") && satnavMode !== "IN_GUIDANCE_MODE")
			{
				changeLargeScreenTo("satnav_current_location");
			} // if

			satnavCutoffBottomLines($('#satnav_current_location [gid="satnav_curr_street_shown"]'));
		} // case
		break;

		case "satnav_next_street":
		{
			satnavNextStreet = value;  // Store what was received

			let selector = $("#satnav_guidance_next_street");

			// No data means: stay on current street.
			// But if current street is empty (""), then just keep the current text.
			if (value !== "") selector.html(value);
			else if (satnavCurrentStreet !== "") selector.html(satnavCurrentStreet);

			satnavCutoffBottomLines(selector);
		} // case
		break;

		case "satnav_personal_address_entry":
		{
			satnavDirectoryEntry = value;

			// The "Rename" resp. "Validate" buttons are disabled if the entry is not on the current disc
			// (value ends with "&#x24E7;" (X))
			let entryOnDisc = value.match(/&#x24E7;$/) === null;

			if ($("#satnav_personal_address_manage_buttons").is(":visible"))
			{
				let title = $("#satnav_rename_entry_in_directory_title").text().replace(/\s+\(.*\)/, "");
				$("#satnav_rename_entry_in_directory_title").text(title + " (" + value + ")");
				$("#satnav_delete_directory_entry_in_popup").text(value);

				$("#satnav_manage_personal_address_rename_button").toggleClass("buttonDisabled", ! entryOnDisc);
				$("#satnav_manage_personal_address_rename_button").toggleClass("buttonSelected", entryOnDisc);
				$("#satnav_manage_personal_address_delete_button").toggleClass("buttonSelected", ! entryOnDisc);
				if (entryOnDisc) resizeButton("satnav_manage_personal_address_rename_button");
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
			let entryOnDisc = value.match(/&#x24E7;$/) === null;

			if ($("#satnav_professional_address_manage_buttons").is(":visible"))
			{
				let title = $("#satnav_rename_entry_in_directory_title").text().replace(/\s+\(.*\)/, "");
				$("#satnav_rename_entry_in_directory_title").text(title + " (" + value + ")");
				$("#satnav_delete_directory_entry_in_popup").text(value);

				$("#satnav_manage_professional_address_rename_button").toggleClass("buttonDisabled", ! entryOnDisc);
				$("#satnav_manage_professional_address_rename_button").toggleClass("buttonSelected", entryOnDisc);
				$("#satnav_manage_professional_address_delete_button").toggleClass("buttonSelected", ! entryOnDisc);
				if (entryOnDisc) resizeButton("satnav_manage_professional_address_rename_button");
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
				satnavRollingBackEntryByCharacter = true;  // Going into "character roll-back" mode
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
				satnavClearEnteredNumber();
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
						function ()
						{
							$("#satnav_to_mfd_show_characters_spinning_disc").show();
							ignoringIrCommands = false;
						},
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

				let lines = splitIntoLines("satnav_choice_list");

				if (lines[value] === undefined) break;

				let selectedLine = lines[value];
				let selectedStreetAndCity = selectedLine.replace(/<[^>]*>/g, '');  // Remove HTML formatting
				let selectedStreet = selectedStreetAndCity.replace(/ -.*-/g, '');  // Keep only the street; discard city

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
			ignoringIrCommands = false;
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

					// TODO - check translations
					let translations =
					{
						"set_language_french": "Ce service n'est pas<br />disponible pour<br />ce lieu",
						"set_language_german": "Dieser Dienst ist f&uuml;r<br />diesen Standort<br />nicht verf&uuml;gbar",
						"set_language_spanish": "Este servicio no est&aacute;<br />disponible para<br />esta ubicaci&oacute;n",
						"set_language_italian": "Questo servizio non<br />&egrave; disponibile per<br />questa localit&agrave;",
						"set_language_dutch": "Deze dienst is<br />niet beschikbaar voor<br />deze locatie"
					};
					let content = translations[localStorage.mfdLanguage] ||
						"This service is<br />not available for<br />this location";

					showStatusPopup(content, 8000);
				}
				else if (handleItemChange.mfdToSatnavRequest === "personal_address_list")
				{
					exitMenu();

					// TODO - check translations
					let translations =
					{
						"set_language_french": "R&eacute;pertoire personnel<br />est vide",
						"set_language_german": "Pers&ouml;nliches Zielverzeichnis<br />ist leer",
						"set_language_spanish": "Directorio personal<br />est&aacute; vac&iacute;o",
						"set_language_italian": "Rub. personale<br />&egrave; vuota",
						"set_language_dutch": "Lijst van priv&eacute;-<br />adressen is leeg"
					};
					let content = translations[localStorage.mfdLanguage] || "Personal directory<br />is empty";

					showStatusPopup(content, 8000);
				}
				else if (handleItemChange.mfdToSatnavRequest === "professional_address_list")
				{
					exitMenu();

					// TODO - check translations
					let translations =
					{
						"set_language_french": "R&eacute;pertoire professionnel<br />est vide",
						"set_language_german": "Berufliches Zielverzeichnis<br />ist leer",
						"set_language_spanish": "Directorio profesional<br />est&aacute; vac&iacute;o",
						"set_language_italian": "Rub. professionale<br />&egrave; vuota",
						"set_language_dutch": "Lijst van zaken-<br />adressen is leeg"
					};
					let content = translations[localStorage.mfdLanguage] || "Professional directory<br />is empty";

					showStatusPopup(content, 8000);
				} // if
			} // if
		} // case
		break;

		case "satnav_to_mfd_list_2_size":
		{
			// Sometimes there is a second list size. "satnav_to_mfd_list_size" (above) is the number of items
			// *containing* the selected characters; "satnav_to_mfd_list_2_size" (this one) is the number of items
			// *starting* with the selected characters.

			if (! satnavEnteringByLetter()) break;  // Only applicable in "character-by-character entry" mode

			// If there is a second list size, enable the "Validate" button
			$("#satnav_enter_characters_validate_button").toggleClass("buttonDisabled", value === "" || value === "0");
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
						let lines = splitIntoLines("satnav_choice_list");
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
			satnavServiceAddressSetButtons(parseInt(value) + 1);
		} // case
		break;

		case "satnav_guidance_preference":
		{
			if (value === "---") break;
			satnavGuidanceSetPreference(value);
			localStorage.satnavGuidancePreference = value;
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

			// Show "Continue guidance to destination?" popup if applicable
			if (contactKeyPosition === "ON" && localStorage.askForGuidanceContinuation === "YES")
			{
				showPopup("satnav_continue_guidance_popup", 15000);
				localStorage.askForGuidanceContinuation = "NO";
			} // if
		} // case
		break;

		case "satnav_distance_to_dest_via_road":
		case "satnav_service_address_distance":
		case "satnav_distance_to_dest_via_straight_line":
		case "satnav_turn_at":
		{
			let a = satnavFormatDistance(value);
			let distance = a[0];
			let unit = a[1];
			$("#" + item + "_number").text(distance);
			$("#" + item + "_unit").text(unit);

			if (item === "satnav_distance_to_dest_via_road")
			{
				// Show straight line distance if distance via road is reported "0"
				let id = "#satnav_distance_to_dest_via_";
				$(id + "road_visible").toggle(distance > 0);
				$(id + "straight_line_visible").toggle(distance == 0);
			} // if

			// Near junction: make sure to show the satnav guidance screen
			if (item === "satnav_turn_at" && $("#satnav_turn_at_indication").css("display") === "block")
			{
				let parts = value.split(" ");
				let reportedUnits = parts[1]; // 5000 m and less is reported in metres
				if (reportedUnits === "m" || reportedUnits ==="yd")
				{
					let distance = parts[0];
					let change = distance <= 300;
					if (localStorage.mfdDistanceUnit === "set_units_mph")
					{
						change = change || (vehicleSpeed > 35 && distance <= 800) || (vehicleSpeed > 50 && distance <= 2000);
					}
					else
					{
						change = change || (vehicleSpeed > 60 && distance <= 800) || (vehicleSpeed > 80 && distance <= 2000);
					} // if
					if (change) temporarilyChangeLargeScreenTo("satnav_guidance", 60000);
				} // if
			} // if
		} // case
		break;

		case "satnav_heading_on_roundabout_as_text":
		{
			// Show or hide the roundabout
			$("#satnav_curr_turn_roundabout").toggle(value !== "---");
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
			// Hide the spinning disc
			ignoringIrCommands = false;
			clearTimeout(handleItemChange.showCharactersSpinningDiscTimer);
			$("#satnav_to_mfd_show_characters_spinning_disc").hide();

			if ($("#satnav_enter_characters").is(":visible"))
			{
				if (satnavRollingBackEntryByCharacter) satnavAvailableCharactersStack.pop();
				else satnavAvailableCharactersStack.push(value);
			} // if

			clearTimeout(showAvailableCharactersTimer);

			if (value === "") break;

			if ($("#satnav_enter_characters").is(":visible")) satnavEnterOrDeleteCharacter(1);

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
				let splitAt = 24;

				// If the first 24 characters contain both 'I' and '1', the first line can contain 25 characters
				if (value.indexOf("I") > 0 && value.indexOf("1") > 0 && value.indexOf("1") <= 23) splitAt = 25;

				let line_1 = value.substr(0, splitAt);
				let line_2 = value.substr(splitAt);


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

				let translations =
				{
					"set_language_french": "Suivez le cap indiqu&eacute;",
					"set_language_german": "Folgen Sie dem Pfeil",
					"set_language_spanish": "Siga el rumbo indicado",
					"set_language_italian": "Sequire la direzione",
					"set_language_dutch": "Volg deze richting"
				};
				let content = translations[localStorage.mfdLanguage] || "Follow the heading";
				$("#satnav_guidance_next_street").html(content);
				satnavCutoffBottomLines($("#satnav_guidance_next_street"));

				//satnavCurrentStreet = ""; // Bug: original MFD clears currently known street in this situation...

				// To replicate a bug in the original MFD; in fact the current street is usually known
				$("#satnav_guidance_curr_street").html(notDigitizedAreaText);
				satnavCutoffBottomLines($("#satnav_guidance_curr_street"));
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
			$("#satnav_curr_heading_compass_tag *").removeClass('satNavInstructionDisabledIconText');
			$("#satnav_curr_heading_compass_needle *").removeClass('satNavInstructionDisabledIcon');
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
			$("#satnav_heading_to_dest_tag *").removeClass('satNavInstructionDisabledIconText');
			$("#satnav_heading_to_dest_pointer *").removeClass('satNavInstructionDisabledIcon');
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
		} // case
		break;

		case "trip_computer_popup_tab":
		{
			// Apply mapping from string to index
			let tabIndex =
				value === "TRIP_INFO_1" ? 1 :
				value === "TRIP_INFO_2" ? 2 : 0;
			selectTabInTripComputerPopup(tabIndex);
		} // case
		break;

		case "large_screen":
		{
			mfdLargeScreen = value;
		} // case
		break;

		case "mfd_disable_navigation_menu_while_driving":
		{
			mfdDisableNavigationMenuWhileDriving = value === "YES";
		} // case
		break;

		case "mfd_remote_control":
		{
			// Ignore remote control buttons if contact key is off
			if ($("#contact_key_position").text() === "OFF") break;

			if (economyMode === "ON") break;  // Ignore also when in power-save mode

			let parts = value.split(" ");
			let button = parts[0];

			if (button === "MENU_BUTTON")
			{
				// Directly hide any visible audio popup
				hideAudioSettingsPopup();
				hideTunerPresetsPopup();

				gotoTopLevelMenu();
			}
			else if (button === "ESC_BUTTON")
			{
				// If a popup is showing, hide it and break
				if (hideTunerPresetsPopup()) break;
				if (hideAudioSettingsPopup()) break;

				if (hidePopup()) break;

				if (ignoringIrCommands) break;

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
				// In non-menu screens, MFD dim level is adjusted with the IR remote control "UP" and "DOWN" buttons
				if (! inMenu() && (button === "UP_BUTTON" || button === "DOWN_BUTTON"))
				{
					let id = adjustDimLevel(headlightStatus, button)
					let dimLevel = $("#" + id).text();

					// Show very short popup
					$("#screen_brightness_popup_value").text(dimLevel);
					$("#screen_brightness_perc").css("transform", "scaleX(" + dimLevel/14 + ")");
					showPopup("screen_brightness_popup", 3000);

					// Store
					localStorage.dimLevelReduced = $("#display_reduced_brightness_level").text();
					localStorage.dimLevel = $("#display_brightness_level").text();

					break;
				} // if

				// While driving, it is not possible to navigate the main sat nav menu
				// (but "satnav_guidance_tools_menu" can be navigated)
				if (currentMenu === "satnav_main_menu" || menuStack[1] === "satnav_main_menu")
				{
					if (mfdDisableNavigationMenuWhileDriving && satnavVehicleMoving()) break;
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

				let held = parts[1];
				if (held && ! acceptingHeldValButton) break;

				// If one of these popups is showing, hide it and break
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
					if (mfdDisableNavigationMenuWhileDriving && satnavVehicleMoving()) break;
				} // if

				// Entering a character in the "satnav_enter_street_characters" screen?
				if ($("#satnav_enter_street_characters").is(":visible")) userHadOpportunityToEnterStreet = true;

				if (currentMenu === "satnav_enter_city_characters" || currentMenu === "satnav_enter_street_characters")
				{
					// In case both the "Val" packet and the "End_of_button_press" packet were missed...
					let id = "#satnav_to_mfd_show_characters_line_";
					let selectedChar = $(id + "1 .invertedText").text();
					if (! selectedChar) selectedChar = $(id + "2 .invertedText").text();
					if (selectedChar) satnavLastEnteredChar = selectedChar;
				} // if

				if (currentLargeScreenId === "satnav_vocal_synthesis_level") satnavNoAudioIcon();

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
				if (economyMode === "ON" && currentLargeScreenId !== "pre_flight" && engineRpm <= 0) showPowerSavePopup();
			}
			else if (value === "MFD_SCREEN_OFF")
			{
				hidePopup();
			} // if
		} // case
		break;

		case "recirc":
		case "rear_heater_2":
		case "ac_enabled":
		case "set_fan_speed":
		{
			// Has anything changed?
			if (value === handleItemChange[item]) break;
			let previousValue = handleItemChange[item];
			handleItemChange[item] = value;

			if (item === "set_fan_speed")
			{
				let on = value !== "0";
				$("#fan_icon").toggleClass("ledOn", on).toggleClass("ledOff", ! on);
			} // if

			if (previousValue === undefined) break;  // No popup when receiving initial update
			if (suppressClimateControlPopup !== null) break;  // No popup when suppress timer is running

			// No popup when in menu or "pre_flight" screen, or if engine off
			if (inMenu() || currentLargeScreenId === "pre_flight"
				|| engineRunning !== "YES" || contactKeyPosition !== "ON")
			{
				hidePopup("climate_control_popup");
			}
			else
			{
				showPopup("climate_control_popup", 5000);
			} // if
		} // case
		break;

		case "uptime_seconds":
		{
			$("#uptime").text(new Date(value * 1000).toLocaleTimeString([], { timeZone: "UTC" }));
		} // case
		break;

		default:
		break;
	} // switch
}

// -----
// Functions for localization (language, units)

function setLanguage(language)
{
	var languageSelections = "D E F GB NL I ".replace(/(.*?) /g, "<span class='languageIcon'>$1</span>");

	thousandsSeparator = ".";  // Default

	switch(language)
	{
		case "set_language_english":
		{
			thousandsSeparator = ",";

			doorOpenText = "Door open";
			doorsOpenText = "Doors open";
			bootOpenText = "Boot open";
			bootAndDoorOpenText = "Boot and door open";
			bootAndDoorsOpenText = "Boot and doors open";

			riskOfIceText = "Risk of ice!";

			$("#main_menu .menuTitleLine").html("Main menu<br />");
			$("#main_menu_goto_satnav_button").html("Navigation / Guidance");
			$("#main_menu_goto_screen_configuration_button").html("Configure display");

			let id = "#screen_configuration_menu";
			$(id + " .menuTitleLine").html("Configure display<br />");
			$(id + " .button:eq(0)").html("Set brightness");
			$(id + " .button:eq(1)").html("Set date and time");
			$(id + " .button:eq(2)").html("Select a language " + languageSelections);
			$(id + " .button:eq(3)").html("Set format and units");

			$("#set_screen_brightness .menuTitleLine").html("Set brightness<br />");
			$("#set_dark_theme_tag").html("Dark theme");
			$("#set_light_theme_tag").html("Light theme");
			$("#set_screen_brightness div:eq(2)").html("Brightness");

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
			satnavDiscUnreadbleText = "Navigation CD-ROM<br />is unreadable";
			satnavDiscMissingText = "Navigation CD-ROM is<br />missing";

			id = "#satnav_main_menu";
			$(id + " .menuTitleLine").html("Navigation / Guidance<br />");
			$(id + " .button:eq(0)").html("Enter new destination");
			$(id + " .button:eq(1)").html("Select a service");
			$(id + " .button:eq(2)").html("Select destination from memory");
			$(id + " .button:eq(3)").html("Navigation options");

			id = "#satnav_select_from_memory_menu";
			$(id + " .menuTitleLine").html("Select from memory<br />");
			$(id + " .button:eq(0)").html("Personal directory");
			$(id + " .button:eq(1)").html("Professional directory");

			id = "#satnav_navigation_options_menu";
			$(id + " .menuTitleLine").html("Navigation options<br />");
			$(id + " .button:eq(0)").html("Directory management");
			$(id + " .button:eq(1)").html("Vocal synthesis volume");
			$(id + " .button:eq(2)").html("Delete directories");

			id = "#satnav_directory_management_menu";
			$(id + " .menuTitleLine").html("Directory management<br />");
			$(id + " .button:eq(0)").html("Personal directory");
			$(id + " .button:eq(1)").html("Professional directory");

			id = "#satnav_guidance_tools_menu";
			$(id + " .menuTitleLine").html("Guidance tools<br />");
			$(id + " .button:eq(0)").html("Guidance criteria");
			$(id + " .button:eq(1)").html("Programmed destination");
			$(id + " .button:eq(2)").html("Vocal synthesis volume");

			id = "#satnav_guidance_preference_menu";
			$(id + " .tickBoxLabel:eq(0)").html("Fastest route<br />");
			$(id + " .tickBoxLabel:eq(1)").html("Shortest route<br />");
			$(id + " .tickBoxLabel:eq(2)").html("Avoid highway route<br />");
			$(id + " .tickBoxLabel:eq(3)").html("Fast/short compromise route<br />");

			id = "#satnav_vocal_synthesis_level";
			$(id + " .menuTitleLine").html("Vocal synthesis level<br />");
			$(id + " .tag").html("Level");

			$("#satnav_enter_city_characters .tag").html("Enter city");
			$("#satnav_enter_street_characters .tag").html("Enter street");
			$("#satnav_enter_house_number .tag").html("Enter number");

			id = "#satnav_enter_characters";
			$(id + " .tag:eq(0)").html("Choose next letter");
			$(id + " .tag:eq(1)").html("Enter city");
			$(id + " .tag:eq(2)").html("Enter street");

			$("#satnav_tag_city_list").html("Choose city");
			$("#satnav_tag_street_list").html("Choose street");
			$("#satnav_tag_service_list").html("Service list");
			$("#satnav_tag_personal_address_list").html("Personal address list");
			$("#satnav_tag_professional_address_list").html("Professional address list");

			id = "#satnav_show_personal_address";
			$(id + " .tag:eq(0)").html("City");
			$(id + " .tag:eq(1)").html("Street");
			$(id + " .tag:eq(2)").html("Number");

			id = "#satnav_show_professional_address";
			$(id + " .tag:eq(0)").html("City");
			$(id + " .tag:eq(1)").html("Street");
			$(id + " .tag:eq(3)").html("Number");

			id = "#satnav_show_service_address";
			$(id + " .tag:eq(0)").html("City");
			$(id + " .tag:eq(1)").html("Street");

			id = "#satnav_show_current_destination";
			$(id + " .tag:eq(0)").html("Programmed destination");
			$(id + " .tag:eq(1)").html("City");
			$(id + " .tag:eq(2)").html("Street");
			$(id + " .tag:eq(3)").html("Number");

			id = "#satnav_show_programmed_destination";
			$(id + " .tag:eq(0)").html("Programmed destination");
			$(id + " .tag:eq(1)").html("City");
			$(id + " .tag:eq(2)").html("Street");
			$(id + " .tag:eq(3)").html("Number");

			id = "#satnav_show_last_destination";
			$(id + " .tag:eq(0)").html("Select a service");
			$(id + " .tag:eq(1)").html("City");
			$(id + " .tag:eq(2)").html("Street");
			$(id + " .button:eq(2)").html("Current location");

			id = "#satnav_archive_in_directory";
			$(id + "_title").html("Archive in directory");
			$(id + " .satNavEntryNameTag").html("Name");
			$(id + " .button:eq(0)").html("Correction");
			$(id + " .button:eq(1)").html("Personal dir");
			$(id + " .button:eq(2)").html("Professional dir");

			id = "#satnav_rename_entry_in_directory";
			$(id + "_title").html("Rename entry");
			$(id + " .satNavEntryNameTag").html("Name");
			$(id + " .button:eq(1)").html("Correction");

			$("#satnav_reached_destination_popup_title").html("Destination reached");
			$("#satnav_delete_item_popup_title").html("Delete item ?<br />");
			$("#satnav_guidance_preference_popup_title").html("Keep criteria");
			$("#satnav_delete_directory_data_popup_title").html("This will delete all data in directories");
			$("#satnav_continue_guidance_popup_title").html("Continue guidance to destination ?");

			$(".yesButton").html("Yes");
			$(".noButton").html("No");

			$(".validateButton").html("Validate");
			$("#satnav_disclaimer_validate_button").html("Validate");

			id = "#satnav_manage_personal_address_";
			$(id + "rename_button").html("Rename");
			$(id + "delete_button").html("Delete");

			id = "#satnav_manage_professional_address_";
			$(id + "rename_button").html("Rename");
			$(id + "delete_button").html("Delete");

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
			thousandsSeparator = " ";

			doorOpenText = "Porte ouverte";
			doorsOpenText = "Portes ouverts";
			bootOpenText = "Coffre ouvert";
			bootAndDoorOpenText = "Porte et coffre ouverts";
			bootAndDoorsOpenText = "Portes et coffre ouverts";

			riskOfIceText = "Risque de glace!";  // TODO - check

			$("#main_menu .menuTitleLine").html("Menu g&eacute;n&eacute;ral<br />");
			$("#main_menu_goto_satnav_button").html("Navigation / Guidage");
			$("#main_menu_goto_screen_configuration_button").html("Configuration afficheur");

			let id = "#screen_configuration_menu";
			$(id + " .menuTitleLine").html("Configuration afficheur<br />");
			$(id + " .button:eq(0)").html("R&eacute;glage luminosit&eacute;");
			$(id + " .button:eq(1)").html("R&eacute;glage de date et heure");
			$(id + " .button:eq(2)").html("Choix de la langue " + languageSelections);
			$(id + " .button:eq(3)").html("R&eacute;glage des formats et unit&eacute;s");

			$("#set_screen_brightness .menuTitleLine").html("R&eacute;glage luminosit&eacute;<br />");
			$("#set_dark_theme_tag").html("Th&egrave;me fonc&eacute;");
			$("#set_light_theme_tag").html("Th&egrave;me clair");
			$("#set_screen_brightness div:eq(2)").html("Luminosit&eacute;");

			$("#set_date_time .menuTitleLine").html("R&eacute;glage de date et heure<br />");
			$("#set_language .menuTitleLine").html("Choix de la langue<br />");
			$("#set_units .menuTitleLine").html("R&eacute;glage des formats et unit&eacute;s<br />");

			$("#satnav_initializing_popup .messagePopupArea").html("Initialisation du syst&egrave;me<br />de navigation"); // TODO - check
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
			satnavDiscUnreadbleText = "Le CD-ROM de navigation<br />est illisible";
			satnavDiscMissingText = "Le CD-ROM de navigation<br />est absent";

			id = "#satnav_main_menu";
			$(id + " .menuTitleLine").html("Navigation / Guidance<br />");
			$(id + " .button:eq(0)").html("Saisie d'une nouvelle destination");
			$(id + " .button:eq(1)").html("Choix d'un service");
			$(id + " .button:eq(2)").html("Choix d'une destination archiv&eacute;e");
			$(id + " .button:eq(3)").html("Options de navigation");

			id = "#satnav_select_from_memory_menu";
			$(id + " .menuTitleLine").html("Choix destination archiv&eacute;e<br />");
			$(id + " .button:eq(0)").html("R&eacute;pertoire personnel");
			$(id + " .button:eq(1)").html("R&eacute;pertoire professionnel");

			id = "#satnav_navigation_options_menu";
			$(id + " .menuTitleLine").html("Options de navigation<br />");
			$(id + " .button:eq(0)").html("Gestion des r&eacute;pertoires");
			$(id + " .button:eq(1)").html("Volume synth&egrave;se vocale");
			$(id + " .button:eq(2)").html("Effacement des r&eacute;pertoires");

			id = "#satnav_directory_management_menu";
			$(id + " .menuTitleLine").html("Gestion des r&eacute;pertoires<br />");
			$(id + " .button:eq(0)").html("R&eacute;pertoire personnel");
			$(id + " .button:eq(1)").html("R&eacute;pertoire professionnel");

			id = "#satnav_guidance_tools_menu";
			$(id + " .menuTitleLine").html("Outils de guidage<br />");
			$(id + " .button:eq(0)").html("Crit&egrave;res de guidage");
			$(id + " .button:eq(1)").html("Destination programm&eacute;e");
			$(id + " .button:eq(2)").html("Volume synth&egrave;se vocale");

			id = "#satnav_guidance_preference_menu";
			$(id + " .tickBoxLabel:eq(0)").html("Trajet le plus rapide<br />");
			$(id + " .tickBoxLabel:eq(1)").html("Trajet le plus court<br />");
			$(id + " .tickBoxLabel:eq(2)").html("Itin&eacute;raire sans voie rapide<br />");
			$(id + " .tickBoxLabel:eq(3)").html("Compromis rapidit&eacute;/distance<br />");

			id = "#satnav_vocal_synthesis_level";
			$(id + " .menuTitleLine").html("Volume synth&egrave;se vocale<br />");
			$(id + " .tag").html("Volume");

			$("#satnav_enter_city_characters .tag").html("Saisie de la ville");
			$("#satnav_enter_street_characters .tag").html("Saisie de la voie");
			$("#satnav_enter_house_number .tag").html("Saisie du num&eacute;ro");

			id = "#satnav_enter_characters";
			$(id + " .tag:eq(0)").html("Choisis une lettre");  // TODO - check
			$(id + " .tag:eq(1)").html("Saisie de la ville");
			$(id + " .tag:eq(2)").html("Saisie de la voie");

			$("#satnav_tag_city_list").html("Saisie de la ville");
			$("#satnav_tag_street_list").html("Saisie de la voie");
			$("#satnav_tag_service_list").html("Choix d'un service");  // TODO - check
			$("#satnav_tag_personal_address_list").html("R&eacute;pertoire personnel");  // TODO - check
			$("#satnav_tag_professional_address_list").html("R&eacute;pertoire professionnel");  // TODO - check

			id = "#satnav_show_personal_address";
			$(id + " .tag:eq(0)").html("Ville");
			$(id + " .tag:eq(1)").html("Voie");
			$(id + " .tag:eq(2)").html("Num&eacute;ro");

			id = "#satnav_show_professional_address";
			$(id + " .tag:eq(0)").html("Ville");
			$(id + " .tag:eq(1)").html("Voie");
			$(id + " .tag:eq(3)").html("Num&eacute;ro");

			id = "#satnav_show_service_address";
			$(id + " .tag:eq(0)").html("Ville");
			$(id + " .tag:eq(1)").html("Voie");

			id = "#satnav_show_current_destination";
			$(id + " .tag:eq(0)").html("Destination programm&eacute;e");
			$(id + " .tag:eq(1)").html("Ville");
			$(id + " .tag:eq(2)").html("Voie");
			$(id + " .tag:eq(3)").html("Num&eacute;ro");

			id = "#satnav_show_programmed_destination";
			$(id + " .tag:eq(0)").html("Destination programm&eacute;e");
			$(id + " .tag:eq(1)").html("Ville");
			$(id + " .tag:eq(2)").html("Voie");
			$(id + " .tag:eq(3)").html("Num&eacute;ro");

			id = "#satnav_show_last_destination";
			$(id + " .tag:eq(0)").html("Choix d'un service");
			$(id + " .tag:eq(1)").html("Ville");
			$(id + " .tag:eq(2)").html("Voie");
			$(id + " .button:eq(2)").html("Position actuelle");  // TODO - check

			id = "#satnav_archive_in_directory";
			$(id + "_title").html("Archiver dans r&eacute;pertoire");
			$(id + " .satNavEntryNameTag").html("Libell&eacute;");
			$(id + " .button:eq(0)").html("Corriger");
			$(id + " .button:eq(1)").html("R&eacute;p. personnel");
			$(id + " .button:eq(2)").html("R&eacute;p. professionel");

			id = "#satnav_rename_entry_in_directory";
			$(id + "_title").html("Renommer libell&eacute;");  // TODO - check
			$(id + " .satNavEntryNameTag").html("Libell&eacute;");
			$(id + " .button:eq(1)").html("Corriger");

			$("#satnav_reached_destination_popup_title").html("Destination atteinte");  // TODO - check
			$("#satnav_delete_item_popup_title").html("Voulez-vous supprimer<br />la fiche ?<br />");
			$("#satnav_guidance_preference_popup_title").html("Conserver le crit&egrave;re");
			$("#satnav_delete_directory_data_popup_title").html("Cela supprimera toutes les donn&eacute;es des r&eacute;pertoires");  // TODO - check
			$("#satnav_continue_guidance_popup_title").html("Souhaitez-vous poursuivre le guidage vers votre destination ?");

			$(".yesButton").html("Oui");
			$(".noButton").html("Non");

			$(".validateButton").html("Valider");
			$("#satnav_disclaimer_validate_button").html("Valider");

			id = "#satnav_manage_personal_address_";
			$(id + "rename_button").html("Renommer");
			$(id + "delete_button").html("Supprimer");

			id = "#satnav_manage_professional_address_";
			$(id + "rename_button").html("Renommer");
			$(id + "delete_button").html("Supprimer");

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

			riskOfIceText = "Rutschgefahr!";  // TODO - check

			$("#main_menu .menuTitleLine").html("Hauptmen&uuml;<br />");
			$("#main_menu_goto_satnav_button").html("Navigation / F&uuml;hrung");
			$("#main_menu_goto_screen_configuration_button").html("Display konfigurieren");

			let id = "#screen_configuration_menu";
			$(id + " .menuTitleLine").html("Display konfigurieren<br />");
			$(id + " .button:eq(0)").html("Helligkeit einstellen");
			$(id + " .button:eq(1)").html("Datum und Uhrzeit einstellen");
			$(id + " .button:eq(2)").html("Sprache w&auml;hlen " + languageSelections);
			$(id + " .button:eq(3)").html("Einstellen der Einheiten");

			$("#set_screen_brightness .menuTitleLine").html("Helligkeit einstellen<br />");
			$("#set_dark_theme_tag").html("Dunkles Thema");
			$("#set_light_theme_tag").html("Helles Thema");
			$("#set_screen_brightness div:eq(2)").html("Bildhelligkeit");

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
			changeText = "Ändern";  // Not "&Auml;nderen": text-based comparison doesn't work with HTML special chars
			noNumberText = "Keine Nummer";
			stopGuidanceText = "F&uuml;hrung abbrechen";
			resumeGuidanceText = "F&uuml;hrung wieder aufnehmen";
			streetNotListedText = "Stra&szlig;e unbekannt";  // TODO - check
			satnavDiscUnreadbleText = "Die Navigations CD-ROM<br />is unleserlich";
			satnavDiscMissingText = "Die Navigations CD-ROM<br />fehlt";

			id = "#satnav_main_menu";
			$(id + " .menuTitleLine").html("Navigation / F&uuml;hrung<br />");
			$(id + " .button:eq(0)").html("Neues Ziel eingeben");
			$(id + " .button:eq(1)").html("Einen Dienst w&auml;hlen");
			$(id + " .button:eq(2)").html("Gespeichertes Ziel W&auml;hlen");
			$(id + " .button:eq(3)").html("Navigationsoptionen");

			id = "#satnav_select_from_memory_menu";
			$(id + " .menuTitleLine").html("Gespeichertes Ziel w&auml;hlen<br />");
			$(id + " .button:eq(0)").html("Pers&ouml;nliches Zielverzeichnis");
			$(id + " .button:eq(1)").html("Berufliches Zielverzeichnis");

			id = "#satnav_navigation_options_menu";
			$(id + " .menuTitleLine").html("Navigationsoptionen<br />");
			$(id + " .button:eq(0)").html("Verwalben der Verzeichnisse");
			$(id + " .button:eq(1)").html("Lautst. der Synthesestimme");
			$(id + " .button:eq(2)").html("L&ouml;schen der Verzeichnisse");

			id = "#satnav_directory_management_menu";
			$(id + " .menuTitleLine").html("Verwalben der Verzeichnisse<br />");
			$(id + " .button:eq(0)").html("Pers&ouml;nliches Zielverzeichnis");
			$(id + " .button:eq(1)").html("Berufliches Zielverzeichnis");

			id = "#satnav_guidance_tools_menu";
			$(id + " .menuTitleLine").html("F&uuml;hrungshilfen<br />");
			$(id + " .button:eq(0)").html("F&uuml;hrungskriterien");
			$(id + " .button:eq(1)").html("Programmiertes Ziel");
			$(id + " .button:eq(2)").html("Lautst. der Synthesestimme");

			id = "#satnav_guidance_preference_menu";
			$(id + " .tickBoxLabel:eq(0)").html("Die schnellste Route<br />");
			$(id + " .tickBoxLabel:eq(1)").html("Die k&uuml;rzeste Route<br />");
			$(id + " .tickBoxLabel:eq(2)").html("Autobahn meiden<br />");
			$(id + " .tickBoxLabel:eq(3)").html("Vergleich Fahrzeit/Routenl&auml;nge<br />");

			id = "#satnav_vocal_synthesis_level";
			$(id + " .menuTitleLine").html("Lautst. der Synthesestimme<br />");
			$(id + " .tag").html("Lautst.");

			$("#satnav_enter_city_characters .tag").html("Stadt eingeben");
			$("#satnav_enter_street_characters .tag").html("Stra&szlig;e eingeben");
			$("#satnav_enter_house_number .tag").html("Hausnummer eingeben");

			id = "#satnav_enter_characters";
			$(id + " .tag:eq(0)").html("Buchstabe w&auml;hlen");  // TODO - check
			$(id + " .tag:eq(1)").html("Stadt eingeben");
			$(id + " .tag:eq(2)").html("Stra&szlig;e eingeben");

			$("#satnav_tag_city_list").html("Stadt eingeben");
			$("#satnav_tag_street_list").html("Stra&szlig;e eingeben");
			$("#satnav_tag_service_list").html("Einen Dienst w&auml;hlen");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Pers&ouml;nliches Zielverzeichnis");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Berufliches Zielverzeichnis");  // TODO - check

			id = "#satnav_show_personal_address";
			$(id + " .tag:eq(0)").html("Stadt");
			$(id + " .tag:eq(1)").html("Stra&szlig;e");
			$(id + " .tag:eq(2)").html("Nummer");

			id = "#satnav_show_professional_address";
			$(id + " .tag:eq(0)").html("Stadt");
			$(id + " .tag:eq(1)").html("Stra&szlig;e");
			$(id + " .tag:eq(3)").html("Nummer");

			id = "#satnav_show_service_address";
			$(id + " .tag:eq(0)").html("Stadt");
			$(id + " .tag:eq(1)").html("Stra&szlig;e");

			id = "#satnav_show_current_destination";
			$(id + " .tag:eq(0)").html("Programmiertes Ziel");
			$(id + " .tag:eq(1)").html("Stadt");
			$(id + " .tag:eq(2)").html("Stra&szlig;e");
			$(id + " .tag:eq(3)").html("Nummer");

			id = "#satnav_show_programmed_destination";
			$(id + " .tag:eq(0)").html("Programmiertes Ziel");
			$(id + " .tag:eq(1)").html("Stadt");
			$(id + " .tag:eq(2)").html("Stra&szlig;e");
			$(id + " .tag:eq(3)").html("Nummer");

			id = "#satnav_show_last_destination";
			$(id + " .tag:eq(0)").html("Einen Dienst w&auml;hlen");
			$(id + " .tag:eq(1)").html("Stadt");
			$(id + " .tag:eq(2)").html("Stra&szlig;e");
			$(id + " .button:eq(2)").html("Aktueller Standort");  // TODO - check

			id = "#satnav_archive_in_directory";
			$(id + "_title").html("Im Verzeichnis speichern");
			$(id + " .satNavEntryNameTag").html("Name");
			$(id + " .button:eq(0)").html("Korrigieren");
			$(id + " .button:eq(1)").html("Pers&ouml;nl. Speic");
			$(id + " .button:eq(2)").html("Berufl. Speich");

			id = "#satnav_rename_entry_in_directory";
			$(id + "_title").html("Umbenennen");
			$(id + " .satNavEntryNameTag").html("Name");
			$(id + " .button:eq(1)").html("Korrigieren");

			$("#satnav_reached_destination_popup_title").html("Ziel erreicht");  // TODO - check
			$("#satnav_delete_item_popup_title").html("M&ouml;chten Sie Diese<br />Position l&ouml;schen ?<br />");
			$("#satnav_guidance_preference_popup_title").html("Beibehalten Routenart");
			$("#satnav_delete_directory_data_popup_title").html("Hiermit werden alle Daten der Verzeichnisse gel&ouml;scht");
			$("#satnav_continue_guidance_popup_title").html("M&ouml;chten Sie weiter zu Ihrem Ziel gef&uuml;hrt werden ?");

			$(".yesButton").html("Ja");
			$(".noButton").html("Nein");

			$(".validateButton").html("Best&auml;tigen");
			$("#satnav_disclaimer_validate_button").html("Best&auml;tigen");

			id = "#satnav_manage_personal_address_";
			$(id + "rename_button").html("Umbenennen");
			$(id + "delete_button").html("L&ouml;schen");

			id = "#satnav_manage_professional_address_";
			$(id + "rename_button").html("Umbenennen");
			$(id + "delete_button").html("L&ouml;schen");

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

			riskOfIceText = "Riesgo de resbalar!";  // TODO - check

			$("#main_menu .menuTitleLine").html("Men&uacute; general<br />");
			$("#main_menu_goto_satnav_button").html("Navegaci&oacute;n / Guiado");
			$("#main_menu_goto_screen_configuration_button").html("Configuraci&oacute;n de pantalla");

			let id = "#screen_configuration_menu";
			$(id + " .menuTitleLine").html("Configuraci&oacute;n de pantalla<br />");
			$(id + " .button:eq(0)").html("Ajustar luminosidad");
			$(id + " .button:eq(1)").html("Ajustar hora y fecha");
			$(id + " .button:eq(2)").html("Seleccionar idioma " + languageSelections);
			$(id + " .button:eq(3)").html("Ajustar formatos y unidades");

			$("#set_screen_brightness .menuTitleLine").html("Ajustar luminosidad<br />");
			$("#set_dark_theme_tag").html("Tema oscuro");
			$("#set_light_theme_tag").html("Tema claro");
			$("#set_screen_brightness div:eq(2)").html("Luminosidad");

			$("#set_date_time .menuTitleLine").html("Ajustar hora y fecha<br />");
			$("#set_language .menuTitleLine").html("Seleccionar idioma<br />");
			$("#set_units .menuTitleLine").html("Ajustar formatos y unidades<br />");

			$("#satnav_initializing_popup .messagePopupArea").html("Sistema de navegaci&oacute;n<br />en proceso de<br />inicializaci&oacute;n");  // TODO - check
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
			satnavDiscUnreadbleText = "No se puede leer<br />CD-ROM de navegaci&oacute;n";
			satnavDiscMissingText = "No hay CD-ROM de<br />navegaci&oacute;n";

			id = "#satnav_main_menu";
			$(id + " .menuTitleLine").html("Navegaci&oacute;n / Guiado<br />");
			$(id + " .button:eq(0)").html("Introducir nuevo destino");
			$(id + " .button:eq(1)").html("Seleccionar servicio");
			$(id + " .button:eq(2)").html("Seleccionar destino archivado");
			$(id + " .button:eq(3)").html("Opciones de navegaci&oacute;n");

			id = "#satnav_select_from_memory_menu";
			$(id + " .menuTitleLine").html("Seleccionar destino archivado<br />");
			$(id + " .button:eq(0)").html("Directorio personal");
			$(id + " .button:eq(1)").html("Directorio profesional");

			id = "#satnav_navigation_options_menu";
			$(id + " .menuTitleLine").html("Opciones de navegaci&oacute;n<br />");
			$(id + " .button:eq(0)").html("Gesti&oacute;n de directorios");
			$(id + " .button:eq(1)").html("Volumen de s&iacute;ntesis vocal");
			$(id + " .button:eq(2)").html("Borrado de directorios");

			id = "#satnav_directory_management_menu";
			$(id + " .menuTitleLine").html("Gesti&oacute;n de directorios<br />");
			$(id + " .button:eq(0)").html("Directorio personal");
			$(id + " .button:eq(1)").html("Directorio profesional");

			id = "#satnav_guidance_tools_menu";
			$(id + " .menuTitleLine").html("Herramientas de guiado<br />");
			$(id + " .button:eq(0)").html("Criterios de guiado");
			$(id + " .button:eq(1)").html("Destino programado");
			$(id + " .button:eq(2)").html("Volumen de s&iacute;ntesis vocal");

			id = "#satnav_guidance_preference_menu";
			$(id + " .tickBoxLabel:eq(0)").html("M&aacute;s r&aacute;pido<br />");
			$(id + " .tickBoxLabel:eq(1)").html("M&aacute;s corto<br />");
			$(id + " .tickBoxLabel:eq(2)").html("Sin autopistas<br />");
			$(id + " .tickBoxLabel:eq(3)").html("Compromiso r&aacute;pido/corto<br />");

			id = "#satnav_vocal_synthesis_level";
			$(id + " .menuTitleLine").html("Volumen de s&iacute;ntesis vocal<br />");
			$(id + " .tag").html("Volumen");

			$("#satnav_enter_city_characters .tag").html("Introducir la ciudad");
			$("#satnav_enter_street_characters .tag").html("Introducir la calle");
			$("#satnav_enter_house_number .tag").html("Introducir el n&uacute;mero");

			id = "#satnav_enter_characters";
			$(id + " .tag:eq(0)").html("Elegir letra");  // TODO - check
			$(id + " .tag:eq(1)").html("Introducir la ciudad");
			$(id + " .tag:eq(2)").html("Introducir la calle");

			$("#satnav_tag_city_list").html("Introducir la ciudad");
			$("#satnav_tag_street_list").html("Introducir la calle");
			$("#satnav_tag_service_list").html("Seleccionar servicio");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Directorio personal");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Directorio profesional");  // TODO - check

			id = "#satnav_show_personal_address";
			$(id + " .tag:eq(0)").html("Ciudad");
			$(id + " .tag:eq(1)").html("Calle");
			$(id + " .tag:eq(2)").html("N&uacute;mero");

			id = "#satnav_show_professional_address";
			$(id + " .tag:eq(0)").html("Ciudad");
			$(id + " .tag:eq(1)").html("Calle");
			$(id + " .tag:eq(3)").html("N&uacute;mero");

			id = "#satnav_show_service_address";
			$(id + " .tag:eq(0)").html("Ciudad");
			$(id + " .tag:eq(1)").html("Calle");

			id = "#satnav_show_current_destination";
			$(id + " .tag:eq(0)").html("Destino programado");
			$(id + " .tag:eq(1)").html("Ciudad");
			$(id + " .tag:eq(2)").html("Calle");
			$(id + " .tag:eq(3)").html("N&uacute;mero");

			id = "#satnav_show_programmed_destination";
			$(id + " .tag:eq(0)").html("Destino programado");
			$(id + " .tag:eq(1)").html("Ciudad");
			$(id + " .tag:eq(2)").html("Calle");
			$(id + " .tag:eq(3)").html("N&uacute;mero");

			id = "#satnav_show_last_destination";
			$(id + " .tag:eq(0)").html("Seleccionar servicio");
			$(id + " .tag:eq(1)").html("Ciudad");
			$(id + " .tag:eq(2)").html("Calle");
			$(id + " .button:eq(2)").html("Ubicaci&oacute;n actual");  // TODO - check

			id = "#satnav_archive_in_directory";
			$(id + "_title").html("Archivar en directorio");
			$(id + " .satNavEntryNameTag").html("Denominaci&oacute;n");
			$(id + " .button:eq(0)").html("Corregir");
			$(id + " .button:eq(1)").html("Directorio personal");
			$(id + " .button:eq(2)").html("Directorio profesional");

			id = "#satnav_rename_entry_in_directory";
			$(id + "_title").html("Cambiar");
			$(id + " .satNavEntryNameTag").html("Denominaci&oacute;n");
			$(id + " .button:eq(1)").html("Corregir");

			$("#satnav_reached_destination_popup_title").html("Destino alcanzado");  // TODO - check
			$("#satnav_delete_item_popup_title").html("&iquest;Desea suprimir<br />la ficha?");
			$("#satnav_guidance_preference_popup_title").html("&iquest;Mantener criterios");  // TODO - check
			$("#satnav_delete_directory_data_popup_title").html("Se borrar&aacute;n todos los<br />catos de los directorios");
			$("#satnav_continue_guidance_popup_title").html("&iquest; Desea proseguire el guiado para el destino ?");

			$(".yesButton").html("S&iacute;");
			$(".noButton").html("No");

			$(".validateButton").html("Aceptar");
			$("#satnav_disclaimer_validate_button").html("Aceptar");

			id = "#satnav_manage_personal_address_";
			$(id + "rename_button").html("Cambiar");
			$(id + "delete_button").html("Suprimir");

			id = "#satnav_manage_professional_address_";
			$(id + "rename_button").html("Cambiar");
			$(id + "delete_button").html("Suprimir");

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

			riskOfIceText = "Rischio di scivolosit&agrave;!";  // TODO - check

			$("#main_menu .menuTitleLine").html("Men&ugrave; generale<br />");
			$("#main_menu_goto_satnav_button").html("Navigazione/Guida");
			$("#main_menu_goto_screen_configuration_button").html("Configurazione monitor");

			let id = "#screen_configuration_menu";
			$(id + " .menuTitleLine").html("Configurazione monitor<br />");
			$(id + " .button:eq(0)").html("Regolazione luminosit&agrave;");
			$(id + " .button:eq(1)").html("Regolazione date e ora");
			$(id + " .button:eq(2)").html("Scelta della lingua " + languageSelections);
			$(id + " .button:eq(3)").html("Regolazione formato e unita");

			$("#set_screen_brightness .menuTitleLine").html("Regolazione luminosit&agrave;<br />");
			$("#set_dark_theme_tag").html("Tema nero");
			$("#set_light_theme_tag").html("Tema chiaro");
			$("#set_screen_brightness div:eq(2)").html("Luminosit&agrave;");

			$("#set_date_time .menuTitleLine").html("Regolazione hora y fecha<br />");
			$("#set_language .menuTitleLine").html("Scelta della lingua<br />");
			$("#set_units .menuTitleLine").html("Regolazione formato e unita<br />");

			$("#satnav_initializing_popup .messagePopupArea").html("Sistema di navigazione<br />inizializzato");  // TODO - check
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
			cityCenterText = "Centro della città";  // Not "citt&agrave;": text-based comparison doesn't work with HTML special chars
			changeText = "Modificare";
			noNumberText = "Nessun numero";
			stopGuidanceText = "Interrompere la guida";
			resumeGuidanceText = "Riprendere la guida";
			streetNotListedText = "Strada non elencata";  // TODO - check
			satnavDiscUnreadbleText = "Il CD-ROM di navigazione<br />&egrave; illeggibile";
			satnavDiscMissingText = "Il CD-ROM di navigazione<br />&egrave; assente";

			id = "#satnav_main_menu";
			$(id + " .menuTitleLine").html("Navigazione/Guida<br />");
			$(id + " .button:eq(0)").html("Inserire una nuova destinazione");
			$(id + " .button:eq(1)").html("Scelta un servizio");
			$(id + " .button:eq(2)").html("Scelta una destinazione salvata");
			$(id + " .button:eq(3)").html("Opzioni di navigazione");

			id = "#satnav_select_from_memory_menu";
			$(id + " .menuTitleLine").html("Scelta una destinazione salvata<br />");
			$(id + " .button:eq(0)").html("Rub. personale");
			$(id + " .button:eq(1)").html("Rub. professionale");

			id = "#satnav_navigation_options_menu";
			$(id + " .menuTitleLine").html("Opzioni di navigazione<br />");
			$(id + " .button:eq(0)").html("Gestione rubrica");
			$(id + " .button:eq(1)").html("Volume navigazione");
			$(id + " .button:eq(2)").html("Cancella rubrica");

			id = "#satnav_directory_management_menu";
			$(id + " .menuTitleLine").html("Gestione rubrica<br />");
			$(id + " .button:eq(0)").html("Rub. personale");
			$(id + " .button:eq(1)").html("Rub. professionale");

			id = "#satnav_guidance_tools_menu";
			$(id + " .menuTitleLine").html("Strumenti de guida<br />");
			$(id + " .button:eq(0)").html("Criteri di guida");
			$(id + " .button:eq(1)").html("Destinazione programmata");
			$(id + " .button:eq(2)").html("Volume navigazione");

			id = "#satnav_guidance_preference_menu";
			$(id + " .tickBoxLabel:eq(0)").html("Percorso pi&ugrave; rapido<br />");
			$(id + " .tickBoxLabel:eq(1)").html("Percorso pi&ugrave; corto<br />");
			$(id + " .tickBoxLabel:eq(2)").html("Esclusione autostrada<br />");
			$(id + " .tickBoxLabel:eq(3)").html("Compromesso rapidit&agrave;/distanza<br />");

			id = "#satnav_vocal_synthesis_level";
			$(id + " .menuTitleLine").html("Volume navigazione<br />");
			$(id + " .tag").html("Volume");

			$("#satnav_enter_city_characters .tag").html("Selezionare citt&agrave;");
			$("#satnav_enter_street_characters .tag").html("Selezionare via");
			$("#satnav_enter_house_number .tag").html("Selezionare numero civico");

			id = "#satnav_enter_characters";
			$(id + " .tag:eq(0)").html("Scegli lettera");  // TODO - check
			$(id + " .tag:eq(1)").html("Selezionare citt&agrave;");
			$(id + " .tag:eq(2)").html("Selezionare via");

			$("#satnav_tag_city_list").html("Selezionare citt&agrave;");
			$("#satnav_tag_street_list").html("Selezionare via");
			$("#satnav_tag_service_list").html("Scelta un servizio");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Rubrica personale");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Rubrica professionale");  // TODO - check

			id = "#satnav_show_personal_address";
			$(id + " .tag:eq(0)").html("Citt&agrave;");
			$(id + " .tag:eq(1)").html("Via");
			$(id + " .tag:eq(2)").html("Numero");

			id = "#satnav_show_professional_address";
			$(id + " .tag:eq(0)").html("Citt&agrave;");
			$(id + " .tag:eq(1)").html("Via");
			$(id + " .tag:eq(3)").html("Numero");

			id = "#satnav_show_service_address";
			$(id + " .tag:eq(0)").html("Citt&agrave;");
			$(id + " .tag:eq(1)").html("Via");

			id = "#satnav_show_current_destination";
			$(id + " .tag:eq(0)").html("Destinazione programmata");
			$(id + " .tag:eq(1)").html("Citt&agrave;");
			$(id + " .tag:eq(2)").html("Via");
			$(id + " .tag:eq(3)").html("Numero");

			id = "#satnav_show_programmed_destination";
			$(id + " .tag:eq(0)").html("Destinazione programmata");
			$(id + " .tag:eq(1)").html("Citt&agrave;");
			$(id + " .tag:eq(2)").html("Via");
			$(id + " .tag:eq(3)").html("Numero");

			id = "#satnav_show_last_destination";
			$(id + " .tag:eq(0)").html("Scelta un servizio");
			$(id + " .tag:eq(1)").html("Citt&agrave;");
			$(id + " .tag:eq(2)").html("Via");
			$(id + " .button:eq(2)").html("Posizione attuale");  // TODO - check

			id = "#satnav_archive_in_directory";
			$(id + "_title").html("Salvare nella rubrica");
			$(id + " .satNavEntryNameTag").html("Denominazione");
			$(id + " .button:eq(0)").html("Correggere");
			$(id + " .button:eq(1)").html("Rub. personale");
			$(id + " .button:eq(2)").html("Rub. professionale");

			id = "#satnav_rename_entry_in_directory";
			$(id + "_title").html("Rinominare");
			$(id + " .satNavEntryNameTag").html("Denominazione");
			$(id + " .button:eq(1)").html("Correggere");

			$("#satnav_reached_destination_popup_title").html("Destinazione raggiunta");  // TODO - check
			$("#satnav_delete_item_popup_title").html("Cancellare la scheda?<br />");
			$("#satnav_guidance_preference_popup_title").html("Conservare il criterio");
			$("#satnav_delete_directory_data_popup_title").html("I dati delle rubrica<br />verranno cancellati");
			$("#satnav_continue_guidance_popup_title").html("Continuare la guida verso la meta prescelta?");

			$(".yesButton").html("S&igrave;");
			$(".noButton").html("No");

			$(".validateButton").html("Convalidare");
			$("#satnav_disclaimer_validate_button").html("Convalidare");

			id = "#satnav_manage_personal_address_";
			$(id + "rename_button").html("Rinominare");
			$(id + "delete_button").html("Eliminare");

			id = "#satnav_manage_professional_address_";
			$(id + "rename_button").html("Rinominare");
			$(id + "delete_button").html("Eliminare");

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

			riskOfIceText = "Kans op ijzel!";  // Should be: "Kans op gladheid"

			$("#main_menu .menuTitleLine").html("Hoofdmenu<br />");
			$("#main_menu_goto_satnav_button").html("Navigatie / Begeleiding");
			$("#main_menu_goto_screen_configuration_button").html("Beeldschermconfiguratie");

			let id = "#screen_configuration_menu";
			$(id + " .menuTitleLine").html("Beeldschermconfiguratie<br />");
			$(id + " .button:eq(0)").html("Instelling helderheid");
			$(id + " .button:eq(1)").html("Instelling datum en tijd");
			$(id + " .button:eq(2)").html("Taalkeuze " + languageSelections);
			$(id + " .button:eq(3)").html("Instelling van eenheden");

			$("#set_screen_brightness .menuTitleLine").html("Instelling helderheid<br />");
			$("#set_dark_theme_tag").html("Donkere modus");
			$("#set_light_theme_tag").html("Lichte modus");
			$("#set_screen_brightness div:eq(2)").html("Helderheid");

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
			satnavDiscUnreadbleText = "De navigatie CD-rom is<br />onleesbaar";
			satnavDiscMissingText = "De navigatie CD-rom is<br />niet aanwezig";

			id = "#satnav_main_menu";
			$(id + " .menuTitleLine").html("Navigatie/ Begeleiding<br />");
			$(id + " .button:eq(0)").html("Nieuwe bestemming invoeren");
			$(id + " .button:eq(1)").html("Informatie-dienstverlening");
			$(id + " .button:eq(2)").html("Bestemming uit archief kiezen");
			$(id + " .button:eq(3)").html("Navigatiekeuze");

			id = "#satnav_select_from_memory_menu";
			$(id + " .menuTitleLine").html("Bestemming uit archief kiezen<br />");
			$(id + " .button:eq(0)").html("Priv&eacute;-adressen");
			$(id + " .button:eq(1)").html("Zaken-adressen");

			id = "#satnav_navigation_options_menu";
			$(id + " .menuTitleLine").html("Navigatiekeuze<br />");
			$(id + " .button:eq(0)").html("Adressenbestand");
			$(id + " .button:eq(1)").html("Geluidsvolume");
			$(id + " .button:eq(2)").html("Bestanden wissen");

			id = "#satnav_directory_management_menu";
			$(id + " .menuTitleLine").html("Adressenbestand<br />");
			$(id + " .button:eq(0)").html("Priv&eacute;-adressen");
			$(id + " .button:eq(1)").html("Zaken-adressen");

			id = "#satnav_guidance_tools_menu";
			$(id + " .menuTitleLine").html("Navigatiehulp<br />");
			$(id + " .button:eq(0)").html("Navigatiecriteria");
			$(id + " .button:eq(1)").html("Geprogrammeerde bestemming");
			$(id + " .button:eq(2)").html("Geluidsvolume");

			id = "#satnav_guidance_preference_menu";
			$(id + " .tickBoxLabel:eq(0)").html("Snelste weg<br />");
			$(id + " .tickBoxLabel:eq(1)").html("Kortste afstand<br />");
			$(id + " .tickBoxLabel:eq(2)").html("Traject zonder snelweg<br />");
			$(id + " .tickBoxLabel:eq(3)").html("Compromis snelheid/afstand<br />");

			id = "#satnav_vocal_synthesis_level";
			$(id + " .menuTitleLine").html("Geluidsvolume<br />");
			$(id + " .tag").html("Volume");

			$("#satnav_enter_city_characters .tag").html("Stad invoeren");
			$("#satnav_enter_street_characters .tag").html("Straat invoeren");
			$("#satnav_enter_house_number .tag").html("Nummer invoeren");

			id = "#satnav_enter_characters";
			$(id + " .tag:eq(0)").html("Kies letter");
			$(id + " .tag:eq(1)").html("Stad invoeren");
			$(id + " .tag:eq(2)").html("Straat invoeren");

			$("#satnav_tag_city_list").html("Kies stad");
			$("#satnav_tag_street_list").html("Kies straat");
			$("#satnav_tag_service_list").html("Kies dienst");  // TODO - check
			$("#satnav_tag_personal_address_list").html("Kies priv&eacute;-adres");  // TODO - check
			$("#satnav_tag_professional_address_list").html("Kies zaken-adres");  // TODO - check

			id = "#satnav_show_personal_address";
			$(id + " .tag:eq(0)").html("Stad");
			$(id + " .tag:eq(1)").html("Straat");
			$(id + " .tag:eq(2)").html("Nummer");

			id = "#satnav_show_professional_address";
			$(id + " .tag:eq(0)").html("Stad");
			$(id + " .tag:eq(1)").html("Straat");
			$(id + " .tag:eq(3)").html("Nummer");

			id = "#satnav_show_service_address";
			$(id + " .tag:eq(0)").html("Stad");
			$(id + " .tag:eq(1)").html("Straat");

			id = "#satnav_show_current_destination";
			$(id + " .tag:eq(0)").html("Geprogrammeerde bestemming");
			$(id + " .tag:eq(1)").html("Stad");
			$(id + " .tag:eq(2)").html("Straat");
			$(id + " .tag:eq(3)").html("Nummer");

			id = "#satnav_show_programmed_destination";
			$(id + " .tag:eq(0)").html("Geprogrammeerde bestemming");
			$(id + " .tag:eq(1)").html("Stad");
			$(id + " .tag:eq(2)").html("Straat");
			$(id + " .tag:eq(3)").html("Nummer");

			id = "#satnav_show_last_destination";
			$(id + " .tag:eq(0)").html("Informatie-dienstverlening");
			$(id + " .tag:eq(1)").html("Stad");
			$(id + " .tag:eq(2)").html("Straat");
			$(id + " .button:eq(2)").html("Huidige locatie");

			id = "#satnav_archive_in_directory";
			$(id + "_title").html("Opslaan in adressenbestand");  // TODO - check
			$(id + " .satNavEntryNameTag").html("Naam");
			$(id + " .button:eq(0)").html("Verbeteren");
			$(id + " .button:eq(1)").html("Priv&eacute;-adressen");
			$(id + " .button:eq(2)").html("Zaken-adressen");

			id = "#satnav_rename_entry_in_directory";
			$(id + "_title").html("Nieuwe naam");
			$(id + " .satNavEntryNameTag").html("Naam");
			$(id + " .button:eq(1)").html("Verbeteren");

			$("#satnav_reached_destination_popup_title").html("Bestemming bereikt");  // TODO - check
			$("#satnav_delete_item_popup_title").html("Wilt u dit gegeven<br />wissen?<br />");
			$("#satnav_guidance_preference_popup_title").html("Bewaar de gegevens");
			$("#satnav_delete_directory_data_popup_title").html("Pas op : alle gegevens<br />worden gewist");
			$("#satnav_continue_guidance_popup_title").html("Wilt u met de navigatie doorgaan?");

			$(".yesButton").html("Ja");
			$(".noButton").html("Nee");

			$(".validateButton").html("Bevestigen");
			$("#satnav_disclaimer_validate_button").html("Bevestigen");

			id = "#satnav_manage_personal_address_";
			$(id + "rename_button").html("Nieuwe naam");
			$(id + "delete_button").html("Wissen");

			id = "#satnav_manage_professional_address_";
			$(id + "rename_button").html("Nieuwe naam");
			$(id + "delete_button").html("Wissen");

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

	$("#door_open_popup_text").html(doorOpenText);
	satnavGuidanceSetPreference(localStorage.satnavGuidancePreference);

	$("#satnav_navigation_options_menu .button:eq(3)").html(
		satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);
	$("#satnav_guidance_tools_menu .button:eq(3)").html(
		satnavMode === "IN_GUIDANCE_MODE" ? stopGuidanceText : resumeGuidanceText);
}

function setUnits(distanceUnit, temperatureUnit, timeUnit)
{
	localStorage.mfdDistanceUnit = distanceUnit;
	localStorage.mfdTemperatureUnit = temperatureUnit;
	localStorage.mfdTimeUnit = timeUnit;

	if (distanceUnit === "set_units_mph")
	{
		if ($('[gid="fuel_level_unit"]').first().text() !== "%") $('[gid="fuel_level_unit"]').text("gl");
		$('[gid="fuel_consumption_unit"]').text("mpg");
		$("#fuel_consumption_unit_sm").text("mpg");
		$('[gid="speed_unit"]').text("mph");
		$('[gid="distance_unit"]').text("mi");
	}
	else
	{
		if ($('[gid="fuel_level_unit"]').first().text() !== "%") $('[gid="fuel_level_unit"]').text("lt");
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
}

// To be called by the body "onload" event
function htmlBodyOnLoad()
{
	setColorTheme(localStorage.colorTheme);
	initDimLevel();
	setUnits(localStorage.mfdDistanceUnit, localStorage.mfdTemperatureUnit, localStorage.mfdTimeUnit);
	setLanguage(localStorage.mfdLanguage);

	gotoSmallScreen(localStorage.smallScreen);

	resizeScreenToFit();

	// Update time now and every next second
	updateDateTime();
	setInterval(updateDateTime, 1000);

	connectToWebSocket();
}

)=====";
