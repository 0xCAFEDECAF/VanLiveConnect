
char carInfo_css[] PROGMEM = R"=====(
@font-face
{
  /* For the "fixed" items in the display */
  font-family:Arial-Rounded-MT-bold;
  src:url(ArialRoundedMTbold.woff) format('woff');
}
@font-face
{
  /* For the "frequency" items in the display */
  font-family:DSEG7_Classic_BoldItalic;
  src:url(DSEG7Classic-BoldItalic.woff) format('woff');
}
@font-face
{
  /* For the "RDS" item in the display */
  font-family:DSEG14_Classic_BoldItalic;
  src:url(DSEG14Classic-BoldItalic.woff) format('woff');
}
@font-face
{
  /* For the "dotted" items in the display */
  font-family:Dots-All-For-Now;
  src:url(DotsAllForNow.woff) format('woff');
}
body
{
  /* Nice "dark-theme" background with light-blue text */
  background-color:rgb(41,55,74);
  font-family:Arial-Rounded-MT-bold,Arial,Helvetica,Sans-Serif;
  color:#dfe7f2;
  font-size:50px;
  white-space:nowrap;
/*
  background-image: url('6.jpg');
  background-repeat: no-repeat;
  background-attachment: fixed;
  background-size: 970px 650px;
*/
  /*background-blend-mode: luminosity;*/
  background-blend-mode: multiply; /* TODO - does not work well with IE11 */
  /*background-blend-mode: lighten;*/
}
/*
textarea
{
  background-color:rgb(41,55,74);
  color:#dfe7f2;
}
*/
/* Breaking of lines is prevented by setting 'white-space:pre', but in IE, that shoud *NOT* be set to achieve
   the same */
/*
@-moz-document url-prefix()
{
  textarea
  {
    white-space:pre;
  }
}
@media screen and (-ms-high-contrast: active), (-ms-high-contrast: none)
{
  textarea
  {
    background-color:rgb(41,55,74);
    color:#dfe7f2;
  }
}
*/
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
  font-family:DSEG7_Classic_BoldItalic;
  line-height:1.000000;
}
.dseg14
{
  position:absolute;
  font-family:DSEG14_Classic_BoldItalic;
  line-height:1.000000;
}
.dots
{
  position:absolute;
  overflow:hidden;
  font-family:Dots-All-For-Now;
  line-height:1.000000;
  font-size:60px;
}

/* Style the "LED" elements */
.led
{
  position:absolute;
  overflow:hidden;
  color:rgb(41,55,74);
  border-radius:5px;
  text-align:center;
  font-size:44px;
  line-height:0.85;
}
.ledOn
{
  background-color:#dfe7f2;
}
.ledOff
{
  background-color:rgb(67,82,105);
}
.ledActive
{
  border-top:25px solid #dfe7f2;
  border-bottom:25px solid #dfe7f2;
}

/* Style the "icon" elements */
.icon
{
  position:absolute;
  overflow:hidden;
  color:#dfe7f2;
  text-align:center;
  /*font-size:80px;*/
  /*line-height:1.3;*/
  /*background-color:rgb(41,55,74);*/
}
.iconLed
{
  color:rgb(41,55,74);
  border-radius:5px;
}
.iconBorder
{
  border:5px solid #dfe7f2;
  border-radius:15px;
}
.iconSmall
{
  font-size:44px;
  width:70px;
  height:57px;
  /*line-height:1.3;*/
}
.iconMedium
{
  font-size:80px;
  width:100px;
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
}
/*.iconNoBorder
{
  position:absolute;
  overflow:hidden;
  color:#dfe7f2;
  text-align:center;
  font-size:80px;
  line-height:1.3;
}
*/

/* Style the tab */
.tab
{
  position:absolute;
  overflow:hidden;
  color:#dfe7f2;
  text-align:center;
  line-height:1.3;
}
.tabTop
{
  border-top:5px solid #dfe7f2;
  border-left:5px solid #dfe7f2;
  border-right:5px solid #dfe7f2;
  border-top-left-radius:15px;
  border-top-right-radius:15px;
}
.tabBottom
{
  height:70px;
  border-bottom:5px solid #dfe7f2;
  border-left:5px solid #dfe7f2;
  border-right:5px solid #dfe7f2;
  border-bottom-left-radius:15px;
  border-bottom-right-radius:15px;
  line-height:1.5;
}
/* Style of the buttons inside the tab */
.tab button
{
  position:absolute;
  background-color:rgb(41,55,74);
  font-family:Arial-Rounded-MT-bold,Arial,Helvetica,Sans-Serif;
  color:#dfe7f2;
  font-size:50px;
  line-height:1.0;
  white-space:nowrap;
  outline: none;
  border-top:5px solid #dfe7f2;
  border-left:5px solid #dfe7f2;
  border-right:5px solid #dfe7f2;
  border-bottom:none;
  border-top-left-radius:15px;
  border-top-right-radius:15px;
}
/* Create an active/current tablink class */
.tab button.active
{
  color:rgb(41,55,74);
  background-color:#dfe7f2;
}
/* Style the tab content */
.tabcontent
{
  display:none;
  border:5px solid #dfe7f2;
  border-radius:15px;
  position:absolute;
}
.tabActive
{
  color:rgb(41,55,74);
  background-color:#dfe7f2;
}
.subScreen
{
  position:absolute;
}
.horizontalLine
{
/*  height:10px; */
  position:absolute;
  border-top:5px solid #dfe7f2;
}
.verticalLine
{
/*  height:10px; */
  position:absolute;
  border-left:5px solid #dfe7f2;
}
.satNavInstructionIcon
{
  stroke:#dfe7f2;
  stroke-width:14;
  stroke-linecap:round;
  fill:rgb(41,55,74);
}
.satNavInstructionIconLeg
{
  stroke-width:7;
}
.centerAligned
{
  position:relative;
  top:50%;
  transform:translateY(-50%);
  white-space:normal;
  line-height:1.2;
  text-align:center;
}
.notificationPopup
{
/*  background-color:rgba(41,55,74,0.95); */
  background-color:rgba(15,19,23,0.95);
  border:5px solid #dfe7f2;
  border-radius:15px;
  left:80px;
  top:200px;
  width:800px;
  height:200px;
}
.highlight
{
  display:none;
  border:12px solid #dfe7f2;
  background-color:rgba(223,231,242,0.4);
}
.show
{
  display:block !important;
}
.gaugeBox
{
  fill-opacity:0;
  stroke-width:8;
  stroke:#dfe7f2;
}
.satNavRoundabout
{
  fill:rgb(41,55,74);
  stroke-width:5;
  stroke:#dfe7f2;
}

/* Style the buttons and menu item elements */
.menuTitle
{
  font-size:60px;
  text-align:center;
}
.button
{
  overflow:hidden;
  border:5px solid #dfe7f2;
  border-radius:15px;
  border-style:dotted;
  text-align:center;
  font-size:44px;
  line-height:0.85;
  margin:auto;
  width:700px; /* Default, e.g. for items in a menu */
  padding:10px;
}
.buttonSelected
{
  color:rgb(41,55,74);
  background-color:#dfe7f2;
  border:5px solid #dfe7f2;
}
.buttonDisabled
{
  color:rgb(67,82,105);
}
.invertedText
{
  /* Add a bit of extra whitespace around the letter */
  line-height:1.5;
  display:inline-block;
  padding-left:10px;
  padding-right:5px;

  /* Invert foreground and background color */
  color:rgb(41,55,74);
  background-color:#dfe7f2;
}
.tickBox
{
  padding:0px;
  display:inline-block;
  width:50px;
  height:50px;
  margin-bottom:-10px;
  font-size:60px;
}
)=====";
