// Reconstructed from Grimrock.bin.x86 AssetProcessor.cpp.
#include "engine/AssetProcessor.h"
#include "core/Array.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include <cstring>

namespace engine
{

using namespace core;

static Array<SharedPtr<AssetProcessor>> g_assetProcessors[AssetProcessor::NumAssetTypes];

namespace
{
// 0x080d7a80
class DummyAssetProcessor : public AssetProcessor
{
  public:
    void processFile(const char* filename) {}
};
} // namespace

// 0x080d7d00
String AssetProcessor::getNativeFile(const char* filename) const
{
    String base = stripExtension(filename);
    base.push_back(".", 1);
    String result(base);
    result.append(m_nativeFileExtension);
    return result;
}

// 0x080d8240
void SingleFileAssetProcessor::processFile(const char* filename)
{
    if (!fileExists(filename))
        return;
    String native = getNativeFile(filename);
    if (fileExists(native.c_str()) && fileDate(filename) == fileDate(native.c_str()))
        return;
    processSingleFile(filename, native.c_str());
    setFileDate(native.c_str(), fileDate(filename));
}

// 0x080d7e30: a sibling ".texture" data file may override compression/alpha options
// and takes part in the date check.
void BaseTextureAssetProcessor::processFile(const char* filename)
{
    if (!fileExists(filename))
        return;
    String native = getNativeFile(filename);
    String options = stripExtension(filename);
    options.push_back(".texture", 8);
    FileDate sourceDate = fileDate(filename);
    if (fileExists(options.c_str()))
    {
        FileDate optionsDate = fileDate(options.c_str());
        if (optionsDate > sourceDate)
            sourceDate = optionsDate;
    }
    if (fileExists(native.c_str()) && fileDate(native.c_str()) == sourceDate)
        return;
    bool flags[2] = {false, false}; // compressed, alpha (from the DataDef options file)
    processTexture(filename, native.c_str(), flags);
    setFileDate(native.c_str(), sourceDate);
}

// 0x080d85e0: replaces an existing processor for the same extension.
void registerAssetProcessor(AssetProcessor::AssetType type, const char* extension,
                            AssetProcessor* processor)
{
    processor->setExtensions(type, extension, 0);
    Array<SharedPtr<AssetProcessor>>& list = g_assetProcessors[type];
    for (int i = 0; i < list.size(); ++i)
    {
        if (strcmp(list[i]->getFileExtension().c_str(), extension) == 0)
        {
            list.erase(i);
            break;
        }
    }
    list.push_back(SharedPtr<AssetProcessor>(processor));
}
// 0x080d8bc0
void registerAssetProcessor(AssetProcessor::AssetType type, const char* extension,
                            const char* nativeExtension)
{
    DummyAssetProcessor* processor = new DummyAssetProcessor;
    processor->setExtensions(type, extension, nativeExtension);
    registerAssetProcessor(type, extension, processor);
}
// 0x080d8380
void removeAssetProcessor(AssetProcessor::AssetType type, const char* extension)
{
    Array<SharedPtr<AssetProcessor>>& list = g_assetProcessors[type];
    for (int i = 0; i < list.size(); ++i)
    {
        if (strcmp(list[i]->getFileExtension().c_str(), extension) == 0)
        {
            list.erase(i);
            return;
        }
    }
}
// 0x080d7bf0
AssetProcessor* findAssetProcessor(AssetProcessor::AssetType type, const char* filename)
{
    String ext = getFileExtension(filename);
    Array<SharedPtr<AssetProcessor>>& list = g_assetProcessors[type];
    for (int i = 0; i < list.size(); ++i)
        if (strcmp(list[i]->getFileExtension().c_str(), ext.c_str()) == 0)
            return list[i].get();
    throw Exception("Could not find asset processor for file: %s", filename);
}
// 0x080d7b70
bool checkSingleFileUptoDate(const char* source, const char* native)
{
    if (!fileExists(native))
        return false;
    return fileDate(source) == fileDate(native);
}

} // namespace engine
