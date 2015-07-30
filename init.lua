-- LDMS initialization file
-- put in /usr/share/ldms

--Open hardware specific libraried
--tlc5948a = require("tlc5948a")
--mcdc04 = require("mcdc04")
--ad5522 = require("ad5522")
--id = require("id")
--db = require("db")
----Initializes the basic objects as global variables
--led = tlc5948a.new("/dev/spidev2.0")
--pmu = ad5522.new(1, 0, 0, 126) 
--lmu = mcdc04.new(1, 0x74)
--hw = id.new("/sys/bus/i2c/devices/0-0050/eeprom", "/var/lib/w1/bus.0")
--nltsdb = db.new("192.168.16.15", "root", "V0st!novaled#", "nltsdb")


function append_to_db(driver, str)
    nltsdb:push_results(driver, str)
end

-- Turn on right led with white, full intensity
led:turn_on(1)
led:turn_on(2)
led:turn_on(3)
