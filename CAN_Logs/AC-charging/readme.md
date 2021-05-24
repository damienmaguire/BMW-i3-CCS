These files were sniffed from PT-CAN of 2015 BMW i3 I01 REX using imBMW.net V2 device, that is not optimized yet for logging 500kbps CAN-BUS, so there are lost frames in some of these files (rows with ID = 0x1FFFFFFF).

Files were converted from free-text and binary formats to SavvyCAN CSV format. Original logs are stored in respective directory.

- 1.ptcan_start-end-chrg_69perc.csv
- 2.ptcan_start-end-chrg_69perc_one-more.csv
-- Two sessions of starting and ending AC charging with 69% SOC.
- 3.ptcan_start-chrg_69perc_has-lost-frames.csv -- One more attempt of starting AC charging with 69%-69.5% SOC (this file contains lost frames).
- 4.ptcan_fully-chrgd_start-chrg_many-lost-frames.csv -- Trying to start AC charging with 100% SOC (this file contains lost frames).