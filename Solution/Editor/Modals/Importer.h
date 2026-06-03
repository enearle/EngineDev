#pragma once
#include <string>

class Importer
{
public:
    static Importer& GetInstance();
    void Open(std::string path);
    void Render();
};
