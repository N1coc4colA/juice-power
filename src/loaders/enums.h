#ifndef JP_LOADERS_ENUMS_H
#define JP_LOADERS_ENUMS_H

#include <cstdint>

namespace Loaders {

/**
 * @brief Return status enum used for map & res loadings.
 */
enum class Status : uint8_t {
    Ok = 0,              ///< Everything went well.
    OpenError,           ///< The opening of a resource failed.
    NotDir,              ///< A supplied path should have been a dir, but was something else.
    MissingDirectory,    ///< A directory should exist but does not.
    JsonError,           ///< An error occurred while parsing a JSON file.
    MissingJson,         ///< A JSON file is missing.
    MissingMapFile,      ///< The map file (JSON) is missing.
    MissingChunkFile,    ///< A map chunk file (JSON) is missing.
    MissingResourceFile, ///< A resource file (most likely image) is missing at the given path.
    MissingResource,     ///< The resource that has been requested does not exist.
    MissingRequirement,  ///< A requirement has not been fulfilled (such as image size bounds).

    First = Ok,
    Last = MissingRequirement,
};
}

#endif // JP_LOADERS_ENUMS_H
