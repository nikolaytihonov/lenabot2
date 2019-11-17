#ifndef __VKAPICONTROLLER_H
#define __VKAPICONTROLLER_H

#include "json/json.h"

class IVkRequest;
class VkException;

//Этот интерфейс описывает сущность, которая будет ответственна за ошибки
//и любые непредвиденнные последствия. Полный контроль запросов и ошибок.
//После ошибки этот класс должен будет предпринять меры по решению проблемы.

class IVkApiController
{
public:
	virtual void OnLongPoll(json_value& val) = 0;
	virtual void OnError(const VkException& e) = 0;
	virtual void OnRequestFinished(IVkRequest* req) = 0;
};

#endif