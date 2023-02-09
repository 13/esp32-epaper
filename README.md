# esp32-epaper

An ESP32 Epaper temperature, humidity & heating display

## Contents

 * [About](#about)
   * [Built With](#built-with)
 * [Getting Started](#getting-started)
   * [Prerequisites](#prerequisites)
   * [Installation](#installation)
 * [Usage](#usage)
 * [Roadmap](#roadmap)
 * [Release History](#release-history)
 * [License](#license)
 * [Contact](#contact)
 * [Acknowledgements](#acknowledgements)

## About

Display temperature, humidity and heating state over MQTT on an epaper display.

```
____________________
    16:11 24.01.
--------------------
      OUTSIDE
    -2.2°   44%
--------------------
       INSIDE
    21.3°   56%
--------------------
    HEIZUNG: EIN
____________________

```

### Built With

* [VSCodium](https://vscodium.com/)
* [PlatformIO](https://platformio.org/)

## Getting Started

### Prerequisites

* An ESP32
* A Waveshare 4.2" Epaper
* VSCodium
* PlatformIO

### Installation

```sh
git clone https://github.com/13/esp32-epaper.git
```

## Usage

* Edit `config-credentials.h` and save as `credentials.h`
* Edit `main.cpp` to your needs
* Edit `platformio.ini` to your needs
* Build & upload to your Arduino

## Roadmap

- [ ] Add influxdb for initial state
- [ ] Add high/low value
- [ ] Add style (seperation lines)
- [ ] Add powersave
- [ ] Add last time update

## Release History

* 1.0.0
    * Initial release

## Contact

* **13** - *Initial work* - [13](https://github.com/13)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* Thank you
