//===== Hercules Plugin ======================================
//= MobID on Display
//===== By: ==================================================
//= Ghost / Seabois
//===== Current Version: =====================================
//= 1.0
//===== Description: =========================================
//= Adds MobID besides Mobname
//= query using mob name.
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

HPExport struct hplugin_info pinfo = {
	"MobIDDisplay",		// Plugin name
	SERVER_TYPE_MAP,// Which server types this plugin works with?
	"1.0",			// Plugin version
	HPM_VERSION,	// HPM Version (don't change, macro is automatically updated)
};

static inline int get_percentage(unsigned int val, unsigned int max) {
	return (max > 0) ? ((val * 100) / max) : 0;
}

static void clif_mobname_additional_ack_with_mobid(int fd, struct block_list *bl)
{
	nullpo_retv(bl);
	Assert_retv(bl->type == BL_MOB);

	struct PACKET_ZC_ACK_REQNAME_TITLE packet = { 0 };
	packet.packet_id = HEADER_ZC_ACK_REQNAME_TITLE;
	packet.gid = bl->id;
	const struct mob_data *md = BL_UCCAST(BL_MOB, bl);
	
	char mobidline[NAME_LENGTH];
	snprintf(mobidline, sizeof(mobidline), "%s [%d]",md->name, md->class_);
	memcpy(packet.name, mobidline, NAME_LENGTH);
#if PACKETVER_MAIN_NUM >= 20180207 || PACKETVER_RE_NUM >= 20171129 || PACKETVER_ZERO_NUM >= 20171130
	struct unit_data *ud = unit->bl2ud(bl);
	if (ud != NULL) {
		memcpy(packet.title, ud->title, NAME_LENGTH);
		packet.groupId = ud->groupId;
	}
#endif

	clif->send_selforarea(fd, bl, &packet, sizeof(struct PACKET_ZC_ACK_REQNAME_TITLE));
}


HPExport void plugin_init(void) {
	clif->mobname_normal_ack = clif_mobname_additional_ack_with_mobid;
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Ghost/Seabois. Version '%s'\n", pinfo.name, pinfo.version);
}