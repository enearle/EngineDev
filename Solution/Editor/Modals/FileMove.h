#pragma once
#include <string>
#include <system_error>

class FileMove
{
public:
    static FileMove& GetInstance();
    void Open(std::string startingPath, std::string destinationPath);
    void Render();
    
private:
    char Buffer[128] = {};
    std::string StartingPath;
    std::string DestinationPath;
    std::error_code ec;
    bool PendingOpen = false;
};
