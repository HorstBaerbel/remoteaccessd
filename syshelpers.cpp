#include "syshelpers.h"

#include <iostream>
#include <regex>

std::string stem(const std::string &path)
{
    return stdfs::path(path).stem();
}

std::string extension(const std::string &path)
{
    return stdfs::path(path).extension();
}

bool systemCommand(const std::string &cmd)
{
    if (std::system(nullptr) != 0)
    {
        return std::system(cmd.c_str()) == 0;
    }
    else
    {
        std::cerr << "Command processor not available" << std::endl;
    }
    return false;
}

bool systemCommand(const std::string &cmd, std::string & result)
{
    if (std::system(nullptr) != 0)
    {
        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe)
        {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            {
                result += buffer.data();
            }
            return true;
        }
    }
    else
    {
        std::cerr << "Command processor not available" << std::endl;
    }
    result = "";
    return false;
}

std::string firstGroupMatch(const std::string s, const std::string regex)
{
    try
    {
        const std::regex re(regex);
        std::smatch sm;
        std::regex_search(s, sm, re);
        if (sm.size() >= 2)
        {
            return sm[1].str();
        }
    }
    catch (const std::regex_error &e)
    {
        std::cout << "std::regex error: " << e.what() << std::endl;
    }
    return "";
}

bool isWiFiAvailable()
{
    return !getWiFiDeviceName().empty();
}

std::string getWiFiDeviceName()
{
    // check if there's a IEEE 802.xx device
    std::string result;
    if (systemCommand("iwconfig | grep --color=never \"IEEE 802\"", result))
    {
        // extract the first portion of the string
        return firstGroupMatch(result, "^\\W*(\\w+)");
    }
    return "";
}

bool hasEthernetAddress(const std::string &deviceName)
{
    return !getEthernetAddress(deviceName).empty();
}

std::string getEthernetAddress(const std::string &deviceName)
{
    // check if wifi has a hardware adress -> means it is on the APs network
    std::string result;
    if (systemCommand("ip link | grep --color=never -A 1 " + deviceName, result))
    {
        return firstGroupMatch(result, deviceName + ".*\\W.*ether\\W*(([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})");
    }
    return "";
}

bool hasIPv4Address(const std::string &deviceName)
{
    return !getIPv4Address(deviceName).empty();
}

std::string getIPv4Address(const std::string &deviceName)
{
    // check if wifi has an IP adress -> means it is on the APs IP network
    std::string result;
    if (systemCommand("ip r | grep --color=never " + deviceName, result))
    {
        return firstGroupMatch(result, deviceName + ".*\\blink.*\\b(([0-9]{1,3}\\.){3}[0-9]{1,3})");
    }
    return "";
}

bool isFileContentSame(const stdfs::path &fileA, const stdfs::path &fileB)
{
    return systemCommand("diff \"" + fileA.string() + "\" \"" + fileB.string() + "\"");
}
