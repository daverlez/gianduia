#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace gnd {

    /// Static class to handle the resolution of relative paths.
    /// Holds a reference to the base folder, i.e. the one of the currently rendered scene.
    class FileResolver {
    public:
        /// Sets the current working directory (called by the parser).
        static void setBasePath(const std::filesystem::path& path);

        /// Sets the path to output file
        static void setOutputPath(const std::filesystem::path& path);

        /// Sets output file name
        static void setOutputName(const std::string& name);

        /// Returns the current working directory.
        static std::filesystem::path getBasePath();

        /// Returns the path to output file
        static std::filesystem::path getOutputPath();

        /// Returns output file name
        static std::string getOutputName();

        /// Resolves a path into an absolute path.
        static std::filesystem::path resolve(const std::string& path);

        /// Resolves a path into an absolute path, returning the corresponding string.
        static std::string resolveStr(const std::string& path);

    private:
        static std::filesystem::path m_basePath;
        static std::filesystem::path m_outputPath;
        static std::string m_outputName;
    };

}