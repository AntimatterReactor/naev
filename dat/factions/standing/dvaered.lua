-- Dvaered faction standing script
require "factions.standing.lib.base"

_fcap_kill     = 25 -- Kill cap
_fdelta_distress = {-0.5, 0} -- Maximum change constraints
_fdelta_kill     = {-5, 1.5} -- Maximum change constraints
_fcap_misn     = 40 -- Starting mission cap, gets overwritten
_fcap_misn_var = "_fcap_dvaered"
_fthis         = faction.get("Dvaered")
