--[[
<?xml version='1.0' encoding='utf8'?>
<mission name="The FLF Split">
  <flags>
   <unique />
  </flags>
  <avail>
   <priority>2</priority>
   <chance>30</chance>
   <done>Assault on Haleb</done>
   <location>Bar</location>
   <faction>FLF</faction>
   <cond>faction.playerStanding("FLF") &gt;= 90</cond>
  </avail>
  <notes>
   <campaign>Save the Frontier</campaign>
  </notes>
 </mission>
 --]]
--[[

   The FLF Split

--]]
local flf = require "missions.flf.flf_common"
require "missions.flf.flf_rogue"

title = {}
text = {}

title[1] = _("The Split")
text[1] = _([[As you approach, you notice that Benito has an unusually annoyed expression. But when she sees you, she calms down somewhat. "Ah, %s." She sighs. "No one is willing to take up this mission, and while I can understand it's a tough mission, it really has to be taken care of.
    "See, for some reason, a group of FLF pilots has decided to turn traitor on us. They're hanging around outside of Sindbad and shooting us down. They need to be stopped, but no one wants to get their hands dirty killing fellow FLF pilots. But they're not FLF pilots anymore! They betrayed us! Can't anyone see that?" She takes a deep breath. "Will you do it, please? You'll be paid for the service, of course."]])

text[2] = _([["Yes, finally!" It's as if a massive weight has been lifted off of Benito's chest. "Everyone trusts you a lot, so I'm sure this will convince them that, yes, killing traitors is the right thing to do. They're no better than Dvaereds, or those Empire scum who started shooting at us recently! Thank you for accepting the mission. Now I should at least be able to get a couple more pilots to join in and help you defend our interests against the traitors. Good luck!"]])

text[3] = _([["Ugh, this is so annoying... I understand, though. Just let me know if you change your mind, okay?"]])

pay_text = {}
pay_text[1] = _([[Upon your return to the station, you are greeted by Benito. "Thanks once again for a job well done. I really do appreciate it. Not only have those traitors been taken care of, the others have become much more open to the idea that, hey, traitors are traitors and must be eliminated." She hands you a credit chip. "Here is your pay. Thank you again."]])

misn_title = _("The Split")
misn_desc = _("A fleet of FLF soldiers has betrayed the FLF. Destroy this fleet.")
misn_reward = _("Getting rid of treacherous scum")

npc_name = _("Benito")
npc_desc = _("Benito seems to be frantically searching for a pilot.")

log_text = _([[Regrettably, some rogue FLF pilots have turned traitor, forcing you to destroy them. Your action helped to assure fellow FLF pilots that treacherous FLF pilots who turn on their comrades are enemies just like any other.]])


function create ()
   missys = system.get( "Sigur" )
   if not misn.claim( missys ) then misn.finish( false ) end

   level = 3
   ships = 4
   flfships = 2

   credits = 100e3

   late_arrival = true
   late_arrival_delay = rnd.uniform( 10.0, 120.0 )

   misn.setNPC( npc_name, "flf/unique/benito.webp", npc_desc )
end


function accept ()
   if tk.yesno( title[1], text[1]:format( player.name() ) ) then
      tk.msg( title[1], text[2]:format( player.name() ) )

      misn.accept()

      misn.setTitle( misn_title )
      misn.setDesc( misn_desc )
      misn.setReward( misn_reward )
      marker = misn.markerAdd( missys, "high" )

      osd_desc[1] = osd_desc[1]:format( missys:name() )
      misn.osdCreate( osd_title, osd_desc )

      rogue_ships_left = 0
      job_done = false
      last_system = planet.cur()

      hook.enter( "enter" )
      hook.jumpout( "leave" )
      hook.land( "leave" )
   else
      tk.msg( title[1], text[3] )
   end
end


function land_flf ()
   leave()
   last_system = nil
   if planet.cur():faction() == faction.get("FLF") then
      tk.msg( "", pay_text[1] )
      player.pay( credits )
      flf.setReputation( 98 )
      flf.addLog( log_text )
      misn.finish( true )
   end
end

