#pragma once
#include <string>

class NewDirectory
{
public:
    static NewDirectory& GetInstance();
    void Open(std::string startingPath);
    void Render();

private:
    char Buffer[128] = {};
    std::string StartingPath;
    bool IsOpen = false;
    bool PendingOpen = false;
    void Close();
};