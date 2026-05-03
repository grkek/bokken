#include "GameObject.hpp"

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            int GameObject::declare(JSContext *ctx, JSModuleDef *m)
            {
                JS_NewClassID(JS_GetRuntime(ctx), &s_class_id);
                JS_NewClassID(JS_GetRuntime(ctx), &s_rigidbody_class_id);
                JS_NewClassID(JS_GetRuntime(ctx), &s_transform_class_id);

                static JSClassDef go_class = {"GameObject", .finalizer = nullptr};
                static JSClassDef rb_class = {"Rigidbody", .finalizer = nullptr};
                static JSClassDef tr_class = {"Transform", .finalizer = nullptr};

                JS_NewClass(JS_GetRuntime(ctx), s_class_id, &go_class);
                JS_NewClass(JS_GetRuntime(ctx), s_rigidbody_class_id, &rb_class);
                JS_NewClass(JS_GetRuntime(ctx), s_transform_class_id, &tr_class);

                JS_AddModuleExport(ctx, m, "Mesh");

                return JS_AddModuleExport(ctx, m, "GameObject");
            }

            int GameObject::init(JSContext *ctx, JSModuleDef *m)
            {
                s_renderer = m_renderer;

                JSValue go_proto = JS_NewObject(ctx);
                const JSCFunctionListEntry go_proto_funcs[] = {
                    JS_CFUNC_DEF("addComponent", 1, GameObject::js_add_component),
                    JS_CFUNC_DEF("getComponent", 1, GameObject::js_get_component),
                    JS_CFUNC_DEF("setParent", 1, GameObject::js_set_parent),
                    JS_CFUNC_DEF("getChildren", 0, GameObject::js_get_children),
                };
                JS_SetPropertyFunctionList(ctx, go_proto, go_proto_funcs, sizeof(go_proto_funcs) / sizeof(JSCFunctionListEntry));

                JSValue go_ctor = JS_NewCFunction2(ctx, GameObject::js_constructor, "GameObject", 1, JS_CFUNC_constructor, 0);
                const JSCFunctionListEntry go_static_funcs[] = {
                    JS_CFUNC_DEF("destroy", 1, GameObject::js_destroy),
                    JS_CFUNC_DEF("find", 1, GameObject::js_find),
                };
                JS_SetPropertyFunctionList(ctx, go_ctor, go_static_funcs, sizeof(go_static_funcs) / sizeof(JSCFunctionListEntry));

                JS_SetConstructor(ctx, go_ctor, go_proto);
                JS_SetClassProto(ctx, s_class_id, go_proto);

                JSValue tr_proto = JS_NewObject(ctx);
                // Example: JS_SetPropertyStr(ctx, tr_proto, "translate", JS_NewCFunction(...));
                JS_SetClassProto(ctx, s_transform_class_id, tr_proto);

                JSValue rb_proto = JS_NewObject(ctx);
                // Example: JS_SetPropertyStr(ctx, rb_proto, "applyForce", JS_NewCFunction(...));
                JS_SetClassProto(ctx, s_rigidbody_class_id, rb_proto);

                // Mesh enum
                JSValue mesh = JS_NewObject(ctx);

                JS_SetPropertyStr(ctx, mesh, "Empty", JS_NewString(ctx, "Empty"));
                JS_SetPropertyStr(ctx, mesh, "Cube", JS_NewString(ctx, "Cube"));
                JS_SetPropertyStr(ctx, mesh, "Sphere", JS_NewString(ctx, "Sphere"));
                JS_SetPropertyStr(ctx, mesh, "Plane", JS_NewString(ctx, "Plane"));

                JS_SetModuleExport(ctx, m, "Mesh", mesh);
                JS_SetModuleExport(ctx, m, "GameObject", go_ctor);

                return 0;
            }

            // Placeholder for get_children if not yet implemented
            JSValue GameObject::js_get_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go)
                    return JS_UNDEFINED;

                JSValue arr = JS_NewArray(ctx);
                auto children = go->getChildren();
                for (size_t i = 0; i < children.size(); ++i)
                {
                    JSValue child_obj = JS_NewObjectClass(ctx, s_class_id);
                    JS_SetOpaque(child_obj, children[i]);
                    JS_SetPropertyUint32(ctx, arr, i, child_obj);
                }
                return arr;
            }

            // JS constructor: new GameObject(name?)
            JSValue GameObject::js_constructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_class_id);
                if (JS_IsException(obj))
                    return obj;

                std::string name = "Untitled";
                if (argc > 0 && JS_IsString(argv[0]))
                {
                    const char *s = JS_ToCString(ctx, argv[0]);
                    name = s;
                    JS_FreeCString(ctx, s);
                }

                // Create the engine object and register it in the engine registry
                auto go = std::make_unique<Bokken::GameObject::Base>(name);
                Bokken::GameObject::Base *raw = go.get();
                Bokken::GameObject::Base::s_objects.push_back(std::move(go));

                JS_SetOpaque(obj, raw);
                return obj;
            }

            // addComponent(componentClass)
            JSValue GameObject::js_add_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                JSValue nameVal = JS_GetPropertyStr(ctx, argv[0], "name");
                const char *className = JS_ToCString(ctx, nameVal);
                JS_FreeValue(ctx, nameVal);

                JSValue result = JS_UNDEFINED;

                if (strcmp(className, "Rigidbody") == 0)
                {
                    auto &rb = go->addComponent<Bokken::GameObject::Rigidbody>();
                    result = wrap_rigidbody(ctx, &rb);
                }

                JS_FreeCString(ctx, className);
                return result;
            }

            // getComponent(componentClass)
            JSValue GameObject::js_get_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                JSValue nameVal = JS_GetPropertyStr(ctx, argv[0], "name");
                const char *className = JS_ToCString(ctx, nameVal);
                JS_FreeValue(ctx, nameVal);

                JSValue result = JS_UNDEFINED;

                if (strcmp(className, "Rigidbody") == 0)
                {
                    auto *rb = go->getComponent<Bokken::GameObject::Rigidbody>();
                    result = rb ? wrap_rigidbody(ctx, rb) : JS_UNDEFINED;
                }
                else if (strcmp(className, "Transform") == 0)
                {
                    result = wrap_transform(ctx, go->transform);
                }

                JS_FreeCString(ctx, className);
                return result;
            }

            // setParent(parent | null)
            JSValue GameObject::js_set_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                if (JS_IsNull(argv[0]))
                {
                    go->setParent(nullptr);
                }
                else
                {
                    auto *parent = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(argv[0], s_class_id));
                    go->setParent(parent);
                }
                return JS_UNDEFINED;
            }

            // static destroy(obj)
            JSValue GameObject::js_destroy(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (argc < 1)
                    return JS_UNDEFINED;
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(argv[0], s_class_id));
                if (go)
                    Bokken::GameObject::Base::destroy(go);
                return JS_UNDEFINED;
            }

            // static find(name)
            JSValue GameObject::js_find(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (argc < 1)
                    return JS_UNDEFINED;
                const char *name = JS_ToCString(ctx, argv[0]);
                Bokken::GameObject::Base *go = Bokken::GameObject::Base::find(name);
                JS_FreeCString(ctx, name);

                if (!go)
                    return JS_UNDEFINED;

                // Re-wrap the existing engine object in a new JS handle
                JSValue obj = JS_NewObjectClass(ctx, s_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, go);
                return obj;
            }

            static JSClassDef s_rigidbody_class = {"Rigidbody", .finalizer = nullptr};
            static JSClassDef s_transform_class = {"Transform", .finalizer = nullptr};

            JSValue GameObject::wrap_rigidbody(JSContext *ctx, Bokken::GameObject::Rigidbody *rb)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_rigidbody_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, rb);
                return obj;
            }

            JSValue GameObject::wrap_transform(JSContext *ctx, Bokken::GameObject::Transform *t)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_transform_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, t);
                return obj;
            }

            // Engine hooks — drive the engine registry, not the module
            void GameObject::update(float dt)
            {
                for (auto &go : Bokken::GameObject::Base::s_objects)
                    go->update(dt);

                Bokken::GameObject::Base::flushDestroyed();
            }

            void GameObject::fixedUpdate(float dt)
            {
                for (auto &go : Bokken::GameObject::Base::s_objects)
                    go->fixedUpdate(dt);
            }

            void GameObject::render()
            {
                if (!s_renderer)
                    return;

                int w, h;
                SDL_GetWindowSize(SDL_GetRenderWindow(s_renderer), &w, &h);

                glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)w / (float)h, 0.1f, 100.0f);
                glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -10));

                for (auto &go : Bokken::GameObject::Base::s_objects)
                {
                    auto *t = go->transform;
                    if (!t || t->mesh == Bokken::GameObject::Mesh::Empty)
                        continue;

                    glm::mat4 mvp = projection * view * t->getMatrix();

                    // TODO: Draw based on mesh type
                }
            }
        }
    }
}
