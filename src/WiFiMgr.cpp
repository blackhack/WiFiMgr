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
#include "time.h"
#include "esp_wifi.h"

WiFiMgr::WiFiMgr()
{
    _time_management = false;
    _time_initialized = false;

    // Other variables are initialized at .begin() and EnableTimeMgr()
}

bool WiFiMgr::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 /*= (uint32_t)0x00000000*/, IPAddress dns2 /*= (uint32_t)0x00000000*/)
{
    return WiFi.config(local_ip, gateway, subnet, dns1, dns2);
}

bool WiFiMgr::ap_config(IPAddress local_ip, IPAddress gateway, IPAddress subnet)
{
    return WiFi.softAPConfig(local_ip, gateway, subnet);
}

void WiFiMgr::begin(bool ap_mode, String ssid, String password)
{
    _running = true;
    _ap_mode = ap_mode;
    _ssid = ssid;
    _password = password;
    _state = WIFIMGR_INITIAL;

    Connect();

    if (!_ap_mode)
    {
        xTaskCreate(
            WiFiMgr::RunWrapper,
            "WiFiManager",
            3000,
            this,
            1,
            nullptr
        );
    }
}

void WiFiMgr::EnableTimeMgr(int32_t gmt_offset, int32_t daylight_offset, String ntp_server_1, String ntp_server_2, String ntp_server_3)
{
    _time_management = true;
    _gmt_offset = gmt_offset;
    _daylight_offset = daylight_offset;
    _ntp_server_1 = ntp_server_1;
    _ntp_server_2 = ntp_server_2;
    _ntp_server_3 = ntp_server_3;
}

void WiFiMgr::SetHostName(String hostname)
{
    WiFi.setHostname(hostname.c_str());
}

bool WiFiMgr::SetPowerSavingMode(wifi_ps_type_t powersaving_mode)
{
    return esp_wifi_set_ps(powersaving_mode) == ESP_OK;
}

void WiFiMgr::Disconnect()
{
    _running = false;
    _state = WIFIMGR_DISCONNECTED_REQUESTED;
    WiFi.disconnect();

    log_i("WiFi client mode disconnected as requested");
}

bool WiFiMgr::IsConnected()
{
    return WiFi.isConnected() && _state == WIFIMGR_CONNECTED;
}

String WiFiMgr::GetTimeFormat(String format)
{
    if (!_time_initialized)
        return "NN:NN:NN";

    tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        log_e("Failed to obtain time");
        return "NN:NN:NN";
    }

    char buf[64];
    size_t written = strftime(buf, 64, format.c_str(), &timeinfo);
    if (written == 0)
    {
        log_e("Failed to format time");
        return "NN:NN:NN";
    }

    return String(buf);
}

bool WiFiMgr::GetTimeInfo(tm* time_info)
{
    if (!_time_initialized)
        return false;

    if (!getLocalTime(time_info))
    {
        log_e("Failed to obtain time");
        return false;
    }

    return true;
}

void WiFiMgr::RunWrapper(void* parameter)
{
    static_cast<WiFiMgr*>(parameter)->Run();
}

void WiFiMgr::Run()
{
    uint32_t startUpdate = millis();
    uint32_t EndUpdate = startUpdate;
    uint32_t lastStartUpdate = startUpdate;

    while (_running)
    {
        startUpdate = millis();
        uint32_t diff = startUpdate - lastStartUpdate;
        lastStartUpdate = startUpdate;

        ReconnectAttempt(diff);
        TimeCheck(diff);

        EndUpdate = millis();
        uint32_t updateThreadTime = EndUpdate - startUpdate;

        if (updateThreadTime < sleep_timer_const)
            vTaskDelay((sleep_timer_const - updateThreadTime) / portTICK_PERIOD_MS);
    }

    vTaskDelete(nullptr);
}

void WiFiMgr::Connect()
{
    WiFi.disconnect();

    if (_ap_mode)
    {
        const char* ap_pass = _password.length() > 0 ? _password.c_str() : nullptr;
        WiFi.softAP(_ssid.c_str(), ap_pass);
        _state = WIFIMGR_SOFT_AP;
        log_i("WiFi SoftAP mode initialized, AP IP: %s", WiFi.softAPIP().toString());
    }
    else
    {
        log_i("WiFi client mode connection started");
        WiFi.begin(_ssid.c_str(), _password.c_str());
        _state = WIFIMGR_CONNECTING;
    }
}

void WiFiMgr::ReconnectAttempt(uint32_t diff)
{
    static uint32_t reconnect_timer = 1000;
    static uint32_t attempt_count = 1;

    if (!_running || WiFi.isConnected())
    {
        if (_state == WIFIMGR_CONNECTING)
        {
            log_i("WiFi connected at attempt %u, IP: %s", attempt_count, WiFi.localIP().toString());
            _state = WIFIMGR_CONNECTED;
        }

        reconnect_timer = 1000;
        attempt_count = 1;

        return;
    }

    if (_state == WIFIMGR_CONNECTED)
    {
        log_e("WiFi connection loss, attempting reconnection...");
        log_i("Internal status: %s(%i)", StatusToString(WiFi.status()).c_str(), WiFi.status());
        _state = WIFIMGR_DISCONNECTED_LOSS;
    }

    if (reconnect_timer <= diff)
    {
        if (_state == WIFIMGR_DISCONNECTED_LOSS || _state == WIFIMGR_INITIAL)
            Connect();

        log_i("Attempt number %u, internal status: %s(%i)", attempt_count, StatusToString(WiFi.status()).c_str(), WiFi.status());

        if (attempt_count % 10 == 0)
        {
            log_e("Can't connect after %u attempts, retrying...", attempt_count);
            Connect();
        }

        ++attempt_count;
        reconnect_timer = 1000;
    }
    else reconnect_timer -= diff;
}

void WiFiMgr::TimeCheck(uint32_t diff)
{
    static uint32_t time_initialization_timer = 1000;

    if (_time_initialized || !_time_management || !IsConnected())
        return;

    if (time_initialization_timer <= diff)
    {
        const char* server1 = _ntp_server_1.c_str();
        const char* server2 = _ntp_server_2.length() > 0 ? _ntp_server_2.c_str() : nullptr;
        const char* server3 = _ntp_server_3.length() > 0 ? _ntp_server_3.c_str() : nullptr;

        configTime(_gmt_offset, _daylight_offset, server1, server2, server3);

        tm timeinfo;
        if (!getLocalTime(&timeinfo))
            log_e("Failed to obtain time");
        else
        {
            char buf[64];
            size_t written = strftime(buf, 64, "%A, %B %d %Y %H:%M:%S", &timeinfo);
            if (written != 0)
                log_i("Time initialized at %s", buf);

            _time_initialized = true;
        }

        time_initialization_timer = 1000;
    }
    else time_initialization_timer -= diff;
}

void WiFiMgr::Reboot()
{
    log_w("Rebooting ESP32...");
    WiFi.disconnect();

    delay(1000);
    ESP.restart();
}

String WiFiMgr::StatusToString(wl_status_t status)
{
    switch (status)
    {
        case WL_NO_SHIELD: return "WL_NO_SHIELD";
        case WL_STOPPED: return "WL_STOPPED";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
    }

    return "WL_UNK";
}

WiFiMgr WiFiManager;
