#ifndef LOADERS_ENUMS_H
#define LOADERS_ENUMS_H

#include <cstdint>

namespace Loaders {

enum class Status : uint8_t {
    Ok = 0,
    OpenError,
    NotDir,
    MissingDirectory,
    JsonError,
    MissingJson,
    MissingMapFile,
    MissingChunkFile,
    MissingResourceFile,
    MissingResource,
    MissingRequirement,

    First = Ok,
    Last = MissingRequirement,
};
}

#endif // LOADERS_ENUMS_H
