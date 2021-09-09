--[[
<?xml version='1.0' encoding='utf8'?>
<mission name="Pirate Convoy Raid">
 <avail>
  <priority>4</priority>
  <cond>faction.playerStanding("Pirate") &gt;= -20</cond>
  <chance>460</chance>
  <location>Computer</location>
  <faction>Wild Ones</faction>
  <faction>Black Lotus</faction>
  <faction>Raven Clan</faction>
  <faction>Dreamer Clan</faction>
  <faction>Pirate</faction>
 </avail>
 <notes>
  <tier>1</tier>
 </notes>
</mission>
--]]
--[[
   Have to raid a convoy and bring stuff back.
   1. Convoy moves slower because material needs careful delivery.
   2. Have to disable convoy ships and recover stuff.
   3. Payment is based on how much stuff is recovered.
--]]
local pir = require "missions.pirate.common"
local fmt = require "format"
local flt = require "fleet"
local lmisn = require "lmisn"
local vntk = require "vntk"
require "jumpdist"

function get_route( sys )
   local adj = sys:jumps()
   if #adj < 2 then return end
   local jumpenter, jumpexit
   local dist = 0
   for i,j1 in ipairs(adj) do
      for j,j2 in ipairs(adj) do
         if j1 ~= j2 then
            local d = j1:pos():dist2(j2:pos())
            if d > dist then
               dist = d
               jumpenter = j1
               jumpexit = j2
            end
         end
      end
   end
   if rnd.rnd() < 0.5 then
      return jumpenter, jumpexit, dist
   else
      return jumpexit, jumpenter, dist
   end
end

function create ()
   local target_factions = {
      "Independent",
      "Trader",
      "Empire",
      "Soromid",
      "Sirius",
      "Dvaered",
      "Za'lek",
   }
   -- Choose system and try to claim
   local sysfilter = function ( sys )
      local p = sys:presences()
      local total =  0
      for k,v in ipairs(target_factions) do
         total = total + (p[v] or 0)
      end
      local pirates = pir.systemPresence(sys)
      -- Some presence check
      if total < 300 or pirates > total then
         return false
      end
      -- Must not be cliamed
      if not misn.claim( sys, true ) then
         return false
      end
      -- Must have two jumps
      local j1, j2, d = get_route( sys )
      if j1 ~= nil and d < 10e3*10e3 then
         return false
      end

      return true
   end
   local syslist = getsysatdistance( nil, 2, 7, sysfilter, true )
   if #syslist <= 0 then
      misn.finish(false)
   end

   -- Choose system
   targetsys = syslist[ rnd.rnd(1,#syslist) ]
   if not misn.claim( targetsys ) then
      misn.finish(false)
   end

   local cargoes = {
      -- Standard stuff
      {N_("Corporate Documents"), N_("Documents detailing transactions and operations of certain corporations.")},
      {N_("Technology Blueprints"), N_("Blueprints of under development advanced technology.")},
      {N_("Research Prototypes"), N_("Advanced prototypes of cutting edge research. Doesn't seem like of much use outside of an academic environment.")},
      {N_("High-end Implants"), N_("Some of the newest and fanciest cybernetic implants available. They included nose implants that allow amplifying and modifying smells beyond human imagination.")},
      {N_("Synthetic Organs"), N_("Special synthetic copies of natural human organs that are able to ")},
      -- A bit sillier
      {N_("Audiophile Paraphernalia"), N_("High end audio systems meant for true audio connoisseurs.")},
      {N_("Rare Plants"), N_("High quality and rare specimens of plants.")},
      {N_("Ornamental Shrimp"), N_("An assortment of small and colourful shrimp.")},
      {N_("High Quality Pasta"), N_("Dried pasta of the highest quality.")},
      {N_("Premium Body Soap"), N_("Incredibly silky soap that creates a seemingly infinite amount of bubbles.")},
      {N_("Luxury Captain Chairs"), N_("Very comfortable chairs meant for ship captains. Every captain dreams of having such chairs.")},
      {N_("Incredibly Spicy Sauce"), N_("Hot sauce made from the spiciest peppers that have been genetically engineered. Not really suited for human consumption, but people use them anyway.")},
      {N_("Exquisite Cat Toys"), N_("Cat toys with built in light and motion system to stimulate any cat to the max. They also don't use cheap glue that make them break down within 5 minutes of playing with a cat.")},
   }

   -- Finish mission details
   returnpnt, returnsys = planet.cur()
   cargo = cargoes[ rnd.rnd(1,#cargoes) ]
   cargo.__save = true
   misn_cargo = misn.cargoNew( cargo[1], cargo[2] )
   enemyfaction = faction.get("Trader")
   convoy_enter, convoy_exit = get_route( targetsys )
   -- TODO make tiers based on how many times the player does them or something
   local r = rnd.rnd()
   local done = var.peek("pir_convoy_raid") or 0
   local mod = math.exp( -done*0.05 ) -- 0.95 for 1, 0.90 for 2 0.86 for 3, etc.
   mod = math.max( 0.5, mod ) -- Limit it so that 50% are large
   local adjective
   if r < 0.5*mod then
      tier = 1
      adjective = "tiny"
   elseif r < 1.0*mod then
      tier = 2
      adjective = "small"
   elseif r < 1.2*mod then
      tier = 3
      adjective = "medium"
   else
      tier = 4
      adjective = "large"
   end

   -- Set up rewards
   reward_faction = pir.systemClanP( system.cur() )
   reward_base = 25e3 + rnd.rnd() * 15e3 + 25e3*math.sqrt(tier)
   reward_cargo = 2e3 + rnd.rnd() * 2e3 + 3e3*math.sqrt(tier)

   local faction_text = pir.reputationMessage( reward_faction )
   local faction_title = ""
   if pir.factionIsClan( reward_faction ) then
      faction_title = fmt.f(_(" ({clanname})"), {clanname=reward_faction:name()})
   end

   misn.setTitle(fmt.f(_("#rPIRACY#0: Raid a {adjective} {name} convoy in the {sys} system{fct}"), {adjective=adjective, name=enemyfaction:name(), sys=targetsys:name(), fct=faction_title} ))
   misn.setDesc(fmt.f(_("A convoy carrying {cargo} will be passing through the {sys} system on the way from {entersys} to {exitsys}. A local Pirate Lord wants you to assault the convoy and bring back as many tonnes of {cargo} as possible. You will be paid based on how much you are able to bring back.{reputation}"),
         {cargo=cargo[1], sys=targetsys:name(), entersys=convoy_enter:dest():name(), exitsys=convoy_exit:dest():name(), reputation=faction_text}))
   misn.setReward(fmt.f(_("{rbase} and {rcargo} per ton of {cargo} recovered"), {rbase=fmt.credits(reward_base),rcargo=fmt.credits(reward_cargo),cargo=cargo[1]}))
   misn.markerAdd( targetsys )
end

function accept ()
   misn.accept()

   misn.osdCreate(_("Pirate Raid"), {
      fmt.f(_("Go to the {sysname} system"),{sysname=targetsys:name()}),
      fmt.f(_("Plunder {cargoname} from the convoy"),{cargoname=cargo[1]}),
      fmt.f(_("Deliver the loot to {pntname} in the {sysname} system"),{pntname=returnpnt:name(), sysname=returnsys:name()}),
   } )

   hook.enter("enter")
   hook.land("land")
end

function enter ()
   if convoy_spawned and player.pilot():cargoHas( misn_cargo ) <= 0 then
      player.msg(fmt.f(_("#rMISSION FAILED: You did not recover any {cargoname} from the convoy!"), {cargoname=cargo[1]}))
      misn.finish(false)
   end
   if system.cur() ~= targetsys or convoy_spawned then
      return
   end

   convoy_spawned = true
   misn.osdActive(2)
   hook.timer( 10+5*rnd.rnd(), "enter_delay" )
end

function land ()
   local pp = player.pilot()
   local q = pp:cargoHas( misn_cargo )
   if convoy_spawned and q > 0 then
      local reward = reward_base + q * reward_cargo
      lmisn.sfxVictory()
      vntk.msg( _("Mission Success"), fmt.f(_("The workers unload your {cargoname} and take it away to somewhere you can't see. As you wonder about your payment, you suddenly receive a message that #g{reward}#0 was transferred to your account."), {cargoname=cargo[1], reward=fmt.credits(reward)}) )
      pp:cargoRm( misn_cargo, q )
      player.pay( reward )

      -- Faction hit
      faction.modPlayerSingle(reward_faction, tier*rnd.rnd(1, 2))

      -- Mark as done
      local done = var.peek( "pir_convoy_raid" ) or 0
      var.push( "pir_convoy_raid", done+1 )
      misn.finish(true)
   end
end

function enter_delay ()
   mrkentry = system.mrkAdd( _("Convoy Entry Point"), convoy_enter:pos() )
   mrkexit = system.mrkAdd( _("Convoy Exit Point"), convoy_exit:pos() )

   player.autonavReset( 5 )
   player.msg(fmt.f(_("The convoy will be coming in from {sysname} shortly!"), {sysname=convoy_enter:name()}))
   hook.timer( 5+10*rnd.rnd(), "spawn_convoy" )
end

function spawn_convoy ()
   -- Tier 1
   local tships, eships
   if tier==4 then
      tships = {"Mule", "Mule", "Mule"}
      if rnd.rnd() < 0.5 then
         table.insert( tships, "Mule" )
      end
      if rnd.rnd() < 0.5 then
         eships = {"Kestrel"}
      else
         eships = {"Pacifier", "Pacifier"}
      end
      for i=1,rnd.rnd(4.5) do
         table.insert( eships, (rnd.rnd() < 0.7 and "Lancelot") or "Admonisher" )
      end

   elseif tier==3 then
      tships = {"Mule"}
      local r = rnd.rnd()
      if r < 0.3 then
         eships = {"Pacifier"}
      elseif  r < 0.6 then
         eships = {"Vigilance"}
      else
         eships = {"Admonisher", "Admonisher"}
      end
      for i=1,rnd.rnd(3.4) do
         table.insert( eships, (rnd.rnd() < 0.7 and "Shark") or "Lancelot" )
      end

   elseif tier==2 then
      if rnd.rnd() < 0.5 then
         tships = {"Koala", "Koala"}
      else
         tships = {"Koala", "Llama", "Llama"}
      end
      if rnd.rnd() < 0.5 then
         eships = { "Lancelot" }
      else
         eships = { "Vendetta" }
      end
      for i=1,3 do
         table.insert( eships, "Shark" )
      end

   else -- tier==1
      tships = {"Llama", "Llama"}
      eships = {"Shark", "Shark"}
      if rnd.rnd() < 0.5 then
         table.insert( eships, "Shark" )
      end
   end
   sconvoy = flt.add( 1, tships, enemyfaction, convoy_enter, _("Convoy") )
   for k,p in ipairs(sconvoy) do
      p:cargoRm("all")
      p:cargoAdd( misn_cargo, math.floor((0.8+0.2*rnd.rnd())*p:cargoFree()) )
      p:intrinsicSet( "speed_mod", -33 )
      p:intrinsicSet( "thrust_mod", -33 )
      p:intrinsicSet( "turn_mod", -33 )
      hook.pilot( p, "board", "convoy_board" )
   end
   sescorts = flt.add( 1, eships, enemyfaction, convoy_enter )
   for k,p in ipairs(sescorts) do
      p:setLeader( sconvoy[1] )
   end
   sconvoy[1]:setHilight(true)
   sconvoy[1]:control()
   sconvoy[1]:hyperspace( convoy_exit, true )
   hook.pilot( sconvoy[1], "death", "convoy_done" )
end

function convoy_board ()
   hook.timer( 1, "convoy_boarded" )
   convoy_done()
end

function convoy_boarded ()
   if player.pilot():cargoHas( misn_cargo ) > 0 then
      misn.osdGetActive(3)
   end
end

function convoy_done ()
   system.mrkClear()
end
