#include "AssetPack.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace Bokken
{

    // Construction / destruction
    AssetPack::AssetPack()
    {
        // PhysFS is reference-counted — safe to call init multiple times.
        if (!PHYSFS_isInit())
        {
            if (!PHYSFS_init(nullptr))
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] PHYSFS_init failed: %s\n",
                        PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            }
        }
    }

    AssetPack::~AssetPack()
    {
        unmountAll();
        // Only deinit if we were the ones who initialised.
        // In practice a single AssetPack lives for the engine's lifetime,
        // so this is safe. If you ever stack multiple AssetPack instances,
        // wrap PhysFS init/deinit in a separate singleton guard.
        if (PHYSFS_isInit())
        {
            PHYSFS_deinit();
        }
    }

    // Mount / unmount

    bool AssetPack::mount(const std::string &packPath, const std::string &mountPoint)
    {
        std::string finalPath = packPath;

        // We check the REAL filesystem here, not the PhysFS VFS
        if (!std::filesystem::exists(finalPath))
        {
            const char *baseDir = PHYSFS_getBaseDir();
            if (baseDir)
            {
                std::string combined = std::string(baseDir) + packPath;
                if (std::filesystem::exists(combined))
                {
                    finalPath = combined;
                }
            }
        }

        if (!PHYSFS_mount(finalPath.c_str(), mountPoint.c_str(), 1))
        {
            PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] Failed to mount:\n"
                            "  Requested: %s\n"
                            "  Resolved:  %s\n"
                            "  Error:     %s\n",
                    packPath.c_str(), finalPath.c_str(), PHYSFS_getErrorByCode(err));
            return false;
        }

        m_mounted.push_back(finalPath);
        fprintf(stdout, "[AssetPack] Successfully mounted '%s' at '%s'\n",
                finalPath.c_str(), mountPoint.c_str());

        return true;
    }

    void AssetPack::unmount(const std::string &packPath)
    {
        if (!PHYSFS_isInit())
            return;
        PHYSFS_unmount(packPath.c_str());
        m_mounted.erase(std::remove(m_mounted.begin(), m_mounted.end(), packPath),
                        m_mounted.end());
    }

    void AssetPack::unmountAll()
    {
        if (!PHYSFS_isInit())
            return;
        // Iterate in reverse so the last-mounted is unmounted first.
        for (auto it = m_mounted.rbegin(); it != m_mounted.rend(); ++it)
        {
            PHYSFS_unmount(it->c_str());
        }
        m_mounted.clear();
    }

    // Query
    bool AssetPack::exists(const std::string &virtualPath) const
    {
        return PHYSFS_isInit() && PHYSFS_exists(virtualPath.c_str());
    }

    // Read helpers
    std::vector<uint8_t> AssetPack::readBytes(const std::string &virtualPath) const
    {
        if (!PHYSFS_isInit())
            return {};

        PHYSFS_File *f = PHYSFS_openRead(virtualPath.c_str());
        if (!f)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] readBytes: cannot open '%s': %s\n",
                    virtualPath.c_str(),
                    PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return {};
        }

        PHYSFS_sint64 fileLen = PHYSFS_fileLength(f);
        if (fileLen < 0)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] readBytes: unknown length for '%s'\n",
                    virtualPath.c_str());
            PHYSFS_close(f);
            return {};
        }

        std::vector<uint8_t> buf(static_cast<size_t>(fileLen));
        PHYSFS_sint64 read = PHYSFS_readBytes(f, buf.data(), static_cast<PHYSFS_uint64>(fileLen));
        PHYSFS_close(f);

        if (read != fileLen)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] readBytes: short read on '%s' (%lld of %lld)\n",
                    virtualPath.c_str(), (long long)read, (long long)fileLen);
            return {};
        }

        return buf;
    }

    // SDL_IOStream bridge
    // Internal userdata struct passed to every SDL_IOStream callback.
    struct PhysFSIOData
    {
        PHYSFS_File *file = nullptr;
        std::string virtualPath; // for error messages
    };

    Sint64 AssetPack::io_size(void *userdata)
    {
        auto *d = static_cast<PhysFSIOData *>(userdata);
        return PHYSFS_fileLength(d->file);
    }

    Sint64 AssetPack::io_seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
    {
        auto *d = static_cast<PhysFSIOData *>(userdata);

        PHYSFS_sint64 target = 0;
        switch (whence)
        {
        case SDL_IO_SEEK_SET:
            target = offset;
            break;
        case SDL_IO_SEEK_CUR:
            target = static_cast<PHYSFS_sint64>(PHYSFS_tell(d->file)) + offset;
            break;
        case SDL_IO_SEEK_END:
            target = PHYSFS_fileLength(d->file) + offset;
            break;
        default:
            return -1;
        }

        if (!PHYSFS_seek(d->file, static_cast<PHYSFS_uint64>(target)))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] io_seek failed on '%s': %s\n",
                    d->virtualPath.c_str(),
                    PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return -1;
        }
        return target;
    }

    size_t AssetPack::io_read(void *userdata, void *ptr, size_t size,
                              SDL_IOStatus *status)
    {
        auto *d = static_cast<PhysFSIOData *>(userdata);
        PHYSFS_sint64 n = PHYSFS_readBytes(d->file, ptr, static_cast<PHYSFS_uint64>(size));

        if (n < 0)
        {
            if (status)
                *status = SDL_IO_STATUS_ERROR;
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] io_read error on '%s': %s\n",
                    d->virtualPath.c_str(),
                    PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return 0;
        }
        if (static_cast<size_t>(n) < size)
        {
            if (status)
                *status = SDL_IO_STATUS_EOF;
        }
        else
        {
            if (status)
                *status = SDL_IO_STATUS_READY;
        }
        return static_cast<size_t>(n);
    }

    size_t AssetPack::io_write(void * /*userdata*/, const void * /*ptr*/,
                               size_t /*size*/, SDL_IOStatus *status)
    {
        // .assetpack files are read-only.
        if (status)
            *status = SDL_IO_STATUS_ERROR;
        return 0;
    }

    bool AssetPack::io_close(void *userdata)
    {
        auto *d = static_cast<PhysFSIOData *>(userdata);
        bool ok = (PHYSFS_close(d->file) != 0);
        delete d;
        return ok;
    }

    SDL_IOStream *AssetPack::wrapPhysFSFile(PHYSFS_File *file,
                                            const std::string &virtualPath)
    {
        auto *data = new PhysFSIOData{file, virtualPath};
        SDL_IOStreamInterface iface{};
        SDL_INIT_INTERFACE(&iface);
        iface.size = &AssetPack::io_size;
        iface.seek = &AssetPack::io_seek;
        iface.read = &AssetPack::io_read;
        iface.write = &AssetPack::io_write;
        iface.close = &AssetPack::io_close;

        SDL_IOStream *io = SDL_OpenIO(&iface, data);
        if (!io)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] SDL_OpenIO failed for '%s': %s\n",
                    virtualPath.c_str(), SDL_GetError());
            PHYSFS_close(file);
            delete data;
            return nullptr;
        }
        return io;
    }

    SDL_IOStream *AssetPack::openIOStream(const std::string &virtualPath) const
    {
        if (!PHYSFS_isInit())
            return nullptr;

        PHYSFS_File *f = PHYSFS_openRead(virtualPath.c_str());
        if (!f)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[AssetPack] openIOStream: cannot open '%s': %s\n",
                    virtualPath.c_str(),
                    PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return nullptr;
        }

        return wrapPhysFSFile(f, virtualPath);
    }

    // Enumeration
    void AssetPack::enumerate(const std::string &virtualDir,
                              const std::function<void(const std::string &)> &callback,
                              bool recursive) const
    {
        if (!PHYSFS_isInit())
            return;

        char **files = PHYSFS_enumerateFiles(virtualDir.c_str());
        if (!files)
            return;

        for (char **i = files; *i != nullptr; ++i)
        {
            // Build full virtual path.
            std::string fullPath = virtualDir;
            if (fullPath.back() != '/')
                fullPath += '/';
            fullPath += *i;

            PHYSFS_Stat stat{};
            if (PHYSFS_stat(fullPath.c_str(), &stat))
            {
                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
                {
                    if (recursive)
                    {
                        enumerate(fullPath, callback, true);
                    }
                }
                else
                {
                    callback(fullPath);
                }
            }
        }

        PHYSFS_freeList(files);
    }

} // namespace Bokken
