#include <WebServer.h>
WebServer webserver(80);


void www_setup()
{
  webserver.on("/", handle_OnConnect);
  webserver.on("/set_parm", handle_set_parm);
  webserver.on("/ota", handle_ota);
  webserver.onNotFound(handle_NotFound);
  
  webserver.begin();
  Serial.println("HTTP server started");
  
  if (!MDNS.begin(current_config.hostname)) {
      Serial.println("Error setting up MDNS responder!");
      while(1) {
          delay(1000);
      }
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("telnet", "tcp", 23);
}

bool www_loop()
{
  webserver.handleClient();
  return false;
}

void handle_OnConnect()
{
  webserver.send(200, "text/html", SendHTML()); 
}

void handle_ota()
{
  ota_setup();
  webserver.send(200, "text/html", SendHTML()); 
}

void handle_set_parm()
{
  char hostname[32];
  strncpy(hostname, webserver.arg("hostname").c_str(), 32);
  
  if(strlen(hostname) > 0 && strlen(hostname) < 31)
  {
    strcpy(current_config.hostname, hostname);
  }
  
  int baud = atoi(webserver.arg("baud").c_str());
  current_config.baudrate = max(1200, min(1000000, baud));
  
  save_cfg();
  serial_setup();
  webserver.send(200, "text/html", SendHTML()); 
}

void handle_NotFound()
{
  webserver.send(404, "text/plain", "Not found");
}

String SendHTML()
{
  char buf[128];
  
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  sprintf(buf, "<title>ESP232 '%s' Control</title>\n", current_config.hostname);
  ptr += buf;
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  
  sprintf(buf, "<h1>ESP232 - %s</h1>\n", current_config.hostname);
  ptr += buf;
  if(!ota_enabled)
  {
    ptr +="<a href=\"/ota\">[Enable OTA]</a> ";
  }
  if(config_mode)
  {
    ptr +="--- config mode enabled ---";
  }
  ptr +="<br><br>\n";


  ptr +="<form action=\"/set_parm\">\n";
  ptr += "<table>";
  
  ptr +="<tr><td><label for=\"hostname\">Name:</label></td>";
  sprintf(buf, "<td><input type=\"text\" id=\"hostname\" name=\"hostname\" value=\"%s\"></td></tr>\n", current_config.hostname);
  ptr += buf;
  ptr +="<tr><td><label for=\"baud\">Baud:</label></td>";
  sprintf(buf, "<td><input type=\"text\" id=\"baud\" name=\"baud\" value=\"%d\"></td></tr>\n", current_config.baudrate);
  ptr += buf;
  ptr +="<td></td><td><input type=\"submit\" value=\"Write\"></td></table></form>\n";

  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
