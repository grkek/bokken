#pragma once

#include "Base.hpp"
#include "../Engine.hpp"
#include "../../game_object/Base.hpp"
#include "../../game_object/Mesh.hpp"
#include "../../game_object/Rigidbody.hpp"
#include "../../game_object/Transform.hpp"
#include "../../AssetPack.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            /*
             * Bridges Bokken's native GameObject layer (engine-side scene graph,
             * components, transforms, physics) into the JS scripting runtime.
             *
             * Exposes the "bokken/gameObject" module with:
             *   - GameObject class (constructor, addComponent, getComponent,
             *     setParent, getChildren, static destroy, static find).
             *   - Mesh enum (Empty, Cube, Sphere, Plane).
             *   - Transform / Rigidbody as opaque-wrapped component handles.
             *
             * Engine integration:
             *   The static update / fixedUpdate / render hooks must be called from
             *   the main Loop each frame so the registered objects tick correctly.
             */
            class GameObject : public Base
            {
            public:
                /**
                 * Construct the bokken/gameObject scripting module.
                 *
                 * Post-GL refactor we no longer hold a renderer — render
                 * is currently a no-op (the 2D path is the Canvas tree;
                 * 3D mesh rendering will land in a future MeshStage).
                 * Window is kept for aspect-ratio queries.
                 */
                GameObject(SDL_Window *window, AssetPack *assets)
                    : Base("bokken/gameObject"), m_window(window), m_assets(assets) {}

                int declare(JSContext *ctx, JSModuleDef *m) override;
                int init(JSContext *ctx, JSModuleDef *m) override;

                /* Per-frame update — drives every registered GameObject's components. */
                static void update(float deltaTime);

                /* Fixed-step update — primarily used by Rigidbody integration. */
                static void fixedUpdate(float deltaTime);

                /* Render pass — placeholder until 3D MeshStage lands. */
                static void render();

            private:
                SDL_Window *m_window;
                AssetPack *m_assets;

                static inline JSClassID s_class_id = 0;
                static inline JSClassID s_rigidbody_class_id = 0;
                static inline JSClassID s_transform_class_id = 0;

                static inline SDL_Window *s_window = nullptr;

                /* GameObject class — constructor & instance / static methods. */
                static JSValue js_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
                static JSValue js_add_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_get_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_set_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_get_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_find(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                /* Transform — getters / setters / methods. */
                static JSValue js_transform_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_transform_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue js_transform_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_transform_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                /* Rigidbody — getters / setters / methods. */
                static JSValue js_rigidbody_get(JSContext *ctx, JSValueConst this_val, int magic);
                static JSValue js_rigidbody_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
                static JSValue js_rigidbody_apply_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
                static JSValue js_rigidbody_set_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

                /* Helpers. */
                static JSValue wrap_rigidbody(JSContext *ctx, Bokken::GameObject::Rigidbody *rb);
                static JSValue wrap_transform(JSContext *ctx, Bokken::GameObject::Transform *t);
                static JSValue make_vec3(JSContext *ctx, const glm::vec3 &v);
                static bool read_vec3(JSContext *ctx, JSValueConst val, glm::vec3 &out);
                static Bokken::GameObject::Mesh parse_mesh(const char *name);
                static const char *mesh_to_string(Bokken::GameObject::Mesh mesh);
            };
        }
    }
}