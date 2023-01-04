#pragma once

void danmu_init();
void danmu_cleanup();

void danmu_set_reading_enabled(BOOL enabled);
BOOL danmu_get_reading_enabled();

void danmu_set_roomid(int roomId);

struct DanmuItem
{
	int type;
	char text[];
};

#define DANMUITEM_TYPE_MSG 0
#define DANMUITEM_TYPE_SYSTEM 1

typedef void DanmuItemHandlerType(DanmuItem*);
void danmu_get_buffered(DanmuItemHandlerType handler);

unsigned int danmu_get_popularity();