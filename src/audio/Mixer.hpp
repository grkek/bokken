#pragma once

#include "Channel.hpp"
#include <unordered_map>
#include <memory>
#include <string>

namespace Bokken
{
    namespace Audio
    {

        /**
         * Singleton that owns all Channels.
         * The "Master" channel is created automatically on construction.
         * All other channels feed into master.
         *
         * Signal flow:
         *   ["Special Effects"] ──┐
         *             ["Music"] ──┤──► ["Master"] ──► output
         *          ["Ambience"] ──┘
         */
        class Mixer
        {
        public:
            static Mixer &get()
            {
                static Mixer instance;
                return instance;
            }

            /**
             * Creates a new channel or returns the existing one if the name is taken.
             * "Master" always exists and cannot be replaced.
             */
            Channel *createChannel(const std::string &name,
                                   float volume = 1.0f,
                                   bool muted = false)
            {
                auto it = m_channels.find(name);
                if (it != m_channels.end())
                    return it->second.get();

                auto ch = std::make_unique<Channel>(name, volume, muted);
                auto *ptr = ch.get();
                m_channels.emplace(name, std::move(ch));
                return ptr;
            }

            /**
             * Returns an existing channel by name, or nullptr if not found.
             * "Master" is always present and never returns nullptr.
             */
            Channel *channel(const std::string &name)
            {
                auto it = m_channels.find(name);
                return it != m_channels.end() ? it->second.get() : nullptr;
            }

            /** Convenience — master channel is always available */
            Channel *master() { return m_channels.at("Master").get(); }

            /**
             * Process all channels then run the master chain.
             * Called by the AudioEngine once per audio frame on the audio thread.
             */
            void process(Signal &signal)
            {
                // Process all non-master channels first
                for (auto &[name, ch] : m_channels)
                {
                    if (name == "Master")
                        continue;
                    ch->process(signal);
                }
                // Master chain last
                m_channels.at("Master")->process(signal);
            }

        private:
            Mixer()
            {
                // Master channel always exists
                m_channels.emplace("Master",
                                   std::make_unique<Channel>("Master", 1.0f, false));
            }

            ~Mixer() = default;

            Mixer(const Mixer &) = delete;
            Mixer &operator=(const Mixer &) = delete;

            std::unordered_map<std::string, std::unique_ptr<Channel>> m_channels;
        };
    }
}