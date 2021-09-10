require 'ai.core.core'
require "numstring"

-- Settings
mem.armour_run    = 40
mem.armour_return = 70
mem.aggressive    = true
mem.lanes_useneutral = true

local bribe_no_list = {
   _([["You won't buy your way out of this one."]]),
   _([["I'm afraid you can't make it worth my while."]]),
}
local taunt_list_offensive = {
   _("Don't take this personally."),
   _("It's just business."),
}
local taunt_list_defensive = {
   _("Your skull will make a great hood ornament."),
   _("I've destroyed ships twice the size of yours!"),
   _("I'll crush you like a grape!"),
   _("This isn't what I signed up for!"),
}

function create ()
   ai.setcredits( rnd.rnd(ai.pilot():ship():price()/150, ai.pilot():ship():price()/50) )

   mem.loiter = 3 -- This is the amount of waypoints the pilot will pass through before leaving the system

   hail()

   -- Finish up creation
   create_post()
end

-- When hailed
function hail ()
   local p = ai.pilot()

   -- Remove randomness from future calls
   if not mem.hailsetup then
      mem.refuel_base = rnd.rnd( 3000, 5000 )
      mem.bribe_base = math.sqrt( p:stats().mass ) * (750 * rnd.rnd() + 2500)
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

   -- Refuel
   mem.refuel = mem.refuel_base
   mem.refuel_msg = string.format(_([["I'll supply your ship with fuel for %s."]]),
         creditstring(mem.refuel))

   -- Set up bribes
   mem.bribe = mem.bribe_base
   if (mem.natural or mem.allowbribe) and mem.bribe_rng > 0.7 then
      mem.bribe_prompt = string.format(_([["Your life is worth %s to me."]]), creditstring(mem.bribe) )
      mem.bribe_paid = _([["Beat it."]])
   else
      mem.bribe_no = bribe_no_list[ rnd.rnd(1,#bribe_no_list) ]
   end
end

-- taunts
function taunt ( target, offense )
   -- Only 20% of actually taunting.
   if rnd.rnd() > 0.2 then
      return
   end

   -- some taunts
   local taunts
   if offense then
      taunts = taunt_list_offensive
   else
      taunts = taunt_list_defensive
   end

   ai.pilot():comm(target, taunts[ rnd.rnd(1,#taunts) ])
end


