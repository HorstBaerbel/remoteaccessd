// Raspbian WiFi remote access daemon
//
// Copyright (c) 2020 HorstBaerbel / Bim Overbohm / bim.overbohm@googlemail.com
//
// Inspired by posts here: https://stackoverflow.com/questions/28841139/how-to-get-coordinates-of-touchscreen-rawdata-using-linux?noredirect=1&lq=1
// And kernel docs here: https://www.kernel.org/doc/Documentation/input/input.txt
// See iwconfig here: http://manpages.ubuntu.com/manpages/trusty/man8/iwconfig.8.html
// Takes three arguments:
// The event input device to watch for key input.
// The directory to watch for a wpa_supplicant.conf file.
// The method used to toggle WiFi ("useOverlay" (same as "", default) or "useIwconfig").

#include "syshelpers.h"

#include <csignal>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#if defined(__GNUC__) || defined(__clang__)
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#endif

#define PLAY_AUDIO // Uncomment this to play audio when access is toggled or a wpa config file is found etc.
const std::string AUDIO_CMD = "aplay";
const std::string DATA_PATH = "/usr/local/share/remoteaccessd/";
const std::string WPA_CONFIG_FILENAME = "wpa_supplicant.conf";
const std::string WPA_CONFIG_DIRECTORY = "/etc/wpa_supplicant/";
const decltype(input_event::code) TOGGLE_KEYCODE = KEY_F12;
const std::string SERVICE_ENABLE_CMD = "systemctl enable";
const std::string SERVICE_DISABLE_CMD = "systemctl disable";
const std::string SERVICE_START_CMD = "systemctl start";
const std::string SERVICE_STOP_CMD = "systemctl stop";
const std::vector<std::string> SERVICES_TO_TOGGLE = {
    "ssh",
    "dhcpcd"};
constexpr std::chrono::milliseconds WIFI_TOGGLE_DURATION_MS(2000);
constexpr std::chrono::milliseconds WPS_START_DURATION_MS(5000);
constexpr std::chrono::milliseconds IGNORE_DURATION_MS(8000);
constexpr std::chrono::milliseconds POLL_TIMEOUT_MS(3000);

static bool quit = false;
static bool actionInProgress = false;

static void playWav(const std::string &fileName)
{
#ifdef PLAY_AUDIO
    const std::string cmd = AUDIO_CMD + " \"" + DATA_PATH + fileName + "\"";
    systemCommand(AUDIO_CMD);
#endif
}

static void toggleWiFiIwconfig(const std::string &wifiDeviceName, bool enable)
{
    std::cout << "Turning WiFi " << (enable ? "on" : "off") << std::endl;
    if (enable)
    {
        playWav("wifi_on.wav");
        // it seems this command has to be sent twice
        systemCommand("iwconfig " + wifiDeviceName + " txpower auto");
        systemCommand("iwconfig " + wifiDeviceName + " txpower auto");
        // turn wifi power saving off. otherwise the RPi will power down
        // WiFi after a couple of minutes unless an input device is plugged in...
        systemCommand("iwconfig " + wifiDeviceName + " power off");
    }
    else
    {
        playWav("wifi_off.wav");
        systemCommand("iwconfig " + wifiDeviceName + " power on");
        systemCommand("iwconfig " + wifiDeviceName + " txpower off");
    }
}

static bool toggleWiFiOverlay(const std::string &wifiDeviceName, bool enable)
{
    const auto configLine = systemCommandStdout("grep -F --color=never \"dtoverlay=disable-wifi\" /boot/config.txt");
    // check if state is already what we want
    if ((enable && ((!configLine.first) || (configLine.first && configLine.second == "#dtoverlay=disable-wifi"))) || (!enable && configLine.first && configLine.second == "dtoverlay=disable-wifi"))
    {
        std::cout << "WiFi already " << (enable ? "on" : "off") << std::endl;
        return false;
    }
    std::cout << "Turning WiFi " << (enable ? "on" : "off") << std::endl;
    if (enable)
    {
        playWav("wifi_on.wav");
        if (configLine.first)
        {
            // line found, replace line
            systemCommand(R"(sed -i "/#dtoverlay=disable-wifi/c\dtoverlay=disable-wifi" "/boot/config.txt")");
        }
        else
        {
            // line not found, append
            systemCommand("echo \"#dtoverlay=disable-wifi\" >> /boot/config.txt");
        }
        // turn wifi power saving off. otherwise the RPi will power down
        // WiFi after a couple of minutes unless an input device is plugged in...
        systemCommand("iwconfig " + wifiDeviceName + " power off");
    }
    else
    {
        playWav("wifi_off.wav");
        systemCommand("iwconfig " + wifiDeviceName + " power on");
        if (configLine.first)
        {
            // line found, replace line
            systemCommand(R"(sed -i "/dtoverlay=disable-wifi/c\#dtoverlay=disable-wifi" /boot/config.txt)");
        }
        else
        {
            // line not found, append
            systemCommand("echo \"#dtoverlay=disable-wifi\" >> /boot/config.txt");
        }
    }
    return true;
}

static void startStopServices(bool start)
{
    std::cout << (start ? "Starting" : "Stopping") << " service ";
    for (const auto &s : SERVICES_TO_TOGGLE)
    {
        std::cout << s << " ";
        systemCommand((start ? SERVICE_START_CMD : SERVICE_STOP_CMD) + " " + s);
    }
    std::cout << std::endl;
}

static void enableDisableServices(bool enable)
{
    std::cout << (enable ? "Enabling" : "Disabling") << " service ";
    for (const auto &s : SERVICES_TO_TOGGLE)
    {
        std::cout << s << " ";
        systemCommand((enable ? SERVICE_ENABLE_CMD : SERVICE_DISABLE_CMD) + " " + s);
    }
    std::cout << std::endl;
}

static void toggleRemoteAccess(bool useOverlay)
{
    // make sure we're not doing anything while or possibly after toggling access
    if (actionInProgress)
    {
        return;
    }
    actionInProgress = true;
    // get WiFi device name
    const std::string wifiDeviceName = getWiFiDeviceName();
    if (wifiDeviceName.empty())
    {
        std::cerr << "Failed to find WiFi device name" << std::endl;
        actionInProgress = false;
        return;
    }
    // toggle WiFi and services on/off
    bool mustReboot = false;
    if (useOverlay)
    {
        const bool targetState = !isWiFiAvailable();
        mustReboot = toggleWiFiOverlay(wifiDeviceName, targetState);
        // we have to enable the services to be active after a reboot
        enableDisableServices(targetState);
        // if we do not have to reboot now, we can also just start or stop the services
        if (!mustReboot)
        {
            startStopServices(targetState);
        }
    }
    else
    {
        const bool targetState = !hasEthernetAddress(wifiDeviceName);
        toggleWiFiIwconfig(wifiDeviceName, targetState);
        startStopServices(targetState);
    }
    // reboot if we must
    if (mustReboot)
    {
        std::cout << "Rebooting..." << std::endl;
        playWav("rebooting.wav");
        systemCommand("reboot");
    }
    else
    {
        actionInProgress = false;
    }
}

static void startWPSConnection(bool useOverlay)
{
    // make sure we're not doing anything while connecting via WPS
    if (actionInProgress)
    {
        return;
    }
    actionInProgress = true;
    // get WiFi device name
    const std::string wifiDeviceName = getWiFiDeviceName();
    if (wifiDeviceName.empty())
    {
        std::cerr << "Failed to find WiFi device name. Enabling WiFi" << std::endl;
        actionInProgress = false;
        toggleRemoteAccess(useOverlay);
        return;
    }
    actionInProgress = true;
    if (hasIPv4Address(wifiDeviceName))
    {
        std::cout << "WiFi already connected" << std::endl;
        actionInProgress = false;
        return;
    }
    std::cout << "Starting WPS connection..." << std::endl;
    // check if WPA config includes "update_config=1"
    if (!systemCommand("grep -i \"update_config=1\" " + WPA_CONFIG_DIRECTORY + WPA_CONFIG_FILENAME))
    {
        // stop wpa_supplicant, update config and restart
        systemCommand("killall -q wpa_supplicant");
        sleep(1);
        systemCommand("echo \"update_config=1\" >> " + WPA_CONFIG_DIRECTORY + WPA_CONFIG_FILENAME);
        systemCommand("wpa_supplicant -B");
        sleep(3);
    }
    // clear all stored networks from list
    systemCommand("for i in \"wpa_cli -i" + wifiDeviceName + " list_networks | grep ^[0-9] | cut -f1\"; do wpa_cli -i" + wifiDeviceName + " remove_network $i; done");
    // list all routers supporting WPS sorted by signal strength and extract first line
    const auto scanResult = systemCommandStdout("wpa_cli -i" + wifiDeviceName + R"( scan_results | grep "WPS" | sort -r -k3 | sed -n "1p")");
    if (scanResult.first && !scanResult.second.empty())
    {
        const auto bssid = systemCommandStdout("echo \" " + scanResult.second + R"( | sed -n "s/^\W*\([0-9a-fA-F:]\+\)\b.*/\1/p")");
        const auto ssid = systemCommandStdout("echo \" " + scanResult.second + R"( | sed -n "s/.*\b\(\w\+\)\W*$/\1/p")");
        if (!bssid.second.empty() && !ssid.second.empty())
        {
            // try to connect
            std::cout << "Connecting to " << ssid.second << "(" << bssid.second << ")" << std::endl;
            playWav("wps_started.wav");
            if (systemCommand("wpa_cli -i" + wifiDeviceName + " wps_pbc " + bssid.second))
            {
                // connecting seemed to work, wait a bit and check .conf file
                sleep(10);
                const bool configHasNetwork = systemCommand("grep -i \"^network=\" " + WPA_CONFIG_DIRECTORY + WPA_CONFIG_FILENAME);
                const auto configModifiedAgoS = systemCommandStdout(R"($(($(date +"%s") - $(stat -c "%Y" )" + WPA_CONFIG_DIRECTORY + WPA_CONFIG_FILENAME + ")))");
                if (configHasNetwork && !configModifiedAgoS.second.empty() && std::stoi(configModifiedAgoS.second) < 13)
                {
                    // stop wpa_supplicant, restart with new config
                    /*systemCommand("killall -q wpa_supplicant");
                            sleep(1);
                            systemCommand("wpa_action " + wifiDeviceName + " stop");
                            systemCommand("wpa_action " + wifiDeviceName + " reload");
                            sleep(3);*/
                    std::cout << "Connected to " << ssid.second << "(" << bssid.second << "). wpa_supplicant.conf updated" << std::endl;
                    playWav("succeded.wav");
                }
            }
            else
            {
                std::cerr << "Failed to connect to access point" << std::endl;
                playWav("failed.wav");
            }
        }
    }
    else
    {
        std::cerr << "Failed to find WPS-enabled WiFi access points" << std::endl;
        playWav("failed.wav");
    }
    actionInProgress = false;
}

static void copyConfigFile(const stdfs::path &filePath, const stdfs::path &destDir)
{
    // make sure we're not doing anything while or possibly after copying the file
    if (actionInProgress)
    {
        return;
    }
    actionInProgress = true;
    const auto destPath = destDir / filePath.filename();
    if (isFileContentSame(filePath, destPath))
    {
        std::cout << "File in " << filePath << " is the same as " << destPath << std::endl;
        actionInProgress = false;
    }
    else
    {
        std::cout << "Copying " << filePath << " to " << destPath << std::endl;
        try
        {
            stdfs::copy(filePath, destPath, stdfs::copy_options::overwrite_existing);
            systemCommand("chmod 600 \"" + destPath.string() + "\"");
            playWav("wpa_updated.wav");
            std::cout << "Rebooting..." << std::endl;
            playWav("rebooting.wav");
            systemCommand("reboot");
        }
        catch (const stdfs::filesystem_error &e)
        {
            std::cout << "Copying failed: " << e.what() << std::endl;
            actionInProgress = false;
        }
    }
}

/*static void eventToStdout(const input_event &ev)
{
    std::cout << "Event:" << std::endl;
    std::cout << "Time: " << ev.time.tv_sec << "." << ev.time.tv_usec << "s, ";
    std::cout << "Type: " << ev.type << ", code: " << ev.code;
    std::cout << ", value: " << ev.value << std::endl;
}*/

static void signalHandler(int signum)
{
    std::cout << "Signal received: " << signum << ". Quitting..." << std::endl;
    quit = true;
}

auto main(int argc, char *argv[]) -> int
{
    int returnValue = 0;
    pollfd inputDevice = {0, 0, 0};
    std::array<input_event, 64> events{};
    auto buttonPressStart = std::chrono::system_clock::now();
    bool dirExists = false;
    bool toggleWiFiByOverlay = true;
    try
    {
        if ((getuid()) != 0)
        {
            std::cerr << "Must be run as root!" << std::endl;
            return 4;
        }
        if (argc < 3 || argc > 4)
        {
            std::cerr << "Must specify input device, watch directory and optionally WiFi toggle mode, e.g." << std::endl;
            std::cerr << "e.g. remoteaccessd /dev/input/event2 /media/usb/ useOverlay" << std::endl;
            return 2;
        }
        // check which method toggle WiFi with
        if (argc == 4)
        {
            const std::string argv3(argv[3]);
            if (argv3 == "useIwconfig")
            {
                toggleWiFiByOverlay = false;
            }
            else if (argv3 == "useOverlay")
            {
                toggleWiFiByOverlay = true;
            }
            else
            {
                std::cerr << "Unknown WiFi toggle mode \"" << argv3 << R"(". Use "useIwconfig" or "useOverlay")" << std::endl;
                return 2;
            }
        }
        // open input device for reading
        const std::string keyDevice = argv[1];
        inputDevice.fd = open(keyDevice.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (inputDevice.fd < 0)
        {
            std::cerr << "Failed to open \"" << keyDevice << "\" for reading" << std::endl;
            return 1;
        }
        std::cout << "Opened \"" << keyDevice << "\" for reading" << std::endl;
        // get input device name
        std::array<char, 512> inputDeviceName{};
        if (ioctl(inputDevice.fd, EVIOCGNAME(sizeof(inputDeviceName)), inputDeviceName.data()) >= 0)
        {
            std::cout << "Device name: \"" << inputDeviceName.data() << "\"" << std::endl;
        }
        // check watch directory
        const std::string usbDirectory = argv[2];
        const auto watchDir = stdfs::path(usbDirectory);
        std::cout << "Watching directory \"" << usbDirectory << "\" for " << WPA_CONFIG_FILENAME << std::endl;
        // alright. ready to go. register signal handler so can quit when asked to
        if (signal(SIGINT, signalHandler) == SIG_IGN)
        {
            signal(SIGINT, SIG_IGN);
        }
        if (signal(SIGHUP, signalHandler) == SIG_IGN)
        {
            signal(SIGHUP, SIG_IGN);
        }
        if (signal(SIGTERM, signalHandler) == SIG_IGN)
        {
            signal(SIGTERM, SIG_IGN);
        }
        // run event loop
        while (!quit)
        {
            // poll input device for events
            inputDevice.events = POLLIN;
            if (poll(&inputDevice, 1, POLL_TIMEOUT_MS.count()) > 0)
            {
                if (inputDevice.revents != 0)
                {
                    const auto nrOfBytesRead = read(inputDevice.fd, events.data(), sizeof(events));
                    if (nrOfBytesRead < 0)
                    {
                        std::cerr << "Input device read failed: " << nrOfBytesRead << std::endl;
                    }
                    else
                    {
                        //std::cout << nrOfBytesRead << " bytes read" << std::endl;
                        // check if we have enough data for a complete event
                        if (nrOfBytesRead >= static_cast<ssize_t>(sizeof(input_event)))
                        {
                            uint32_t eventIndex = 0;
                            ssize_t byteIndex = 0;
                            while (eventIndex < 64 && byteIndex < nrOfBytesRead)
                            {
                                const auto &ev = events[eventIndex];
                                //eventToStdout(ev);
                                if (ev.type == EV_KEY && ev.code == TOGGLE_KEYCODE)
                                {
                                    if (ev.value == 1)
                                    {
                                        buttonPressStart = std::chrono::system_clock::now();
                                    }
                                    else if (ev.value == 0)
                                    {
                                        auto pressDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - buttonPressStart);
                                        if (pressDuration >= WIFI_TOGGLE_DURATION_MS && pressDuration < WPS_START_DURATION_MS)
                                        {
                                            toggleRemoteAccess(toggleWiFiByOverlay);
                                        }
                                        else if (pressDuration >= WPS_START_DURATION_MS && pressDuration < IGNORE_DURATION_MS)
                                        {
                                            startWPSConnection(toggleWiFiByOverlay);
                                        }
                                    }
                                }
                                // skip to next event
                                byteIndex += sizeof(input_event);
                                eventIndex++;
                            }
                        }
                    }
                }
                else
                {
                    std::cerr << "Error polling input device" << std::endl;
                }
            } // else an error or poll timeout occurred, so no events arrived
            // now check the directory for a wpa_supplicant.conf file
            try
            {
                // check if the directory is accessible
                if (stdfs::exists(watchDir) && stdfs::is_directory(watchDir) && stdfs::directory_iterator(watchDir) != stdfs::directory_iterator())
                {
                    // check if it was already accessible before
                    if (!dirExists)
                    {
                        // no. this means this was just inserted or we're checking for the first time
                        std::cout << "New content found in " << watchDir << std::endl;
                        // list the files and find the wpa_supplicant.conf
                        dirExists = true;
                        auto dirIt = stdfs::directory_iterator(watchDir);
                        while (dirIt != stdfs::directory_iterator())
                        {
                            auto path = dirIt->path();
                            if (stdfs::is_regular_file(path) && path.filename() == WPA_CONFIG_FILENAME)
                            {
                                copyConfigFile(path, WPA_CONFIG_DIRECTORY);
                                break;
                            }
                            ++dirIt;
                        }
                    }
                }
                else
                {
                    dirExists = false;
                }
            }
            catch (const stdfs::filesystem_error & /*e*/)
            {
                dirExists = false;
            }
        }
    }
    catch (const std::runtime_error & /*e*/)
    {
        returnValue = 1;
    }
    close(inputDevice.fd);
    return returnValue;
}
