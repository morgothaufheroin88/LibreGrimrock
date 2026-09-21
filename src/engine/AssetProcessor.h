// Asset pipeline registry, from AssetProcessor.cpp (0x080d7a70-0x080d8eb0). Loaders ask
// for the processor registered for a file extension, run it and then open the
// "native" file (same name with the native extension).
#pragma once
#include "core/SharedPtr.h"
#include "core/String.h"

namespace engine
{

class AssetProcessor
{
  public:
    enum AssetType
    {
        ModelAsset = 0,
        AnimationAsset = 1,
        TextureAsset = 2,
        NumAssetTypes = 3
    };

    AssetProcessor() : m_type(ModelAsset) {}
    virtual ~AssetProcessor() {}
    virtual void processFile(const char* filename) = 0;

    // 0x080d7d00: strip the extension and append the native one.
    core::String getNativeFile(const char* filename) const;
    AssetType getAssetType() const
    {
        return m_type;
    }
    const core::String& getFileExtension() const
    {
        return m_fileExtension;
    }
    const core::String& getNativeFileExtension() const
    {
        return m_nativeFileExtension;
    }

    void setExtensions(AssetType type, const char* extension, const char* nativeExtension)
    {
        m_type = type;
        m_fileExtension = extension;
        if (nativeExtension)
            m_nativeFileExtension = nativeExtension;
    }

  protected:
    AssetType m_type;
    core::String m_fileExtension;
    core::String m_nativeFileExtension;
};

// Converts one source file into one native file when the dates differ.
class SingleFileAssetProcessor : public AssetProcessor
{
  public:
    void processFile(const char* filename);
    virtual void processSingleFile(const char* source, const char* native) = 0;
};

class BaseTextureAssetProcessor : public AssetProcessor
{
  public:
    void processFile(const char* filename);
    virtual void processTexture(const char* source, const char* native, const bool* options) = 0;
};

void registerAssetProcessor(AssetProcessor::AssetType type, const char* extension,
                            AssetProcessor* processor);
// 0x080d8bc0: registers a pass-through processor (files are already native).
void registerAssetProcessor(AssetProcessor::AssetType type, const char* extension,
                            const char* nativeExtension);
void removeAssetProcessor(AssetProcessor::AssetType type, const char* extension);
// 0x080d7bf0: throws when no processor matches the file extension.
AssetProcessor* findAssetProcessor(AssetProcessor::AssetType type, const char* filename);
bool checkSingleFileUptoDate(const char* source, const char* native);

} // namespace engine
