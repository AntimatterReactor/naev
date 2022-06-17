--[[
<?xml version='1.0' encoding='utf8'?>
<mission name="Point of Interest">
 <location>none</location>
 <chance>0</chance>
</mission>
--]]
--[[

   Point of Interest missions. These are obtained through various means and make the player do a bit of exploration in order to obtain fancy rewards.

   General approach is:
   1. Go to Point of Interest
   2. Activate scanning outfit
   3. Follow trails (with potential hostiles) to goal
   4. Loot goal!

   TODO
   - Use minigames? stringguess is a good candidate
   - Give a couple interesting outfit rewards, some that are repeatable
   - Add lots of lore rewards, not sure if best to store in ship log or somewhere else

--]]
local fmt = require "format"
--local luaspfx = require "luaspfx"
--local tut = require "common.tutorial"
local der = require 'common.derelict'
local poi = require "common.poi"
local vn = require "vn"

-- luacheck: globals found board (Hook functions passed by name)

function create ()
   mem.sys, mem.risk = poi.start()
   mem.rewardrisk = mem.risk

   -- We do a soft claim on the final system
   if not misn.claim( {mem.sys}, nil, true ) then
      return
   end

   -- Chance of being locked with less risk
   if rnd.rnd() < 0.2*mem.risk then
      mem.locked = true
      mem.risk = mem.risk-1
   end

   -- Roll for rewards here to disallow save scumming
   local reward_list = {
      {
         type = "credits",
         value = 100e3 + 100e3*rnd.rnd() * (mem.rewardrisk+1),
      },
   }
   local function add_unique_reward( oname, msg )
      if player.numOutfit( oname ) <= 0 then
         table.insert( reward_list, {
            type = "outfit",
            value = oname,
            msg = msg,
         } )
      end
   end
   add_unique_reward( "Jump Deregulator", _([[You explore the ship, and while most things seem like they aren't of any use to you, one thing catches your eye. It seems like there is a weird module attached to the navigation console. Upon closer inspection it seems like it overrides some core jump behaviour of the ships. You don't know if it will be of use to you, but pocket it just in case.]]) )

   -- Choose a random reward and stick to it
   mem.reward = reward_list[ rnd.rnd(1,#reward_list) ]

   poi.misnSetup{ sys=mem.sys, found="found", risk=mem.risk }
end

function found ()
   player.msg(_("You have found something!"),true)

   -- TODO something more interesting
   local p = pilot.add( "Mule", "Derelict", mem.goal, _("Pristine Derelict"), {naked=true} )
   p:disable()
   p:setInvincible()
   p:setHilight()
   p:effectAdd( "Fade-In" )
   hook.pilot( p, "board", "board" )
end

function board( p )
   local failed = false

   vn.clear()
   vn.scene()
   vn.sfx( der.sfx.board )
   vn.music( der.sfx.ambient )
   vn.transition()
   vn.na(_([[You board the derelict which seems oddly in pretty good condition. What a lucky find!]]))

   -- Have to resolve lock or bad thing happens (tm)
   if mem.locked then
      local stringguess = require "stringguess"
      vn.na(_([[You access the main computer and try to log in, however, it seems like you're going to have to break the code to gain complete access to the ship.]]))
      stringguess.vn()
      vn.func( function ()
         if stringguess.completed then
            vn.jump("unlocked")
            return
         end
         vn.jump("unlock_failed")
      end )

      vn.label("unlocked")
      vn.na(_([[You deftly crack the code and the screen flashes with 'AUTHORIZATION GRANTED'. Time to see what goodness awaits you!]]))
      vn.jump("reward")

      vn.label("unlock_failed")
      vn.na(_([["A brief 'AUTHORIZATION DENIED' flashes on the screen and you hear the ship internals groan as the emergency security protocol kicks in and everything gets locked down. It looks like you won't be getting anywhere hre, the ship is as good as debris. You have no option but to return dejectedly to your ship. Maybe next time."]]))
      vn.func( function () failed = true end )
      vn.done()
   end

   vn.label("reward")
   local msg = ""
   if mem.reward.type == "credits" then
      msg = _([[You access the main computer and are able to login to find a hefty amount of credits. This will come in handy.]])
      player.pay( mem.reward.value )
   elseif mem.reward.type == "outfit" then
      msg = mem.reward.msg or _([[Exploring the cargo bay, you find something that might be of use to you.]])
      player.outfitAdd( mem.reward.value )
   end
   msg = msg .. "\n\n" .. fmt.reward(mem.reward.value)
   vn.na( msg )
   vn.sfxVictory()
   vn.na(_([[You explore the rest of the ship but do not find anything else of interest.]]))
   vn.sfx( der.sfx.unboard )
   vn.run()

   -- Clean up stuff
   poi.misnDone( failed )
   p:setHilight(false)
   player.unboard()
   misn.finish( not failed )
end
