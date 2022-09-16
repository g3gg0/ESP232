#include <WebServer.h>
#include <ESP32httpUpdate.h>
#include "Config.h"

#define xstr(s) str(s)
#define str(s) #s

WebServer webserver(80);
extern char wifi_error[];
extern bool wifi_captive;
int www_wifi_scanned = -1;
int www_last_captive = 0;

#define min(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define max(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

void www_setup()
{
    webserver.on("/", handle_root);
    webserver.on("/index.html", handle_index);
    webserver.on("/set_parm", handle_set_parm);
    webserver.on("/ota", handle_ota);
    webserver.on("/dbg", handle_dbg);
    webserver.on("/reset", handle_reset);
    webserver.on("/test", handle_test);
    webserver.onNotFound(handle_404);

    webserver.begin();
    // Serial.println("HTTP server started");

    if (!MDNS.begin(current_config.hostname))
    {
        Serial.println("Error setting up MDNS responder!");
        while (1)
        {
            delay(1000);
        }
    }
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("telnet", "tcp", 23);
}

unsigned char h2int(char c)
{
    if (c >= '0' && c <= '9')
    {
        return ((unsigned char)c - '0');
    }
    if (c >= 'a' && c <= 'f')
    {
        return ((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F')
    {
        return ((unsigned char)c - 'A' + 10);
    }
    return (0);
}

String urldecode(String str)
{
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++)
    {
        c = str.charAt(i);
        if (c == '+')
        {
            encodedString += ' ';
        }
        else if (c == '%')
        {
            i++;
            code0 = str.charAt(i);
            i++;
            code1 = str.charAt(i);
            c = (h2int(code0) << 4) | h2int(code1);
            encodedString += c;
        }
        else
        {
            encodedString += c;
        }

        yield();
    }

    return encodedString;
}

void www_activity()
{
    if (wifi_captive)
    {
        www_last_captive = millis();
    }
}

int www_is_captive_active()
{
    if (wifi_captive && millis() - www_last_captive < 30000)
    {
        return 1;
    }
    return 0;
}

void handle_404()
{
    www_activity();

    if (wifi_captive)
    {
        char buf[128];
        sprintf(buf, "HTTP/1.1 302 Found\r\nContent-Type: text/html\r\nContent-length: 0\r\nLocation: http://%s/\r\n\r\n", WiFi.softAPIP().toString().c_str());
        webserver.sendContent(buf);
        // Serial.printf("[WWW] 302 - http://%s%s/ -> http://%s/\n", webserver.hostHeader().c_str(), webserver.uri().c_str(), WiFi.softAPIP().toString().c_str());
    }
    else
    {
        webserver.send(404, "text/plain", "So empty here");
        // Serial.printf("[WWW] 404 - http://%s%s/\n", webserver.hostHeader().c_str(), webserver.uri().c_str());
    }
}

void handle_index()
{
    webserver.send(200, "text/html", SendHTML());
}

bool www_loop()
{
    webserver.handleClient();
    return false;
}

void handle_root()
{
    webserver.send(200, "text/html", SendHTML());
}

void handle_ota()
{
    ota_setup();
    webserver.send(200, "text/html", SendHTML());
}

void handle_dbg()
{
    uart_flush(uart_num);
    webserver.send(200, "text/html", "OK");
}

void handle_reset()
{
    webserver.send(200, "text/html", SendHTML());
    ESP.restart();
}

void handle_test()
{
    // Serial.printf("Test\n");
    webserver.send(200, "text/html", SendHTML());
}

void handle_set_parm()
{
    if (webserver.arg("http_download") != "" && webserver.arg("http_name") != "")
    {
        String url = webserver.arg("http_download");
        String filename = webserver.arg("http_name");
        HTTPClient http;

        http.begin(url);

        int httpCode = http.GET();

        // Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        switch (httpCode)
        {
            case HTTP_CODE_OK:
            {
                int len = http.getSize();
                const int blocksize = 1024;
                uint8_t *buffer = (uint8_t *)malloc(blocksize);

                if (!buffer)
                {
                    // Serial.printf("[HTTP] Failed to alloc %d byte\n", blocksize);
                    return;
                }

                WiFiClient *stream = http.getStreamPtr();
                File file = SPIFFS.open("/" + filename, "w");

                if (!file)
                {
                    // Serial.printf("[HTTP] Failed to open file\n", blocksize);
                    return;
                }

                int written = 0;

                while (http.connected() && (written < len))
                {
                    size_t size = stream->available();

                    if (size)
                    {
                        int c = stream->readBytes(buffer, ((size > blocksize) ? blocksize : size));

                        if (c > 0)
                        {
                            file.write(buffer, c);
                            written += c;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                free(buffer);
                file.close();

                // Serial.printf("[HTTP] Finished. Wrote %d byte to %s\n", written, filename.c_str());
                webserver.send(200, "text/plain", "Downloaded " + url + " and wrote " + written + " byte to " + filename);
                break;
            }

            default:
            {
                // Serial.print("[HTTP] unexpected response\n");
                webserver.send(200, "text/plain", "Unexpected HTTP status code " + httpCode);
                break;
            }
        }

        return;
    }

    if (webserver.arg("http_update") != "")
    {
        String url = webserver.arg("http_update");

        Serial.printf("Update from %s\n", url.c_str());

        ESPhttpUpdate.rebootOnUpdate(false);
        t_httpUpdate_return ret = ESPhttpUpdate.update(url);

        switch (ret)
        {
            case HTTP_UPDATE_FAILED:
                webserver.send(200, "text/plain", "HTTP_UPDATE_FAILED while updating from " + url + " " + ESPhttpUpdate.getLastErrorString());
                // Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;

            case HTTP_UPDATE_NO_UPDATES:
                webserver.send(200, "text/plain", "HTTP_UPDATE_NO_UPDATES: Updating from " + url);
                // Serial.println("Update failed: HTTP_UPDATE_NO_UPDATES");
                break;

            case HTTP_UPDATE_OK:
                webserver.send(200, "text/html", "<html><head><meta http-equiv=\"Refresh\" content=\"5; url=/\"/></head><body><h1>Firmware updated. Rebooting...</h1>(will refresh page in 5 seconds)</body></html>");
                webserver.close();
                // Serial.println("Update successful");
                delay(500);
                ESP.restart();
                return;
        }

        return;
    }
    char hostname[32];
    strncpy(hostname, webserver.arg("hostname").c_str(), 32);

    if (strlen(hostname) > 0 && strlen(hostname) < 31)
    {
        strcpy(current_config.hostname, hostname);
    }

    int baud = atoi(webserver.arg("baud").c_str());
    current_config.baudrate = max(1200, min(1000000, baud));

    int verbose = atoi(webserver.arg("verbose").c_str());
    current_config.verbose = 0;
    current_config.verbose |= (webserver.arg("verbose_c0") != "") ? 1 : 0;
    current_config.verbose |= (webserver.arg("verbose_c1") != "") ? 2 : 0;
    current_config.verbose |= (webserver.arg("verbose_c2") != "") ? 4 : 0;
    current_config.verbose |= (webserver.arg("verbose_c3") != "") ? 8 : 0;

    strncpy(current_config.connect_string, webserver.arg("conn_string").c_str(), 127);
    strncpy(current_config.disconnect_string, webserver.arg("disconn_string").c_str(), 127);
    strncpy(current_config.wifi_ssid, webserver.arg("wifi_ssid").c_str(), sizeof(current_config.wifi_ssid));
    strncpy(current_config.wifi_password, webserver.arg("wifi_password").c_str(), sizeof(current_config.wifi_password));

    cfg_save();
    serial_setup();

    if (current_config.verbose)
    {
        // Serial.printf("[Webserver] '%s' %d %d '%s' '%s'\n", current_config.hostname, current_config.baudrate, current_config.verbose, current_config.connect_string, current_config.disconnect_string);
    }



    if (webserver.arg("reboot") == "true")
    {
        webserver.send(200, "text/html", "<html><head><meta http-equiv=\"Refresh\" content=\"5; url=/index.html\"/></head><body><h1>Saved. Rebooting...</h1>(will refresh page in 5 seconds)</body></html>");
        delay(500);
        ESP.restart();
        return;
    }

    if (webserver.arg("scan") == "true")
    {
        www_wifi_scanned = WiFi.scanNetworks();
    }
    webserver.send(200, "text/html", SendHTML());
    www_wifi_scanned = -1;
}

void handle_NotFound()
{
    webserver.send(404, "text/plain", "Not found");
}

String SendHTML()
{
    char buf[1024];

    www_activity();

    String ptr = "<!DOCTYPE html> <html>\n";
    ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";

    sprintf(buf, "<title>ESP232 '%s' Control</title>\n", current_config.hostname);

    ptr += buf;
    ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
    ptr += "input{font-family: Consolas,Monaco,Lucida Console,Liberation Mono,DejaVu Sans Mono,Bitstream Vera Sans Mono,Courier New, monospace; }\n";
    ptr += "tr:nth-child(odd) {background: #CCC} tr:nth-child(even) {background: #FFF}\n";
    ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
    ptr += ".button-on {background-color: #3498db;}\n";
    ptr += ".button-on:active {background-color: #2980b9;}\n";
    ptr += ".button-off {background-color: #34495e;}\n";
    ptr += ".button-off:active {background-color: #2c3e50;}\n";
    ptr += ".toggle-buttons input[type=\"radio\"] {visibility: hidden;}\n";
    ptr += ".toggle-buttons label { border: 1px solid #333; border-radius: 0.5em; padding: 0.3em; } \n";
    ptr += ".toggle-buttons input:checked + label { background: #40ff40; color: #116600; box-shadow: none; }\n";
    ptr += ".check-buttons input[type=\"checkbox\"] {visibility: hidden;}\n";
    ptr += ".check-buttons label { border: 1px solid #333; border-radius: 0.5em; padding: 0.3em; } \n";
    ptr += ".check-buttons input:checked + label { background: #40ff40; color: #116600; box-shadow: none; }\n";
    ptr += "input:hover + label, input:focus + label { background: #ff4040; } \n";
    ptr += ".together { position: relative; } \n";
    ptr += ".together input { position: absolute; width: 1px; height: 1px; top: 0; left: 0; } \n";
    ptr += ".together label { margin: 0.5em 0; border-radius: 0; } \n";
    ptr += ".together label:first-of-type { border-radius: 0.5em 0 0 0.5em; } \n";
    ptr += ".together label:last-of-type { border-radius: 0 0.5em 0.5em 0; } \n";
    ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
    ptr += "td {padding: 0.3em}\n";
    ptr += "</style>\n";
    ptr += "</head>\n";
    ptr += "<body>\n";

    sprintf(buf, "<h1>ESP232 - %s</h1>\n", current_config.hostname);
    ptr += buf;

    sprintf(buf, "<h3>v1." xstr(PIO_SRC_REVNUM) " - " xstr(PIO_SRC_REV) "</h3>\n");
    ptr += buf;

    if (strlen(wifi_error) != 0)
    {
        sprintf(buf, "<h2>WiFi Error: %s</h2>\n", wifi_error);
        ptr += buf;
    }

    if (!ota_enabled())
    {
        ptr += "<a href=\"/ota\">[Enable OTA]</a> ";
    }
    sprintf(buf, "<br>\n");
    ptr += buf;
    ptr += "<br><br>\n";

    ptr += "<form id=\"config\" action=\"/set_parm\">\n";
    ptr += "<table>";

#define ADD_CONFIG(name, value, fmt, desc)                                                                                \
    do                                                                                                                    \
    {                                                                                                                     \
        ptr += "<tr><td><label for=\"" name "\">" desc ":</label></td>";                                                  \
        sprintf(buf, "<td><input type=\"text\" id=\"" name "\" name=\"" name "\" value=\"" fmt "\"></td></tr>\n", value); \
        ptr += buf;                                                                                                       \
    } while (0)

#define ADD_CONFIG_CHECK4(name, value, fmt, desc, text0, text1, text2, text3)                                                             \
    do                                                                                                                                    \
    {                                                                                                                                     \
        ptr += "<tr><td>" desc ":</td><td><div class=\"check-buttons together\">";                                                        \
        sprintf(buf, "<input type=\"checkbox\" id=\"" name "_c0\" name=\"" name "_c0\" value=\"1\" %s>\n", (value & 1) ? "checked" : ""); \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<label for=\"" name "_c0\">" text0 "</label>\n");                                                                   \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<input type=\"checkbox\" id=\"" name "_c1\" name=\"" name "_c1\" value=\"1\" %s>\n", (value & 2) ? "checked" : ""); \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<label for=\"" name "_c1\">" text1 "</label>\n");                                                                   \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<input type=\"checkbox\" id=\"" name "_c2\" name=\"" name "_c2\" value=\"1\" %s>\n", (value & 4) ? "checked" : ""); \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<label for=\"" name "_c2\">" text2 "</label>\n");                                                                   \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<input type=\"checkbox\" id=\"" name "_c3\" name=\"" name "_c3\" value=\"1\" %s>\n", (value & 8) ? "checked" : ""); \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "<label for=\"" name "_c3\">" text3 "</label>\n");                                                                   \
        ptr += buf;                                                                                                                       \
        sprintf(buf, "</div></td></tr>\n");                                                                                               \
        ptr += buf;                                                                                                                       \
    } while (0)

#define ADD_CONFIG_COLOR(name, value, fmt, desc)                                                                                       \
    do                                                                                                                                 \
    {                                                                                                                                  \
        ptr += "<tr><td><label for=\"" name "\">" desc ":</label></td>";                                                               \
        sprintf(buf, "<td><input type=\"text\" id=\"" name "\" name=\"" name "\" value=\"" fmt "\" data-coloris></td></tr>\n", value); \
        ptr += buf;                                                                                                                    \
    } while (0)

    ADD_CONFIG("hostname", current_config.hostname, "%s", "Hostname");
    ADD_CONFIG("wifi_ssid", current_config.wifi_ssid, "%s", "WiFi SSID");
    ADD_CONFIG("wifi_password", current_config.wifi_password, "%s", "WiFi Password");

    ptr += "<tr><td>WiFi networks:</td><td>";

    if (www_wifi_scanned == -1)
    {
        ptr += "<button type=\"submit\" name=\"scan\" value=\"true\">Scan WiFi</button>";
    }
    else if (www_wifi_scanned == 0)
    {
        ptr += "No networks found, <button type=\"submit\" name=\"scan\" value=\"true\">Rescan WiFi</button>";
    }
    else
    {
        ptr += "<table>";
        ptr += "<tr><td><button type=\"submit\" name=\"scan\" value=\"true\">Rescan WiFi</button></td></tr>";
        for (int i = 0; i < www_wifi_scanned; ++i)
        {
            if (WiFi.SSID(i) != "")
            {
                ptr += "<tr><td align=\"left\"><tt><a href=\"javascript:void(0);\" onclick=\"document.getElementById('wifi_ssid').value = '";
                ptr += WiFi.SSID(i);
                ptr += "'\">";
                ptr += WiFi.SSID(i);
                ptr += "</a></tt></td><td align=\"left\"><tt>";
                ptr += WiFi.RSSI(i);
                ptr += " dBm</tt></td></tr>";
            }
        }
        ptr += "</table>";
    }

    ptr += "</td></tr>";

    ADD_CONFIG("baud", current_config.baudrate, "%d", "Baudrate");
    ADD_CONFIG("baud", current_config.connect_string, "%s", "Connect Message");
    ADD_CONFIG("baud", current_config.disconnect_string, "%s", "Disconnect Message");
    ADD_CONFIG_CHECK4("verbose", current_config.verbose, "%d", "Verbosity", "Serial", "UDP", "_", "_");

    ADD_CONFIG("http_update", "", "%s", "Update URL");

    ptr += "<td></td><td><input type=\"submit\" value=\"Save\"><button type=\"submit\" name=\"reboot\" value=\"true\">Save &amp; Reboot</button></td></table></form>\n";

    ptr += "</body>\n";
    ptr += "</html>\n";
    return ptr;
}
