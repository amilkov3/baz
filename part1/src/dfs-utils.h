#ifndef PR4_DFS_UTILS_H
#define PR4_DFS_UTILS_H

#include <sstream>
#include <string>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>

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
