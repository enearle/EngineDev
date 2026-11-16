#include "Material.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

static const std::string MaterialDirectory = "../Engine/Materials/";
static const std::string TextureDirectory = "../Engine/Textures/";

static std::string Trim(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t\r");
    size_t last = text.find_last_not_of(" \t\r");
    return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

bool MaterialFiles::Read(const std::string& name, MaterialDesc& desc)
{
    std::string path = MaterialDirectory + name + ".material";
    std::ifstream file(path);
    if (!file)
        return false;

    desc = {};
    std::string line;
    for (uint32_t lineNumber = 1; std::getline(file, line); lineNumber++)
    {
        line = Trim(line.substr(0, line.find('#')));
        if (line.empty())
            continue;

        size_t equals = line.find('=');
        if (equals == std::string::npos)
            throw std::runtime_error(path + "(" + std::to_string(lineNumber) + "): expected key = value");
        std::string key = Trim(line.substr(0, equals));
        std::istringstream value(line.substr(equals + 1));

        std::vector<std::string> words;
        for (std::string word; value >> word;)
            words.push_back(word);
        if (key.empty() || words.empty())
            throw std::runtime_error(path + "(" + std::to_string(lineNumber) + "): expected key = value");

        if (key == "type")
            desc.Type = words[0];
        else
            desc.Values[key] = std::move(words);
    }
    if (desc.Type.empty())
        throw std::runtime_error(path + ": no type");
    return true;
}

bool MaterialFiles::ReadConvention(const std::string& name, MaterialDesc& desc)
{
    if (!std::filesystem::exists(TextureDirectory + name + "_diff.png"))
        return false;

    desc = {};
    desc.Type = "textured_pbr";
    desc.Values["albedo"] = { name + "_diff" };
    desc.Values["normal"] = { name + "_norm" };
    desc.Values["metallic_roughness_occlusion"] = { name + "_metal", name + "_rough", name + "_ao" };
    return true;
}
