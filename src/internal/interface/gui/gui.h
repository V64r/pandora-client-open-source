#pragma once

#include <windows.h>

void InitMenuData();
void RenderMenuContents();
void RenderNotifications();
void TriggerNotification(const char* title, const char* body, const char* tag);
