require 'ai.core.core'
require "numstring"

-- Settings
mem.aggressive    = true
mem.whiteknight   = true

local bribe_no_list = {
   _([["You insult my honour."]]),
   _([["I find your lack of honour disturbing."]]),
   _([["You disgust me."]]),
   _([["Bribery carries a harsh penalty."]]),
   _([["House Goddard does not lower itself to common scum."]])
}
local taunt_list = {
   _("Prepare to face annihilation!"),
   _("I shall wash my hull in your blood!"),
   _("Your head will make a great trophy!"),
   _("These moments will be your last!"),
   _("You are a parasite!")
}

-- Create function
function create ()
   -- Credits
   local price = ai.pilot():ship():price()
   ai.setcredits( rnd.rnd(price/300, price/70) )

   mem.loiter = 3 -- This is the amount of waypoints the pilot will pass through before leaving the system

   -- Finish up creation
   create_post()
end

-- When hailed
function hail ()
   local p = ai.pilot()

   -- Remove randomness from future calls
   if not mem.hailsetup then
      mem.refuel_base = rnd.rnd( 2000, 4000 )
      mem.bribe_base = math.sqrt( p:stats().mass ) * (300 * rnd.rnd() + 850)
      mem.bribe_rng = rnd.rnd()
      mem.hailsetup = true
   end

   -- Clean up
   mem.refuel        = 0
   mem.refuel_msg    = nil
   mem.bribe         = 0
   mem.bribe_prompt  = nil
   mem.bribe_prompt_nearby = nil
   mem.bribe_paid    = nil
   mem.bribe_no      = nil

   -- Deal with refueling
   local standing = p:faction():playerStanding()
   mem.refuel = mem.refuel_base
   if standing > 60 then mem.refuel = mem.refuel * 0.7 end
   mem.refuel_msg = string.format( _([["I could do you the favour of refueling for the price of %s."]]),
         creditstring(mem.refuel) )

   -- Bribing
   mem.bribe_no = bribe_no_list[ rnd.rnd(1,#bribe_no_list) ]
end

-- taunts
function taunt ( target, offense )
   -- Offense is not actually used
   local taunts = taunt_list
   ai.pilot():comm( target, taunts[ rnd.rnd(1,#taunts) ] )
end

