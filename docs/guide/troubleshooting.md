# Troubleshooting

## Logs

```sh
journalctl -u purrdometer-logger -u purrdometer-display -f
```

The logger says when it scans, finds, connects to and loses the sensor, and
logs every measurement that adds rotations (`revs=… +… rpm=…`).

## A sensor that keeps dropping

The Pi Zero 2 W shares one radio between WiFi and Bluetooth. With
Bluetooth's default 420 ms supervision timeout and the sensor's ~200 ms
connection interval, two lost packets in a row end the connection, and WiFi
takes enough of the radio for that to happen within seconds. The package sets
a 6 s timeout and turns WiFi power-save off. Check both:

```sh
grep -B1 ConnectionSupervisionTimeout /etc/bluetooth/main.conf   # 600, under a "# purrdometer:" line
sudo cat /sys/kernel/debug/bluetooth/hci0/supervision_timeout      # 600
/usr/sbin/iwconfig wlan0 | grep Power                              # Power Management:off
```

If the BlueZ line is missing (after a `bluez` upgrade replaced `main.conf`),
reinstall the package.

## A sensor that is connected but silent

The sensor sends data only while it detects rotation: full turns in the plane
of its face, the way a crank turns it, like a clock hand going round. Rocking
it does not even wake it; it takes about three full turns in a row
([the sensor](sensor.md#waking-up)).

If it is turning and still silent:

- check that BlueZ connects with a random address: `grep -B1 "^Privacy" /etc/bluetooth/main.conf`
  shows `Privacy = device` under a `# purrdometer:` line. A sensor that was
  once paired with the Pi sends nothing to the Pi's own address; reinstall the
  package if the line is missing;
- check the battery level on NOW;
- check it is mounted face-parallel to the wheel ([the sensor](sensor.md#mounting)).

## The Bluetooth trace

```sh
sudo purrdometer-btdiag
```

Shows each connection's interval and supervision timeout, disconnects with
their reason, and measurements as they arrive. `Connection Timeout (0x08)` is
the radio problem above; a timeout shorter than 6 s means the setting is not
in effect. It also re-applies a 6 s timeout to a link that came up shorter.

## The database

```sh
sqlite3 /var/lib/purrdometer/purrdometer.db \
  "select date(minute*60,'unixepoch','localtime') d, sum(revs) from minutes group by d order by d desc limit 7"
sqlite3 /var/lib/purrdometer/purrdometer.db "select * from state"
```

## The screen stays blank

Check that SPI and I²C are on (`ls /dev/spidev0.0 /dev/i2c-1`) and that the
display service is running. `purrdometer-display -v` in a terminal (stop the
service first) shows each tap and refresh.
