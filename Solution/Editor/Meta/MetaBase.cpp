#include "MetaBase.h"

uint64_t MetaBase::NewID = 0;
std::map<uint64_t, std::string> MetaBase::MetaMap;

void MetaBase::SerializeMainMetaList()
{
    // Write to main meta file (map and New ID)
}

void MetaBase::LoadMainMetaList()
{
    // Load main meta file (map and New ID)
}

MetaBase::MetaBase(ResourceType type, const std::string& fileLocation)
    : Type(type), FileLocation(fileLocation)
{
    ID = NewID++;
    SerializeLocalMetaFile();
}

MetaBase::MetaBase(const std::string& fileLocation)
{
    // Load local meta file
}

void MetaBase::UpdateLocation(const std::string& fileLocation)
{
    // Move files
    FileLocation = fileLocation;
    SerializeLocalMetaFile();
}

void MetaBase::SerializeLocalMetaFile()
{
    // Write to a local meta file
}
