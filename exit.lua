-- LDMS initialization file
-- put in /usr/share/ldms

function append_to_db(driver, str)
    nltsdb:push_results(driver, str)
end

-- Turn on right led with white, full intensity
led:turn_on(1)
led:turn_on(2)
led:turn_on(3)
