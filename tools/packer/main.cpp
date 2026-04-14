#include "miniz.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

// ANSI Color Codes
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define GREEN "\033[32m"
#define CYN "\033[36m"
#define YEL "\033[33m"
#define RED "\033[31m"
#define MAG "\033[35m"

const std::vector<std::string> QUALITY_TIERS = {"low", "middle", "high", "ultra"};

void pack_folder(const fs::path &input_path, const std::string &output_filename, bool is_scripts = false)
{
    fs::path out_file(output_filename);
    if (out_file.has_parent_path())
    {
        fs::create_directories(out_file.parent_path());
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    std::cout << CYN << "  [Init] " << RESET << "Creating archive: " << BOLD << output_filename << RESET << std::endl;

    if (!fs::exists(input_path))
    {
        std::cerr << RED << "  [!] Input path does not exist: " << input_path << RESET << std::endl;
        return;
    }

    if (!mz_zip_writer_init_file(&zip, output_filename.c_str(), 0))
    {
        std::cerr << RED << "  [!] CRITICAL: Could not open " << output_filename << " (Check permissions/path)" << RESET << std::endl;
        return;
    }

    int file_count = 0;
    for (auto &p : fs::recursive_directory_iterator(input_path))
    {
        if (!p.is_regular_file())
            continue;

        std::string relative_path = fs::relative(p.path(), input_path).string();
        std::replace(relative_path.begin(), relative_path.end(), '\\', '/');

        std::string internal_zip_path = (is_scripts ? "scripts/" : "") + relative_path;

        uintmax_t sz = fs::file_size(p.path());
        std::cout << CYN << "    (+) " << RESET << std::left << std::setw(45) << internal_zip_path
                  << YEL << " [" << std::fixed << std::setprecision(1) << (sz / 1024.0) << " KB]" << RESET << std::endl;
        mz_zip_writer_add_file(&zip, internal_zip_path.c_str(), p.path().string().c_str(), nullptr, 0, MZ_BEST_COMPRESSION);
        file_count++;
    }

    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    std::cout << GREEN << "  [Done] " << BOLD << file_count << RESET << GREEN << " items -> " << output_filename << RESET << "\n" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << YEL << "Usage: BokkenPacker <input_dir> <output_path_prefix>" << RESET << std::endl;
        return 1;
    }

    fs::path input_dir = argv[1];
    std::string output_prefix = argv[2];

    if (!fs::exists(input_dir))
    {
        std::cerr << RED << "Error: Source path " << input_dir << " not found." << RESET << std::endl;
        return 1;
    }

    bool is_scripts = (output_prefix.find("scripts") != std::string::npos);

    if (is_scripts)
    {
        pack_folder(input_dir, output_prefix + ".assetpack", is_scripts);
        return 0;
    }

    bool found_tier = false;
    for (const auto &tier : QUALITY_TIERS)
    {
        fs::path tier_path = input_dir / tier;
        if (fs::exists(tier_path) && fs::is_directory(tier_path))
        {
            std::cout << BOLD << "Processing Tier: " << CYN << tier << RESET << std::endl;
            pack_folder(tier_path, output_prefix + "_" + tier + ".assetpack", is_scripts);
            found_tier = true;
        }
    }

    if (!found_tier)
    {
        pack_folder(input_dir, output_prefix + ".assetpack", is_scripts);
    }

    return 0;
}