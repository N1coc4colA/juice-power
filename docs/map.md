# Map element documentation
Every map is described as JSON.

## Directory architecture
```
maps
  +- <mapId>
       +- assets
           +- ...
       +- map.json
      (+- resources.json)
      (+- <chunkId>.json)
```

## Map
|Name|Type|Default|Description|
|--|--|--|--|
|name|string|"<map>"|Name of the map|
|chunksExternal|boolean|false|Tells if the chunks are in standalone files. If false, "chunks" field should be provided.|
|resourcesExternal|boolean|false|Tells if the resources descriptions are in a standalone file. If false, "resources" field should be provided.|
|chunksCount|integer|0|Must be provided if chunksExternal is true.|
|chunks|array|[]|Array of chunk objects, see following Chunk section.|
|resources|map|{}|Resources object, see following Resources section.|

## Resources
The Resources object is an array of the resource elements.
Each resource must be described as follows:
|Name|Type|Description|
|--|--|--|
|name|string|Name of the resource|
|w|float|Width of the resource (image)|
|h|float|Height of the resource (image)|
|source|string|Path to the image, relative to __./assets__|
|type|integer|Resource type in the game|

## Chunk
Each chunk is an array of elements that must be described as follows:
|Name|Type|Default|Description|
|--|--|--|--|
|type|string||How the resource is described, corresponding to indexed element in the Resources element.|
|position|array<float, 3>||X, Y, Z position within the chunk. Z position within the world, used for rendering layering.|
|ignores|boolean|false|Tells if the physics engine must ignore this element.|
