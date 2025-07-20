#ifndef LOADERS_ENUMS_H
#define LOADERS_ENUMS_H

#include <cstdint>

namespace Loaders {

enum Status : uint8_t {
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

    First = Ok,
    Last = MissingResource,
};

}

#endif // LOADERS_ENUMS_H
