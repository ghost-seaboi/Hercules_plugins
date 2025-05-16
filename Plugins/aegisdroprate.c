//===== Hercules Plugin ======================================
//= AegisDropRate
//===== By: ==================================================
//= Ghost / Seabois
//===== Current Version: =====================================
//= 1.0
//===== Description: =========================================
//= Adds additional 0.01% drop rate to everything.
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

HPExport struct hplugin_info pinfo = {
	"AegisDropRate",		// Plugin name
	SERVER_TYPE_MAP,// Which server types this plugin works with?
	"1.0",			// Plugin version
	HPM_VERSION,	// HPM Version (don't change, macro is automatically updated)
};

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

static unsigned int mob_drop_adjust_mine(int baserate, int rate_adjust, unsigned short rate_min, unsigned short rate_max)
{
	int64 rate = baserate;

	Assert_ret(baserate >= 0);

	if (rate_adjust != 100 && baserate > 0) {
		if (battle->bc->logarithmic_drops && rate_adjust > 0) {
			// Logarithmic drops equation by Ishizu-Chan
			//Equation: Droprate(x,y) = x * (5 - log(x)) ^ (ln(y) / ln(5))
			//x is the normal Droprate, y is the Modificator.
			rate = (int64)(baserate * pow((5.0 - log10(baserate)), (log(rate_adjust/100.) / log(5.0))) + 0.5);
		} else {
			//Classical linear rate adjustment.
			rate = apply_percentrate64(baserate, rate_adjust, 100);
		}
	}

	// Ensure every item with a drop rate gets a +0.01% (1 in Hercules system)
	if (baserate > 0) {
		rate += 1; // +0.01% increase
	}

	// Apply min/max limits if needed
	if (rate < rate_min) rate = rate_min;
	if (rate > rate_max) rate = rate_max;

	return (unsigned int)cap_value(rate,rate_min,rate_max);
}

HPExport void plugin_init(void) {
	mob->drop_adjust = mob_drop_adjust_mine;
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Ghost/Seabois. Version '%s'\n", pinfo.name, pinfo.version);
}