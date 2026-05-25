#pragma once
#include <string>

class Importer
{
public:
    static Importer& GetInstance();
    void Open();
    void Render();
};
