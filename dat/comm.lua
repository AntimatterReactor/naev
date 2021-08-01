local vn = require 'vn'
local lg = require 'love.graphics'
require 'numstring'

local function can_bribe( plt )
   local mem = plt:memory()
   if mem.bribe_no then
      return false
   end
   if not mem.bribe then
      return false
   end
   if mem.bribe==0 then
      return false
   end
   if plt:flags("bribed") then
      return false
   end
   if not plt:hostile() then
      return false
   end
   return true
end

-- See if part of a fleet
local function bribe_fleet( plt )
   local lea = plt:leader()
   local fol = plt:followers()
   if #fol==0 then fol = nil end
   bribe_group = nil
   if lea or fol then
      if lea then
         if not can_bribe(lea) then
            return nil
         end
         bribe_group = lea:followers()
         table.insert( bribe_group, lea )
      else
         if not can_bribe(plt) then
            return nil
         end
         bribe_group = fol
         table.insert( bribe_group, plt )
      end
      local ng = {}
      for k,v in ipairs(bribe_group) do
         if can_bribe( v ) then
            table.insert( ng, v )
         end
      end
      bribe_group = ng
   end
   return bribe_group
end

-- Get nearby pilots for bribing (includes current pilot)
-- Respects fleets
local function nearby_bribeable( plt )
   local pp = player.pilot()
   local bribeable = {}
   for k,v in ipairs(pp:getVisible()) do
      if v:faction() == plt:faction() and can_bribe(v) then
         local flt = bribe_fleet( v )
         if flt then
            for i,p in ipairs(flt) do
               local found = false
               for j,c in ipairs(bribeable) do
                  if c==p then
                     found = true
                     break
                  end
               end
               if not found then
                  table.insert( bribeable, p )
               end
            end
         else
            table.insert( bribeable, v )
         end
      end
   end
   return bribeable
end

local function bribe_cost( plt )
   if type(plt)=="table" then
      local cost = 0
      for k,v in ipairs(plt) do
         cost = cost + bribe_cost(v)
      end
      return cost
   end

   local mem = plt:memory()
   if not mem.bribe then
      warn(string.format(_("Pilot '%s' accepts bribes but doesn't give a price!"), plt:name() ))
      return 1e6 -- just ridiculous for now, players should report it
   end
   return mem.bribe
end

local function bribe_msg( plt, group )
   local mem = plt:memory()
   if group then
      local cost = bribe_cost( group )
      bribe_nearby_cost = cost -- save as global again
      local str = mem.bribe_prompt_nearby
      local cstr = creditstring(cost)
      local chave = creditstring(player.credits())
      if not str then
         str = _([["We'll need at least %s to not leave you as a hunk of floating debris."]])
      end
      str = string.format( str, cstr )
      return string.format(_("%s\n\nThis action will bribe %d %s pilots.\nYou have %s. Pay #r%s#0?"), str, #bribeable, plt:faction():name(), chave, cstr )
   else
      local cost = bribe_cost( plt )
      local str = mem.bribe_prompt
      local cstr = creditstring(cost)
      local chave = creditstring(player.credits())
      if not str then
         str = string.format(_([["I'm gonna need at least %s to not leave you as a hunk of floating debris."]]), cstr)
      end
      return string.format(_("%s\n\nYou have %s. Pay #r%s#0?"), str, chave, cstr )
   end
end

-- stolen from scripts/vn.lua
local function _draw_bg( x, y, w, h, col, border_col, alpha )
   col = col or {0, 0, 0, 1}
   border_col = border_col or {0.5, 0.5, 0.5, 1}
   vn.setColor( border_col, alpha )
   lg.rectangle( "fill", x, y, w, h )
   vn.setColor( col, alpha )
   lg.rectangle( "fill", x+2, y+2, w-4, h-4 )
end

function comm( plt )
   vn.reset()
   vn.scene()

   -- Shortcuts and graphics
   local shipgfx = lg.newImage( plt:ship():gfxComm() )
   local mem = plt:memory()
   local fac = plt:faction()

   -- Set up the namebox
   local function setup_namebox ()
      local nw, nh = naev.gfx.dim()
      vn.menu_x = math.min( -1, 500 - nw/2 )
      vn.namebox_alpha = 0
      local namebox_font = vn.namebox_font
      local faction_str
      if plt:flags("bribed") then
         faction_str = _("#gBribed#0")
      else
         local std, str = fac:playerStanding()
         if plt:hostile() then
            faction_str = _("#rHostile#0")
         else
            faction_str = str
         end
      end
      local namebox_text = string.format("%s\n%s\n%s", fac:name(), plt:name(), faction_str )
      local namebox_col = fac:colour()
      if namebox_col then namebox_col = {namebox_col:rgb()}
      else namebox_col = {1,1,1}
      end
      local namebox_x = math.max( 1, nw/2-600 )
      local namebox_y = vn.namebox_y + vn.namebox_h -- Correct
      local namebox_text_w, wrapped = namebox_font:getWrap( namebox_text, nw )
      local namebox_b = 20
      namebox_w = namebox_text_w + 2*namebox_b
      namebox_h = namebox_font:getLineHeight()*#wrapped + 2*namebox_b
      namebox_y = namebox_y - namebox_h

      -- Get the logo
      local logo = fac:logo()
      local logo_size = namebox_h - 2*namebox_b
      local logo_scale
      if logo then
         namebox_w = namebox_w + logo_size + 10
         logo = lg.newImage( logo )
         local w, h = logo:getDimensions()
         logo_scale = logo_size / math.max(w,h)
      end

      local function render_namebox ()
         local bw, bh = namebox_b, namebox_b
         local x, y = namebox_x, namebox_y
         local w, h = namebox_w, namebox_h

         _draw_bg( x, y, w, h, vn.namebox_bg, nil, 1 )
         vn.setColor( namebox_col, 1 )
         lg.print( namebox_text, namebox_font, x+bw, y+bh )

         if logo then
            vn.setColor( {1, 1, 1}, 1 )
            logo:draw( x+namebox_text_w+10+bw, y+bh, 0, logo_scale )
         end
      end
      vn.setForeground( render_namebox )
   end
   setup_namebox()

   local p = vn.newCharacter( plt:name(), { image=shipgfx } )
   vn.transition()
   if mem.comm_greet then
      p( mem.comm_greet )
   else
      vn.na(string.format(_("You open a communication channel with %s."), plt:name()))
   end
   vn.label("menu")
   vn.menu( function ()
      local hostile = plt:hostile()
      local opts = {
         {_("Close"), "close"},
      }
      if hostile and not plt:flags("bribed") then
         if mem.bribe_no then
            table.insert( opts, 1, {"Bribe", "bribe_no"} )
         elseif mem.bribe and mem.bribe == 0 then
            table.insert( opts, 1, {"Bribe", "bribe_0"} )
         else
            bribe_group = bribe_fleet( plt )
            bribeable = nearby_bribeable( plt ) -- global
            if #bribeable > 1 and not (bribe_group and #bribe_group==#bribeable) then
               table.insert( opts, 1, {string.format(n_("Bribe %d nearby %s pilot","Bribe %d nearby %s pilots",#bribeable), #bribeable, fac:name()), "bribe_nearby"} )
            end

            if bribe_group then
               table.insert( opts, 1, {string.format(_("Bribe fleet (%d pilot)", "Bribe fleet (%d pilots)", #bribe_group), #bribe_group), "bribe"} )
            else
               table.insert( opts, 1, {_("Bribe"), "bribe"} )
            end
         end
      end
      if not hostile then
         table.insert( opts, 1, {"Request Fuel", "refuel"} )
      end
      return opts
   end )


   --
   -- Bribing
   --
   vn.label("bribe_no")
   p( mem.bribe_no )
   vn.jump("menu")

   vn.label("bribe_0")
   p(_([["Money won't save your hide now!"]]))
   vn.jump("menu")

   vn.label("bribe")
   p( function ()
      return bribe_msg( plt, bribe_group )
   end )
   vn.menu{
      {_("Pay"), "bribe_trypay"},
      {_("Refuse"), "bribe_refuse"},
   }

   vn.label("bribe_refuse")
   vn.na(_("You decide not to pay."))
   vn.jump("menu")

   vn.label("bribe_nomoney")
   vn.na( function ()
      local cost
      if bribe_group then
         cost = bribe_cost( bribe_group )
      else
         cost = bribe_cost( plt )
      end
      local cstr = creditstring( player.credits() )
      local cdif = creditstring( cost - player.credits() )
      return string.format(_("You only have %s credits. You need #r%s#0 more to be able to afford the bribe!"), cstr, cdif )
   end )
   vn.jump("menu")

   vn.label("bribe_trypay")
   vn.func( function ()
      local cost = bribe_cost( plt )
      if cost > player.credits() then
         vn.jump("bribe_nomoney")
      end
   end )
   p( function ()
      if mem.bribe_paid then
         return mem.bribe_paid
      end
      return _([["Pleasure to do business with you."]])
   end )
   vn.func( function ()
      local cost = bribe_cost( plt )
      player.pay( -cost, true )
      if bribe_group then
         for k,v in ipairs(bribe_group) do
            v:credits( bribe_cost(v) )
            v:setBribed(true)
            v:setHostile(false)
            local vmem = v:memory()
            vmem.bribe = 0 -- Disable rebribes
         end
      else
         plt:credits( cost )
         plt:setBribed(true)
         plt:setHostile(false)
         mem.bribe = 0 -- Disable rebribes
      end
      setup_namebox()
   end )
   vn.jump("menu")

   vn.label("bribe_nearby")
   p( function () return bribe_msg( plt, bribeable ) end )
   vn.menu{
      {_("Pay"), "bribe_nearby_trypay"},
      {_("Refuse"), "bribe_refuse"},
   }

   vn.label("bribe_nearby_nomoney")
   vn.na( function ()
      local cstr = creditstring( player.credits() )
      local cdif = creditstring( bribe_nearby_cost - player.credits() )
      return string.format(_("You only have %s credits. You need #r%s#0 more to be able to afford the bribe!"), cstr, cdif )
   end )
   vn.jump("menu")

   vn.label("bribe_nearby_trypay")
   vn.func( function ()
      local cost = bribe_nearby_cost
      if cost > player.credits() then
         vn.jump("bribe_nearby_nomoney")
      end
   end )
   p( function ()
      if mem.bribe_paid then
         return mem.bribe_paid
      end
      return _([["Pleasure to do business with you."]])
   end )
   vn.func( function ()
      local cost = bribe_nearby_cost
      player.pay( -cost, true )
      for k,v in ipairs(bribeable) do
         v:credits( bribe_cost(v) )
         v:setBribed(true)
         v:setHostile(false)
         local vmem = v:memory()
         vmem.bribe = 0 -- Disable rebribes
      end
      setup_namebox()
   end )
   vn.jump("menu")

   --
   -- REFUELING
   --
   vn.label("refuel_no")
   p( function () return mem.refuel_no end )
   vn.jump("menu")

   vn.label("refuel_full")
   vn.na(_("Your fuel deposits are already full."))
   vn.jump("menu")

   vn.label("refuel_low")
   p(_([["Sorry, I don't have enough fuel to spare at the moment."]]))
   vn.jump("menu")

   vn.label("refuel_busy")
   p(_([["Sorry, I'm busy now."]]))
   vn.jump("menu")

   vn.label("refuel_refueling")
   vn.na(_("Pilot is already refueling you."))
   vn.jump("menu")

   vn.label("refuel_refuse")
   vn.na(_("You decide not to pay."))
   vn.jump("menu")

   vn.label("refuel")
   vn.func( function ()
      if mem.refuel_no then
         vn.jump("refuel_no")
      end
      local pps = player.pilot():stats()
      if pps.fuel >= pps.fuel_max then
         vn.jump("refuel_full")
      end
      local ps = plt:stats()
      if ps.fuel < 200 then
         vn.jump("refuel_low")
      end
      local msg = mem.refuel_msg
      local val = mem.refuel
      if val==nil or plt:flags("manualcontrol") then
         vn.jump("refuel_busy")
      end
   end )
   p( function ()
      local str = mem.refuel_msg
      local cost = mem.refuel
      local cstr = creditstring(cost)
      local chave = creditstring(player.credits())
      if not str then
         str = string.format(_([["I should be able to refuel you for %s."]]), cstr)
      end
      return string.format(_("%s\n\nYou have %s. Pay #r%s#0?"), str, chave, cstr )
   end )
   vn.menu{
      {_("Pay"), "refuel_trypay"},
      {_("Refuse"), "refuel_refuse"},
   }

   vn.label("refuel_nomoney")
   vn.na( function ()
      local cstr = creditstring( player.credits() )
      local cdif = creditstring( mem.refuel - player.credits() )
      return string.format(_("You only have %s credits. You need #r%s#0 more to be able to afford the refueling!"), cstr, cdif )
   end )
   vn.jump("menu")

   vn.label("refuel_trypay")
   vn.func( function ()
      local cost = mem.refuel
      if cost > player.credits() then
         vn.jump("refuel_nomoney")
      end
   end )
   vn.func( function ()
      local cost = mem.refuel
      player.pay( -cost, true )
      plt:credits( cost )
      plt:refuel( player.pilot() )
   end )
   p(_([["On my way."]]))
   vn.jump("menu")

   vn.label("close")
   vn.run()
end

