//===== Hercules Plugin ======================================
//= Adds Drop Announcement to items with certain drop rates.
//===== By: ==================================================
//= Ghost / Seabois
//===== Current Version: =====================================
//= 1.0
//===== Description: =========================================
//= Adds drop announce function based on items' drop rate.
//= Set the variable "rate_announce" to whatever drop rate
//= preferred. Eg. 10 if 0.1% and below.
//===== Changelog: ===========================================
//= v1.0 - Initial Conversion
//===== Additional Comments: =================================
//= 
//===== Repo Link: ===========================================
//= 
//============================================================

#include "common/hercules.h"

#include "config/core.h" // AUTOLOOT_DISTANCE, DBPATH, DEFTYPE_MAX, DEFTYPE_MIN, RENEWAL_DROP, RENEWAL_EXP
#include "map/mob.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/clif.h"
#include "map/date.h"
#include "map/elemental.h"
#include "map/guild.h"
#include "map/homunculus.h"
#include "map/intif.h"
#include "map/itemdb.h"
#include "map/log.h"
#include "map/map.h"
#include "map/mercenary.h"
#include "map/npc.h"
#include "map/party.h"
#include "map/path.h"
#include "map/pc.h"
#include "map/pet.h"
#include "map/quest.h"
#include "map/script.h"
#include "map/skill.h"
#include "map/status.h"
#include "map/achievement.h"
#include "common/HPMi.h"
#include "common/cbasetypes.h"
#include "common/conf.h"
#include "common/db.h"
#include "common/ers.h"
#include "common/memmgr.h"
#include "common/nullpo.h"
#include "common/random.h"
#include "common/showmsg.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "common/timer.h"
#include "common/utils.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"



#define MAX_ITEMRATIO_MOBS 10
#define ERS_BLOCK_ENTRIES 2048

HPExport struct hplugin_info pinfo = {
	"DropAnnounceRate",		// Plugin name
	SERVER_TYPE_MAP,// Which server types this plugin works with?
	"1.0",			// Plugin version
	HPM_VERSION,	// HPM Version (don't change, macro is automatically updated)
};

int rate_announce = 11;

struct item_drop_ratio {
	int drop_ratio;
	int mob_id[MAX_ITEMRATIO_MOBS];
};
static struct item_drop_ratio *item_drop_ratio_db[MAX_ITEMDB];
static struct DBMap *item_drop_ratio_other_db = NULL;

static struct eri *item_drop_list_ers = NULL;

static struct mob_db *mob_db(int index)
{
	if (index < 0 || index > MAX_MOB_DB || mob->db_data[index] == NULL)
		return mob->dummy;
	return mob->db_data[index];
}

// Define of ERS
struct ers_list
{
	struct ers_list *Next;
};

struct ers_instance_t;

typedef struct ers_cache
{
	// Allocated object size, including ers_list size
	unsigned int ObjectSize;

	// Number of ers_instances referencing this
	int ReferenceCount;

	// Reuse linked list
	struct ers_list *ReuseList;

	// Memory blocks array
	unsigned char **Blocks;

	// Max number of blocks
	unsigned int Max;

	// Free objects count
	unsigned int Free;

	// Used blocks count
	unsigned int Used;

	// Objects in-use count
	unsigned int UsedObjs;

	// Default = ERS_BLOCK_ENTRIES, can be adjusted for performance for individual cache sizes.
	unsigned int ChunkSize;

	// Misc options, some options are shared from the instance
	enum ERSOptions Options;

	// Linked list
	struct ers_cache *Next, *Prev;
} ers_cache_t;

struct ers_instance_t {
	// Interface to ERS
	struct eri VTable;

	// Name, used for debugging purposes
	char *Name;

	// Misc options
	enum ERSOptions Options;

	// Our cache
	ers_cache_t *Cache;

	// Count of objects in use, used for detecting memory leaks
	unsigned int Count;

#ifdef DEBUG
	/* for data analysis [Ind/Hercules] */
	unsigned int Peak;
#endif
	struct ers_instance_t *Next, *Prev;
};


// Array containing a pointer for all ers_cache structures
static ers_cache_t *CacheList = NULL;
static struct ers_instance_t *InstanceList = NULL;

static void *ers_obj_alloc_entry(ERS *self)
{
	struct ers_instance_t *instance = (struct ers_instance_t *)self;
	void *ret;

	if (instance == NULL) {
		ShowError("ers_obj_alloc_entry: NULL object, aborting entry freeing.\n");
		return NULL;
	}

	if (instance->Cache->ReuseList != NULL) {
		ret = (void *)((unsigned char *)instance->Cache->ReuseList + sizeof(struct ers_list));
		instance->Cache->ReuseList = instance->Cache->ReuseList->Next;
	} else if (instance->Cache->Free > 0) {
		instance->Cache->Free--;
		ret = &instance->Cache->Blocks[instance->Cache->Used - 1][instance->Cache->Free * instance->Cache->ObjectSize + sizeof(struct ers_list)];
	} else {
		if (instance->Cache->Used == instance->Cache->Max) {
			instance->Cache->Max = (instance->Cache->Max * 4) + 3;
			RECREATE(instance->Cache->Blocks, unsigned char *, instance->Cache->Max);
		}

		CREATE(instance->Cache->Blocks[instance->Cache->Used], unsigned char, instance->Cache->ObjectSize * instance->Cache->ChunkSize);
		instance->Cache->Used++;

		instance->Cache->Free = instance->Cache->ChunkSize -1;
		ret = &instance->Cache->Blocks[instance->Cache->Used - 1][instance->Cache->Free * instance->Cache->ObjectSize + sizeof(struct ers_list)];
	}

	instance->Count++;
	instance->Cache->UsedObjs++;

#ifdef DEBUG
	if( instance->Count > instance->Peak )
		instance->Peak = instance->Count;
#endif

	return ret;
}

static void ers_obj_free_entry(ERS *self, void *entry)
{
	struct ers_instance_t *instance = (struct ers_instance_t *)self;
	struct ers_list *reuse = (struct ers_list *)((unsigned char *)entry - sizeof(struct ers_list));

	if (instance == NULL) {
		ShowError("ers_obj_free_entry: NULL object, aborting entry freeing.\n");
		return;
	} else if (entry == NULL) {
		ShowError("ers_obj_free_entry: NULL entry, nothing to free.\n");
		return;
	}

	if( instance->Cache->Options & ERS_OPT_CLEAN )
		memset((unsigned char*)reuse + sizeof(struct ers_list), 0, instance->Cache->ObjectSize - sizeof(struct ers_list));

	reuse->Next = instance->Cache->ReuseList;
	instance->Cache->ReuseList = reuse;
	instance->Count--;
	instance->Cache->UsedObjs--;
}

static size_t ers_obj_entry_size(ERS *self)
{
	struct ers_instance_t *instance = (struct ers_instance_t *)self;

	if (instance == NULL) {
		ShowError("ers_obj_entry_size: NULL object, aborting entry freeing.\n");
		return 0;
	}

	return instance->Cache->ObjectSize;
}

static void ers_free_cache(ers_cache_t *cache, bool remove)
{
	unsigned int i;

	nullpo_retv(cache);
	for (i = 0; i < cache->Used; i++)
		aFree(cache->Blocks[i]);

	if (cache->Next)
		cache->Next->Prev = cache->Prev;

	if (cache->Prev)
		cache->Prev->Next = cache->Next;
	else
		CacheList = cache->Next;

	aFree(cache->Blocks);

	aFree(cache);
}

static void ers_obj_destroy(ERS *self)
{
	struct ers_instance_t *instance = (struct ers_instance_t *)self;

	if (instance == NULL) {
		ShowError("ers_obj_destroy: NULL object, aborting entry freeing.\n");
		return;
	}

	if (instance->Count > 0)
		if (!(instance->Options & ERS_OPT_CLEAR))
			ShowWarning("Memory leak detected at ERS '%s', %u objects not freed.\n", instance->Name, instance->Count);

	if (--instance->Cache->ReferenceCount <= 0)
		ers_free_cache(instance->Cache, true);

	if (instance->Next)
		instance->Next->Prev = instance->Prev;

	if (instance->Prev)
		instance->Prev->Next = instance->Next;
	else
		InstanceList = instance->Next;

	if( instance->Options & ERS_OPT_FREE_NAME )
		aFree(instance->Name);

	aFree(instance);
}

static void ers_cache_size(ERS *self, unsigned int new_size)
{
	struct ers_instance_t *instance = (struct ers_instance_t *)self;

	nullpo_retv(instance);

	if( !(instance->Cache->Options&ERS_OPT_FLEX_CHUNK) ) {
		ShowWarning("ers_cache_size: '%s' has adjusted its chunk size to '%u', however ERS_OPT_FLEX_CHUNK is missing!\n", instance->Name, new_size);
	}

	instance->Cache->ChunkSize = new_size;
}

/* static void ers_free_cache(ers_cache_t *cache, bool remove)
{
	unsigned int i;

	nullpo_retv(cache);
	for (i = 0; i < cache->Used; i++)
		aFree(cache->Blocks[i]);

	if (cache->Next)
		cache->Next->Prev = cache->Prev;

	if (cache->Prev)
		cache->Prev->Next = cache->Next;
	else
		CacheList = cache->Next;

	aFree(cache->Blocks);

	aFree(cache);
} */

static ers_cache_t *ers_find_cache(unsigned int size, enum ERSOptions Options)
{
	ers_cache_t *cache;

	for (cache = CacheList; cache; cache = cache->Next)
		if ( cache->ObjectSize == size && cache->Options == ( Options & ERS_CACHE_OPTIONS ) )
			return cache;

	CREATE(cache, ers_cache_t, 1);
	cache->ObjectSize = size;
	cache->ReferenceCount = 0;
	cache->ReuseList = NULL;
	cache->Blocks = NULL;
	cache->Free = 0;
	cache->Used = 0;
	cache->UsedObjs = 0;
	cache->Max = 0;
	cache->ChunkSize = ERS_BLOCK_ENTRIES;
	cache->Options = (Options & ERS_CACHE_OPTIONS);

	if (CacheList == NULL)
	{
		CacheList = cache;
	}
	else
	{
		cache->Next = CacheList;
		cache->Next->Prev = cache;
		CacheList = cache;
		CacheList->Prev = NULL;
	}

	return cache;
}

ERS *ers_new(uint32 size, char *name, enum ERSOptions options)
{
	struct ers_instance_t *instance;

	nullpo_retr(NULL, name);
	CREATE(instance,struct ers_instance_t, 1);

	size += sizeof(struct ers_list);

#if ERS_ALIGNED > 1 // If it's aligned to 1-byte boundaries, no need to bother.
	if (size % ERS_ALIGNED)
		size += ERS_ALIGNED - size % ERS_ALIGNED;
#endif

	instance->VTable.alloc = ers_obj_alloc_entry;
	instance->VTable.free = ers_obj_free_entry;
	instance->VTable.entry_size = ers_obj_entry_size;
	instance->VTable.destroy = ers_obj_destroy;
	instance->VTable.chunk_size = ers_cache_size;

	instance->Name = ( options & ERS_OPT_FREE_NAME ) ? aStrdup(name) : name;
	instance->Options = options;

	instance->Cache = ers_find_cache(size,instance->Options);

	instance->Cache->ReferenceCount++;

	if (InstanceList == NULL) {
		InstanceList = instance;
	} else {
		instance->Next = InstanceList;
		instance->Next->Prev = instance;
		InstanceList = instance;
		InstanceList->Prev = NULL;
	}

	instance->Count = 0;

	return &instance->VTable;
}

int64 apply_percentrate64(int64 value, int rate, int stdrate)
{
	Assert_ret(stdrate > 0);
	Assert_ret(rate >= 0);
	if (rate == stdrate)
		return value;
	if (rate == 0)
		return 0;
	if (INT64_MAX / rate < value) {
		// Give up some precision to prevent overflows
		return value / stdrate * rate;
	}
	return value * rate / stdrate;
}



static int mob_dead_mine(struct mob_data *md, struct block_list *src, int type)
{
	GUARD_MAP_LOCK
	
	struct status_data *mstatus;
	struct map_session_data *sd = BL_CAST(BL_PC, src);
	struct map_session_data *tmpsd[DAMAGELOG_SIZE] = { NULL };
	struct map_session_data *mvp_sd = sd, *second_sd = NULL, *third_sd = NULL;
	struct item_data *id = NULL;

	struct {
		struct party_data *p;
		int id,zeny;
		unsigned int base_exp,job_exp;
	} pt[DAMAGELOG_SIZE] = { { 0 } };
	int i, temp, count, m;
	int dmgbltypes = 0;  // bitfield of all bl types, that caused damage to the mob and are eligible for exp distribution
	unsigned int mvp_damage;
	int64 tick = timer->gettick();
	bool rebirth, homkillonly;

	nullpo_retr(3, md);
	m = md->bl.m;
	Assert_retr(false, m >= 0 && m < map->count);
	mstatus = &md->status;
	
	if( md->guardian_data && md->guardian_data->number >= 0 && md->guardian_data->number < MAX_GUARDIANS )
		guild->castledatasave(md->guardian_data->castle->castle_id, 10+md->guardian_data->number,0);

	if( src )
	{ // Use Dead skill only if not killed by Script or Command
		md->state.skillstate = MSS_DEAD;
		mob->use_skill(md, tick, -1);
	}

	map->freeblock_lock();

	if (src != NULL && src->type == BL_MOB)
		mob->unlocktarget(BL_UCAST(BL_MOB, src), tick);

	// filter out entries not eligible for exp distribution
	for(i = 0, count = 0, mvp_damage = 0; i < DAMAGELOG_SIZE && md->dmglog[i].id; i++) {
		struct map_session_data* tsd = map->charid2sd(md->dmglog[i].id);

		if(tsd == NULL)
			continue; // skip empty entries
		if(tsd->bl.m != m)
			continue; // skip players not on this map
		count++; //Only logged into same map chars are counted for the total.
		if (pc_isdead(tsd))
			continue; // skip dead players
		if(md->dmglog[i].flag == MDLF_HOMUN && !homun_alive(tsd->hd))
			continue; // skip homunc's share if inactive
		if( md->dmglog[i].flag == MDLF_PET && (!tsd->status.pet_id || !tsd->pd) )
			continue; // skip pet's share if inactive

		if(md->dmglog[i].dmg > mvp_damage) {
			third_sd = second_sd;
			second_sd = mvp_sd;
			mvp_sd = tsd;
			mvp_damage = md->dmglog[i].dmg;
		}

		tmpsd[i] = tsd; // record as valid damage-log entry

		switch( md->dmglog[i].flag ) {
			case MDLF_NORMAL: dmgbltypes|= BL_PC;  break;
			case MDLF_HOMUN:  dmgbltypes|= BL_HOM; break;
			case MDLF_PET:    dmgbltypes|= BL_PET; break;
		}
	}
		
	// determines, if the monster was killed by homunculus' damage only
	homkillonly = (bool)( ( dmgbltypes&BL_HOM ) && !( dmgbltypes&~BL_HOM ) );

	if (!battle->bc->exp_calc_type && count > 1) {
		//Apply first-attacker 200% exp share bonus
		//TODO: Determine if this should go before calculating the MVP player instead of after.
		if (UINT_MAX - md->dmglog[0].dmg > md->tdmg) {
			md->tdmg += md->dmglog[0].dmg;
			md->dmglog[0].dmg<<=1;
		} else {
			md->dmglog[0].dmg+= UINT_MAX - md->tdmg;
			md->tdmg = UINT_MAX;
		}
	}

	if( !(type&2) //No exp
	 && (!map->list[m].flag.pvp || battle->bc->pvp_exp) //Pvp no exp rule [MouseJstr]
	 && (!md->master_id || md->special_state.ai == AI_NONE) //Only player-summoned mobs do not give exp. [Skotlex]
	 && (!map->list[m].flag.nobaseexp || !map->list[m].flag.nojobexp) //Gives Exp
	) { //Experience calculation.
		int bonus = 100; //Bonus on top of your share (common to all attackers).
		int pnum = 0;
		if (md->sc.data[SC_RICHMANKIM])
			bonus += md->sc.data[SC_RICHMANKIM]->val2;
		if(sd) {
			temp = status->get_class(&md->bl);
			if(sd->sc.data[SC_MIRACLE]) i = 2; //All mobs are Star Targets
			else
			ARR_FIND(0, MAX_PC_FEELHATE, i, temp == sd->hate_mob[i] &&
				(battle->bc->allow_skill_without_day || pc->sg_info[i].day_func()));
			if(i<MAX_PC_FEELHATE && (temp=pc->checkskill(sd,pc->sg_info[i].bless_id)) > 0)
				bonus += (i==2?20:10)*temp;
		}
		if(battle->bc->mobs_level_up && md->level > md->db->lv) // [Valaris]
			bonus += (md->level-md->db->lv)*battle->bc->mobs_level_up_exp_rate;
	
	for(i = 0; i < DAMAGELOG_SIZE && md->dmglog[i].id; i++) {
		int flag=1,zeny=0;
		unsigned int base_exp, job_exp;
		double per; //Your share of the mob's exp

		if (!tmpsd[i]) continue;

		if (!battle->bc->exp_calc_type && md->tdmg)
			//jAthena's exp formula based on total damage.
			per = (double)md->dmglog[i].dmg/(double)md->tdmg;
		else {
			//eAthena's exp formula based on max hp.
			per = (double)md->dmglog[i].dmg/(double)mstatus->max_hp;
			if (per > 2) per = 2; // prevents unlimited exp gain
		}

		if (count>1 && battle->bc->exp_bonus_attacker) {
			//Exp bonus per additional attacker.
			if (count > battle->bc->exp_bonus_max_attacker)
				count = battle->bc->exp_bonus_max_attacker;
			per += per*((count-1)*battle->bc->exp_bonus_attacker)/100.;
		}

		// change experience for different sized monsters [Valaris]
		if (battle->bc->mob_size_influence) {
			switch( md->special_state.size ) {
				case SZ_MEDIUM:
					per /= 2.;
					break;
				case SZ_BIG:
					per *= 2.;
					break;
			}
		}

		if( md->dmglog[i].flag == MDLF_PET )
			per *= battle->bc->pet_attack_exp_rate/100.;

		if(battle->bc->zeny_from_mobs && md->level) {
			 // zeny calculation moblv + random moblv [Valaris]
			zeny=(int) ((md->level+rnd()%md->level)*per*bonus/100.);
			if(md->db->mexp > 0)
				zeny*=rnd()%250;
		}

		if (map->list[m].flag.nobaseexp || !md->db->base_exp)
			base_exp = 0;
		else
			base_exp = (unsigned int)cap_value(md->db->base_exp * per * bonus/100. * map->list[m].bexp/100., 1, UINT_MAX);

		if (map->list[m].flag.nojobexp || !md->db->job_exp || md->dmglog[i].flag == MDLF_HOMUN) //Homun earned job-exp is always lost.
			job_exp = 0;
		else
			job_exp = (unsigned int)cap_value(md->db->job_exp * per * bonus/100. * map->list[m].jexp/100., 1, UINT_MAX);

		if ( (temp = tmpsd[i]->status.party_id) > 0 ) {
			int j;
			for( j = 0; j < pnum && pt[j].id != temp; j++ ); //Locate party.

			if( j == pnum ){ //Possibly add party.
				pt[pnum].p = party->search(temp);
				if(pt[pnum].p && pt[pnum].p->party.exp) {
					pt[pnum].id = temp;
					pt[pnum].base_exp = base_exp;
					pt[pnum].job_exp = job_exp;
					pt[pnum].zeny = zeny; // zeny share [Valaris]
					pnum++;
					flag=0;
				}
			} else {
				//Add to total
				if (pt[j].base_exp > UINT_MAX - base_exp)
					pt[j].base_exp = UINT_MAX;
				else
					pt[j].base_exp += base_exp;

				if (pt[j].job_exp > UINT_MAX - job_exp)
					pt[j].job_exp = UINT_MAX;
				else
					pt[j].job_exp += job_exp;

				pt[j].zeny+=zeny;  // zeny share [Valaris]
				flag=0;
			}
		}
		if(base_exp && md->dmglog[i].flag == MDLF_HOMUN) //tmpsd[i] is null if it has no homunc.
			homun->gainexp(tmpsd[i]->hd, base_exp);
		if(flag) {
			if(base_exp || job_exp) {
				if( md->dmglog[i].flag != MDLF_PET || battle->bc->pet_attack_exp_to_master ) {
					pc->gainexp(tmpsd[i], &md->bl, base_exp, job_exp, false);
				}
			}
			if(zeny) // zeny from mobs [Valaris]
				pc->getzeny(tmpsd[i], zeny, LOG_TYPE_PICKDROP_MONSTER, NULL);

			if (!md->special_state.clone && !mob->is_clone(md->class_))
				achievement->validate_mob_kill(tmpsd[i], md->db->mob_id); // Achievements [Smokexyz/Hercules]
		}
	}
	
	for( i = 0; i < pnum; i++ ) //Party share.
		party->exp_share(pt[i].p, &md->bl, pt[i].base_exp,pt[i].job_exp,pt[i].zeny);

	} //End EXP giving.
	
	if( !(type&1) && !map->list[m].flag.nomobloot && !md->state.rebirth && (
		md->special_state.ai == AI_NONE || //Non special mob
		battle->bc->alchemist_summon_reward == 2 || //All summoned give drops
		(md->special_state.ai == AI_SPHERE && battle->bc->alchemist_summon_reward == 1) //Marine Sphere Drops items.
		) )
	{ // Item Drop
		if (item_drop_list_ers == NULL) {
			item_drop_list_ers = ers_new(sizeof(struct item_drop_list), "plugin::item_drop_list_ers", ERS_OPT_NONE);
			if (!item_drop_list_ers) {
				printf("Failed to initialize item_drop_list_ers\n");
				return;
			}
		}
		struct item_drop_list *dlist = ers_alloc(item_drop_list_ers, struct item_drop_list);
		struct item_drop *ditem;
		struct item_data* it = NULL;
		int drop_rate;
		
#ifdef RENEWAL_DROP
		int drop_modifier = mvp_sd    ? pc->level_penalty_mod( md->level - mvp_sd->status.base_level, md->status.race, md->status.mode, 2)   :
							second_sd ? pc->level_penalty_mod( md->level - second_sd->status.base_level, md->status.race, md->status.mode, 2):
							third_sd  ? pc->level_penalty_mod( md->level - third_sd->status.base_level, md->status.race, md->status.mode, 2) :
							100;/* no player was attached, we don't use any modifier (100 = rates are not touched) */
#endif
		
		dlist->m = md->bl.m;
		dlist->x = md->bl.x;
		dlist->y = md->bl.y;
		dlist->first_charid = (mvp_sd ? mvp_sd->status.char_id : 0);
		dlist->second_charid = (second_sd ? second_sd->status.char_id : 0);
		dlist->third_charid = (third_sd ? third_sd->status.char_id : 0);
		dlist->item = NULL;
		
		for (i = 0; i < MAX_MOB_DROP; i++)
		{
			if (md->db->dropitem[i].nameid <= 0)
				continue;
			if ( !(it = itemdb->exists(md->db->dropitem[i].nameid)) )
				continue;
			drop_rate = md->db->dropitem[i].p;

			// change drops depending on monsters size [Valaris]
			if (battle->bc->mob_size_influence) {
				if (md->special_state.size == SZ_MEDIUM && drop_rate >= 2)
					drop_rate /= 2;
				else if( md->special_state.size == SZ_BIG)
					drop_rate *= 2;
			}

			if (src != NULL) {
				//Drops affected by luk as a fixed increase [Valaris]
				if (battle->bc->drops_by_luk)
					drop_rate += status_get_luk(src) * battle->bc->drops_by_luk / 100;

				//Drops affected by luk as a % increase [Skotlex]
				if (battle->bc->drops_by_luk2)
					drop_rate += (int)(0.5 + drop_rate * status_get_luk(src) * battle->bc->drops_by_luk2 / 10000.);

				if (sd != NULL) {
					int drop_rate_bonus = 100;

					// When PK Mode is enabled, increase item drop rate bonus of each items by 25% when there is a 20 level difference between the player and the monster.[KeiKun]
					if (battle->bc->pk_mode && (md->level - sd->status.base_level >= 20))
						drop_rate_bonus += 25; // flat 25% bonus 

					drop_rate_bonus += sd->dropaddrace[md->status.race] + (is_boss(src) ? sd->dropaddrace[RC_BOSS] : sd->dropaddrace[RC_NONBOSS]); // bonus2 bDropAddRace[KeiKun]

					if (sd->sc.data[SC_CASH_RECEIVEITEM] != NULL) // Increase drop rate if user has SC_CASH_RECEIVEITEM
						drop_rate_bonus += sd->sc.data[SC_CASH_RECEIVEITEM]->val1;

					if (sd->sc.data[SC_OVERLAPEXPUP] != NULL)
						drop_rate_bonus += sd->sc.data[SC_OVERLAPEXPUP]->val2;

					drop_rate = (int)(0.5 + drop_rate * drop_rate_bonus / 100.);

					// Limit drop rate, default: 90%
					drop_rate = min(drop_rate, 9000);
				}
			}

#ifdef RENEWAL_DROP
			if (drop_modifier != 100) {
				drop_rate = drop_rate * drop_modifier / 100;
				if (drop_rate < 1)
					drop_rate = 1;
			}
#endif
			if (sd != NULL && sd->status.mod_drop != 100) {
				drop_rate = drop_rate * sd->status.mod_drop / 100;
				if (drop_rate < 1)
					drop_rate = 1;
			}

			if (battle->bc->drop_rate0item)
				drop_rate = max(drop_rate, 0);
			else
				drop_rate = max(drop_rate, 1);

			// attempt to drop the item
			if (rnd() % 10000 >= drop_rate)
					continue;

			if( mvp_sd && it->type == IT_PETEGG ) {
				pet->create_egg(mvp_sd, md->db->dropitem[i].nameid);
				continue;
			}

			ditem = mob->setdropitem(md->db->dropitem[i].nameid, md->db->dropitem[i].options, 1, it);
			
			// Official Drop Announce [Jedzkie]
			if (mvp_sd != NULL) {
				if ((id = itemdb->search(it->nameid)) != NULL && drop_rate <= rate_announce) {
					clif->item_drop_announce(mvp_sd, it->nameid, md->name);
				}
			}

			// Announce first, or else ditem will be freed. [Lance]
			// By popular demand, use base drop rate for autoloot code. [Skotlex]
			mob->item_drop(md, dlist, ditem, 0, battle->bc->autoloot_adjust ? drop_rate : md->db->dropitem[i].p, homkillonly);
		}
		
		// Ore Discovery [Celest]
		if (sd == mvp_sd && pc->checkskill(sd,BS_FINDINGORE) > 0) {
			if( (temp = itemdb->chain_item(itemdb->chain_cache[ECC_ORE],&i)) ) {
				ditem = mob->setdropitem(temp, NULL, 1, NULL);
				mob->item_drop(md, dlist, ditem, 0, i, homkillonly);
			}
		}

		if(sd) {
			// process script-granted extra drop bonuses
			int itemid = 0;
			for (i = 0; i < ARRAYLENGTH(sd->add_drop) && (sd->add_drop[i].id != 0 || sd->add_drop[i].is_group); i++)
			{
				if ( sd->add_drop[i].race == -md->class_ ||
					( sd->add_drop[i].race > 0 && (
						sd->add_drop[i].race & map->race_id2mask(mstatus->race) ||
						sd->add_drop[i].race & map->race_id2mask((mstatus->mode&MD_BOSS) ? RC_BOSS : RC_NONBOSS)
					)))
				{
					//check if the bonus item drop rate should be multiplied with mob level/10 [Lupus]
					if(sd->add_drop[i].rate < 0) {
						//it's negative, then it should be multiplied. e.g. for Mimic,Myst Case Cards, etc
						// rate = base_rate * (mob_level/10) + 1
						drop_rate = -sd->add_drop[i].rate*(md->level/10)+1;
						drop_rate = cap_value(drop_rate, battle->bc->item_drop_adddrop_min, battle->bc->item_drop_adddrop_max);
						if (drop_rate > 10000) drop_rate = 10000;
					}
					else
						//it's positive, then it goes as it is
						drop_rate = sd->add_drop[i].rate;

					if (rnd()%10000 >= drop_rate)
						continue;
					itemid = (!sd->add_drop[i].is_group) ? sd->add_drop[i].id : itemdb->chain_item(sd->add_drop[i].id, &drop_rate);
					if( itemid )
						mob->item_drop(md, dlist, mob->setdropitem(itemid, NULL, 1, NULL), 0, drop_rate, homkillonly);
				}
			}

			// process script-granted zeny bonus (get_zeny_num) [Skotlex]
			if( sd->bonus.get_zeny_num && rnd()%100 < sd->bonus.get_zeny_rate ) {
				i = sd->bonus.get_zeny_num > 0 ? sd->bonus.get_zeny_num : -md->level * sd->bonus.get_zeny_num;
				if (!i) i = 1;
				pc->getzeny(sd, 1+rnd()%i, LOG_TYPE_PICKDROP_MONSTER, NULL);
			}
		}

		// process items looted by the mob
		if(md->lootitem) {
			for(i = 0; i < md->lootitem_count; i++)
				mob->item_drop(md, dlist, mob->setlootitem(&md->lootitem[i]), 1, 10000, homkillonly);
		}
		if (dlist->item) //There are drop items.
			timer->add(tick + (!battle->bc->delay_battle_damage?500:0), mob->delay_item_drop, 0, (intptr_t)dlist);
		else //No drops
			ers_free(item_drop_list_ers, dlist);
	} else if (md->lootitem && md->lootitem_count) {
		//Loot MUST drop!
		
		struct item_drop_list *dlist = ers_alloc(item_drop_list_ers, struct item_drop_list);
		dlist->m = md->bl.m;
		dlist->x = md->bl.x;
		dlist->y = md->bl.y;
		dlist->first_charid = (mvp_sd ? mvp_sd->status.char_id : 0);
		dlist->second_charid = (second_sd ? second_sd->status.char_id : 0);
		dlist->third_charid = (third_sd ? third_sd->status.char_id : 0);
		dlist->item = NULL;
		for(i = 0; i < md->lootitem_count; i++)
			mob->item_drop(md, dlist, mob->setlootitem(&md->lootitem[i]), 1, 10000, homkillonly);
		timer->add(tick + (!battle->bc->delay_battle_damage?500:0), mob->delay_item_drop, 0, (intptr_t)dlist);
	}
	
	if(mvp_sd && md->db->mexp > 0 && md->special_state.ai == AI_NONE) {
		int log_mvp[2] = {0};
		unsigned int mexp;
		int64 exp;

		//mapflag: noexp check [Lorky]
		if (map->list[m].flag.nobaseexp || type&2) {
			exp = 1;
		} else {
			exp = md->db->mexp;
			if (count > 1)
				exp += apply_percentrate64(exp, battle->bc->exp_bonus_attacker * (count-1), 100); //[Gengar]
		}

		mexp = (unsigned int)cap_value(exp, 1, UINT_MAX);

		clif->mvp_effect(mvp_sd);
		clif->mvp_exp(mvp_sd,mexp);
		pc->gainexp(mvp_sd, &md->bl, mexp,0, false);
		log_mvp[1] = mexp;

		if (!(map->list[m].flag.nomvploot || type&1)) {
			/* pose them randomly in the list -- so on 100% drop servers it wont always drop the same item */
			struct mob_drop mdrop[MAX_MVP_DROP] = { { 0 } };

			for (i = 0; i < MAX_MVP_DROP; i++) {
				int rpos;
				if (md->db->mvpitem[i].nameid == 0)
					continue;
				do {
					rpos = rnd()%MAX_MVP_DROP;
				} while (mdrop[rpos].nameid != 0);

				mdrop[rpos].nameid = md->db->mvpitem[i].nameid;
				mdrop[rpos].p = md->db->mvpitem[i].p;
				mdrop[rpos].options = md->db->mvpitem[i].options;
			}

			for (i = 0; i < MAX_MVP_DROP; i++) {
				struct item_data *data = NULL;
				int rate = 0;

				if (mdrop[i].nameid <= 0)
					continue;
				if ((data = itemdb->exists(mdrop[i].nameid)) == NULL)
					continue;

				rate = mdrop[i].p;
				if (rate <= 0 && !battle->bc->drop_rate0item)
					rate = 1;
				if (rate > rnd()%10000) {
					struct item item = { 0 };

					item.nameid = mdrop[i].nameid;
					item.identify = itemdb->isidentified2(data);
					if (mdrop[i].options != NULL)
						mob->setdropitem_options(&item, mdrop[i].options);
					clif->mvp_item(mvp_sd, item.nameid);
					log_mvp[0] = item.nameid;

					if((temp = pc->additem(mvp_sd,&item,1,LOG_TYPE_PICKDROP_PLAYER)) != 0) {
						clif->additem(mvp_sd,0,0,temp);
						map->addflooritem(&md->bl, &item, 1, mvp_sd->bl.m, mvp_sd->bl.x, mvp_sd->bl.y, mvp_sd->status.char_id, (second_sd?second_sd->status.char_id : 0), (third_sd ? third_sd->status.char_id : 0), 1, true);
					}

					//Logs items, MVP prizes [Lupus]
					logs->pick_mob(md, LOG_TYPE_MVP, -1, &item, data);
					break;
				}
			}
		}

		logs->mvpdrop(mvp_sd, md->class_, log_mvp);
	}
	
	if (type&2 && !sd && md->class_ == MOBID_EMPELIUM && md->guardian_data) {
		//Emperium destroyed by script. Discard mvp character. [Skotlex]
		mvp_sd = NULL;
	}

	rebirth =  ( md->sc.data[SC_KAIZEL] || (md->sc.data[SC_REBIRTH] && !md->state.rebirth) );
	if( !rebirth ) { // Only trigger event on final kill
		md->status.hp = 0; //So that npc_event invoked functions KNOW that mob is dead
		if( src ) {
			switch( src->type ) {
				case BL_PET: sd = BL_UCAST(BL_PET, src)->msd; break;
				case BL_HOM: sd = BL_UCAST(BL_HOM, src)->master; break;
				case BL_MER: sd = BL_UCAST(BL_MER, src)->master; break;
				case BL_ELEM: sd = BL_UCAST(BL_ELEM, src)->master; break;

				case BL_NUL:
				case BL_ITEM:
				case BL_SKILL:
				case BL_NPC:
				case BL_CHAT:
				case BL_PC:
				case BL_MOB:
				case BL_ALL:
					break;
			}
		}

		if( sd ) {
			if( sd->mission_mobid == md->class_) { //TK_MISSION [Skotlex]
				if (++sd->mission_count >= 100 && (temp = mob->get_random_id(0, 0xE, sd->status.base_level)) != 0) {
					pc->addfame(sd, RANKTYPE_TAEKWON, 1);
					sd->mission_mobid = temp;
					pc_setglobalreg(sd,script->add_variable("TK_MISSION_ID"), temp);
					sd->mission_count = 0;
					clif->mission_info(sd, temp, 0);
				}
				pc_setglobalreg(sd,script->add_variable("TK_MISSION_COUNT"), sd->mission_count);
			}

			if( sd->status.party_id )
				map->foreachinrange(quest->update_objective_sub, &md->bl, AREA_SIZE, BL_PC, sd->status.party_id, md);
			else if( sd->avail_quests )
				quest->update_objective(sd, md);

			if( sd->md && src && src->type != BL_HOM && mob->db(md->class_)->lv > sd->status.base_level/2 )
				mercenary->kills(sd->md);
		}

		if( md->npc_event[0] && !md->state.npc_killmonster ) {
			if( sd && battle->bc->mob_npc_event_type ) {
				pc->setparam(sd, SP_KILLERRID, sd->bl.id);
				npc->event(sd,md->npc_event,0);
			} else if( mvp_sd ) {
				pc->setparam(mvp_sd, SP_KILLERRID, sd?sd->bl.id:0);
				npc->event(mvp_sd,md->npc_event,0);
			} else
				npc->event_do(md->npc_event);
		} else if( mvp_sd && !md->state.npc_killmonster ) {
			pc->setparam(mvp_sd, SP_KILLEDRID, md->class_);
			npc->script_event(mvp_sd, NPCE_KILLNPC); // PCKillNPC [Lance]
		}

		md->status.hp = 1;
	}

	if(md->deletetimer != INVALID_TIMER) {
		timer->delete(md->deletetimer,mob->timer_delete);
		md->deletetimer = INVALID_TIMER;
	}
	/**
	 * Only loops if necessary (e.g. a poring would never need to loop)
	 **/
	
	if( md->can_summon )
		mob->deleteslave(md);

	map->freeblock_unlock();

	if( !rebirth ) {

		if (pc->db_checkid(md->vd->class)) {
			// Player mobs are not removed automatically by the client.
			/* first we set them dead, then we delay the out sight effect */
			clif->clearunit_area(&md->bl,CLR_DEAD);
			clif->clearunit_delayed(&md->bl, CLR_OUTSIGHT,tick+3000);
		} else
			/**
			 * We give the client some time to breath and this allows it to display anything it'd like with the dead corpose
			 * For example, this delay allows it to display soul drain effect
			 **/
			clif->clearunit_delayed(&md->bl, CLR_DEAD, tick+250);

	}

	if(!md->spawn) //Tell status->damage to remove it from memory.
		return 5; // Note: Actually, it's 4. Oh well...

	// MvP tomb [GreenBox]
	if (battle->bc->mvp_tomb_enabled && md->spawn->state.boss == BTYPE_MVP && map->list[md->bl.m].flag.notomb != 1)
		mob->mvptomb_create(md, mvp_sd ? mvp_sd->status.name : NULL, time(NULL));

	if( !rebirth ) {
		status->change_clear(&md->bl,1);
		mob->setdelayspawn(md); //Set respawning.
	}
	
	
	
	return 3; //Remove from map.
}


HPExport void plugin_init(void) {
	mob->dead = mob_dead_mine;
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Ghost/Seabois. Version '%s'\n", pinfo.name, pinfo.version);
}