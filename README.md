# WiFi remote access and WPS daemon for Raspbian

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) ![Build](https://github.com/HorstBaerbel/remoteaccessd/workflows/Build/badge.svg) ![Clang-format](https://github.com/HorstBaerbel/remoteaccessd/workflows/Clang-format/badge.svg) ![Clang-tidy](https://github.com/HorstBaerbel/remoteaccessd/workflows/clang-tidy/badge.svg)

This systemd service can toggle WiFi, SSH and DHCP on a Raspberry Pi from via a [GPIO button](#how-to-add-a-gpio-button-to-your-system). Additionally it can connect to an access point using WPS by long-pressing that same button. It can also detect and copy new [wpa_supplicant.conf](#an-example-wpasupplicantconf-file) files from an USB stick to the proper system directory to reconfigure the WPA encryption.

If you find a bug or make an improvement your pull requests are appreciated.

## License

All of this is under the [MIT License](LICENSE).

## Usage

### Remote access toggle functionality

By default it waits for a 2-5s press of the F12 key from an input device to:

1. Either: Comment / uncomment the "dtoverlay=disable-wifi" entry in "/boot/config.txt" (default).
2. Or: Power up / down the WiFi device using ```iwconfig```.

* Enable / disable the "ssh" systemd service.
* Enable / disable the "dhcpcd" systemd service.
* Reboot the RPi to apply the changes (option 1. needs a reboot).

With option 1. WiFi can be fully disabled. Option 2. only disables the device.

### WPS connect functionality

By default it waits for a 5-8s press of the F12 key from an input device to:

* Enable WiFi if necessary(see above, might involve a reboot).
* Start looking for the strongest WiFi access point with WPS enabled and connect to that.
* Store the configuration for that AP.

### WPA configuration file copy functionality

The daemon will watch for a path to become available (use [usbmount](https://github.com/rbrito/usbmount) to mount USB sticks automatically) with a [wpa_supplicant.conf](wpa_supplicant.conf) [file](https://raspberrypi.stackexchange.com/questions/10251/prepare-sd-card-for-wifi-on-headless-pi) in its base directory every 3s. It will then copy that file to the proper location on the file system (/etc/wpa_supplicant/wpa_supplicant.conf) if it differs from the current configuration and reboot the system. This way you can get a headless RPi onto new networks really quickly without WPS.

## Build, configure, install

* Clone repo using ```git clone https://github.com/HorstBaerbel/remoteaccessd```
* Navigate to the remoteaccessd folder, then:

```sh
mkdir build && cd build
cmake --config Release ..
make -j$(nproc)
```

### Configuring

* Adjust the ```ExecStart=``` call in "remoteaccess.service" to your needs before installing. The command line options for the daemon are:

1. [GPIO button input device](#how-to-add-a-gpio-button-to-your-system), e.g. "/dev/input/event0"
2. Directory to watch for a "wpa_supplicant.conf" file, e.g. "/media/usb"
3. Optionally a method to toggle WiFi. Pass either "useOverlay" (or nothing, option 1.) or "useIwconfig" (option 2.) to specify which method to use.  

The line should look something like this: ```ExecStart=/usr/local/bin/remoteaccessd /dev/input/event0 /media/usb useOverlay```

### Installing

* Run installation: ```sudo make install```

The daemon should now run. You can check its state with: ```systemctl status remoteaccess```. The service starts relatively late in the boot process (after "basic.target"), so it might take couple of seconds after bootup until button presses or config files are detected.

### Uninstalling

* Run deinstallation: ```sudo make uninstall```

## Additional information

### How to add a GPIO button to your system

* Connect a normally-open / "NO" button to your RPi
* Add a button overlay to "/boot/config.txt": ```sudo nano /boot/config.txt``` and add:

```sh
# Toggle WiFi / SSH, KEY_F12
dtoverlay=gpio-key,gpio=23,label="wifi-toggle",keycode=88
```

This will create a GPIO button input device under /dev/input/eventX (connected to GPIO 23). You can find out what X is by running ```evtest```, selecting a device number and trying the button. You can read more about device overlays [here](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README) and about keycodes [here](https://github.com/torvalds/linux/blob/v4.12/include/uapi/linux/input-event-codes.h).

### An example wpa_supplicant.conf file

```sh
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
country=TWO_LETTER_COUNTRY_CODE

network={ 
ssid="ROUTER_SSID"
psk="WIFI_PASSWORD"
}
```

You can encrypt your password, so you don't need to store it in clear text using: ```wpa_passphrase ROUTER_SSID```. The tool will prompt you for your WPA password and encrypt it. The use that string in the "wpa_supplicant.conf" file instead of the password, but WITHOUT quotation marks, e.g. ```psk=34985abcf4576e9```.
