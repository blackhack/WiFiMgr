/*
    WiFiMgr - ESP32 library to take care of WiFi connections/reconnections and NTP Time using its own thread

    Copyright(C) 2021 Blackhack <davidaristi.0504@gmail.com>

    This program is free software : you can redistribute it and /or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.If not, see < https://www.gnu.org/licenses/>.
*/

#include "WiFiMgr.h"

const char* ssid = "MY_SSID";
const char* password = "MY_PASSWORD";

const int32_t HOURS_TO_SECONDS = 3600;
const int32_t gmt_offset = -5 * HOURS_TO_SECONDS; // Time zone, example the east coast is -5 GMT
const int32_t daylight_offset = 1 * HOURS_TO_SECONDS; // Summer time offset if applies, some countries this is just 0

void setup()
{
    Serial.begin(115200);

    WiFiManager.begin(false, ssid, password);
    
    // By default pool.ntp.org is used as server 1, but it can be change and up to 3 servers can be use
    // WiFiManager.EnableTimeMgr(gmt_offset, daylight_offset);
    WiFiManager.EnableTimeMgr(gmt_offset, daylight_offset, "time.google.com", "time.cloudflare.com", "time.windows.com");
}

void loop()
{
    // All WiFiMgr logic runs in a separate thread or task, so this execution loop is pretty much unaffected
    // WiFiManager.GetTimeFormat() will return NN:NN:NN until a connection is established and time initialized,
    // so IsTimeAvailable() can be used to check when time is initialized, IsConnected() can be used to know when the wifi
    // is ready, but once time is initialized for the first occasion it can work without wifi.

    if (WiFiManager.IsTimeAvailable())
    {
        Serial.println(WiFiManager.GetTimeFormat("%A, %B %d %Y %H:%M:%S"));

        // Just to show that time is still working even after disconnection.
        if (WiFiManager.IsConnected())
            WiFiManager.Disconnect();
    }

    delay(1000);
}
