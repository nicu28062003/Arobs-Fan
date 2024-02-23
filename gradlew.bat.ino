#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define PWM_MAX_DUTY 1023

const char* ssid = "nicu";
const char* password = "11111111";

const int speedPin = D3;
const int temperatureInputPin = A0;
const int motorPinOne = D1;
const int motorPinTwo = D2;
const int powerPin = D4;

const double VCC = 3.3;
const double R2 = 9980;
const double adc_resolution = 1023;
const double A = 0.001129148;
const double B = 0.000234125;
const double C = 0.0000000876741;

ESP8266WebServer server(80);
int targetSpeed = 0;
double maxTemperature = 30.0;
int manualModeFlag = 1;
int powerState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(speedPin, OUTPUT);
  analogWrite(speedPin, 0);
  pinMode(motorPinOne, OUTPUT);
  pinMode(motorPinTwo, OUTPUT);
  pinMode(temperatureInputPin, INPUT);
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, powerState);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, []() {
    String htmlPage = R"(
<!DOCTYPE html>
<html lang="ro">

<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control Ventilator</title>
  <style>
    body {
  background-color: #f4f4f4; 
  font-family: Arial, sans-serif; 
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100vh; 
}

h1 {
  color: #333; 
}

form {
  background-color: #fff;
  padding: 20px;
  border-radius: 8px;
  box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
  margin-bottom: 20px;
  width: 300px; 
  display: flex;
  flex-direction: column;
  align-items: center;
}

input[type=submit],
input[type=range],
input[type=number] {
  width: 100%;
  padding: 15px;
  margin: 10px 0;
  display: inline-block;
  border: none;
  background: #45a049;
  color: #fff;
  border-radius: 15px;
  cursor: pointer;
  box-sizing: border-box;
}

input[type=submit]:hover {
  background-color: #4CAF50;
}

div {
  margin-bottom: 15px;
}

.slider {
  -webkit-appearance: none;
  height: 15px;
  background: #789bd6;
  outline: none;
  opacity: 1;
  -webkit-transition: .2s;
  transition: opacity .2s;
  border-radius: 4px;
}

.slider:hover {
  opacity: 1;
}

.percentage {
  color: #333;
  font-size: 10px;
  margin-top: 15px;
}

#currentTemperature {
  color: #333;
  font-size: 24px;
  font-weight: bold;
  margin-top: 20px;
}
  </style>
  <script src="https://code.jquery.com/jquery-3.6.4.min.js"></script>
  <script>
var speedValue;
    var rotationValue;

    function onSubmitSpeedForm() {
      sessionStorage.setItem('speedValue', speedValue);
      return true;
    }

    function onSpeedInput() {
      speedValue = document.getElementById('speed').value;
      updatePercentage('speed', 'speedPercentage');
    }

    function onSliderMouseDown(sliderId) {
      document.getElementById(sliderId + 'Percentage').style.display = 'block';
      updatePercentage(sliderId, sliderId + 'Percentage');
    }

    function onSliderMouseUp(sliderId) {
      document.getElementById(sliderId + 'Percentage').style.display = 'none';
    }

    function updatePercentage(sliderId, percentageId) {
      var percentage = document.getElementById(sliderId).value;
      document.getElementById(percentageId).innerHTML = percentage + '%';
    }

    function onSubmitAutoForm() {
      sessionStorage.setItem('targetTemp', document.getElementById('targetTemp').value);
      return true;
    }

    function getStoredTemp() {
      return sessionStorage.getItem('targetTemp') || '25';
    }

    function updateTemperature() {
      $.get("/temperature", function (data) {
        $("#currentTemperature").text("Temperatura curentă: " + data + " °C");
      });
    }

    $(document).ready(function () {
      updateTemperature();
      setInterval(updateTemperature, 2000);
    });
    
      function onSubmitPowerForm() {
    $.get("/power", function (data) {
      // Aici puteți adăuga logica suplimentară în funcție de răspunsul primit de la server
      console.log("Power state updated.");
    });
    return false; // Pentru a opri trimiterea formularului
  }
  
  </script>
</head>

<body>
  <h1>Control Ventilator</h1>

  <form action='/speed' method='GET' onsubmit='return onSubmitSpeedForm()'>
    <div>Viteza <input type='range' class='slider' min='0' max='100' name='speed' id='speed' value='50' oninput='onSpeedInput()' onmousedown='onSliderMouseDown("speed")' onmouseup='onSliderMouseUp("speed")' style="width: 300px; height: 45px">
      <div id='speedPercentage' class='percentage' style='display: none; '></div>
    </div>
    <input type='submit' value='Setează Viteza'>
  </form>

  <form action='/auto' method='GET' onsubmit='return onSubmitAutoForm()'>
    <div>Temperatura dorită: <input type='number' name='targetTemp' id='targetTemp' placeholder='21' required></div>
    <input type='submit' value='Setează Modul Auto'>
  </form>

  <form action='/power' method='GET' onsubmit='return onSubmitPowerForm()'>
    <input type='submit' value='Power On/Off'>
  </form>

  <div id="currentTemperature">Temperatura curentă: ... °C</div>
</body>

</html>
)";
    server.send(200, "text/html", htmlPage);
  });

  server.on("/speed", HTTP_GET, []() {
    targetSpeed = server.arg("speed").toInt();
    Serial.println("Viteza setată: " + String(targetSpeed));
    manualModeFlag = 1;

    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });

  server.on("/auto", HTTP_GET, []() {
    targetSpeed = server.arg("targetTemp").toInt();
    Serial.println("Temperatura dorită setată: " + String(targetSpeed));
    manualModeFlag = 0;

    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });

  server.on("/power", HTTP_GET, []() {
    powerState = (powerState == LOW) ? HIGH : LOW;
    digitalWrite(powerPin, powerState);
    Serial.println("Power state updated: " + String(powerState));

    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Redirecting...");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  double Vout, Rth, temperature, adc_value;

  adc_value = analogRead(temperatureInputPin);
  Vout = (adc_value * VCC) / adc_resolution;
  Rth = (Vout * R2) / (VCC - Vout);

  temperature = 1 / (A + B * log(Rth) + C * pow(log(Rth), 3));
  temperature = temperature - 273.15;

  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" degrees Celsius");

  if (manualModeFlag == 1) {
    analogWrite(motorPinOne, map(targetSpeed, 0, 100, 0, 255));
    analogWrite(motorPinTwo, 0);
  } else {
    double error = targetSpeed - temperature;
    static double integral = 0.0;
    static double previousError = 0.0;
    double Kp = 0.1;
    double Ki = 0.01;

    integral = integral + error;
    double derivative = error - previousError;

    double pidValue = Kp * error + Ki * integral + derivative;

    int automaticSpeed = constrain(map(pidValue, -100, 100, 0, 255), 0, 255);

    analogWrite(motorPinOne, automaticSpeed);
    analogWrite(motorPinTwo, 0);

    previousError = error;

    Serial.print("Temperature = ");
    Serial.print(temperature);
    Serial.print(" degrees Celsius | Target Speed = ");
    Serial.print(targetSpeed);
    Serial.print(" | Automatic Speed = ");
    Serial.println(automaticSpeed);
  }

  server.send(200, "text/plain", String(temperature));

  delay(500);
}
