#pragma once
#include "Base.hpp"
#include "../Engine.hpp"
#include "../../game_object/Base.hpp"
#include "../../game_object/Rigidbody.hpp"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            class GameObject : public Base
            {
            public:
                GameObject(SDL_Renderer *renderer, AssetPack *assets)
                    : Base("bokken/gameObject"), m_renderer(renderer), m_assets(assets) {}

                int declare(JSContext *ctx, JSModuleDef *m) override;
                int init(JSContext *ctx, JSModuleDef *m) override;

                static void update(float dt);
                static void fixedUpdate(float dt);
                static void render();

            private:
                SDL_Renderer *m_renderer;
                AssetPack *m_assets;

                static inline JSClassID s_class_id = 0;
                static inline JSClassID s_rigidbody_class_id = 0;
                static inline JSClassID s_transform_class_id = 0;

                static inline SDL_Renderer *s_renderer = nullptr;

                static JSValue js_constructor(JSContext *, JSValueConst, int, JSValueConst *);
                static JSValue js_add_component(JSContext *, JSValueConst, int, JSValueConst *);
                static JSValue js_get_component(JSContext *, JSValueConst, int, JSValueConst *);
                static JSValue js_set_parent(JSContext *, JSValueConst, int, JSValueConst *);
                static JSValue js_get_children(JSContext *, JSValueConst, int, JSValueConst *);
                static JSValue js_destroy(JSContext *, JSValueConst, int, JSValueConst *);
                static JSValue js_find(JSContext *, JSValueConst, int, JSValueConst *);

                static JSValue wrap_rigidbody(JSContext *ctx, Bokken::GameObject::Rigidbody *rb);
                static JSValue wrap_transform(JSContext *ctx, Bokken::GameObject::Transform *t);
            };
        }
    }
}