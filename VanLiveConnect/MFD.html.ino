
// Following DOM elements are compiled conditionally

#ifdef DEBUG_WEBSOCKET

// Flashing LED indicating web socket activity
#define COMMS_LED \
"		<div id='comms_led' class='led ledOn' style='left:125px; top:560px; width:20px; height:77px;'></div>\n"

#else
#define COMMS_LED ""
#endif // DEBUG_WEBSOCKET

char mfd_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
	<head>
		<meta name="google" content="notranslate" />
		<meta charset='utf-8'>

		<!-- Works on mobile Firefox, also in full-screen mode -->
		<meta name="viewport" content="width=1350, minimum-scale=0.2, maximum-scale=1, user-scalable=yes" />

		<title>Multi-functional Display</title>
		<link rel='icon' href='data:,' />

		<link rel="stylesheet" href="CarInfo.css" />
		<link rel="stylesheet" href="css/all.css" async />	<!-- Font Awesome -->

		<script src="jquery-3.5.1.min.js"></script>	<!-- jQuery -->

		<!-- Our own stuff. Added '?foo=1' to force Chrome reload; see also https://stackoverflow.com/a/70410178 -->
		<script src="MFD.js?foo=1" async></script>
	</head>

	<body id="body" translate="no" onload="htmlBodyOnLoad();" onresize="resizeScreenToFit();">


		<!-- "Small" information panel -->

		<div id="small_panel" style="position:absolute; left:0px; top:0px; width:390px; height:550px;">

			<!-- Trip info -->

			<div id="trip_info" style="display:block;">

				<!-- Tab links -->
				<div class="tab" style="left:30px; top:27px; width:500px; height:100px;">

					<button id="trip_1_button" class="tablinks active" style="left:0px; top:8px; width:120px;"
						onclick="openTripInfoTab(event, 'trip_1');">1</button>

					<button id="trip_2_button" class="tablinks" style="left:115px; top:8px; width:120px;"
						onclick="openTripInfoTab(event, 'trip_2');">2</button>

				</div>

				<!-- Tab contents -->

				<!-- Tripcounter #1 -->

				<div id="trip_1" class="tabContent" style="display:block; left:20px; top:90px; width:330px; height:400px;">

					<div class="icon iconSmall fas fa-angle-double-right" style="top:30px;"></div>
					<div class="icon iconSmall fas fa-fire-alt" style="left:50px; top:30px;"></div>
					<div gid="avg_consumption_1" class="dots" style="left:100px; top:20px; width:220px; text-align:right;">--.-</div>
					<div gid="fuel_consumption_unit" class="tag tripComputerTag" style="top:75px;">l/100 km</div>

					<div class="icon iconSmall fas fa-angle-double-right" style="top:160px;"></div>
					<div class="icon iconSmall fas fa-tachometer-alt" style="left:50px; top:160px;"></div>
					<div gid="avg_speed_1" class="dots" style="left:100px; top:150px; width:220px; text-align:right;">--</div>
					<div gid="speed_unit" class="tag tripComputerTag" style="top:205px;">km/h</div>

					<div class="icon iconSmall" style="top:280px;">...</div>
					<div class="icon iconSmall fas fa-car-side" style="left:50px; top:280px;"></div>
					<div gid="distance_1" class="dots" style="left:100px; top:270px; width:220px; text-align:right;">--</div>
					<div gid="distance_unit" class="tag tripComputerTag" style="top:325px;">km</div>

				</div>	<!-- Tripcounter #1 -->

				<!-- Tripcounter #2 -->

				<div id="trip_2" class="tabContent" style="display:none; left:20px; top:90px; width:330px; height:400px;">

					<div class="icon iconSmall fas fa-angle-double-right" style="top:30px;"></div>
					<div class="icon iconSmall fas fa-fire-alt" style="left:50px; top:30px;"></div>
					<div gid="avg_consumption_2" class="dots" style="left:100px; top:20px; width:220px; text-align:right;">--.-</div>
					<div gid="fuel_consumption_unit" class="tag tripComputerTag" style="top:75px;">l/100 km</div>

					<div class="icon iconSmall fas fa-angle-double-right" style="top:160px;"></div>
					<div class="icon iconSmall fas fa-tachometer-alt" style="left:50px; top:160px;"></div>
					<div gid="avg_speed_2" class="dots" style="left:100px; top:150px; width:220px; text-align:right;">--</div>
					<div gid="speed_unit" class="tag tripComputerTag" style="top:205px;">km/h</div>

					<div class="icon iconSmall" style="top:280px;">...</div>
					<div class="icon iconSmall fas fa-car-side" style="left:50px; top:280px;"></div>
					<div gid="distance_2" class="dots" style="left:100px; top:270px; width:220px; text-align:right;">--</div>
					<div gid="distance_unit" class="tag tripComputerTag" style="top:325px;">km</div>

				</div>	<!-- Tripcounter #2 -->

			</div>	<!-- Trip info -->

			<!-- GPS info: GPS fix icon and current street -->

			<div id="gps_info" style="display:none;">

				<div gid="satnav_gps_fix" class="iconVeryLarge led ledOff" style="left:120px; top:100px; height:160px; line-height:1;">
					<div class="centerAligned fas fa-satellite-dish"></div>
				</div>

				<!-- Current street and city -->
				<div id="satnav_curr_street_small"
					on_enter="satnavCutoffBottomLines($('#satnav_curr_street_small [gid=\'satnav_curr_street_shown\']'));"
					class="icon" style="word-wrap:break-word; top:270px; height:280px; width:390px;">
					<div gid="satnav_curr_street_shown" class="centerAligned" style="font-size:40px;"></div>
				</div>

			</div>

			<!-- Instrument panel -->

			<div id="instrument_small" style="display:none;">

				<div gid="vehicle_speed" class="dseg7" style="font-size:120px; left:10px; top:33px; width:260px;">0</div>
				<div gid="distance_unit" class="tag" style="text-align:left; left:280px; top:75px; font-size:30px;">km</div>
				<div class="tag" style="text-align:left; left:280px; top:110px; font-size:30px;">/h</div>

				<div gid="engine_rpm" class="dseg7" style="font-size:70px; left:10px; top:173px; width:260px;">0</div>
				<div class="tag" style="text-align:left; left:280px; top:203px; font-size:30px;">rpm</div>

				<div gid="fog_rear" class="iconSmallMaterialDesign led ledOff" style="left:40px; top:280px;transform: scaleX(-1);">
					<div class="centerAligned fas md-car-light-fog"></div>
				</div>

				<div gid="parking_light" class="iconSmallMaterialDesign led ledOff" style="left:120px; top:280px; color:rgb(119, 217, 64);">
					<div class="centerAligned fas md-car-parking-lights"></div>
				</div>

				<div gid="dipped_beam" class="iconSmallMaterialDesign led ledOff" style="left:120px; top:280px;">
					<div class="centerAligned fas md-car-light-dimmed"></div>
				</div>

				<div gid="high_beam" class="iconSmallMaterialDesign led ledOff" style="left:200px; top:280px;">
					<div class="centerAligned fas md-car-light-high"></div>
				</div>

				<div gid="fog_front" class="iconSmallMaterialDesign led ledOff" style="left:280px; top:280px;">
					<div class="centerAligned fas md-car-light-fog"></div>
				</div>

				<div class="icon iconSmall fas fa-gas-pump" style="left:30px; top:375px;"></div>
				<div gid="fuel_level" class="dseg7" style="font-size:50px; left:100px; top:372px; width:170px;">--.-</div>
				<div gid="fuel_level_unit" class="tag" style="text-align:left; left:280px; top:385px; font-size:30px;">lt</div>

				<!-- Engine coolant temperature -->
				<div class="icon iconSmall fas fa-thermometer-half" style="left:30px; top:445px;"></div>
				<div gid="coolant_temp" class="dseg7" style="font-size:50px; left:100px; top:442px; width:170px;">--.-</div>
				<div gid="coolant_temp_unit" class="tag" style="text-align:left; left:280px; top:455px; font-size:30px;">&deg;C</div>
			</div>
		</div>	<!-- "Small" information panel -->

		<!-- "Large" information panel -->

		<div id="large_panel" style="position:absolute; left:390px; top:0px; width:960px; height:550px;">

			<!-- Large clock (nothing better to show) -->

			<div id="clock" style="display:block; font-size:70px;"
				on_enter="$('#exterior_temp_shown').hide(); $('#date_time_small').hide();"
				on_exit="$('#exterior_temp_shown').show(); $('#date_time_small').show();">
				<div id="date_weekday" class="tag" style="top:50px; width:960px; text-align:center;">---</div>
				<div id="date" class="tag" style="font-size:60px; top:140px; width:960px; text-align:center;">---</div>
				<div id="time" class="tag" style="top:250px; width:960px; text-align:center;">--:--</div>

				<div id="splash_text" class="tag"
					style="top:330px; width:960px; height:220px; line-height:3; text-align:center; text-shadow:5px 5px 30px white;">
					PSA Live Display
				</div>

				<!-- Also show exterior temperature -->
				<div gid="exterior_temp_shown" class="tag"
					style="font-size:100px; top:330px; width:960px; height:220px; line-height:1.8; text-align:center;"></div>


				<!--
					"Gear" icon in the bottom right corner
					Note: the 'onclick="gearIconAreaClicked();"' is specified in a separate div, near the end
				-->
				<div class="iconSmall led ledOn" style="left:870px; top:480px;">
					<div class="centerAligned fas fa-cog"></div>
				</div>
			</div>	 <!-- "clock" -->

			<!-- Audio equipment -->

			<div id="audio" style="display:none;">

				<!-- Head unit status LEDs -->

				<div id="head_unit_power" class="iconSmall led ledOn" style="left:870px; top:40px;">
					<div class="centerAligned fas fa-power-off"></div>
				</div>
				<div id="cd_present" class="iconSmall led ledOn" style="left:870px; top:105px;">
					<div class="centerAligned fas fa-compact-disc"></div>
				</div>
				<div id="tape_present" class="iconSmallMaterialDesign led ledOff" style="left:870px; top:170px;">
					<div class="centerAligned fas md-cassette"></div>
				</div>
				<div id="cd_changer_cartridge_present" class="iconSmall led ledOff" style="left:870px; top:235px;">
					<div class="centerAligned fas fa-archive"></div>
				</div>

				<div gid="info_traffic" class="led ledOff" style="left:30px; top:490px; width:140px;">INFO</div>
				<div gid="ext_mute" class="led ledOff" style="left:210px; top:490px; width:110px;">EXT</div>
				<div gid="mute" class="led ledOff" style="left:330px; top:490px; width:150px;">MUTE</div>

				<div gid="ta_selected" class="led ledOn" style="left:670px; top:490px; width:100px;">TA</div>
				<div gid="ta_not_available" class="icon" style="position:absolute; left:670px; top:490px; width:100px;">
					<svg>
						<line stroke="var(--disabled-element-color)" stroke-width="14" stroke-linecap="round" x1="10" y1="30" x2="87" y2="8"></line>
					</svg>
				</div>

				<div id="tuner" style="display:none;">

					<div gid="tuner_memory" class="dseg7" style="font-size:80px; left:40px; top:40px;">-</div>

					<div id="fm_band" class="led ledOn" style="left:110px; top:35px; width:80px;">FM</div>
					<div id="am_band" class="led ledOff" style="left:200px; top:35px; width:160px;">AM/LW</div>
					<div id="fm_band_1" class="led ledOn" style="left:110px; top:78px; width:60px;">1</div>
					<div id="fm_band_2" class="led ledOff" style="left:180px; top:78px; width:60px;">2</div>
					<div id="fm_band_ast" class="led ledOff" style="left:250px; top:78px; width:110px;">AST</div>

					<div gid="frequency_data_small" style="display:block;">
						<div gid="frequency" class="dseg7" style="font-size:80px; left:385px; top:40px; width:260px;"></div>
						<div gid="frequency_h" class="dseg7" style="font-size:50px; left:630px; top:40px; width:60px;"></div>
						<div gid="frequency_khz" class="led ledOff" style="left:700px; top:35px; width:120px;">KHz</div>
						<div gid="frequency_mhz" class="led ledOff" style="left:700px; top:78px; width:120px;">MHz</div>
					</div>

					<div gid="frequency_data_large" style="display:none;">
						<div gid="frequency" class="dseg14" style="font-size:120px; left:10px; top:150px; width:400px; text-align:right;">---.-</div>
						<div gid="frequency_h" class="dseg14" style="font-size:120px; left:410px; top:150px; width:90px; text-align:left;">-</div>
						<div gid="frequency_unit" class="dseg14" style="font-size:120px; left:540px; top:150px; width:280px; text-align:right;">MHz</div>
					</div>

					<!-- Icons involved in station searching -->
					<div gid="search_manual" class="led ledOff" style="left:380px; top:290px; width:120px;">MAN</div>
					<div id="search_sensitivity_dx" class="led ledOff" style="left:530px; top:290px; width:80px;">Dx</div>
					<div id="search_sensitivity_lo" class="led ledOff" style="left:620px; top:290px; width:80px;">Lo</div>
					<div gid="search_direction_up" class="led ledOff fas fa-caret-up" style="left:730px; top:290px; width:40px; height:37px;"></div>
					<div gid="search_direction_down" class="led ledOff fas fa-caret-down" style="left:780px; top:290px; width:40px; height:37px;"></div>

					<div gid="fm_tuner_data" style="display:block;">
						<div id="rds_text" gid="rds_text" class="dseg14" style="font-size:120px; left:10px; top:150px; width:815px; text-align:right;"></div>

						<div class="tag" style="left:20px; top:368px; width:150px;">PTY</div>
						<div id="pty_standby_mode" class="led ledOn" style="display:none; font-size:53px; left:20px; top:360px; width:150px; height: 55px;">PTY</div>
						<div id="pty_selection_menu" class="led ledOn" style="display:none; font-size:53px; left:20px; top:360px; width:150px; height: 55px;">SEL</div>
						<div id="pty_16" class="dots" style="left:190px; top:353px; width:750px;"></div>
						<div id="selected_pty_16" class="dots" style="display:none; left:190px; top:357px; width:750px;"></div>

						<div class="tag" style="left:20px; top:428px; width:150px;">PI</div>
						<div gid="pi_country" class="dots" style="left:190px; top:413px; width:150px;">--</div>

						<div id="regional" class="led ledOn" style="left:510px; top:490px; width:140px;">REG</div>
						<div gid="rds_selected" class="led ledOff" style="left:795px; top:490px; width:140px;">RDS</div>
						<div gid="rds_not_available" class="icon" style="position:absolute; left:795px; top:490px; width:140px;">
							<svg>
								<line stroke="var(--disabled-element-color)" stroke-width="14" stroke-linecap="round" x1="10" y1="30" x2="127" y2="8"></line>
							</svg>
						</div>
					</div>

					<div class="tag" style="left:690px; top:428px; width:160px;">Signal</div>
					<div id="signal_strength" class="dots" style=" left:870px; top:413px;">--</div>

				</div>	<!-- "tuner" -->

				<!-- Removable media -->

				<div id="media" style="display:none;">

					<div class="tag" style="left:50px; top:63px; width:200px;">Source</div>

					<div id="tape" style="display:none;">

						<div class="dots" style="left:270px; top:48px; width:450px;">Tape</div>

						<div class="tag" style="left:50px; top:221px; width:200px;">Side</div>
						<div gid="tape_side" class="dseg7" style="font-size:123px; left:280px; top:145px; width:140px;">-</div>

						<div gid="tape_status_stopped" class="icon mediaStatus">
							<div class="centerAligned fas fa-stop"></div>
						</div>
						<div gid="tape_status_loading" class="icon mediaStatus">
							<div class="centerAligned fas fa-sign-in-alt"></div>
						</div>
						<div gid="tape_status_play" class="icon mediaStatus">
							<div class="centerAligned fas fa-play"></div>
						</div>
						<div gid="tape_status_fast_forward" class="icon mediaStatus">
							<div class="centerAligned fas fa-forward"></div>
						</div>
						<div gid="tape_status_next_track" class="icon mediaStatus">
							<div class="centerAligned fas fa-fast-forward"></div>
						</div>
						<div gid="tape_status_rewind" class="icon mediaStatus">
							<div class="centerAligned fas fa-backward"></div>
						</div>
						<div gid="tape_status_previous_track" class="icon mediaStatus">
							<div class="centerAligned fas fa-fast-backward"></div>
						</div>

						<div gid="loudness" class="led ledOff" style="left:795px; top:490px; width:140px;">LOUD</div>
					</div>	<!-- "tape" -->

					<div id="cd_player" style="display:none;">

						<div class="dots" style="left:270px; top:48px; width:450px;">CD</div>

						<div gid="cd_track_time" class="dseg7" style="font-size:123px; left:100px; top:145px; width:420px;">-:--</div>

						<div gid="cd_status_pause" class="icon mediaStatus">
							<div class="centerAligned fas fa-pause"></div>
						</div>
						<div gid="cd_status_play" class="icon mediaStatus">
							<div class="centerAligned fas fa-play"></div>
						</div>
						<div gid="cd_status_fast_forward" class="icon mediaStatus">
							<div class="centerAligned fas fa-forward"></div>
						</div>
						<div gid="cd_status_rewind" class="icon mediaStatus">
							<div class="centerAligned fas fa-backward"></div>
						</div>
						<div gid="cd_status_searching" class="icon mediaStatus">
							<div class="centerAligned fas fa-compact-disc"></div>
						</div>
						<div gid="cd_status_loading" class="icon mediaStatus">
							<div class="centerAligned fas fa-sign-in-alt"></div>
						</div>
						<div gid="cd_status_eject" class="icon mediaStatus">
							<div class="centerAligned fas fa-eject"></div>
						</div>

						<div id="cd_total_time_tag" class="tag" style="display:none; top:320px; width:180px;">Total</div>
						<div id="cd_total_time" class="dots" style="display:none; left:180px; top:315px; width:200px; text-align:right;">--:--</div>

						<div class="tag" style="left:365px; top:327px; width:200px;">Track</div>
						<div gid="cd_current_track" class="dots" style="left:565px; top:315px; width:90px; text-align:right;">--</div>
						<div class="tag" style="left:640px; top:320px; width:50px;">/</div>
						<div gid="cd_total_tracks" class="dots" style="left:685px; top:315px; width:90px; text-align:right;">--</div>

						<div id="cd_status_error" class="tag" style="display:none; left:60px; top:400px; width:700px; text-align:left;">DISC ERROR</div>

						<div gid="cd_random" class="iconSmall led ledOff" style="left:870px; top:300px;">
							<div class="centerAligned fas fa-random"></div>
						</div>

						<div gid="loudness" class="led ledOff" style="left:795px; top:490px; width:140px;">LOUD</div>
					</div>	<!-- "cd_player" -->

					<div id="cd_changer" style="display:none;">

						<div class="dots" style="left:270px; top:48px; width:450px;">CD Changer</div>

						<div gid="cd_changer_track_time" class="dseg7" style="font-size:123px; left:100px; top:145px; width:420px;">-:--</div>

						<div gid="cd_changer_status_pause" class="icon mediaStatus">
							<div class="centerAligned fas fa-pause"></div>
						</div>
						<div gid="cd_changer_status_play" class="icon mediaStatus">
							<div class="centerAligned fas fa-play"></div>
						</div>
						<div gid="cd_changer_status_searching" class="icon mediaStatus">
							<div class="centerAligned fas fa-compact-disc"></div>
						</div>
						<div gid="cd_changer_status_fast_forward" class="icon mediaStatus">
							<div class="centerAligned fas fa-forward"></div>
						</div>
						<div gid="cd_changer_status_rewind" class="icon mediaStatus">
							<div class="centerAligned fas fa-backward"></div>
						</div>
						<div id="cd_changer_status_loading" gid="cd_changer_status_loading" class="icon mediaStatus">
							<div class="centerAligned fas fa-sign-in-alt"></div>
						</div>
						<div gid="cd_changer_status_eject" class="icon mediaStatus">
							<div class="centerAligned fas fa-eject"></div>
						</div>

						<div id="cd_changer_disc_not_present" class="tag" style="display:none; top:320px; width:205px;">No</div>
						<div class="tag" style="left:65px; top:330px; width:235px;">CD</div>
						<div id="cd_changer_selected_disc" class="dots" style="display:none; left:300px; top:315px; width:90px; text-align:right;">-</div>
						<div id="cd_changer_current_disc" gid="cd_changer_current_disc" class="dots" style="left:300px; top:315px; width:90px; text-align:right;">-</div>
						<div class="tag" style="left:365px; top:330px; width:200px;">Track</div>
						<div gid="cd_changer_current_track" class="dots" style="left:565px; top:315px; width:90px; text-align:right;">--</div>
						<div class="tag" style="left:640px; top:320px; width:50px;">/</div>
						<div gid="cd_changer_total_tracks" class="dots" style="left:685px; top:315px; width:90px; text-align:right;">--</div>

						<div id="cd_changer_status_error" class="tag" style="display:none; top:320px; width:205px;">ERROR</div>

						<div gid="cd_changer_random" class="iconSmall led ledOff" style="left:870px; top:300px;">
							<div class="centerAligned fas fa-random"></div>
						</div>

						<!-- JSON data item "cd_changer_current_disc" is also parsed to apply class "ledActive" to the appropriate LED -->
						<div id="cd_changer_disc_1_present" class="led ledOff" style="left:30px; top:410px; width:135px;">1</div>
						<div id="cd_changer_disc_2_present" class="led ledOff" style="left:185px; top:410px; width:135px;">2</div>
						<div id="cd_changer_disc_3_present" class="led ledOff" style="left:340px; top:410px; width:135px;">3</div>
						<div id="cd_changer_disc_4_present" class="led ledOff" style="left:495px; top:410px; width:135px;">4</div>
						<div id="cd_changer_disc_5_present" class="led ledOff" style="left:650px; top:410px; width:135px;">5</div>
						<div id="cd_changer_disc_6_present" class="led ledOff" style="left:805px; top:410px; width:135px;">6</div>

						<div gid="loudness" class="led ledOff" style="left:795px; top:490px; width:140px;">LOUD</div>
					</div>	<!-- "cd_changer" -->
				</div>	<!-- Removable media -->
			</div>	<!-- Audio equipment -->

			<!-- Vehicle data -->
			<div style="display:none;">

				<!-- Fuel level, both as number and as linear gauge -->

				<div class="icon iconMedium fas fa-gas-pump" style="left:20px; top:55px;"></div>

				<div gid="fuel_level" class="dseg7" style="font-size:55px; left:80px; top:65px; width:240px;">--.-</div>
				<div gid="fuel_level_unit" class="tag" style="text-align:left; left:330px; top:80px; width:140px;">lt</div>

				<div style="position:absolute; left:20px; top:130px; width:350px; height:60px;">
					<div style="position:absolute; left:8px; width:332px; height:60px;">
						<svg style="width:334px;">
							<line style="stroke:#c40000; stroke-width:24;" x1="0" y1="20" x2="44" y2="20"></line> <!-- 10 litres -->
							<line style="stroke:#066a0c; stroke-width:24;" x1="45" y1="20" x2="333" y2="20"></line>
						</svg>
					</div>
					<div gid="fuel_level_perc" class="gauge">
						<svg style="width:324px;">
							<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:14; stroke-opacity:0.8;" x1="0" y1="20" x2="324" y2="20"></line>
						</svg>
					</div>
					<div class="gaugeBoxDiv">
						<svg style="width:348px;">
							<rect x="5" y="5" width="340" height="30" class="gaugeBox"></rect>
							<line style="stroke-width:5;" x1="170" y1="8" x2="170" y2="32"></line>
							<line style="stroke-width:5;" x1="85" y1="8" x2="85" y2="32"></line>
							<line style="stroke-width:5;" x1="255" y1="8" x2="255" y2="32"></line>
						</svg>
					</div>
				</div>

				<!-- Engine coolant temperature, both as number (in degrees) and as linear gauge -->

				<div class="icon iconMedium fas fa-thermometer-half" style="left:580px; top:55px;"></div>

				<div gid="coolant_temp" class="dseg7" style="font-size:55px; left:630px; top:65px; width:240px;">--.-</div>
				<div gid="coolant_temp_unit" class="tag" style="text-align:left; left:890px; top:80px; width:60px;">&deg;C</div>

				<div style="position:absolute; left:590px; top:130px; width:350px; height:60px;">
					<div class="gaugeInnerBoxDiv">
						<svg style="width:334px;">
							<line style="stroke:#00588c; stroke-width:24;" x1="0" y1="20" x2="176" y2="20"></line>
							<line style="stroke:#066a0c; stroke-width:24;" x1="177" y1="20" x2="277" y2="20"></line>
							<line style="stroke:#c40000; stroke-width:24;" x1="278" y1="20" x2="333" y2="20"></line>
						</svg>
					</div>
					<div id="coolant_temp_perc" class="gauge">
						<svg style="width:324px;">
							<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:14; stroke-opacity:0.8;" x1="0" y1="20" x2="324" y2="20"></line>
						</svg>
					</div>
					<div class="gaugeBoxDiv">
						<svg style="width:348px;">
							<rect x="5" y="5" width="340" height="30" class="gaugeBox"></rect>
							<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:5;" x1="184" y1="8" x2="184" y2="32"></line> <!-- 70 degrees -->
							<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:5;" x1="285" y1="8" x2="285" y2="32"></line> <!-- 110 degrees -->
						</svg>
					</div>
				</div>

				<!-- "Pre-flight" checks -->

				<div id="pre_flight" style="display:none;">

					<!-- Oil level, both as number and as linear gauge -->

					<div class="icon iconMedium fas fa-oil-can" style="left:20px; top:220px;"></div>

					<div id="oil_level_raw" class="dseg7" style="font-size:50px; left:125px; top:225px; width:240px;">--</div>

					<div style="position:absolute; left:20px; top:280px; width:350px; height:60px;">
						<div id="oil_level_raw_perc" class="gauge">
							<svg style="width:324px;">
								<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:14;" x1="0" y1="20" x2="324" y2="20"></line>
							</svg>
						</div>
						<div class="gaugeBoxDiv">
							<svg style="width:348px;">
								<rect x="5" y="5" width="340" height="30" class="gaugeBox"></rect>
							</svg>
						</div>
					</div>

					<!-- Service interval, both as number (in km) and as linear gauge -->

					<div class="icon iconMedium" style="left:440px; top:210px;">...</div>
					<div class="icon iconMedium fas fa-tools" style="left:520px; top:210px;"></div>

					<div id="distance_to_service" class="dseg7" style="font-size:50px; left:630px; top:225px; width:230px;">---</div>
					<div gid="distance_unit" class="tag" style="text-align:left; left:870px; top:233px; width:80px;">km</div>

					<div style="position:absolute; left:590px; top:280px; width:350px; height:60px;">
						<div id="distance_to_service_perc" class="gauge">
							<svg style="width:324px;">
								<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:14;" x1="0" y1="20" x2="324" y2="20"></line>
							</svg>
						</div>
						<div class="gaugeBoxDiv">
							<svg style="width:348px;">
								<rect x="5" y="5" width="340" height="30" class="gaugeBox"></rect>
							</svg>
						</div>
					</div>

					<!-- Status LEDs -->
					<div id="hazard_lights" class="led fas md-hazard-lights" style="font-size:90px; line-height:2.6; top:365px; width:170px; height:180px;"></div>
					<div id="door_open_led" class="led fas fa-door-open" style="font-size:70px; line-height:2.6; left:80px; top:390px; width:200px; height:160px;"></div>
					<div id="diesel_glow_plugs" class="led ledOff fas fa-sun" style="font-size:70px; line-height:1.3; left:260px; top:440px; width:80px;"></div>
					<div id="lights" class="led ledOff fas fa-lightbulb" style="font-size:70px; line-height:1.3; left:370px; top:440px; width:80px;"></div>

					<!-- Contact key position -->
					<div class="icon iconSmall fas fa-key" style="left:20px; top:373px;"></div>

					<div id="contact_key_position" class="dots" style="padding-left:50px; line-height:2.7; left:90px; top:315px; width:300px; height:160px;">OFF</div>

					<!-- Dashboard illumination level -->
					<div class="icon iconSmall fas fa-tachometer-alt" style="left:710px; top:373px;"></div>
					<div class="icon iconSmall fas fa-sun" style="left:770px; top:370px;"></div>
					<div id="dashboard_programmed_brightness" class="dseg7" style="font-size:50px; left:810px; top:370px; width:130px;">--</div>

					<!-- VIN number (very small) -->
					<div id="vin" class="tag" style="font-size:30px; left:480px; top:477px; width:460px;"></div>

				</div>	<!-- "Pre-flight" checks -->

				<!-- Instrument cluster -->

				<div id="instruments" style="display:none;">

					<div id="left_indicator" class="iconSmall led ledOff" style="left:70px; top:400px;">
						<div class="centerAligned fas fa-angle-double-left"></div>
					</div>

					<div gid="fog_rear" class="iconSmallMaterialDesign led ledOff" style="left:220px; top:400px;transform: scaleX(-1);">
						<div class="centerAligned fas md-car-light-fog"></div>
					</div>

					<div gid="parking_light" class="iconSmallMaterialDesign led ledOff" style="left:300px; top:400px; color:rgb(119, 217, 64);">
						<div class="centerAligned fas md-car-parking-lights"></div>
					</div>

					<div gid="dipped_beam" class="iconSmallMaterialDesign led ledOff" style="left:300px; top:400px;">
						<div class="centerAligned fas md-car-light-dimmed"></div>
					</div>

					<div gid="high_beam" class="iconSmallMaterialDesign led ledOff" style="left:380px; top:400px;">
						<div class="centerAligned fas md-car-light-high"></div>
					</div>

					<div gid="fog_front" class="iconSmallMaterialDesign led ledOff" style="left:460px; top:400px;">
						<div class="centerAligned fas md-car-light-fog"></div>
					</div>

					<div id="right_indicator" class="iconSmall led ledOff" style="left:610px; top:400px;">
						<div class="centerAligned fas fa-angle-double-right"></div>
					</div>

					<div gid="vehicle_speed" class="dseg7" style="font-size:120px; left:10px; top:235px; width:300px;">0</div>
					<div gid="speed_unit" class="tag" style="left:310px; top:310px;">km/h</div>

					<div gid="engine_rpm" class="dseg7" style="font-size:120px; left:440px; top:235px; width:400px;">0</div>
					<div class="tag" style="left:840px; top:310px;">rpm</div>

					<div class="icon iconSmall" style="top:475px; width:90px;">...</div>
					<div class="icon iconSmall fas fa-car-side" style="left:55px; top:480px;"></div>
					<div id="odometer_1" class="dseg7" style="font-size:45px; left:120px; top:480px; width:280px;">--.-</div>
					<div gid="distance_unit" class="tag" style="font-size:30px; text-align:left; left:415px; top:490px;">km</div>

					<div id="chosen_gear" class="dseg7" style="font-size:45px; left:480px; top:480px; width:100px;">-</div>
					<div class="icon iconSmallMaterialDesign fas md-car-shift-pattern" style="left:575px; top:470px;"></div>


					<!-- Delivered power (estimation) -->
					<div id="delivered_power" class="dseg7" style="font-size:45px; left:630px; top:415px; width:200px;">--.-</div>
					<div class="tag" style="font-size:30px; left:840px; top:425px;">HP</div>

					<!-- Delivered torque (estimation) -->
					<div id="delivered_torque" class="dseg7" style="font-size:45px; left:630px; top:480px; width:200px;">--.-</div>
					<div class="tag" style="font-size:30px; left:840px; top:490px;">Nm</div>

				</div>	<!-- Instrument cluster -->
			</div>

			<!-- MFD main menu -->

			<div id="main_menu" class="tag menuScreen">
				<div class="menuTitleLine">Main menu<br /></div>
				<div id="main_menu_goto_satnav_button"
					on_click="satnavGotoMainMenu();"
					class="button">
					Navigation / Guidance
				</div>
				<div id="main_menu_goto_screen_configuration_button"
					class="button" goto_id="screen_configuration_menu">
					Configure display
				</div>
			</div>	<!-- "main_menu" -->

			<div id="screen_configuration_menu" class="tag menuScreen">
				<div class="menuTitleLine">Configure display<br /></div>
				<div class="button buttonSelected" goto_id="set_screen_brightness">Set color and brightness</div>
				<div class="button" goto_id="set_date_time">Set date and time</div>
				<div class="button" goto_id="set_language">Select a language</div>
				<div class="button" goto_id="set_units">Set format and units</div>
			</div>	<!-- "screen_configuration_menu" -->

			<div id="set_screen_brightness" class="tag menuScreen" style="top:100px;"
				on_goto="colorThemeSelectTickedButton();"
				on_esc="setBrightnessEscape();"
				on_exit="acceptingHeldValButton = false;">
				<div class="menuTitleLine">Set color and brightness<br /></div>

				<div style="position:absolute; font-size:40px; text-align:left;">
					<span id="set_dark_theme"
						LEFT_BUTTON="set_brightness_higher"
						RIGHT_BUTTON="set_brightness_lower"
						UP_BUTTON="set_brightness_validate_button"
						on_click="colorThemeTickSet();"
						class="tickBox button buttonSelected">
					</span> <span id="set_dark_theme_tag">Dark theme</span><br />
					<span id="set_light_theme"
						LEFT_BUTTON="set_brightness_higher"
						RIGHT_BUTTON="set_brightness_lower"
						DOWN_BUTTON="set_brightness_validate_button"
						on_click="colorThemeTickSet();"
						class="tickBox button">
					</span> <span id="set_light_theme_tag">Light theme</span><br />
					<small><i>&rarr; Use MOD-button to cycle color palette</i></small>
				</div>

				<div class="tag"
					style="top:82px; left:590px; width:310px; text-align:center; font-size:40px;">
					Brightness
				</div>

				<div style="position:absolute; left:600px; top:175px;">
					<div id="set_brightness_lower"
						class="button" style="width:60px"
						LEFT_BUTTON="set_dark_theme"
						RIGHT_BUTTON="set_brightness_higher"
						DOWN_BUTTON="set_brightness_validate_button"
						on_enter="acceptingHeldValButton = true;"
						on_exit="acceptingHeldValButton = false;"
						on_click="adjustDimLevel(headlightStatus, 'DOWN_BUTTON');"
						>&ndash;</div>
					<div id="set_brightness_higher"
						class="button" style="position:absolute; left:200px; top: 0px; width:60px;"
						LEFT_BUTTON="set_brightness_lower"
						RIGHT_BUTTON="set_dark_theme"
						DOWN_BUTTON="set_brightness_validate_button"
						on_enter="acceptingHeldValButton = true;"
						on_exit="acceptingHeldValButton = false;"
						on_click="adjustDimLevel(headlightStatus, 'UP_BUTTON');"
						>&plus;</div>
				</div>

				<div id="display_brightness_level"
					class="dseg7" style="left:600px; top:185px; width:290px; text-align:center;">
					14
				</div>

				<div id="display_reduced_brightness_level"
					class="dseg7" style="left:600px; top:180px; width:290px; text-align:center;">
					0
				</div>

				<div id="set_brightness_validate_button"
					class="button validateButton" style="top:330px;"
					UP_BUTTON="set_light_theme"
					DOWN_BUTTON="set_dark_theme"
					on_click="setBrightnessValidate();">
					Validate
				</div>

			</div>	<!-- "set_screen_brightness" -->

			<div id="set_date_time" class="tag menuScreen" style="top:50px; height:460px;">
				<div class="menuTitleLine">Set date and time<br /></div>
				<small>[Not implemented]</small>

				<div style="position:absolute; top:190px;">
					<div style="position:absolute; left:30px;">1</div>
					<div style="position:absolute; left:150px;">/</div>
					<div style="position:absolute; left:210px;">Jan</div>
					<div style="position:absolute; left:340px;">/</div>
					<div style="position:absolute; left:390px;">2000</div>
					<div style="position:absolute; left:650px;">0</div>
					<div style="position:absolute; left:750px;">:</div>
					<div style="position:absolute; left:800px;">00</div>
				</div>

				<div style="position:absolute; top:270px;">
					<div style="position:absolute;">
						<span id="set_date_time_increase_day"
							class="button buttonSelected"
							LEFT_BUTTON="set_date_time_decrease_minute"
							RIGHT_BUTTON="set_date_time_decrease_day"
							DOWN_BUTTON="set_date_time_validate_button"
							>&plus;</span>
						<span id="set_date_time_decrease_day"
							class="button"
							LEFT_BUTTON="set_date_time_increase_day"
							RIGHT_BUTTON="set_date_time_increase_month"
							DOWN_BUTTON="set_date_time_validate_button"
							>&ndash;</span>
					</div>
					<div style="position:absolute; left:200px;">
						<span id="set_date_time_increase_month"
							class="button"
							LEFT_BUTTON="set_date_time_decrease_day"
							RIGHT_BUTTON="set_date_time_decrease_month"
							DOWN_BUTTON="set_date_time_validate_button"
							>&plus;</span>
						<span id="set_date_time_decrease_month"
							class="button"
							LEFT_BUTTON="set_date_time_increase_month"
							RIGHT_BUTTON="set_date_time_increase_year"
							DOWN_BUTTON="set_date_time_validate_button"
							>&ndash;</span>
					</div>
					<div style="position:absolute; left:400px;">
						<span id="set_date_time_increase_year"
							class="button"
							LEFT_BUTTON="set_date_time_decrease_month"
							RIGHT_BUTTON="set_date_time_decrease_year"
							DOWN_BUTTON="set_date_time_validate_button"
							>&plus;</span>
						<span id="set_date_time_decrease_year"
							class="button"
							LEFT_BUTTON="set_date_time_increase_year"
							RIGHT_BUTTON="set_date_time_increase_hour"
							DOWN_BUTTON="set_date_time_validate_button"
							>&ndash;</span>
					</div>
					<div style="position:absolute; left:600px;">
						<span id="set_date_time_increase_hour"
							class="button"
							LEFT_BUTTON="set_date_time_decrease_year"
							RIGHT_BUTTON="set_date_time_decrease_hour"
							DOWN_BUTTON="set_date_time_validate_button"
							>&plus;</span>
						<span id="set_date_time_decrease_hour"
							class="button"
							LEFT_BUTTON="set_date_time_increase_hour"
							RIGHT_BUTTON="set_date_time_increase_minute"
							DOWN_BUTTON="set_date_time_validate_button"
							>&ndash;</span>
					</div>
					<div style="position:absolute; left:780px;">
						<span id="set_date_time_increase_minute"
							class="button"
							LEFT_BUTTON="set_date_time_decrease_hour"
							RIGHT_BUTTON="set_date_time_decrease_minute"
							DOWN_BUTTON="set_date_time_validate_button"
							>&plus;</span>
						<span id="set_date_time_decrease_minute"
							class="button"
							LEFT_BUTTON="set_date_time_increase_minute"
							RIGHT_BUTTON="set_date_time_increase_day"
							DOWN_BUTTON="set_date_time_validate_button"
							>&ndash;</span>
					</div>
				</div>

				<div id="set_date_time_validate_button"
					UP_BUTTON="set_date_time_increase_day"
					on_click="upMenu(); upMenu(); upMenu();"
					class="button validateButton"
					style="top:380px;">Validate</div>
			</div>	<!-- "set_date_time" -->

			<div id="set_language" class="tag menuScreen"
				on_esc="setLanguage(localStorage.mfdLanguage); upMenu();"
				on_goto="setLanguageSelectTickedButton();">
				<div class="menuTitleLine">Select a language<br /></div>

				<div style="font-size:37px; text-align:left;">
					<div style="position:absolute;">
						<span id="set_language_german"
							LEFT_BUTTON="set_language_dutch"
							RIGHT_BUTTON="set_language_spanish"
							UP_BUTTON="set_language_validate_button"
							on_click="setLanguageTickSet(); keyPressed('UP_BUTTON');"
							class="tickBox button">
						</span> Deutsch<br />
						<span id="set_language_english"
							LEFT_BUTTON="set_language_italian"
							RIGHT_BUTTON="set_language_french"
							DOWN_BUTTON="set_language_validate_button"
							on_click="setLanguageTickSet(); keyPressed('DOWN_BUTTON');"
							class="tickBox button buttonSelected">
						</span> English<br />
					</div>

					<div style="position:absolute; left:280px;">
						<span id="set_language_spanish"
							UP_BUTTON="set_language_french"
							LEFT_BUTTON="set_language_german"
							RIGHT_BUTTON="set_language_dutch"
							on_click="setLanguageTickSet(); keyPressed('LEFT_BUTTON'); keyPressed('UP_BUTTON');"
							class="tickBox button">
						</span> Espa&ntilde;ol<br />
						<span id="set_language_french"
							LEFT_BUTTON="set_language_english"
							RIGHT_BUTTON="set_language_italian"
							DOWN_BUTTON="set_language_spanish"
							on_click="setLanguageTickSet(); keyPressed('LEFT_BUTTON'); keyPressed('DOWN_BUTTON');"
							class="tickBox button">
					</span> Fran&ccedil;ais<br />
					</div>

					<div style="position:absolute; left:560px;">
						<span id="set_language_dutch"
							UP_BUTTON="set_language_italian"
							LEFT_BUTTON="set_language_spanish"
							RIGHT_BUTTON="set_language_german"
							on_click="setLanguageTickSet(); keyPressed('RIGHT_BUTTON'); keyPressed('UP_BUTTON');"
							class="tickBox button">
						</span> Nederlands<br />
						<span id="set_language_italian"
							LEFT_BUTTON="set_language_french"
							RIGHT_BUTTON="set_language_english"
							DOWN_BUTTON="set_language_dutch"
							on_click="setLanguageTickSet(); keyPressed('RIGHT_BUTTON'); keyPressed('DOWN_BUTTON');"
							class="tickBox button">
						</span> Italiano<br />
					</div>
				</div>

				<div id="set_language_validate_button"
					class="button validateButton"
					style="top:300px;"
					UP_BUTTON="set_language_english"
					DOWN_BUTTON="set_language_german"
					on_click="setLanguageValidate();">Validate
				</div>

			</div>	<!-- "set_language" -->

			<div id="set_units"
				on_goto="unitsSelectTickedButtons();"
				class="tag menuScreen">
				<div class="menuTitleLine">Set format and units<br /></div>

				<div style="font-size:40px; text-align:left;">
					<div id="set_distance_unit">
						<div style="position:absolute;">
							<span id="set_units_km_h"
								class="tickBox button"
								LEFT_BUTTON="set_units_24h"
								RIGHT_BUTTON="set_units_deg_celsius"
								UP_BUTTON="set_units_validate_button"
								on_click="toggleTick(); keyPressed('RIGHT_BUTTON');">
							</span> km/h - l/100<br />
							<span id="set_units_mph"
								class="tickBox button buttonSelected"
								LEFT_BUTTON="set_units_12h"
								RIGHT_BUTTON="set_units_deg_fahrenheit"
								DOWN_BUTTON="set_units_validate_button"
								on_click="toggleTick(); keyPressed('RIGHT_BUTTON');">
							</span> mph - mpg<br />
						</div>
					</div>

					<div id="set_temperature_unit">
						<div style="position:absolute; left:430px;">
							<span id="set_units_deg_celsius"
								UP_BUTTON="set_units_validate_button"
								LEFT_BUTTON="set_units_km_h"
								RIGHT_BUTTON="set_units_24h"
								on_click="toggleTick(); keyPressed('RIGHT_BUTTON');"
								class="tickBox button">
							</span> &deg; C<br />
							<span id="set_units_deg_fahrenheit"
								LEFT_BUTTON="set_units_mph"
								RIGHT_BUTTON="set_units_12h"
								DOWN_BUTTON="set_units_validate_button"
								on_click="toggleTick(); keyPressed('RIGHT_BUTTON');"
								class="tickBox button">
							</span> &deg; F<br />
						</div>
					</div>

					<div id="set_time_unit">
						<div style="position:absolute; left:680px;">
							<span id="set_units_24h"
								UP_BUTTON="set_units_validate_button"
								LEFT_BUTTON="set_units_deg_celsius"
								RIGHT_BUTTON="set_units_km_h"
								on_click="toggleTick(); keyPressed('RIGHT_BUTTON');"
								class="tickBox button">
							</span> 24H<br />
							<span id="set_units_12h"
								LEFT_BUTTON="set_units_deg_fahrenheit"
								RIGHT_BUTTON="set_units_mph"
								DOWN_BUTTON="set_units_validate_button"
								on_click="toggleTick(); keyPressed('RIGHT_BUTTON');"
								class="tickBox button">
							</span> 12H<br />
						</div>
					</div>
				</div>

				<div id="set_units_validate_button"
					UP_BUTTON="set_units_mph"
					DOWN_BUTTON="set_units_km_h"
					on_click="unitsValidate();"
					class="button validateButton"
					style="top:300px;">Validate
				</div>

			</div>	<!-- "set_units" -->

			<!-- Navigation -->

			<div id="sat_nav" style="display:none;">

				<!--
					Top line shows navigation data we always want to see, whether in guidance mode,
					setting a destination, or just viewing current location
				-->

				<!-- Status LEDs -->

				<div id="satnav_disc_recognized" class="iconSmall led ledOff" style="left:370px; top:30px;">
					<div class="centerAligned fas fa-compact-disc"></div>
				</div>

				<div id="satnav_gps_scanning" class="iconSmall led ledOff" style="left:465px; top:30px;">
					<div class="centerAligned fas fa-search-location"></div>
				</div>

				<div gid="satnav_gps_fix" class="iconSmall led ledOff" style="left:560px; top:30px;">
					<div class="centerAligned fas fa-satellite-dish"></div>
				</div>

				<div id="satnav_gps_fix_lost" class="iconSmall led ledOff" style="display:block; left:655px; top:30px;">
					<div class="centerAligned fas fa-smog"></div>
				</div>

				<!-- Current heading, shown as compass needle -->
				<div id="satnav_curr_heading_compass_needle" class="satNavCompassNeedle" style="transform:rotate(0deg);">
					<svg>
						<path class="satNavInstructionIcon satNavInstructionDisabledIcon" style="stroke-width:8;" d="M40 15 l30 100 l-60 0 Z" transform="scale(0.6)"></path>
					</svg>
				</div>

				<!-- Small "N" at end of arrow, indicating "North" -->
				<div id="satnav_curr_heading_compass_tag" class="satNavCompassNeedle" style="transform:translate(0px,-42px);">
					<svg>
						<text class="satNavInstructionIconText satNavInstructionDisabledIconText" x="24" y="36" font-size="20px">N</text>
					</svg>
				</div>

				<!-- Current GPS speed -->
				<div id="satnav_gps_speed" class="dots" style="left:75px; top:34px; width:160px; text-align:right;">--</div>
				<div class="tag" style="left:245px; top:30px; font-size:25px;">GPS</div>
				<div gid="speed_unit" class="tag" style="left:245px; top:57px; font-size:25px;">km/h</div>

				<!-- Current location -->
				<div id="satnav_current_location"
					on_enter="$('#satnav_curr_street_small').hide(); satnavCutoffBottomLines($('#satnav_current_location [gid=\'satnav_curr_street_shown\']'));"
					on_exit="$('#satnav_curr_street_small').show();"
					style="display:none;">

					<!-- Current street and city -->
					<div class="icon iconBorder" style="left:20px; top:150px; width:880px; height:250px; padding:10px;">
						<div gid="satnav_curr_street_shown" class="centerAligned" style="font-size:60px;"></div>
					</div>

				</div>	<!-- "satnav_current_location" -->

				<!-- Sat nav disclaimer screen -->
				<div id="satnav_disclaimer" style="display:none;">
					<div class="tag" style="font-size:35px; left:20px; top:150px; width:920px; height:400px; text-align:center;">
						<div id="satnav_disclaimer_text">
							Navigation is an electronic help system. It<br />
							cannot replace the driver's decisions. All<br />
							guidance instructions must be carefully<br />
							checked by the user.<br />
						</div>
						<div id="satnav_disclaimer_validate_button"
							on_click="satnavDisclaimerAcceptClicked();"
							class="button validateButton buttonSelected" style="left:630px; top:300px;">
							Validate
						</div>
					</div>
				</div>	<!-- "satnav_disclaimer" -->

				<!-- Sat nav main menu -->

				<div id="satnav_main_menu" class="tag menuScreen"
					on_enter="highlightFirstLine('satnav_choice_list');">

					<div class="menuTitleLine">Navigation / Guidance<br /></div>

					<!-- TODO - trigger the screen change via "go_to_screen"? -->
					<div class="button buttonSelected" on_click="satnavGotoEnterCity();">Enter new destination</div>

					<!--
						This button is disabled until a
						  {"satnav_to_mfd_response":"service","satnav_to_mfd_list_size":"<x>"}
						packet is received
					-->
					<div id="satnav_main_menu_select_a_service_button"
						goto_id="satnav_show_last_destination"
						class="button buttonDisabled">Select a service</div>

					<div class="button"
						on_click="satnavSetDirectoryAddressScreenMode('SELECT');"
						goto_id="satnav_select_from_memory_menu">Select destination from memory</div>

					<div class="button"
						goto_id="satnav_navigation_options_menu">Navigation options</div>

				</div>	<!-- "satnav_main_menu" -->

				<!-- Sat nav select from memory menu -->
				<div id="satnav_select_from_memory_menu" class="tag menuScreen">
					<div class="menuTitleLine">Select from memory<br /></div>

					<div class="button buttonSelected"
						on_click="satnavGotoListScreenPersonalAddressList();">
						Personal directory</div>

					<div class="button"
						on_click="satnavGotoListScreenProfessionalAddressList();">
						Professional directory</div>
				</div>	<!-- "satnav_select_from_memory_menu" -->

				<!-- Sat nav navigation options menu -->
				<div id="satnav_navigation_options_menu" class="tag menuScreen">
					<div class="menuTitleLine">Navigation options<br /></div>

					<div class="button buttonSelected"
						on_click="satnavSetDirectoryAddressScreenMode('MANAGE');"
						goto_id="satnav_directory_management_menu">Directory management</div>

					<div class="button buttonDisabled" gid="satnav_volume_menu_button" goto_id="satnav_vocal_synthesis_level">Vocal synthesis volume</div>

					<div class="button" on_click="showPopup('satnav_delete_directory_data_popup');">Delete directories</div>

					<div class="button" on_click="satnavStopOrResumeGuidance();">Resume guidance</div>
				</div>	<!-- "satnav_navigation_options_menu" -->

				<!-- Sat nav directory management menu -->
				<div id="satnav_directory_management_menu" class="tag menuScreen">
					<div class="menuTitleLine">Directory management<br /></div>

					<!-- TODO - trigger the screen change via "go_to_screen"? -->
					<div class="button buttonSelected" on_click="satnavGotoListScreenPersonalAddressList();">Personal directory</div>

					<!-- TODO - trigger the screen change via "go_to_screen"? -->
					<div class="button" on_click="satnavGotoListScreenProfessionalAddressList();">Professional directory</div>
				</div>	<!-- "satnav_directory_management_menu" -->

				<!-- Sat nav guidance tools (context) menu -->
				<div id="satnav_guidance_tools_menu" class="tag menuScreen">
					<div class="menuTitleLine">Guidance tools<br /></div>

					<div class="button buttonSelected" goto_id="satnav_guidance_preference_menu">Guidance criteria</div>

					<div class="button" goto_id="satnav_show_programmed_destination">Programmed destination</div>

					<div class="button buttonDisabled" gid="satnav_volume_menu_button" goto_id="satnav_vocal_synthesis_level">Vocal synthesis volume</div>

					<div id="satnav_tools_menu_stop_guidance_button" class="button"
						on_click="satnavStopOrResumeGuidance();">Stop guidance</div>
				</div>	<!-- "satnav_guidance_tools_menu" -->

				<!-- Sat nav guidance criteria menu -->
				<div id="satnav_guidance_preference_menu"
					on_goto="satnavGuidancePreferenceSelectTickedButton();"
					on_exit="showDestinationNotAccessiblePopupIfApplicable();"
					on_esc="satnavGuidancePreferenceEscape();"
					style="position:absolute; display:none; left:20px; top:130px; width:920px; height:410px;">

					<!-- As long we don't know the "satnav_guidance_preference" value, "Fastest route" will be ticked -->
					<span id="satnav_guidance_preference_fastest" class="tickBox button buttonSelected"
						on_click="toggleTick(); keyPressed('UP_BUTTON');"><b>&#10004;</b></span> <span class="tickBoxLabel">Fastest route<br /></span>

					<span id="satnav_guidance_preference_shortest" class="tickBox button"
						on_click="toggleTick(); keyPressed('UP_BUTTON'); keyPressed('UP_BUTTON');"></span> <span class="tickBoxLabel">Shortest route<br /></span>

					<span id="satnav_guidance_preference_avoid_highway" class="tickBox button"
						on_click="toggleTick(); keyPressed('DOWN_BUTTON'); keyPressed('DOWN_BUTTON');"></span> <span class="tickBoxLabel">Avoid highway route<br /></span>

					<span id="satnav_guidance_preference_compromise" class="tickBox button"
						on_click="toggleTick(); keyPressed('DOWN_BUTTON');"></span> <span class="tickBoxLabel">Fast/short compromise route<br /></span>

					<div id="satnav_guidance_preference_menu_validate_button"
						class="button validateButton"
						on_click="satnavGuidancePreferenceValidate();"
						style="top:300px;">
						Validate
					</div>
				</div>	<!-- "satnav_guidance_preference_menu" -->

				<!-- Sat nav vocal synthesis level screen -->
				<div id="satnav_vocal_synthesis_level" on_esc="satnavEscapeVocalSynthesisLevel();" class="tag menuScreen">

					<div class="menuTitleLine">Vocal synthesis level<br /></div>

					<div class="tag" style="left:10px; top:148px; width:300px;">Level</div>

					<!-- TODO - also show the volume as a linear gauge -->
					<div gid="volume" class="dseg7" style="font-size:100px; left:270px; top:115px; width:200px;">-</div>

					<div
						class="button buttonSelected validateButton"
						on_click="satnavValidateVocalSynthesisLevel();"
						style="top:300px;">Validate</div>
				</div>	<!-- "satnav_vocal_synthesis_level" -->

				<!-- Enter destination -->

				<div id="satnav_enter_destination" style="display:none;">

					<!-- Either entering letter by letter... -->

					<div id="satnav_enter_characters" style="display:none;">

						<!-- The city or street name that has been entered thus far, letter by letter -->
						<div id="satnav_entered_string" class="dots satNavEnterDestination"></div>

						<div class="tag" style="left:20px; top:240px; width:550px; text-align:left;">Choose next letter</div>

						<!-- Show spinning disc as long as no data has come in -->
						<div id="satnav_to_mfd_show_characters_spinning_disc"
							style="display:none; position:absolute; left:400px; top:300px; transform:scaleX(2);">
							<div class="fas fa-compact-disc" style="font-size:100px;"></div>
						</div>

						<!--
							On the original MFD, there is only room for 24 or 25 characters. If there are more
							characters to choose from, they spill over to the next line
						-->
						<div id="satnav_to_mfd_show_characters_line_1"
							UP_BUTTON="satnav_to_mfd_show_characters_line_1"
							DOWN_BUTTON="satnav_to_mfd_show_characters_line_2"
							on_left_button="highlightPreviousLetter();"
							on_right_button="highlightNextLetter();"
							on_down_button="highlightIndexes['satnav_to_mfd_show_characters_line_2'] = highlightIndexes['satnav_to_mfd_show_characters_line_1']; navigateButtons('DOWN_BUTTON');"
							on_enter="highlightLetter();"
							on_exit="unhighlightLetter();"
							class="dots buttonSelected satNavShowCharacters"
							style="top:290px;"></div>

						<div id="satnav_to_mfd_show_characters_line_2"
							UP_BUTTON="satnav_to_mfd_show_characters_line_1"
							DOWN_BUTTON="satnav_enter_characters_validate_button"
							on_left_button="highlightPreviousLetter();"
							on_right_button="highlightNextLetter();"
							on_up_button="highlightIndexes['satnav_to_mfd_show_characters_line_1'] = highlightIndexes['satnav_to_mfd_show_characters_line_2']; navigateButtons('UP_BUTTON');"
							on_enter="highlightLetter();"
							on_exit="unhighlightLetter();"
							class="dots satNavShowCharacters"
							style="top:360px;"></div>

						<div button_orientation="horizontal" class="buttonBar">

							<div id="satnav_enter_characters_validate_button"
								UP_BUTTON="satnav_to_mfd_show_characters_line_2"
								on_click="satnavGotoEnterStreetOrNumber();"
								class="button buttonDisabled validateButton"
								style="width:180px;">
								Validate
							</div>

							<div id="satnav_enter_characters_correction_button"
								UP_BUTTON="satnav_to_mfd_show_characters_line_2"
								class="icon button buttonDisabled correctionButton">
								Correction
							</div>

							<div id="satnav_enter_characters_list_button"
								UP_BUTTON="satnav_to_mfd_show_characters_line_2"
								on_click="satnavGotoListScreenEmpty();"
								class="icon button buttonDisabled"
								style="left:470px; width:240px; height:40px;">
								<span id="satnav_to_mfd_list_tag">List</span> <span gid="satnav_to_mfd_list_size"></span>
							</div>

							<div id="satnav_enter_characters_change_or_city_center_button"
								UP_BUTTON="satnav_to_mfd_show_characters_line_2"
								on_click="satnavEnterCharactersChangeOrCityCenterButtonPress();"
								class="icon button"
								style="left:740px; width:170px; height:40px;">
								Change
							</div>
						</div>

						<div id="satnav_enter_city_characters"
							on_enter="satnavEnterCityCharactersScreen();"
							style="display:none;">

							<div class="tag satNavEnterDestinationTag">Enter city</div>

							<!--
								The following div will be hidden as soon as the user starts choosing letters, which
								will then appear one by one in "satnav_entered_string"
							-->
							<div id="satnav_current_destination_city" class="dots satNavEnterDestination"></div>
						</div>

						<div id="satnav_enter_street_characters"
							on_enter="satnavEnterStreetCharactersScreen();"
							on_esc="satnavConfirmCityMode(); upMenu();"
							style="display:none;">

							<div class="tag satNavEnterDestinationTag">Enter street</div>

							<!--
								The following div will be hidden as soon as the user starts choosing letters, which
								will then appear one by one in "satnav_entered_string"
							-->
							<div id="satnav_current_destination_street" class="dots satNavEnterDestination"></div>
						</div>

					</div>	<!-- "satnav_enter_characters" -->

					<!-- ...or choosing entry from list... -->

					<div id="satnav_choose_from_list"
						on_enter="if (highlightIndexes['satnav_choice_list'] === 0) $('#satnav_choice_list').scrollTop(0);"
						on_esc="if ($('#satnav_tag_street_list').is(':visible')) ignoringIrEscCommand = true; upMenu();"
						style="display:none;">

						<!-- What is being entered? (city, street) -->
						<div id="satnav_tag_city_list" class="tag satNavEnterDestinationTag">Choose city</div>
						<div id="satnav_tag_street_list" class="tag satNavEnterDestinationTag" style="display:none;">Choose street</div>
						<div id="satnav_tag_service_list" class="tag satNavEnterDestinationTag" style="display:none;">Service list</div>
						<div id="satnav_tag_personal_address_list" class="tag satNavEnterDestinationTag" style="display:none;">Personal address list</div>
						<div id="satnav_tag_professional_address_list" class="tag satNavEnterDestinationTag" style="display:none;">Professional address list</div>

						<div gid="satnav_to_mfd_list_size" class="dots" style="left:730px; top:107px; width:218px; text-align:right;">-</div>

						<!-- TODO - id="satnav_software_modules_list": show this? -->

						<div id="satnav_choice_list" class="buttonSelected"
							on_up_button="highlightPreviousLine();"
							on_down_button="highlightNextLine();"
							on_click="satnavListItemClicked();"
							style="position:absolute; font-size:33px; line-height:1.3;
								left:20px; top:180px; width:923px; height:346px;
								overflow:hidden; white-space:nowrap;
								background:none; color:#dfe7f2; color:var(--main-color); border-style:none;">
						</div>

						<!-- Show spinning disk while text area is still empty, i.e. data is being retrieved -->
						<div id="satnav_choose_from_list_spinning_disc"
							style="display:none; position:absolute; left:400px; top:300px; transform:scaleX(2);">
							<div class="fas fa-compact-disc" style="font-size:100px;"></div>
						</div>

					</div>	<!-- "satnav_choose_from_list" -->

					<!-- ...or choosing house number from range -->
					<div id="satnav_enter_house_number" style="display:none;">

						<!-- What is being entered? (house number) -->
						<div class="tag satNavEnterDestinationTag">Enter number</div>

						<!-- The number that has been entered thus far -->
						<div id="satnav_entered_number" class="dots" style="left:20px; top:180px; width:930px;"></div>

						<!--
							The following div will disappear as soon as the user starts choosing numbers, which will
							then appear one by one in "satnav_entered_number"
						-->
						<div id="satnav_current_destination_house_number"
							on_enter="satnavConfirmHouseNumber();"
							on_esc="satnavConfirmStreetMode(); upMenu();"
							class="dots" style="left:25px; top:180px; width:930px;"></div>

						<div id="satnav_house_number_range" class="dots" style="left:25px; top:230px; width:930px; text-align:right;"></div>

						<div id="satnav_house_number_show_characters"
							on_click="satnavEnterDigit();"
							UP_BUTTON="satnav_house_number_show_characters"
							DOWN_BUTTON="satnav_enter_house_number_validate_button"
							on_left_button="highlightPreviousLetter();"
							on_right_button="highlightNextLetter();"
							on_enter="highlightFirstLetter();"
							on_exit="unhighlightLetter();"
							class="dots buttonSelected"
							style="left:25px; top:310px; width:830px; line-height:1.5; display:inline-block; letter-spacing:25px;
								background:none; color:#dfe7f2; color:var(--main-color); border-style:none;">1234567890</div>

						<div button_orientation="horizontal" class="buttonBar">
							<div id="satnav_enter_house_number_validate_button"
								UP_BUTTON="satnav_house_number_show_characters"
								on_click="satnavCheckEnteredHouseNumber();"
								class="icon button validateButton">
								Validate
							</div>
							<div id="satnav_enter_house_number_change_button"
								UP_BUTTON="satnav_house_number_show_characters"
								on_click="satnavHouseNumberEntryMode();"
								class="icon button buttonDisabled correctionButton" style="left:290px;">
								Change
							</div>
						</div>

					</div>	<!-- "satnav_enter_house_number" -->

				</div>	<!-- "satnav_enter_destination" -->

				<!-- Showing an address (entry) -->
				<div id="satnav_show_address" style="display:none;">

					<div id="satnav_show_personal_address"
						on_esc="mfdToSatnavRequest = 'personal_address_list';"
						style="display:none;">

						<div id="satnav_personal_address_entry" class="dots satNavAddressEntry"></div>

						<div class="tag satNavCityTag">City</div>
						<div id="satnav_personal_address_city" class="dots satNavShowAddressCity"></div>

						<div class="tag satNavStreetTag">Street</div>
						<div id="satnav_personal_address_street_shown" class="dots satNavShowAddressStreet"></div>

						<div class="tag satNavNumberTag">Number</div>
						<div id="satnav_personal_address_house_number_shown" class="dots satNavShowAddressNumber"></div>

						<div id="satnav_personal_address_validate_buttons" style="display:block">
							<div id="satnav_show_personal_address_validate_button"
								on_click="showPopup('satnav_guidance_preference_popup', 6000);"
								class="icon button buttonDisabled validateButton" style="left:25px; top:460px;">
								Validate
							</div>
						</div>

						<div id="satnav_personal_address_manage_buttons" style="display:none">
							<div button_orientation="horizontal" class="buttonBar">
								<div id="satnav_manage_personal_address_rename_button"
									goto_id="satnav_rename_entry_in_directory_entry"
									class="icon button validateButton">
									Rename
								</div>

								<div id="satnav_manage_personal_address_delete_button"
									on_click="satnavDirectoryEntry = $('#satnav_personal_address_entry').text(); showPopup('satnav_delete_item_popup', 30000);"
									class="icon button validateButton" style="left:290px;">
									Delete
								</div>
							</div>
						</div>
					</div>	<!-- "satnav_show_personal_address" -->

					<div id="satnav_show_professional_address"
						on_esc="mfdToSatnavRequest = 'professional_address_list';"
						style="display:none;">

						<div id="satnav_professional_address_entry" class="dots satNavAddressEntry"></div>

						<div class="tag satNavCityTag">City</div>
						<div id="satnav_professional_address_city" class="dots satNavShowAddressCity"></div>

						<div class="tag satNavStreetTag">Street</div>
						<div id="satnav_professional_address_street_shown" class="dots satNavShowAddressStreet"></div>

						<div class="tag satNavNumberTag">Number</div>
						<div id="satnav_professional_address_house_number_shown" class="dots satNavShowAddressNumber"></div>

						<div id="satnav_professional_address_validate_buttons" style="display:block">
							<div id="satnav_show_professional_address_validate_button"
								on_click="showPopup('satnav_guidance_preference_popup', 6000);"
								class="icon button buttonDisabled validateButton" style="left:25px; top:460px;">
								Validate
							</div>
						</div>

						<div id="satnav_professional_address_manage_buttons" style="display:none">
							<div button_orientation="horizontal" class="buttonBar">
								<div id="satnav_manage_professional_address_rename_button"
									goto_id="satnav_rename_entry_in_directory_entry"
									class="icon button validateButton">
									Rename
								</div>

								<div id="satnav_manage_professional_address_delete_button"
									on_click="satnavDirectoryEntry = $('#satnav_professional_address_entry').text(); showPopup('satnav_delete_item_popup', 30000);"
									class="icon button validateButton" style="left:290px;">
									Delete
								</div>
							</div>
						</div>
					</div>	<!-- "satnav_show_professional_address" -->

					<div id="satnav_show_service_address"
						style="display:none;"
						on_esc="mfdToSatnavRequest = 'service'; upMenu();"
						on_enter="satnavEnterShowServiceAddress();">

						<div id="satnav_service_address_entry" class="dots satNavAddressEntry"></div>

						<div class="tag satNavCityTag">City</div>
						<div id="satnav_service_address_city" class="dots satNavShowAddressCity">
						</div>

						<div class="tag satNavStreetTag">Street</div>
						<div id="satnav_service_address_street" class="dots satNavShowAddressStreet">
						</div>

						<div id="satnav_service_address_distance_number" class="dots" style=" left:25px; top:376px; width:180px; text-align:right;">-</div>
						<div id="satnav_service_address_distance_unit" class="dots" style="font-size:30px; left:210px; top:400px;"></div>

						<!-- Show also the entry number and the total number of entries found -->
						<div id="satnav_service_address_entry_number" class="dots" style=" left:600px; top:376px; width:160px; text-align:right;">-</div>
						<div class="tag" style="left:736px; top:380px; width:50px;">/</div>
						<div id="satnav_to_mfd_list_size" class="dots" style=" left:800px; top:376px; width:160px; text-align:left;">-</div>

						<div button_orientation="horizontal" class="buttonBar">
							<div id="satnav_service_address_validate_button"
								on_click="showPopup('satnav_guidance_preference_popup', 6000);"
								class="icon button validateButton">
								Validate
							</div>
							<div id="satnav_service_address_previous_button"
								class="icon button validateButton" style="left:290px;">
								Previous
							</div>
							<div id="satnav_service_address_next_button"
								class="icon button buttonSelected validateButton" style="left:580px;">
								Next
							</div>
						</div>
					</div>	<!-- "satnav_show_service_address" -->

					<div id="satnav_show_current_destination"
						on_goto="selectFirstMenuItem('satnav_show_current_destination');"
						on_esc="currentMenu = 'main_menu'; satnavGotoMainMenu();"
						style="left:25px; top:110px; width:950px; display:none;">

						<div class="tag satNavShowAddress">Programmed destination</div>

						<div class="tag satNavCityTag">City</div>
						<div gid="satnav_current_destination_city" class="dots satNavShowAddressCity"></div>

						<div class="tag satNavStreetTag">Street</div>
						<div id="satnav_current_destination_street_shown" class="dots satNavShowAddressStreet"></div>

						<div class="tag satNavNumberTag">Number</div>
						<div id="satnav_current_destination_house_number_shown" class="dots satNavShowAddressNumber"></div>

						<div button_orientation="horizontal" class="buttonBar">
							<div
								on_click="showPopup('satnav_guidance_preference_popup', 6000);"
								class="icon button buttonSelected validateButton">
								Validate
							</div>
							<div
								on_click="satnavGotoEnterCity();"
								class="icon button correctionButton" style="left:290px;">
								Change
							</div>
							<div id="satnav_store_entry_in_directory"
								goto_id="satnav_archive_in_directory_entry"
								class="icon button correctionButton" style="left:550px; width:270px;">
								Store
							</div>
						</div>
					</div>	<!-- "satnav_show_current_destination" -->

					<div id="satnav_show_programmed_destination"
						on_enter="satnavClearLastDestination();"
						on_esc="upMenu(); upMenu();"
						style="display:none;">

						<div class="tag satNavShowAddress">Programmed destination</div>

						<div class="tag satNavCityTag">City</div>
						<div gid="satnav_last_destination_city" class="dots satNavShowAddressCity"></div>

						<div class="tag satNavStreetTag">Street</div>
						<div gid="satnav_last_destination_street_shown" class="dots satNavShowAddressStreet"></div>

						<div class="tag satNavNumberTag">Number</div>
						<div id="satnav_last_destination_house_number_shown" class="dots satNavShowAddressNumber"></div>

						<div button_orientation="horizontal" class="buttonBar">
							<div id="satnav_show_programmed_destination_validate_button"
								on_click="upMenu(); upMenu();"
								class="icon button buttonSelected validateButton">
								Validate
							</div>
							<div
								on_click="satnavChangeProgrammedDestination();"
								class="icon button correctionButton" style="left:290px;">
								Change
							</div>
						</div>
					</div>	<!-- "satnav_show_programmed_destination" -->

					<div id="satnav_show_last_destination"
						style="display:none;">

						<div class="tag satNavShowAddress">Select a service</div>

						<div class="tag satNavCityTag">City</div>
						<div gid="satnav_last_destination_city" class="dots satNavShowAddressCity"></div>

						<div class="tag satNavStreetTag">Street</div>
						<div gid="satnav_last_destination_street_shown" class="dots satNavShowAddressStreet"></div>

						<div button_orientation="horizontal" class="buttonBar">
							<div on_click="satnavGotoListScreenServiceList();"
								class="icon button buttonSelected validateButton">
								Validate
							</div>
							<div on_click="satnavEnterNewCityForService();"
								class="icon button correctionButton" style="left:290px;">
								Change
							</div>
							<div on_click="satnavGotoListScreenServiceList();"
								class="icon button" style="left:550px; width:360px; height:40px;">
								Current location
							</div>
						</div>
					</div>	<!-- "satnav_show_last_destination" -->
				</div>	<!-- "satnav_show_address" -->

				<div id="satnav_archive_in_directory"
					style="left:25px; top:110px; width:950px; display:none;">

					<div id="satnav_archive_in_directory_title" class="tag satNavShowAddress">Archive in directory</div>

					<div class="tag satNavEntryExistsTag">This entry already exists</div>

					<div class="tag satNavEntryNameTag" style="width:310px;">Name</div>
					<div id="satnav_archive_in_directory_entry"
						on_enter="satnavEnterArchiveInDirectoryScreen();"
						on_esc="satnavLastEnteredChar = null; upMenu();"
						class="dots satNavShowAddressCity" style="left:350px; top:210px;">----------------</div>

					<div id="satnav_archive_in_directory_characters_line_1"
						UP_BUTTON="satnav_archive_in_directory_characters_line_1"
						DOWN_BUTTON="satnav_archive_in_directory_characters_line_2"
						on_left_button="highlightPreviousLetter();"
						on_right_button="highlightNextLetter();"
						on_down_button="highlightIndexes['satnav_archive_in_directory_characters_line_2'] = highlightIndexes['satnav_archive_in_directory_characters_line_1']; navigateButtons('DOWN_BUTTON');"
						on_enter="highlightLetter();"
						on_exit="unhighlightLetter();"
						class="dots buttonSelected satNavShowCharacters"
						style="top:310px;">ABCDEFGHIJKLMNOPQRSTUVWX</div>

					<div id="satnav_archive_in_directory_characters_line_2"
						UP_BUTTON="satnav_archive_in_directory_characters_line_1"
						DOWN_BUTTON="satnav_archive_in_directory_correction_button"
						on_left_button="highlightPreviousLetter();"
						on_right_button="highlightNextLetter();"
						on_up_button="highlightIndexes['satnav_archive_in_directory_characters_line_1'] = highlightIndexes['satnav_archive_in_directory_characters_line_2']; navigateButtons('UP_BUTTON');"
						on_enter="highlightLetter();"
						on_exit="unhighlightLetter();"
						class="dots satNavShowCharacters"
						style="top:380px;">YZ0123456789_</div>

					<div button_orientation="horizontal" class="buttonBar">

						<div id="satnav_archive_in_directory_correction_button"
							UP_BUTTON="satnav_archive_in_directory_characters_line_2"
							on_up_button="satnavDirectoryEntryMoveUpFromCorrectionButton('satnav_archive_in_directory');"
							class="icon button"
							style="width:240px; height:40px;">
							Correction
						</div>

						<div id="satnav_archive_in_directory_personal_button"
							UP_BUTTON="satnav_archive_in_directory_characters_line_2"
							on_click="satnavArchiveInDirectoryCreatePersonalEntry();"
							class="icon button" style="left:270px; width:260px; height:40px;">
							Personal dir
						</div>

						<div id="satnav_archive_in_directory_professional_button"
							UP_BUTTON="satnav_archive_in_directory_characters_line_2"
							on_click="satnavArchiveInDirectoryCreateProfessionalEntry();"
							class="icon button" style="left:560px; width:350px; height:40px;">
							Professional dir
						</div>
					</div>
				</div>	<!-- "satnav_archive_in_directory" -->

				<div id="satnav_rename_entry_in_directory"
					style="left:25px; top:110px; width:950px; display:none;">

					<div id="satnav_rename_entry_in_directory_title" class="tag satNavShowAddress">Rename entry</div>

					<div class="tag satNavEntryNameTag" style="width:310px;">Name</div>

					<div class="tag satNavEntryExistsTag">This entry already exists</div>

					<!-- on_esc: this goes back to the "List" screen -->
					<div id="satnav_rename_entry_in_directory_entry"
						on_enter="satnavEnterRenameDirectoryEntryScreen();"
						on_esc="satnavLastEnteredChar = null; upMenu();"
						class="dots satNavShowAddressCity" style="left:350px; top:210px;">----------------</div>

					<div id="satnav_rename_entry_in_directory_characters_line_1"
						UP_BUTTON="satnav_rename_entry_in_directory_characters_line_1"
						DOWN_BUTTON="satnav_rename_entry_in_directory_characters_line_2"
						on_left_button="highlightPreviousLetter();"
						on_right_button="highlightNextLetter();"
						on_down_button="highlightIndexes['satnav_rename_entry_in_directory_characters_line_2'] = highlightIndexes['satnav_rename_entry_in_directory_characters_line_1']; navigateButtons('DOWN_BUTTON');"
						on_enter="highlightLetter();"
						on_exit="unhighlightLetter();"
						class="dots buttonSelected satNavShowCharacters"
						style="top:310px;">ABCDEFGHIJKLMNOPQRSTUVWX</div>

					<div id="satnav_rename_entry_in_directory_characters_line_2"
						UP_BUTTON="satnav_rename_entry_in_directory_characters_line_1"
						DOWN_BUTTON="satnav_rename_entry_in_directory_validate_button"
						on_left_button="highlightPreviousLetter();"
						on_right_button="highlightNextLetter();"
						on_up_button="highlightIndexes['satnav_rename_entry_in_directory_characters_line_1'] = highlightIndexes['satnav_rename_entry_in_directory_characters_line_2']; navigateButtons('UP_BUTTON');"
						on_enter="highlightLetter();"
						on_exit="unhighlightLetter();"
						class="dots satNavShowCharacters"
						style="top:380px;">YZ0123456789</div>

					<div button_orientation="horizontal" class="buttonBar">

						<!-- on_click: goes all the way back to the "Directory management" menu -->
						<div id="satnav_rename_entry_in_directory_validate_button"
							UP_BUTTON="satnav_rename_entry_in_directory_characters_line_2"
							on_click="satnavDirectoryEntryRenameValidateButton();"
							class="icon button validateButton">
							Validate
						</div>

						<div id="satnav_rename_entry_in_directory_correction_button"
							UP_BUTTON="satnav_rename_entry_in_directory_characters_line_2"
							on_up_button="satnavDirectoryEntryMoveUpFromCorrectionButton('satnav_rename_entry_in_directory');"
							class="icon button" style="left:290px; width:260px; height:40px;">
							Correction
						</div>
					</div>
				</div>	<!-- "satnav_rename_entry_in_directory" -->

				<!-- Sat nav guidance -->

				<div id="satnav_guidance"
					on_enter="satnavOnEnterGuidanceScreen();"
					on_exit="gotoSmallScreen(localStorage.smallScreen);"
					style="display:none;">

					<!-- Distance to destination -->

					<div class="icon iconSmall" style="left:610px; top:459px; width:80px;">...</div>
					<div class="icon iconSmall fas fa-sign-in-alt" style="left:660px; top:470px;"></div>

					<div id="satnav_distance_to_dest_via_road_visible">
						<div id="satnav_distance_to_dest_via_road_number"
							class="dots" style="left:350px; top:457px; width:200px; text-align:right;">-</div>
						<div id="satnav_distance_to_dest_via_road_unit"
							class="dots" style="font-size:30px; left:560px; top:480px;"></div>
					</div>

					<!-- This one is shown instead of the above, in case that is reported as "0" -->
					<div id="satnav_distance_to_dest_via_straight_line_visible" style="display:none">
						<div id="satnav_distance_to_dest_via_straight_line_number"
							class="dots" style="left:350px; top:457px; width:200px; text-align:right;">-</div>
						<div id="satnav_distance_to_dest_via_straight_line_unit"
							class="dots" style="font-size:30px; left:560px; top:480px;"></div>
					</div>

					<!-- Heading to destination -->

					<div style="position:absolute; left:780px; top:400px; width:200px; height:150px;">

						<!--
							Rotating round an abstract transform-origin like 'center' is better supported for a <div>
							than an <svg> element
						-->
						<div id="satnav_heading_to_dest_pointer"
							style="position:absolute; left:60px; top:5px; width:80px; height:120px; transform:rotate(0deg); transform-origin:center;">
							<svg>
								<path class="satNavInstructionIcon satNavInstructionDisabledIcon" style="stroke-width:8;" d="M40 15 l30 100 l-60 0 Z"></path>
							</svg>
						</div>

						<!-- Small "D" at end of arrow, indicating "Destination" -->
						<div id="satnav_heading_to_dest_tag"
							style="position:absolute; left:60px; top:5px; width:80px; height:120px; transform:translate(0px,-70px); transform-origin:center;">
							<svg>
								<text class="satNavInstructionIconText satNavInstructionDisabledIconText" x="40" y="60" font-size="30px">D</text>
							</svg>
						</div>

					</div>

					<!-- Current and next street name -->

					<div style="border:5px solid #dfe7f2; border:5px solid var(--main-color); border-radius:15px; position:absolute; overflow: hidden; left:350px; top:120px; width:570px; height:120px; padding:10px 10px">
						<div id="satnav_guidance_next_street" class="centerAligned" style="font-size:33px; white-space:normal;">
						</div>
					</div>
					<div style="border:5px solid #dfe7f2; border:5px solid var(--main-color); border-radius:15px; border-style:dotted; position:absolute; overflow: hidden; left:350px; top:280px; width:570px; height:120px; padding:10px 10px">
						<div id="satnav_guidance_curr_street" class="centerAligned" style="font-size:33px;">
						</div>
					</div>

					<div id="satnav_no_audio_icon" class="icon iconSmall led ledOn fas fa-volume-mute" style="display:none; left:20px; top:40px; height:45px; width:60px;"></div>

					<div id="satnav_audio" class="iconSmall led ledOff" style="left:750px; top:30px;">
						<div class="centerAligned fas fa-volume-up"></div>
					</div>

					<!-- Area with guidance icon(s) -->

					<div style="position:absolute; top:120px; width:300px; height:390px;">

						<!-- "Turn at" indication -->
						<div id="satnav_turn_at_indication" style="display:block;">
							<div id="satnav_turn_at_number" class="dots" style="top:285px; width:200px; text-align:right;">-</div>
							<div id="satnav_turn_at_unit" class="dots" style="font-size:30px; left:210px; top:310px;"></div>
						</div>

						<!-- "Not on map" icon -->
						<div id="satnav_not_on_map_icon" class="icon" style="display:none; position:absolute; left:50px; top:50px; width:200px; height:200px;">

							<!--
								Rotating round an abstract transform-origin like 'center' is better supported for a <div>
								than an <svg> element
							-->
							<div id="satnav_not_on_map_follow_heading" style="position:absolute; left:60px; top:20px; width:80px; height:120px; transform:rotate(0deg); transform-origin:center;">
								<svg>
									<path class="satNavInstructionIcon" style="stroke-width:8;" d="M40 15 l30 100 l-60 0 Z"></path>
								</svg>
							</div>
						</div>	<!-- "satnav_not_on_map_icon" -->

						<!-- "Follow road" icon(s) -->
						<div id="satnav_follow_road_icon" class="icon" style="display:none; position:absolute; left:50px; width:200px; height:300px;">

							<!-- The small "next instruction" icons on top -->

							<div id="satnav_follow_road_then_turn_right" class="icon" style="display:none; top:30px; width:200px; height:100px;">
								<div class="centerAligned fas fa-caret-square-right" style="font-size:80px; width:200px;"></div>
							</div>

							<div id="satnav_follow_road_then_turn_left" class="icon" style="display:none; top:30px; width:200px; height:100px;">
								<div class="centerAligned fas fa-caret-square-left" style="font-size:80px; width:200px;"></div>
							</div>

							<div id="satnav_follow_road_until_roundabout" class="icon" style="display:none; top:30px; width:200px; height:100px;">
								<div class="centerAligned fas fa-dot-circle" style="font-size:80px; width:200px;"></div>
							</div>

							<div id="satnav_follow_road_straight_ahead" class="icon" style="display:block; top:30px; width:200px; height:100px;">
								<div class="centerAligned fas fa-caret-square-up" style="font-size:80px; width:200px;"></div>
							</div>

							<div id="satnav_follow_road_retrieving_next_instruction" class="icon" style="display:none; top:30px; width:200px; height:100px;">
								<div class="centerAligned fas fa-hourglass-half" style="font-size:80px; width:200px;"></div>
							</div>

							<!-- The large "follow road" icon at the bottom -->
							<div style="display:block; position:absolute; top:100px; width:200px; height:200px;">
								<div class="centerAligned fas fa-road" style="font-size:150px; width:200px;"></div>
							</div>
						</div>	<!-- "satnav_follow_road_icon" -->

						<!-- "Turn around" icon -->
						<div id="satnav_turn_around_if_possible_icon" class="icon" style="display:none; position:absolute; left:50px; top:50px; width:200px; height:200px;">
							<svg style="width:200px; height:200px;">
								<line class="satNavInstructionIcon" x1="140" y1="80" x2="140" y2="140"></line>
								<line class="satNavInstructionIcon" x1="60" y1="80" x2="60" y2="140"></line>
								<line class="satNavInstructionIcon" x1="60" y1="150" x2="40" y2="120"></line>
								<line class="satNavInstructionIcon" x1="60" y1="150" x2="80" y2="120"></line>
							</svg>
							<div style="position:absolute; top:0px;">
								<svg style="width:200px; height:80px;">
									<circle class="satNavInstructionIcon" fill-opacity="0.0" cx="100" cy="80" r="40"></circle>
								</svg>
							</div>
						</div>	<!-- "satnav_turn_around_if_possible_icon" -->

						<!-- Fork icons -->

						<div id="satnav_fork_icon_keep_right" class="icon" style="display:none; position:absolute; left:50px; top:50px; width:200px; height:200px;">
							<svg style="width:200px;height:200px">
								<g id="fork_icon_keep_right">
									<path class="satNavInstructionIcon" d="M100 120 100 190 M100 120 140 80 M140 80 140 10 M140 10 120 30 M140 10 160 30"></path>
									<path class="satNavInstructionIcon satNavInstructionIconLeg" d="M100 120 60 80 M60 80 60 20"></path>
								</g>
							</svg>
						</div>	<!-- "satnav_fork_icon_keep_right" -->

						<div id="satnav_next_fork_icon_keep_right" class="icon" style="display:none; position:absolute; left:160px;">
							<div style="transform:scale(0.6); transform-origin:center;">
								<svg style="width:200px; height:200px;">
									<use xlink:href="#fork_icon_keep_right"></use>
								</svg>
							</div>
						</div>	<!-- "satnav_next_fork_icon_keep_right" -->

						<div id="satnav_fork_icon_keep_left" class="icon" style="display:none; position:absolute; left:50px; top:50px; width:200px; height:200px;">
							<svg style="width:200px;height:200px">
								<g id="fork_icon_keep_left">
									<path class="satNavInstructionIcon" d="M100 120 100 190 M100 120 60 80 M60 80 60 20 M60 10 40 30 M60 10 80 30"></path>
									<path class="satNavInstructionIcon satNavInstructionIconLeg" d="M100 120 140 80 M140 80 140 10"></path>
								</g>
							</svg>
						</div>	<!-- "satnav_fork_icon_keep_left" -->

						<div id="satnav_next_fork_icon_keep_left" class="icon" style="display:none; position:absolute; left:160px;">
							<div style="transform:scale(0.6); transform-origin:center;">
								<svg style="width:200px; height:200px;">
									<use xlink:href="#fork_icon_keep_left"></use>
								</svg>
							</div>
						</div>	<!-- "satnav_next_fork_icon_keep_left" -->

						<!-- Take exit icons -->

						<div id="satnav_fork_icon_take_right_exit" class="icon" style="display:none; position:absolute; left:50px; top:50px; width:200px; height:200px;">
							<svg style="width:200px;height:200px">
								<g id="fork_icon_take_right_exit">
									<path class="satNavInstructionIcon" d="M100 120 100 190 M100 120 140 80 M140 80 140 10 M140 10 120 30 M140 10 160 30"></path>
									<path class="satNavInstructionIcon satNavInstructionIconLeg" d="M100 120 100 20"></path>
								</g>
							</svg>
						</div>	<!-- "satnav_fork_icon_take_right_exit" -->

						<div id="satnav_next_fork_icon_take_right_exit" class="icon" style="display:none; position:absolute; left:160px;">
							<div style="transform:scale(0.6); transform-origin:center;">
								<svg style="width:200px; height:200px;">
									<use xlink:href="#fork_icon_take_right_exit"></use>
								</svg>
							</div>
						</div>	<!-- "satnav_next_fork_icon_take_right_exit" -->

						<div id="satnav_fork_icon_take_left_exit" class="icon" style="display:none; position:absolute; left:50px; top:50px; width:200px; height:200px;">
							<svg style="width:200px;height:200px">
								<g id="fork_icon_take_left_exit">
									<path class="satNavInstructionIcon" d="M100 120 100 190 M100 120 60 80 M60 80 60 10 M60 10 40 30 M60 10 80 30"></path>
									<path class="satNavInstructionIcon satNavInstructionIconLeg" d="M100 120 100 20"></path>
								</g>
							</svg>
						</div>	<!-- "satnav_fork_icon_take_left_exit" -->

						<div id="satnav_next_fork_icon_take_left_exit" class="icon" style="display:none; position:absolute; left:160px;">
							<div style="transform:scale(0.6); transform-origin:center;">
								<svg style="width:200px; height:200px;">
									<use xlink:href="#fork_icon_take_left_exit"></use>
								</svg>
							</div>
						</div>	<!-- "satnav_next_fork_icon_take_left_exit" -->

						<!-- Current turn icon -->
						<div id="satnav_curr_turn_icon" class="icon" style="display:none; position:absolute; width:300px; height:300px;">

							<!-- "To" direction as arrow -->
							<!-- Show in a separate div, for easy rotation around 'top center' -->
							<div id="satnav_curr_turn_icon_direction" style="z-index:-1; position:absolute; left:120px; top:150px; width:60px; height:110px; transform-origin:top center;">
								<svg style="width:60px; height:110px;">
									<g id="direction_arrow">
										<line class="satNavInstructionIcon" x1="30" y1="0" x2="30" y2="90"></line>
										<line class="satNavInstructionIcon" x1="30" y1="90" x2="10" y2="50"></line>
										<line class="satNavInstructionIcon" x1="30" y1="90" x2="50" y2="50"></line>

										<!-- For a roundabout: draw a circle over the arrow -->
										<!--<circle class="satNavInstructionIcon" cx="30" cy="0" r="30"/>-->
									</g>
								</svg>
							</div>

							<svg width="300" height="300">
								<!-- Show always the "from" direction -->
								<line class="satNavInstructionIcon" x1="150" y1="150" x2="150" y2="240"></line>

								<!-- Leg shape definition -->
								<defs>
									<g id="leg">
										<line class="satNavInstructionIcon satNavInstructionIconLeg" x1="150" y1="150" x2="150" y2="240"></line>
									</g>
								</defs>

								<!-- All the legs to the junction or roundabout -->
								<use id="satnav_curr_turn_icon_leg_22_5" xlink:href="#leg" transform="rotate(22.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_45_0" xlink:href="#leg" transform="rotate(45.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_67_5" xlink:href="#leg" transform="rotate(67.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_90_0" xlink:href="#leg" transform="rotate(90.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_112_5" xlink:href="#leg" transform="rotate(112.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_135_0" xlink:href="#leg" transform="rotate(135.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_157_5" xlink:href="#leg" transform="rotate(157.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_180_0" xlink:href="#leg" transform="rotate(180.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_202_5" xlink:href="#leg" transform="rotate(202.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_225_0" xlink:href="#leg" transform="rotate(225.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_247_5" xlink:href="#leg" transform="rotate(247.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_270_0" xlink:href="#leg" transform="rotate(270.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_292_5" xlink:href="#leg" transform="rotate(292.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_315_0" xlink:href="#leg" transform="rotate(315.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_leg_337_5" xlink:href="#leg" transform="rotate(337.5 150,150)" style="display:none;"></use>

								<!-- "No entry" icon definition -->
								<defs>
									<g id="no_entry">
										<path style="fill:#dfe7f2; fill:var(--main-color)" transform="translate(130 250) scale(0.08)" d="M256 8C119 8 8 119 8 256s111 248 248 248 248-111 248-248S393 8 256 8zM124 296c-6.6 0-12-5.4-12-12v-56c0-6.6 5.4-12 12-12h264c6.6 0 12 5.4 12 12v56c0 6.6-5.4 12-12 12H124z"></path>
									</g>
								</defs>

								<!-- "No entry" sign for each of the legs -->
								<use id="satnav_curr_turn_icon_no_entry_22_5" xlink:href="#no_entry" transform="rotate(22.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_45_0" xlink:href="#no_entry" transform="rotate(45.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_67_5" xlink:href="#no_entry" transform="rotate(67.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_90_0" xlink:href="#no_entry" transform="rotate(90.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_112_5" xlink:href="#no_entry" transform="rotate(112.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_135_0" xlink:href="#no_entry" transform="rotate(135.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_157_5" xlink:href="#no_entry" transform="rotate(157.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_180_0" xlink:href="#no_entry" transform="rotate(180.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_202_5" xlink:href="#no_entry" transform="rotate(202.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_225_0" xlink:href="#no_entry" transform="rotate(225.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_247_5" xlink:href="#no_entry" transform="rotate(247.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_270_0" xlink:href="#no_entry" transform="rotate(270.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_292_5" xlink:href="#no_entry" transform="rotate(292.5 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_315_0" xlink:href="#no_entry" transform="rotate(315.0 150,150)" style="display:none;"></use>
								<use id="satnav_curr_turn_icon_no_entry_337_5" xlink:href="#no_entry" transform="rotate(337.5 150,150)" style="display:none;"></use>
							</svg>

							<!-- Roundabout -->
							<div id="satnav_curr_turn_roundabout"
								style="display:none; position:absolute; top:0px; width:300px; height:300px;">
								<svg width="300" height="300">
									<!-- <circle class="satNavRoundabout" cx="150" cy="150" r="40"></circle> -->
									<circle class="satNavRoundabout" style="fill:rgb(67,82,105); fill:var(--disabled-element-color);" cx="150" cy="150" r="20"></circle>
									<path id="p" class="satNavRoundabout" d="M150 150m-40,0a40,40,0 1,0 80,0a 40,40 0 1,0 -80,0zM150 150m-20,0a20,20,0 0,1 40,0a 20,20 0 0,1 -40,0z"></path>
								</svg>
								<div style="position:absolute; top:0px; width:300px; height:300px;">
									<!-- Indicate the "to" direction also in the roundabout -->
									<svg width="300" height="300">
										<path id="satnav_curr_turn_icon_direction_on_roundabout" fill="none" style="stroke:#dfe7f2; stroke:var(--main-color);" stroke-width="8"></path>
									</svg>
								</div>
							</div>	<!-- "satnav_curr_turn_roundabout" -->
						</div>	<!-- "satnav_curr_turn_icon" -->

						<!-- Next turn icon -->
						<div id="satnav_next_turn_icon"
							class="icon"
							style="display:none; position:absolute; left:160px; width:180px; height:180px;">

							<!-- "To" direction as arrow -->
							<div id="satnav_next_turn_icon_direction" style="z-index:-1; position:absolute; left:60px; top:90px; width:60px; height:110px; transform-origin:top center;">
								<div style="transform:scale(0.6); transform-origin:inherit;">
									<svg style="width:60px; height:110px;">
										<use xlink:href="#direction_arrow"></use>
									</svg>
								</div>
							</div>

							<!-- Legs -->
							<div style="position:absolute; width:180px; height:180px; transform-origin:top left;">

								<!-- Separate div for easy scaling -->
								<div style="transform:scale(0.6); transform-origin:inherit;">

									<svg width="300" height="300">

										<!-- Show always the "from" direction -->
										<line class="satNavInstructionIcon" x1="150" y1="150" x2="150" y2="240"></line>

										<!-- All the legs to the junction or roundabout -->
										<use id="satnav_next_turn_icon_leg_22_5" xlink:href="#leg" transform="rotate(22.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_45_0" xlink:href="#leg" transform="rotate(45.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_67_5" xlink:href="#leg" transform="rotate(67.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_90_0" xlink:href="#leg" transform="rotate(90.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_112_5" xlink:href="#leg" transform="rotate(112.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_135_0" xlink:href="#leg" transform="rotate(135.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_157_5" xlink:href="#leg" transform="rotate(157.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_180_0" xlink:href="#leg" transform="rotate(180.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_202_5" xlink:href="#leg" transform="rotate(202.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_225_0" xlink:href="#leg" transform="rotate(225.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_247_5" xlink:href="#leg" transform="rotate(247.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_270_0" xlink:href="#leg" transform="rotate(270.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_292_5" xlink:href="#leg" transform="rotate(292.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_315_0" xlink:href="#leg" transform="rotate(315.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_leg_337_5" xlink:href="#leg" transform="rotate(337.5 150,150)" style="display:none;"></use>

										<!-- "No entry" sign for each of the legs -->
										<use id="satnav_next_turn_icon_no_entry_22_5" xlink:href="#no_entry" transform="rotate(22.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_45_0" xlink:href="#no_entry" transform="rotate(45.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_67_5" xlink:href="#no_entry" transform="rotate(67.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_90_0" xlink:href="#no_entry" transform="rotate(90.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_112_5" xlink:href="#no_entry" transform="rotate(112.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_135_0" xlink:href="#no_entry" transform="rotate(135.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_157_5" xlink:href="#no_entry" transform="rotate(157.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_180_0" xlink:href="#no_entry" transform="rotate(180.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_202_5" xlink:href="#no_entry" transform="rotate(202.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_225_0" xlink:href="#no_entry" transform="rotate(225.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_247_5" xlink:href="#no_entry" transform="rotate(247.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_270_0" xlink:href="#no_entry" transform="rotate(270.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_292_5" xlink:href="#no_entry" transform="rotate(292.5 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_315_0" xlink:href="#no_entry" transform="rotate(315.0 150,150)" style="display:none;"></use>
										<use id="satnav_next_turn_icon_no_entry_337_5" xlink:href="#no_entry" transform="rotate(337.5 150,150)" style="display:none;"></use>
									</svg>
								</div>	<!-- Separate div for easy scaling -->
							</div>	<!-- Legs and roundabout -->
						</div>	<!-- "satnav_next_turn_icon" -->
					</div>	<!-- Area with guidance icon(s) -->
				</div>	<!-- "satnav_guidance" -->
			</div>	<!-- Navigation -->

			<!-- Popups in the "large" information panel -->

			<div id="climate_control_popup"
				class="icon notificationPopup">

				<!-- Status LEDs -->
				<div id="fan_icon" class="iconSmall led ledOn" style="left:350px; top:45px;">
					<div class="centerAligned fas fa-fan"></div>
				</div>
				<div id="recirc" class="iconSmall led ledOff" style="left:450px; top:45px;">
					<div class="centerAligned fas fa-sync-alt"></div>
				</div>
				<div id="rear_heater_2" class="iconSmallMaterialDesign led ledOff" style="font-size:57px; left:700px; top:45px;">
					<div class="centerAligned fas md-car-defrost-rear"></div>
				</div>
				<div id="ac_enabled" class="iconSmall led ledOff" style="left:350px; top:125px;">
					<div class="centerAligned fas md-air-conditioner"></div>
				</div>
				<div id="ac_compressor" class="iconSmall led ledOff" style="left:700px; top:125px;">
					<div class="centerAligned fas fa-snowflake"></div>
				</div>

				<!-- Data -->
				<div id="set_fan_speed" class="dseg7" style="font-size:90px; left:200px; top:20px; width:100px;">-</div>
				<div id="condenser_pressure_bar" class="tag" style="font-size:50px; left:60px; top:125px; width:160px;">--</div>
				<div class="tag" style="font-size:50px; left:230px; top:125px; width:110px; text-align:left;">Bar</div>
				<div id="evaporator_temp" class="tag" style="font-size:50px; left:380px; top:125px; width:230px;">-.-</div>
				<div class="tag" style="font-size:50px; left:620px; top:125px; width:90px; text-align:left;">&deg;C</div>
			</div>

			<!-- Audio popup -->

			<div id="audio_popup"
				class="icon notificationPopup" style="top:100px; height:350px;">

				<!-- Status LEDs -->

				<div gid="ext_mute" class="led ledOff" style="left:10px; top:290px; width:110px;">EXT</div>
				<div gid="mute" class="led ledOff" style="left:130px; top:290px; width:150px;">MUTE</div>
				<div gid="ta_selected" class="led ledOn" style="left:500px; top:290px; width:100px;">TA</div>
				<div gid="ta_not_available" class="icon" style="position:absolute; left:500px; top:290px; width:100px;">
					<svg>
						<line stroke="var(--disabled-element-color)" stroke-width="14" stroke-linecap="round" x1="10" y1="30" x2="87" y2="8"></line>
					</svg>
				</div>
				<div gid="loudness" class="led ledOff" style="left:625px; top:290px; width:140px;">LOUD</div>

				<!-- Tuner popup -->
				<div id="tuner_popup" style="display:none;">

					<div gid="tuner_memory" class="dseg7" style="font-size:80px; left:25px; top:20px;">-</div>

					<div gid="tuner_band" class="dots" style="left:120px; top:20px; width:250px; text-align:left;">---</div>

					<!-- Icons involved in station searching -->
					<div gid="search_manual" class="led ledOff" style="left:280px; top:90px; width:120px;">MAN</div>
					<div gid="search_direction_up" class="led ledOff fas fa-caret-up" style="left:440px; top:20px; width:40px; height:37px;"></div>
					<div gid="search_direction_down" class="led ledOff fas fa-caret-down" style="left:440px; top:65px; width:40px; height:37px;"></div>

					<div gid="frequency_data_small" style="display:block;">
						<div gid="frequency" class="dseg7" style="font-size:80px; left:455px; top:20px; width:260px;"></div>
						<div gid="frequency_h" class="dseg7" style="font-size:50px; left:700px; top:20px; width:60px;"></div>
					</div>

					<div gid="frequency_data_large" style="display:none;">
						<div gid="frequency" class="dseg14" style="font-size:120px; left:270px; top:150px; width:400px; text-align:right;">---.-</div>
						<div gid="frequency_h" class="dseg14" style="font-size:120px; left:670px; top:150px; width:90px; text-align:left;">-</div>
					</div>

					<div gid="fm_tuner_data" style="display:block;">
						<div gid="rds_text" class="dseg14" style="font-size:110px; left:10px; top:150px; width:755px; text-align:right;"></div>
						<div gid="rds_selected" class="led ledOff" style="left:335px; top:290px; width:140px;">RDS</div>
						<div gid="rds_not_available" class="icon" style="position:absolute; left:335px; top:290px; width:140px;">
							<svg>
								<line stroke="var(--disabled-element-color)" stroke-width="14" stroke-linecap="round" x1="10" y1="30" x2="127" y2="8"></line>
							</svg>
						</div>
					</div>

				</div>	<!-- "tuner_popup" -->

				<!-- Tape popup -->
				<div id="tape_popup" style="display:none;">

					<div class="icon fas fa-vr-cardboard" style="left:20px; top:25px; font-size:160px; color:#576171;"></div>

					<div class="tag" style="left:180px; top:113px; width:200px;">Side</div>
					<div gid="tape_side" class="dseg7" style="font-size:120px; left:380px; top:40px; width:140px;">-</div>

					<div gid="tape_status_stopped" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-stop"></div>
					</div>
					<div gid="tape_status_loading" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-sign-in-alt"></div>
					</div>
					<div gid="tape_status_play" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-play"></div>
					</div>
					<div gid="tape_status_fast_forward" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-forward"></div>
					</div>
					<div gid="tape_status_next_track" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-fast-forward"></div>
					</div>
					<div gid="tape_status_rewind" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-backward"></div>
					</div>
					<div gid="tape_status_previous_track" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-fast-backward"></div>
					</div>

				</div>	<!-- "tape_popup" -->

				<!-- CD player popup -->

				<div id="cd_player_popup" style="display:none;">

					<div class="icon fas fa-compact-disc" style="left:20px; top:25px; font-size:160px; color:#576171;"></div>

					<div gid="cd_track_time" class="dseg7" style="font-size:123px; left:140px; top:40px; width:420px;">-:--</div>

					<div gid="cd_status_pause" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-pause"></div>
					</div>
					<div gid="cd_status_play" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-play"></div>
					</div>
					<div gid="cd_status_fast_forward" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-forward"></div>
					</div>
					<div gid="cd_status_rewind" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-backward"></div>
					</div>
					<div gid="cd_status_searching" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-compact-disc"></div>
					</div>
					<div gid="cd_status_loading" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-sign-in-alt"></div>
					</div>
					<div gid="cd_status_eject" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-eject"></div>
					</div>

					<div class="tag" style="left:365px; top:207px; width:200px;">Track</div>
					<div gid="cd_current_track" class="dots" style="left:565px; top:195px; width:90px; text-align:right;">--</div>
					<div class="tag" style="left:640px; top:200px; width:50px;">/</div>
					<div gid="cd_total_tracks" class="dots" style="left:685px; top:195px; width:90px; text-align:right;">--</div>

					<div gid="cd_random" class="iconSmall led ledOff" style="left:350px; top:280px;">
						<div class="centerAligned fas fa-random"></div>
					</div>

				</div>	<!-- "cd_player_popup" -->

				<!-- CD changer popup -->

				<div id="cd_changer_popup" style="display:none;">

					<div class="icon fas fa-compact-disc" style="left:20px; top:25px; font-size:70px; color:#576171;"></div>
					<div class="icon fas fa-compact-disc" style="left:65px; top:70px; font-size:70px; color:#576171;"></div>
					<div class="icon fas fa-compact-disc" style="left:110px; top:115px; font-size:70px; color:#576171;"></div>

					<div gid="cd_changer_track_time" class="dseg7" style="font-size:120px; left:140px; top:40px; width:420px;">-:--</div>

					<div gid="cd_changer_status_pause" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-pause"></div>
					</div>
					<div gid="cd_changer_status_play" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-play"></div>
					</div>
					<div gid="cd_changer_status_searching" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-compact-disc"></div>
					</div>
					<div gid="cd_changer_status_fast_forward" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-forward"></div>
					</div>
					<div gid="cd_changer_status_rewind" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-backward"></div>
					</div>
					<div gid="cd_changer_status_loading" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-sign-in-alt"></div>
					</div>
					<div gid="cd_changer_status_eject" class="icon mediaStatusInPopup">
						<div class="centerAligned fas fa-eject"></div>
					</div>

					<div class="tag" style="left:65px; top:207px; width:235px;">CD</div>
					<div gid="cd_changer_current_disc" class="dots" style="left:300px; top:195px; width:90px; text-align:right;">-</div>
					<div class="tag" style="left:365px; top:207px; width:200px;">Track</div>
					<div gid="cd_changer_current_track" class="dots" style="left:565px; top:195px; width:90px; text-align:right;">--</div>
					<div class="tag" style="left:640px; top:200px; width:50px;">/</div>
					<div gid="cd_changer_total_tracks" class="dots" style="left:685px; top:195px; width:90px; text-align:right;">--</div>

					<div gid="cd_changer_random" class="iconSmall led ledOff" style="left:350px; top:280px;">
						<div class="centerAligned fas fa-random"></div>
					</div>

				</div>	<!-- "cd_changer_popup" -->

			</div>	<!-- "audio_popup" -->

			<!-- Destination reached popup -->

			<div id="satnav_reached_destination_popup"
				class="icon notificationPopup" style="top:160px; height:280px;">

				<div class="centerAligned" style="position:absolute; left:10px; width:830px; height:200px;">
					<div id="satnav_reached_destination_popup_title">Destination reached</div>
					<div gid="satnav_last_destination_city" class="dots"
						style="top:90px; width:830px; font-size:40px; white-space:nowrap;">
					</div>
					<div class="dots"
						style="top:140px; width:830px; height:80px; font-size:40px;">
						<span gid="satnav_last_destination_street_shown"></span>&nbsp;<span id="satnav_reached_destination_house_number"></span>
					</div>
				</div>
			</div>	<!-- "satnav_reached_destination_popup" -->

			<!-- Trip computer popup -->

			<div id="trip_computer_popup"
				on_enter="initTripComputerPopup();"
				class="icon notificationPopup" style="left:30px; top:160px; height:280px; width:850px;">

				<div class="verticalLine" style="left:130px; height:280px;"></div>

				<!-- Tabs -->
				<div class="tab" style="left:30px; top:10px; width:800px; height:250px;">

					<!-- Tab buttons -->
					<div id="trip_computer_popup_fuel_data_button" class="tabLeft" style="top:10px; width:100px;">
						<div class="iconSmall fas fa-gas-pump"></div>
					</div>
					<div id="trip_computer_popup_trip_1_button" class="tabLeft" style="top:75px; width:100px;">1</div>
					<div id="trip_computer_popup_trip_2_button" class="tabLeft" style="top:140px; width:100px;">2</div>

					<!-- Tab contents -->
					<div id="trip_computer_popup_fuel_data" class="tabContent"
						style="display:none; border:none; position:absolute; left:100px; top:5px; width:700px; height:230px;">

						<div class="icon iconSmall" style="left:180px; top:20px;">
							<div class="fas fa-fire-alt"></div>
						</div>

						<div gid="inst_consumption" class="dots" style="left:100px; top:100px; width:220px; text-align:center;">--.-</div>
						<div gid="fuel_consumption_unit" class="tag tripComputerPopupTag" style="left:100px;">l/100 km</div>

						<div class="icon iconSmall" style="left:500px; top:15px;">
							<div class="fas fa-gas-pump"></div>
						</div>
						<div gid="distance_to_empty" class="dots" style="left:430px; top:100px; width:220px; text-align:center;">---</div>
						<div gid="distance_unit" class="tag tripComputerPopupTag" style="left:430px;">km</div>
					</div>

					<div id="trip_computer_popup_trip_1" class="tabContent"
						style="display:none; border:none; position:absolute; left:100px; top:5px; width:700px; height:230px;">

						<div class="icon iconSmall" style="left:70px; top:20px;">
							<div class="fas fa-angle-double-right"></div>
						</div>
						<div class="icon iconSmall" style="left:120px; top:20px;">
							<div class="fas fa-fire-alt"></div>
						</div>
						<div gid="avg_consumption_1" class="dots" style="left:30px; top:100px; width:220px; text-align:center;">--.-</div>
						<div gid="fuel_consumption_unit" class="tag tripComputerPopupTag" style="left:30px;">l/100 km</div>

						<div class="icon iconSmall" style="left:290px; top:20px;">
							<div class="fas fa-angle-double-right"></div>
						</div>
						<div class="icon iconSmall" style="left:340px; top:20px;">
							<div class="fas fa-tachometer-alt"></div>
						</div>
						<div gid="avg_speed_1" class="dots" style="left:250px; top:100px; width:220px; text-align:center;">--</div>
						<div gid="speed_unit" class="tag tripComputerPopupTag" style="left:250px;">km/h</div>

						<div class="icon iconSmall" style="left:510px; top:20px; width:70px;">...</div>
						<div class="icon iconSmall" style="left:560px; top:20px;">
							<div class="fas fa-car-side"></div>
						</div>
						<div gid="distance_1" class="dots" style="left:465px; top:100px; width:230px; text-align:center;">--</div>
						<div gid="distance_unit" class="tag tripComputerPopupTag" style="left:470px;">km</div>
					</div>

					<div id="trip_computer_popup_trip_2" class="tabContent"
						style="display:none; border:none; position:absolute; left:100px; top:5px; width:700px; height:230px;">

						<div class="icon iconSmall" style="left:70px; top:20px;">
							<div class="fas fa-angle-double-right"></div>
						</div>
						<div class="icon iconSmall" style="left:120px; top:20px;">
							<div class="fas fa-fire-alt"></div>
						</div>
						<div gid="avg_consumption_2" class="dots" style="left:30px; top:100px; width:220px; text-align:center;">--.-</div>
						<div gid="fuel_consumption_unit" class="tag tripComputerPopupTag" style="left:30px;">l/100 km</div>

						<div class="icon iconSmall" style="left:290px; top:20px;">
							<div class="fas fa-angle-double-right"></div>
						</div>
						<div class="icon iconSmall" style="left:340px; top:20px;">
							<div class="fas fa-tachometer-alt"></div>
						</div>
						<div gid="avg_speed_2" class="dots" style="left:250px; top:100px; width:220px; text-align:center;">--</div>
						<div gid="speed_unit" class="tag tripComputerPopupTag" style="left:250px;">km/h</div>

						<div class="icon iconSmall" style="left:510px; top:20px; width:70px;">...</div>
						<div class="icon iconSmall" style="left:560px; top:20px;">
							<div class="fas fa-car-side"></div>
						</div>
						<div gid="distance_2" class="dots" style="left:465px; top:100px; width:230px; text-align:center;">--</div>
						<div gid="distance_unit" class="tag tripComputerPopupTag" style="left:470px;">km</div>
					</div>
				</div>
			</div>	<!-- "trip_computer_popup" -->

			<!-- Audio settings popup -->

			<div id="audio_settings_popup" class="icon notificationPopup"
				style="left:50px; top:80px; width:830px; height:430px;">

				<div class="tag" style="left:50px; top:40px; width:200px;">Source</div>
				<div id="audio_source" class="dots" style="left:270px; top:33px; width:500px; text-align:left;"></div>

				<div class="tag" style="left:50px; top:142px; width:200px;">Volume</div>
				<div id="volume" gid="volume" class="dseg7" style="font-size:90px; left:270px; top:100px; width:150px; text-align: left;">-</div>

				<div gid="info_traffic" class="led ledOff" style="left:645px; top:150px; width:140px;">INFO</div>

				<div class="tag" style="left:50px; top:220px; width:200px;">Bass</div>
				<div id="bass" class="dots" style="left:260px; top:211px; width:100px; text-align:right;">-</div>
				<div class="tag" style="left:50px; top:290px; width:200px;">Treble</div>
				<div id="treble" class="dots" style="left:260px; top:281px; width:100px; text-align:right;">-</div>

				<div gid="loudness" class="led ledOn" style="left:200px; top:360px; width:160px;">LOUD</div>

				<div class="tag" style="left:410px; top:220px; width:200px;">Fader</div>
				<div id="fader" class="dots" style="left:640px; top:211px; width:150px; text-align:right;">-</div>
				<div class="tag" style="left:410px; top:290px; width:200px;">Balance</div>
				<div id="balance" class="dots" style="left:640px; top:281px; width:150px; text-align:right;">-</div>

				<div id="auto_volume" class="led ledOff" style="left:525px; top:360px; width:260px;">AUTO-VOL</div>

				<!-- Highlight boxes -->
				<div id="bass_select" class="highlight icon iconBorder" style="left:30px; top:198px; width:320px; height:60px;"></div>
				<div id="treble_select" class="highlight icon iconBorder" style="left:30px; top:268px; width:320px; height:60px;"></div>
				<div id="loudness_select" class="highlight icon iconBorder" style="left:190px; top:349px; width:147px; height:34px;"></div>
				<div id="fader_select" class="highlight icon iconBorder" style="left:400px; top:198px; width:390px; height:60px;"></div>
				<div id="balance_select" class="highlight icon iconBorder" style="left:400px; top:268px; width:390px; height:60px;"></div>
				<div id="auto_volume_select" class="highlight icon iconBorder" style="left:515px; top:349px; width:257px; height:34px;"></div>
			</div>	<!-- "audio_settings_popup" -->

			<!-- Door open popup -->

			<div id="door_open_popup" class="icon notificationPopup">
				<div class="centerAligned icon" style="position:absolute; left:40px; width:200px; height:175px;">
					<svg width="160px" height="175px" style="fill:#dfe7f2; fill:var(--main-color)">
						<defs>
							<g id="car_icon">
								<path d="M29.395,0H17.636c-3.117,0-5.643,3.467-5.643,6.584v34.804c0,3.116,2.526,5.644,5.643,5.644h11.759
									c3.116,0,5.644-2.527,5.644-5.644V6.584C35.037,3.467,32.511,0,29.395,0z M34.05,14.188v11.665l-2.729,0.351v-4.806L34.05,14.188z
									 M32.618,10.773c-1.016,3.9-2.219,8.51-2.219,8.51H16.631l-2.222-8.51C14.41,10.773,23.293,7.755,32.618,10.773z M15.741,21.713
									v4.492l-2.73-0.349V14.502L15.741,21.713z M13.011,37.938V27.579l2.73,0.343v8.196L13.011,37.938z M14.568,40.882l2.218-3.336
									h13.771l2.219,3.336H14.568z M31.321,35.805v-7.872l2.729-0.355v10.048L31.321,35.805z">
								</path>
							</g>
						</defs>
						<use xlink:href="#car_icon" transform="scale(3.3)"></use>

						<!-- One line for each door -->
						<line class="doors" id="door_front_left" x1="10" y1="65" x2="40" y2="48" style="display:none;"></line>
						<line class="doors" id="door_front_right" x1="146" y1="65" x2="116" y2="48" style="display:none;"></line>
						<line class="doors" id="door_rear_left" x1="10" y1="110" x2="40" y2="93" style="display:none;"></line>
						<line class="doors" id="door_rear_right" x1="146" y1="110" x2="116" y2="93" style="display:none;"></line>
						<line class="doors" id="door_boot" x1="50" y1="165" x2="105" y2="165" style="display:none;"></line>
					</svg>
				</div>
				<div id="door_open_popup_text" class="centerAligned" style="position:absolute; left:250px; width:500px;">
					Door open!
				</div>
			</div>	<!-- "door_open_popup" -->

			<!-- Notification popup, with warning or information icon -->
			<div id="notification_popup" class="icon notificationPopup">
				<div id="notification_icon_warning" class="glow centerAligned icon iconVeryLarge fas fa-exclamation-triangle"
					style="display:none; position:absolute; line-height:2.2; left:-20px; width:260px; height:260px;"></div>
				<div id="notification_icon_info" class="centerAligned icon iconVeryLarge fas fa-info-circle"
					style="display:block; position:absolute; left:30px;"></div>
				<div id="notification_icon" class="centerAligned icon iconVeryLarge fas"
					style="display:none; position:absolute; line-height:2.2; left:-20px; width:260px; height:260px;"></div>
				<div id="last_notification_message_on_mfd" class="centerAligned" style="position:absolute; left:200px; width:610px;">
				</div>
			</div>

			<!-- Sat nav initializing popup -->

			<div id="satnav_initializing_popup" class="icon notificationPopup">
				<div class="centerAligned messagePopupArea">
					Navigation system<br />being initialised
				</div>
			</div>	<!-- "satnav_initializing_popup" -->

			<!-- Sat nav popup "Input has been stored in personal directory" -->

			<div id="satnav_input_stored_in_personal_dir_popup"
				on_exit="upMenu();"
				class="icon notificationPopup">
				<div class="centerAligned messagePopupArea">
					Input has been stored in<br />the personal<br />directory
				</div>
			</div>	<!-- "satnav_input_stored_in_personal_dir_popup" -->

			<!-- Sat nav popup "Input has been stored in professional directory" -->

			<div id="satnav_input_stored_in_professional_dir_popup"
				on_exit="upMenu();"
				class="icon notificationPopup">
				<div class="centerAligned messagePopupArea">
					Input has been stored in<br />the professional<br />directory
				</div>
			</div>	<!-- "satnav_input_stored_in_professional_dir_popup" -->

			<!-- Sat nav popup "Delete item?" -->

			<div id="satnav_delete_item_popup"
				on_enter="selectButton('satnav_delete_item_popup_no_button');"
				class="icon notificationPopup" style="top:120px; height:350px;">
				<div class="centerAligned yesNoPopupArea" style="height:300px;">
					<div id="satnav_delete_item_popup_title">Delete item ?<br /><br /></div>

					<span style="font-size:60px;" id="satnav_delete_directory_entry_in_popup"></span>

					<div button_orientation="horizontal">
						<div id="satnav_delete_item_popup_yes_button"
							on_click="satnavDeleteDirectoryEntry();"
							class="icon button yesButton" style="left:150px; top:220px; width:150px; height:40px;">
							Yes
						</div>
						<div id="satnav_delete_item_popup_no_button"
							on_click="hidePopup();"
							class="icon button noButton" style="left:360px; top:220px; width:150px; height:40px;">
							No
						</div>
					</div>
				</div>
			</div>	<!-- "satnav_delete_item_popup" -->

			<!-- Sat nav calculating route popup -->

			<div id="satnav_computing_route_popup"
				on_exit="showDestinationNotAccessiblePopupIfApplicable();"
				class="icon notificationPopup">
				<div class="centerAligned messagePopupArea">Computing route<br />in progress</div>
			</div>	<!-- "satnav_computing_route_popup" -->

			<!--
				Sat nav popup with question: Keep criteria "Fastest route?" (Yes/No) .
				Note: this popup can appear over the "satnav_computing_route_popup"
			-->

			<div id="satnav_guidance_preference_popup"
				on_enter="selectButton('satnav_guidance_preference_popup_yes_button'); satnavDestinationNotAccessible = false;"
				on_esc="showDestinationNotAccessiblePopupIfApplicable();"
				class="icon notificationPopup" style="height:300px;">
				<div class="centerAligned yesNoPopupArea">
					<span id="satnav_guidance_preference_popup_title">Keep criteria</span>
					"<span id="satnav_guidance_current_preference_text">fastest route</span>"?

					<div button_orientation="horizontal">
						<div id="satnav_guidance_preference_popup_yes_button"
							class="icon button yesButton"
							on_click="satnavGuidancePreferencePopupYesButton();"
							style="left:150px; top:150px; width:150px; height:40px;">
							Yes
						</div>
						<div id="satnav_guidance_preference_popup_no_button"
							class="icon button noButton"
							on_click="satnavGuidancePreferencePopupNoButton();"
							style="left:360px; top:150px; width:150px; height:40px;">
							No
						</div>
					</div>
				</div>
			</div>	<!-- "satnav_guidance_preference_popup" -->

			<!-- Sat nav popup with question: Delete all data in directories? (Yes/No) -->

			<div id="satnav_delete_directory_data_popup"
				on_enter="selectButton('satnav_delete_directory_data_popup_no_button');"
				class="icon notificationPopup" style="height:300px;">
				<div class="centerAligned yesNoPopupArea">
					<div id="satnav_delete_directory_data_popup_title">This will delete all data in directories</div>

					<div button_orientation="horizontal">
						<div id="satnav_delete_directory_data_popup_yes_button"
							on_click="satnavDeleteDirectories();"
							class="icon button yesButton" style="left:150px; top:150px; width:150px; height:40px;">
							Yes
						</div>
						<div id="satnav_delete_directory_data_popup_no_button"
							on_click="hidePopup('satnav_delete_directory_data_popup');"
							class="icon button noButton" style="left:360px; top:150px; width:150px; height:40px;">
							No
						</div>
					</div>
				</div>
			</div>	<!-- "satnav_delete_directory_data_popup" -->

			<!-- Sat nav popup with question: Continue guidance to destination? (Yes/No) -->

			<div id="satnav_continue_guidance_popup"
				on_enter="selectButton('satnav_continue_guidance_popup_yes_button');"
				class="icon notificationPopup" style="height:300px;">
				<div class="centerAligned yesNoPopupArea">
					<div id="satnav_continue_guidance_popup_title">Continue guidance to destination ?</div>

					<div button_orientation="horizontal">
						<div id="satnav_continue_guidance_popup_yes_button"
							on_click="hidePopup('satnav_continue_guidance_popup'); satnavShowDisclaimer();"
							class="icon button yesButton" style="left:150px; top:150px; width:150px; height:40px;">
							Yes
						</div>
						<div id="satnav_continue_guidance_popup_no_button"
							on_click="hidePopup('satnav_continue_guidance_popup');"
							class="icon button noButton" style="left:360px; top:150px; width:150px; height:40px;">
							No
						</div>
					</div>
				</div>
			</div>	<!-- "satnav_continue_guidance_popup" -->

			<!-- Status popup (without icon) -->

			<div id="status_popup" class="icon notificationPopup">
				<div id="status_popup_text" class="centerAligned messagePopupArea">
				</div>
			</div>	<!-- "status_popup" -->

			<!-- Screen brightness popup -->

			<div id="screen_brightness_popup" class="icon notificationPopup">
				<div class="icon iconLarge fas fa-sun" style="left:70px; top:50px;"></div>
				<div id="screen_brightness_popup_value" class="dseg7" style="left:580px; top:55px; width:150px; text-align:right; font-size:90px">14</div>

				<div style="position:absolute; left:240px; top:60px; width:350px; height:80px;">
					<div id="screen_brightness_perc" class="gauge" style="transform: scaleX(1.0);">
						<svg style="width:324px;">
							<line style="stroke:#dfe7f2; stroke:var(--main-color); stroke-width:54; stroke-opacity:0.8;" x1="0" y1="40" x2="324" y2="40"></line>
						</svg>
					</div>
					<div class="gaugeBoxDiv">
						<svg style="width:348px;">
							<rect x="5" y="5" width="340" height="70" class="gaugeBox"></rect>
						</svg>
					</div>
				</div>
			</div>	<!-- "screen_brightness_popup" -->

			<!-- A tap anywhere on the screen triggers a change to the next screen -->
			<div style="display:block; position:absolute; width:960px; height:550px;" onclick="largeScreenTapped();"></div>

			<!-- "Gear" icon in the bottom right corner -->
			<div style="display:block; position:absolute; left:870px; top:480px; width:90px; height:100px;"
				onclick="gearIconAreaClicked();">
			</div>

		</div>	<!-- "Large" information panel -->

		<!-- Small area in the top right of the screen which triggers changing to full-screen mode -->
		<div onclick="toggleFullScreen();"
			style="position:absolute; left:800px; width:900px; height:250px; opacity:50%">
			<i id="full_screen_button" class="fas fa-expand" style="position:relative; left:500px;"></i>
		</div>

		<!-- Full-screen popups -->

		<!-- "Popup" window for tuner presets -->

		<div id="tuner_presets_popup" class="icon notificationPopup" style="left:150px; top:140px; width:1040px; height:310px; text-align:left;">

			<!-- Highlight boxes -->
			<div id="presets_memory_1_select" class="highlight icon iconBorder" style="left:20px; top:10px; width:470px; height:75px;"></div>
			<div id="presets_memory_2_select" class="highlight icon iconBorder" style="left:20px; top:100px; width:470px; height:75px;"></div>
			<div id="presets_memory_3_select" class="highlight icon iconBorder" style="left:20px; top:190px; width:470px; height:75px;"></div>
			<div id="presets_memory_4_select" class="highlight icon iconBorder" style="left:530px; top:10px; width:470px; height:75px;"></div>
			<div id="presets_memory_5_select" class="highlight icon iconBorder" style="left:530px; top:100px; width:470px; height:75px;"></div>
			<div id="presets_memory_6_select" class="highlight icon iconBorder" style="left:530px; top:190px; width:470px; height:75px;"></div>

			<div id="presets_fm_1" style="display:none;">
					<div class="tag" style="left:10px; top:40px; width:60px; text-align:right;">1</div>
					<div id="radio_preset_FM1_1" class="dots" style="left:90px; top:35px; width:450px;"></div>

					<div class="tag" style="left:10px; top:130px; width:60px; text-align:right;">2</div>
					<div id="radio_preset_FM1_2" class="dots" style="left:90px; top:125px; width:450px;"></div>

					<div class="tag" style="left:10px; top:220px; width:60px; text-align:right;">3</div>
					<div id="radio_preset_FM1_3" class="dots" style="left:90px; top:215px; width:450px;"></div>

					<div class="tag" style="left:500px; top:40px; width:80px; text-align:right;">4</div>
					<div id="radio_preset_FM1_4" class="dots" style="left:600px; top:35px; width:450px;"></div>

					<div class="tag" style="left:500px; top:130px; width:80px; text-align:right;">5</div>
					<div id="radio_preset_FM1_5" class="dots" style="left:600px; top:125px; width:450px;"></div>

					<div class="tag" style="left:500px; top:220px; width:80px; text-align:right;">6</div>
					<div id="radio_preset_FM1_6" class="dots" style="left:600px; top:215px; width:450px;"></div>
			</div>

			<div id="presets_fm_2" style="display:none;">
					<div class="tag" style="left:10px; top:40px; width:60px; text-align:right;">1</div>
					<div id="radio_preset_FM2_1" class="dots" style="left:90px; top:35px; width:450px;"></div>

					<div class="tag" style="left:10px; top:130px; width:60px; text-align:right;">2</div>
					<div id="radio_preset_FM2_2" class="dots" style="left:90px; top:125px; width:450px;"></div>

					<div class="tag" style="left:10px; top:220px; width:60px; text-align:right;">3</div>
					<div id="radio_preset_FM2_3" class="dots" style="left:90px; top:215px; width:450px;"></div>

					<div class="tag" style="left:500px; top:40px; width:80px; text-align:right;">4</div>
					<div id="radio_preset_FM2_4" class="dots" style="left:600px; top:35px; width:450px;"></div>

					<div class="tag" style="left:500px; top:130px; width:80px; text-align:right;">5</div>
					<div id="radio_preset_FM2_5" class="dots" style="left:600px; top:125px; width:450px;"></div>

					<div class="tag" style="left:500px; top:220px; width:80px; text-align:right;">6</div>
					<div id="radio_preset_FM2_6" class="dots" style="left:600px; top:215px; width:450px;"></div>
			</div>

			<div id="presets_fm_ast" style="display:none;">
					<div class="tag" style="left:10px; top:40px; width:60px; text-align:right;">1</div>
					<div id="radio_preset_FMAST_1" class="dots" style="left:90px; top:35px; width:450px;"></div>

					<div class="tag" style="left:10px; top:130px; width:60px; text-align:right;">2</div>
					<div id="radio_preset_FMAST_2" class="dots" style="left:90px; top:125px; width:450px;"></div>

					<div class="tag" style="left:10px; top:220px; width:60px; text-align:right;">3</div>
					<div id="radio_preset_FMAST_3" class="dots" style="left:90px; top:215px; width:450px;"></div>

					<div class="tag" style="left:500px; top:40px; width:80px; text-align:right;">4</div>
					<div id="radio_preset_FMAST_4" class="dots" style="left:600px; top:35px; width:450px;"></div>

					<div class="tag" style="left:500px; top:130px; width:80px; text-align:right;">5</div>
					<div id="radio_preset_FMAST_5" class="dots" style="left:600px; top:125px; width:450px;"></div>

					<div class="tag" style="left:500px; top:220px; width:80px; text-align:right;">6</div>
					<div id="radio_preset_FMAST_6" class="dots" style="left:600px; top:215px; width:450px;"></div>
			</div>

			<div id="presets_am" style="display:none;">
					<div class="tag" style="left:10px; top:40px; width:60px; text-align:right;">1</div>
					<div id="radio_preset_AM_1" class="dots" style="left:90px; top:35px; width:450px;"></div>

					<div class="tag" style="left:10px; top:130px; width:60px; text-align:right;">2</div>
					<div id="radio_preset_AM_2" class="dots" style="left:90px; top:125px; width:450px;"></div>

					<div class="tag" style="left:10px; top:220px; width:60px; text-align:right;">3</div>
					<div id="radio_preset_AM_3" class="dots" style="left:90px; top:215px; width:450px;"></div>

					<div class="tag" style="left:500px; top:40px; width:80px; text-align:right;">4</div>
					<div id="radio_preset_AM_4" class="dots" style="left:600px; top:35px; width:450px;"></div>

					<div class="tag" style="left:500px; top:130px; width:80px; text-align:right;">5</div>
					<div id="radio_preset_AM_5" class="dots" style="left:600px; top:125px; width:450px;"></div>

					<div class="tag" style="left:500px; top:220px; width:80px; text-align:right;">6</div>
					<div id="radio_preset_AM_6" class="dots" style="left:600px; top:215px; width:450px;"></div>
			</div>
		</div>

		<!-- Sat nav downloading popup -->

		<div id="satnav_downloading_popup" style="left:445px;"
			on_enter="satnavDownloading = true;"
			class="icon notificationPopup">
			<div class="centerAligned messagePopupArea">
				DOWNLOAD IN PROGRESS... <span id="satnav_download_progress"></span>
			</div>
		</div>	<!-- "satnav_downloading_popup" -->

		<!-- Full-screen panels -->

		<!-- System -->

		<div id="system" style="position:absolute; font-size:22px; background-color:var(--selected-element-color);
			display:none; left:0px; top:0px; width:1350px; height:550px; text-align:left;"
			on_enter="$('#web_socket_server_host').text(webSocketServerHost);">

			<div style="font-size:50px; text-align:center; padding-top:10px;">System</div>

			<div class="tabTop tabActive" style="position:absolute; font-size:35px; left:30px; top:60px; height:50px; padding-left:20px; padding-right:20px;">Browser</div>
			<div class="iconBorder" style="display:block; position:absolute; left:20px; top:110px; width:380px; height:270px;">
				<div class="tag" style="left:130px; top:10px; width:120px;">Width</div>
				<div class="tag" style="left:240px; top:10px; width:120px;">Height</div>
				<div class="tag" style="top:60px; width:150px;">Screen</div>
				<div class="tag" style="top:100px; width:150px;">Viewport</div>
				<div class="tag" style="top:140px; width:150px;">Window</div>
				<div id="screen_width" class="tag" style="left:130px; top:60px; width:120px;">---</div>
				<div id="viewport_width" class="tag" style="left:130px; top:100px; width:120px;">---</div>
				<div id="window_width" class="tag" style="left:130px; top:140px; width:120px;">---</div>
				<div id="screen_height" class="tag" style="left:240px; top:60px; width:120px;">---</div>
				<div id="viewport_height" class="tag" style="left:240px; top:100px; width:120px;">---</div>
				<div id="window_height" class="tag" style="left:240px; top:140px; width:120px;">---</div>

				<div class="tag" style="left:10px; top:200px; width:370px; text-align:left; font-size:25px;">Websocket server host:</div>
				<div id="web_socket_server_host" class="tag" style="left:10px; top:230px; width:370px; text-align:left; font-size:25px;">---</div>
			</div>

			<div class="tabTop tabActive" style="position:absolute; font-size:35px; left:430px; top:60px; height:50px; padding-left:20px; padding-right:20px;">ESP</div>
			<div class="iconBorder" style="display:block; position:absolute; left:420px; top:110px; width:910px; height:360px;">
				<div style="font-size:20px; line-height: 1.2;">
					<div class="tag" style="top:10px; width:230px;">Boot Version</div>
					<div class="tag" style="top:40px; width:230px;">Flash ID</div>
					<div class="tag" style="top:70px; width:230px;">Flash size (real)</div>
					<div class="tag" style="top:100px; width:230px;">Flash size (IDE)</div>
					<div class="tag" style="top:130px; width:230px;">Flash speed (IDE)</div>
					<div class="tag" style="top:160px; width:230px;">Flash mode (IDE)</div>
					<div class="tag" style="top:190px; width:230px;">Compiled @</div>
					<div class="tag" style="top:220px; width:230px;">MD5 checksum</div>
					<div class="tag" style="top:250px; width:230px;">SDK</div>
					<div class="tag" style="top:280px; width:230px;">Reset Reason</div>
					<div class="tag" style="top:310px; width:230px;">Reset Info</div>
					<div style="position:absolute; left:360px;">
						<div class="tag" style="top:10px; width:260px;">CPU Speed</div>
						<div class="tag" style="top:40px; width:260px;">Chip ID</div>
						<div class="tag" style="top:70px; width:260px;">MAC address</div>
						<div class="tag" style="top:100px; width:260px;">IP address</div>
						<div class="tag" style="top:130px; width:260px;">Wi-Fi RSSI</div>
						<div class="tag" style="top:160px; width:260px;">Free RAM</div>
						<div class="tag" style="top:190px; width:260px;">Uptime</div>
					</div>
				</div>
				<div id="esp_boot_version" class="tag" style="left:240px; top:7px;">---</div>
				<div id="esp_flash_id" class="tag" style="left:240px; top:37px;">---</div>
				<div id="esp_flash_size_real" class="tag" style="left:240px; top:67px;">---</div>
				<div id="esp_flash_size_ide" class="tag" style="left:240px; top:97px;">---</div>
				<div id="esp_flash_speed_ide" class="tag" style="left:240px; top:127px;">---</div>
				<div id="esp_flash_mode_ide" class="tag" style="left:240px; top:157px;">---</div>
				<div id="img_compile_date" class="tag" style="left:240px; top:187px;">---</div>
				<div id="img_md5_checksum" class="tag" style="left:240px; top:217px;">---</div>
				<div id="esp_sdk_version" class="tag" style="left:240px; top:247px;">---</div>
				<div id="esp_last_reset_reason" class="tag" style="left:240px; top:277px;">---</div>
				<div id="esp_last_reset_info" class="tag" style="left:240px; top:307px; width:660px; text-align:left; white-space:normal; height:60px;">---</div>
				<div id="esp_cpu_speed" class="tag" style="left:630px; top:7px;">---</div>
				<div id="esp_chip_id" class="tag" style="left:630px; top:37px;">---</div>
				<div id="esp_mac_address" class="tag" style="left:630px; top:67px;">---</div>
				<div id="esp_ip_address" class="tag" style="left:630px; top:97px;">---</div>
				<div id="esp_wifi_rssi" class="tag" style="left:630px; top:127px;">---</div>
				<div id="esp_free_ram" class="tag" style="left:630px; top:157px;">---</div>
				<div id="uptime" class="tag" style="left:630px; top:187px;">---</div>
			</div>

			<div id="van_bus_stats" class="tag"
				style="left:420px; top:485px; width:830px; text-align:left; font-size:25px; white-space:normal;"></div>

			<!-- "Back" icon in the bottom right corner -->
			<div class="iconSmall led ledOn" style="left:1260px; top:480px;">
				<div class="centerAligned fas fa-undo" onclick="gearIconAreaClicked();"></div>
			</div>
		</div>	<!-- "system" -->

		<!-- "Status" line: fixed element in each screen -->

		<div style="position:absolute; top:550px; width:1350px; height:90px;
			background:linear-gradient(to right, var(--background-color), var(--gradient-high-color), var(--background-color));">
		</div>

		<div id="doors_locked" class="iconSmall led ledOff" style="left:20px; top:560px;">
			<div class="centerAligned fas fa-lock"></div>
		</div>

		<div style="left:100px; top:560px; width:230px; position:absolute;">
			<div gid="inst_consumption" style="width:140px; font-size:55px; text-align:right; position:absolute;">--.-</div>
			<div id="fuel_consumption_unit_sm" class="tag" style="left:150px; top:24px; width:90px; font-size:31px; text-align:left;">/100</div>
		</div>

		<div style="left:345px; top:560px; width:220px; position:absolute; height:80px;">
			<div gid="distance_to_empty" style="width:150px; font-size:55px; text-align:right;">---</div>
			<div class="icon iconSmall" style="top:8px; left:145px;">
				<div class="fas fa-gas-pump"></div>
			</div>
		</div>

		<div id="exterior_temp_shown" gid="exterior_temp_shown" style="display:none; left:590px; top:560px; width:240px; height:70px; position:absolute; text-align:center; font-size:55px;">-- &deg;C</div>
		<div id="date_time_small" style="display:none; left:840px; top:568px; width:480px; position:absolute; text-align:center; font-size:40px;">---  - --:--</div>
)====="

COMMS_LED

R"=====(

	</body>
</html>
)=====";
