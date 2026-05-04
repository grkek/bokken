#pragma once

#include "Component.hpp"
#include <memory>
#include <vector>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <algorithm>

namespace Bokken
{
    namespace GameObject
    {
        // The core scene entity. Dimension-agnostic — just a named bag of Components
        // with parent/child relationships. 2D vs 3D is determined entirely by which
        // components are attached.
        class Base
        {
        public:
            std::string name;

            explicit Base(std::string name = "New GameObject")
                : name(std::move(name)) {}

            template <typename T>
            T &addComponent()
            {
                static_assert(std::is_base_of_v<Component, T>,
                              "T must derive from Component");
                return addComponentInternal<T>();
            }

            template <typename T>
            T *getComponent()
            {
                auto it = m_components.find(std::type_index(typeid(T)));
                if (it == m_components.end())
                    return nullptr;
                return static_cast<T *>(it->second.get());
            }

            template <typename T>
            bool hasComponent() const
            {
                return m_components.find(std::type_index(typeid(T)))
                       != m_components.end();
            }

            template <typename T>
            void removeComponent()
            {
                auto it = m_components.find(std::type_index(typeid(T)));
                if (it != m_components.end())
                {
                    it->second->onDestroy();
                    m_components.erase(it);
                }
            }

            void setParent(Base *parent)
            {
                if (m_parent)
                {
                    auto &siblings = m_parent->m_children;
                    siblings.erase(
                        std::remove(siblings.begin(), siblings.end(), this),
                        siblings.end());
                }
                m_parent = parent;
                if (m_parent)
                    m_parent->m_children.push_back(this);
            }

            Base *getParent() const { return m_parent; }
            std::vector<Base *> getChildren() const { return m_children; }

            void update(float dt)
            {
                for (auto &[key, comp] : m_components)
                    if (comp->enabled)
                        comp->update(dt);
            }

            void fixedUpdate(float dt)
            {
                for (auto &[key, comp] : m_components)
                    if (comp->enabled)
                        comp->fixedUpdate(dt);
            }

            static void destroy(Base *obj) { obj->m_pendingDestroy = true; }

            static Base *find(const std::string &name)
            {
                for (auto &obj : s_objects)
                    if (obj->name == name)
                        return obj.get();
                return nullptr;
            }

            static void flushDestroyed()
            {
                s_objects.erase(
                    std::remove_if(s_objects.begin(), s_objects.end(),
                                   [](const std::unique_ptr<Base> &o)
                                   {
                                       if (!o->m_pendingDestroy)
                                           return false;
                                       for (auto &[k, c] : o->m_components)
                                           c->onDestroy();
                                       return true;
                                   }),
                    s_objects.end());
            }

            static inline std::vector<std::unique_ptr<Base>> s_objects;

        private:
            std::unordered_map<std::type_index,
                               std::unique_ptr<Component>> m_components;
            std::vector<Base *> m_children;
            Base *m_parent = nullptr;
            bool m_pendingDestroy = false;

            template <typename T>
            T &addComponentInternal()
            {
                auto comp = std::make_unique<T>();
                comp->gameObject = this;
                comp->onAttach();
                T *raw = comp.get();
                m_components[std::type_index(typeid(T))] = std::move(comp);
                return *raw;
            }
        };
    }
}
