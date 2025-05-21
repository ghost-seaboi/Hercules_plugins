//===== Hercules Plugin ======================================
//= LooterNoDelete
//===== By: ==================================================
//= Ghost / Seabois
//===== Current Version: =====================================
//= 1.0
//===== Description: =========================================
//= Looter mobs only pick up to 10 items, they will skip
//= picking up items when full.
//===== Changelog: ===========================================
//= v1.0 - Initial Conversion
//===== Additional Comments: =================================
//= 
//===== Repo Link: ===========================================
//= 
//============================================================
#include "common/hercules.h"


#include "common/utils.h"
#include "map/clif.h"
#include "map/mob.h"
#include "map/battle.h"
#include "map/mapdefines.h"

#include "map/log.h"
#include "map/map.h"
#include "map/pc.h"

#include "common/HPMi.h"
#include "common/cbasetypes.h"
#include "common/conf.h"
#include "common/ers.h"
#include "common/grfio.h"
#include "common/memmgr.h"
#include "common/mmo.h" // NEW_CARTS, char_achievements
#include "common/nullpo.h"
#include "common/packets.h"
#include "common/random.h"
#include "common/showmsg.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "common/timer.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#define ACTIVE_AI_RANGE 2 //Distance added on top of 'AREA_SIZE' at which mobs enter active AI mode.

#define IDLE_SKILL_INTERVAL 10 //Active idle skills should be triggered every 1 second (1000/MIN_MOBTHINKTIME)

// Probability for mobs far from players from doing their IDLE skill. (rate of 1000 minute)
// in Aegis, this is 100% for mobs that have been activated by players and none otherwise.
#define MOB_LAZYSKILLPERC(md) (md->state.spotted?1000:0)
// Move probability for mobs away from players (rate of 1000 minute)
// in Aegis, this is 100% for mobs that have been activated by players and none otherwise.
#define MOB_LAZYMOVEPERC(md) ((md)->state.spotted?1000:0)
#define MOB_MAX_CASTTIME (10 * 60 * 1000) // Maximum cast time for monster skills. (10 minutes)
#define MOB_MAX_DELAY (24*3600*1000)
#define MAX_MINCHASE 30 //Max minimum chase value to use for mobs.
#define RUDE_ATTACKED_COUNT 2 //After how many rude-attacks should the skill be used?

//Dynamic item drop ratio database for per-item drop ratio modifiers overriding global drop ratios.
#define MAX_ITEMRATIO_MOBS 10

HPExport struct hplugin_info pinfo = {
	"LooterNoDelete",		// Plugin name
	SERVER_TYPE_MAP,// Which server types this plugin works with?
	"1.0",			// Plugin version
	HPM_VERSION,	// HPM Version (don't change, macro is automatically updated)
};

static bool mob_ai_sub_hard_mine(struct mob_data *md, int64 tick)
{
	struct block_list *tbl = NULL, *abl = NULL;
	uint32 mode;
	int view_range, can_move;

	nullpo_retr(false, md);
	if(md->bl.prev == NULL || md->status.hp <= 0)
		return false;

	if (DIFF_TICK(tick, md->last_thinktime) < MIN_MOBTHINKTIME)
		return false;

	md->last_thinktime = tick;

	if (md->ud.skilltimer != INVALID_TIMER)
		return false;

	// Abnormalities
	if(( md->sc.opt1 > 0 && md->sc.opt1 != OPT1_STONEWAIT && md->sc.opt1 != OPT1_BURNING && md->sc.opt1 != OPT1_CRYSTALIZE )
	  || md->sc.data[SC_DEEP_SLEEP] || md->sc.data[SC_BLADESTOP] || md->sc.data[SC__MANHOLE] || md->sc.data[SC_CURSEDCIRCLE_TARGET]) {
		//Should reset targets.
		md->target_id = md->attacked_id = 0;
		return false;
	}

	if (md->sc.count && md->sc.data[SC_BLIND])
		view_range = 3;
	else
		view_range = md->db->range2;
	mode = status_get_mode(&md->bl);

	can_move = (mode&MD_CANMOVE)&&unit->can_move(&md->bl);

	if (md->target_id) {
		//Check validity of current target. [Skotlex]
		struct map_session_data *tsd = NULL;
		tbl = map->id2bl(md->target_id);
		tsd = BL_CAST(BL_PC, tbl);
		if (tbl == NULL || tbl->m != md->bl.m
		 || (md->ud.attacktimer == INVALID_TIMER && !status->check_skilluse(&md->bl, tbl, 0, 0))
		 || (md->ud.walktimer != INVALID_TIMER && !(battle->bc->mob_ai&0x1) && !check_distance_bl(&md->bl, tbl, md->min_chase))
		 || (tsd != NULL && ((tsd->state.gangsterparadise && !(mode&MD_BOSS)) || tsd->invincible_timer != INVALID_TIMER))
		) {
			//No valid target
			if (mob->warpchase(md, tbl))
				return true; //Chasing this target.
			if(md->ud.walktimer != INVALID_TIMER && (!can_move || md->ud.walkpath.path_pos <= battle->bc->mob_chase_refresh)
				&& (tbl || md->ud.walkpath.path_pos == 0))
				return true; //Walk at least "mob_chase_refresh" cells before dropping the target unless target is non-existent
			mob->unlocktarget(md, tick); //Unlock target
			tbl = NULL;
		}
	}

	// Check for target change.
	if (md->attacked_id && mode&MD_CANATTACK) {
		if (md->attacked_id == md->target_id) {
			//Rude attacked check.
			if (!battle->check_range(&md->bl, tbl, md->status.rhw.range)
			 && ( //Can't attack back and can't reach back.
			       (!can_move && DIFF_TICK(tick, md->ud.canmove_tick) > 0 && (battle->bc->mob_ai&0x2 || (md->sc.data[SC_SPIDERWEB] && md->sc.data[SC_SPIDERWEB]->val1)
			      || md->sc.data[SC_WUGBITE] || md->sc.data[SC_VACUUM_EXTREME] || md->sc.data[SC_THORNS_TRAP]
			      || md->sc.data[SC__MANHOLE] // Not yet confirmed if boss will teleport once it can't reach target.
			      || md->walktoxy_fail_count > 0)
			       )
			    || !mob->can_reach(md, tbl, md->min_chase, MSS_RUSH)
			    )
			 && md->state.attacked_count++ >= RUDE_ATTACKED_COUNT
			 && mob->use_skill(md, tick, MSC_RUDEATTACKED) != 0 // If can't rude Attack
			 && can_move != 0 && unit->attempt_escape(&md->bl, tbl, rnd() % 10 + 1) == 0 // Attempt escape
			) {
				//Escaped
				md->attacked_id = 0;
				return true;
			}
		}
		else
		if( (abl = map->id2bl(md->attacked_id)) && (!tbl || mob->can_changetarget(md, abl, mode) || (md->sc.count && md->sc.data[SC__CHAOS]))) {
			int dist;
			if( md->bl.m != abl->m || abl->prev == NULL
			 || (dist = distance_bl(&md->bl, abl)) >= MAX_MINCHASE // Attacker longer than visual area
			 || battle->check_target(&md->bl, abl, BCT_ENEMY) <= 0 // Attacker is not enemy of mob
			 || (battle->bc->mob_ai&0x2 && !status->check_skilluse(&md->bl, abl, 0, 0)) // Cannot normal attack back to Attacker
			 || (!battle->check_range(&md->bl, abl, md->status.rhw.range) // Not on Melee Range and ...
			    && ( // Reach check
					(!can_move && DIFF_TICK(tick, md->ud.canmove_tick) > 0 && (battle->bc->mob_ai&0x2 || (md->sc.data[SC_SPIDERWEB] && md->sc.data[SC_SPIDERWEB]->val1)
						|| md->sc.data[SC_WUGBITE] || md->sc.data[SC_VACUUM_EXTREME] || md->sc.data[SC_THORNS_TRAP]
						|| md->sc.data[SC__MANHOLE] // Not yet confirmed if boss will teleport once it can't reach target.
						|| md->walktoxy_fail_count > 0)
					)
					   || !mob->can_reach(md, abl, dist+md->db->range3, MSS_RUSH)
			       )
			    )
			) {
				// Rude attacked
				if (md->state.attacked_count++ >= RUDE_ATTACKED_COUNT
				 && mob->use_skill(md, tick, MSC_RUDEATTACKED) != 0 && can_move != 0
				 && tbl == NULL && unit->attempt_escape(&md->bl, abl, rnd() % 10 + 1) == 0
				) {
					//Escaped.
					//TODO: Maybe it shouldn't attempt to run if it has another, valid target?
					md->attacked_id = 0;
					return true;
				}
			}
			else
			if (!(battle->bc->mob_ai&0x2) && !status->check_skilluse(&md->bl, abl, 0, 0)) {
				//Can't attack back, but didn't invoke a rude attacked skill...
			} else {
				//Attackable
				if (!tbl || dist < md->status.rhw.range
				 || !check_distance_bl(&md->bl, tbl, dist)
				 || battle->get_target(tbl) != md->bl.id
				) {
					//Change if the new target is closer than the actual one
					//or if the previous target is not attacking the mob. [Skotlex]
					md->target_id = md->attacked_id; // set target
					if (md->state.attacked_count)
					  md->state.attacked_count--; //Should we reset rude attack count?
					md->min_chase = dist+md->db->range3;
					if(md->min_chase>MAX_MINCHASE)
						md->min_chase=MAX_MINCHASE;
					tbl = abl; //Set the new target
				}
			}
		}

		//Clear it since it's been checked for already.
		md->attacked_id = 0;
	}

	// Processing of slave monster
	if (md->master_id > 0 && mob->ai_sub_hard_slavemob(md, tick))
		return true;

	// Scan area for targets
	if (battle->bc->monster_loot_type != 1 && tbl == NULL && (mode & MD_LOOTER) != 0x0 && md->lootitem != NULL
	    && DIFF_TICK(tick, md->ud.canact_tick) > 0 && md->lootitem_count < LOOTITEM_SIZE) {
		// Scan area for items to loot, avoid trying to loot if the mob is full and can't consume the items.
		map->foreachinrange (mob->ai_sub_hard_lootsearch, &md->bl, view_range, BL_ITEM, md, &tbl);
	}

	if ((!tbl && mode&MD_AGGRESSIVE) || md->state.skillstate == MSS_FOLLOW) {
		map->foreachinrange(mob->ai_sub_hard_activesearch, &md->bl, view_range, DEFAULT_ENEMY_TYPE(md), md, &tbl, mode);
	} else if ((mode&MD_CHANGECHASE && (md->state.skillstate == MSS_RUSH || md->state.skillstate == MSS_FOLLOW)) || (md->sc.count && md->sc.data[SC__CHAOS])) {
		int search_size;
		search_size = view_range<md->status.rhw.range ? view_range:md->status.rhw.range;
		map->foreachinrange (mob->ai_sub_hard_changechase, &md->bl, search_size, DEFAULT_ENEMY_TYPE(md), md, &tbl);
	}

	if (!tbl) { //No targets available.
		if (mode&MD_ANGRY && !md->state.aggressive)
			md->state.aggressive = 1; //Restore angry state when no targets are available.

		/* bg guardians follow allies when no targets nearby */
		if( md->bg_id && mode&MD_CANATTACK ) {
			if( md->ud.walktimer != INVALID_TIMER )
				return true;/* we are already moving */
			map->foreachinrange (mob->ai_sub_hard_bg_ally, &md->bl, view_range, BL_PC, md, &tbl, mode);
			if( tbl ) {
				if (distance_blxy(&md->bl, tbl->x, tbl->y) <= 3 || unit->walk_tobl(&md->bl, tbl, 1, 1) == 0)
					return true;/* we're moving or close enough don't unlock the target. */
			}
		}

		//This handles triggering idle/walk skill.
		mob->unlocktarget(md, tick);
		return true;
	}

	//Target exists, attack or loot as applicable.
	if (tbl->type == BL_ITEM) {
		//Loot time.
		struct flooritem_data *fitem = BL_UCAST(BL_ITEM, tbl);
		if (md->ud.target == tbl->id && md->ud.walktimer != INVALID_TIMER)
			return true; //Already locked.
		if (md->lootitem == NULL) {
			//Can't loot...
			mob->unlocktarget (md, tick);
			return true;
		}
		if (!check_distance_bl(&md->bl, tbl, 1)) {
			//Still not within loot range.
			if (!(mode&MD_CANMOVE)) {
				//A looter that can't move? Real smart.
				mob->unlocktarget(md,tick);
				return true;
			}
			if (!can_move) //Stuck. Wait before walking.
				return true;
			md->state.skillstate = MSS_LOOT;
			if (unit->walk_tobl(&md->bl, tbl, 1, 1) != 0)
				mob->unlocktarget(md, tick); //Can't loot...
			return true;
		}
		//Within looting range.
		if (md->ud.attacktimer != INVALID_TIMER)
			return true; //Busy attacking?

		//Logs items, taken by (L)ooter Mobs [Lupus]
		logs->pick_mob(md, LOG_TYPE_LOOT, fitem->item_data.amount, &fitem->item_data, NULL);

		if (md->lootitem_count < LOOTITEM_SIZE) {
			memcpy (&md->lootitem[md->lootitem_count++], &fitem->item_data, sizeof(md->lootitem[0]));
		} else {
			//Inventory is full, do not pick up item.
			return; // skip picking up item
		}
		if (pc->db_checkid(md->vd->class)) {
			//Give them walk act/delay to properly mimic players. [Skotlex]
			clif->takeitem(&md->bl,tbl);
			md->ud.canact_tick = tick + md->status.amotion;
			unit->set_walkdelay(&md->bl, tick, md->status.amotion, 1);
		}
		//Clear item.
		map->clearflooritem (tbl);
		mob->unlocktarget (md,tick);
		return true;
	}

	//Attempt to attack.
	//At this point we know the target is attackable, we just gotta check if the range matches.
	if (battle->check_range(&md->bl, tbl, md->status.rhw.range) && !(md->sc.option&OPTION_HIDE)) {
		//Target within range and able to use normal attack, engage
		if (md->ud.target != tbl->id || md->ud.attacktimer == INVALID_TIMER)
		{ //Only attack if no more attack delay left
			if(tbl->type == BL_PC)
				mob->log_damage(md, tbl, 0); //Log interaction (counts as 'attacker' for the exp bonus)
			unit->attack(&md->bl,tbl->id,1);
		}
		return true;
	}

	//Monsters in berserk state, unable to use normal attacks, will always attempt a skill
	if(md->ud.walktimer == INVALID_TIMER && (md->state.skillstate == MSS_BERSERK || md->state.skillstate == MSS_ANGRY)) {
		if (DIFF_TICK(md->ud.canmove_tick, tick) <= MIN_MOBTHINKTIME && DIFF_TICK(md->ud.canact_tick, tick) < -MIN_MOBTHINKTIME*IDLE_SKILL_INTERVAL)
		{ //Only use skill if able to walk on next tick and not used a skill the last second
			mob->use_skill(md, tick, -1);
		}
	}

	//Target still in attack range, no need to chase the target
	if(battle->check_range(&md->bl, tbl, md->status.rhw.range))
		return true;

	//Only update target cell / drop target after having moved at least "mob_chase_refresh" cells
	if(md->ud.walktimer != INVALID_TIMER && (!can_move || md->ud.walkpath.path_pos <= battle->bc->mob_chase_refresh))
		return true;

	//Out of range...
	if (!(mode&MD_CANMOVE) || (!can_move && DIFF_TICK(tick, md->ud.canmove_tick) > 0)) {
		//Can't chase. Immobile and trapped mobs should unlock target and use an idle skill.
		if (md->ud.attacktimer == INVALID_TIMER)
		{ //Only unlock target if no more attack delay left
			//This handles triggering idle/walk skill.
			mob->unlocktarget(md,tick);
		}
		return true;
	}

	if (md->ud.walktimer != INVALID_TIMER && md->ud.target == tbl->id &&
		(
			!(battle->bc->mob_ai&0x1) ||
			check_distance_blxy(tbl, md->ud.to_x, md->ud.to_y, md->status.rhw.range)
	)) //Current target tile is still within attack range.
		return true;

	//Follow up if possible.
	//Hint: Chase skills are handled in the walktobl routine
	if (mob->can_reach(md, tbl, md->min_chase, MSS_RUSH) == 0
	    || unit->walk_tobl(&md->bl, tbl, md->status.rhw.range, 2) != 0)
		mob->unlocktarget(md,tick);

	return true;
}

HPExport void plugin_init(void) {
	mob->ai_sub_hard = mob_ai_sub_hard_mine;
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Ghost/Seabois. Version '%s'\n", pinfo.name, pinfo.version);
}