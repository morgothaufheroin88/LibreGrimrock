// Renderable textures, from Texture.cpp (0x080fee80-0x080ff1f0).
#pragma once
#include "core/Image.h"
#include "core/String.h"

namespace engine
{

class RenderableTexture
{
  public:
    RenderableTexture();
    virtual ~RenderableTexture();
    virtual void init(const core::Image& image) = 0;
    virtual void load(const char* filename, int flags, bool mipmaps) = 0;
    virtual void reload() = 0;
    virtual void save(const char* filename) = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;

    const core::String& getFilename() const
    {
        return m_filename;
    }
    void setFilename(const char* filename)
    {
        m_filename = filename;
    }
    // 0x080fef20
    static RenderableTexture* getTextureByFilename(const char* filename);

  protected:
    core::String m_filename;
    static core::Array<RenderableTexture*> sm_textures;
};

// 0x080fee80: texture from an image through the active renderer.
RenderableTexture* createRenderableTexture(const core::Image& image);
// 0x080ff120: cached by filename; goes through the texture asset processor.
RenderableTexture* loadRenderableTexture(const char* filename, int flags, bool mipmaps);

} // namespace engine
