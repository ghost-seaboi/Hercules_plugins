//===== Hercules Plugin ======================================
//= @homgrowth
//===== By: ==================================================
//= Ghost / Seabois
//===== Current Version: =====================================
//= 1.0
//===== Description: =========================================
//= Shows estimated Growth-tier of the homunculus.
//===== Changelog: ===========================================
//= v1.0 - Initial Conversion
//===== Additional Comments: =================================
//= 
//===== Repo Link: ===========================================
//= 
//============================================================

#include "common/hercules.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/HPMi.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "common/mapindex.h"
#include "map/clif.h"
#include "map/script.h"
#include "map/skill.h"
#include "map/pc.h"
#include "map/map.h"
#include "map/mob.h"
#include "map/battle.h"
#include "map/homunculus.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

HPExport struct hplugin_info pinfo = {
	"@homgrowth Atcommand",		// Plugin name
	SERVER_TYPE_MAP,// Which server types this plugin works with?
	"1.0",			// Plugin version
	HPM_VERSION,	// HPM Version (don't change, macro is automatically updated)
};

static char atcmd_output[CHAT_SIZE_MAX];

// For Rank naming
typedef enum {
	RANK_ABYSMAL,
	RANK_TERRIBLE,
	RANK_POOR,
	RANK_BELOW_AVERAGE,
	RANK_AVERAGE,
	RANK_GOOD,
	RANK_VERY_GOOD,
	RANK_EXCELLENT,
	RANK_AMAZING,
	RANK_COUNT
} StatRank;

// Identification on which stat to check
typedef enum {
	STAT_HP,
	STAT_SP,
	STAT_STR,
	STAT_AGI,
	STAT_VIT,
	STAT_INT,
	STAT_DEX,
	STAT_LUK
} StatType;

typedef struct {
    float hp_min, hp_max;
    float sp_min, sp_max;
    float str_min, str_max;
    float agi_min, agi_max;
    float vit_min, vit_max;
    float int_min, int_max;
    float dex_min, dex_max;
    float luk_min, luk_max;
} StatRange;

// Translation of Rank into String/Char
const char* get_rank_name(StatRank rank) {
	switch (rank) {
		case RANK_ABYSMAL:        return "Abysmal";
		case RANK_TERRIBLE:       return "Terrible";
		case RANK_POOR:           return "Poor";
		case RANK_BELOW_AVERAGE:  return "Below Average";
		case RANK_AVERAGE:        return "Average";
		case RANK_GOOD:           return "Good";
		case RANK_VERY_GOOD:      return "Very good";
		case RANK_EXCELLENT:      return "Excellent";
		case RANK_AMAZING:        return "Amazing";
		default:                  return "Unknown";
	}
}


typedef struct HomunStatRankNames {
	const char* hp;
	const char* sp;
	const char* str;
	const char* agi;
	const char* vit;
	const char* int_;
	const char* dex;
	const char* luk;
} HomunStatRankNames;


typedef struct HomunStatValues {
	float hp;
	float sp;
	float str;
	float agi;
	float vit;
	float int_;
	float dex;
	float luk;
} HomunStatValues;

// Function to get stat rank
StatRank get_stat_rank(float value, StatType stat, const StatRange *ranges) {
	for (int i = 0; i < RANK_COUNT; i++) {
		float min = 0.0f, max = 0.0f;

		switch (stat) {
			case STAT_HP:  min = ranges[i].hp_min;  max = ranges[i].hp_max;  break;
			case STAT_SP:  min = ranges[i].sp_min;  max = ranges[i].sp_max;  break;
			case STAT_STR: min = ranges[i].str_min; max = ranges[i].str_max; break;
			case STAT_AGI: min = ranges[i].agi_min; max = ranges[i].agi_max; break;
			case STAT_VIT: min = ranges[i].vit_min; max = ranges[i].vit_max; break;
			case STAT_INT: min = ranges[i].int_min; max = ranges[i].int_max; break;
			case STAT_DEX: min = ranges[i].dex_min; max = ranges[i].dex_max; break;
			case STAT_LUK: min = ranges[i].luk_min; max = ranges[i].luk_max; break;
		}

		if (i == RANK_ABYSMAL && value < max)
			return RANK_ABYSMAL;
		else if (i == RANK_AMAZING && value > min)
			return RANK_AMAZING;
		else if (value >= min && value <= max)
			return (StatRank)i;
	}

	return RANK_ABYSMAL; // fallback if no range matched
}

void display_all_stat_ranks(
	float hp, float sp, float str, float agi,
	float vit, float int_, float dex, float luk,
	const StatRange *ranges,
	HomunStatRankNames *out_names // Can be NULL if you only want to display
) {
	StatRank rank;
	const char* name;

	rank = get_stat_rank(hp, STAT_HP, ranges);
	name = get_rank_name(rank);
	// ShowDebug("HP Rank: %s\n", name);
	if (out_names) out_names->hp = name;

	rank = get_stat_rank(sp, STAT_SP, ranges);
	name = get_rank_name(rank);
	// ShowDebug("SP Rank: %s\n", name);
	if (out_names) out_names->sp = name;

	rank = get_stat_rank(str, STAT_STR, ranges);
	name = get_rank_name(rank);
	// ShowDebug("STR Rank: %s\n", name);
	if (out_names) out_names->str = name;

	rank = get_stat_rank(agi, STAT_AGI, ranges);
	name = get_rank_name(rank);
	// ShowDebug("AGI Rank: %s\n", name);
	if (out_names) out_names->agi = name;

	rank = get_stat_rank(vit, STAT_VIT, ranges);
	name = get_rank_name(rank);
	// ShowDebug("VIT Rank: %s\n", name);
	if (out_names) out_names->vit = name;

	rank = get_stat_rank(int_, STAT_INT, ranges);
	name = get_rank_name(rank);
	// ShowDebug("INT Rank: %s\n", name);
	if (out_names) out_names->int_ = name;

	rank = get_stat_rank(dex, STAT_DEX, ranges);
	name = get_rank_name(rank);
	// ShowDebug("DEX Rank: %s\n", name);
	if (out_names) out_names->dex = name;

	rank = get_stat_rank(luk, STAT_LUK, ranges);
	name = get_rank_name(rank);
	// ShowDebug("LUK Rank: %s\n", name);
	if (out_names) out_names->luk = name;
}



// ========== VANILMIRTH GROWTH SHEET ====================


// Vanilmirth 1-10
static const StatRange VanilStatRange10[RANK_COUNT] = {
    // Abysmal
    { .hp_max=70.000f, .sp_max=2.333f, .str_max=0.667f, .agi_max=0.667f,
      .vit_max=0.667f, .int_max=0.667f, .dex_max=0.667f, .luk_max=0.667f },

    // Terrible
    { .hp_min=70.000f, .hp_max=74.444f, .sp_min=2.333f, .sp_max=2.778f,
      .str_min=0.667f, .str_max=0.778f, .agi_min=0.667f, .agi_max=0.778f,
      .vit_min=0.667f, .vit_max=0.778f, .int_min=0.667f, .int_max=0.778f,
      .dex_min=0.667f, .dex_max=0.778f, .luk_min=0.667f, .luk_max=0.778f },

    // Poor
    { .hp_min=74.444f, .hp_max=80.000f, .sp_min=2.778f, .sp_max=3.111f,
      .str_min=0.778f, .str_max=0.889f, .agi_min=0.778f, .agi_max=0.889f,
      .vit_min=0.778f, .vit_max=0.889f, .int_min=0.778f, .int_max=0.889f,
      .dex_min=0.778f, .dex_max=0.889f, .luk_min=0.778f, .luk_max=0.889f },

    // Below Average
    { .hp_min=80.000f, .hp_max=86.667f, .sp_min=3.111f, .sp_max=3.333f,
      .str_min=0.889f, .str_max=1.000f, .agi_min=0.889f, .agi_max=1.000f,
      .vit_min=0.889f, .vit_max=1.000f, .int_min=0.889f, .int_max=1.000f,
      .dex_min=0.889f, .dex_max=1.000f, .luk_min=0.889f, .luk_max=1.000f },

    // Average
    { .hp_min=86.667f, .hp_max=92.222f, .sp_min=3.333f, .sp_max=3.667f,
      .str_min=1.000f, .str_max=1.222f, .agi_min=1.000f, .agi_max=1.222f,
      .vit_min=1.000f, .vit_max=1.222f, .int_min=1.000f, .int_max=1.222f,
      .dex_min=1.000f, .dex_max=1.222f, .luk_min=1.000f, .luk_max=1.222f },

    // Good
    { .hp_min=92.222f, .hp_max=98.889f, .sp_min=3.667f, .sp_max=3.889f,
      .str_min=1.222f, .str_max=1.333f, .agi_min=1.222f, .agi_max=1.333f,
      .vit_min=1.222f, .vit_max=1.333f, .int_min=1.222f, .int_max=1.333f,
      .dex_min=1.222f, .dex_max=1.333f, .luk_min=1.222f, .luk_max=1.333f },

    // Very Good
    { .hp_min=98.889f, .hp_max=104.444f, .sp_min=3.889f, .sp_max=4.222f,
      .str_min=1.333f, .str_max=1.444f, .agi_min=1.333f, .agi_max=1.444f,
      .vit_min=1.333f, .vit_max=1.444f, .int_min=1.333f, .int_max=1.444f,
      .dex_min=1.333f, .dex_max=1.444f, .luk_min=1.333f, .luk_max=1.444f },

    // Excellent
    { .hp_min=104.444f, .hp_max=108.889f, .sp_min=4.222f, .sp_max=4.667f,
      .str_min=1.444f, .str_max=1.556f, .agi_min=1.444f, .agi_max=1.556f,
      .vit_min=1.444f, .vit_max=1.556f, .int_min=1.444f, .int_max=1.556f,
      .dex_min=1.444f, .dex_max=1.556f, .luk_min=1.444f, .luk_max=1.556f },

    // Amazing
    { .hp_min=108.889f, .sp_min=4.667f,
      .str_min=1.556f, .agi_min=1.556f, .vit_min=1.556f,
      .int_min=1.556f, .dex_min=1.556f, .luk_min=1.556f },
};
// Vanilmirth 11-20
static const StatRange VanilStatRange20[RANK_COUNT] = {
    // Abysmal
    { .hp_max=76.316f, .sp_max=2.842f, .str_max=0.789f, .agi_max=0.789f,
      .vit_max=0.789f, .int_max=0.789f, .dex_max=0.789f, .luk_max=0.789f },

    // Terrible
    { .hp_min=76.316f, .hp_max=79.474f, .sp_min=2.842f, .sp_max=3.105f,
      .str_min=0.789f, .str_max=0.842f, .agi_min=0.789f, .agi_max=0.842f,
      .vit_min=0.789f, .vit_max=0.842f, .int_min=0.789f, .int_max=0.842f,
      .dex_min=0.789f, .dex_max=0.842f, .luk_min=0.789f, .luk_max=0.842f },

    // Poor
    { .hp_min=79.474f, .hp_max=83.158f, .sp_min=3.105f, .sp_max=3.263f,
      .str_min=0.842f, .str_max=0.947f, .agi_min=0.842f, .agi_max=0.947f,
      .vit_min=0.842f, .vit_max=0.947f, .int_min=0.842f, .int_max=0.947f,
      .dex_min=0.842f, .dex_max=0.947f, .luk_min=0.842f, .luk_max=0.947f },

    // Below Average
    { .hp_min=83.158f, .hp_max=87.895f, .sp_min=3.263f, .sp_max=3.421f,
      .str_min=0.947f, .str_max=1.053f, .agi_min=0.947f, .agi_max=1.053f,
      .vit_min=0.947f, .vit_max=1.053f, .int_min=0.947f, .int_max=1.053f,
      .dex_min=0.947f, .dex_max=1.053f, .luk_min=0.947f, .luk_max=1.053f },

    // Average
    { .hp_min=87.895f, .hp_max=91.579f, .sp_min=3.421f, .sp_max=3.579f,
      .str_min=1.053f, .str_max=1.158f, .agi_min=1.053f, .agi_max=1.158f,
      .vit_min=1.053f, .vit_max=1.158f, .int_min=1.053f, .int_max=1.158f,
      .dex_min=1.053f, .dex_max=1.158f, .luk_min=1.053f, .luk_max=1.158f },

    // Good
    { .hp_min=91.579f, .hp_max=96.316f, .sp_min=3.579f, .sp_max=3.737f,
      .str_min=1.158f, .str_max=1.263f, .agi_min=1.158f, .agi_max=1.263f,
      .vit_min=1.158f, .vit_max=1.263f, .int_min=1.158f, .int_max=1.263f,
      .dex_min=1.158f, .dex_max=1.263f, .luk_min=1.158f, .luk_max=1.263f },

    // Very Good
    { .hp_min=96.316f, .hp_max=100.000f, .sp_min=3.737f, .sp_max=3.895f,
      .str_min=1.263f, .str_max=1.368f, .agi_min=1.263f, .agi_max=1.368f,
      .vit_min=1.263f, .vit_max=1.368f, .int_min=1.263f, .int_max=1.368f,
      .dex_min=1.263f, .dex_max=1.368f, .luk_min=1.263f, .luk_max=1.368f },

    // Excellent
    { .hp_min=100.000f, .hp_max=102.632f, .sp_min=3.895f, .sp_max=4.158f,
      .str_min=1.368f, .str_max=1.421f, .agi_min=1.368f, .agi_max=1.421f,
      .vit_min=1.368f, .vit_max=1.421f, .int_min=1.368f, .int_max=1.421f,
      .dex_min=1.368f, .dex_max=1.421f, .luk_min=1.368f, .luk_max=1.421f },

    // Amazing
    { .hp_min=102.632f, .sp_min=4.158f,
      .str_min=1.421f, .agi_min=1.421f, .vit_min=1.421f,
      .int_min=1.421f, .dex_min=1.421f, .luk_min=1.421f },
};
// Vanilmirth 21-30
static const StatRange VanilStatRange30[RANK_COUNT] = {
    // Abysmal
    { .hp_max=78.966f, .sp_max=3.034f, .str_max=0.828f, .agi_max=0.828f,
      .vit_max=0.828f, .int_max=0.828f, .dex_max=0.828f, .luk_max=0.828f },

    // Terrible
    { .hp_min=78.966f, .hp_max=81.379f, .sp_min=3.034f, .sp_max=3.207f,
      .str_min=0.828f, .str_max=0.897f, .agi_min=0.828f, .agi_max=0.897f,
      .vit_min=0.828f, .vit_max=0.897f, .int_min=0.828f, .int_max=0.897f,
      .dex_min=0.828f, .dex_max=0.897f, .luk_min=0.828f, .luk_max=0.897f },

    // Poor
    { .hp_min=81.379f, .hp_max=84.483f, .sp_min=3.207f, .sp_max=3.310f,
      .str_min=0.897f, .str_max=0.966f, .agi_min=0.897f, .agi_max=0.966f,
      .vit_min=0.897f, .vit_max=0.966f, .int_min=0.897f, .int_max=0.966f,
      .dex_min=0.897f, .dex_max=0.966f, .luk_min=0.897f, .luk_max=0.966f },

    // Below Average
    { .hp_min=84.483f, .hp_max=88.276f, .sp_min=3.310f, .sp_max=3.448f,
      .str_min=0.966f, .str_max=1.069f, .agi_min=0.966f, .agi_max=1.069f,
      .vit_min=0.966f, .vit_max=1.069f, .int_min=0.966f, .int_max=1.069f,
      .dex_min=0.966f, .dex_max=1.069f, .luk_min=0.966f, .luk_max=1.069f },

    // Average
    { .hp_min=88.276f, .hp_max=91.379f, .sp_min=3.448f, .sp_max=3.552f,
      .str_min=1.069f, .str_max=1.138f, .agi_min=1.069f, .agi_max=1.138f,
      .vit_min=1.069f, .vit_max=1.138f, .int_min=1.069f, .int_max=1.138f,
      .dex_min=1.069f, .dex_max=1.138f, .luk_min=1.069f, .luk_max=1.138f },

    // Good
    { .hp_min=91.379f, .hp_max=95.172f, .sp_min=3.552f, .sp_max=3.690f,
      .str_min=1.138f, .str_max=1.241f, .agi_min=1.138f, .agi_max=1.241f,
      .vit_min=1.138f, .vit_max=1.241f, .int_min=1.138f, .int_max=1.241f,
      .dex_min=1.138f, .dex_max=1.241f, .luk_min=1.138f, .luk_max=1.241f },

    // Very Good
    { .hp_min=95.172f, .hp_max=97.931f, .sp_min=3.690f, .sp_max=3.793f,
      .str_min=1.241f, .str_max=1.310f, .agi_min=1.241f, .agi_max=1.310f,
      .vit_min=1.241f, .vit_max=1.310f, .int_min=1.241f, .int_max=1.310f,
      .dex_min=1.241f, .dex_max=1.310f, .luk_min=1.241f, .luk_max=1.310f },

    // Excellent
    { .hp_min=97.931f, .hp_max=100.345f, .sp_min=3.793f, .sp_max=3.966f,
      .str_min=1.310f, .str_max=1.379f, .agi_min=1.310f, .agi_max=1.379f,
      .vit_min=1.310f, .vit_max=1.379f, .int_min=1.310f, .int_max=1.379f,
      .dex_min=1.310f, .dex_max=1.379f, .luk_min=1.310f, .luk_max=1.379f },

    // Amazing
    { .hp_min=100.345f, .sp_min=3.966f,
      .str_min=1.379f, .agi_min=1.379f, .vit_min=1.379f,
      .int_min=1.379f, .dex_min=1.379f, .luk_min=1.379f },
};

static const StatRange VanilStatRange40[RANK_COUNT] = {
    // Abysmal
    { .hp_max=80.513f, .sp_max=3.128f, .str_max=0.872f, .agi_max=0.872f,
      .vit_max=0.872f, .int_max=0.872f, .dex_max=0.872f, .luk_max=0.872f },

    // Terrible
    { .hp_min=80.513f, .hp_max=82.564f, .sp_min=3.128f, .sp_max=3.282f,
      .str_min=0.872f, .str_max=0.923f, .agi_min=0.872f, .agi_max=0.923f,
      .vit_min=0.872f, .vit_max=0.923f, .int_min=0.872f, .int_max=0.923f,
      .dex_min=0.872f, .dex_max=0.923f, .luk_min=0.872f, .luk_max=0.923f },

    // Poor
    { .hp_min=82.564f, .hp_max=85.128f, .sp_min=3.282f, .sp_max=3.359f,
      .str_min=0.923f, .str_max=0.974f, .agi_min=0.923f, .agi_max=0.974f,
      .vit_min=0.923f, .vit_max=0.974f, .int_min=0.923f, .int_max=0.974f,
      .dex_min=0.923f, .dex_max=0.974f, .luk_min=0.923f, .luk_max=0.974f },

    // Below Average
    { .hp_min=85.128f, .hp_max=88.462f, .sp_min=3.359f, .sp_max=3.462f,
      .str_min=0.974f, .str_max=1.051f, .agi_min=0.974f, .agi_max=1.051f,
      .vit_min=0.974f, .vit_max=1.051f, .int_min=0.974f, .int_max=1.051f,
      .dex_min=0.974f, .dex_max=1.051f, .luk_min=0.974f, .luk_max=1.051f },

    // Average
    { .hp_min=88.462f, .hp_max=91.282f, .sp_min=3.462f, .sp_max=3.538f,
      .str_min=1.051f, .str_max=1.128f, .agi_min=1.051f, .agi_max=1.128f,
      .vit_min=1.051f, .vit_max=1.128f, .int_min=1.051f, .int_max=1.128f,
      .dex_min=1.051f, .dex_max=1.128f, .luk_min=1.051f, .luk_max=1.128f },

    // Good
    { .hp_min=91.282f, .hp_max=94.615f, .sp_min=3.538f, .sp_max=3.641f,
      .str_min=1.128f, .str_max=1.205f, .agi_min=1.128f, .agi_max=1.231f,
      .vit_min=1.128f, .vit_max=1.231f, .int_min=1.128f, .int_max=1.231f,
      .dex_min=1.128f, .dex_max=1.231f, .luk_min=1.128f, .luk_max=1.231f },

    // Very Good
    { .hp_min=94.615f, .hp_max=96.923f, .sp_min=3.641f, .sp_max=3.718f,
      .str_min=1.205f, .str_max=1.282f, .agi_min=1.231f, .agi_max=1.282f,
      .vit_min=1.231f, .vit_max=1.282f, .int_min=1.231f, .int_max=1.282f,
      .dex_min=1.231f, .dex_max=1.282f, .luk_min=1.231f, .luk_max=1.282f },

    // Excellent
    { .hp_min=96.923f, .hp_max=98.974f, .sp_min=3.718f, .sp_max=3.872f,
      .str_min=1.282f, .str_max=1.333f, .agi_min=1.282f, .agi_max=1.333f,
      .vit_min=1.282f, .vit_max=1.333f, .int_min=1.282f, .int_max=1.333f,
      .dex_min=1.282f, .dex_max=1.333f, .luk_min=1.282f, .luk_max=1.333f },

    // Amazing
    { .hp_min=98.974f, .sp_min=3.872f,
      .str_min=1.333f, .agi_min=1.333f, .vit_min=1.333f,
      .int_min=1.333f, .dex_min=1.333f, .luk_min=1.333f },
};

static const StatRange VanilStatRange50[RANK_COUNT] = {
    // Abysmal
    { .hp_max=81.633f, .sp_max=3.163f, .str_max=0.898f, .agi_max=0.898f,
      .vit_max=0.898f, .int_max=0.898f, .dex_max=0.898f, .luk_max=0.898f },

    // Terrible
    { .hp_min=81.633f, .hp_max=83.469f, .sp_min=3.163f, .sp_max=3.306f,
      .str_min=0.898f, .str_max=0.939f, .agi_min=0.898f, .agi_max=0.939f,
      .vit_min=0.898f, .vit_max=0.939f, .int_min=0.898f, .int_max=0.939f,
      .dex_min=0.898f, .dex_max=0.939f, .luk_min=0.898f, .luk_max=0.939f },

    // Poor
    { .hp_min=83.469f, .hp_max=85.714f, .sp_min=3.306f, .sp_max=3.388f,
      .str_min=0.939f, .str_max=1.000f, .agi_min=0.939f, .agi_max=1.000f,
      .vit_min=0.939f, .vit_max=1.000f, .int_min=0.939f, .int_max=1.000f,
      .dex_min=0.939f, .dex_max=1.000f, .luk_min=0.939f, .luk_max=1.000f },

    // Below Average
    { .hp_min=85.714f, .hp_max=88.776f, .sp_min=3.388f, .sp_max=3.469f,
      .str_min=1.000f, .str_max=1.061f, .agi_min=1.000f, .agi_max=1.061f,
      .vit_min=1.000f, .vit_max=1.061f, .int_min=1.000f, .int_max=1.061f,
      .dex_min=1.000f, .dex_max=1.061f, .luk_min=1.000f, .luk_max=1.061f },

    // Average
    { .hp_min=88.776f, .hp_max=91.224f, .sp_min=3.469f, .sp_max=3.531f,
      .str_min=1.061f, .str_max=1.122f, .agi_min=1.061f, .agi_max=1.122f,
      .vit_min=1.061f, .vit_max=1.122f, .int_min=1.061f, .int_max=1.122f,
      .dex_min=1.061f, .dex_max=1.122f, .luk_min=1.061f, .luk_max=1.122f },

    // Good
    { .hp_min=91.224f, .hp_max=94.082f, .sp_min=3.531f, .sp_max=3.612f,
      .str_min=1.122f, .str_max=1.204f, .agi_min=1.122f, .agi_max=1.204f,
      .vit_min=1.122f, .vit_max=1.204f, .int_min=1.122f, .int_max=1.204f,
      .dex_min=1.122f, .dex_max=1.204f, .luk_min=1.122f, .luk_max=1.204f },

    // Very Good
    { .hp_min=94.082f, .hp_max=96.122f, .sp_min=3.612f, .sp_max=3.694f,
      .str_min=1.204f, .str_max=1.265f, .agi_min=1.204f, .agi_max=1.265f,
      .vit_min=1.204f, .vit_max=1.265f, .int_min=1.204f, .int_max=1.265f,
      .dex_min=1.204f, .dex_max=1.265f, .luk_min=1.204f, .luk_max=1.265f },

    // Excellent
    { .hp_min=96.122f, .hp_max=97.959f, .sp_min=3.694f, .sp_max=3.837f,
      .str_min=1.265f, .str_max=1.306f, .agi_min=1.265f, .agi_max=1.306f,
      .vit_min=1.265f, .vit_max=1.306f, .int_min=1.265f, .int_max=1.306f,
      .dex_min=1.265f, .dex_max=1.306f, .luk_min=1.265f, .luk_max=1.306f },

    // Amazing
    { .hp_min=97.959f, .sp_min=3.837f,
      .str_min=1.306f, .agi_min=1.306f, .vit_min=1.306f,
      .int_min=1.306f, .dex_min=1.306f, .luk_min=1.306f },
};

static const StatRange VanilStatRange60[RANK_COUNT] = {
    // Abysmal
    { .hp_max=82.373f, .sp_max=3.203f, .str_max=0.915f, .agi_max=0.915f,
      .vit_max=0.915f, .int_max=0.915f, .dex_max=0.915f, .luk_max=0.915f },

    // Terrible
    { .hp_min=82.373f, .hp_max=84.068f, .sp_min=3.203f, .sp_max=3.322f,
      .str_min=0.915f, .str_max=0.949f, .agi_min=0.915f, .agi_max=0.949f,
      .vit_min=0.915f, .vit_max=0.949f, .int_min=0.915f, .int_max=0.949f,
      .dex_min=0.915f, .dex_max=0.949f, .luk_min=0.915f, .luk_max=0.949f },

    // Poor
    { .hp_min=84.068f, .hp_max=86.102f, .sp_min=3.322f, .sp_max=3.407f,
      .str_min=0.949f, .str_max=1.000f, .agi_min=0.949f, .agi_max=1.000f,
      .vit_min=0.949f, .vit_max=1.000f, .int_min=0.949f, .int_max=1.000f,
      .dex_min=0.949f, .dex_max=1.000f, .luk_min=0.949f, .luk_max=1.000f },

    // Below Average
    { .hp_min=86.102f, .hp_max=88.814f, .sp_min=3.407f, .sp_max=3.475f,
      .str_min=1.000f, .str_max=1.068f, .agi_min=1.000f, .agi_max=1.068f,
      .vit_min=1.000f, .vit_max=1.068f, .int_min=1.000f, .int_max=1.068f,
      .dex_min=1.000f, .dex_max=1.068f, .luk_min=1.000f, .luk_max=1.068f },

    // Average
    { .hp_min=88.814f, .hp_max=91.186f, .sp_min=3.475f, .sp_max=3.525f,
      .str_min=1.068f, .str_max=1.119f, .agi_min=1.068f, .agi_max=1.136f,
      .vit_min=1.068f, .vit_max=1.136f, .int_min=1.068f, .int_max=1.136f,
      .dex_min=1.068f, .dex_max=1.136f, .luk_min=1.068f, .luk_max=1.136f },

    // Good
    { .hp_min=91.186f, .hp_max=93.729f, .sp_min=3.525f, .sp_max=3.593f,
      .str_min=1.119f, .str_max=1.186f, .agi_min=1.136f, .agi_max=1.203f,
      .vit_min=1.136f, .vit_max=1.203f, .int_min=1.136f, .int_max=1.203f,
      .dex_min=1.136f, .dex_max=1.203f, .luk_min=1.136f, .luk_max=1.203f },

    // Very Good
    { .hp_min=93.729f, .hp_max=95.593f, .sp_min=3.593f, .sp_max=3.678f,
      .str_min=1.186f, .str_max=1.237f, .agi_min=1.203f, .agi_max=1.237f,
      .vit_min=1.203f, .vit_max=1.237f, .int_min=1.203f, .int_max=1.237f,
      .dex_min=1.203f, .dex_max=1.237f, .luk_min=1.203f, .luk_max=1.237f },

    // Excellent
    { .hp_min=95.593f, .hp_max=97.288f, .sp_min=3.678f, .sp_max=3.797f,
      .str_min=1.237f, .str_max=1.288f, .agi_min=1.237f, .agi_max=1.288f,
      .vit_min=1.237f, .vit_max=1.288f, .int_min=1.237f, .int_max=1.288f,
      .dex_min=1.237f, .dex_max=1.288f, .luk_min=1.237f, .luk_max=1.288f },

    // Amazing
    { .hp_min=97.288f, .sp_min=3.797f,
      .str_min=1.288f, .agi_min=1.288f, .vit_min=1.288f,
      .int_min=1.288f, .dex_min=1.288f, .luk_min=1.288f },
};

static const StatRange VanilStatRange70[RANK_COUNT] = {
    // Abysmal
    { .hp_max=82.899f, .sp_max=3.261f, .str_max=0.928f, .agi_max=0.928f,
      .vit_max=0.928f, .int_max=0.928f, .dex_max=0.928f, .luk_max=0.928f },

    // Terrible
    { .hp_min=82.899f, .hp_max=84.493f, .sp_min=3.261f, .sp_max=3.362f,
      .str_min=0.928f, .str_max=0.971f, .agi_min=0.928f, .agi_max=0.971f,
      .vit_min=0.928f, .vit_max=0.971f, .int_min=0.928f, .int_max=0.971f,
      .dex_min=0.928f, .dex_max=0.971f, .luk_min=0.928f, .luk_max=0.971f },

    // Poor
    { .hp_min=84.493f, .hp_max=86.377f, .sp_min=3.362f, .sp_max=3.420f,
      .str_min=0.971f, .str_max=1.014f, .agi_min=0.971f, .agi_max=1.014f,
      .vit_min=0.971f, .vit_max=1.014f, .int_min=0.971f, .int_max=1.014f,
      .dex_min=0.971f, .dex_max=1.014f, .luk_min=0.971f, .luk_max=1.014f },

    // Below Average
    { .hp_min=86.377f, .hp_max=88.841f, .sp_min=3.420f, .sp_max=3.478f,
      .str_min=1.014f, .str_max=1.072f, .agi_min=1.014f, .agi_max=1.072f,
      .vit_min=1.014f, .vit_max=1.072f, .int_min=1.014f, .int_max=1.072f,
      .dex_min=1.014f, .dex_max=1.072f, .luk_min=1.014f, .luk_max=1.072f },

    // Average
    { .hp_min=88.841f, .hp_max=91.014f, .sp_min=3.478f, .sp_max=3.522f,
      .str_min=1.072f, .str_max=1.130f, .agi_min=1.072f, .agi_max=1.130f,
      .vit_min=1.072f, .vit_max=1.130f, .int_min=1.072f, .int_max=1.130f,
      .dex_min=1.072f, .dex_max=1.130f, .luk_min=1.072f, .luk_max=1.130f },

    // Good
    { .hp_min=91.014f, .hp_max=93.478f, .sp_min=3.522f, .sp_max=3.580f,
      .str_min=1.130f, .str_max=1.188f, .agi_min=1.130f, .agi_max=1.188f,
      .vit_min=1.130f, .vit_max=1.188f, .int_min=1.130f, .int_max=1.188f,
      .dex_min=1.130f, .dex_max=1.188f, .luk_min=1.130f, .luk_max=1.188f },

    // Very Good
    { .hp_min=93.478f, .hp_max=95.217f, .sp_min=3.580f, .sp_max=3.638f,
      .str_min=1.188f, .str_max=1.232f, .agi_min=1.188f, .agi_max=1.232f,
      .vit_min=1.188f, .vit_max=1.232f, .int_min=1.188f, .int_max=1.232f,
      .dex_min=1.188f, .dex_max=1.232f, .luk_min=1.188f, .luk_max=1.232f },

    // Excellent
    { .hp_min=95.217f, .hp_max=96.667f, .sp_min=3.638f, .sp_max=3.739f,
      .str_min=1.232f, .str_max=1.275f, .agi_min=1.232f, .agi_max=1.275f,
      .vit_min=1.232f, .vit_max=1.275f, .int_min=1.232f, .int_max=1.275f,
      .dex_min=1.232f, .dex_max=1.275f, .luk_min=1.232f, .luk_max=1.275f },

    // Amazing
    { .hp_min=96.667f, .sp_min=3.739f,
      .str_min=1.275f, .agi_min=1.275f, .vit_min=1.275f,
      .int_min=1.275f, .dex_min=1.275f, .luk_min=1.275f },
};

static const StatRange VanilStatRange80[RANK_COUNT] = {
    // Abysmal
    { .hp_max=83.291f, .sp_max=3.316f, .str_max=0.937f, .agi_max=0.937f,
      .vit_max=0.937f, .int_max=0.937f, .dex_max=0.937f, .luk_max=0.937f },

    // Terrible
    { .hp_min=83.291f, .hp_max=84.810f, .sp_min=3.316f, .sp_max=3.380f,
      .str_min=0.937f, .str_max=0.975f, .agi_min=0.937f, .agi_max=0.975f,
      .vit_min=0.937f, .vit_max=0.975f, .int_min=0.937f, .int_max=0.975f,
      .dex_min=0.937f, .dex_max=0.975f, .luk_min=0.937f, .luk_max=0.975f },

    // Poor
    { .hp_min=84.810f, .hp_max=86.582f, .sp_min=3.380f, .sp_max=3.430f,
      .str_min=0.975f, .str_max=1.013f, .agi_min=0.975f, .agi_max=1.013f,
      .vit_min=0.975f, .vit_max=1.013f, .int_min=0.975f, .int_max=1.013f,
      .dex_min=0.975f, .dex_max=1.013f, .luk_min=0.975f, .luk_max=1.013f },

    // Below Average
    { .hp_min=86.582f, .hp_max=88.987f, .sp_min=3.430f, .sp_max=3.481f,
      .str_min=1.013f, .str_max=1.076f, .agi_min=1.013f, .agi_max=1.076f,
      .vit_min=1.013f, .vit_max=1.076f, .int_min=1.013f, .int_max=1.076f,
      .dex_min=1.013f, .dex_max=1.076f, .luk_min=1.013f, .luk_max=1.076f },

    // Average
    { .hp_min=88.987f, .hp_max=91.013f, .sp_min=3.481f, .sp_max=3.519f,
      .str_min=1.076f, .str_max=1.127f, .agi_min=1.076f, .agi_max=1.127f,
      .vit_min=1.076f, .vit_max=1.127f, .int_min=1.076f, .int_max=1.127f,
      .dex_min=1.076f, .dex_max=1.127f, .luk_min=1.076f, .luk_max=1.127f },

    // Good
    { .hp_min=91.013f, .hp_max=93.291f, .sp_min=3.519f, .sp_max=3.570f,
      .str_min=1.127f, .str_max=1.177f, .agi_min=1.127f, .agi_max=1.177f,
      .vit_min=1.127f, .vit_max=1.177f, .int_min=1.127f, .int_max=1.177f,
      .dex_min=1.127f, .dex_max=1.177f, .luk_min=1.127f, .luk_max=1.177f },

    // Very Good
    { .hp_min=93.291f, .hp_max=94.937f, .sp_min=3.570f, .sp_max=3.620f,
      .str_min=1.177f, .str_max=1.228f, .agi_min=1.177f, .agi_max=1.228f,
      .vit_min=1.177f, .vit_max=1.228f, .int_min=1.177f, .int_max=1.228f,
      .dex_min=1.177f, .dex_max=1.228f, .luk_min=1.177f, .luk_max=1.228f },

    // Excellent
    { .hp_min=94.937f, .hp_max=96.203f, .sp_min=3.620f, .sp_max=3.684f,
      .str_min=1.228f, .str_max=1.266f, .agi_min=1.228f, .agi_max=1.253f,
      .vit_min=1.228f, .vit_max=1.266f, .int_min=1.228f, .int_max=1.266f,
      .dex_min=1.228f, .dex_max=1.266f, .luk_min=1.228f, .luk_max=1.266f },

    // Amazing
    { .hp_min=96.203f, .sp_min=3.684f,
      .str_min=1.266f, .agi_min=1.266f, .vit_min=1.266f,
      .int_min=1.266f, .dex_min=1.266f, .luk_min=1.266f },
};

static const StatRange VanilStatRange90[RANK_COUNT] = {
    // Abysmal
    { .hp_max=83.708f, .sp_max=3.303f, .str_max=0.944f, .agi_max=0.944f,
      .vit_max=0.944f, .int_max=0.944f, .dex_max=0.944f, .luk_max=0.944f },

    // Terrible
    { .hp_min=83.708f, .hp_max=85.169f, .sp_min=3.303f, .sp_max=3.382f,
      .str_min=0.944f, .str_max=0.978f, .agi_min=0.944f, .agi_max=0.978f,
      .vit_min=0.944f, .vit_max=0.978f, .int_min=0.944f, .int_max=0.978f,
      .dex_min=0.944f, .dex_max=0.978f, .luk_min=0.944f, .luk_max=0.978f },

    // Poor
    { .hp_min=85.169f, .hp_max=86.854f, .sp_min=3.382f, .sp_max=3.427f,
      .str_min=0.978f, .str_max=1.022f, .agi_min=0.978f, .agi_max=1.022f,
      .vit_min=0.978f, .vit_max=1.022f, .int_min=0.978f, .int_max=1.022f,
      .dex_min=0.978f, .dex_max=1.022f, .luk_min=0.978f, .luk_max=1.022f },

    // Below Average
    { .hp_min=86.854f, .hp_max=88.989f, .sp_min=3.427f, .sp_max=3.483f,
      .str_min=1.022f, .str_max=1.079f, .agi_min=1.022f, .agi_max=1.079f,
      .vit_min=1.022f, .vit_max=1.079f, .int_min=1.022f, .int_max=1.079f,
      .dex_min=1.022f, .dex_max=1.079f, .luk_min=1.022f, .luk_max=1.079f },

    // Average
    { .hp_min=88.989f, .hp_max=90.899f, .sp_min=3.483f, .sp_max=3.517f,
      .str_min=1.079f, .str_max=1.124f, .agi_min=1.079f, .agi_max=1.124f,
      .vit_min=1.079f, .vit_max=1.124f, .int_min=1.079f, .int_max=1.124f,
      .dex_min=1.079f, .dex_max=1.124f, .luk_min=1.079f, .luk_max=1.124f },

    // Good
    { .hp_min=90.899f, .hp_max=93.034f, .sp_min=3.517f, .sp_max=3.573f,
      .str_min=1.124f, .str_max=1.180f, .agi_min=1.124f, .agi_max=1.180f,
      .vit_min=1.124f, .vit_max=1.180f, .int_min=1.124f, .int_max=1.180f,
      .dex_min=1.124f, .dex_max=1.180f, .luk_min=1.124f, .luk_max=1.180f },

    // Very Good
    { .hp_min=93.034f, .hp_max=94.607f, .sp_min=3.573f, .sp_max=3.618f,
      .str_min=1.180f, .str_max=1.213f, .agi_min=1.180f, .agi_max=1.213f,
      .vit_min=1.180f, .vit_max=1.213f, .int_min=1.180f, .int_max=1.213f,
      .dex_min=1.180f, .dex_max=1.213f, .luk_min=1.180f, .luk_max=1.213f },

    // Excellent
    { .hp_min=94.607f, .hp_max=95.843f, .sp_min=3.618f, .sp_max=3.697f,
      .str_min=1.213f, .str_max=1.258f, .agi_min=1.213f, .agi_max=1.247f,
      .vit_min=1.213f, .vit_max=1.247f, .int_min=1.213f, .int_max=1.247f,
      .dex_min=1.213f, .dex_max=1.247f, .luk_min=1.213f, .luk_max=1.247f },

    // Amazing
    { .hp_min=95.843f, .sp_min=3.697f,
      .str_min=1.258f, .agi_min=1.247f, .vit_min=1.247f,
      .int_min=1.247f, .dex_min=1.247f, .luk_min=1.247f },
};

static const StatRange VanilStatRange99[RANK_COUNT] = {
    // Abysmal
    { .hp_max=84.082f, .sp_max=3.296f, .str_max=0.959f, .agi_max=0.959f,
      .vit_max=0.959f, .int_max=0.949f, .dex_max=0.959f, .luk_max=0.959f },

    // Terrible
    { .hp_min=84.082f, .hp_max=85.408f, .sp_min=3.296f, .sp_max=3.388f,
      .str_min=0.959f, .str_max=0.990f, .agi_min=0.959f, .agi_max=0.990f,
      .vit_min=0.959f, .vit_max=0.990f, .int_min=0.949f, .int_max=0.980f,
      .dex_min=0.959f, .dex_max=0.990f, .luk_min=0.959f, .luk_max=0.990f },

    // Poor
    { .hp_min=85.408f, .hp_max=86.939f, .sp_min=3.388f, .sp_max=3.439f,
      .str_min=0.990f, .str_max=1.031f, .agi_min=0.990f, .agi_max=1.031f,
      .vit_min=0.990f, .vit_max=1.020f, .int_min=0.980f, .int_max=1.020f,
      .dex_min=0.990f, .dex_max=1.031f, .luk_min=0.990f, .luk_max=1.031f },

    // Below Average
    { .hp_min=86.939f, .hp_max=89.082f, .sp_min=3.439f, .sp_max=3.480f,
      .str_min=1.031f, .str_max=1.071f, .agi_min=1.031f, .agi_max=1.082f,
      .vit_min=1.020f, .vit_max=1.071f, .int_min=1.020f, .int_max=1.082f,
      .dex_min=1.031f, .dex_max=1.082f, .luk_min=1.031f, .luk_max=1.082f },

    // Average
    { .hp_min=89.082f, .hp_max=90.918f, .sp_min=3.480f, .sp_max=3.520f,
      .str_min=1.071f, .str_max=1.122f, .agi_min=1.082f, .agi_max=1.122f,
      .vit_min=1.071f, .vit_max=1.122f, .int_min=1.082f, .int_max=1.122f,
      .dex_min=1.082f, .dex_max=1.122f, .luk_min=1.082f, .luk_max=1.122f },

    // Good
    { .hp_min=90.918f, .hp_max=92.857f, .sp_min=3.520f, .sp_max=3.561f,
      .str_min=1.122f, .str_max=1.173f, .agi_min=1.122f, .agi_max=1.173f,
      .vit_min=1.122f, .vit_max=1.173f, .int_min=1.122f, .int_max=1.173f,
      .dex_min=1.122f, .dex_max=1.173f, .luk_min=1.122f, .luk_max=1.173f },

    // Very Good
    { .hp_min=92.857f, .hp_max=94.388f, .sp_min=3.561f, .sp_max=3.612f,
      .str_min=1.173f, .str_max=1.214f, .agi_min=1.173f, .agi_max=1.214f,
      .vit_min=1.173f, .vit_max=1.214f, .int_min=1.173f, .int_max=1.214f,
      .dex_min=1.173f, .dex_max=1.214f, .luk_min=1.173f, .luk_max=1.214f },

    // Excellent
    { .hp_min=94.388f, .hp_max=95.612f, .sp_min=3.612f, .sp_max=3.704f,
      .str_min=1.214f, .str_max=1.245f, .agi_min=1.214f, .agi_max=1.245f,
      .vit_min=1.214f, .vit_max=1.245f, .int_min=1.214f, .int_max=1.245f,
      .dex_min=1.214f, .dex_max=1.245f, .luk_min=1.214f, .luk_max=1.245f },

    // Amazing
    { .hp_min=95.612f, .sp_min=3.704f,
      .str_min=1.245f, .agi_min=1.245f, .vit_min=1.245f,
      .int_min=1.245f, .dex_min=1.245f, .luk_min=1.245f },
};

static const StatRange *vanil_stat_ranges[] = {
    VanilStatRange10,
    VanilStatRange20,
    VanilStatRange30,
    VanilStatRange40,
    VanilStatRange50,
    VanilStatRange60,
    VanilStatRange70,
    VanilStatRange80,
    VanilStatRange90,
    VanilStatRange99
};

// ============= End of Vanil ==============

// ============= FILIR GROWTH SHEET =============

static const StatRange FilirStatRange10[RANK_COUNT] = {
    // Abysmal
    { .hp_max=54.444f, .sp_max=4.000f, .str_max=0.444f, .agi_max=0.667f,
      .vit_max=0.000f, .int_max=0.333f, .dex_max=0.444f, .luk_max=0.333f },

    // Terrible
    { .hp_min=54.444f, .hp_max=55.556f, .sp_min=4.000f, .sp_max=4.111f,
      .str_min=0.444f, .str_max=0.444f, .agi_min=0.667f, .agi_max=0.778f,
      .vit_min=0.000f, .vit_max=0.000f, .int_min=0.333f, .int_max=0.333f,
      .dex_min=0.444f, .dex_max=0.444f, .luk_min=0.333f, .luk_max=0.333f },

    // Poor
    { .hp_min=55.556f, .hp_max=56.667f, .sp_min=4.111f, .sp_max=4.333f,
      .str_min=0.444f, .str_max=0.556f, .agi_min=0.778f, .agi_max=0.778f,
      .vit_min=0.000f, .vit_max=0.000f, .int_min=0.333f, .int_max=0.444f,
      .dex_min=0.444f, .dex_max=0.556f, .luk_min=0.333f, .luk_max=0.444f },

    // Below Average
    { .hp_min=56.667f, .hp_max=58.889f, .sp_min=4.333f, .sp_max=4.444f,
      .str_min=0.556f, .str_max=0.667f, .agi_min=0.778f, .agi_max=0.889f,
      .vit_min=0.000f, .vit_max=0.111f, .int_min=0.444f, .int_max=0.556f,
      .dex_min=0.556f, .dex_max=0.667f, .luk_min=0.444f, .luk_max=0.556f },

    // Average
    { .hp_min=58.889f, .hp_max=60.000f, .sp_min=4.444f, .sp_max=4.556f,
      .str_min=0.667f, .str_max=0.778f, .agi_min=0.889f, .agi_max=1.000f,
      .vit_min=0.111f, .vit_max=0.111f, .int_min=0.556f, .int_max=0.667f,
      .dex_min=0.667f, .dex_max=0.778f, .luk_min=0.556f, .luk_max=0.667f },

    // Good
    { .hp_min=60.000f, .hp_max=62.222f, .sp_min=4.556f, .sp_max=4.667f,
      .str_min=0.778f, .str_max=0.889f, .agi_min=1.000f, .agi_max=1.000f,
      .vit_min=0.111f, .vit_max=0.222f, .int_min=0.667f, .int_max=0.778f,
      .dex_min=0.778f, .dex_max=0.889f, .luk_min=0.667f, .luk_max=0.778f },

    // Very Good
    { .hp_min=62.222f, .hp_max=63.333f, .sp_min=4.667f, .sp_max=4.889f,
      .str_min=0.889f, .str_max=1.000f, .agi_min=1.000f, .agi_max=1.111f,
      .vit_min=0.222f, .vit_max=0.222f, .int_min=0.778f, .int_max=0.778f,
      .dex_min=0.889f, .dex_max=1.000f, .luk_min=0.778f, .luk_max=0.778f },

    // Excellent
    { .hp_min=63.333f, .hp_max=64.444f, .sp_min=4.889f, .sp_max=5.000f,
      .str_min=1.000f, .str_max=1.000f, .agi_min=1.111f, .agi_max=1.222f,
      .vit_min=0.222f, .vit_max=0.222f, .int_min=0.778f, .int_max=0.889f,
      .dex_min=1.000f, .dex_max=1.000f, .luk_min=0.778f, .luk_max=0.889f },

    // Amazing
    { .hp_min=64.444f, .sp_min=5.000f,
      .str_min=1.000f, .agi_min=1.222f, .vit_min=0.222f,
      .int_min=0.889f, .dex_min=1.000f, .luk_min=0.889f }
};

static const StatRange FilirStatRange20[RANK_COUNT] = {
    // Abysmal
    { .hp_max=56.316f, .sp_max=4.211f, .str_max=0.474f, .agi_max=0.737f,
      .vit_max=0.000f, .int_max=0.421f, .dex_max=0.474f, .luk_max=0.421f },

    // Terrible
    { .hp_min=56.316f, .hp_max=57.368f, .sp_min=4.211f, .sp_max=4.316f,
      .str_min=0.474f, .str_max=0.526f, .agi_min=0.737f, .agi_max=0.789f,
      .vit_min=0.000f, .vit_max=0.000f, .int_min=0.421f, .int_max=0.421f,
      .dex_min=0.474f, .dex_max=0.526f, .luk_min=0.421f, .luk_max=0.421f },

    // Poor
    { .hp_min=57.368f, .hp_max=57.895f, .sp_min=4.316f, .sp_max=4.368f,
      .str_min=0.526f, .str_max=0.579f, .agi_min=0.789f, .agi_max=0.842f,
      .vit_min=0.000f, .vit_max=0.053f, .int_min=0.421f, .int_max=0.474f,
      .dex_min=0.526f, .dex_max=0.579f, .luk_min=0.421f, .luk_max=0.474f },

    // Below Average
    { .hp_min=57.895f, .hp_max=59.474f, .sp_min=4.368f, .sp_max=4.474f,
      .str_min=0.579f, .str_max=0.684f, .agi_min=0.842f, .agi_max=0.895f,
      .vit_min=0.053f, .vit_max=0.053f, .int_min=0.474f, .int_max=0.579f,
      .dex_min=0.579f, .dex_max=0.684f, .luk_min=0.474f, .luk_max=0.579f },

    // Average
    { .hp_min=59.474f, .hp_max=60.526f, .sp_min=4.474f, .sp_max=4.526f,
      .str_min=0.684f, .str_max=0.737f, .agi_min=0.895f, .agi_max=0.947f,
      .vit_min=0.053f, .vit_max=0.105f, .int_min=0.579f, .int_max=0.632f,
      .dex_min=0.684f, .dex_max=0.737f, .luk_min=0.579f, .luk_max=0.632f },

    // Good
    { .hp_min=60.526f, .hp_max=61.579f, .sp_min=4.526f, .sp_max=4.632f,
      .str_min=0.737f, .str_max=0.789f, .agi_min=0.947f, .agi_max=1.000f,
      .vit_min=0.105f, .vit_max=0.158f, .int_min=0.632f, .int_max=0.684f,
      .dex_min=0.737f, .dex_max=0.789f, .luk_min=0.632f, .luk_max=0.684f },

    // Very Good
    { .hp_min=61.579f, .hp_max=62.632f, .sp_min=4.632f, .sp_max=4.684f,
      .str_min=0.789f, .str_max=0.895f, .agi_min=1.000f, .agi_max=1.053f,
      .vit_min=0.158f, .vit_max=0.211f, .int_min=0.684f, .int_max=0.737f,
      .dex_min=0.789f, .dex_max=0.895f, .luk_min=0.684f, .luk_max=0.737f },

    // Excellent
    { .hp_min=62.632f, .hp_max=63.158f, .sp_min=4.684f, .sp_max=4.789f,
      .str_min=0.895f, .str_max=0.895f, .agi_min=1.053f, .agi_max=1.105f,
      .vit_min=0.211f, .vit_max=0.211f, .int_min=0.737f, .int_max=0.789f,
      .dex_min=0.895f, .dex_max=0.895f, .luk_min=0.737f, .luk_max=0.789f },

    // Amazing
    { .hp_min=63.158f, .sp_min=4.789f,
      .str_min=0.895f, .agi_min=1.105f, .vit_min=0.211f,
      .int_min=0.789f, .dex_min=0.895f, .luk_min=0.789f }
};

static const StatRange FilirStatRange30[RANK_COUNT] = {
    // Abysmal
    { .hp_max=57.241f, .sp_max=4.345f, .str_max=0.517f, .agi_max=0.793f,
      .vit_max=0.034f, .int_max=0.448f, .dex_max=0.517f, .luk_max=0.448f },

    // Terrible
    { .hp_min=57.241f, .hp_max=57.586f, .sp_min=4.345f, .sp_max=4.379f,
      .str_min=0.517f, .str_max=0.586f, .agi_min=0.793f, .agi_max=0.828f,
      .vit_min=0.034f, .vit_max=0.034f, .int_min=0.448f, .int_max=0.483f,
      .dex_min=0.517f, .dex_max=0.586f, .luk_min=0.448f, .luk_max=0.483f },

    // Poor
    { .hp_min=57.586f, .hp_max=58.621f, .sp_min=4.379f, .sp_max=4.414f,
      .str_min=0.586f, .str_max=0.621f, .agi_min=0.828f, .agi_max=0.862f,
      .vit_min=0.034f, .vit_max=0.069f, .int_min=0.483f, .int_max=0.517f,
      .dex_min=0.586f, .dex_max=0.621f, .luk_min=0.483f, .luk_max=0.517f },

    // Below Average
    { .hp_min=58.621f, .hp_max=59.310f, .sp_min=4.414f, .sp_max=4.483f,
      .str_min=0.621f, .str_max=0.690f, .agi_min=0.862f, .agi_max=0.897f,
      .vit_min=0.069f, .vit_max=0.069f, .int_min=0.517f, .int_max=0.552f,
      .dex_min=0.621f, .dex_max=0.690f, .luk_min=0.517f, .luk_max=0.552f },

    // Average
    { .hp_min=59.310f, .hp_max=60.345f, .sp_min=4.483f, .sp_max=4.517f,
      .str_min=0.690f, .str_max=0.724f, .agi_min=0.897f, .agi_max=0.931f,
      .vit_min=0.069f, .vit_max=0.103f, .int_min=0.552f, .int_max=0.621f,
      .dex_min=0.690f, .dex_max=0.724f, .luk_min=0.552f, .luk_max=0.621f },

    // Good
    { .hp_min=60.345f, .hp_max=61.379f, .sp_min=4.517f, .sp_max=4.586f,
      .str_min=0.724f, .str_max=0.793f, .agi_min=0.931f, .agi_max=1.000f,
      .vit_min=0.103f, .vit_max=0.138f, .int_min=0.621f, .int_max=0.655f,
      .dex_min=0.724f, .dex_max=0.793f, .luk_min=0.621f, .luk_max=0.655f },

    // Very Good
    { .hp_min=61.379f, .hp_max=62.069f, .sp_min=4.586f, .sp_max=4.621f,
      .str_min=0.793f, .str_max=0.828f, .agi_min=1.000f, .agi_max=1.034f,
      .vit_min=0.138f, .vit_max=0.172f, .int_min=0.655f, .int_max=0.690f,
      .dex_min=0.793f, .dex_max=0.828f, .luk_min=0.655f, .luk_max=0.690f },

    // Excellent
    { .hp_min=62.069f, .hp_max=62.759f, .sp_min=4.621f, .sp_max=4.655f,
      .str_min=0.828f, .str_max=0.897f, .agi_min=1.034f, .agi_max=1.069f,
      .vit_min=0.172f, .vit_max=0.207f, .int_min=0.690f, .int_max=0.724f,
      .dex_min=0.828f, .dex_max=0.897f, .luk_min=0.690f, .luk_max=0.724f },

    // Amazing
    { .hp_min=62.759f, .sp_min=4.655f,
      .str_min=0.897f, .agi_min=1.069f, .vit_min=0.207f,
      .int_min=0.724f, .dex_min=0.897f, .luk_min=0.724f }
};

static const StatRange FilirStatRange40[RANK_COUNT] = {
    // Abysmal
    { .hp_max=57.692f, .sp_max=4.359f, .str_max=0.564f, .agi_max=0.795f,
      .vit_max=0.026f, .int_max=0.462f, .dex_max=0.564f, .luk_max=0.462f },

    // Terrible
    { .hp_min=57.692f, .hp_max=58.205f, .sp_min=4.359f, .sp_max=4.410f,
      .str_min=0.564f, .str_max=0.590f, .agi_min=0.795f, .agi_max=0.821f,
      .vit_min=0.026f, .vit_max=0.051f, .int_min=0.462f, .int_max=0.487f,
      .dex_min=0.564f, .dex_max=0.590f, .luk_min=0.462f, .luk_max=0.487f },

    // Poor
    { .hp_min=58.205f, .hp_max=58.718f, .sp_min=4.410f, .sp_max=4.436f,
      .str_min=0.590f, .str_max=0.641f, .agi_min=0.821f, .agi_max=0.872f,
      .vit_min=0.051f, .vit_max=0.051f, .int_min=0.487f, .int_max=0.513f,
      .dex_min=0.590f, .dex_max=0.641f, .luk_min=0.487f, .luk_max=0.513f },

    // Below Average
    { .hp_min=58.718f, .hp_max=59.487f, .sp_min=4.436f, .sp_max=4.487f,
      .str_min=0.641f, .str_max=0.692f, .agi_min=0.872f, .agi_max=0.897f,
      .vit_min=0.051f, .vit_max=0.077f, .int_min=0.513f, .int_max=0.564f,
      .dex_min=0.641f, .dex_max=0.692f, .luk_min=0.513f, .luk_max=0.564f },

    // Average
    { .hp_min=59.487f, .hp_max=60.256f, .sp_min=4.487f, .sp_max=4.513f,
      .str_min=0.692f, .str_max=0.718f, .agi_min=0.897f, .agi_max=0.949f,
      .vit_min=0.077f, .vit_max=0.103f, .int_min=0.564f, .int_max=0.615f,
      .dex_min=0.692f, .dex_max=0.718f, .luk_min=0.564f, .luk_max=0.615f },

    // Good
    { .hp_min=60.256f, .hp_max=61.026f, .sp_min=4.513f, .sp_max=4.564f,
      .str_min=0.718f, .str_max=0.795f, .agi_min=0.949f, .agi_max=0.974f,
      .vit_min=0.103f, .vit_max=0.128f, .int_min=0.615f, .int_max=0.667f,
      .dex_min=0.718f, .dex_max=0.795f, .luk_min=0.615f, .luk_max=0.667f },

    // Very Good
    { .hp_min=61.026f, .hp_max=61.795f, .sp_min=4.564f, .sp_max=4.590f,
      .str_min=0.795f, .str_max=0.821f, .agi_min=0.974f, .agi_max=1.026f,
      .vit_min=0.128f, .vit_max=0.154f, .int_min=0.667f, .int_max=0.692f,
      .dex_min=0.795f, .dex_max=0.821f, .luk_min=0.667f, .luk_max=0.692f },

    // Excellent
    { .hp_min=61.795f, .hp_max=62.308f, .sp_min=4.590f, .sp_max=4.641f,
      .str_min=0.821f, .str_max=0.846f, .agi_min=1.026f, .agi_max=1.051f,
      .vit_min=0.154f, .vit_max=0.179f, .int_min=0.692f, .int_max=0.718f,
      .dex_min=0.821f, .dex_max=0.846f, .luk_min=0.692f, .luk_max=0.718f },

    // Amazing
    { .hp_min=62.308f, .sp_min=4.641f,
      .str_min=0.846f, .agi_min=1.051f, .vit_min=0.179f,
      .int_min=0.718f, .dex_min=0.846f, .luk_min=0.718f }
};

static const StatRange FilirStatRange50[RANK_COUNT] = {
    // Abysmal
    { .hp_max=57.959f, .sp_max=4.367f, .str_max=0.571f, .agi_max=0.816f,
      .vit_max=0.041f, .int_max=0.469f, .dex_max=0.571f, .luk_max=0.469f },

    // Terrible
    { .hp_min=57.959f, .hp_max=58.367f, .sp_min=4.367f, .sp_max=4.408f,
      .str_min=0.571f, .str_max=0.592f, .agi_min=0.816f, .agi_max=0.837f,
      .vit_min=0.041f, .vit_max=0.041f, .int_min=0.469f, .int_max=0.490f,
      .dex_min=0.571f, .dex_max=0.592f, .luk_min=0.469f, .luk_max=0.490f },

    // Poor
    { .hp_min=58.367f, .hp_max=58.776f, .sp_min=4.408f, .sp_max=4.449f,
      .str_min=0.592f, .str_max=0.633f, .agi_min=0.837f, .agi_max=0.857f,
      .vit_min=0.041f, .vit_max=0.061f, .int_min=0.490f, .int_max=0.531f,
      .dex_min=0.592f, .dex_max=0.633f, .luk_min=0.490f, .luk_max=0.531f },

    // Below Average
    { .hp_min=58.776f, .hp_max=59.592f, .sp_min=4.449f, .sp_max=4.490f,
      .str_min=0.633f, .str_max=0.694f, .agi_min=0.857f, .agi_max=0.898f,
      .vit_min=0.061f, .vit_max=0.082f, .int_min=0.531f, .int_max=0.571f,
      .dex_min=0.633f, .dex_max=0.694f, .luk_min=0.531f, .luk_max=0.571f },

    // Average
    { .hp_min=59.592f, .hp_max=60.204f, .sp_min=4.490f, .sp_max=4.510f,
      .str_min=0.694f, .str_max=0.735f, .agi_min=0.898f, .agi_max=0.939f,
      .vit_min=0.082f, .vit_max=0.102f, .int_min=0.571f, .int_max=0.612f,
      .dex_min=0.694f, .dex_max=0.735f, .luk_min=0.571f, .luk_max=0.612f },

    // Good
    { .hp_min=60.204f, .hp_max=61.020f, .sp_min=4.510f, .sp_max=4.551f,
      .str_min=0.735f, .str_max=0.776f, .agi_min=0.939f, .agi_max=0.980f,
      .vit_min=0.102f, .vit_max=0.143f, .int_min=0.612f, .int_max=0.653f,
      .dex_min=0.735f, .dex_max=0.776f, .luk_min=0.612f, .luk_max=0.653f },

    // Very Good
    { .hp_min=61.020f, .hp_max=61.633f, .sp_min=4.551f, .sp_max=4.592f,
      .str_min=0.776f, .str_max=0.816f, .agi_min=0.980f, .agi_max=1.000f,
      .vit_min=0.143f, .vit_max=0.163f, .int_min=0.653f, .int_max=0.673f,
      .dex_min=0.776f, .dex_max=0.816f, .luk_min=0.653f, .luk_max=0.673f },

    // Excellent
    { .hp_min=61.633f, .hp_max=62.041f, .sp_min=4.592f, .sp_max=4.633f,
      .str_min=0.816f, .str_max=0.837f, .agi_min=1.000f, .agi_max=1.041f,
      .vit_min=0.163f, .vit_max=0.163f, .int_min=0.673f, .int_max=0.694f,
      .dex_min=0.816f, .dex_max=0.837f, .luk_min=0.673f, .luk_max=0.694f },

    // Amazing
    { .hp_min=62.041f, .sp_min=4.633f,
      .str_min=0.837f, .agi_min=1.041f, .vit_min=0.163f,
      .int_min=0.694f, .dex_min=0.837f, .luk_min=0.694f }
};

static const StatRange FilirStatRange60[RANK_COUNT] = {
    // Abysmal
    { .hp_max=58.136f, .sp_max=4.407f, .str_max=0.576f, .agi_max=0.831f,
      .vit_max=0.034f, .int_max=0.475f, .dex_max=0.593f, .luk_max=0.475f },

    // Terrible
    { .hp_min=58.136f, .hp_max=58.475f, .sp_min=4.407f, .sp_max=4.424f,
      .str_min=0.576f, .str_max=0.610f, .agi_min=0.831f, .agi_max=0.847f,
      .vit_min=0.034f, .vit_max=0.051f, .int_min=0.475f, .int_max=0.508f,
      .dex_min=0.593f, .dex_max=0.610f, .luk_min=0.475f, .luk_max=0.508f },

    // Poor
    { .hp_min=58.475f, .hp_max=58.983f, .sp_min=4.424f, .sp_max=4.458f,
      .str_min=0.610f, .str_max=0.644f, .agi_min=0.847f, .agi_max=0.864f,
      .vit_min=0.051f, .vit_max=0.068f, .int_min=0.508f, .int_max=0.525f,
      .dex_min=0.610f, .dex_max=0.644f, .luk_min=0.508f, .luk_max=0.542f },

    // Below Average
    { .hp_min=58.983f, .hp_max=59.661f, .sp_min=4.458f, .sp_max=4.492f,
      .str_min=0.644f, .str_max=0.695f, .agi_min=0.864f, .agi_max=0.915f,
      .vit_min=0.068f, .vit_max=0.085f, .int_min=0.525f, .int_max=0.576f,
      .dex_min=0.644f, .dex_max=0.678f, .luk_min=0.542f, .luk_max=0.576f },

    // Average
    { .hp_min=59.661f, .hp_max=60.169f, .sp_min=4.492f, .sp_max=4.508f,
      .str_min=0.695f, .str_max=0.729f, .agi_min=0.915f, .agi_max=0.932f,
      .vit_min=0.085f, .vit_max=0.102f, .int_min=0.576f, .int_max=0.610f,
      .dex_min=0.678f, .dex_max=0.729f, .luk_min=0.576f, .luk_max=0.610f },

    // Good
    { .hp_min=60.169f, .hp_max=60.847f, .sp_min=4.508f, .sp_max=4.542f,
      .str_min=0.729f, .str_max=0.763f, .agi_min=0.932f, .agi_max=0.983f,
      .vit_min=0.102f, .vit_max=0.136f, .int_min=0.610f, .int_max=0.644f,
      .dex_min=0.729f, .dex_max=0.763f, .luk_min=0.610f, .luk_max=0.644f },

    // Very Good
    { .hp_min=60.847f, .hp_max=61.356f, .sp_min=4.542f, .sp_max=4.576f,
      .str_min=0.763f, .str_max=0.797f, .agi_min=0.983f, .agi_max=1.000f,
      .vit_min=0.136f, .vit_max=0.153f, .int_min=0.644f, .int_max=0.678f,
      .dex_min=0.763f, .dex_max=0.797f, .luk_min=0.644f, .luk_max=0.678f },

    // Excellent
    { .hp_min=61.356f, .hp_max=61.864f, .sp_min=4.576f, .sp_max=4.593f,
      .str_min=0.797f, .str_max=0.831f, .agi_min=1.000f, .agi_max=1.017f,
      .vit_min=0.153f, .vit_max=0.169f, .int_min=0.678f, .int_max=0.695f,
      .dex_min=0.797f, .dex_max=0.831f, .luk_min=0.678f, .luk_max=0.695f },

    // Amazing
    { .hp_min=61.864f, .sp_min=4.593f,
      .str_min=0.831f, .agi_min=1.017f, .vit_min=0.169f,
      .int_min=0.695f, .dex_min=0.831f, .luk_min=0.695f }
};

static const StatRange FilirStatRange70[RANK_COUNT] = {
    // Abysmal
    { .hp_max=58.261f, .sp_max=4.420f, .str_max=0.594f, .agi_max=0.826f,
      .vit_max=0.043f, .int_max=0.493f, .dex_max=0.594f, .luk_max=0.493f },

    // Terrible
    { .hp_min=58.261f, .hp_max=58.551f, .sp_min=4.420f, .sp_max=4.435f,
      .str_min=0.594f, .str_max=0.623f, .agi_min=0.826f, .agi_max=0.855f,
      .vit_min=0.043f, .vit_max=0.058f, .int_min=0.493f, .int_max=0.507f,
      .dex_min=0.594f, .dex_max=0.623f, .luk_min=0.493f, .luk_max=0.507f },

    // Poor
    { .hp_min=58.551f, .hp_max=58.986f, .sp_min=4.435f, .sp_max=4.464f,
      .str_min=0.623f, .str_max=0.652f, .agi_min=0.855f, .agi_max=0.870f,
      .vit_min=0.058f, .vit_max=0.072f, .int_min=0.507f, .int_max=0.536f,
      .dex_min=0.623f, .dex_max=0.652f, .luk_min=0.507f, .luk_max=0.536f },

    // Below Average
    { .hp_min=58.986f, .hp_max=59.710f, .sp_min=4.464f, .sp_max=4.493f,
      .str_min=0.652f, .str_max=0.681f, .agi_min=0.870f, .agi_max=0.913f,
      .vit_min=0.072f, .vit_max=0.087f, .int_min=0.536f, .int_max=0.580f,
      .dex_min=0.652f, .dex_max=0.681f, .luk_min=0.536f, .luk_max=0.580f },

    // Average
    { .hp_min=59.710f, .hp_max=60.145f, .sp_min=4.493f, .sp_max=4.507f,
      .str_min=0.681f, .str_max=0.725f, .agi_min=0.913f, .agi_max=0.942f,
      .vit_min=0.087f, .vit_max=0.101f, .int_min=0.580f, .int_max=0.609f,
      .dex_min=0.681f, .dex_max=0.725f, .luk_min=0.580f, .luk_max=0.609f },

    // Good
    { .hp_min=60.145f, .hp_max=60.870f, .sp_min=4.507f, .sp_max=4.536f,
      .str_min=0.725f, .str_max=0.768f, .agi_min=0.942f, .agi_max=0.971f,
      .vit_min=0.101f, .vit_max=0.130f, .int_min=0.609f, .int_max=0.638f,
      .dex_min=0.725f, .dex_max=0.768f, .luk_min=0.609f, .luk_max=0.638f },

    // Very Good
    { .hp_min=60.870f, .hp_max=61.304f, .sp_min=4.536f, .sp_max=4.565f,
      .str_min=0.768f, .str_max=0.797f, .agi_min=0.971f, .agi_max=1.000f,
      .vit_min=0.130f, .vit_max=0.145f, .int_min=0.638f, .int_max=0.667f,
      .dex_min=0.768f, .dex_max=0.797f, .luk_min=0.638f, .luk_max=0.667f },

    // Excellent
    { .hp_min=61.304f, .hp_max=61.739f, .sp_min=4.565f, .sp_max=4.580f,
      .str_min=0.797f, .str_max=0.826f, .agi_min=1.000f, .agi_max=1.014f,
      .vit_min=0.145f, .vit_max=0.159f, .int_min=0.667f, .int_max=0.681f,
      .dex_min=0.797f, .dex_max=0.826f, .luk_min=0.667f, .luk_max=0.681f },

    // Amazing
    { .hp_min=61.739f, .sp_min=4.580f,
      .str_min=0.826f, .agi_min=1.014f, .vit_min=0.159f,
      .int_min=0.681f, .dex_min=0.826f, .luk_min=0.681f }
};

static const StatRange FilirStatRange80[RANK_COUNT] = {
    // Abysmal
    { .hp_max=58.354f, .sp_max=4.405f, .str_max=0.595f, .agi_max=0.835f,
      .vit_max=0.051f, .int_max=0.494f, .dex_max=0.608f, .luk_max=0.494f },

    // Terrible
    { .hp_min=58.354f, .hp_max=58.608f, .sp_min=4.405f, .sp_max=4.443f,
      .str_min=0.595f, .str_max=0.620f, .agi_min=0.835f, .agi_max=0.861f,
      .vit_min=0.051f, .vit_max=0.063f, .int_min=0.494f, .int_max=0.519f,
      .dex_min=0.608f, .dex_max=0.620f, .luk_min=0.494f, .luk_max=0.519f },

    // Poor
    { .hp_min=58.608f, .hp_max=59.114f, .sp_min=4.443f, .sp_max=4.468f,
      .str_min=0.620f, .str_max=0.646f, .agi_min=0.861f, .agi_max=0.873f,
      .vit_min=0.063f, .vit_max=0.076f, .int_min=0.519f, .int_max=0.544f,
      .dex_min=0.620f, .dex_max=0.646f, .luk_min=0.519f, .luk_max=0.544f },

    // Below Average
    { .hp_min=59.114f, .hp_max=59.620f, .sp_min=4.468f, .sp_max=4.494f,
      .str_min=0.646f, .str_max=0.684f, .agi_min=0.873f, .agi_max=0.911f,
      .vit_min=0.076f, .vit_max=0.089f, .int_min=0.544f, .int_max=0.570f,
      .dex_min=0.646f, .dex_max=0.684f, .luk_min=0.544f, .luk_max=0.570f },

    // Average
    { .hp_min=59.620f, .hp_max=60.127f, .sp_min=4.494f, .sp_max=4.506f,
      .str_min=0.684f, .str_max=0.722f, .agi_min=0.911f, .agi_max=0.937f,
      .vit_min=0.089f, .vit_max=0.101f, .int_min=0.570f, .int_max=0.608f,
      .dex_min=0.684f, .dex_max=0.722f, .luk_min=0.570f, .luk_max=0.608f },

    // Good
    { .hp_min=60.127f, .hp_max=60.759f, .sp_min=4.506f, .sp_max=4.532f,
      .str_min=0.722f, .str_max=0.759f, .agi_min=0.937f, .agi_max=0.962f,
      .vit_min=0.101f, .vit_max=0.127f, .int_min=0.608f, .int_max=0.633f,
      .dex_min=0.722f, .dex_max=0.759f, .luk_min=0.608f, .luk_max=0.633f },

    // Very Good
    { .hp_min=60.759f, .hp_max=61.266f, .sp_min=4.532f, .sp_max=4.557f,
      .str_min=0.759f, .str_max=0.785f, .agi_min=0.962f, .agi_max=0.987f,
      .vit_min=0.127f, .vit_max=0.139f, .int_min=0.633f, .int_max=0.658f,
      .dex_min=0.759f, .dex_max=0.785f, .luk_min=0.633f, .luk_max=0.658f },

    // Excellent
    { .hp_min=61.266f, .hp_max=61.646f, .sp_min=4.557f, .sp_max=4.595f,
      .str_min=0.785f, .str_max=0.810f, .agi_min=0.987f, .agi_max=1.013f,
      .vit_min=0.139f, .vit_max=0.152f, .int_min=0.658f, .int_max=0.684f,
      .dex_min=0.785f, .dex_max=0.810f, .luk_min=0.658f, .luk_max=0.684f },

    // Amazing
    { .hp_min=61.646f, .sp_min=4.595f,
      .str_min=0.810f, .agi_min=1.013f, .vit_min=0.152f,
      .int_min=0.684f, .dex_min=0.810f, .luk_min=0.684f }
};

static const StatRange FilirStatRange90[RANK_COUNT] = {
    // Abysmal
    { .hp_max=58.427f, .sp_max=4.427f, .str_max=0.607f, .agi_max=0.843f,
      .vit_max=0.056f, .int_max=0.506f, .dex_max=0.607f, .luk_max=0.506f },

    // Terrible
    { .hp_min=58.427f, .hp_max=58.764f, .sp_min=4.427f, .sp_max=4.449f,
      .str_min=0.607f, .str_max=0.629f, .agi_min=0.843f, .agi_max=0.854f,
      .vit_min=0.056f, .vit_max=0.056f, .int_min=0.506f, .int_max=0.517f,
      .dex_min=0.607f, .dex_max=0.629f, .luk_min=0.506f, .luk_max=0.517f },

    // Poor
    { .hp_min=58.764f, .hp_max=59.101f, .sp_min=4.449f, .sp_max=4.472f,
      .str_min=0.629f, .str_max=0.652f, .agi_min=0.854f, .agi_max=0.876f,
      .vit_min=0.056f, .vit_max=0.079f, .int_min=0.517f, .int_max=0.539f,
      .dex_min=0.629f, .dex_max=0.652f, .luk_min=0.517f, .luk_max=0.539f },

    // Below Average
    { .hp_min=59.101f, .hp_max=59.663f, .sp_min=4.472f, .sp_max=4.494f,
      .str_min=0.652f, .str_max=0.685f, .agi_min=0.876f, .agi_max=0.910f,
      .vit_min=0.079f, .vit_max=0.090f, .int_min=0.539f, .int_max=0.573f,
      .dex_min=0.652f, .dex_max=0.685f, .luk_min=0.539f, .luk_max=0.573f },

    // Average
    { .hp_min=59.663f, .hp_max=60.225f, .sp_min=4.494f, .sp_max=4.506f,
      .str_min=0.685f, .str_max=0.719f, .agi_min=0.910f, .agi_max=0.933f,
      .vit_min=0.090f, .vit_max=0.112f, .int_min=0.573f, .int_max=0.607f,
      .dex_min=0.685f, .dex_max=0.719f, .luk_min=0.573f, .luk_max=0.607f },

    // Good
    { .hp_min=60.225f, .hp_max=60.787f, .sp_min=4.506f, .sp_max=4.528f,
      .str_min=0.719f, .str_max=0.753f, .agi_min=0.933f, .agi_max=0.966f,
      .vit_min=0.112f, .vit_max=0.124f, .int_min=0.607f, .int_max=0.629f,
      .dex_min=0.719f, .dex_max=0.753f, .luk_min=0.607f, .luk_max=0.629f },

    // Very Good
    { .hp_min=60.787f, .hp_max=61.124f, .sp_min=4.528f, .sp_max=4.551f,
      .str_min=0.753f, .str_max=0.787f, .agi_min=0.966f, .agi_max=0.989f,
      .vit_min=0.124f, .vit_max=0.146f, .int_min=0.629f, .int_max=0.652f,
      .dex_min=0.753f, .dex_max=0.787f, .luk_min=0.629f, .luk_max=0.652f },

    // Excellent
    { .hp_min=61.124f, .hp_max=61.573f, .sp_min=4.551f, .sp_max=4.573f,
      .str_min=0.787f, .str_max=0.809f, .agi_min=0.989f, .agi_max=1.011f,
      .vit_min=0.146f, .vit_max=0.157f, .int_min=0.652f, .int_max=0.674f,
      .dex_min=0.787f, .dex_max=0.809f, .luk_min=0.652f, .luk_max=0.674f },

    // Amazing
    { .hp_min=61.573f, .sp_min=4.573f,
      .str_min=0.809f, .agi_min=1.011f, .vit_min=0.157f,
      .int_min=0.674f, .dex_min=0.809f, .luk_min=0.674f }
};

static const StatRange FilirStatRange99[RANK_COUNT] = {
    // Abysmal
    { .hp_max=58.469f, .sp_max=4.439f, .str_max=0.612f, .agi_max=0.847f,
      .vit_max=0.051f, .int_max=0.510f, .dex_max=0.612f, .luk_max=0.510f },

    // Terrible
    { .hp_min=58.469f, .hp_max=58.776f, .sp_min=4.439f, .sp_max=4.449f,
      .str_min=0.612f, .str_max=0.633f, .agi_min=0.847f, .agi_max=0.857f,
      .vit_min=0.051f, .vit_max=0.061f, .int_min=0.510f, .int_max=0.520f,
      .dex_min=0.612f, .dex_max=0.633f, .luk_min=0.510f, .luk_max=0.520f },

    // Poor
    { .hp_min=58.776f, .hp_max=59.184f, .sp_min=4.449f, .sp_max=4.469f,
      .str_min=0.633f, .str_max=0.653f, .agi_min=0.857f, .agi_max=0.888f,
      .vit_min=0.061f, .vit_max=0.071f, .int_min=0.520f, .int_max=0.541f,
      .dex_min=0.633f, .dex_max=0.653f, .luk_min=0.520f, .luk_max=0.551f },

    // Below Average
    { .hp_min=59.184f, .hp_max=59.694f, .sp_min=4.469f, .sp_max=4.490f,
      .str_min=0.653f, .str_max=0.694f, .agi_min=0.888f, .agi_max=0.908f,
      .vit_min=0.071f, .vit_max=0.092f, .int_min=0.541f, .int_max=0.571f,
      .dex_min=0.653f, .dex_max=0.694f, .luk_min=0.551f, .luk_max=0.571f },

    // Average
    { .hp_min=59.694f, .hp_max=60.204f, .sp_min=4.490f, .sp_max=4.510f,
      .str_min=0.694f, .str_max=0.724f, .agi_min=0.908f, .agi_max=0.939f,
      .vit_min=0.092f, .vit_max=0.102f, .int_min=0.571f, .int_max=0.602f,
      .dex_min=0.694f, .dex_max=0.724f, .luk_min=0.571f, .luk_max=0.602f },

    // Good
    { .hp_min=60.204f, .hp_max=60.714f, .sp_min=4.510f, .sp_max=4.531f,
      .str_min=0.724f, .str_max=0.755f, .agi_min=0.939f, .agi_max=0.959f,
      .vit_min=0.102f, .vit_max=0.122f, .int_min=0.602f, .int_max=0.633f,
      .dex_min=0.724f, .dex_max=0.755f, .luk_min=0.602f, .luk_max=0.633f },

    // Very Good
    { .hp_min=60.714f, .hp_max=61.122f, .sp_min=4.531f, .sp_max=4.551f,
      .str_min=0.755f, .str_max=0.776f, .agi_min=0.959f, .agi_max=0.980f,
      .vit_min=0.122f, .vit_max=0.143f, .int_min=0.633f, .int_max=0.653f,
      .dex_min=0.755f, .dex_max=0.776f, .luk_min=0.633f, .luk_max=0.653f },

    // Excellent
    { .hp_min=61.122f, .hp_max=61.429f, .sp_min=4.551f, .sp_max=4.561f,
      .str_min=0.776f, .str_max=0.796f, .agi_min=0.980f, .agi_max=1.000f,
      .vit_min=0.143f, .vit_max=0.153f, .int_min=0.653f, .int_max=0.673f,
      .dex_min=0.776f, .dex_max=0.796f, .luk_min=0.653f, .luk_max=0.673f },

    // Amazing
    { .hp_min=61.429f, .sp_min=4.561f,
      .str_min=0.796f, .agi_min=1.000f, .vit_min=0.153f,
      .int_min=0.673f, .dex_min=0.796f, .luk_min=0.673f }
};

static const StatRange *filir_stat_ranges[] = {
    FilirStatRange10,
    FilirStatRange20,
    FilirStatRange30,
    FilirStatRange40,
    FilirStatRange50,
    FilirStatRange60,
    FilirStatRange70,
    FilirStatRange80,
    FilirStatRange90,
    FilirStatRange99
};

// ================= End of Filir ====================

// ================= AMISTR GROWTH SHEET ================

static const StatRange AmistrStatRange10[RANK_COUNT] = {
    // Abysmal
    { .hp_max=96.667f, .sp_max=2.000f, .str_max=0.667f, .agi_max=0.444f,
      .vit_max=0.444f, .int_max=0.000f, .dex_max=0.333f, .luk_max=0.333f },

    // Terrible
    { .hp_min=96.667f, .hp_max=97.778f, .sp_min=2.000f, .sp_max=2.111f,
      .str_min=0.667f, .str_max=0.778f, .agi_min=0.444f, .agi_max=0.444f,
      .vit_min=0.444f, .vit_max=0.444f, .int_min=0.000f, .int_max=0.000f,
      .dex_min=0.333f, .dex_max=0.333f, .luk_min=0.333f, .luk_max=0.333f },

    // Poor
    { .hp_min=97.778f, .hp_max=100.000f, .sp_min=2.111f, .sp_max=2.333f,
      .str_min=0.778f, .str_max=0.778f, .agi_min=0.444f, .agi_max=0.556f,
      .vit_min=0.444f, .vit_max=0.556f, .int_min=0.000f, .int_max=0.000f,
      .dex_min=0.333f, .dex_max=0.444f, .luk_min=0.333f, .luk_max=0.444f },

    // Below Average
    { .hp_min=100.000f, .hp_max=103.333f, .sp_min=2.333f, .sp_max=2.444f,
      .str_min=0.778f, .str_max=0.889f, .agi_min=0.556f, .agi_max=0.667f,
      .vit_min=0.556f, .vit_max=0.667f, .int_min=0.000f, .int_max=0.111f,
      .dex_min=0.444f, .dex_max=0.556f, .luk_min=0.444f, .luk_max=0.556f },

    // Average
    { .hp_min=103.333f, .hp_max=105.556f, .sp_min=2.444f, .sp_max=2.556f,
      .str_min=0.889f, .str_max=1.000f, .agi_min=0.667f, .agi_max=0.778f,
      .vit_min=0.667f, .vit_max=0.778f, .int_min=0.111f, .int_max=0.111f,
      .dex_min=0.556f, .dex_max=0.667f, .luk_min=0.556f, .luk_max=0.667f },

    // Good
    { .hp_min=105.556f, .hp_max=108.889f, .sp_min=2.556f, .sp_max=2.667f,
      .str_min=1.000f, .str_max=1.000f, .agi_min=0.778f, .agi_max=0.889f,
      .vit_min=0.778f, .vit_max=0.889f, .int_min=0.111f, .int_max=0.222f,
      .dex_min=0.667f, .dex_max=0.778f, .luk_min=0.667f, .luk_max=0.778f },

    // Very Good
    { .hp_min=108.889f, .hp_max=111.111f, .sp_min=2.667f, .sp_max=2.889f,
      .str_min=1.000f, .str_max=1.111f, .agi_min=0.889f, .agi_max=1.000f,
      .vit_min=0.889f, .vit_max=1.000f, .int_min=0.222f, .int_max=0.222f,
      .dex_min=0.778f, .dex_max=0.778f, .luk_min=0.778f, .luk_max=0.778f },

    // Excellent
    { .hp_min=111.111f, .hp_max=112.222f, .sp_min=2.889f, .sp_max=3.000f,
      .str_min=1.111f, .str_max=1.222f, .agi_min=1.000f, .agi_max=1.000f,
      .vit_min=1.000f, .vit_max=1.000f, .int_min=0.222f, .int_max=0.333f,
      .dex_min=0.778f, .dex_max=0.889f, .luk_min=0.778f, .luk_max=0.889f },

    // Amazing
    { .hp_min=112.222f, .sp_min=3.000f,
      .str_min=1.222f, .agi_min=1.000f, .vit_min=1.000f,
      .int_min=0.333f, .dex_min=0.889f, .luk_min=0.889f }
};

static const StatRange AmistrStatRange20[RANK_COUNT] = {
    // Abysmal
    { .hp_max=98.947f, .sp_max=2.211f, .str_max=0.737f, .agi_max=0.474f,
      .vit_max=0.474f, .int_max=0.000f, .dex_max=0.421f, .luk_max=0.421f },

    // Terrible
    { .hp_min=98.947f, .hp_max=100.526f, .sp_min=2.211f, .sp_max=2.316f,
      .str_min=0.737f, .str_max=0.789f, .agi_min=0.474f, .agi_max=0.526f,
      .vit_min=0.474f, .vit_max=0.526f, .int_min=0.000f, .int_max=0.000f,
      .dex_min=0.421f, .dex_max=0.421f, .luk_min=0.421f, .luk_max=0.421f },

    // Poor
    { .hp_min=100.526f, .hp_max=102.105f, .sp_min=2.316f, .sp_max=2.368f,
      .str_min=0.789f, .str_max=0.842f, .agi_min=0.526f, .agi_max=0.579f,
      .vit_min=0.526f, .vit_max=0.579f, .int_min=0.000f, .int_max=0.053f,
      .dex_min=0.421f, .dex_max=0.474f, .luk_min=0.421f, .luk_max=0.474f },

    // Below Average
    { .hp_min=102.105f, .hp_max=103.684f, .sp_min=2.368f, .sp_max=2.474f,
      .str_min=0.842f, .str_max=0.895f, .agi_min=0.579f, .agi_max=0.684f,
      .vit_min=0.579f, .vit_max=0.684f, .int_min=0.053f, .int_max=0.053f,
      .dex_min=0.474f, .dex_max=0.579f, .luk_min=0.474f, .luk_max=0.579f },

    // Average
    { .hp_min=103.684f, .hp_max=105.789f, .sp_min=2.474f, .sp_max=2.526f,
      .str_min=0.895f, .str_max=0.947f, .agi_min=0.684f, .agi_max=0.737f,
      .vit_min=0.684f, .vit_max=0.737f, .int_min=0.053f, .int_max=0.105f,
      .dex_min=0.579f, .dex_max=0.632f, .luk_min=0.579f, .luk_max=0.632f },

    // Good
    { .hp_min=105.789f, .hp_max=107.368f, .sp_min=2.526f, .sp_max=2.632f,
      .str_min=0.947f, .str_max=1.000f, .agi_min=0.737f, .agi_max=0.789f,
      .vit_min=0.737f, .vit_max=0.789f, .int_min=0.105f, .int_max=0.158f,
      .dex_min=0.632f, .dex_max=0.684f, .luk_min=0.632f, .luk_max=0.684f },

    // Very Good
    { .hp_min=107.368f, .hp_max=108.947f, .sp_min=2.632f, .sp_max=2.684f,
      .str_min=1.000f, .str_max=1.053f, .agi_min=0.789f, .agi_max=0.895f,
      .vit_min=0.789f, .vit_max=0.895f, .int_min=0.158f, .int_max=0.211f,
      .dex_min=0.684f, .dex_max=0.737f, .luk_min=0.684f, .luk_max=0.737f },

    // Excellent
    { .hp_min=108.947f, .hp_max=110.526f, .sp_min=2.684f, .sp_max=2.789f,
      .str_min=1.053f, .str_max=1.105f, .agi_min=0.895f, .agi_max=0.895f,
      .vit_min=0.895f, .vit_max=0.895f, .int_min=0.211f, .int_max=0.211f,
      .dex_min=0.737f, .dex_max=0.789f, .luk_min=0.737f, .luk_max=0.789f },

    // Amazing
    { .hp_min=110.526f, .sp_min=2.789f,
      .str_min=1.105f, .agi_min=0.895f, .vit_min=0.895f,
      .int_min=0.211f, .dex_min=0.789f, .luk_min=0.789f }
};

static const StatRange AmistrStatRange30[RANK_COUNT] = {
    // Abysmal
    { .hp_max=100.345f, .sp_max=2.345f, .str_max=0.793f, .agi_max=0.517f,
      .vit_max=0.517f, .int_max=0.034f, .dex_max=0.448f, .luk_max=0.448f },

    // Terrible
    { .hp_min=100.345f, .hp_max=101.379f, .sp_min=2.345f, .sp_max=2.379f,
      .str_min=0.793f, .str_max=0.828f, .agi_min=0.517f, .agi_max=0.586f,
      .vit_min=0.517f, .vit_max=0.586f, .int_min=0.034f, .int_max=0.034f,
      .dex_min=0.448f, .dex_max=0.483f, .luk_min=0.448f, .luk_max=0.483f },

    // Poor
    { .hp_min=101.379f, .hp_max=102.414f, .sp_min=2.379f, .sp_max=2.414f,
      .str_min=0.828f, .str_max=0.862f, .agi_min=0.586f, .agi_max=0.621f,
      .vit_min=0.586f, .vit_max=0.621f, .int_min=0.034f, .int_max=0.069f,
      .dex_min=0.483f, .dex_max=0.517f, .luk_min=0.483f, .luk_max=0.517f },

    // Below Average
    { .hp_min=102.414f, .hp_max=104.138f, .sp_min=2.414f, .sp_max=2.483f,
      .str_min=0.862f, .str_max=0.897f, .agi_min=0.621f, .agi_max=0.690f,
      .vit_min=0.621f, .vit_max=0.690f, .int_min=0.069f, .int_max=0.069f,
      .dex_min=0.517f, .dex_max=0.552f, .luk_min=0.517f, .luk_max=0.552f },

    // Average
    { .hp_min=104.138f, .hp_max=105.517f, .sp_min=2.483f, .sp_max=2.517f,
      .str_min=0.897f, .str_max=0.931f, .agi_min=0.690f, .agi_max=0.724f,
      .vit_min=0.690f, .vit_max=0.724f, .int_min=0.069f, .int_max=0.103f,
      .dex_min=0.552f, .dex_max=0.621f, .luk_min=0.552f, .luk_max=0.621f },

    // Good
    { .hp_min=105.517f, .hp_max=107.241f, .sp_min=2.517f, .sp_max=2.586f,
      .str_min=0.931f, .str_max=1.000f, .agi_min=0.724f, .agi_max=0.793f,
      .vit_min=0.724f, .vit_max=0.793f, .int_min=0.103f, .int_max=0.138f,
      .dex_min=0.621f, .dex_max=0.655f, .luk_min=0.621f, .luk_max=0.655f },

    // Very Good
    { .hp_min=107.241f, .hp_max=108.276f, .sp_min=2.586f, .sp_max=2.621f,
      .str_min=1.000f, .str_max=1.034f, .agi_min=0.793f, .agi_max=0.828f,
      .vit_min=0.793f, .vit_max=0.828f, .int_min=0.138f, .int_max=0.172f,
      .dex_min=0.655f, .dex_max=0.690f, .luk_min=0.655f, .luk_max=0.690f },

    // Excellent
    { .hp_min=108.276f, .hp_max=109.310f, .sp_min=2.621f, .sp_max=2.655f,
      .str_min=1.034f, .str_max=1.069f, .agi_min=0.828f, .agi_max=0.862f,
      .vit_min=0.828f, .vit_max=0.897f, .int_min=0.172f, .int_max=0.207f,
      .dex_min=0.690f, .dex_max=0.724f, .luk_min=0.690f, .luk_max=0.724f },

    // Amazing
    { .hp_min=109.310f, .sp_min=2.655f,
      .str_min=1.069f, .agi_min=0.862f, .vit_min=0.897f,
      .int_min=0.207f, .dex_min=0.724f, .luk_min=0.724f }
};

static const StatRange AmistrStatRange40[RANK_COUNT] = {
    // Abysmal
    { .hp_max=101.026f, .sp_max=2.359f, .str_max=0.795f, .agi_max=0.564f,
      .vit_max=0.564f, .int_max=0.026f, .dex_max=0.462f, .luk_max=0.462f },

    // Terrible
    { .hp_min=101.026f, .hp_max=101.795f, .sp_min=2.359f, .sp_max=2.410f,
      .str_min=0.795f, .str_max=0.821f, .agi_min=0.564f, .agi_max=0.590f,
      .vit_min=0.564f, .vit_max=0.590f, .int_min=0.026f, .int_max=0.051f,
      .dex_min=0.462f, .dex_max=0.487f, .luk_min=0.462f, .luk_max=0.487f },

    // Poor
    { .hp_min=101.795f, .hp_max=102.821f, .sp_min=2.410f, .sp_max=2.436f,
      .str_min=0.821f, .str_max=0.872f, .agi_min=0.590f, .agi_max=0.641f,
      .vit_min=0.590f, .vit_max=0.641f, .int_min=0.051f, .int_max=0.051f,
      .dex_min=0.487f, .dex_max=0.513f, .luk_min=0.487f, .luk_max=0.513f },

    // Below Average
    { .hp_min=102.821f, .hp_max=104.359f, .sp_min=2.436f, .sp_max=2.487f,
      .str_min=0.872f, .str_max=0.897f, .agi_min=0.641f, .agi_max=0.692f,
      .vit_min=0.641f, .vit_max=0.692f, .int_min=0.051f, .int_max=0.077f,
      .dex_min=0.513f, .dex_max=0.564f, .luk_min=0.513f, .luk_max=0.564f },

    // Average
    { .hp_min=104.359f, .hp_max=105.385f, .sp_min=2.487f, .sp_max=2.513f,
      .str_min=0.897f, .str_max=0.949f, .agi_min=0.692f, .agi_max=0.718f,
      .vit_min=0.692f, .vit_max=0.718f, .int_min=0.077f, .int_max=0.103f,
      .dex_min=0.564f, .dex_max=0.615f, .luk_min=0.564f, .luk_max=0.615f },

    // Good
    { .hp_min=105.385f, .hp_max=106.923f, .sp_min=2.513f, .sp_max=2.564f,
      .str_min=0.949f, .str_max=0.974f, .agi_min=0.718f, .agi_max=0.795f,
      .vit_min=0.718f, .vit_max=0.795f, .int_min=0.103f, .int_max=0.128f,
      .dex_min=0.615f, .dex_max=0.667f, .luk_min=0.615f, .luk_max=0.667f },

    // Very Good
    { .hp_min=106.923f, .hp_max=107.949f, .sp_min=2.564f, .sp_max=2.590f,
      .str_min=0.974f, .str_max=1.026f, .agi_min=0.795f, .agi_max=0.821f,
      .vit_min=0.795f, .vit_max=0.821f, .int_min=0.128f, .int_max=0.154f,
      .dex_min=0.667f, .dex_max=0.692f, .luk_min=0.667f, .luk_max=0.692f },

    // Excellent
    { .hp_min=107.949f, .hp_max=108.718f, .sp_min=2.590f, .sp_max=2.641f,
      .str_min=1.026f, .str_max=1.051f, .agi_min=0.821f, .agi_max=0.846f,
      .vit_min=0.821f, .vit_max=0.846f, .int_min=0.154f, .int_max=0.179f,
      .dex_min=0.692f, .dex_max=0.718f, .luk_min=0.692f, .luk_max=0.718f },

    // Amazing
    { .hp_min=108.718f, .sp_min=2.641f,
      .str_min=1.051f, .agi_min=0.846f, .vit_min=0.846f,
      .int_min=0.179f, .dex_min=0.718f, .luk_min=0.718f }
};

static const StatRange AmistrStatRange50[RANK_COUNT] = {
    // Abysmal
    { .hp_max=101.429f, .sp_max=2.367f, .str_max=0.816f, .agi_max=0.571f,
      .vit_max=0.571f, .int_max=0.041f, .dex_max=0.469f, .luk_max=0.469f },

    // Terrible
    { .hp_min=101.429f, .hp_max=102.245f, .sp_min=2.367f, .sp_max=2.408f,
      .str_min=0.816f, .str_max=0.837f, .agi_min=0.571f, .agi_max=0.592f,
      .vit_min=0.571f, .vit_max=0.592f, .int_min=0.041f, .int_max=0.041f,
      .dex_min=0.469f, .dex_max=0.490f, .luk_min=0.469f, .luk_max=0.490f },

    // Poor
    { .hp_min=102.245f, .hp_max=103.061f, .sp_min=2.408f, .sp_max=2.449f,
      .str_min=0.837f, .str_max=0.857f, .agi_min=0.592f, .agi_max=0.633f,
      .vit_min=0.592f, .vit_max=0.633f, .int_min=0.041f, .int_max=0.061f,
      .dex_min=0.490f, .dex_max=0.531f, .luk_min=0.490f, .luk_max=0.531f },

    // Below Average
    { .hp_min=103.061f, .hp_max=104.286f, .sp_min=2.449f, .sp_max=2.490f,
      .str_min=0.857f, .str_max=0.898f, .agi_min=0.633f, .agi_max=0.694f,
      .vit_min=0.633f, .vit_max=0.694f, .int_min=0.061f, .int_max=0.082f,
      .dex_min=0.531f, .dex_max=0.571f, .luk_min=0.531f, .luk_max=0.571f },

    // Average
    { .hp_min=104.286f, .hp_max=105.510f, .sp_min=2.490f, .sp_max=2.510f,
      .str_min=0.898f, .str_max=0.939f, .agi_min=0.694f, .agi_max=0.735f,
      .vit_min=0.694f, .vit_max=0.735f, .int_min=0.082f, .int_max=0.102f,
      .dex_min=0.571f, .dex_max=0.612f, .luk_min=0.571f, .luk_max=0.612f },

    // Good
    { .hp_min=105.510f, .hp_max=106.735f, .sp_min=2.510f, .sp_max=2.551f,
      .str_min=0.939f, .str_max=0.980f, .agi_min=0.735f, .agi_max=0.776f,
      .vit_min=0.735f, .vit_max=0.776f, .int_min=0.102f, .int_max=0.143f,
      .dex_min=0.612f, .dex_max=0.653f, .luk_min=0.612f, .luk_max=0.653f },

    // Very Good
    { .hp_min=106.735f, .hp_max=107.551f, .sp_min=2.551f, .sp_max=2.592f,
      .str_min=0.980f, .str_max=1.000f, .agi_min=0.776f, .agi_max=0.816f,
      .vit_min=0.776f, .vit_max=0.816f, .int_min=0.143f, .int_max=0.163f,
      .dex_min=0.653f, .dex_max=0.673f, .luk_min=0.653f, .luk_max=0.673f },

    // Excellent
    { .hp_min=107.551f, .hp_max=108.367f, .sp_min=2.592f, .sp_max=2.633f,
      .str_min=1.000f, .str_max=1.041f, .agi_min=0.816f, .agi_max=0.837f,
      .vit_min=0.816f, .vit_max=0.837f, .int_min=0.163f, .int_max=0.163f,
      .dex_min=0.673f, .dex_max=0.694f, .luk_min=0.673f, .luk_max=0.694f },

    // Amazing
    { .hp_min=108.367f, .sp_min=2.633f,
      .str_min=1.041f, .agi_min=0.837f, .vit_min=0.837f,
      .int_min=0.163f, .dex_min=0.694f, .luk_min=0.694f }
};

static const StatRange AmistrStatRange60[RANK_COUNT] = {
    // Abysmal
    { .hp_max=101.695f, .sp_max=2.407f, .str_max=0.814f, .agi_max=0.576f,
      .vit_max=0.593f, .int_max=0.034f, .dex_max=0.475f, .luk_max=0.475f },

    // Terrible
    { .hp_min=101.695f, .hp_max=102.373f, .sp_min=2.407f, .sp_max=2.424f,
      .str_min=0.814f, .str_max=0.847f, .agi_min=0.576f, .agi_max=0.610f,
      .vit_min=0.593f, .vit_max=0.610f, .int_min=0.034f, .int_max=0.051f,
      .dex_min=0.475f, .dex_max=0.508f, .luk_min=0.475f, .luk_max=0.508f },

    // Poor
    { .hp_min=102.373f, .hp_max=103.220f, .sp_min=2.424f, .sp_max=2.458f,
      .str_min=0.847f, .str_max=0.864f, .agi_min=0.610f, .agi_max=0.644f,
      .vit_min=0.610f, .vit_max=0.644f, .int_min=0.051f, .int_max=0.068f,
      .dex_min=0.508f, .dex_max=0.542f, .luk_min=0.508f, .luk_max=0.542f },

    // Below Average
    { .hp_min=103.220f, .hp_max=104.407f, .sp_min=2.458f, .sp_max=2.492f,
      .str_min=0.864f, .str_max=0.915f, .agi_min=0.644f, .agi_max=0.695f,
      .vit_min=0.644f, .vit_max=0.695f, .int_min=0.068f, .int_max=0.085f,
      .dex_min=0.542f, .dex_max=0.576f, .luk_min=0.542f, .luk_max=0.576f },

    // Average
    { .hp_min=104.407f, .hp_max=105.424f, .sp_min=2.492f, .sp_max=2.508f,
      .str_min=0.915f, .str_max=0.932f, .agi_min=0.695f, .agi_max=0.729f,
      .vit_min=0.695f, .vit_max=0.729f, .int_min=0.085f, .int_max=0.102f,
      .dex_min=0.576f, .dex_max=0.610f, .luk_min=0.576f, .luk_max=0.610f },

    // Good
    { .hp_min=105.424f, .hp_max=106.610f, .sp_min=2.508f, .sp_max=2.542f,
      .str_min=0.932f, .str_max=0.983f, .agi_min=0.729f, .agi_max=0.763f,
      .vit_min=0.729f, .vit_max=0.763f, .int_min=0.102f, .int_max=0.136f,
      .dex_min=0.610f, .dex_max=0.644f, .luk_min=0.610f, .luk_max=0.644f },

    // Very Good
    { .hp_min=106.610f, .hp_max=107.458f, .sp_min=2.542f, .sp_max=2.576f,
      .str_min=0.983f, .str_max=1.000f, .agi_min=0.763f, .agi_max=0.797f,
      .vit_min=0.763f, .vit_max=0.797f, .int_min=0.136f, .int_max=0.153f,
      .dex_min=0.644f, .dex_max=0.678f, .luk_min=0.644f, .luk_max=0.678f },

    // Excellent
    { .hp_min=107.458f, .hp_max=108.136f, .sp_min=2.576f, .sp_max=2.593f,
      .str_min=1.000f, .str_max=1.017f, .agi_min=0.797f, .agi_max=0.831f,
      .vit_min=0.797f, .vit_max=0.831f, .int_min=0.153f, .int_max=0.169f,
      .dex_min=0.678f, .dex_max=0.695f, .luk_min=0.678f, .luk_max=0.695f },

    // Amazing
    { .hp_min=108.136f, .sp_min=2.593f,
      .str_min=1.017f, .agi_min=0.831f, .vit_min=0.831f,
      .int_min=0.169f, .dex_min=0.695f, .luk_min=0.695f }
};

static const StatRange AmistrStatRange70[RANK_COUNT] = {
    // Abysmal
    { .hp_max=102.029f, .sp_max=2.420f, .str_max=0.826f, .agi_max=0.594f,
      .vit_max=0.594f, .int_max=0.043f, .dex_max=0.493f, .luk_max=0.493f },

    // Terrible
    { .hp_min=102.029f, .hp_max=102.609f, .sp_min=2.420f, .sp_max=2.435f,
      .str_min=0.826f, .str_max=0.855f, .agi_min=0.594f, .agi_max=0.623f,
      .vit_min=0.594f, .vit_max=0.623f, .int_min=0.043f, .int_max=0.058f,
      .dex_min=0.493f, .dex_max=0.507f, .luk_min=0.493f, .luk_max=0.507f },

    // Poor
    { .hp_min=102.609f, .hp_max=103.478f, .sp_min=2.435f, .sp_max=2.464f,
      .str_min=0.855f, .str_max=0.870f, .agi_min=0.623f, .agi_max=0.652f,
      .vit_min=0.623f, .vit_max=0.652f, .int_min=0.058f, .int_max=0.072f,
      .dex_min=0.507f, .dex_max=0.536f, .luk_min=0.507f, .luk_max=0.536f },

    // Below Average
    { .hp_min=103.478f, .hp_max=104.493f, .sp_min=2.464f, .sp_max=2.493f,
      .str_min=0.870f, .str_max=0.913f, .agi_min=0.652f, .agi_max=0.681f,
      .vit_min=0.652f, .vit_max=0.681f, .int_min=0.072f, .int_max=0.087f,
      .dex_min=0.536f, .dex_max=0.580f, .luk_min=0.536f, .luk_max=0.580f },

    // Average
    { .hp_min=104.493f, .hp_max=105.362f, .sp_min=2.493f, .sp_max=2.507f,
      .str_min=0.913f, .str_max=0.942f, .agi_min=0.681f, .agi_max=0.725f,
      .vit_min=0.681f, .vit_max=0.725f, .int_min=0.087f, .int_max=0.101f,
      .dex_min=0.580f, .dex_max=0.609f, .luk_min=0.580f, .luk_max=0.609f },

    // Good
    { .hp_min=105.362f, .hp_max=106.377f, .sp_min=2.507f, .sp_max=2.536f,
      .str_min=0.942f, .str_max=0.971f, .agi_min=0.725f, .agi_max=0.768f,
      .vit_min=0.725f, .vit_max=0.768f, .int_min=0.101f, .int_max=0.130f,
      .dex_min=0.609f, .dex_max=0.638f, .luk_min=0.609f, .luk_max=0.638f },

    // Very Good
    { .hp_min=106.377f, .hp_max=107.246f, .sp_min=2.536f, .sp_max=2.565f,
      .str_min=0.971f, .str_max=1.000f, .agi_min=0.768f, .agi_max=0.797f,
      .vit_min=0.768f, .vit_max=0.797f, .int_min=0.130f, .int_max=0.145f,
      .dex_min=0.638f, .dex_max=0.667f, .luk_min=0.638f, .luk_max=0.667f },

    // Excellent
    { .hp_min=107.246f, .hp_max=107.826f, .sp_min=2.565f, .sp_max=2.580f,
      .str_min=1.000f, .str_max=1.014f, .agi_min=0.797f, .agi_max=0.826f,
      .vit_min=0.797f, .vit_max=0.826f, .int_min=0.145f, .int_max=0.159f,
      .dex_min=0.667f, .dex_max=0.681f, .luk_min=0.667f, .luk_max=0.681f },

    // Amazing
    { .hp_min=107.826f, .sp_min=2.580f,
      .str_min=1.014f, .agi_min=0.826f, .vit_min=0.826f,
      .int_min=0.159f, .dex_min=0.681f, .luk_min=0.681f }
};

static const StatRange AmistrStatRange80[RANK_COUNT] = {
    // Abysmal
    { .hp_max=102.152f, .sp_max=2.405f, .str_max=0.835f, .agi_max=0.595f,
      .vit_max=0.608f, .int_max=0.051f, .dex_max=0.494f, .luk_max=0.494f },

    // Terrible
    { .hp_min=102.152f, .hp_max=102.785f, .sp_min=2.405f, .sp_max=2.443f,
      .str_min=0.835f, .str_max=0.861f, .agi_min=0.595f, .agi_max=0.620f,
      .vit_min=0.608f, .vit_max=0.620f, .int_min=0.051f, .int_max=0.063f,
      .dex_min=0.494f, .dex_max=0.519f, .luk_min=0.494f, .luk_max=0.519f },

    // Poor
    { .hp_min=102.785f, .hp_max=103.544f, .sp_min=2.443f, .sp_max=2.468f,
      .str_min=0.861f, .str_max=0.873f, .agi_min=0.620f, .agi_max=0.646f,
      .vit_min=0.620f, .vit_max=0.646f, .int_min=0.063f, .int_max=0.076f,
      .dex_min=0.519f, .dex_max=0.544f, .luk_min=0.519f, .luk_max=0.544f },

    // Below Average
    { .hp_min=103.544f, .hp_max=104.557f, .sp_min=2.468f, .sp_max=2.494f,
      .str_min=0.873f, .str_max=0.911f, .agi_min=0.646f, .agi_max=0.684f,
      .vit_min=0.646f, .vit_max=0.684f, .int_min=0.076f, .int_max=0.089f,
      .dex_min=0.544f, .dex_max=0.570f, .luk_min=0.544f, .luk_max=0.570f },

    // Average
    { .hp_min=104.557f, .hp_max=105.316f, .sp_min=2.494f, .sp_max=2.506f,
      .str_min=0.911f, .str_max=0.937f, .agi_min=0.684f, .agi_max=0.722f,
      .vit_min=0.684f, .vit_max=0.722f, .int_min=0.089f, .int_max=0.101f,
      .dex_min=0.570f, .dex_max=0.608f, .luk_min=0.570f, .luk_max=0.608f },

    // Good
    { .hp_min=105.316f, .hp_max=106.329f, .sp_min=2.506f, .sp_max=2.532f,
      .str_min=0.937f, .str_max=0.962f, .agi_min=0.722f, .agi_max=0.759f,
      .vit_min=0.722f, .vit_max=0.759f, .int_min=0.101f, .int_max=0.127f,
      .dex_min=0.608f, .dex_max=0.633f, .luk_min=0.608f, .luk_max=0.633f },

    // Very Good
    { .hp_min=106.329f, .hp_max=107.089f, .sp_min=2.532f, .sp_max=2.557f,
      .str_min=0.962f, .str_max=0.987f, .agi_min=0.759f, .agi_max=0.785f,
      .vit_min=0.759f, .vit_max=0.785f, .int_min=0.127f, .int_max=0.139f,
      .dex_min=0.633f, .dex_max=0.658f, .luk_min=0.633f, .luk_max=0.658f },

    // Excellent
    { .hp_min=107.089f, .hp_max=107.595f, .sp_min=2.557f, .sp_max=2.595f,
      .str_min=0.987f, .str_max=1.013f, .agi_min=0.785f, .agi_max=0.810f,
      .vit_min=0.785f, .vit_max=0.810f, .int_min=0.139f, .int_max=0.152f,
      .dex_min=0.658f, .dex_max=0.684f, .luk_min=0.658f, .luk_max=0.684f },

    // Amazing
    { .hp_min=107.595f, .sp_min=2.595f,
      .str_min=1.013f, .agi_min=0.810f, .vit_min=0.810f,
      .int_min=0.152f, .dex_min=0.684f, .luk_min=0.684f }
};

static const StatRange AmistrStatRange90[RANK_COUNT] = {
    // Abysmal
    { .hp_max=102.360f, .sp_max=2.427f, .str_max=0.843f, .agi_max=0.607f,
      .vit_max=0.607f, .int_max=0.056f, .dex_max=0.506f, .luk_max=0.506f },

    // Terrible
    { .hp_min=102.360f, .hp_max=102.921f, .sp_min=2.427f, .sp_max=2.449f,
      .str_min=0.843f, .str_max=0.854f, .agi_min=0.607f, .agi_max=0.629f,
      .vit_min=0.607f, .vit_max=0.629f, .int_min=0.056f, .int_max=0.067f,
      .dex_min=0.506f, .dex_max=0.517f, .luk_min=0.506f, .luk_max=0.517f },

    // Poor
    { .hp_min=102.921f, .hp_max=103.596f, .sp_min=2.449f, .sp_max=2.472f,
      .str_min=0.854f, .str_max=0.876f, .agi_min=0.629f, .agi_max=0.652f,
      .vit_min=0.629f, .vit_max=0.652f, .int_min=0.067f, .int_max=0.079f,
      .dex_min=0.517f, .dex_max=0.539f, .luk_min=0.517f, .luk_max=0.539f },

    // Below Average
    { .hp_min=103.596f, .hp_max=104.494f, .sp_min=2.472f, .sp_max=2.494f,
      .str_min=0.876f, .str_max=0.910f, .agi_min=0.652f, .agi_max=0.685f,
      .vit_min=0.652f, .vit_max=0.685f, .int_min=0.079f, .int_max=0.090f,
      .dex_min=0.539f, .dex_max=0.573f, .luk_min=0.539f, .luk_max=0.573f },

    // Average
    { .hp_min=104.494f, .hp_max=105.393f, .sp_min=2.494f, .sp_max=2.506f,
      .str_min=0.910f, .str_max=0.933f, .agi_min=0.685f, .agi_max=0.719f,
      .vit_min=0.685f, .vit_max=0.719f, .int_min=0.090f, .int_max=0.101f,
      .dex_min=0.573f, .dex_max=0.607f, .luk_min=0.573f, .luk_max=0.607f },

    // Good
    { .hp_min=105.393f, .hp_max=106.292f, .sp_min=2.506f, .sp_max=2.528f,
      .str_min=0.933f, .str_max=0.966f, .agi_min=0.719f, .agi_max=0.753f,
      .vit_min=0.719f, .vit_max=0.753f, .int_min=0.101f, .int_max=0.124f,
      .dex_min=0.607f, .dex_max=0.629f, .luk_min=0.607f, .luk_max=0.629f },

    // Very Good
    { .hp_min=106.292f, .hp_max=106.966f, .sp_min=2.528f, .sp_max=2.551f,
      .str_min=0.966f, .str_max=0.989f, .agi_min=0.753f, .agi_max=0.787f,
      .vit_min=0.753f, .vit_max=0.787f, .int_min=0.124f, .int_max=0.135f,
      .dex_min=0.629f, .dex_max=0.652f, .luk_min=0.629f, .luk_max=0.652f },

    // Excellent
    { .hp_min=106.966f, .hp_max=107.528f, .sp_min=2.551f, .sp_max=2.573f,
      .str_min=0.989f, .str_max=1.000f, .agi_min=0.787f, .agi_max=0.809f,
      .vit_min=0.787f, .vit_max=0.809f, .int_min=0.135f, .int_max=0.157f,
      .dex_min=0.652f, .dex_max=0.674f, .luk_min=0.652f, .luk_max=0.674f },

    // Amazing
    { .hp_min=107.528f, .sp_min=2.573f,
      .str_min=1.000f, .agi_min=0.809f, .vit_min=0.809f,
      .int_min=0.157f, .dex_min=0.674f, .luk_min=0.674f }
};

static const StatRange AmistrStatRange99[RANK_COUNT] = {
    // Abysmal
    { .hp_max=102.449f, .sp_max=2.439f, .str_max=0.847f, .agi_max=0.612f,
      .vit_max=0.612f, .int_max=0.051f, .dex_max=0.510f, .luk_max=0.510f },

    // Terrible
    { .hp_min=102.449f, .hp_max=103.061f, .sp_min=2.439f, .sp_max=2.449f,
      .str_min=0.847f, .str_max=0.857f, .agi_min=0.612f, .agi_max=0.633f,
      .vit_min=0.612f, .vit_max=0.633f, .int_min=0.051f, .int_max=0.061f,
      .dex_min=0.510f, .dex_max=0.520f, .luk_min=0.510f, .luk_max=0.520f },

    // Poor
    { .hp_min=103.061f, .hp_max=103.673f, .sp_min=2.449f, .sp_max=2.469f,
      .str_min=0.857f, .str_max=0.888f, .agi_min=0.633f, .agi_max=0.653f,
      .vit_min=0.633f, .vit_max=0.653f, .int_min=0.061f, .int_max=0.071f,
      .dex_min=0.520f, .dex_max=0.551f, .luk_min=0.520f, .luk_max=0.551f },

    // Below Average
    { .hp_min=103.673f, .hp_max=104.592f, .sp_min=2.469f, .sp_max=2.490f,
      .str_min=0.888f, .str_max=0.908f, .agi_min=0.653f, .agi_max=0.694f,
      .vit_min=0.653f, .vit_max=0.694f, .int_min=0.071f, .int_max=0.092f,
      .dex_min=0.551f, .dex_max=0.571f, .luk_min=0.551f, .luk_max=0.571f },

    // Average
    { .hp_min=104.592f, .hp_max=105.306f, .sp_min=2.490f, .sp_max=2.510f,
      .str_min=0.908f, .str_max=0.939f, .agi_min=0.694f, .agi_max=0.724f,
      .vit_min=0.694f, .vit_max=0.724f, .int_min=0.092f, .int_max=0.102f,
      .dex_min=0.571f, .dex_max=0.602f, .luk_min=0.571f, .luk_max=0.602f },

    // Good
    { .hp_min=105.306f, .hp_max=106.224f, .sp_min=2.510f, .sp_max=2.531f,
      .str_min=0.939f, .str_max=0.959f, .agi_min=0.724f, .agi_max=0.755f,
      .vit_min=0.724f, .vit_max=0.755f, .int_min=0.102f, .int_max=0.122f,
      .dex_min=0.602f, .dex_max=0.633f, .luk_min=0.602f, .luk_max=0.633f },

    // Very Good
    { .hp_min=106.224f, .hp_max=106.837f, .sp_min=2.531f, .sp_max=2.551f,
      .str_min=0.959f, .str_max=0.980f, .agi_min=0.755f, .agi_max=0.776f,
      .vit_min=0.755f, .vit_max=0.776f, .int_min=0.122f, .int_max=0.143f,
      .dex_min=0.633f, .dex_max=0.653f, .luk_min=0.633f, .luk_max=0.653f },

    // Excellent
    { .hp_min=106.837f, .hp_max=107.347f, .sp_min=2.551f, .sp_max=2.561f,
      .str_min=0.980f, .str_max=1.000f, .agi_min=0.776f, .agi_max=0.796f,
      .vit_min=0.776f, .vit_max=0.806f, .int_min=0.143f, .int_max=0.153f,
      .dex_min=0.653f, .dex_max=0.673f, .luk_min=0.653f, .luk_max=0.673f },

    // Amazing
    { .hp_min=107.347f, .sp_min=2.561f,
      .str_min=1.000f, .agi_min=0.796f, .vit_min=0.806f,
      .int_min=0.153f, .dex_min=0.673f, .luk_min=0.673f }
};

static const StatRange *amistr_stat_ranges[] = {
    AmistrStatRange10,
    AmistrStatRange20,
    AmistrStatRange30,
    AmistrStatRange40,
    AmistrStatRange50,
    AmistrStatRange60,
    AmistrStatRange70,
    AmistrStatRange80,
    AmistrStatRange90,
    AmistrStatRange99
};

// ================== End of Amistr ====================

// ================== LIF GROWTH SHEET =====================

static const StatRange LifStatRange10[RANK_COUNT] = {
    // Abysmal
    { .hp_max=73.333f, .sp_max=5.556f, .str_max=0.444f, .agi_max=0.444f,
      .vit_max=0.444f, .int_max=0.444f, .dex_max=0.556f, .luk_max=0.556f },

    // Terrible
    { .hp_min=73.333f, .hp_max=74.444f, .sp_min=5.556f, .sp_max=5.778f,
      .str_min=0.444f, .str_max=0.444f, .agi_min=0.444f, .agi_max=0.444f,
      .vit_min=0.444f, .vit_max=0.444f, .int_min=0.444f, .int_max=0.444f,
      .dex_min=0.556f, .dex_max=0.556f, .luk_min=0.556f, .luk_max=0.556f },

    // Poor
    { .hp_min=74.444f, .hp_max=76.667f, .sp_min=5.778f, .sp_max=6.000f,
      .str_min=0.444f, .str_max=0.556f, .agi_min=0.444f, .agi_max=0.556f,
      .vit_min=0.444f, .vit_max=0.556f, .int_min=0.444f, .int_max=0.556f,
      .dex_min=0.556f, .dex_max=0.667f, .luk_min=0.556f, .luk_max=0.667f },

    // Below Average
    { .hp_min=76.667f, .hp_max=78.889f, .sp_min=6.000f, .sp_max=6.333f,
      .str_min=0.556f, .str_max=0.667f, .agi_min=0.556f, .agi_max=0.667f,
      .vit_min=0.556f, .vit_max=0.667f, .int_min=0.556f, .int_max=0.667f,
      .dex_min=0.667f, .dex_max=0.778f, .luk_min=0.667f, .luk_max=0.778f },

    // Average
    { .hp_min=78.889f, .hp_max=80.000f, .sp_min=6.333f, .sp_max=6.667f,
      .str_min=0.667f, .str_max=0.667f, .agi_min=0.667f, .agi_max=0.667f,
      .vit_min=0.667f, .vit_max=0.667f, .int_min=0.667f, .int_max=0.778f,
      .dex_min=0.778f, .dex_max=0.889f, .luk_min=0.778f, .luk_max=0.889f },

    // Good
    { .hp_min=80.000f, .hp_max=83.333f, .sp_min=6.667f, .sp_max=7.000f,
      .str_min=0.667f, .str_max=0.778f, .agi_min=0.667f, .agi_max=0.778f,
      .vit_min=0.667f, .vit_max=0.778f, .int_min=0.778f, .int_max=0.889f,
      .dex_min=0.889f, .dex_max=1.000f, .luk_min=0.889f, .luk_max=1.000f },

    // Very Good
    { .hp_min=83.333f, .hp_max=84.444f, .sp_min=7.000f, .sp_max=7.222f,
      .str_min=0.778f, .str_max=0.889f, .agi_min=0.778f, .agi_max=0.889f,
      .vit_min=0.778f, .vit_max=0.889f, .int_min=0.889f, .int_max=1.000f,
      .dex_min=1.000f, .dex_max=1.000f, .luk_min=1.000f, .luk_max=1.000f },

    // Excellent
    { .hp_min=84.444f, .hp_max=85.556f, .sp_min=7.222f, .sp_max=7.444f,
      .str_min=0.889f, .str_max=0.889f, .agi_min=0.889f, .agi_max=0.889f,
      .vit_min=0.889f, .vit_max=0.889f, .int_min=1.000f, .int_max=1.000f,
      .dex_min=1.000f, .dex_max=1.111f, .luk_min=1.000f, .luk_max=1.111f },

    // Amazing
    { .hp_min=85.556f, .sp_min=7.444f,
      .str_min=0.889f, .agi_min=0.889f, .vit_min=0.889f,
      .int_min=1.000f, .dex_min=1.111f, .luk_min=1.111f }
};

static const StatRange LifStatRange20[RANK_COUNT] = {
    // Abysmal
    { .hp_max=75.263f, .sp_max=5.842f, .str_max=0.474f, .agi_max=0.474f,
      .vit_max=0.474f, .int_max=0.474f, .dex_max=0.579f, .luk_max=0.579f },

    // Terrible
    { .hp_min=75.263f, .hp_max=76.316f, .sp_min=5.842f, .sp_max=6.000f,
      .str_min=0.474f, .str_max=0.526f, .agi_min=0.474f, .agi_max=0.526f,
      .vit_min=0.474f, .vit_max=0.526f, .int_min=0.474f, .int_max=0.526f,
      .dex_min=0.579f, .dex_max=0.632f, .luk_min=0.579f, .luk_max=0.632f },

    // Poor
    { .hp_min=76.316f, .hp_max=77.368f, .sp_min=6.000f, .sp_max=6.158f,
      .str_min=0.526f, .str_max=0.579f, .agi_min=0.526f, .agi_max=0.579f,
      .vit_min=0.526f, .vit_max=0.579f, .int_min=0.526f, .int_max=0.579f,
      .dex_min=0.632f, .dex_max=0.684f, .luk_min=0.632f, .luk_max=0.684f },

    // Below Average
    { .hp_min=77.368f, .hp_max=78.947f, .sp_min=6.158f, .sp_max=6.421f,
      .str_min=0.579f, .str_max=0.632f, .agi_min=0.579f, .agi_max=0.632f,
      .vit_min=0.579f, .vit_max=0.632f, .int_min=0.579f, .int_max=0.684f,
      .dex_min=0.684f, .dex_max=0.789f, .luk_min=0.684f, .luk_max=0.789f },

    // Average
    { .hp_min=78.947f, .hp_max=80.526f, .sp_min=6.421f, .sp_max=6.579f,
      .str_min=0.632f, .str_max=0.684f, .agi_min=0.632f, .agi_max=0.684f,
      .vit_min=0.632f, .vit_max=0.684f, .int_min=0.684f, .int_max=0.737f,
      .dex_min=0.789f, .dex_max=0.842f, .luk_min=0.789f, .luk_max=0.842f },

    // Good
    { .hp_min=80.526f, .hp_max=82.105f, .sp_min=6.579f, .sp_max=6.842f,
      .str_min=0.684f, .str_max=0.737f, .agi_min=0.684f, .agi_max=0.737f,
      .vit_min=0.684f, .vit_max=0.737f, .int_min=0.737f, .int_max=0.789f,
      .dex_min=0.842f, .dex_max=0.895f, .luk_min=0.842f, .luk_max=0.895f },

    // Very Good
    { .hp_min=82.105f, .hp_max=83.158f, .sp_min=6.842f, .sp_max=7.000f,
      .str_min=0.737f, .str_max=0.789f, .agi_min=0.737f, .agi_max=0.789f,
      .vit_min=0.737f, .vit_max=0.789f, .int_min=0.789f, .int_max=0.895f,
      .dex_min=0.895f, .dex_max=0.947f, .luk_min=0.895f, .luk_max=0.947f },

    // Excellent
    { .hp_min=83.158f, .hp_max=84.211f, .sp_min=7.000f, .sp_max=7.158f,
      .str_min=0.789f, .str_max=0.842f, .agi_min=0.789f, .agi_max=0.842f,
      .vit_min=0.789f, .vit_max=0.842f, .int_min=0.895f, .int_max=0.895f,
      .dex_min=0.947f, .dex_max=1.000f, .luk_min=0.947f, .luk_max=1.000f },

    // Amazing
    { .hp_min=84.211f, .sp_min=7.158f,
      .str_min=0.842f, .agi_min=0.842f, .vit_min=0.842f,
      .int_min=0.895f, .dex_min=1.000f, .luk_min=1.000f }
};

static const StatRange LifStatRange30[RANK_COUNT] = {
    // Abysmal
    { .hp_max=76.207f, .sp_max=6.000f, .str_max=0.517f, .agi_max=0.517f,
      .vit_max=0.517f, .int_max=0.517f, .dex_max=0.621f, .luk_max=0.621f },

    // Terrible
    { .hp_min=76.207f, .hp_max=76.897f, .sp_min=6.000f, .sp_max=6.103f,
      .str_min=0.517f, .str_max=0.552f, .agi_min=0.517f, .agi_max=0.552f,
      .vit_min=0.517f, .vit_max=0.552f, .int_min=0.517f, .int_max=0.586f,
      .dex_min=0.621f, .dex_max=0.655f, .luk_min=0.621f, .luk_max=0.655f },

    // Poor
    { .hp_min=76.897f, .hp_max=77.931f, .sp_min=6.103f, .sp_max=6.241f,
      .str_min=0.552f, .str_max=0.586f, .agi_min=0.552f, .agi_max=0.586f,
      .vit_min=0.552f, .vit_max=0.586f, .int_min=0.586f, .int_max=0.621f,
      .dex_min=0.655f, .dex_max=0.724f, .luk_min=0.655f, .luk_max=0.724f },

    // Below Average
    { .hp_min=77.931f, .hp_max=79.310f, .sp_min=6.241f, .sp_max=6.414f,
      .str_min=0.586f, .str_max=0.655f, .agi_min=0.586f, .agi_max=0.655f,
      .vit_min=0.586f, .vit_max=0.655f, .int_min=0.621f, .int_max=0.690f,
      .dex_min=0.724f, .dex_max=0.759f, .luk_min=0.724f, .luk_max=0.759f },

    // Average
    { .hp_min=79.310f, .hp_max=80.345f, .sp_min=6.414f, .sp_max=6.586f,
      .str_min=0.655f, .str_max=0.690f, .agi_min=0.655f, .agi_max=0.690f,
      .vit_min=0.655f, .vit_max=0.690f, .int_min=0.690f, .int_max=0.724f,
      .dex_min=0.759f, .dex_max=0.828f, .luk_min=0.759f, .luk_max=0.828f },

    // Good
    { .hp_min=80.345f, .hp_max=81.724f, .sp_min=6.586f, .sp_max=6.759f,
      .str_min=0.690f, .str_max=0.724f, .agi_min=0.690f, .agi_max=0.724f,
      .vit_min=0.690f, .vit_max=0.724f, .int_min=0.724f, .int_max=0.793f,
      .dex_min=0.828f, .dex_max=0.897f, .luk_min=0.828f, .luk_max=0.897f },

    // Very Good
    { .hp_min=81.724f, .hp_max=82.759f, .sp_min=6.759f, .sp_max=6.897f,
      .str_min=0.724f, .str_max=0.793f, .agi_min=0.724f, .agi_max=0.793f,
      .vit_min=0.724f, .vit_max=0.793f, .int_min=0.793f, .int_max=0.828f,
      .dex_min=0.897f, .dex_max=0.931f, .luk_min=0.897f, .luk_max=0.931f },

    // Excellent
    { .hp_min=82.759f, .hp_max=83.448f, .sp_min=6.897f, .sp_max=7.000f,
      .str_min=0.793f, .str_max=0.793f, .agi_min=0.793f, .agi_max=0.793f,
      .vit_min=0.793f, .vit_max=0.793f, .int_min=0.828f, .int_max=0.862f,
      .dex_min=0.931f, .dex_max=0.966f, .luk_min=0.931f, .luk_max=0.966f },

    // Amazing
    { .hp_min=83.448f, .sp_min=7.000f,
      .str_min=0.793f, .agi_min=0.793f, .vit_min=0.793f,
      .int_min=0.862f, .dex_min=0.966f, .luk_min=0.966f }
};

static const StatRange LifStatRange40[RANK_COUNT] = {
    // Abysmal
    { .hp_max=76.667f, .sp_max=6.051f, .str_max=0.538f, .agi_max=0.538f,
      .vit_max=0.538f, .int_max=0.564f, .dex_max=0.667f, .luk_max=0.667f },

    // Terrible
    { .hp_min=76.667f, .hp_max=77.436f, .sp_min=6.051f, .sp_max=6.154f,
      .str_min=0.538f, .str_max=0.564f, .agi_min=0.538f, .agi_max=0.564f,
      .vit_min=0.538f, .vit_max=0.564f, .int_min=0.564f, .int_max=0.590f,
      .dex_min=0.667f, .dex_max=0.692f, .luk_min=0.667f, .luk_max=0.692f },

    // Poor
    { .hp_min=77.436f, .hp_max=78.205f, .sp_min=6.154f, .sp_max=6.282f,
      .str_min=0.564f, .str_max=0.615f, .agi_min=0.564f, .agi_max=0.615f,
      .vit_min=0.564f, .vit_max=0.615f, .int_min=0.590f, .int_max=0.641f,
      .dex_min=0.692f, .dex_max=0.718f, .luk_min=0.692f, .luk_max=0.718f },

    // Below Average
    { .hp_min=78.205f, .hp_max=79.487f, .sp_min=6.282f, .sp_max=6.436f,
      .str_min=0.615f, .str_max=0.641f, .agi_min=0.615f, .agi_max=0.641f,
      .vit_min=0.615f, .vit_max=0.641f, .int_min=0.641f, .int_max=0.692f,
      .dex_min=0.718f, .dex_max=0.769f, .luk_min=0.718f, .luk_max=0.769f },

    // Average
    { .hp_min=79.487f, .hp_max=80.256f, .sp_min=6.436f, .sp_max=6.564f,
      .str_min=0.641f, .str_max=0.692f, .agi_min=0.641f, .agi_max=0.692f,
      .vit_min=0.641f, .vit_max=0.692f, .int_min=0.692f, .int_max=0.718f,
      .dex_min=0.769f, .dex_max=0.821f, .luk_min=0.769f, .luk_max=0.821f },

    // Good
    { .hp_min=80.256f, .hp_max=81.538f, .sp_min=6.564f, .sp_max=6.718f,
      .str_min=0.692f, .str_max=0.718f, .agi_min=0.692f, .agi_max=0.718f,
      .vit_min=0.692f, .vit_max=0.718f, .int_min=0.718f, .int_max=0.795f,
      .dex_min=0.821f, .dex_max=0.872f, .luk_min=0.821f, .luk_max=0.872f },

    // Very Good
    { .hp_min=81.538f, .hp_max=82.308f, .sp_min=6.718f, .sp_max=6.846f,
      .str_min=0.718f, .str_max=0.769f, .agi_min=0.718f, .agi_max=0.769f,
      .vit_min=0.718f, .vit_max=0.769f, .int_min=0.795f, .int_max=0.821f,
      .dex_min=0.872f, .dex_max=0.923f, .luk_min=0.872f, .luk_max=0.897f },

    // Excellent
    { .hp_min=82.308f, .hp_max=83.077f, .sp_min=6.846f, .sp_max=6.949f,
      .str_min=0.769f, .str_max=0.795f, .agi_min=0.769f, .agi_max=0.795f,
      .vit_min=0.769f, .vit_max=0.795f, .int_min=0.821f, .int_max=0.846f,
      .dex_min=0.923f, .dex_max=0.949f, .luk_min=0.897f, .luk_max=0.949f },

    // Amazing
    { .hp_min=83.077f, .sp_min=6.949f,
      .str_min=0.795f, .agi_min=0.795f, .vit_min=0.795f,
      .int_min=0.846f, .dex_min=0.949f, .luk_min=0.949f }
};

static const StatRange LifStatRange50[RANK_COUNT] = {
    // Abysmal
    { .hp_max=77.143f, .sp_max=6.102f, .str_max=0.551f, .agi_max=0.551f,
      .vit_max=0.551f, .int_max=0.571f, .dex_max=0.673f, .luk_max=0.673f },

    // Terrible
    { .hp_min=77.143f, .hp_max=77.755f, .sp_min=6.102f, .sp_max=6.204f,
      .str_min=0.551f, .str_max=0.571f, .agi_min=0.551f, .agi_max=0.571f,
      .vit_min=0.551f, .vit_max=0.571f, .int_min=0.571f, .int_max=0.592f,
      .dex_min=0.673f, .dex_max=0.694f, .luk_min=0.673f, .luk_max=0.694f },

    // Poor
    { .hp_min=77.755f, .hp_max=78.571f, .sp_min=6.204f, .sp_max=6.306f,
      .str_min=0.571f, .str_max=0.612f, .agi_min=0.571f, .agi_max=0.612f,
      .vit_min=0.571f, .vit_max=0.612f, .int_min=0.592f, .int_max=0.633f,
      .dex_min=0.694f, .dex_max=0.735f, .luk_min=0.694f, .luk_max=0.735f },

    // Below Average
    { .hp_min=78.571f, .hp_max=79.592f, .sp_min=6.306f, .sp_max=6.449f,
      .str_min=0.612f, .str_max=0.653f, .agi_min=0.612f, .agi_max=0.653f,
      .vit_min=0.612f, .vit_max=0.653f, .int_min=0.633f, .int_max=0.694f,
      .dex_min=0.735f, .dex_max=0.776f, .luk_min=0.735f, .luk_max=0.776f },

    // Average
    { .hp_min=79.592f, .hp_max=80.408f, .sp_min=6.449f, .sp_max=6.551f,
      .str_min=0.653f, .str_max=0.694f, .agi_min=0.653f, .agi_max=0.694f,
      .vit_min=0.653f, .vit_max=0.694f, .int_min=0.694f, .int_max=0.735f,
      .dex_min=0.776f, .dex_max=0.816f, .luk_min=0.776f, .luk_max=0.816f },

    // Good
    { .hp_min=80.408f, .hp_max=81.429f, .sp_min=6.551f, .sp_max=6.694f,
      .str_min=0.694f, .str_max=0.714f, .agi_min=0.694f, .agi_max=0.714f,
      .vit_min=0.694f, .vit_max=0.714f, .int_min=0.735f, .int_max=0.776f,
      .dex_min=0.816f, .dex_max=0.857f, .luk_min=0.816f, .luk_max=0.857f },

    // Very Good
    { .hp_min=81.429f, .hp_max=82.041f, .sp_min=6.694f, .sp_max=6.796f,
      .str_min=0.714f, .str_max=0.755f, .agi_min=0.714f, .agi_max=0.755f,
      .vit_min=0.714f, .vit_max=0.755f, .int_min=0.776f, .int_max=0.816f,
      .dex_min=0.857f, .dex_max=0.898f, .luk_min=0.857f, .luk_max=0.898f },

    // Excellent
    { .hp_min=82.041f, .hp_max=82.653f, .sp_min=6.796f, .sp_max=6.878f,
      .str_min=0.755f, .str_max=0.776f, .agi_min=0.755f, .agi_max=0.776f,
      .vit_min=0.755f, .vit_max=0.776f, .int_min=0.816f, .int_max=0.837f,
      .dex_min=0.898f, .dex_max=0.918f, .luk_min=0.898f, .luk_max=0.918f },

    // Amazing
    { .hp_min=82.653f, .sp_min=6.878f,
      .str_min=0.776f, .agi_min=0.776f, .vit_min=0.776f,
      .int_min=0.837f, .dex_min=0.918f, .luk_min=0.918f }
};

static const StatRange LifStatRange60[RANK_COUNT] = {
    // Abysmal
    { .hp_max=77.288f, .sp_max=6.153f, .str_max=0.559f, .agi_max=0.559f,
      .vit_max=0.559f, .int_max=0.593f, .dex_max=0.678f, .luk_max=0.678f },

    // Terrible
    { .hp_min=77.288f, .hp_max=77.966f, .sp_min=6.153f, .sp_max=6.220f,
      .str_min=0.559f, .str_max=0.593f, .agi_min=0.559f, .agi_max=0.593f,
      .vit_min=0.559f, .vit_max=0.593f, .int_min=0.593f, .int_max=0.610f,
      .dex_min=0.678f, .dex_max=0.712f, .luk_min=0.678f, .luk_max=0.712f },

    // Poor
    { .hp_min=77.966f, .hp_max=78.644f, .sp_min=6.220f, .sp_max=6.322f,
      .str_min=0.593f, .str_max=0.610f, .agi_min=0.593f, .agi_max=0.610f,
      .vit_min=0.593f, .vit_max=0.610f, .int_min=0.610f, .int_max=0.644f,
      .dex_min=0.712f, .dex_max=0.746f, .luk_min=0.712f, .luk_max=0.746f },

    // Below Average
    { .hp_min=78.644f, .hp_max=79.492f, .sp_min=6.322f, .sp_max=6.441f,
      .str_min=0.610f, .str_max=0.644f, .agi_min=0.610f, .agi_max=0.644f,
      .vit_min=0.610f, .vit_max=0.644f, .int_min=0.644f, .int_max=0.678f,
      .dex_min=0.746f, .dex_max=0.780f, .luk_min=0.746f, .luk_max=0.780f },

    // Average
    { .hp_min=79.492f, .hp_max=80.339f, .sp_min=6.441f, .sp_max=6.559f,
      .str_min=0.644f, .str_max=0.678f, .agi_min=0.644f, .agi_max=0.678f,
      .vit_min=0.644f, .vit_max=0.678f, .int_min=0.678f, .int_max=0.729f,
      .dex_min=0.780f, .dex_max=0.814f, .luk_min=0.780f, .luk_max=0.814f },

    // Good
    { .hp_min=80.339f, .hp_max=81.186f, .sp_min=6.559f, .sp_max=6.678f,
      .str_min=0.678f, .str_max=0.712f, .agi_min=0.678f, .agi_max=0.712f,
      .vit_min=0.678f, .vit_max=0.712f, .int_min=0.729f, .int_max=0.763f,
      .dex_min=0.814f, .dex_max=0.864f, .luk_min=0.814f, .luk_max=0.864f },

    // Very Good
    { .hp_min=81.186f, .hp_max=81.864f, .sp_min=6.678f, .sp_max=6.780f,
      .str_min=0.712f, .str_max=0.746f, .agi_min=0.712f, .agi_max=0.746f,
      .vit_min=0.712f, .vit_max=0.746f, .int_min=0.763f, .int_max=0.797f,
      .dex_min=0.864f, .dex_max=0.898f, .luk_min=0.864f, .luk_max=0.881f },

    // Excellent
    { .hp_min=81.864f, .hp_max=82.373f, .sp_min=6.780f, .sp_max=6.847f,
      .str_min=0.746f, .str_max=0.763f, .agi_min=0.746f, .agi_max=0.763f,
      .vit_min=0.746f, .vit_max=0.763f, .int_min=0.797f, .int_max=0.831f,
      .dex_min=0.898f, .dex_max=0.915f, .luk_min=0.881f, .luk_max=0.915f },

    // Amazing
    { .hp_min=82.373f, .sp_min=6.847f,
      .str_min=0.763f, .agi_min=0.763f, .vit_min=0.763f,
      .int_min=0.831f, .dex_min=0.915f, .luk_min=0.915f }
};

static const StatRange LifStatRange70[RANK_COUNT] = {
    // Abysmal
    { .hp_max=77.536f, .sp_max=6.174f, .str_max=0.565f, .agi_max=0.580f,
      .vit_max=0.565f, .int_max=0.594f, .dex_max=0.696f, .luk_max=0.696f },

    // Terrible
    { .hp_min=77.536f, .hp_max=78.116f, .sp_min=6.174f, .sp_max=6.246f,
      .str_min=0.565f, .str_max=0.594f, .agi_min=0.580f, .agi_max=0.594f,
      .vit_min=0.565f, .vit_max=0.594f, .int_min=0.594f, .int_max=0.623f,
      .dex_min=0.696f, .dex_max=0.710f, .luk_min=0.696f, .luk_max=0.710f },

    // Poor
    { .hp_min=78.116f, .hp_max=78.696f, .sp_min=6.246f, .sp_max=6.333f,
      .str_min=0.594f, .str_max=0.623f, .agi_min=0.594f, .agi_max=0.623f,
      .vit_min=0.594f, .vit_max=0.623f, .int_min=0.623f, .int_max=0.652f,
      .dex_min=0.710f, .dex_max=0.739f, .luk_min=0.710f, .luk_max=0.739f },

    // Below Average
    { .hp_min=78.696f, .hp_max=79.565f, .sp_min=6.333f, .sp_max=6.449f,
      .str_min=0.623f, .str_max=0.652f, .agi_min=0.623f, .agi_max=0.652f,
      .vit_min=0.623f, .vit_max=0.652f, .int_min=0.652f, .int_max=0.681f,
      .dex_min=0.739f, .dex_max=0.783f, .luk_min=0.739f, .luk_max=0.783f },

    // Average
    { .hp_min=79.565f, .hp_max=80.290f, .sp_min=6.449f, .sp_max=6.551f,
      .str_min=0.652f, .str_max=0.681f, .agi_min=0.652f, .agi_max=0.681f,
      .vit_min=0.652f, .vit_max=0.681f, .int_min=0.681f, .int_max=0.725f,
      .dex_min=0.783f, .dex_max=0.812f, .luk_min=0.783f, .luk_max=0.812f },

    // Good
    { .hp_min=80.290f, .hp_max=81.159f, .sp_min=6.551f, .sp_max=6.667f,
      .str_min=0.681f, .str_max=0.710f, .agi_min=0.681f, .agi_max=0.710f,
      .vit_min=0.681f, .vit_max=0.710f, .int_min=0.725f, .int_max=0.768f,
      .dex_min=0.812f, .dex_max=0.855f, .luk_min=0.812f, .luk_max=0.855f },

    // Very Good
    { .hp_min=81.159f, .hp_max=81.739f, .sp_min=6.667f, .sp_max=6.754f,
      .str_min=0.710f, .str_max=0.739f, .agi_min=0.710f, .agi_max=0.739f,
      .vit_min=0.710f, .vit_max=0.739f, .int_min=0.768f, .int_max=0.797f,
      .dex_min=0.855f, .dex_max=0.884f, .luk_min=0.855f, .luk_max=0.884f },

    // Excellent
    { .hp_min=81.739f, .hp_max=82.174f, .sp_min=6.754f, .sp_max=6.826f,
      .str_min=0.739f, .str_max=0.754f, .agi_min=0.739f, .agi_max=0.754f,
      .vit_min=0.739f, .vit_max=0.754f, .int_min=0.797f, .int_max=0.826f,
      .dex_min=0.884f, .dex_max=0.913f, .luk_min=0.884f, .luk_max=0.913f },

    // Amazing
    { .hp_min=82.174f, .sp_min=6.826f,
      .str_min=0.754f, .agi_min=0.754f, .vit_min=0.754f,
      .int_min=0.826f, .dex_min=0.913f, .luk_min=0.913f }
};

static const StatRange LifStatRange80[RANK_COUNT] = {
    // Abysmal
    { .hp_max=77.722f, .sp_max=6.190f, .str_max=0.582f, .agi_max=0.582f,
      .vit_max=0.582f, .int_max=0.608f, .dex_max=0.696f, .luk_max=0.696f },

    // Terrible
    { .hp_min=77.722f, .hp_max=78.228f, .sp_min=6.190f, .sp_max=6.266f,
      .str_min=0.582f, .str_max=0.595f, .agi_min=0.582f, .agi_max=0.595f,
      .vit_min=0.582f, .vit_max=0.595f, .int_min=0.608f, .int_max=0.620f,
      .dex_min=0.696f, .dex_max=0.722f, .luk_min=0.696f, .luk_max=0.722f },

    // Poor
    { .hp_min=78.228f, .hp_max=78.861f, .sp_min=6.266f, .sp_max=6.342f,
      .str_min=0.595f, .str_max=0.620f, .agi_min=0.595f, .agi_max=0.620f,
      .vit_min=0.595f, .vit_max=0.620f, .int_min=0.620f, .int_max=0.646f,
      .dex_min=0.722f, .dex_max=0.747f, .luk_min=0.722f, .luk_max=0.747f },

    // Below Average
    { .hp_min=78.861f, .hp_max=79.620f, .sp_min=6.342f, .sp_max=6.456f,
      .str_min=0.620f, .str_max=0.658f, .agi_min=0.620f, .agi_max=0.658f,
      .vit_min=0.620f, .vit_max=0.658f, .int_min=0.646f, .int_max=0.684f,
      .dex_min=0.747f, .dex_max=0.785f, .luk_min=0.747f, .luk_max=0.785f },

    // Average
    { .hp_min=79.620f, .hp_max=80.253f, .sp_min=6.456f, .sp_max=6.544f,
      .str_min=0.658f, .str_max=0.684f, .agi_min=0.658f, .agi_max=0.684f,
      .vit_min=0.658f, .vit_max=0.684f, .int_min=0.684f, .int_max=0.722f,
      .dex_min=0.785f, .dex_max=0.810f, .luk_min=0.785f, .luk_max=0.810f },

    // Good
    { .hp_min=80.253f, .hp_max=81.013f, .sp_min=6.544f, .sp_max=6.658f,
      .str_min=0.684f, .str_max=0.709f, .agi_min=0.684f, .agi_max=0.709f,
      .vit_min=0.684f, .vit_max=0.709f, .int_min=0.722f, .int_max=0.759f,
      .dex_min=0.810f, .dex_max=0.848f, .luk_min=0.810f, .luk_max=0.848f },

    // Very Good
    { .hp_min=81.013f, .hp_max=81.646f, .sp_min=6.658f, .sp_max=6.734f,
      .str_min=0.709f, .str_max=0.734f, .agi_min=0.709f, .agi_max=0.734f,
      .vit_min=0.709f, .vit_max=0.734f, .int_min=0.759f, .int_max=0.785f,
      .dex_min=0.848f, .dex_max=0.873f, .luk_min=0.848f, .luk_max=0.873f },

    // Excellent
    { .hp_min=81.646f, .hp_max=82.152f, .sp_min=6.734f, .sp_max=6.810f,
      .str_min=0.734f, .str_max=0.747f, .agi_min=0.734f, .agi_max=0.747f,
      .vit_min=0.734f, .vit_max=0.747f, .int_min=0.785f, .int_max=0.810f,
      .dex_min=0.873f, .dex_max=0.899f, .luk_min=0.873f, .luk_max=0.899f },

    // Amazing
    { .hp_min=82.152f, .sp_min=6.810f,
      .str_min=0.747f, .agi_min=0.747f, .vit_min=0.747f,
      .int_min=0.810f, .dex_min=0.899f, .luk_min=0.899f }
};

static const StatRange LifStatRange90[RANK_COUNT] = {
    // Abysmal
    { .hp_max=77.865f, .sp_max=6.213f, .str_max=0.584f, .agi_max=0.584f,
      .vit_max=0.584f, .int_max=0.607f, .dex_max=0.708f, .luk_max=0.708f },

    // Terrible
    { .hp_min=77.865f, .hp_max=78.315f, .sp_min=6.213f, .sp_max=6.270f,
      .str_min=0.584f, .str_max=0.607f, .agi_min=0.584f, .agi_max=0.607f,
      .vit_min=0.584f, .vit_max=0.607f, .int_min=0.607f, .int_max=0.629f,
      .dex_min=0.708f, .dex_max=0.730f, .luk_min=0.708f, .luk_max=0.730f },

    // Poor
    { .hp_min=78.315f, .hp_max=78.876f, .sp_min=6.270f, .sp_max=6.348f,
      .str_min=0.607f, .str_max=0.629f, .agi_min=0.607f, .agi_max=0.629f,
      .vit_min=0.607f, .vit_max=0.629f, .int_min=0.629f, .int_max=0.652f,
      .dex_min=0.730f, .dex_max=0.753f, .luk_min=0.730f, .luk_max=0.753f },

    // Below Average
    { .hp_min=78.876f, .hp_max=79.663f, .sp_min=6.348f, .sp_max=6.461f,
      .str_min=0.629f, .str_max=0.652f, .agi_min=0.629f, .agi_max=0.652f,
      .vit_min=0.629f, .vit_max=0.652f, .int_min=0.652f, .int_max=0.685f,
      .dex_min=0.753f, .dex_max=0.787f, .luk_min=0.753f, .luk_max=0.787f },

    // Average
    { .hp_min=79.663f, .hp_max=80.225f, .sp_min=6.461f, .sp_max=6.551f,
      .str_min=0.652f, .str_max=0.685f, .agi_min=0.652f, .agi_max=0.685f,
      .vit_min=0.652f, .vit_max=0.685f, .int_min=0.685f, .int_max=0.719f,
      .dex_min=0.787f, .dex_max=0.809f, .luk_min=0.787f, .luk_max=0.820f },

    // Good
    { .hp_min=80.225f, .hp_max=81.011f, .sp_min=6.551f, .sp_max=6.652f,
      .str_min=0.685f, .str_max=0.708f, .agi_min=0.685f, .agi_max=0.708f,
      .vit_min=0.685f, .vit_max=0.708f, .int_min=0.719f, .int_max=0.753f,
      .dex_min=0.809f, .dex_max=0.843f, .luk_min=0.820f, .luk_max=0.843f },

    // Very Good
    { .hp_min=81.011f, .hp_max=81.573f, .sp_min=6.652f, .sp_max=6.719f,
      .str_min=0.708f, .str_max=0.730f, .agi_min=0.708f, .agi_max=0.730f,
      .vit_min=0.708f, .vit_max=0.730f, .int_min=0.753f, .int_max=0.787f,
      .dex_min=0.843f, .dex_max=0.876f, .luk_min=0.843f, .luk_max=0.876f },

    // Excellent
    { .hp_min=81.573f, .hp_max=82.022f, .sp_min=6.719f, .sp_max=6.787f,
      .str_min=0.730f, .str_max=0.742f, .agi_min=0.730f, .agi_max=0.742f,
      .vit_min=0.730f, .vit_max=0.742f, .int_min=0.787f, .int_max=0.809f,
      .dex_min=0.876f, .dex_max=0.899f, .luk_min=0.876f, .luk_max=0.888f },

    // Amazing
    { .hp_min=82.022f, .sp_min=6.787f,
      .str_min=0.742f, .agi_min=0.742f, .vit_min=0.742f,
      .int_min=0.809f, .dex_min=0.899f, .luk_min=0.888f }
};

static const StatRange LifStatRange99[RANK_COUNT] = {
    // Abysmal
    { .hp_max=77.959f, .sp_max=6.224f, .str_max=0.592f, .agi_max=0.592f,
      .vit_max=0.592f, .int_max=0.612f, .dex_max=0.714f, .luk_max=0.714f },

    // Terrible
    { .hp_min=77.959f, .hp_max=78.367f, .sp_min=6.224f, .sp_max=6.286f,
      .str_min=0.592f, .str_max=0.602f, .agi_min=0.592f, .agi_max=0.602f,
      .vit_min=0.592f, .vit_max=0.602f, .int_min=0.612f, .int_max=0.633f,
      .dex_min=0.714f, .dex_max=0.735f, .luk_min=0.714f, .luk_max=0.735f },

    // Poor
    { .hp_min=78.367f, .hp_max=78.980f, .sp_min=6.286f, .sp_max=6.357f,
      .str_min=0.602f, .str_max=0.622f, .agi_min=0.602f, .agi_max=0.622f,
      .vit_min=0.602f, .vit_max=0.622f, .int_min=0.633f, .int_max=0.653f,
      .dex_min=0.735f, .dex_max=0.755f, .luk_min=0.735f, .luk_max=0.755f },

    // Below Average
    { .hp_min=78.980f, .hp_max=79.694f, .sp_min=6.357f, .sp_max=6.459f,
      .str_min=0.622f, .str_max=0.653f, .agi_min=0.622f, .agi_max=0.653f,
      .vit_min=0.622f, .vit_max=0.653f, .int_min=0.653f, .int_max=0.694f,
      .dex_min=0.755f, .dex_max=0.786f, .luk_min=0.755f, .luk_max=0.786f },

    // Average
    { .hp_min=79.694f, .hp_max=80.306f, .sp_min=6.459f, .sp_max=6.541f,
      .str_min=0.653f, .str_max=0.684f, .agi_min=0.653f, .agi_max=0.684f,
      .vit_min=0.653f, .vit_max=0.684f, .int_min=0.694f, .int_max=0.724f,
      .dex_min=0.786f, .dex_max=0.816f, .luk_min=0.786f, .luk_max=0.816f },

    // Good
    { .hp_min=80.306f, .hp_max=80.918f, .sp_min=6.541f, .sp_max=6.643f,
      .str_min=0.684f, .str_max=0.704f, .agi_min=0.684f, .agi_max=0.704f,
      .vit_min=0.684f, .vit_max=0.704f, .int_min=0.724f, .int_max=0.755f,
      .dex_min=0.816f, .dex_max=0.847f, .luk_min=0.816f, .luk_max=0.847f },

    // Very Good
    { .hp_min=80.918f, .hp_max=81.429f, .sp_min=6.643f, .sp_max=6.714f,
      .str_min=0.704f, .str_max=0.724f, .agi_min=0.704f, .agi_max=0.724f,
      .vit_min=0.704f, .vit_max=0.724f, .int_min=0.755f, .int_max=0.776f,
      .dex_min=0.847f, .dex_max=0.867f, .luk_min=0.847f, .luk_max=0.867f },

    // Excellent
    { .hp_min=81.429f, .hp_max=81.837f, .sp_min=6.714f, .sp_max=6.776f,
      .str_min=0.724f, .str_max=0.745f, .agi_min=0.724f, .agi_max=0.745f,
      .vit_min=0.724f, .vit_max=0.745f, .int_min=0.776f, .int_max=0.806f,
      .dex_min=0.867f, .dex_max=0.888f, .luk_min=0.867f, .luk_max=0.888f },

    // Amazing
    { .hp_min=81.837f, .sp_min=6.776f,
      .str_min=0.745f, .agi_min=0.745f, .vit_min=0.745f,
      .int_min=0.806f, .dex_min=0.888f, .luk_min=0.888f }
};

static const StatRange *lif_stat_ranges[] = {
    LifStatRange10,
    LifStatRange20,
    LifStatRange30,
    LifStatRange40,
    LifStatRange50,
    LifStatRange60,
    LifStatRange70,
    LifStatRange80,
    LifStatRange90,
    LifStatRange99
};

// ================== End of Lif ====================

/* void init_homun_stats(HomunStatValues* out) {

	struct homun_data *hd;
	struct s_homunculus_db *db;
	struct s_homunculus *hom;
	int lv, min, max, evo;
	
	
	hd = sd->hd;

	hom = &hd->homunculus;
	db = hd->homunculusDB;
	lv = hom->level;
	lv--;
	
	
	if (db->name == "Vanilmirth")
	{
		base_hp = 80;
		base_sp = 11;
		base_str = 11;
		base_agi = 11;
		base_vit = 11;
		base_int = 11;
		base_dex = 11;
		base_luk = 11;
		
	}
	
	out->hp   = (hom->max_hp - base_hp) / lv;
	out->sp   = (hom->max_sp - base_sp) / lv;
	out->str  = (hom->str/10 - base_str) / lv;
	out->agi  = (hom->agi/10 - base_agi) / lv;
	out->vit  = (hom->vit/10 - base_vit) / lv;
	out->int_ = (hom->int_/10 - base_int) / lv;
	out->dex  = (hom->dex/10 - base_dex) / lv;
	out->luk  = (hom->luk/10 - base_luk) / lv;
} */

ACMD(homgrowth)
{
	struct homun_data *hd;
	struct s_homunculus_db *db;
	struct s_homunculus *hom;
	int lv, c_lv, min, max, evo;
	int base_hp, base_sp, base_str, base_agi, base_vit, base_int, base_dex, base_luk;
	double hp, sp, str, agi, vit, int_, dex, luk;
	
	HomunStatRankNames rank_names;

	if (!homun_alive(sd->hd)) {
		clif->message(fd, msg_fd(fd,1254)); // You do not have a homunculus.
		return false;
	}

	hd = sd->hd;

	hom = &hd->homunculus;
	db = hd->homunculusDB;
	lv = hom->level;
	c_lv = hom->level;
		
	clif->message(fd,"------ Growth Chart Ranking ------");
	clif->message(fd,"1. Abysmal");
	clif->message(fd,"2. Terrible");
	clif->message(fd,"3. Poor");
	clif->message(fd,"4. Below Average");
	clif->message(fd,"5. Average");
	clif->message(fd,"6. Good");
	clif->message(fd,"7. Very Good");
	clif->message(fd,"8. Excellent");
	clif->message(fd,"9. Amazing");
	clif->message(fd,"----------------------------------");
	snprintf(atcmd_output, sizeof(atcmd_output) ,
			 msg_fd(fd,1266), lv, db->name); // Homunculus growth stats (Lv %d %s):
	clif->message(fd, atcmd_output);
	lv--; //Since the first increase is at level 2.

	
	// Establishing base values
	
	if (strcmp(db->name, "Vanilmirth") == 0) {
		base_hp = 80;
		base_sp = 11;
		base_str = 11;
		base_agi = 11;
		base_vit = 11;
		base_int = 11;
		base_dex = 11;
		base_luk = 11;
	} else if (strcmp(db->name, "Filir") == 0) {
		base_hp = 90;
		base_sp = 25;
		base_str = 29;
		base_agi = 35;
		base_vit = 9;
		base_int = 8;
		base_dex = 30;
		base_luk = 9;
	} else if (strcmp(db->name, "Amistr") == 0) {
		base_hp = 320;
		base_sp = 10;
		base_str = 20;
		base_agi = 17;
		base_vit = 35;
		base_int = 11;
		base_dex = 24;
		base_luk = 12;
	} else if (strcmp(db->name, "Lif") == 0) {
		base_hp = 150;
		base_sp = 40;
		base_str = 12;
		base_agi = 20;
		base_vit = 15;
		base_int = 35;
		base_dex = 24;
		base_luk = 15;
	}
	
	// Calculation of growth
	
	hp   = (double)(hom->max_hp - base_hp) / lv;
	sp   = (double)(hom->max_sp - base_sp) / lv;
	str  = (double)(hom->str/10 - base_str) / lv;
	agi  = (double)(hom->agi/10 - base_agi) / lv;
	vit  = (double)(hom->vit/10 - base_vit) / lv;
	int_ = (double)(hom->int_/10 - base_int) / lv;
	dex  = (double)(hom->dex/10 - base_dex) / lv;
	luk  = (double)(hom->luk/10 - base_luk) / lv;

	if (strcmp(db->name, "Vanilmirth") == 0){
		
		int index = c_lv / 10;
		if (index > 9) index = 9; // cap at 99

		display_all_stat_ranks(
			hp, sp, str, agi,
			vit, int_, dex, luk,
			vanil_stat_ranges[index],  // Pass the pointer directly
			&rank_names
		);
		
	} else if (strcmp(db->name, "Filir") == 0){
		
		int index = c_lv / 10;
		if (index > 9) index = 9; // cap at 99

		display_all_stat_ranks(
			hp, sp, str, agi,
			vit, int_, dex, luk,
			filir_stat_ranges[index],  // Pass the pointer directly
			&rank_names
		);
		
	} else if (strcmp(db->name, "Amistr") == 0){
		
		int index = c_lv / 10;
		if (index > 9) index = 9; // cap at 99

		display_all_stat_ranks(
			hp, sp, str, agi,
			vit, int_, dex, luk,
			amistr_stat_ranges[index],  // Pass the pointer directly
			&rank_names
		);
		
	} else if (strcmp(db->name, "Lif") == 0){
		
		int index = c_lv / 10;
		if (index > 9) index = 9; // cap at 99

		display_all_stat_ranks(
			hp, sp, str, agi,
			vit, int_, dex, luk,
			lif_stat_ranges[index],  // Pass the pointer directly
			&rank_names
		);
		
	}
	


	snprintf(atcmd_output, sizeof(atcmd_output), "HP: %d (%s)", hom->max_hp, rank_names.hp);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "SP: %d (%s)", hom->max_sp, rank_names.sp);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "Str: %d (%s)", hom->str/10, rank_names.str);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "Agi: %d (%s)", hom->agi/10, rank_names.agi);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "Vit: %d (%s)", hom->vit/10, rank_names.vit);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "Int: %d (%s)", hom->int_/10, rank_names.int_);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "Dex: %d (%s)", hom->dex/10, rank_names.dex);
	clif->message(fd, atcmd_output);
	snprintf(atcmd_output, sizeof(atcmd_output), "Luk: %d (%s)", hom->luk/10, rank_names.luk);
	clif->message(fd, atcmd_output);
	
	
	/* 	int max_hp = hom->max_hp;
	float vanil_hp = (max_hp - 80)/lv;
	
	StatRank hp_rank = get_stat_rank(vanil_hp, STAT_HP, VanilStatRange10);
	const char* rank_name = get_rank_name(hp_rank);
	snprintf(atcmd_output, sizeof(atcmd_output), "HP Growth Rank: %s", rank_name);
	clif->message(fd, atcmd_output); */

/* 	evo = (hom->class_ == db->evo_class);
	min = db->base.HP +lv*db->gmin.HP +(evo?db->emin.HP:0);
	max = db->base.HP +lv*db->gmax.HP +(evo?db->emax.HP:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1267), hom->max_hp, min, max); // Max HP: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.SP +lv*db->gmin.SP +(evo?db->emin.SP:0);
	max = db->base.SP +lv*db->gmax.SP +(evo?db->emax.SP:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1268), hom->max_sp, min, max); // Max SP: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.str +lv*(db->gmin.str/10) +(evo?db->emin.str:0);
	max = db->base.str +lv*(db->gmax.str/10) +(evo?db->emax.str:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1269), hom->str/10, min, max); // Str: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.agi +lv*(db->gmin.agi/10) +(evo?db->emin.agi:0);
	max = db->base.agi +lv*(db->gmax.agi/10) +(evo?db->emax.agi:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1270), hom->agi/10, min, max); // Agi: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.vit +lv*(db->gmin.vit/10) +(evo?db->emin.vit:0);
	max = db->base.vit +lv*(db->gmax.vit/10) +(evo?db->emax.vit:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1271), hom->vit/10, min, max); // Vit: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.int_ +lv*(db->gmin.int_/10) +(evo?db->emin.int_:0);
	max = db->base.int_ +lv*(db->gmax.int_/10) +(evo?db->emax.int_:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1272), hom->int_/10, min, max); // Int: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.dex +lv*(db->gmin.dex/10) +(evo?db->emin.dex:0);
	max = db->base.dex +lv*(db->gmax.dex/10) +(evo?db->emax.dex:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1273), hom->dex/10, min, max); // Dex: %d (%d~%d)
	clif->message(fd, atcmd_output);

	min = db->base.luk +lv*(db->gmin.luk/10) +(evo?db->emin.luk:0);
	max = db->base.luk +lv*(db->gmax.luk/10) +(evo?db->emax.luk:0);;
	snprintf(atcmd_output, sizeof(atcmd_output) ,msg_fd(fd,1274), hom->luk/10, min, max); // Luk: %d (%d~%d)
	clif->message(fd, atcmd_output); */

	return true;

}

/* Server Startup */
HPExport void plugin_init(void)
{
	addAtcommand("homgrowth", homgrowth);
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Ghost/Seabois. Version '%s'\n", pinfo.name, pinfo.version);
}