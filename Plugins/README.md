# Hercules_plugins
HPM Plugins made for Hercules Emulator

## @homgrowth.c
  Command can be used to know how your homunculus' stats growth are doing. The Tiers/Ranks used in this command is from [Doddler's Tool](http://ro.doddlercon.com/homunstats/).
  
    Usage: @homgrowth
   _Note: This is only compatible on pre-renewal setting._
    
## aegisdroprate.c
  This will add additional 0.01% drop rate to every item on drop table emulating the Aegis Drop Rate Bug.

## mobiddisplay.c
  Adds Mob ID beside the Mob Name in-game. ![Sample](https://ibb.co/pvRPZhNc)

## looternodelete.c
  Looter mobs will only pick up 10 items and will skip picking up new items when full. Picked-up items will not be deleted.

## dropannouncerate.c
  Adds announcement feature on rare drops (no DropAnnounce modification needed on itemdb). To configure, just edit the 'rate_announce' variable.
  
    int rate_announce = xx;
