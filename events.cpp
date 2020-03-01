#include "event.h"
#include "bot.h"
#include "service.h"
#include "leto.h"

extern Bot* g_pBot;

//2000000057

//int g_TargetConv = 2000000057;
//
//DECLARE_TIMER(testevent,Time(60*60*2,0))
//
//EVENT_FIRE_BEGIN()
//	//bot.SetHighPriority(true);
//	//bot.Send(g_TargetConv,"Св.Елена превыше всего!");
//	bot.Send(ConvGroup,"Св.Елена превыше всего!");
//EVENT_FIRE_END()
//
//END_TIMER(testevent)

//Таймер автоматического сохранения базы данных!
DECLARE_TIMER(db_save,Time(60*5,0))

EVENT_FIRE_BEGIN()
	services.SaveServices();
EVENT_FIRE_END()

END_TIMER(db_save)

//Таймер автоматического обновления базы артов (каждые 12 часов)
DECLARE_TIMER(leto_update,Time(60*60*12,0))

EVENT_FIRE_BEGIN()
    leto->Update();
EVENT_FIRE_END()

END_TIMER(leto_update)
