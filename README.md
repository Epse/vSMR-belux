# Belux vSMR Plugin

Belux vSMR is a plugin for Euroscope that simulates the AMS Developments / Belgocontrol / Skeyes AMS RDD ground radar system on VATSIM.
It is based on vSMR,  a plugin for EuroScope that simulates the NOVA 9000 A-SMGCS system on VATSIM.

Features:
* Custom aircraft polygons with history trails.
* Realistic aircraft tags.
* RIMCAS simulation.
* Approach view window.
* Hoppie CPDLC clearance system integration.

Find out all you need to know on the wiki: <https://github.com/Epse/vSMR-belux/wiki>, or the original vSMR wiki: <https://github.com/pierr3/vSMR/wiki>

### Belux additions
The plugin was heavily modified for the Belux vACC.
Features:
* Different font size and weight for the first line (callsign).
* Changed speed threshold when taxing to switch tag information (gate, rwy) with groundspeed from 25 to 50 knots.
* Implemented gate field from Belux plugin in the tag
* Added pro_mode_belux filter: only show tag when AC is squawking mode C.
* Removed green tag background color when on runway
* Set symbol brightness to 70 by default (making it more easy to spot the secondary track symbol).
* Custom filters support
* Non-rectangular tags
* Realistic target symbols
* Realistic tag borders
* Support for the TopSky menubar

### Aircraft Size Database

We load aircraft sizes from a file, a default one is included in this repo.
We base this information on the [FAA Aircraft Characteristics Database](https://www.faa.gov/airports/engineering/aircraft_char_database).
To convert that Excel document to our needed CSV, see below spoiler.

<details>
<summary>Creating an Aircraft Size Database</summary>
<!-- Markdown lets us keep the below numbering each at 1 and it'll renumber, fun! -->

1. Downlaod the Excel file from the FAA
1. Keep the columns `ICAO_Code`, Length, Tail Height and Main Gear Width. Keep both wingspan columns, but merge them (only one is filled). We use Main Gear Width to approximate cabin width.
1. Convert each column to meters. This is a simple multiplication by 0.3048. Make sure to convert to value (Data, Calculate, Formula to Value in LibreOffice Calc)
1. Drop the columns in feet.
1. Keep the heading row, pick any titles you like. They are ignored by the tooling.
1. Set the column order to type, wingspan, length, tail, main gear
1. Export as TSV. In LibreOffice, you use Save As, select csv, pick no quoting and Tab as field delimiter. The file should be called `aircraft_data.tsv`.

</details>

### Release

This is the latest stable release, which has been tested and the one you should use for day to day use.

Download the latest release here: <https://github.com/Epse/vSMR-belux/releases>

### Nightly builds

If you know what you're doing, you can nab the latest dev builds here: <https://github.com/Epse/vSMR-belux/actions> under Artifacts of an MSBuild workflow. Try to grab one from the master branch.

### Thanks

Thanks to pierr3 for the original version of this plugin and Nicola Macoir for the original Belux fork.

The below note of thanks is integrally copied from upstream and we share its sentiment.
Special thanks to Daniel Lange, Even Rognlien, Juha Holopainen, Lionel Bischof and Wenjun Zhou for their help with the code, thanks to Sam White and Theo Bearman for their help on the wiki, and thanks to Jonas Kuster for the countless bug hunts and help on issue tracking!
