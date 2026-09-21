// Reconstructed from Grimrock.bin.x86 Texture.cpp (texture part).
#include "engine/Texture.h"
#include "engine/Renderer.h"
#include <cstring>

namespace engine
{

core::Array<RenderableTexture*> RenderableTexture::sm_textures;

// 0x080ff040
RenderableTexture::RenderableTexture()
{
    sm_textures.push_back(this);
}
// 0x080fef90
RenderableTexture::~RenderableTexture()
{
    sm_textures.remove(this);
}
// 0x080fef20
RenderableTexture* RenderableTexture::getTextureByFilename(const char* filename)
{
    for (int i = 0; i < sm_textures.size(); ++i)
        if (strcmp(sm_textures[i]->m_filename.c_str(), filename) == 0)
            return sm_textures[i];
    return 0;
}
// 0x080fee80
RenderableTexture* createRenderableTexture(const core::Image& image)
{
    RenderableTexture* texture = Renderer::getActiveRenderer()->createRenderableTexture();
    texture->init(image);
    return texture;
}
// 0x080ff120
RenderableTexture* loadRenderableTexture(const char* filename, int flags, bool mipmaps)
{
    RenderableTexture* existing = RenderableTexture::getTextureByFilename(filename);
    if (existing)
        return existing;
    RenderableTexture* texture = Renderer::getActiveRenderer()->createRenderableTexture();
    texture->load(filename, flags, mipmaps);
    return texture;
}

} // namespace engine
