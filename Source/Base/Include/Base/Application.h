#pragma once

#include "Base/Common.h"

namespace dfr
{

class Application
{
public:
	Application();
	virtual ~Application();

	virtual void DrawUI() = 0;

};

}