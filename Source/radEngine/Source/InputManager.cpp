#include "InputManager.h"

namespace rad
{
std::unique_ptr<InputManager> InputManager::Instance = nullptr;
bool InputManager::Init()
{
	memset(CUR_KEYS, KEY_UP, sizeof(CUR_KEYS));
	return true;
}
}