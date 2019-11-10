#ifndef PR4_DFS_UTILS_H
#define PR4_DFS_UTILS_H

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#define CRCPP_USE_CPP11
#include "CRC.h"

#define DFS_BUFFERSIZE 4096

/**
 * Clean the path and ensure it ends with a directory separator
 *
 * @param path
 * @return
 */
inline std::string dfs_clean_path(const std::string& path) {
    std::string sep = "/";
    int sep_len = sep.length();
    std::string mount_path = path;
    if (mount_path.length() >= 1 && (
                mount_path.compare(mount_path.length() - sep_len, sep_len, sep) != 0)) {
        mount_path += "/";
    }
    return mount_path;
}

/**
 * Calculate the crc checksum for a file
 *
 * @param filepath
 * @param table
 * @return
 */
inline std::uint32_t dfs_file_checksum(const std::string &filepath, CRC::Table<std::uint32_t, 32> *table) {

    struct stat st;
    size_t file_size;
    std::uint32_t crc = 0;
    std::ifstream stream;
    uint32_t chunk_count = 0;
    uint32_t chunk_sequence = 0;
    std::ifstream::pos_type current_position = 0;

    stream.seekg(0, std::ios::beg);
    if (lstat(filepath.c_str(), &st) != 0) {
        return 0;
    }

    file_size = st.st_size;

    std::uint32_t buffer_size = DFS_BUFFERSIZE;

    // The crc works better if we have
    // at least two chunks to work with
    if (file_size < DFS_BUFFERSIZE) {
        buffer_size = static_cast<uint32_t>(file_size / 2);
        if (buffer_size <= 0) {
            buffer_size = 1;
        }
    }

    char buffer[buffer_size];

    chunk_count = static_cast<uint32_t>(file_size / buffer_size) +
                  static_cast<uint32_t>(static_cast<bool>(file_size % buffer_size));

    stream.open(filepath, std::ios::in | std::ios::binary);

    if (!stream.is_open()) {
        return 0;
    }

    while(chunk_count != chunk_sequence) {
        size_t read_size = (file_size - current_position < buffer_size) ?
                           file_size - current_position :
                           buffer_size;

        if (!stream.read(buffer, read_size)) {
            return crc;

        }

        crc = CRC::Calculate(buffer, sizeof(char) * buffer_size, *table, crc);

        chunk_sequence++;
        current_position = stream.tellg();

    }

    return crc;

}

/**
 * Logging levels
 */
enum dfs_log_level_e {LL_SYSINFO, LL_ERROR, LL_DEBUG, LL_DEBUG2, LL_DEBUG3};

/**
 * Simple logging class
 *
 * This is a simple multi-tiered logging class
 * that can be used throughout the project.
 *
 * The `dfs_log` utility defined below provides the interface
 * and acts as a streaming input that you can send information to.
 *
 * You can set the log-level when you use the `dfs-client` or `dfs-server`
 * commands.
 *
 * NOTE: during testing, the log will only output DEBUG1 and up levels (i.e., LL_DEBUG, LL_ERROR, LL_SYSINFO)
 *
 * Usage:
 *
 *      dfs_log(LL_DEBUG) << "Type your message here: " << add_a_variable << ", and more info, etc."
 *
 */
class DFSLog
{
    private:
        std::ostringstream buffer;

    public:
        DFSLog(dfs_log_level_e level = LL_ERROR) {
#ifdef DFS_GRADER
            std::string desc = level == LL_SYSINFO ? "-S" : (level == LL_ERROR ? "!E" : ">D");
#else
            std::string desc = level == LL_SYSINFO ? "-- SYSINFO" : (level == LL_ERROR ? "!! ERROR" : ">> DEBUG");
#endif
            buffer << desc << ((level > 1) ? std::to_string(level - 1) : "") << ": ";
        }

        template <typename  T>
            DFSLog & operator<<(T const & value) {
                buffer << value;
                return *this;
            }

        ~DFSLog() {
            buffer << std::endl;
            std::cerr << buffer.str();
        }
};

/**
 * Log level defined in dfslib-shared-*.cpp
 */
extern dfs_log_level_e DFS_LOG_LEVEL;

/**
 * Utility function for logging details to std::cerr
 */
#define dfs_log(level) if (level > DFS_LOG_LEVEL) ; else DFSLog(level)

#endif //PR4_DFS_LOG_H
