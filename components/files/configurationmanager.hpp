#ifndef COMPONENTS_FILES_CONFIGURATIONMANAGER_HPP
#define COMPONENTS_FILES_CONFIGURATIONMANAGER_HPP

#ifdef _WIN32
#include <boost/tr1/tr1/unordered_map>
#else
#include <tr1/unordered_map>
#endif

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <components/files/fixedpath.hpp>
#include <components/files/collections.hpp>

/**
 * \namespace Files
 */
namespace Files
{

/**
 * \struct ConfigurationManager
 */
struct ConfigurationManager
{
    ConfigurationManager();
    virtual ~ConfigurationManager();

    void readConfiguration(boost::program_options::variables_map& variables,
        boost::program_options::options_description& description);
    void processPaths(Files::PathContainer& dataDirs);

    /**< Fixed paths */
    const boost::filesystem::path& getGlobalPath() const;
    const boost::filesystem::path& getUserPath() const;
    const boost::filesystem::path& getLocalPath() const;

    const boost::filesystem::path& getGlobalDataPath() const;
    const boost::filesystem::path& getUserDataPath() const;
    const boost::filesystem::path& getLocalDataPath() const;
    const boost::filesystem::path& getInstallPath() const;

    const boost::filesystem::path& getOgreConfigPath() const;
    const boost::filesystem::path& getPluginsConfigPath() const;
    const boost::filesystem::path& getLogPath() const;

    private:
        typedef Files::FixedPath<> FixedPathType;

        typedef const boost::filesystem::path& (FixedPathType::*path_type_f)() const;
        typedef std::tr1::unordered_map<std::string, path_type_f> TokensMappingContainer;

        void loadConfig(const boost::filesystem::path& path,
            boost::program_options::variables_map& variables,
            boost::program_options::options_description& description);

        void setupTokensMapping();

        FixedPathType mFixedPath;

        boost::filesystem::path mOgreCfgPath;
        boost::filesystem::path mPluginsCfgPath;
        boost::filesystem::path mLogPath;

        TokensMappingContainer mTokensMapping;
};

} /* namespace Cfg */

#endif /* COMPONENTS_FILES_CONFIGURATIONMANAGER_HPP */
