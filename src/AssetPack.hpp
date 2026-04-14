#pragma once

#include <SDL3/SDL.h>
#include <physfs.h>
#include <string>
#include <vector>
#include <functional>

namespace Bokken
{
    /**
     * AssetPack
     *
     * Mounts a .assetpack file (zip archive produced by BokkenPacker) into the
     * PhysFS virtual filesystem and provides SDL_IOStream-compatible access to
     * every file inside it.
     *
     * Layout inside an .assetpack (as written by BokkenPacker):
     *
     *   scripts/index.script       ← QuickJS bytecode (entry point)
     *   scripts/components/MyComponent.script
     *   textures/low/...
     *   textures/high/...
     *   sounds/...
     *   fonts/...
     *
     * Usage:
     *   AssetPack pack;
     *   pack.mount("data/scripts.assetpack", "/scripts");
     *   pack.mount("data/textures_high.assetpack", "/textures");
     *
     *   // Read bytecode into a byte vector:
     *   auto bc = pack.readBytes("/scripts/scripts/index.script");
     *
     *   // Or wrap as SDL_IOStream for SDL_image / SDL_mixer etc.:
     *   SDL_IOStream* io = pack.openIOStream("/textures/player.png");
     *   // ... use io ...
     *   SDL_CloseIO(io);
     *
     *   pack.unmountAll(); // or just let the destructor handle it
     */
    class AssetPack
    {
    public:
        AssetPack();
        ~AssetPack();

        // Non-copyable, movable.
        AssetPack(const AssetPack &) = delete;
        AssetPack &operator=(const AssetPack &) = delete;
        AssetPack(AssetPack &&) = default;

        /**
         * Mount a .assetpack archive into the virtual FS at the given mount point.
         * Multiple packs can be mounted at the same or different mount points.
         * If two packs have a file at the same virtual path, the last-mounted one wins
         * (PHYSFS_APPEND behaviour).
         *
         * @param packPath   Real filesystem path to the .assetpack file.
         * @param mountPoint Virtual path prefix (e.g. "/scripts", "/textures").
         *                   Use "/" to mount at the root.
         * @return true on success.
         */
        bool mount(const std::string &packPath,
                   const std::string &mountPoint = "/");

        /**
         * Unmount a previously mounted pack.
         * @param packPath The same real path used in mount().
         */
        void unmount(const std::string &packPath);

        /** Unmount every pack that was mounted through this AssetPack instance. */
        void unmountAll();

        /**
         * Check whether a virtual path exists in any mounted pack.
         */
        bool exists(const std::string &virtualPath) const;

        /**
         * Read the entire contents of a virtual file into a byte vector.
         * Returns an empty vector on failure and logs the error.
         */
        std::vector<uint8_t> readBytes(const std::string &virtualPath) const;

        /**
         * Open a virtual file as an SDL_IOStream.
         * The caller is responsible for calling SDL_CloseIO() when done.
         * Returns nullptr on failure.
         *
         * This is the correct way to feed packed assets into SDL_image, SDL_mixer,
         * SDL_ttf, or any SDL3 subsystem that accepts SDL_IOStream*.
         */
        SDL_IOStream *openIOStream(const std::string &virtualPath) const;

        /**
         * Enumerate all files under a virtual directory.
         * Useful for batch-loading scenes, fonts, etc.
         *
         * @param virtualDir  Virtual directory path (e.g. "/script").
         * @param callback    Called for each entry with the full virtual path.
         * @param recursive   If true, recurse into subdirectories.
         */
        void enumerate(const std::string &virtualDir,
                       const std::function<void(const std::string &)> &callback,
                       bool recursive = false) const;

    private:
        // Track every path we have mounted so unmountAll() works correctly.
        std::vector<std::string> m_mounted;

        // Internal: create an SDL_IOStream backed by a PHYSFS_File.
        static SDL_IOStream *wrapPhysFSFile(PHYSFS_File *file,
                                            const std::string &virtualPath);

        // SDL_IOStream callback implementations for PhysFS.
        static Sint64 io_size(void *userdata);
        static Sint64 io_seek(void *userdata, Sint64 offset, SDL_IOWhence whence);
        static size_t io_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status);
        static size_t io_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status);
        static bool io_close(void *userdata);
    };

} // namespace Bokken
