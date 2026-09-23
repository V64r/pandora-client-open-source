#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

extern std::vector<std::string> g_ConfigList;
extern std::map<std::string, std::string> g_InMemoryConfigs;
extern std::string g_ActiveConfig;
extern bool gui_config_just_loaded;
extern uint64_t g_ConfigLoadRevision;

void RefreshConfigs();
void PollConfigFolderChanges();
void SaveConfig(std::string name);
void LoadConfig(std::string name);
void DeleteConfig(std::string name);
void OpenConfigFolder();
