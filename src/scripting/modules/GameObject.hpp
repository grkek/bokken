#pragma once

#include "Base.hpp"
#include "../Engine.hpp"
#include "../../AssetPack.hpp"
#include "../../game_object/Base.hpp"
#include "../../game_object/Camera2D.hpp"
#include "../../game_object/Mesh2D.hpp"
#include "../../game_object/ParticleEmitter2D.hpp"
#include "../../game_object/Rigidbody2D.hpp"
#include "../../game_object/Shape2D.hpp"
#include "../../game_object/Sprite2D.hpp"
#include "../../game_object/Animation2D.hpp"
#include "../../game_object/Distortion2D.hpp"
#include "../../renderer/SpriteBatcher.hpp"
#include "../../renderer/TextureCache.hpp"
#include "../../game_object/Transform2D.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            // Bridges the native GameObject layer into JS via "bokken/gameObject".
            //
            // addComponent returns `this` for chaining and accepts an optional
            // config object as a second argument:
            //
            //   player
            //       .addComponent(Transform2D, { positionX: 5 })
            //       .addComponent(Mesh2D, { shape: Shape2D.Quad, colorR: 1.0 })
            //       .addComponent(Rigidbody2D, { mass: 2 });
            //
            // getComponent returns the wrapped component handle.
            class GameObject : public Base
            {
            public:
                GameObject(SDL_Window *window, AssetPack *assets)
                    : Base("bokken/gameObject"), m_window(window), m_assets(assets) {}

                // Wired in by the Renderer module before the first frame.
                static void setBatcher(Bokken::Renderer::SpriteBatcher *b) { s_batcher = b; }
                static void setTextureCache(Bokken::Renderer::TextureCache *tc) { s_textures = tc; }

                int declare(JSContext *ctx, JSModuleDef *m) override;
                int init(JSContext *ctx, JSModuleDef *m) override;

                static void update(float deltaTime);
                static void fixedUpdate(float deltaTime);
                static void present();

                // QuickJS C callbacks and helpers — public because file-scope
                // static function list arrays need to reference them directly.
                static inline JSClassID s_class_id = 0;

                static inline JSClassID s_camera2d_class_id = 0;
                static inline JSClassID s_mesh2d_class_id = 0;
                static inline JSClassID s_particle2d_class_id = 0;
                static inline JSClassID s_sprite2d_class_id = 0;
                static inline JSClassID s_animation2d_class_id = 0;
                static inline JSClassID s_distortion2d_class_id = 0;
                static inline JSClassID s_transform2d_class_id = 0;
                static inline JSClassID s_rigidbody2d_class_id = 0;

                static inline SDL_Window *s_window = nullptr;
                static inline Bokken::Renderer::SpriteBatcher *s_batcher = nullptr;
                static inline Bokken::Renderer::TextureCache *s_textures = nullptr;

                static JSValue js_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
                static JSValue js_add_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_get_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_set_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_get_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_find(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_get_destroy_when_idle(JSContext *ctx, JSValueConst this_val);
                static JSValue js_set_destroy_when_idle(JSContext *ctx, JSValueConst this_val, JSValueConst val);

                static JSValue js_camera2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_camera2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue wrap_camera2d(JSContext *ctx, Bokken::GameObject::Camera2D *cam);

                static JSValue js_particle2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_particle2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue js_particle2d_burst(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue wrap_particle2d(JSContext *ctx, Bokken::GameObject::ParticleEmitter2D *em);

                static JSValue js_transform2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_transform2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue js_transform2d_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_transform2d_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                static JSValue js_rigidbody2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_rigidbody2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue js_rigidbody2d_apply_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_rigidbody2d_apply_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_rigidbody2d_set_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                static JSValue js_mesh2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_mesh2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);

                static JSValue js_sprite2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_sprite2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue wrap_sprite2d(JSContext *ctx, Bokken::GameObject::Sprite2D *sprite);

                static JSValue js_animation2d_play(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_animation2d_pause(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_animation2d_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_animation2d_resume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_animation2d_add_clip(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_animation2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue wrap_animation2d(JSContext *ctx, Bokken::GameObject::Animation2D *anim);

                static JSValue js_distortion2d_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_distortion2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue js_distortion2d_trigger(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue wrap_distortion2d(JSContext *ctx, Bokken::GameObject::Distortion2D *dist);

                static JSValue wrap_transform2d(JSContext *ctx, Bokken::GameObject::Transform2D *t);
                static JSValue wrap_rigidbody2d(JSContext *ctx, Bokken::GameObject::Rigidbody2D *rb);
                static JSValue wrap_mesh2d(JSContext *ctx, Bokken::GameObject::Mesh2D *mesh);
                static void apply_props(JSContext *ctx, JSValue target, JSValue props);
                static JSValue make_vec2(JSContext *ctx, const glm::vec2 &v);
                static bool read_vec2(JSContext *ctx, JSValueConst val, glm::vec2 &out);
                static Bokken::GameObject::Shape2D parse_shape2d(const char *name);
                static const char *shape2d_to_string(Bokken::GameObject::Shape2D shape);

            private:
                SDL_Window *m_window;
                AssetPack *m_assets;
            };
        }
    }
}