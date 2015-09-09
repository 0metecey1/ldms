-- LDMS teardown file
-- put in /usr/share/ldms

-- Turn off all leds
led:turn_off(1)
led:turn_off(2)
led:turn_off(3)
led:turn_off(4)
led:turn_off(5)
led:turn_off(6)
-- Turn on left led with red, full intensity
led:turn_on(6)
