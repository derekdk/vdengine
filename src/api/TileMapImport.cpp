#include <vde/VulkanContext.h>
#include <vde/api/TileMapImport.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>
namespace vde {

namespace {

using OrderedJson = nlohmann::ordered_json;

constexpr uint32_t kTiledFlipMask = 0xE0000000u;

struct ParsedTileSet {
    int firstGid = 1;
    int tileCount = 0;
    int columns = 0;
    int rows = 0;
    int spacingPx = 0;
    uint32_t imageWidth = 0;
    uint32_t imageHeight = 0;
    std::string imagePath;
    std::vector<std::pair<int, TileCollisionKind>> collisionKinds;
};

int getRequiredInt(const OrderedJson& object, const char* key, std::string_view context) {
    if (!object.contains(key) || !object.at(key).is_number_integer()) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " missing integer field: " + key);
    }

    return object.at(key).get<int>();
}

float getRequiredFloat(const OrderedJson& object, const char* key, std::string_view context) {
    if (!object.contains(key) || !object.at(key).is_number()) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " missing numeric field: " + key);
    }

    return object.at(key).get<float>();
}

std::string getRequiredString(const OrderedJson& object, const char* key,
                              std::string_view context) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " missing string field: " + key);
    }

    return object.at(key).get<std::string>();
}

bool getOptionalBool(const OrderedJson& object, const char* key, bool defaultValue) {
    if (!object.contains(key)) {
        return defaultValue;
    }
    if (!object.at(key).is_boolean()) {
        throw std::invalid_argument(std::string("TileMapImport expected boolean field: ") + key);
    }
    return object.at(key).get<bool>();
}

float getOptionalFloat(const OrderedJson& object, const char* key, float defaultValue,
                       std::string_view context) {
    if (!object.contains(key)) {
        return defaultValue;
    }
    if (!object.at(key).is_number()) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " expected numeric field: " + key);
    }
    return object.at(key).get<float>();
}

int getOptionalInt(const OrderedJson& object, const char* key, int defaultValue,
                   std::string_view context) {
    if (!object.contains(key)) {
        return defaultValue;
    }
    if (!object.at(key).is_number_integer()) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " expected integer field: " + key);
    }
    return object.at(key).get<int>();
}

std::string getOptionalString(const OrderedJson& object, const char* primaryKey,
                              const char* secondaryKey) {
    if (object.contains(primaryKey)) {
        if (!object.at(primaryKey).is_string()) {
            throw std::invalid_argument(std::string("TileMapImport expected string field: ") +
                                        primaryKey);
        }
        return object.at(primaryKey).get<std::string>();
    }
    if (secondaryKey != nullptr && object.contains(secondaryKey)) {
        if (!object.at(secondaryKey).is_string()) {
            throw std::invalid_argument(std::string("TileMapImport expected string field: ") +
                                        secondaryKey);
        }
        return object.at(secondaryKey).get<std::string>();
    }
    return {};
}

void validateImportOptions(const TileMapImportOptions& options) {
    if (options.tileWidth <= 0.0f || options.tileHeight <= 0.0f) {
        throw std::invalid_argument("TileMapImport tile dimensions must be positive");
    }
    if (options.layerDepthStep < 0.0f) {
        throw std::invalid_argument("TileMapImport layer depth step must be non-negative");
    }
}

void validateSupportedRoot(const OrderedJson& root) {
    if (!root.is_object()) {
        throw std::invalid_argument("TileMapImport requires a JSON object root");
    }

    const std::string type = getOptionalString(root, "type", nullptr);
    if (!type.empty() && type != "map") {
        throw std::invalid_argument("TileMapImport requires a Tiled map JSON root");
    }

    const std::string orientation = getRequiredString(root, "orientation", "map");
    if (orientation != "orthogonal") {
        throw std::invalid_argument("TileMapImport supports only finite orthogonal Tiled maps");
    }

    if (getOptionalBool(root, "infinite", false)) {
        throw std::invalid_argument("TileMapImport supports only finite orthogonal Tiled maps; "
                                    "infinite maps are unsupported");
    }

    const std::string renderOrder =
        root.contains("renderorder") ? getRequiredString(root, "renderorder", "map") : "right-down";
    if (renderOrder != "right-down") {
        throw std::invalid_argument("TileMapImport supports only Tiled renderorder 'right-down'");
    }
}

TileMapImportPropertyValue parsePropertyValue(const OrderedJson& property,
                                              std::string_view context) {
    if (!property.contains("value")) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " property is missing a value field");
    }

    const std::string declaredType = getOptionalString(property, "type", nullptr);
    const auto& value = property.at("value");

    if ((declaredType.empty() && value.is_boolean()) || declaredType == "bool") {
        if (!value.is_boolean()) {
            throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                        " property declared as bool but value is not boolean");
        }
        return value.get<bool>();
    }

    if ((declaredType.empty() && value.is_number_integer()) || declaredType == "int") {
        if (!value.is_number_integer()) {
            throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                        " property declared as int but value is not integer");
        }
        return value.get<int>();
    }

    if ((declaredType.empty() && value.is_number_float()) || declaredType == "float") {
        if (!value.is_number()) {
            throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                        " property declared as float but value is not numeric");
        }
        return value.get<float>();
    }

    if ((declaredType.empty() && value.is_string()) || declaredType == "string") {
        if (!value.is_string()) {
            throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                        " property declared as string but value is not textual");
        }
        return value.get<std::string>();
    }

    throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                " property type is unsupported; use bool, int, float, or string");
}

std::unordered_map<std::string, TileMapImportPropertyValue>
parseProperties(const OrderedJson& owner, std::string_view context) {
    std::unordered_map<std::string, TileMapImportPropertyValue> properties;
    const auto propertiesIt = owner.find("properties");
    if (propertiesIt == owner.end()) {
        return properties;
    }
    if (!propertiesIt->is_array()) {
        throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                    " properties field must be an array");
    }

    for (const auto& property : *propertiesIt) {
        if (!property.is_object()) {
            throw std::invalid_argument(std::string("TileMapImport ") + std::string(context) +
                                        " properties array must contain objects");
        }

        const std::string name = getRequiredString(property, "name", "property");
        properties.insert_or_assign(name, parsePropertyValue(property, context));
    }

    return properties;
}

TileCollisionKind parseCollisionString(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }

    if (value == "solid") {
        return TileCollisionKind::Solid;
    }
    if (value == "oneway" || value == "one-way" || value == "platform") {
        return TileCollisionKind::OneWay;
    }
    if (value == "none" || value.empty()) {
        return TileCollisionKind::None;
    }

    throw std::invalid_argument("TileMapImport unsupported collision kind: " + value);
}

bool tryParseCollisionString(const std::string& value, TileCollisionKind& outKind) {
    try {
        outKind = parseCollisionString(value);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

TileCollisionKind collisionKindFromProperties(
    const std::unordered_map<std::string, TileMapImportPropertyValue>& properties) {
    const auto it = properties.find("collision");
    if (it == properties.end()) {
        return TileCollisionKind::None;
    }

    if (!std::holds_alternative<std::string>(it->second)) {
        throw std::invalid_argument(
            "TileMapImport collision property must be a string value such as 'solid' or 'oneway'");
    }

    return parseCollisionString(std::get<std::string>(it->second));
}

ParsedTileSet parseTileSet(const OrderedJson& root, const std::shared_ptr<Texture>& texture) {
    if (!root.contains("tilesets") || !root.at("tilesets").is_array()) {
        throw std::invalid_argument("TileMapImport map is missing a tilesets array");
    }

    const auto& tilesets = root.at("tilesets");
    if (tilesets.size() != 1) {
        throw std::invalid_argument(
            "TileMapImport supports exactly one embedded image tileset in the current subset");
    }

    const auto& tileset = tilesets.at(0);
    if (!tileset.is_object()) {
        throw std::invalid_argument("TileMapImport tileset entry must be an object");
    }
    if (tileset.contains("source")) {
        throw std::invalid_argument(
            "TileMapImport does not support external .tsx tilesets in the current subset");
    }

    ParsedTileSet parsed;
    parsed.firstGid = getRequiredInt(tileset, "firstgid", "tileset");
    parsed.tileCount = getRequiredInt(tileset, "tilecount", "tileset");
    parsed.columns = getRequiredInt(tileset, "columns", "tileset");
    parsed.spacingPx = getOptionalInt(tileset, "spacing", 0, "tileset");
    const int marginPx = getOptionalInt(tileset, "margin", 0, "tileset");
    if (marginPx != 0) {
        throw std::invalid_argument(
            "TileMapImport does not support tileset margins; export with margin 0");
    }
    if (parsed.firstGid <= 0 || parsed.tileCount <= 0 || parsed.columns <= 0 ||
        parsed.spacingPx < 0) {
        throw std::invalid_argument("TileMapImport tileset metadata contains invalid dimensions");
    }
    if ((parsed.tileCount % parsed.columns) != 0) {
        throw std::invalid_argument(
            "TileMapImport requires a full grid tileset where tilecount divides evenly by columns");
    }

    parsed.rows = parsed.tileCount / parsed.columns;
    parsed.imagePath = getRequiredString(tileset, "image", "tileset");
    const int imageWidth = getRequiredInt(tileset, "imagewidth", "tileset");
    const int imageHeight = getRequiredInt(tileset, "imageheight", "tileset");
    if (imageWidth <= 0 || imageHeight <= 0) {
        throw std::invalid_argument("TileMapImport tileset image dimensions must be positive");
    }

    parsed.imageWidth = static_cast<uint32_t>(imageWidth);
    parsed.imageHeight = static_cast<uint32_t>(imageHeight);

    if (texture && texture->getWidth() != 0 && texture->getHeight() != 0 &&
        (texture->getWidth() != parsed.imageWidth || texture->getHeight() != parsed.imageHeight)) {
        throw std::invalid_argument("TileMapImport provided tileset texture dimensions do not "
                                    "match the Tiled image metadata");
    }

    const auto tilesIt = tileset.find("tiles");
    if (tilesIt != tileset.end()) {
        if (!tilesIt->is_array()) {
            throw std::invalid_argument("TileMapImport tileset tiles field must be an array");
        }

        for (const auto& tile : *tilesIt) {
            if (!tile.is_object()) {
                throw std::invalid_argument(
                    "TileMapImport tileset tile metadata must contain objects");
            }

            const int tileId = getRequiredInt(tile, "id", "tileset tile");
            if (tileId < 0 || tileId >= parsed.tileCount) {
                throw std::out_of_range(
                    "TileMapImport tileset tile id is outside the declared tilecount");
            }

            const auto properties = parseProperties(tile, "tileset tile");
            const TileCollisionKind collisionKind = collisionKindFromProperties(properties);
            if (collisionKind != TileCollisionKind::None) {
                const auto existing =
                    std::ranges::find_if(parsed.collisionKinds, [tileId](const auto& entry) {
                        return entry.first == tileId;
                    });
                if (existing != parsed.collisionKinds.end()) {
                    existing->second = collisionKind;
                } else {
                    parsed.collisionKinds.emplace_back(tileId, collisionKind);
                }
            }
        }
    }

    return parsed;
}

std::vector<int> parseLayerTiles(const OrderedJson& layer, const ParsedTileSet& tileset,
                                 int columns, int rows) {
    const std::string layerName = getRequiredString(layer, "name", "tile layer");

    if (layer.contains("encoding") || layer.contains("compression")) {
        throw std::invalid_argument(
            "TileMapImport tile layer '" + layerName +
            "' must use inline integer data arrays (no encoding/compression)");
    }
    if (layer.contains("chunks")) {
        throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                    "' uses chunked data; infinite maps are unsupported");
    }
    if (!layer.contains("data") || !layer.at("data").is_array()) {
        throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                    "' is missing an inline data array");
    }

    const int layerWidth = getOptionalInt(layer, "width", columns, "tile layer");
    const int layerHeight = getOptionalInt(layer, "height", rows, "tile layer");
    if (layerWidth != columns || layerHeight != rows) {
        throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                    "' dimensions must match the root map dimensions");
    }

    const float offsetX = getOptionalFloat(layer, "offsetx", 0.0f, "tile layer");
    const float offsetY = getOptionalFloat(layer, "offsety", 0.0f, "tile layer");
    if (offsetX != 0.0f || offsetY != 0.0f) {
        throw std::invalid_argument(
            "TileMapImport tile layer '" + layerName +
            "' uses layer offsets, which are unsupported in the current subset");
    }

    const float opacity = getOptionalFloat(layer, "opacity", 1.0f, "tile layer");
    if (std::abs(opacity - 1.0f) > 1e-4f) {
        throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                    "' uses opacity, which is unsupported by TileMap");
    }

    const auto& data = layer.at("data");
    const size_t expected = static_cast<size_t>(columns) * static_cast<size_t>(rows);
    if (data.size() != expected) {
        throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                    "' data size does not match the declared map dimensions");
    }

    std::vector<int> tiles(expected, TileMap::kEmptyTile);
    const auto firstGid = static_cast<uint32_t>(tileset.firstGid);
    const auto lastGid = static_cast<uint32_t>(tileset.firstGid + tileset.tileCount - 1);

    for (int sourceRow = 0; sourceRow < rows; ++sourceRow) {
        const int destRow = rows - 1 - sourceRow;
        for (int column = 0; column < columns; ++column) {
            const size_t sourceIndex =
                static_cast<size_t>(sourceRow) * static_cast<size_t>(columns) +
                static_cast<size_t>(column);
            const auto& gidValue = data.at(sourceIndex);
            if (!gidValue.is_number_unsigned() && !gidValue.is_number_integer()) {
                throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                            "' contains a non-integer tile GID");
            }

            uint32_t rawGid = 0u;
            if (gidValue.is_number_integer()) {
                const int64_t signedGid = gidValue.get<int64_t>();
                if (signedGid < 0) {
                    throw std::invalid_argument("TileMapImport tile layer '" + layerName +
                                                "' contains a negative tile GID");
                }
                rawGid = static_cast<uint32_t>(signedGid);
            } else {
                rawGid = gidValue.get<uint32_t>();
            }
            if ((rawGid & kTiledFlipMask) != 0u) {
                throw std::invalid_argument(
                    "TileMapImport does not support flipped or rotated Tiled tile GIDs on layer '" +
                    layerName + "'");
            }

            int importedTileId = TileMap::kEmptyTile;
            if (rawGid != 0u) {
                if (rawGid < firstGid || rawGid > lastGid) {
                    throw std::invalid_argument("TileMapImport encountered a tile GID outside the "
                                                "supported tileset range on layer '" +
                                                layerName + "'");
                }
                importedTileId = static_cast<int>(rawGid - firstGid);
            }

            const size_t destIndex = static_cast<size_t>(destRow) * static_cast<size_t>(columns) +
                                     static_cast<size_t>(column);
            tiles.at(destIndex) = importedTileId;
        }
    }

    return tiles;
}

ImportedTileObject parseObject(const OrderedJson& object, const std::string& layerName,
                               int mapPixelWidth, int mapPixelHeight, float unitScaleX,
                               float unitScaleY) {
    if (!object.is_object()) {
        throw std::invalid_argument("TileMapImport object layer must contain object entries");
    }

    if (object.contains("gid")) {
        throw std::invalid_argument("TileMapImport does not support tile objects in object layers");
    }
    if ((object.contains("ellipse") && getOptionalBool(object, "ellipse", false)) ||
        object.contains("polygon") || object.contains("polyline") || object.contains("text")) {
        throw std::invalid_argument(
            "TileMapImport supports only point and rectangle objects in the current subset");
    }

    ImportedTileObject imported;
    imported.id = getRequiredInt(object, "id", "object");
    imported.name = getOptionalString(object, "name", nullptr);
    imported.type = getOptionalString(object, "type", "class");
    imported.layerName = layerName;
    imported.point = getOptionalBool(object, "point", false);
    imported.visible = getOptionalBool(object, "visible", true);
    imported.rotationDegrees = getOptionalFloat(object, "rotation", 0.0f, "object");
    imported.properties = parseProperties(object, "object");

    const float xPixels = getRequiredFloat(object, "x", "object");
    const float yPixels = getRequiredFloat(object, "y", "object");
    const float widthPixels = getOptionalFloat(object, "width", 0.0f, "object");
    const float heightPixels = getOptionalFloat(object, "height", 0.0f, "object");
    if (widthPixels < 0.0f || heightPixels < 0.0f) {
        throw std::invalid_argument("TileMapImport object width and height must be non-negative");
    }

    if (imported.point) {
        imported.position.x = xPixels * unitScaleX;
        imported.position.y = (static_cast<float>(mapPixelHeight) - yPixels) * unitScaleY;
    } else {
        imported.size = glm::vec2(widthPixels * unitScaleX, heightPixels * unitScaleY);
        imported.position.x = xPixels * unitScaleX;
        imported.position.y =
            (static_cast<float>(mapPixelHeight) - (yPixels + heightPixels)) * unitScaleY;
    }

    imported.collisionKind = collisionKindFromProperties(imported.properties);
    if (imported.collisionKind == TileCollisionKind::None && !imported.type.empty()) {
        TileCollisionKind parsedKind = TileCollisionKind::None;
        if (tryParseCollisionString(imported.type, parsedKind)) {
            imported.collisionKind = parsedKind;
        }
    }

    const float maxXPixels = imported.point ? xPixels : (xPixels + widthPixels);
    const float maxYPixels = imported.point ? yPixels : (yPixels + heightPixels);
    if (xPixels < 0.0f || yPixels < 0.0f || maxXPixels > static_cast<float>(mapPixelWidth) ||
        maxYPixels > static_cast<float>(mapPixelHeight)) {
        throw std::invalid_argument(
            "TileMapImport objects must stay within the imported map bounds");
    }

    return imported;
}

void parseObjectLayer(const OrderedJson& layer, ImportedTileMap& imported, int mapPixelWidth,
                      int mapPixelHeight, float unitScaleX, float unitScaleY) {
    const std::string layerName = getRequiredString(layer, "name", "object layer");
    if (!layer.contains("objects") || !layer.at("objects").is_array()) {
        throw std::invalid_argument("TileMapImport object layer '" + layerName +
                                    "' is missing an objects array");
    }

    const float offsetX = getOptionalFloat(layer, "offsetx", 0.0f, "object layer");
    const float offsetY = getOptionalFloat(layer, "offsety", 0.0f, "object layer");
    if (offsetX != 0.0f || offsetY != 0.0f) {
        throw std::invalid_argument(
            "TileMapImport object layer '" + layerName +
            "' uses layer offsets, which are unsupported in the current subset");
    }

    for (const auto& object : layer.at("objects")) {
        imported.objects.push_back(
            parseObject(object, layerName, mapPixelWidth, mapPixelHeight, unitScaleX, unitScaleY));
    }
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("TileMapImport failed to open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

ImportedTileMap importTiledJsonImpl(const std::shared_ptr<Texture>& texture,
                                    const std::string& jsonText,
                                    const TileMapImportOptions& options) {
    if (!texture) {
        throw std::invalid_argument("TileMapImport requires a non-null tileset texture");
    }
    if (jsonText.empty()) {
        throw std::invalid_argument("TileMapImport requires non-empty JSON text");
    }

    validateImportOptions(options);

    OrderedJson root;
    try {
        root = OrderedJson::parse(jsonText);
    } catch (const nlohmann::json::parse_error& ex) {
        throw std::invalid_argument(std::string("TileMapImport failed to parse JSON text: ") +
                                    ex.what());
    }
    validateSupportedRoot(root);
    const int columns = getRequiredInt(root, "width", "map");
    const int rows = getRequiredInt(root, "height", "map");
    const int tilePixelWidth = getRequiredInt(root, "tilewidth", "map");
    const int tilePixelHeight = getRequiredInt(root, "tileheight", "map");
    if (columns <= 0 || rows <= 0 || tilePixelWidth <= 0 || tilePixelHeight <= 0) {
        throw std::invalid_argument("TileMapImport map dimensions must be positive");
    }

    if (!root.contains("layers") || !root.at("layers").is_array()) {
        throw std::invalid_argument("TileMapImport map is missing a layers array");
    }

    const ParsedTileSet tileset = parseTileSet(root, texture);

    ImportedTileMap imported;
    imported.tileMap =
        std::make_shared<TileMap>(options.tileWidth, options.tileHeight, columns, rows);
    imported.tileMap->setTileSet(
        SpriteSheet::createGrid(texture, tileset.columns, tileset.rows, tileset.spacingPx));
    for (const auto& [tileId, collisionKind] : tileset.collisionKinds) {
        imported.tileMap->setCollisionKind(tileId, collisionKind);
    }

    const int mapPixelWidth = columns * tilePixelWidth;
    const int mapPixelHeight = rows * tilePixelHeight;
    const float unitScaleX = options.tileWidth / static_cast<float>(tilePixelWidth);
    const float unitScaleY = options.tileHeight / static_cast<float>(tilePixelHeight);

    int tileLayerCount = 0;
    for (const auto& layer : root.at("layers")) {
        if (!layer.is_object()) {
            throw std::invalid_argument("TileMapImport layer entries must be objects");
        }

        const std::string type = getRequiredString(layer, "type", "layer");
        if (type == "tilelayer") {
            const std::string name = getRequiredString(layer, "name", "tile layer");
            const std::vector<int> tiles = parseLayerTiles(layer, tileset, columns, rows);
            const int layerIndex = (tileLayerCount == 0) ? 0 : imported.tileMap->addLayer(name);
            if (tileLayerCount == 0) {
                imported.tileMap->setLayerName(0, name);
            }
            imported.tileMap->setLayerVisible(layerIndex, getOptionalBool(layer, "visible", true));
            imported.tileMap->setLayerDepth(layerIndex, static_cast<float>(tileLayerCount) *
                                                            options.layerDepthStep);
            imported.tileMap->loadLayerFromArray(layerIndex, tiles);
            ++tileLayerCount;
            continue;
        }

        if (type == "objectgroup") {
            if (options.importObjectLayers) {
                parseObjectLayer(layer, imported, mapPixelWidth, mapPixelHeight, unitScaleX,
                                 unitScaleY);
            }
            continue;
        }

        throw std::invalid_argument("TileMapImport does not support Tiled layer type '" + type +
                                    "' in the current subset");
    }

    if (tileLayerCount == 0) {
        throw std::invalid_argument("TileMapImport requires at least one tile layer");
    }

    return imported;
}

}  // namespace

ImportedTileMap TileMapImport::importTiledJson(const std::shared_ptr<Texture>& texture,
                                               const std::string& jsonText,
                                               const TileMapImportOptions& options) {
    return importTiledJsonImpl(texture, jsonText, options);
}

ImportedTileMap TileMapImport::importTiledJsonFile(VulkanContext* context,
                                                   const std::string& jsonPath,
                                                   const TileMapImportOptions& options) {
    if (jsonPath.empty()) {
        throw std::invalid_argument("TileMapImport requires a non-empty JSON file path");
    }

    const std::filesystem::path path(jsonPath);
    const std::string fileText = readTextFile(path);
    OrderedJson root = OrderedJson::parse(fileText);
    validateSupportedRoot(root);

    auto texture = std::make_shared<Texture>();
    const ParsedTileSet tileset = parseTileSet(root, nullptr);
    (void)tileset;

    const auto& tilesetJson = root.at("tilesets").at(0);
    const std::filesystem::path imagePath =
        path.parent_path() / getRequiredString(tilesetJson, "image", "tileset");
    if (!texture->loadFromFile(imagePath.string())) {
        throw std::runtime_error("TileMapImport failed to load tileset image: " +
                                 imagePath.string());
    }
    if (context != nullptr && !texture->uploadToGPU(context)) {
        throw std::runtime_error("TileMapImport failed to upload tileset image to the GPU: " +
                                 imagePath.string());
    }

    return importTiledJsonImpl(texture, fileText, options);
}

}  // namespace vde