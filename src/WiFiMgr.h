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

#ifndef _WIFIMANAGER_h
#define _WIFIMANAGER_h

#include "arduino.h"
#include <WiFi.h>
#include <atomic>

const uint32_t sleep_timer_const = 500;

class WiFiMgr
{
public:
    WiFiMgr();

    bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
    bool ap_config(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
    void begin(bool ap_mode, String ssid, String password);
    void EnableTimeMgr(int32_t gmt_offset, int32_t daylight_offset, String ntp_server_1 = "pool.ntp.org", String ntp_server_2 = "", String ntp_server_3 = "");
    void SetHostName(String hostname);
    bool SetPowerSavingMode(wifi_ps_type_t powersaving_mode);

    void Disconnect();
    bool IsConnected();

    String GetTimeFormat(String format);
    bool GetTimeInfo(tm *time_info);
    bool IsTimeAvailable() { return _time_initialized; }
private:
    static void RunWrapper(void* parameter);
    void Run();
    void Connect();
    void ReconnectAttempt(uint32_t diff);
    void TimeCheck(uint32_t diff);
    void Reboot();
    String StatusToString(wl_status_t status);

private:
    std::atomic_bool _running;
    bool _ap_mode;
    String _ssid;
    String _password;

    enum WIFIMGR_STATE
    {
        WIFIMGR_INITIAL,
        WIFIMGR_CONNECTING,
        WIFIMGR_CONNECTED,
        WIFIMGR_SOFT_AP,
        WIFIMGR_DISCONNECTED_REQUESTED,
        WIFIMGR_DISCONNECTED_LOSS,
    };
    int32_t _state;

    std::atomic_bool _time_management;
    std::atomic_bool _time_initialized;

    int32_t _gmt_offset;
    int32_t _daylight_offset;
    String _ntp_server_1;
    String _ntp_server_2;
    String _ntp_server_3;
};

extern WiFiMgr WiFiManager;

#endif
