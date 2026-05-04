#include "GameObject.hpp"

#include <cstring>

namespace Bokken
{
    namespace Scripting
    {
        namespace Modules
        {
            enum Transform2DProperties
            {
                T2_PositionX = 0,
                T2_PositionY,
                T2_Rotation,
                T2_ScaleX,
                T2_ScaleY,
                T2_ZOrder
            };

            enum Rigidbody2DProperties
            {
                RB2_Mass = 0,
                RB2_UseGravity,
                RB2_IsStatic,
                RB2_VelocityX,
                RB2_VelocityY,
                RB2_AngularVelocity
            };

            enum Mesh2DProperties
            {
                M2_Shape = 0,
                M2_Color, // packed 0xRRGGBBAA
                M2_FlipX,
                M2_FlipY
            };

            // Static function lists — must not be stack-allocated.
            // QuickJS-NG may retain interior pointers into these arrays after
            // JS_SetPropertyFunctionList returns, so they need static lifetime.
            static const JSCFunctionListEntry s_goProtoFuncs[] = {
                JS_CFUNC_DEF("addComponent", 2, GameObject::js_add_component),
                JS_CFUNC_DEF("getComponent", 1, GameObject::js_get_component),
                JS_CFUNC_DEF("setParent", 1, GameObject::js_set_parent),
                JS_CFUNC_DEF("getChildren", 0, GameObject::js_get_children),
            };

            static const JSCFunctionListEntry s_goStaticFuncs[] = {
                JS_CFUNC_DEF("destroy", 1, GameObject::js_destroy),
                JS_CFUNC_DEF("find", 1, GameObject::js_find),
            };

            static const JSCFunctionListEntry s_t2Funcs[] = {
                JS_CGETSET_MAGIC_DEF("positionX", GameObject::js_transform2d_get, GameObject::js_transform2d_set, T2_PositionX),
                JS_CGETSET_MAGIC_DEF("positionY", GameObject::js_transform2d_get, GameObject::js_transform2d_set, T2_PositionY),
                JS_CGETSET_MAGIC_DEF("rotation", GameObject::js_transform2d_get, GameObject::js_transform2d_set, T2_Rotation),
                JS_CGETSET_MAGIC_DEF("scaleX", GameObject::js_transform2d_get, GameObject::js_transform2d_set, T2_ScaleX),
                JS_CGETSET_MAGIC_DEF("scaleY", GameObject::js_transform2d_get, GameObject::js_transform2d_set, T2_ScaleY),
                JS_CGETSET_MAGIC_DEF("zOrder", GameObject::js_transform2d_get, GameObject::js_transform2d_set, T2_ZOrder),
                JS_CFUNC_DEF("translate", 2, GameObject::js_transform2d_translate),
                JS_CFUNC_DEF("rotate", 1, GameObject::js_transform2d_rotate),
            };

            static const JSCFunctionListEntry s_rb2Funcs[] = {
                JS_CGETSET_MAGIC_DEF("mass", GameObject::js_rigidbody2d_get, GameObject::js_rigidbody2d_set, RB2_Mass),
                JS_CGETSET_MAGIC_DEF("useGravity", GameObject::js_rigidbody2d_get, GameObject::js_rigidbody2d_set, RB2_UseGravity),
                JS_CGETSET_MAGIC_DEF("isStatic", GameObject::js_rigidbody2d_get, GameObject::js_rigidbody2d_set, RB2_IsStatic),
                JS_CGETSET_MAGIC_DEF("velocityX", GameObject::js_rigidbody2d_get, GameObject::js_rigidbody2d_set, RB2_VelocityX),
                JS_CGETSET_MAGIC_DEF("velocityY", GameObject::js_rigidbody2d_get, GameObject::js_rigidbody2d_set, RB2_VelocityY),
                JS_CGETSET_MAGIC_DEF("angularVelocity", GameObject::js_rigidbody2d_get, GameObject::js_rigidbody2d_set, RB2_AngularVelocity),
                JS_CFUNC_DEF("applyForce", 2, GameObject::js_rigidbody2d_apply_force),
                JS_CFUNC_DEF("applyTorque", 1, GameObject::js_rigidbody2d_apply_torque),
                JS_CFUNC_DEF("setVelocity", 2, GameObject::js_rigidbody2d_set_velocity),
            };

            static const JSCFunctionListEntry s_m2Funcs[] = {
                JS_CGETSET_MAGIC_DEF("shape", GameObject::js_mesh2d_get, GameObject::js_mesh2d_set, M2_Shape),
                JS_CGETSET_MAGIC_DEF("color", GameObject::js_mesh2d_get, GameObject::js_mesh2d_set, M2_Color),
                JS_CGETSET_MAGIC_DEF("flipX", GameObject::js_mesh2d_get, GameObject::js_mesh2d_set, M2_FlipX),
                JS_CGETSET_MAGIC_DEF("flipY", GameObject::js_mesh2d_get, GameObject::js_mesh2d_set, M2_FlipY),
            };

            int GameObject::declare(JSContext *ctx, JSModuleDef *m)
            {
                JSRuntime *rt = JS_GetRuntime(ctx);

                JS_NewClassID(rt, &s_class_id);
                JS_NewClassID(rt, &s_transform2d_class_id);
                JS_NewClassID(rt, &s_rigidbody2d_class_id);
                JS_NewClassID(rt, &s_mesh2d_class_id);

                static JSClassDef goClass = {"GameObject", .finalizer = nullptr};
                static JSClassDef t2Class = {"Transform2D", .finalizer = nullptr};
                static JSClassDef rb2Class = {"Rigidbody2D", .finalizer = nullptr};
                static JSClassDef m2Class = {"Mesh2D", .finalizer = nullptr};

                JS_NewClass(rt, s_class_id, &goClass);
                JS_NewClass(rt, s_transform2d_class_id, &t2Class);
                JS_NewClass(rt, s_rigidbody2d_class_id, &rb2Class);
                JS_NewClass(rt, s_mesh2d_class_id, &m2Class);

                JS_AddModuleExport(ctx, m, "Shape2D");
                JS_AddModuleExport(ctx, m, "Transform2D");
                JS_AddModuleExport(ctx, m, "Rigidbody2D");
                JS_AddModuleExport(ctx, m, "Mesh2D");

                return JS_AddModuleExport(ctx, m, "GameObject");
            }

            int GameObject::init(JSContext *ctx, JSModuleDef *m)
            {
                s_window = m_window;

                // GameObject prototype and constructor.
                JSValue goProto = JS_NewObject(ctx);
                JS_SetPropertyFunctionList(ctx, goProto, s_goProtoFuncs,
                                           sizeof(s_goProtoFuncs) / sizeof(s_goProtoFuncs[0]));

                JSValue goCtor = JS_NewCFunction2(ctx, GameObject::js_constructor,
                                                  "GameObject", 1, JS_CFUNC_constructor, 0);
                JS_SetPropertyFunctionList(ctx, goCtor, s_goStaticFuncs,
                                           sizeof(s_goStaticFuncs) / sizeof(s_goStaticFuncs[0]));

                JS_SetConstructor(ctx, goCtor, goProto);
                JS_SetClassProto(ctx, s_class_id, goProto);

                // Transform2D prototype.
                JSValue t2Proto = JS_NewObject(ctx);
                JS_SetPropertyFunctionList(ctx, t2Proto, s_t2Funcs,
                                           sizeof(s_t2Funcs) / sizeof(s_t2Funcs[0]));
                JS_SetClassProto(ctx, s_transform2d_class_id, t2Proto);

                // Rigidbody2D prototype.
                JSValue rb2Proto = JS_NewObject(ctx);
                JS_SetPropertyFunctionList(ctx, rb2Proto, s_rb2Funcs,
                                           sizeof(s_rb2Funcs) / sizeof(s_rb2Funcs[0]));
                JS_SetClassProto(ctx, s_rigidbody2d_class_id, rb2Proto);

                // Mesh2D prototype.
                JSValue m2Proto = JS_NewObject(ctx);
                JS_SetPropertyFunctionList(ctx, m2Proto, s_m2Funcs,
                                           sizeof(s_m2Funcs) / sizeof(s_m2Funcs[0]));
                JS_SetClassProto(ctx, s_mesh2d_class_id, m2Proto);

                // Component tokens.
                JSValue t2Token = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, t2Token, "name", JS_NewString(ctx, "Transform2D"));

                JSValue rb2Token = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, rb2Token, "name", JS_NewString(ctx, "Rigidbody2D"));

                JSValue m2Token = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, m2Token, "name", JS_NewString(ctx, "Mesh2D"));

                // Shape2D enum.
                JSValue shape2d = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, shape2d, "Empty", JS_NewString(ctx, "Empty"));
                JS_SetPropertyStr(ctx, shape2d, "Quad", JS_NewString(ctx, "Quad"));
                JS_SetPropertyStr(ctx, shape2d, "Circle", JS_NewString(ctx, "Circle"));
                JS_SetPropertyStr(ctx, shape2d, "Triangle", JS_NewString(ctx, "Triangle"));
                JS_SetPropertyStr(ctx, shape2d, "Line", JS_NewString(ctx, "Line"));

                JS_SetModuleExport(ctx, m, "GameObject", goCtor);
                JS_SetModuleExport(ctx, m, "Shape2D", shape2d);
                JS_SetModuleExport(ctx, m, "Transform2D", t2Token);
                JS_SetModuleExport(ctx, m, "Rigidbody2D", rb2Token);
                JS_SetModuleExport(ctx, m, "Mesh2D", m2Token);

                return 0;
            }

            // JS: new GameObject(name?)
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

                auto go = std::make_unique<Bokken::GameObject::Base>(name);
                Bokken::GameObject::Base *raw = go.get();
                Bokken::GameObject::Base::s_objects.push_back(std::move(go));

                JS_SetOpaque(obj, raw);
                return obj;
            }

            // JS: gameObject.addComponent(Token, props?) — returns `this` for chaining.
            JSValue GameObject::js_add_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                JSValue nameVal = JS_GetPropertyStr(ctx, argv[0], "name");
                const char *className = JS_ToCString(ctx, nameVal);
                JS_FreeValue(ctx, nameVal);
                if (!className)
                    return JS_UNDEFINED;

                JSValue wrapped = JS_UNDEFINED;

                if (strcmp(className, "Transform2D") == 0)
                {
                    auto &t = go->addComponent<Bokken::GameObject::Transform2D>();
                    wrapped = wrap_transform2d(ctx, &t);
                }
                else if (strcmp(className, "Rigidbody2D") == 0)
                {
                    auto &rb = go->addComponent<Bokken::GameObject::Rigidbody2D>();
                    wrapped = wrap_rigidbody2d(ctx, &rb);
                }
                else if (strcmp(className, "Mesh2D") == 0)
                {
                    auto &mesh = go->addComponent<Bokken::GameObject::Mesh2D>();
                    wrapped = wrap_mesh2d(ctx, &mesh);
                }

                JS_FreeCString(ctx, className);

                // Apply config object if provided.
                if (!JS_IsUndefined(wrapped) && argc >= 2 && JS_IsObject(argv[1]))
                    apply_props(ctx, wrapped, argv[1]);

                JS_FreeValue(ctx, wrapped);

                // Return `this` for chaining.
                return JS_DupValue(ctx, this_val);
            }

            // JS: gameObject.getComponent(Token)
            JSValue GameObject::js_get_component(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                JSValue nameVal = JS_GetPropertyStr(ctx, argv[0], "name");
                const char *className = JS_ToCString(ctx, nameVal);
                JS_FreeValue(ctx, nameVal);
                if (!className)
                    return JS_UNDEFINED;

                JSValue result = JS_UNDEFINED;

                if (strcmp(className, "Transform2D") == 0)
                {
                    auto *t = go->getComponent<Bokken::GameObject::Transform2D>();
                    if (t)
                        result = wrap_transform2d(ctx, t);
                }
                else if (strcmp(className, "Rigidbody2D") == 0)
                {
                    auto *rb = go->getComponent<Bokken::GameObject::Rigidbody2D>();
                    if (rb)
                        result = wrap_rigidbody2d(ctx, rb);
                }
                else if (strcmp(className, "Mesh2D") == 0)
                {
                    auto *mesh = go->getComponent<Bokken::GameObject::Mesh2D>();
                    if (mesh)
                        result = wrap_mesh2d(ctx, mesh);
                }

                JS_FreeCString(ctx, className);
                return result;
            }

            JSValue GameObject::js_set_parent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(this_val, s_class_id));
                if (!go || argc < 1)
                    return JS_UNDEFINED;

                if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0]))
                    go->setParent(nullptr);
                else
                {
                    auto *parent = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(argv[0], s_class_id));
                    go->setParent(parent);
                }
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_get_children(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
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

            JSValue GameObject::js_destroy(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
            {
                if (argc < 1)
                    return JS_UNDEFINED;
                auto *go = static_cast<Bokken::GameObject::Base *>(JS_GetOpaque(argv[0], s_class_id));
                if (go)
                    Bokken::GameObject::Base::destroy(go);
                return JS_UNDEFINED;
            }

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

                JSValue obj = JS_NewObjectClass(ctx, s_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, go);
                return obj;
            }

            // Transform2D getters/setters.
            JSValue GameObject::js_transform2d_get(JSContext *ctx, JSValueConst this_val, int magic)
            {
                auto *t = static_cast<Bokken::GameObject::Transform2D *>(
                    JS_GetOpaque(this_val, s_transform2d_class_id));
                if (!t)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case T2_PositionX:
                    return JS_NewFloat64(ctx, t->position.x);
                case T2_PositionY:
                    return JS_NewFloat64(ctx, t->position.y);
                case T2_Rotation:
                    return JS_NewFloat64(ctx, t->rotation);
                case T2_ScaleX:
                    return JS_NewFloat64(ctx, t->scale.x);
                case T2_ScaleY:
                    return JS_NewFloat64(ctx, t->scale.y);
                case T2_ZOrder:
                    return JS_NewFloat64(ctx, t->zOrder);
                }
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_transform2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic)
            {
                auto *t = static_cast<Bokken::GameObject::Transform2D *>(
                    JS_GetOpaque(this_val, s_transform2d_class_id));
                if (!t)
                    return JS_UNDEFINED;

                double d = 0.0;
                if (JS_ToFloat64(ctx, &d, val) < 0)
                    return JS_EXCEPTION;
                float f = static_cast<float>(d);

                switch (magic)
                {
                case T2_PositionX:
                    t->position.x = f;
                    break;
                case T2_PositionY:
                    t->position.y = f;
                    break;
                case T2_Rotation:
                    t->rotation = f;
                    break;
                case T2_ScaleX:
                    t->scale.x = f;
                    break;
                case T2_ScaleY:
                    t->scale.y = f;
                    break;
                case T2_ZOrder:
                    t->zOrder = f;
                    break;
                }
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_transform2d_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *t = static_cast<Bokken::GameObject::Transform2D *>(
                    JS_GetOpaque(this_val, s_transform2d_class_id));
                if (!t || argc < 2)
                    return JS_UNDEFINED;

                double x, y;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0)
                    return JS_EXCEPTION;

                t->translate(static_cast<float>(x), static_cast<float>(y));
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_transform2d_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *t = static_cast<Bokken::GameObject::Transform2D *>(
                    JS_GetOpaque(this_val, s_transform2d_class_id));
                if (!t || argc < 1)
                    return JS_UNDEFINED;

                double deg;
                if (JS_ToFloat64(ctx, &deg, argv[0]) < 0)
                    return JS_EXCEPTION;

                t->rotate(static_cast<float>(deg));
                return JS_UNDEFINED;
            }

            // Rigidbody2D getters/setters.
            JSValue GameObject::js_rigidbody2d_get(JSContext *ctx, JSValueConst this_val, int magic)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody2D *>(
                    JS_GetOpaque(this_val, s_rigidbody2d_class_id));
                if (!rb)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case RB2_Mass:
                    return JS_NewFloat64(ctx, rb->mass);
                case RB2_UseGravity:
                    return JS_NewBool(ctx, rb->useGravity);
                case RB2_IsStatic:
                    return JS_NewBool(ctx, rb->isStatic);
                case RB2_VelocityX:
                    return JS_NewFloat64(ctx, rb->velocity.x);
                case RB2_VelocityY:
                    return JS_NewFloat64(ctx, rb->velocity.y);
                case RB2_AngularVelocity:
                    return JS_NewFloat64(ctx, rb->angularVelocity);
                }
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_rigidbody2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody2D *>(
                    JS_GetOpaque(this_val, s_rigidbody2d_class_id));
                if (!rb)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case RB2_UseGravity:
                    rb->useGravity = JS_ToBool(ctx, val);
                    return JS_UNDEFINED;
                case RB2_IsStatic:
                    rb->isStatic = JS_ToBool(ctx, val);
                    return JS_UNDEFINED;
                }

                double d = 0.0;
                if (JS_ToFloat64(ctx, &d, val) < 0)
                    return JS_EXCEPTION;
                float f = static_cast<float>(d);

                switch (magic)
                {
                case RB2_Mass:
                    rb->mass = f;
                    break;
                case RB2_VelocityX:
                    rb->velocity.x = f;
                    break;
                case RB2_VelocityY:
                    rb->velocity.y = f;
                    break;
                case RB2_AngularVelocity:
                    rb->angularVelocity = f;
                    break;
                }
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_rigidbody2d_apply_force(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody2D *>(
                    JS_GetOpaque(this_val, s_rigidbody2d_class_id));
                if (!rb || argc < 2)
                    return JS_UNDEFINED;

                double x, y;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0)
                    return JS_EXCEPTION;

                rb->applyForce(static_cast<float>(x), static_cast<float>(y));
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_rigidbody2d_apply_torque(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody2D *>(
                    JS_GetOpaque(this_val, s_rigidbody2d_class_id));
                if (!rb || argc < 1)
                    return JS_UNDEFINED;

                double deg;
                if (JS_ToFloat64(ctx, &deg, argv[0]) < 0)
                    return JS_EXCEPTION;

                rb->applyTorque(static_cast<float>(deg));
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_rigidbody2d_set_velocity(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
            {
                auto *rb = static_cast<Bokken::GameObject::Rigidbody2D *>(
                    JS_GetOpaque(this_val, s_rigidbody2d_class_id));
                if (!rb || argc < 2)
                    return JS_UNDEFINED;

                double x, y;
                if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
                    JS_ToFloat64(ctx, &y, argv[1]) < 0)
                    return JS_EXCEPTION;

                rb->setVelocity(static_cast<float>(x), static_cast<float>(y));
                return JS_UNDEFINED;
            }

            // Mesh2D getters/setters.
            JSValue GameObject::js_mesh2d_get(JSContext *ctx, JSValueConst this_val, int magic)
            {
                auto *mesh = static_cast<Bokken::GameObject::Mesh2D *>(
                    JS_GetOpaque(this_val, s_mesh2d_class_id));
                if (!mesh)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case M2_Shape:
                    return JS_NewString(ctx, shape2d_to_string(mesh->shape));
                case M2_Color:
                {
                    uint32_t packed =
                        (static_cast<uint32_t>(mesh->color.r * 255) << 24) |
                        (static_cast<uint32_t>(mesh->color.g * 255) << 16) |
                        (static_cast<uint32_t>(mesh->color.b * 255) << 8) |
                        static_cast<uint32_t>(mesh->color.a * 255);

                    return JS_NewUint32(ctx, packed);
                }
                case M2_FlipX:
                    return JS_NewBool(ctx, mesh->flipX);
                case M2_FlipY:
                    return JS_NewBool(ctx, mesh->flipY);
                }
                return JS_UNDEFINED;
            }

            JSValue GameObject::js_mesh2d_set(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic)
            {
                auto *mesh = static_cast<Bokken::GameObject::Mesh2D *>(
                    JS_GetOpaque(this_val, s_mesh2d_class_id));
                if (!mesh)
                    return JS_UNDEFINED;

                switch (magic)
                {
                case M2_Shape:
                {
                    const char *str = JS_ToCString(ctx, val);
                    if (str)
                    {
                        mesh->shape = parse_shape2d(str);
                        JS_FreeCString(ctx, str);
                    }
                    return JS_UNDEFINED;
                }
                case M2_FlipX:
                    mesh->flipX = JS_ToBool(ctx, val);
                    return JS_UNDEFINED;
                case M2_FlipY:
                    mesh->flipY = JS_ToBool(ctx, val);
                    return JS_UNDEFINED;
                }

                double d = 0.0;
                if (JS_ToFloat64(ctx, &d, val) < 0)
                    return JS_EXCEPTION;
                float f = static_cast<float>(d);

                switch (magic)
                {
                case M2_Color:
                {
                    uint32_t c = 0;
                    JS_ToUint32(ctx, &c, val);
                    mesh->color.r = ((c >> 24) & 0xFF) / 255.0f;
                    mesh->color.g = ((c >> 16) & 0xFF) / 255.0f;
                    mesh->color.b = ((c >> 8) & 0xFF) / 255.0f;
                    mesh->color.a = ((c >> 0) & 0xFF) / 255.0f;
                    return JS_UNDEFINED;
                }
                }

                return JS_UNDEFINED;
            }

            // Wrappers.
            JSValue GameObject::wrap_transform2d(JSContext *ctx, Bokken::GameObject::Transform2D *t)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_transform2d_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, t);
                return obj;
            }

            JSValue GameObject::wrap_rigidbody2d(JSContext *ctx, Bokken::GameObject::Rigidbody2D *rb)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_rigidbody2d_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, rb);
                return obj;
            }

            JSValue GameObject::wrap_mesh2d(JSContext *ctx, Bokken::GameObject::Mesh2D *mesh)
            {
                JSValue obj = JS_NewObjectClass(ctx, s_mesh2d_class_id);
                if (JS_IsException(obj))
                    return obj;
                JS_SetOpaque(obj, mesh);
                return obj;
            }

            // Copies every own enumerable property from `props` onto `target` by
            // reading the key from props and setting it on target. This triggers
            // the target's setters (CGETSET_MAGIC) so { shape: "Quad" } on a
            // Mesh2D handle calls js_mesh2d_set with M2_Shape automatically.
            void GameObject::apply_props(JSContext *ctx, JSValue target, JSValue props)
            {
                JSPropertyEnum *tab = nullptr;
                uint32_t len = 0;

                if (JS_GetOwnPropertyNames(ctx, &tab, &len, props,
                                           JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0)
                    return;

                for (uint32_t i = 0; i < len; i++)
                {
                    JSValue val = JS_GetProperty(ctx, props, tab[i].atom);
                    JS_SetProperty(ctx, target, tab[i].atom, val);
                    // JS_SetProperty takes ownership of val, don't free it.
                }

                for (uint32_t i = 0; i < len; i++)
                    JS_FreeAtom(ctx, tab[i].atom);

                js_free(ctx, tab);
            }

            // Utility.
            JSValue GameObject::make_vec2(JSContext *ctx, const glm::vec2 &v)
            {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, v.x));
                JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, v.y));
                return obj;
            }

            bool GameObject::read_vec2(JSContext *ctx, JSValueConst val, glm::vec2 &out)
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

                return readField("x", out.x) && readField("y", out.y);
            }

            Bokken::GameObject::Shape2D GameObject::parse_shape2d(const char *name)
            {
                if (!name)
                    return Bokken::GameObject::Shape2D::Empty;
                if (strcmp(name, "Quad") == 0)
                    return Bokken::GameObject::Shape2D::Quad;
                if (strcmp(name, "Circle") == 0)
                    return Bokken::GameObject::Shape2D::Circle;
                if (strcmp(name, "Triangle") == 0)
                    return Bokken::GameObject::Shape2D::Triangle;
                if (strcmp(name, "Line") == 0)
                    return Bokken::GameObject::Shape2D::Line;
                return Bokken::GameObject::Shape2D::Empty;
            }

            const char *GameObject::shape2d_to_string(Bokken::GameObject::Shape2D shape)
            {
                switch (shape)
                {
                case Bokken::GameObject::Shape2D::Quad:
                    return "Quad";
                case Bokken::GameObject::Shape2D::Circle:
                    return "Circle";
                case Bokken::GameObject::Shape2D::Triangle:
                    return "Triangle";
                case Bokken::GameObject::Shape2D::Line:
                    return "Line";
                case Bokken::GameObject::Shape2D::Empty:
                default:
                    return "Empty";
                }
            }

            void GameObject::update(float deltaTime)
            {
                for (auto &go : Bokken::GameObject::Base::s_objects)
                    go->update(deltaTime);

                Bokken::GameObject::Base::flushDestroyed();
            }

            void GameObject::fixedUpdate(float deltaTime)
            {
                for (auto &go : Bokken::GameObject::Base::s_objects)
                    go->fixedUpdate(deltaTime);
            }

            void GameObject::present()
            {
                if (!s_window || !s_batcher)
                    return;

                int w, h;
                SDL_GetWindowSize(s_window, &w, &h);
                if (w <= 0 || h <= 0)
                    return;

                float hw = w * 0.5f;
                float hh = h * 0.5f;

                // Pixels per world unit — controls zoom. 64 means 1 world unit = 64px.
                constexpr float PPU = 64.0f;

                for (auto &go : Bokken::GameObject::Base::s_objects)
                {
                    auto *t = go->getComponent<Bokken::GameObject::Transform2D>();
                    auto *mesh = go->getComponent<Bokken::GameObject::Mesh2D>();
                    if (!t || !mesh || mesh->shape == Bokken::GameObject::Shape2D::Empty)
                        continue;

                    // World → screen: center of screen is world origin,
                    // y-up in world but y-down in screen space.
                    float sx = hw + t->position.x * PPU - (t->scale.x * PPU * 0.5f);
                    float sy = hh - t->position.y * PPU - (t->scale.y * PPU * 0.5f);
                    float sw = t->scale.x * PPU;
                    float sh = t->scale.y * PPU;

                    uint32_t rgba =
                        (static_cast<uint32_t>(mesh->color.r * 255) << 24) |
                        (static_cast<uint32_t>(mesh->color.g * 255) << 16) |
                        (static_cast<uint32_t>(mesh->color.b * 255) << 8) |
                        static_cast<uint32_t>(mesh->color.a * 255);

                    s_batcher->drawRect(sx, sy, sw, sh, rgba, static_cast<int>(t->zOrder));
                }
            }
        }
    }
}
