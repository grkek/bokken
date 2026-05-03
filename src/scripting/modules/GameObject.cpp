#include "GameObject.hpp"

#include <cstring>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            /* Property magic numbers used by the Transform getter / setter pair.
             * Each enumerator names a property exposed on the JS Transform object.
             */
            enum TransformProperties
            {
                TP_PositionX = 0,
                TP_PositionY,
                TP_PositionZ,
                TP_RotationX,
                TP_RotationY,
                TP_RotationZ,
                TP_ScaleX,
                TP_ScaleY,
                TP_ScaleZ,
                TP_Mesh
            };

            /* Property magic numbers for Rigidbody — paired getter / setter handle
             * scalar fields and the velocity vector via dedicated entries.
             */
            enum RigidbodyProperties
            {
                RB_Mass = 0,
                RB_UseGravity,
                RB_IsStatic,
                RB_VelocityX,
                RB_VelocityY,
                RB_VelocityZ
            };

            /* Module declaration — registers exports & native classes with QuickJS. */
            int GameObject::declare(JSContext *ctx, JSModuleDef *m)
            {
                JSRuntime *rt = JS_GetRuntime(ctx);

                JS_NewClassID(rt, &s_class_id);
                JS_NewClassID(rt, &s_rigidbody_class_id);
                JS_NewClassID(rt, &s_transform_class_id);

                /* Note: finalizers are intentionally null. The C++ engine owns the
                 * lifetime of GameObject / Component instances via Base::s_objects;
                 * the JS handle is a non-owning view. */
                static JSClassDef goClass = {"GameObject", .finalizer = nullptr};
                static JSClassDef rbClass = {"Rigidbody", .finalizer = nullptr};
                static JSClassDef trClass = {"Transform", .finalizer = nullptr};

                JS_NewClass(rt, s_class_id, &goClass);
                JS_NewClass(rt, s_rigidbody_class_id, &rbClass);
                JS_NewClass(rt, s_transform_class_id, &trClass);

                JS_AddModuleExport(ctx, m, "Mesh");
                JS_AddModuleExport(ctx, m, "Rigidbody");
                JS_AddModuleExport(ctx, m, "Transform");

                return JS_AddModuleExport(ctx, m, "GameObject");
            }

            /* Module initialisation — populates the declared exports with values. */
            int GameObject::init(JSContext *ctx, JSModuleDef *m)
            {
                s_window = m_window;

                /* GameObject prototype. */
                JSValue goProto = JS_NewObject(ctx);
                const JSCFunctionListEntry goProtoFuncs[] = {
                    JS_CFUNC_DEF("addComponent", 1, GameObject::js_add_component),
                    JS_CFUNC_DEF("getComponent", 1, GameObject::js_get_component),
                    JS_CFUNC_DEF("setParent", 1, GameObject::js_set_parent),
                    JS_CFUNC_DEF("getChildren", 0, GameObject::js_get_children),
                };
                JS_SetPropertyFunctionList(ctx, goProto, goProtoFuncs,
                                           sizeof(goProtoFuncs) / sizeof(JSCFunctionListEntry));

                JSValue goCtor = JS_NewCFunction2(ctx, GameObject::js_constructor,
                                                  "GameObject", 1, JS_CFUNC_constructor, 0);
                const JSCFunctionListEntry goStaticFuncs[] = {
                    JS_CFUNC_DEF("destroy", 1, GameObject::js_destroy),
                    JS_CFUNC_DEF("find", 1, GameObject::js_find),
                };
                JS_SetPropertyFunctionList(ctx, goCtor, goStaticFuncs,
                                           sizeof(goStaticFuncs) / sizeof(JSCFunctionListEntry));

                JS_SetConstructor(ctx, goCtor, goProto);
                JS_SetClassProto(ctx, s_class_id, goProto);

                /* Transform prototype — getters/setters via CGETSET_MAGIC,
                 * methods via CFUNC_DEF. */
                JSValue trProto = JS_NewObject(ctx);
                const JSCFunctionListEntry trProtoFuncs[] = {
                    JS_CGETSET_MAGIC_DEF("positionX", js_transform_get, js_transform_set, TP_PositionX),
                    JS_CGETSET_MAGIC_DEF("positionY", js_transform_get, js_transform_set, TP_PositionY),
                    JS_CGETSET_MAGIC_DEF("positionZ", js_transform_get, js_transform_set, TP_PositionZ),
                    JS_CGETSET_MAGIC_DEF("rotationX", js_transform_get, js_transform_set, TP_RotationX),
                    JS_CGETSET_MAGIC_DEF("rotationY", js_transform_get, js_transform_set, TP_RotationY),
                    JS_CGETSET_MAGIC_DEF("rotationZ", js_transform_get, js_transform_set, TP_RotationZ),
                    JS_CGETSET_MAGIC_DEF("scaleX", js_transform_get, js_transform_set, TP_ScaleX),
                    JS_CGETSET_MAGIC_DEF("scaleY", js_transform_get, js_transform_set, TP_ScaleY),
                    JS_CGETSET_MAGIC_DEF("scaleZ", js_transform_get, js_transform_set, TP_ScaleZ),
                    JS_CGETSET_MAGIC_DEF("mesh", js_transform_get, js_transform_set, TP_Mesh),
                    JS_CFUNC_DEF("translate", 3, GameObject::js_transform_translate),
                    JS_CFUNC_DEF("rotate", 3, GameObject::js_transform_rotate),
                };
                JS_SetPropertyFunctionList(ctx, trProto, trProtoFuncs,
                                           sizeof(trProtoFuncs) / sizeof(JSCFunctionListEntry));
                JS_SetClassProto(ctx, s_transform_class_id, trProto);

                /* Rigidbody prototype. */
                JSValue rbProto = JS_NewObject(ctx);
                const JSCFunctionListEntry rbProtoFuncs[] = {
                    JS_CGETSET_MAGIC_DEF("mass", js_rigidbody_get, js_rigidbody_set, RB_Mass),
                    JS_CGETSET_MAGIC_DEF("useGravity", js_rigidbody_get, js_rigidbody_set, RB_UseGravity),
                    JS_CGETSET_MAGIC_DEF("isStatic", js_rigidbody_get, js_rigidbody_set, RB_IsStatic),
                    JS_CGETSET_MAGIC_DEF("velocityX", js_rigidbody_get, js_rigidbody_set, RB_VelocityX),
                    JS_CGETSET_MAGIC_DEF("velocityY", js_rigidbody_get, js_rigidbody_set, RB_VelocityY),
                    JS_CGETSET_MAGIC_DEF("velocityZ", js_rigidbody_get, js_rigidbody_set, RB_VelocityZ),
                    JS_CFUNC_DEF("applyForce", 3, GameObject::js_rigidbody_apply_force),
                    JS_CFUNC_DEF("setVelocity", 3, GameObject::js_rigidbody_set_velocity),
                };
                JS_SetPropertyFunctionList(ctx, rbProto, rbProtoFuncs,
                                           sizeof(rbProtoFuncs) / sizeof(JSCFunctionListEntry));
                JS_SetClassProto(ctx, s_rigidbody_class_id, rbProto);

                /* Sentinel exports — Rigidbody / Transform are passed by name to
                 * addComponent / getComponent. JS code uses `Component.name` to
                 * resolve them, so we hand back named function objects here. */
                JSValue rbToken = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, rbToken, "name", JS_NewString(ctx, "Rigidbody"));

                JSValue trToken = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, trToken, "name", JS_NewString(ctx, "Transform"));

                /* Mesh enum. */
                JSValue mesh = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, mesh, "Empty", JS_NewString(ctx, "Empty"));
                JS_SetPropertyStr(ctx, mesh, "Cube", JS_NewString(ctx, "Cube"));
                JS_SetPropertyStr(ctx, mesh, "Sphere", JS_NewString(ctx, "Sphere"));
                JS_SetPropertyStr(ctx, mesh, "Plane", JS_NewString(ctx, "Plane"));

                JS_SetModuleExport(ctx, m, "GameObject", goCtor);
                JS_SetModuleExport(ctx, m, "Mesh", mesh);
                JS_SetModuleExport(ctx, m, "Rigidbody", rbToken);
                JS_SetModuleExport(ctx, m, "Transform", trToken);

                return 0;
            }

            /* JS: new GameObject(name?) */
            JSValue GameObject::js_constructor(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_class_id);
                if (JS_IsException(obj))
                    return obj;

                std::string name = "Untitled";
                if (argc > 0 && JS_IsString(argv[0]))
                {
                    const char *s = JS_ToCString(ctx, argv[0]);
                    if (s)
                    {
                        name = s;
                        JS_FreeCString(ctx, s);
                    }
                }

                /* Create the engine object and register it in the engine registry. */
                auto go = std::make_unique<Bokken::GameObject::Base>(name);
                Bokken::GameObject::Base *raw = go.get();
                Bokken::GameObject::Base::s_objects.push_back(std::move(go));

                JS_SetOpaque(obj, raw);
                return obj;
            }

            /* JS: gameObject.addComponent(ComponentClass) */
            JSValue GameObject::js_add_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                JSValue nameVal = JS_GetPropertyStr(ctx, argv[0], "name");
                const char *className = JS_ToCString(ctx, nameVal);
                JS_FreeValue(ctx, nameVal);

                JSValue result = JS_UNDEFINED;
                if (className && strcmp(className, "Rigidbody") == 0)
                {
                    auto &rb = go->addComponent<Bokken::GameObject::Rigidbody>();
                    result = wrap_rigidbody(ctx, &rb);
                }

                if (className)
                    JS_FreeCString(ctx, className);
                return result;
            }

            /* JS: gameObject.getComponent(ComponentClass) */
            JSValue GameObject::js_get_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                JSValue nameVal = JS_GetPropertyStr(ctx, argv[0], "name");
                const char *className = JS_ToCString(ctx, nameVal);
                JS_FreeValue(ctx, nameVal);

                JSValue result = JS_UNDEFINED;
                if (className)
                {
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
                }
                return result;
            }

            /* JS: gameObject.setParent(parent | null) */
            JSValue GameObject::js_set_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0]))
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

            /* JS: gameObject.getChildren() */
            JSValue GameObject::js_get_children(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go)
                    return JS_UNDEFINED;

                JSValue arr = JS_NewArray(ctx);
                auto children = go->getChildren();
                for (size_t i = 0; i < children.size(); ++i)
                {
                    JSValue childObj = JS_NewObjectClass(ctx, s_class_id);
                    JS_SetOpaque(childObj, children[i]);
                    JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), childObj);
                }
                return arr;
            }

            /* JS: GameObject.destroy(obj) */
            JSValue GameObject::js_destroy(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (argc < 1)
                    return JS_UNDEFINED;
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(argv[0], s_class_id));
                if (go)
                    Bokken::GameObject::Base::destroy(go);
                return JS_UNDEFINED;
            }

            /* JS: GameObject.find(name) */
            JSValue GameObject::js_find(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (argc < 1)
                    return JS_UNDEFINED;

                const char *name = JS_ToCString(ctx, argv[0]);
                if (!name)
                    return JS_UNDEFINED;

                Bokken::GameObject::Base *go = Bokken::GameObject::Base::find(name);
                JS_FreeCString(ctx, name);

                if (!go)
                    return JS_UNDEFINED;

                /* Re-wrap the existing engine object in a new JS handle. */
                JSValue obj = JS_NewObjectClass(ctx, s_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, go);
                return obj;
            }

            /* Transform property getter — dispatched by magic. */
            JSValue GameObject::js_transform_get(JSContext *ctx, JSValueConst this_val, int magic)
            {
                auto *t = static_cast<Bokken::GameObject::Transform *>(JS_GetOpaque(this_val, s_transform_class_id));
                if (!t)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case TP_PositionX:
                    return JS_NewFloat64(ctx, t->position.x);
                case TP_PositionY:
                    return JS_NewFloat64(ctx, t->position.y);
                case TP_PositionZ:
                    return JS_NewFloat64(ctx, t->position.z);
                case TP_RotationX:
                    return JS_NewFloat64(ctx, t->rotation.x);
                case TP_RotationY:
                    return JS_NewFloat64(ctx, t->rotation.y);
                case TP_RotationZ:
                    return JS_NewFloat64(ctx, t->rotation.z);
                case TP_ScaleX:
                    return JS_NewFloat64(ctx, t->scale.x);
                case TP_ScaleY:
                    return JS_NewFloat64(ctx, t->scale.y);
                case TP_ScaleZ:
                    return JS_NewFloat64(ctx, t->scale.z);
                case TP_Mesh:
                    return JS_NewString(ctx, mesh_to_string(t->mesh));
                }
                return JS_UNDEFINED;
            }

            /* Transform property setter. */
            JSValue GameObject::js_transform_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic)
            {
                auto *t = static_cast<Bokken::GameObject::Transform *>(JS_GetOpaque(this_val, s_transform_class_id));
                if (!t)
                    return JS_UNDEFINED;

                if (magic == TP_Mesh)
                {
                    const char *str = JS_ToCString(ctx, val);
                    if (str)
                    {
                        t->mesh = parse_mesh(str);
                        JS_FreeCString(ctx, str);
                    }
                    return JS_UNDEFINED;
                }

                double d = 0.0;
                if (JS_ToFloat64(ctx, &d, val) < 0)
                    return JS_EXCEPTION;
                float f = static_cast<float>(d);

                switch (magic)
                {
                case TP_PositionX:
                    t->position.x = f;
                    break;
                case TP_PositionY:
                    t->position.y = f;
                    break;
                case TP_PositionZ:
                    t->position.z = f;
                    break;
                case TP_RotationX:
                    t->rotation.x = f;
                    break;
                case TP_RotationY:
                    t->rotation.y = f;
                    break;
                case TP_RotationZ:
                    t->rotation.z = f;
                    break;
                case TP_ScaleX:
                    t->scale.x = f;
                    break;
                case TP_ScaleY:
                    t->scale.y = f;
                    break;
                case TP_ScaleZ:
                    t->scale.z = f;
                    break;
                }
                return JS_UNDEFINED;
            }

            /* JS: transform.translate(x, y, z) */
            JSValue GameObject::js_transform_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *t = static_cast<Bokken::GameObject::Transform *>(JS_GetOpaque(this_val, s_transform_class_id));
                if (!t || argc < 3)
                    return JS_UNDEFINED;

                double x, y, z;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0 ||
                    JS_ToFloat64(ctx, &z, argv[2]) < 0)
                    return JS_EXCEPTION;

                t->translate(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                return JS_UNDEFINED;
            }

            /* JS: transform.rotate(x, y, z) — Euler degrees. */
            JSValue GameObject::js_transform_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *t = static_cast<Bokken::GameObject::Transform *>(JS_GetOpaque(this_val, s_transform_class_id));
                if (!t || argc < 3)
                    return JS_UNDEFINED;

                double x, y, z;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0 ||
                    JS_ToFloat64(ctx, &z, argv[2]) < 0)
                    return JS_EXCEPTION;

                t->rotate(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                return JS_UNDEFINED;
            }

            /* Rigidbody property getter. */
            JSValue GameObject::js_rigidbody_get(JSContext *ctx, JSValueConst this_val, int magic)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody *>(JS_GetOpaque(this_val, s_rigidbody_class_id));
                if (!rb)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case RB_Mass:
                    return JS_NewFloat64(ctx, rb->mass);
                case RB_UseGravity:
                    return JS_NewBool(ctx, rb->useGravity);
                case RB_IsStatic:
                    return JS_NewBool(ctx, rb->isStatic);
                case RB_VelocityX:
                    return JS_NewFloat64(ctx, rb->velocity.x);
                case RB_VelocityY:
                    return JS_NewFloat64(ctx, rb->velocity.y);
                case RB_VelocityZ:
                    return JS_NewFloat64(ctx, rb->velocity.z);
                }
                return JS_UNDEFINED;
            }

            /* Rigidbody property setter. */
            JSValue GameObject::js_rigidbody_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody *>(JS_GetOpaque(this_val, s_rigidbody_class_id));
                if (!rb)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case RB_UseGravity:
                    rb->useGravity = JS_ToBool(ctx, val);
                    return JS_UNDEFINED;
                case RB_IsStatic:
                    rb->isStatic = JS_ToBool(ctx, val);
                    return JS_UNDEFINED;
                }

                double d = 0.0;
                if (JS_ToFloat64(ctx, &d, val) < 0)
                    return JS_EXCEPTION;
                float f = static_cast<float>(d);

                switch (magic)
                {
                case RB_Mass:
                    rb->mass = f;
                    break;
                case RB_VelocityX:
                    rb->velocity.x = f;
                    break;
                case RB_VelocityY:
                    rb->velocity.y = f;
                    break;
                case RB_VelocityZ:
                    rb->velocity.z = f;
                    break;
                }
                return JS_UNDEFINED;
            }

            /* JS: rigidbody.applyForce(x, y, z) */
            JSValue GameObject::js_rigidbody_apply_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody *>(JS_GetOpaque(this_val, s_rigidbody_class_id));
                if (!rb || argc < 3)
                    return JS_UNDEFINED;

                double x, y, z;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0 ||
                    JS_ToFloat64(ctx, &z, argv[2]) < 0)
                    return JS_EXCEPTION;

                rb->applyForce(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                return JS_UNDEFINED;
            }

            /* JS: rigidbody.setVelocity(x, y, z) */
            JSValue GameObject::js_rigidbody_set_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody *>(JS_GetOpaque(this_val, s_rigidbody_class_id));
                if (!rb || argc < 3)
                    return JS_UNDEFINED;

                double x, y, z;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0 ||
                    JS_ToFloat64(ctx, &z, argv[2]) < 0)
                    return JS_EXCEPTION;

                rb->setVelocity(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                return JS_UNDEFINED;
            }

            /* Wrap a native Rigidbody into a JS handle. */
            JSValue GameObject::wrap_rigidbody(JSContext *ctx, Bokken::GameObject::Rigidbody *rb)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_rigidbody_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, rb);
                return obj;
            }

            /* Wrap a native Transform into a JS handle. */
            JSValue GameObject::wrap_transform(JSContext *ctx, Bokken::GameObject::Transform *t)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_transform_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, t);
                return obj;
            }

            /* Build a {x, y, z} plain object — convenience for vector returns. */
            JSValue GameObject::make_vec3(JSContext *ctx, const glm::vec3 &v)
            {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, v.x));
                JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, v.y));
                JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat64(ctx, v.z));
                return obj;
            }

            /* Read a {x, y, z} plain object back into a glm::vec3. */
            bool GameObject::read_vec3(JSContext *ctx, JSValueConst val, glm::vec3 &out)
            {
                if (!JS_IsObject(val))
                    return false;

                auto readField = [&](const char *k, float &dst) -> bool
                {
                    JSValue v = JS_GetPropertyStr(ctx, val, k);
                    double d = 0.0;
                    int rc = JS_ToFloat64(ctx, &d, v);
                    JS_FreeValue(ctx, v);
                    if (rc < 0)
                        return false;
                    dst = static_cast<float>(d);
                    return true;
                };

                return readField("x", out.x) && readField("y", out.y) && readField("z", out.z);
            }

            /* Convert a JS string to the Mesh enum — defaults to Empty. */
            Bokken::GameObject::Mesh GameObject::parse_mesh(const char *name)
            {
                if (!name)
                    return Bokken::GameObject::Mesh::Empty;
                if (strcmp(name, "Cube") == 0)
                    return Bokken::GameObject::Mesh::Cube;
                if (strcmp(name, "Sphere") == 0)
                    return Bokken::GameObject::Mesh::Sphere;
                if (strcmp(name, "Plane") == 0)
                    return Bokken::GameObject::Mesh::Plane;
                return Bokken::GameObject::Mesh::Empty;
            }

            /* Convert a Mesh enum value back into its JS-side string form. */
            const char *GameObject::mesh_to_string(Bokken::GameObject::Mesh mesh)
            {
                switch (mesh)
                {
                case Bokken::GameObject::Mesh::Cube:
                    return "Cube";
                case Bokken::GameObject::Mesh::Sphere:
                    return "Sphere";
                case Bokken::GameObject::Mesh::Plane:
                    return "Plane";
                case Bokken::GameObject::Mesh::Empty:
                default:
                    return "Empty";
                }
            }

            /* Engine hook — drives every registered object's components. */
            void GameObject::update(float deltaTime)
            {
                for (auto &go : Bokken::GameObject::Base::s_objects)
                    go->update(deltaTime);

                Bokken::GameObject::Base::flushDestroyed();
            }

            /* Engine hook — fixed-step physics integration. */
            void GameObject::fixedUpdate(float deltaTime)
            {
                for (auto &go : Bokken::GameObject::Base::s_objects)
                    go->fixedUpdate(deltaTime);
            }

            /* Engine hook — per-frame render pass.
             *
             * Builds a model-view-projection matrix per object using a fixed camera
             * positioned at (0, 0, -10). Actual mesh rasterisation is left to the
             * renderer module that consumes these matrices. */
            void GameObject::render()
            {
                if (!s_window)
                    return;

                int w, h;
                SDL_GetWindowSize(s_window, &w, &h);
                if (w <= 0 || h <= 0)
                    return;

                glm::mat4 projection = glm::perspective(
                    glm::radians(45.0f),
                    static_cast<float>(w) / static_cast<float>(h),
                    0.1f, 100.0f);
                glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));

                for (auto &go : Bokken::GameObject::Base::s_objects)
                {
                    auto *t = go->transform;
                    if (!t || t->mesh == Bokken::GameObject::Mesh::Empty)
                        continue;

                    glm::mat4 mvp = projection * view * t->getMatrix();
                    (void)mvp; /* TODO: hand to MeshStage once 3D lands. */
                }
            }
        }
    }
}