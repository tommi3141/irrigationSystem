#include "WiFi.h"
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <ESP32Time.h>



//----------------------------global variable------------------


volatile bool irrigationActive = false;

// Assign output variables to GPIO pins
volatile const int valve1_RED_LED = 32;
volatile const int yellowLED = 33;

const char* Hour = "hour";
const char* Minute = "minute";


String valveState = "off";

bool irrigation = false;
volatile bool MyBoolen = true;

int IrrigationStartHour = 6;
int IrrigationStartMinute = 0;
int IrrigationStopHour = 7;
int IrrigationStopMinute = 0;

volatile int currentDuration = 60;


//-----------------------Timer Stuff------------------

hw_timer_t * valveTimer = NULL;
hw_timer_t * helperTimer = NULL;
portMUX_TYPE timerMux0 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;

volatile uint8_t yellowledState = 0;

//Timer to disable Valve after <duration>
void IRAM_ATTR onValveTimer() {
  // Critical Code here
  portENTER_CRITICAL_ISR(&timerMux0);
  digitalWrite(valve1_RED_LED, HIGH);   // turn the LED on or off
  irrigationActive = false;
  valveState = "off";
  timerStop(valveTimer);
  portEXIT_CRITICAL_ISR(&timerMux0);
}

void IRAM_ATTR onHelperTimer() {
  // Critical Code here
  portENTER_CRITICAL_ISR(&timerMux1);
  yellowledState = 1 - yellowledState;
  digitalWrite(yellowLED, yellowledState);   // turn the LED on or off
  MyBoolen = true;
  portEXIT_CRITICAL_ISR(&timerMux1);
}


//---------------------------web server general----------------------

const char *ssid = "IrrigationSystem";
const char *password = "1234";
AsyncWebServer server(80);

//-----------------------------set time html-------------------------

const char setTime_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <span class="CurrentTime">CurrentTime</span> 
  <span id="time">%TIME%</span>
  <form action="/get">
    Current Hour: <input type="number" name="hour"
       min="0" max="23">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get">
    Current Minute: <input type="number" name="minute"
        min="0" max="59">
    <input type="submit" value="Submit">
  </form><br>

  <p><a href="/">Home</a></p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("time").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/getCurrentTime", true);
  xhttp.send();
}, 1000 ) ;

</script>
</html>)rawliteral";


//------------------------------SetDuration------------------
const char setDuration_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  
  <form action="/get">
    Current HOT Duration (in Min): <input type="number" name="HotDuration"
       min="0" max="120">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get">
    Current NORMAL Duration (in Min): <input type="number" name="NormalDuration"
       min="0" max="120">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get">
    Current COOL Duration (in Min): <input type="number" name="CoolDuration"
       min="0" max="120">
    <input type="submit" value="Submit">
  </form><br>

  <p><a href="/">Home</a></p>

</body>
</html>)rawliteral";


//--------------------------------HomePage--------------


const char homePage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Elben Irrigation System</title>
    <style>
      html {
        font-family: Helvetica;
        display: inline-block;
        margin: 0px auto;
        text-align: center;
      }
      .Box {
        border: 3px solid rgb(2, 112, 29);
        border-radius: 1rem;
        margin: auto;
      }
      .button {
        background-color: #4caf50;
        border: none;
        color: white;
        padding: 16px 40px;
        text-decoration: none;
        font-size: 30px;
        margin: 2px;
        cursor: pointer;
      }
      .button2 {
        background-color: #555555;
      }
    </style>
  </head>
  <body>
    <h1>Elben Irrigation System</h1>

    <div class="Box">
      <h2>Status</h2>
      <p id="time">%TIME%</p>
      <Span>Valve: </Span>
      <Span id="valveState"> %VALVESTATE%</Span>
      <p id="valveCounter">__</p>
      <p>Start Irrigation at:</p>
      <p id="plan">%STARTTIMEHOUR%:%STARTTIMEMIN% for %DURATION% min</p>
    </div>

    <br />

    <div class="Box">
      <h2>Change Plan</h2>

      <form
        action="#"
        route="/setStartEnd"
        onsubmit="return sendData(this, calculateStartEndTime)"
      >
        Start Time (Hour:Minutes)
        <div class="time-input">
          <input
            type="number"
            id="hour"
            min="0"
            max="24"
            oninput="enforceMinMax(this)"
            value="5"
          />
          :
          <input
            type="number"
            id="minute"
            min="0"
            max="60"
            oninput="enforceMinMax(this)"
            value="0"
          />
        </div>
        Duration (minutes):
        <input
          type="number"
          id="duration"
          min="1"
          max="120"
          oninput="enforceMinMax(this)"
          value="30"
        />
        <br />
        <input class="button" type="submit" value="Change Plan" />
      </form>

    </div>
    <br />
    <div class="Box">
      <h2>Settings</h2>
      <p><a href="/changeDurations">Change Durations</a></p>
      <p><a href="/setTime">Change current Time</a></p>
    </div>
  </body>

<script>


function enforceMinMax(inputElement) {
      inputElement.value = Math.max(inputElement.value, inputElement.min);
      inputElement.value = Math.min(inputElement.value, inputElement.max);
    }

    function calculateStartEndTime(formElement) {
      var start_hour = Number(formElement.querySelector("#hour").value);
      var start_minutes = Number(formElement.querySelector("#minute").value);
      var duration_in_minutes = Number(
        formElement.querySelector("#duration").value
      );

      var end_date = new Date();
      end_date.setHours(start_hour);
      end_date.setMinutes(start_minutes + duration_in_minutes);

      var end_hour = end_date.getHours();
      var end_minutes = end_date.getMinutes();

      return {
        start_hour,
        start_minutes,
        end_hour,
        end_minutes,
        duration_in_minutes,
      };
    }

    function sendData(formElement, prepareDataCB) {
      var data_to_send = prepareDataCB(formElement);

      var route = formElement.getAttribute("route");
      fetch(route + "?" + new URLSearchParams(data_to_send)).then(updateValues);

      return false;
    }
    
var valvestate;

function updateValues ( ) {
  fetch('/getCurrentValveState').then(response => {
    return response.text()
  }).then(data => {
    document.getElementById("valveState").innerHTML = data;
    valvestate = data;
  })

  fetch('/getCurrentTime').then(response => {
    return response.text()
  }).then(data => {
    document.getElementById("time").innerHTML = data;
  })

  fetch('/getPlan').then(response => {
    return response.text()
  }).then(data => {
    document.getElementById("plan").innerHTML = data;
  })
  

  /*  
  var xhttpTime = new XMLHttpRequest();
  var xhttpValveState = new XMLHttpRequest();
  xhttpTime.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("time").innerHTML = this.responseText;
    }
  };

  xhttpValveState.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("valveState").innerHTML = this.responseText;
      valvestate = this.responseText;
    }
  }; 
  
  xhttpValveState.open("GET", "/getCurrentValveState", true);
  xhttpTime.open("GET", "/getCurrentTime", true);
  xhttpValveState.send();
  xhttpTime.send();
  */
}
setInterval(updateValues, 1000 ) ;


</script>
</html>)rawliteral";




//------------------------------testPage------------------------

const char test_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  
  <p>Das ist ein Test: %TEST1%</p>
  <p>Noch ein Test: %TEST2%</p>

  <p><a href="/">Home</a></p>

</body>
</html>)rawliteral";



//------------------------------------RTC_on_Board------------------
ESP32Time rtc(0);
String currentTime;



//-----------------------------------helper functions----------------------

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String TimeProcessor(const String& var) {
  //Serial.println(var);
  if (var == "TIME") {
    currentTime = rtc.getTime();
    return currentTime;
  }
  return String();
}


String HomePageProcessor(const String& var) {
  //Serial.println(var);
  if (var == "TIME") {
    return rtc.getTime();
  }
  if (var == "VALVESTATE") {
    return valveState;
  }
  if (var == "STARTTIMEHOUR") {
    return String(IrrigationStartHour);
  }
  if (var == "STARTTIMEMIN") {
    if (String(IrrigationStartMinute).length() == 1) {
      return "0" + String(IrrigationStartMinute) ;
    } else {
      return String(IrrigationStartMinute);
    }

  }
  if (var == "IrrigationStartMinute") {
    return String(IrrigationStartMinute);
  }
  if (var == "IrrigationStartHour") {
    return String(IrrigationStartHour);
  }
  if (var == "DURATION") {
    return String(currentDuration);
  }
  return String();
}



//-----------------------------------Setup-Functions------------------
void setup_RTC() {
  //in case of time loss
  rtc.setTime(1, 16, 15, 5, 7, 2022);  // 17th Jan 2021 15:24:30
}


void setup_Test_LEDs() {
  pinMode(valve1_RED_LED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  digitalWrite(valve1_RED_LED, HIGH);
  digitalWrite(yellowLED, LOW);
}

void setup_Sever() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());


  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", homePage, HomePageProcessor);
  });


  server.on("/changeDurations", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", setDuration_html);
  });

  server.on("/getCurrentTime", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", rtc.getTime().c_str());
  });

  server.on("/getCurrentValveState", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", valveState.c_str());
  });

  server.on("/getPlan", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(String(IrrigationStartHour) + ":" + String(IrrigationStartMinute) + " for " + String(currentDuration) + " min").c_str());
  });
  
  server.on("/setStartEnd", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("start_hour")) {
      IrrigationStartHour = request->getParam("start_hour")->value().toInt();
    }
    if (request->hasParam("start_minutes")) {
      IrrigationStartMinute = request->getParam("start_minutes")->value().toInt();
    }
    if (request->hasParam("end_minutes")) {
      IrrigationStopMinute = request->getParam("end_minutes")->value().toInt();
    }
    if (request->hasParam("end_hour")) {
      IrrigationStopHour = request->getParam("end_hour")->value().toInt();
    }
    if (request->hasParam("end_minutes")) {
      IrrigationStopMinute = request->getParam("end_minutes")->value().toInt();
    }
    if (request->hasParam("duration_in_minutes")) {
      currentDuration = request->getParam("duration_in_minutes")->value().toInt();
    }
    request->send_P(200, "text/html", homePage, HomePageProcessor);
  });

  





  server.on("/setTime", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", setTime_html, TimeProcessor);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(Hour)) {
      inputMessage = request->getParam(Hour)->value();
      inputParam = Hour;
      rtc.setTime(rtc.getSecond(), rtc.getMinute(), inputMessage.toInt(), rtc.getDay(), rtc.getMonth(), rtc.getYear());
      request->send_P(200, "text/html", setTime_html, TimeProcessor);
    }
    // GET input2 value on <ESP_IP>/get?Minute=<inputMessage>
    else if (request->hasParam(Minute)) {
      inputMessage = request->getParam(Minute)->value();
      inputParam = Minute;
      rtc.setTime(rtc.getSecond(), inputMessage.toInt(), rtc.getHour(true), rtc.getDay(), rtc.getMonth(), rtc.getYear());
      request->send_P(200, "text/html", setTime_html, TimeProcessor);
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);


  });
  server.onNotFound(notFound);
  server.begin();
}

//---------------------------------------Timer---------------------------


void StartTimerBlink(int duration) {
  Serial.println("start timer 1");
  helperTimer = timerBegin(1, 80, true);  // timer 1, MWDT clock period = 12.5 ns * TIMGn_Tx_WDT_CLK_PRESCALE -> 12.5 ns * 80 -> 1000 ns = 1 us, countUp
  timerAttachInterrupt(helperTimer, &onHelperTimer, true); // edge (not level) triggered
  timerAlarmWrite(helperTimer, duration , true); // 2000000 * 1 us = duration s,
  // at least enable the timer alarms
  timerAlarmEnable(helperTimer); // enable
}


void StartValve(int duration) {
  valveState = "on";
  irrigationActive = true;
  digitalWrite(valve1_RED_LED, LOW);
}

void printInfo() {
  Serial.println("Hour" + String(rtc.getHour()));
  Serial.println("Minute" + String(rtc.getMinute()));
  Serial.println("Irrigation Active" + String(irrigationActive));
}


void stopValve() {
  digitalWrite(valve1_RED_LED, HIGH);   // turn the LED on or off
  irrigationActive = false;
  valveState = "off";
}


//----------------------------------main-functions----------------------


void setup() {

  Serial.begin(115200);
  setup_RTC();
  setup_Test_LEDs();
  setup_Sever();
  StartTimerBlink(2000000);

}

void loop() {
  if (rtc.getHour(true) == IrrigationStartHour && rtc.getMinute() == IrrigationStartMinute && !irrigationActive) {
    StartValve(currentDuration);
  }
  if (rtc.getHour(true) == IrrigationStopHour && rtc.getMinute() == IrrigationStopMinute && irrigationActive) {
    stopValve();
  }
  if (MyBoolen) {
    printInfo();
    MyBoolen = false;
  }

}
