
char carInfo_css[] PROGMEM = R"=====(
@font-face
{
  /* New 'Peugot' font */
  font-family:Peugeot-New;
  src: url(PeugeotNewRegular.woff) format('woff');
}

/* Default: "dark-theme" background with light-blue text */
:root
{
  --main-color:hsl(215,42%,91%);
  --background-color:rgb(8,7,19);
  --gradient-high-color:hsl(194,83%,40%);
  --led-off-color:rgb(25,31,40);
  --notification-color:rgba(15,19,23,0.95);
  --highlight-color:rgba(223,231,242,0.4);
  --selected-element-color:rgb(41,55,74);
  --disabled-element-color:rgb(67,82,105);
  --scale-factor:1;
}
body
{
  overflow: hidden; /* No scrollbars */

  background-color:var(--background-color);
  font-family:Peugeot-New,Arial,Helvetica,Sans-Serif;
  color:var(--main-color);
  font-size:33px;
  white-space:nowrap;

  background-image: url('background.jpg');
  background-repeat: no-repeat;
  background-attachment: fixed;
  background-size: 100%;

  transform:scale(var(--scale-factor));
  transform-origin: 0 0;
}
.languageIcon
{
  position:relative;
  font-size:30px;
  border-width:4px;
  border-style:solid;
  border-radius:30px;
  padding-left: 5px;
  padding-right: 5px;
}
.tag
{
  position:absolute;
  text-align:right;
  overflow:hidden;
}
.dseg7
{
  position:absolute;
  text-align:right;
  font-family:Peugeot-New;
  line-height:1.000000;
}
.dseg14
{
  position:absolute;
  font-family:Peugeot-New;
  line-height:1.000000;
}
.dots
{
  position:absolute;
  overflow:hidden;
  font-family:Peugeot-New;
  line-height:1.000000;
  font-size:55px;
}

/* Style the "LED" elements */
.led
{
  position:absolute;
  overflow:hidden;
  color:var(--selected-element-color);
  border-radius:5px;
  text-align:center;
  font-size:30px;
  font-weight:bold;
  line-height:1.2;
}
.ledOn
{
  background-color:var(--main-color);
}
.ledOff
{
  background-color:var(--led-off-color);
}
.ledOnOrange
{
  background-color:rgb(255,144,1);
}
.ledOnRed
{
  background-color:red;
}
.ledOnGreen
{
  background-color:rgb(119,217,64);
}
.ledOnBlue
{
  background-color:rgb(87,89,247);
}
.ledActive
{
  border-top:25px solid var(--main-color);
  border-bottom:25px solid var(--main-color);
}

/* Style the "glow" effect */
.glow
{
  color: #fff;
  animation: glow 1s ease-in-out infinite alternate;
}
@-webkit-keyframes glow
{
  from
  {
    /* Alternative (more orange) color: #e66c00 */
    text-shadow: 0 0 10px #fff, 0 0 20px #fff, 0 0 30px #e60073, 0 0 40px #e60073, 0 0 50px #e60073, 0 0 60px #e60073, 0 0 70px #e60073;
  }
  
  to
  {
    /* Alternative (more orange) color: #ff6e4d */
    text-shadow: 0 0 20px #fff, 0 0 30px #ff4da6, 0 0 40px #ff4da6, 0 0 50px #ff4da6, 0 0 60px #ff4da6, 0 0 70px #ff4da6, 0 0 80px #ff4da6;
  }
}

/* Style the "ice glow" effect */
.glowIce
{
  color: #fff;
  animation: glowIce 1s ease-in-out infinite alternate;
}
@-webkit-keyframes glowIce
{
  from
  {
    text-shadow: 0 0 10px #fff, 0 0 20px #fff, 0 0 30px #0f00e6, 0 0 40px #0f00e6, 0 0 50px #0f00e6, 0 0 60px #0f00e6, 0 0 70px #0f00e6;
  }
  
  to
  {
    text-shadow: 0 0 20px #fff, 0 0 30px #4d91ff, 0 0 40px #4d91ff, 0 0 50px #4d91ff, 0 0 60px #4d91ff, 0 0 70px #4d91ff, 0 0 80px #4d91ff;
  }
}

/* Style the "icon" elements */
.icon
{
  position:absolute;
  overflow:hidden;
  text-align:center;
}
.iconBorder
{
  border:5px solid;
  border-radius:15px;
}
.iconSmall
{
  font-size:44px;
  width:70px;
  height:57px;
}
.iconMedium
{
  font-size:60px;
  width:100px;
  height:104px;
}
.iconLarge
{
  font-size:100px;
  width:160px;
  height:140px;
  line-height:1;
}
.iconVeryLarge
{
  font-size:120px;
  width:160px;
  line-height:1.2;
}

/* Style the tab */
.tab
{
  position:absolute;
  overflow:hidden;
  text-align:center;
  line-height:1.3;
}
.tabTop
{
  border-top:5px solid var(--main-color);
  border-left:5px solid var(--main-color);
  border-right:5px solid var(--main-color);
  border-top-left-radius:15px;
  border-top-right-radius:15px;
}
.tabBottom
{
  height:70px;
  border-bottom:5px solid var(--main-color);
  border-left:5px solid var(--main-color);
  border-right:5px solid var(--main-color);
  border-bottom-left-radius:15px;
  border-bottom-right-radius:15px;
  line-height:1.5;
}
.tabLeft
{
  position:absolute;
  height:60px;
  border-top:5px solid var(--main-color);
  border-left:5px solid var(--main-color);
  border-bottom:5px solid var(--main-color);
  border-top-left-radius:15px;
  border-bottom-left-radius:15px;
  line-height:1.4;
}

/* Style of the buttons inside the tab */
.tab button
{
  position:absolute;
  background:none;
  color:var(--main-color);
  font-size:50px;
  line-height:1.0;
  white-space:nowrap;
  outline: none;
  border-top:5px solid var(--main-color);
  border-left:5px solid var(--main-color);
  border-right:5px solid var(--main-color);
  border-bottom:none;
  border-top-left-radius:15px;
  border-top-right-radius:15px;
}
.tab button.active
{
  color:var(--selected-element-color);
  background-color:var(--main-color);
}

/* Style the tab content */
.tabContent
{
  display:none;
  border:5px solid;
  border-radius:15px;
  position:absolute;
}
.tabActive
{
  color:var(--selected-element-color);
  background-color:var(--main-color);
}
.horizontalLine
{
  position:absolute;
  border-top:5px solid;
}
.verticalLine
{
  position:absolute;
  border-left:5px solid;
}
.centerAligned
{
  position:relative;
  top:50%;
  transform:translateY(-50%);
  white-space:normal;
  line-height:1.25;
  text-align:center;
}

/* Styles for popups */
.notificationPopup
{
  background-color:var(--notification-color);
  border:5px solid;
  border-radius:15px;
  left:55px;
  top:200px;
  width:850px;
  height:200px;
  display:none;
  font-size:40px;
}
.messagePopupArea
{
  position:absolute;
  left:100px;
  width:610px;
  font-size:40px;
}
.yesNoPopupArea
{
  position:absolute;
  left:50px;
  width:710px;
  height:200px;
}

.highlight
{
  display:none;
  border:12px solid;
  background-color:var(--highlight-color);
}
.show
{
  display:block !important;
}
.gauge
{
  position:absolute;
  left:12px;
  top:0px;
  width:324px;
  height:60px;
  transform:scaleX(0.0);
  transform-origin:left center;
}
.gaugeBox
{
  fill-opacity:0;
  stroke-width:8;
  stroke:var(--main-color);
}
.gaugeBoxDiv
{
  position:absolute;
  left:0px;
  top:0px;
  width:340px;
  height:60px;
}
.gaugeInnerBoxDiv
{
  position:absolute;
  left:8px;
  top:0px;
  width:332px;
  height:60px;
}

/* Style the menu screens, buttons and item elements */
.menuScreen
{
  display:none;
  left:20px;
  top:130px;
  width:920px;
  height:410px;
  font-size:55px;
  text-align:center;
  line-height:150%;
}
.button
{
  overflow:hidden;
  border:5px solid;
  border-radius:15px;
  border-color:var(--disabled-element-color);
  text-align:center;
  font-size:37px;
  line-height:1.1;
  margin:auto;
  width:700px; /* Default, e.g. for items in a menu */
  padding:10px;
}
.buttonSelected
{
  color:var(--selected-element-color);
  background-color:var(--main-color);
  border:5px solid var(--main-color);
}
.buttonDisabled
{
  color:var(--disabled-element-color);
}
.buttonBar
{
  position:absolute;
  left:20px;
  top:460px;
  width:940px;
  height:80px;
}
.validateButton
{
  position:absolute;
  left:0px;
  top:0px;
  width:260px;
  height:40px;
  font-weight:bold;
}
.correctionButton
{
  left:210px;
  top:0px;
  width:230px;
  height:40px;
}
.invertedText
{
  /* Add a bit of extra whitespace around the letter */
  line-height:1.5;
  display:inline-block;
  padding-left:10px;
  padding-right:5px;

  /* Invert foreground and background color */
  color:var(--selected-element-color);
  background-color:var(--main-color);
}
.tickBox
{
  padding:0px;
  display:inline-block;
  width:50px;
  height:50px;
  margin-bottom:-10px;
  font-size:50px;
}
.tickBoxLabel br
{
  line-height:70px;
}

/* Multimedia */
.mediaStatus
{
  position:absolute;
  overflow:hidden;
  text-align:center;
  font-size:100px;
  width:160px;
  height:140px;
  line-height:1;
  border:5px solid;
  border-radius:15px;
  display:none;
  left:600px;
  top:140px;
}
.mediaStatusInPopup
{
  position:absolute;
  overflow:hidden;
  text-align:center;
  font-size:100px;
  width:160px;
  height:140px;
  line-height:1;
  border:5px solid;
  border-radius:15px;
  display:none;
  left:600px;
  top:35px;
}

/* Trip computers */
.tripComputerTag
{
  left:180px;
  width:140px;
  font-size:28px;
  line-height:1.5;
}
.tripComputerPopupTag
{
  width:220px;
  top:170px;
  font-size:28px;
  text-align:center;
}

/* Sat nav menu and screen element styles */
.satNavInstructionIcon
{
  stroke:var(--main-color);
  stroke-width:14;
  stroke-linecap:round;
  fill:var(--selected-element-color);
}
.satNavInstructionDisabledIcon
{
  stroke:var(--disabled-element-color);
}
.satNavInstructionIconText
{
  fill:var(--main-color);
  dominant-baseline:middle;
  text-anchor:middle;
}
.satNavInstructionDisabledIconText
{
  fill:var(--disabled-element-color);
}
.satNavInstructionIconLeg
{
  stroke-width:7;
}
.satNavRoundabout
{
  fill:var(--selected-element-color);
  stroke-width:5;
  stroke:var(--main-color);
}
.satNavEnterDestination
{
  left:20px;
  top:180px;
  width:925px;
  height:60px;
  font-size:50px;
}
.satNavEnterDestinationTag
{
  left:20px;
  top:110px;
  width:930px;
  text-align:left;
}
.satNavShowCharacters
{
  left:25px;
  width:940px;
  font-size:44px;
  line-height:1.5;
  display:inline-block;
  background:none;
  color:var(--main-color);
  border-style:none;
}
.satNavAddressEntry
{
  left:25px;
  top:110px;
  width:925px;
  height:65px;
}
.satNavCityTag
{
  left:25px;
  top:190px;
  width:190px;
  text-align:left;
  font-size:32px;
  line-height:1.7;
}
.satNavStreetTag
{
  left:25px;
  top:280px;
  width:190px;
  text-align:left;
  font-size:32px;
  line-height:1.7;
}
.satNavNumberTag
{
  left:25px;
  top:370px;
  width:190px;
  text-align:left;
  font-size:32px;
  line-height:1.7;
}
.satNavCompassNeedle
{
  position:absolute;
  left:870px;
  top:20px;
  width:48px;
  height:72px;
  transform-origin:center;
}
.satNavShowAddress
{
  left:25px;
  top:110px;
  width:830px;
  text-align:left;
}
.satNavShowAddressCity
{
  left:210px;
  top:195px;
  width:720px;
  height:90px;
  font-size:40px;
  white-space:normal;
}
.satNavShowAddressStreet
{
  left:210px;
  top:285px;
  width:720px;
  height:90px;
  font-size:40px;
  white-space:normal;
}
.satNavShowAddressNumber
{
  left:210px;
  top:375px;
  width:720px;
  height:90px;
  font-size:40px;
  white-space:normal;
}
.satNavEntryNameTag
{
  left:25px;
  top:205px;
  width:190px;
  text-align:left;
  font-size:35px;
  line-height:1.5;
}
.satNavEntryExistsTag
{
  display:none;
  left:210px;
  top:255px;
  text-align:left;
  font-size:35px;
  line-height:1.5;
}
)=====";
