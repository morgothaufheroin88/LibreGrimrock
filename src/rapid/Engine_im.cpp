// DebugDraw, ImmediateMode and Graphics bindings of Engine.cpp.
#include "EngineBindings.h"
#include "core/DebugDraw.h"
#include "engine/Graphics.h"
#include "engine/ImmediateMode.h"

using namespace core;
using namespace engine;

// 0x082c46e9: set between ImmediateMode.beginDraw/endDraw.
static bool g_imdebugDraw = false;

// Optional colour argument, white by default.
static Color optColor(lua_State* L, int index)
{
    Color color = Color::White;
    if (lua_gettop(L) >= index)
        color = luax::checkColor(L, index);
    return color;
}

// ---- DebugDraw -------------------------------------------------------------------

// 0x0813c930: drawLine(a, b [, color])
static int DebugDraw_drawLine(lua_State* L)
{
    Vec3 a = luax::checkVector3(L, 1);
    Vec3 b = luax::checkVector3(L, 2);
    Color color = optColor(L, 3);
    DebugDraw::drawLine(a, b, color);
    return 0;
}
// 0x0813cc40: drawBox(min, max [, transform [, color]])
static int DebugDraw_drawBox(lua_State* L)
{
    Vec3 min = luax::checkVector3(L, 1);
    Vec3 max = luax::checkVector3(L, 2);
    Matrix4x3 m = Matrix4x3::sm_mIdentity;
    if (lua_gettop(L) > 2)
        m = luax::checkMatrix4x3(L, 3);
    Color color = optColor(L, 4);
    DebugDraw::drawBox(min, max, m, color);
    return 0;
}
// 0x0813c890: drawSphere(center, radius [, color])
static int DebugDraw_drawSphere(lua_State* L)
{
    Vec3 center = luax::checkVector3(L, 1);
    float radius = (float)luaL_checknumber(L, 2);
    Color color = optColor(L, 3);
    DebugDraw::drawSphere(center, radius, color, 4);
    return 0;
}
// 0x0814e810: drawCapsule(radius, height, transform [, color])
static int DebugDraw_drawCapsule(lua_State* L)
{
    float radius = (float)luaL_checknumber(L, 1);
    float height = (float)luaL_checknumber(L, 2);
    Matrix4x3 m = luax::checkMatrix4x3(L, 3);
    Color color = optColor(L, 4);
    DebugDraw::drawCapsule(radius, height, m, color);
    return 0;
}
// 0x0814e600: drawBase(transform [, size])
static int DebugDraw_drawBase(lua_State* L)
{
    Matrix4x3 m = luax::checkMatrix4x3(L, 1);
    float size = (float)luaL_optnumber(L, 2, 1.0);
    DebugDraw::drawBase(m, size);
    return 0;
}
// 0x08146300: drawText(text, {x, y} [, color])
static int DebugDraw_drawText(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    if (lua_type(L, 2) != LUA_TTABLE)
        luaL_typerror(L, 2, "table");
    Vec2 pos(0.0f, 0.0f);
    lua_rawgeti(L, 2, 1);
    pos.x = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_rawgeti(L, 2, 2);
    pos.y = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    Color color = optColor(L, 3);
    DebugDraw::drawText(text, pos, color);
    return 0;
}
// 0x0813c7f0: drawText3D(text, pos [, color])
static int DebugDraw_drawText3D(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    Vec3 pos = luax::checkVector3(L, 2);
    Color color = optColor(L, 3);
    DebugDraw::drawText(text, pos, color);
    return 0;
}

const luaL_Reg DebugDraw_methods[] = {
    {"drawLine", DebugDraw_drawLine},     {"drawBox", DebugDraw_drawBox},
    {"drawSphere", DebugDraw_drawSphere}, {"drawCapsule", DebugDraw_drawCapsule},
    {"drawBase", DebugDraw_drawBase},     {"drawText", DebugDraw_drawText},
    {"drawText3D", DebugDraw_drawText3D}, {0, 0}};

// ---- ImmediateMode ---------------------------------------------------------------

static void checkDrawing(lua_State* L)
{
    if (!g_imdebugDraw)
        luaL_error(L, "ImmediateMode.beginDraw() not called");
}

// 0x0813e220
static int ImmediateMode_setTransform(lua_State* L)
{
    Matrix4x4 m = luax::checkMatrix4x4(L, 1);
    im::setTransform(m);
    return 0;
}
// 0x0813c7e0
static int ImmediateMode_setIdentityTransform(lua_State* L)
{
    im::setIdentityTransform();
    return 0;
}
// 0x0813c7b0
static int ImmediateMode_setLineWidth(lua_State* L)
{
    im::setLineWidth((float)luaL_checknumber(L, 1));
    return 0;
}
// 0x0813c780
static int ImmediateMode_setBlendMode(lua_State* L)
{
    im::setBlendMode((Material::BlendMode)luax::checkEnum(L, 1, g_blendModes));
    return 0;
}
// 0x0813c700
static int ImmediateMode_clipTo(lua_State* L)
{
    int x0 = luaL_checkinteger(L, 1);
    int y0 = luaL_checkinteger(L, 2);
    int x1 = luaL_checkinteger(L, 3);
    int y1 = luaL_checkinteger(L, 4);
    im::clipTo(x0, y0, x1, y1);
    return 0;
}
// 0x0813c6f0
static int ImmediateMode_resetClip(lua_State* L)
{
    im::resetClip();
    return 0;
}
// 0x0813c670
static int ImmediateMode_getClipRect(lua_State* L)
{
    int x0, y0, x1, y1;
    im::getClipRect(&x0, &y0, &x1, &y1);
    lua_pushnumber(L, x0);
    lua_pushnumber(L, y0);
    lua_pushnumber(L, x1);
    lua_pushnumber(L, y1);
    return 4;
}
// 0x0813c660
static int ImmediateMode_pushState(lua_State* L)
{
    im::pushState();
    return 0;
}
// 0x0813c630
static int ImmediateMode_popState(lua_State* L)
{
    if (!im::popState())
        luaL_error(L, "can't pop empty stack");
    return 0;
}
// 0x0813c610
static int ImmediateMode_beginDraw(lua_State* L)
{
    im::beginDraw();
    g_imdebugDraw = true;
    return 0;
}
// 0x0813c5f0
static int ImmediateMode_endDraw(lua_State* L)
{
    im::endDraw();
    g_imdebugDraw = false;
    return 0;
}
// 0x0813c540: drawPoint(x, y [, color])
static int ImmediateMode_drawPoint(lua_State* L)
{
    checkDrawing(L);
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    Color color = optColor(L, 3);
    im::drawPoint(x, y, color);
    return 0;
}
// 0x0813c450: drawLine(x0, y0, x1, y1 [, color])
static int ImmediateMode_drawLine(lua_State* L)
{
    checkDrawing(L);
    int x0 = luaL_checkinteger(L, 1);
    int y0 = luaL_checkinteger(L, 2);
    int x1 = luaL_checkinteger(L, 3);
    int y1 = luaL_checkinteger(L, 4);
    Color color = optColor(L, 5);
    im::drawLine(x0, y0, x1, y1, color);
    return 0;
}
// 0x0813c360
static int ImmediateMode_drawLineAntialiased(lua_State* L)
{
    checkDrawing(L);
    int x0 = luaL_checkinteger(L, 1);
    int y0 = luaL_checkinteger(L, 2);
    int x1 = luaL_checkinteger(L, 3);
    int y1 = luaL_checkinteger(L, 4);
    Color color = optColor(L, 5);
    im::drawLineAA(x0, y0, x1, y1, color);
    return 0;
}
// 0x0813c270: drawRect(x, y, width, height [, color])
static int ImmediateMode_drawRect(lua_State* L)
{
    checkDrawing(L);
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    Color color = optColor(L, 5);
    im::drawRect(x, y, width, height, color);
    return 0;
}
// 0x081419b0: drawImage(texture, x, y, srcX, srcY, srcWidth, srcHeight, width, height, color [,
// flags])
//             drawImage(texture, x, y, srcX, srcY, srcWidth, srcHeight, color [, flags])
//             drawImage(texture, x, y [, color [, flags]])
static int ImmediateMode_drawImage(lua_State* L)
{
    checkDrawing(L);
    int n = lua_gettop(L);
    if (n > 9)
    {
        RenderableTexture* texture = luax::checkObject<RenderableTexture>(L, 1);
        int x = luaL_checkinteger(L, 2);
        int y = luaL_checkinteger(L, 3);
        int srcX = luaL_checkinteger(L, 4);
        int srcY = luaL_checkinteger(L, 5);
        int srcWidth = luaL_checkinteger(L, 6);
        int srcHeight = luaL_checkinteger(L, 7);
        int width = luaL_checkinteger(L, 8);
        int height = luaL_checkinteger(L, 9);
        Color color = luax::checkColor(L, 10);
        int flags = 0;
        if (n != 10)
            flags = luaL_checkinteger(L, 11);
        im::drawImage(*texture, x, y, srcX, srcY, srcWidth, srcHeight, width, height, color, flags);
        return 0;
    }
    if (n > 7)
    {
        RenderableTexture* texture = luax::checkObject<RenderableTexture>(L, 1);
        int x = luaL_checkinteger(L, 2);
        int y = luaL_checkinteger(L, 3);
        int srcX = luaL_checkinteger(L, 4);
        int srcY = luaL_checkinteger(L, 5);
        int srcWidth = luaL_checkinteger(L, 6);
        int srcHeight = luaL_checkinteger(L, 7);
        Color color = luax::checkColor(L, 8);
        int flags = 0;
        if (n == 9)
            flags = luaL_checkinteger(L, 9);
        im::drawImage(*texture, x, y, srcX, srcY, srcWidth, srcHeight, color, flags);
        return 0;
    }
    RenderableTexture* texture = luax::checkObject<RenderableTexture>(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    Color color = Color::White;
    int flags = 0;
    if (n > 3)
    {
        color = luax::checkColor(L, 4);
        if (n != 4)
            flags = luaL_checkinteger(L, 5);
    }
    im::drawImage(*texture, x, y, color, flags);
    return 0;
}
// 0x0813da10: drawText(text, x, y, font [, color])
static int ImmediateMode_drawText(lua_State* L)
{
    checkDrawing(L);
    const char* text = luaL_checkstring(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    Font* font = luax::checkObject<Font>(L, 4);
    Color color = optColor(L, 5);
    im::drawText(text, x, y, font, color, 0x7fffffff);
    return 0;
}
// 0x0813c180
static int ImmediateMode_fillRect(lua_State* L)
{
    checkDrawing(L);
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    Color color = optColor(L, 5);
    im::fillRect(x, y, width, height, color);
    return 0;
}
// 0x0813c080: fillRoundedRect(x, y, width, height, radius [, color])
static int ImmediateMode_fillRoundedRect(lua_State* L)
{
    checkDrawing(L);
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    float radius = (float)luaL_checknumber(L, 5);
    Color color = optColor(L, 6);
    im::fillRoundedRect(x, y, width, height, radius, color);
    return 0;
}
// 0x0813bf80
static int ImmediateMode_fillRoundedRectAntialiased(lua_State* L)
{
    checkDrawing(L);
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    float radius = (float)luaL_checknumber(L, 5);
    Color color = optColor(L, 6);
    im::fillRoundedRectAA(x, y, width, height, radius, color);
    return 0;
}
// 0x081417f0: fillPolygon({vec, vec, ...} [, color])
static int ImmediateMode_fillPolygon(lua_State* L)
{
    if (!g_imdebugDraw)
        luaL_error(L, "beginDraw not called");
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)lua_objlen(L, 1);
    Array<Vec2> points;
    points.resize(n);
    for (int i = 0; i < n; ++i)
    {
        lua_rawgeti(L, 1, i + 1);
        if (lua_type(L, -1) != LUA_TTABLE)
            luaL_typerror(L, -1, "vec");
        lua_rawgeti(L, -1, 1);
        float x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 2);
        float y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        points[i] = Vec2(x, y);
        lua_pop(L, 1);
    }
    Color color = optColor(L, 2);
    im::fillPolygon(points.data(), n, color);
    return 0;
}
// 0x0814d850: beginShape(type, threeDee)
static int ImmediateMode_beginShape(lua_State* L)
{
    luax::Enum shapes[] = {{"points", 0},    {"lines", 1}, {"lines_antialiased", 2},
                           {"triangles", 3}, {"quads", 4}, {0, 0}};
    int type = luax::checkEnum(L, 1, shapes);
    bool threeDee = luax::checkBool(L, 2);
    im::beginShape(type, threeDee);
    return 0;
}
// 0x0813bf30
static int ImmediateMode_vertexPosition(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    im::vertexPosition(x, y);
    return 0;
}
// 0x0813bef0
static int ImmediateMode_vertexColor(lua_State* L)
{
    im::vertexColor(luax::checkColor(L, 1));
    return 0;
}
// 0x0813bea0
static int ImmediateMode_vertexTexcoord(lua_State* L)
{
    float u = (float)luaL_checknumber(L, 1);
    float v = (float)luaL_checknumber(L, 2);
    im::vertexTexcoord(u, v);
    return 0;
}
// 0x0813be90
static int ImmediateMode_endShape(lua_State* L)
{
    im::endShape();
    return 0;
}

const luaL_Reg ImmediateMode_methods[] = {
    {"setTransform", ImmediateMode_setTransform},
    {"setIdentityTransform", ImmediateMode_setIdentityTransform},
    {"setLineWidth", ImmediateMode_setLineWidth},
    {"setBlendMode", ImmediateMode_setBlendMode},
    {"clipTo", ImmediateMode_clipTo},
    {"resetClip", ImmediateMode_resetClip},
    {"getClipRect", ImmediateMode_getClipRect},
    {"pushState", ImmediateMode_pushState},
    {"popState", ImmediateMode_popState},
    {"beginDraw", ImmediateMode_beginDraw},
    {"endDraw", ImmediateMode_endDraw},
    {"drawPoint", ImmediateMode_drawPoint},
    {"drawLine", ImmediateMode_drawLine},
    {"drawLineAntialiased", ImmediateMode_drawLineAntialiased},
    {"drawRect", ImmediateMode_drawRect},
    {"drawImage", ImmediateMode_drawImage},
    {"drawText", ImmediateMode_drawText},
    {"fillRect", ImmediateMode_fillRect},
    {"fillRoundedRect", ImmediateMode_fillRoundedRect},
    {"fillRoundedRectAntialiased", ImmediateMode_fillRoundedRectAntialiased},
    {"fillPolygon", ImmediateMode_fillPolygon},
    {"beginShape", ImmediateMode_beginShape},
    {"vertexPosition", ImmediateMode_vertexPosition},
    {"vertexColor", ImmediateMode_vertexColor},
    {"vertexTexcoord", ImmediateMode_vertexTexcoord},
    {"endShape", ImmediateMode_endShape},
    {0, 0}};

// ---- Graphics --------------------------------------------------------------------

// 0x0813a5a0: setRenderTarget(texture | nil)
static int Graphics_setRenderTarget(lua_State* L)
{
    RenderableTexture* target = luax::checkObjectOpt<RenderableTexture>(L, 1);
    Graphics::sm_pActive->setRenderTarget(target);
    return 0;
}
// 0x08138de0: clears to transparent black
static int Graphics_clear(lua_State* L)
{
    Graphics::sm_pActive->clear(Color(0, 0, 0, 0));
    return 0;
}
// 0x08138e10
static int Graphics_drawRect(lua_State* L)
{
    Graphics::sm_pActive->drawRect();
    return 0;
}
// 0x0813fac0: blit(source, target, material)
static int Graphics_blit(lua_State* L)
{
    RenderableTexture* source = luax::checkObject<RenderableTexture>(L, 1);
    RenderableTexture* target = luax::checkObjectOpt<RenderableTexture>(L, 2);
    Material* material = luax::checkObjectOpt<Material>(L, 3);
    Graphics::sm_pActive->blit(*source, target, material);
    return 0;
}

const luaL_Reg Graphics_methods[] = {{"setRenderTarget", Graphics_setRenderTarget},
                                     {"clear", Graphics_clear},
                                     {"drawRect", Graphics_drawRect},
                                     {"blit", Graphics_blit},
                                     {0, 0}};
