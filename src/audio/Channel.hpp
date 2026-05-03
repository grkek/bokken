#pragma once

#include "./effects/Base.hpp"
#include "./effects/Gain.hpp"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace Bokken
{
    namespace Audio
    {

        /**
         * A single audio bus.
         * Owns an ordered chain of AudioEffects.
         * The first effect is always an implicit Gain effect (volume/mute).
         *
         * Signal flow:
         *   [sources] → Gain effect → [...user effects] → output
         */
        class Channel
        {
        public:
            const std::string name;

            explicit Channel(std::string name, float volume = 1.0f, bool muted = false)
                : name(std::move(name))
            {
                // Implicit gain node — always index 0
                m_gain = new Effects::Gain(muted ? 0.0f : volume);
                m_chain.emplace_back(m_gain);
                m_volume = volume;
                m_muted = muted;
            }

            float volume() const { return m_volume; }
            bool muted() const { return m_muted; }

            Channel &setVolume(float v)
            {
                m_volume = v;
                m_gain->gain = m_muted ? 0.0f : v;
                return *this;
            }

            Channel &setMuted(bool m)
            {
                m_muted = m;
                m_gain->gain = m ? 0.0f : m_volume;
                return *this;
            }

            Channel &addEffect(Effects::Base *effect)
            {
                m_chain.emplace_back(effect);
                return *this;
            }

            Channel &removeEffect(Effects::Base *effect)
            {
                // Never remove index 0 (implicit gain)
                m_chain.erase(
                    std::remove_if(m_chain.begin() + 1, m_chain.end(),
                                   [effect](const auto &e)
                                   { return e.get() == effect; }),
                    m_chain.end());
                return *this;
            }

            // Returns user-added effects (excludes implicit Effects::Gain at index 0)
            std::vector<Effects::Base *> getEffects() const
            {
                std::vector<Effects::Base *> out;
                for (size_t i = 1; i < m_chain.size(); i++)
                    out.push_back(m_chain[i].get());
                return out;
            }

            /**
             * Run the full effect chain on a signal buffer.
             * Called by the AudioEngine on the audio thread — keep this hot.
             */
            void process(Signal &signal)
            {
                for (auto &effect : m_chain)
                    if (effect->enabled)
                        effect->process(signal);
            }

        private:
            float m_volume = 1.0f;
            bool m_muted = false;

            // Raw (non-owning) pointer to the implicit gain node inside m_chain[0]
            Effects::Gain *m_gain = nullptr;

            // Full chain — index 0 is always the implicit Gain effect
            // unique_ptr owns the effects added via addEffect (transferred ownership)
            std::vector<std::unique_ptr<Effects::Base>> m_chain;
        };

    }
}