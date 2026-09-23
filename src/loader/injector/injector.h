#pragma once

#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <filesystem>
#include <memory>

namespace injector {

    class c_injector
    {
    public:
        c_injector(std::string process_name) {
            this->process_name = process_name;
            this->pid = 0;
            this->dll_path = std::filesystem::current_path().string() + "\\Swift.dll";
        }

        c_injector(DWORD pid) {
            this->pid = pid;
            this->process_name = "";
            this->dll_path = std::filesystem::current_path().string() + "\\Swift.dll";
        }

        DWORD get_process_id();
        bool inject();

        std::string dll_path;

    private:
        std::string process_name;
        DWORD pid;
    };

    inline std::unique_ptr<c_injector> instance;
}