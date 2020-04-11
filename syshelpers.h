// Linux system helper utilities. Should maybe be in a seperate repo...
#pragma once

#include <string>

#if defined(__GNUC__) || defined(__clang__)
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#endif

/// @brief Get stem or basename from path, e.g. "/foo/bar.txt" -> "bar".
std::string stem(const std::string &path);
/// @brief Get extension from path, e.g. "/foo/bar.txt" -> ".txt".
std::string extension(const std::string &path);

/// @brief Run system command. Will return true if command was sucessfully run.
bool systemCommand(const std::string &cmd);
/// @brief Run system command an return result from stdout. Will return <true, ...> if command was sucessfully run.
std::pair<bool, std::string> systemCommandStdout(const std::string &cmd);

/// @brief Search group in s using regular expression regex and return first group match (not the whole match).
/// e.g. firstGroupMatch("blat.txt", "(\\w+)\\..*") -> "bla" which is the first group "(\\w+)".
std::string firstGroupMatch(const std::string &s, const std::string &regex);

/// @brief Returns true if a WiFi device can be found in the system.
bool isWiFiAvailable();
/// @brief Get name of first WiFi device.
std::string getWiFiDeviceName();

/// @brief Returns true if the device has an ethernet adress assigned.
/// This means it is in the network, e.g. on an AP of a WiFi network.
bool hasEthernetAddress(const std::string &deviceName);
/// @brief Returns the ethernet adress of the device on the network.
std::string getEthernetAddress(const std::string &deviceName);

/// @brief Returns true if the device has an IPv4 adress assigned.
/// This means it has a fixed IP address assigned or DHCP resolve worked.
bool hasIPv4Address(const std::string &deviceName);
/// @brief Returns the IPv4 adress of the device on the network.
std::string getIPv4Address(const std::string &deviceName);

/// @brief Returns true if the two files passed have the same content (names and stats can be different).
bool isFileContentSame(const stdfs::path &fileA, const stdfs::path &fileB);
