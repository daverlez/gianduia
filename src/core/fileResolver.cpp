#include <gianduia/core/fileResolver.h>
#include <iostream>

namespace gnd {

    std::filesystem::path FileResolver::m_basePath = std::filesystem::current_path();
    std::filesystem::path FileResolver::m_outputPath = std::filesystem::current_path();
    std::string FileResolver::m_outputName = "output";

    void FileResolver::setBasePath(const std::filesystem::path& path) {
        try {
            std::filesystem::path absPath = std::filesystem::canonical(path);

            if (std::filesystem::is_regular_file(absPath)) {
                m_basePath = absPath.parent_path();
            } else {
                m_basePath = absPath;
            }

            std::cout << "FileResolver: Base path set to " << m_basePath << std::endl;

        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "FileResolver Error: Could not resolve path " << path << "\n"
                      << e.what() << std::endl;
        }
    }

    void FileResolver::setOutputPath(const std::filesystem::path& path) {
        try {
            std::filesystem::path absPath = std::filesystem::canonical(path);

            if (std::filesystem::is_regular_file(absPath)) {
                m_outputPath = absPath.parent_path();
            } else {
                m_outputPath = absPath;
            }

        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "FileResolver Error: Could not resolve path " << path << "\n"
                      << e.what() << std::endl;
        }
    }

    void FileResolver::setOutputName(const std::string& name) {
        std::filesystem::path p(name);
        m_outputName = p.stem().string();
    }

    std::filesystem::path FileResolver::getBasePath() {
        return m_basePath;
    }

    std::filesystem::path FileResolver::getOutputPath() {
        return m_outputPath;
    }

    std::string FileResolver::getOutputName() {
        return m_outputName;
    }

    std::filesystem::path FileResolver::resolve(const std::string& pathStr) {
        std::filesystem::path path(pathStr);

        if (path.is_absolute()) {
            return path;
        }

        return m_basePath / path;
    }

    std::string FileResolver::resolveStr(const std::string& path) {
        return resolve(path).string();
    }
}