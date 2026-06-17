#include "ESP/web_server.h"
#include <Arduino.h>

AsyncWebServer server(80);

const char index_html[] = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Balancing Cube Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <h1>Cube Dashboard</h1>
  <p>Proportional (P): <input type="range" min="0" max="100" value="50"></p>
  <button>Emergency Stop</button>
</body>
</html>
)rawliteral";

void init_web_server() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });
  server.begin();
  Serial.println("Web server initialized.");
}
