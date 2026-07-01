#include <vde/api/SpriteAnimationImport.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace vde {

namespace {

using OrderedJson = nlohmann::ordered_json;

struct ImportedFrameRecord {
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float durationSec = 0.1f;
};

int getRequiredInt(const OrderedJson& object, const char* key) {
    if (!object.contains(key) || !object.at(key).is_number_integer()) {
        throw std::invalid_argument(std::string("SpriteAnimationImport missing integer field: ") +
                                    key);
    }

    return object.at(key).get<int>();
}

std::string getRequiredString(const OrderedJson& object, const char* key) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        throw std::invalid_argument(std::string("SpriteAnimationImport missing string field: ") +
                                    key);
    }

    return object.at(key).get<std::string>();
}

ImportedFrameRecord parseFrameRecord(const std::string& name, const OrderedJson& frameObject) {
    if (!frameObject.contains("frame") || !frameObject.at("frame").is_object()) {
        throw std::invalid_argument(
            "SpriteAnimationImport Aseprite frame entry missing frame rect");
    }

    const auto& rect = frameObject.at("frame");
    ImportedFrameRecord record;
    record.name = name;
    record.x = getRequiredInt(rect, "x");
    record.y = getRequiredInt(rect, "y");
    record.width = getRequiredInt(rect, "w");
    record.height = getRequiredInt(rect, "h");

    if (record.width <= 0 || record.height <= 0) {
        throw std::invalid_argument("SpriteAnimationImport frame dimensions must be positive");
    }

    if (frameObject.contains("duration") && frameObject.at("duration").is_number()) {
        record.durationSec = frameObject.at("duration").get<float>() / 1000.0f;
    }

    return record;
}

std::vector<ImportedFrameRecord> parseAsepriteFrames(const OrderedJson& root) {
    if (!root.contains("frames")) {
        throw std::invalid_argument("SpriteAnimationImport Aseprite JSON missing frames block");
    }

    const auto& frames = root.at("frames");
    std::vector<ImportedFrameRecord> records;

    if (frames.is_array()) {
        records.reserve(frames.size());
        for (const auto& frameObject : frames) {
            if (!frameObject.is_object()) {
                throw std::invalid_argument(
                    "SpriteAnimationImport Aseprite frames array must contain objects");
            }
            records.push_back(
                parseFrameRecord(getRequiredString(frameObject, "filename"), frameObject));
        }
        return records;
    }

    if (frames.is_object()) {
        records.reserve(frames.size());
        for (auto it = frames.begin(); it != frames.end(); ++it) {
            if (!it.value().is_object()) {
                throw std::invalid_argument(
                    "SpriteAnimationImport Aseprite frames object must map names to objects");
            }
            records.push_back(parseFrameRecord(it.key(), it.value()));
        }
        return records;
    }

    throw std::invalid_argument("SpriteAnimationImport Aseprite frames must be an array or object");
}

void appendFrameRange(SpriteAnimation& animation, const std::vector<ImportedFrameRecord>& frames,
                      int from, int to, const std::string& direction) {
    auto appendFrame = [&animation, &frames](int index) {
        if (index < 0 || index >= static_cast<int>(frames.size())) {
            throw std::out_of_range("SpriteAnimationImport frame tag index out of range");
        }
        animation.addFrame(index, frames.at(static_cast<size_t>(index)).durationSec);
    };

    if (direction == "reverse") {
        for (int index = to; index >= from; --index) {
            appendFrame(index);
        }
        return;
    }

    if (direction == "pingpong") {
        for (int index = from; index <= to; ++index) {
            appendFrame(index);
        }
        for (int index = to - 1; index > from; --index) {
            appendFrame(index);
        }
        return;
    }

    if (direction == "pingpong_reverse") {
        for (int index = to; index >= from; --index) {
            appendFrame(index);
        }
        for (int index = from + 1; index < to; ++index) {
            appendFrame(index);
        }
        return;
    }

    for (int index = from; index <= to; ++index) {
        appendFrame(index);
    }
}

std::string readTextFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("SpriteAnimationImport failed to open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

ImportedSpriteAnimationSet
SpriteAnimationImport::importAsepriteJson(std::shared_ptr<Texture> texture,
                                          const std::string& jsonText) {
    if (!texture) {
        throw std::invalid_argument("SpriteAnimationImport requires a non-null texture");
    }
    if (jsonText.empty()) {
        throw std::invalid_argument("SpriteAnimationImport requires non-empty JSON text");
    }

    OrderedJson root = OrderedJson::parse(jsonText);
    if (!root.is_object()) {
        throw std::invalid_argument("SpriteAnimationImport requires a JSON object root");
    }
    auto frames = parseAsepriteFrames(root);

    ImportedSpriteAnimationSet imported;
    imported.spriteSheet = SpriteSheet::create(texture);
    imported.frameNames.reserve(frames.size());

    for (const auto& frame : frames) {
        imported.spriteSheet->addSprite(frame.name, frame.x, frame.y, frame.width, frame.height);
        imported.frameNames.push_back(frame.name);
    }

    const auto metaIt = root.find("meta");
    if (metaIt != root.end() && metaIt->is_object() && metaIt->contains("frameTags") &&
        metaIt->at("frameTags").is_array() && !metaIt->at("frameTags").empty()) {
        for (const auto& tag : metaIt->at("frameTags")) {
            if (!tag.is_object()) {
                throw std::invalid_argument(
                    "SpriteAnimationImport Aseprite frameTags must contain objects");
            }

            const std::string name = getRequiredString(tag, "name");
            const int from = getRequiredInt(tag, "from");
            const int to = getRequiredInt(tag, "to");
            const std::string direction =
                tag.contains("direction") && tag.at("direction").is_string()
                    ? tag.at("direction").get<std::string>()
                    : "forward";

            if (from < 0 || to < from || to >= static_cast<int>(frames.size())) {
                throw std::out_of_range("SpriteAnimationImport Aseprite frameTag range is invalid");
            }

            SpriteAnimation animation(name);
            appendFrameRange(animation, frames, from, to, direction);
            imported.animations.insert_or_assign(name, std::move(animation));
        }
    } else {
        for (size_t index = 0; index < frames.size(); ++index) {
            SpriteAnimation animation(frames.at(index).name);
            animation.addFrame(static_cast<int>(index), frames.at(index).durationSec);
            imported.animations.insert_or_assign(frames.at(index).name, std::move(animation));
        }
    }

    return imported;
}

ImportedSpriteAnimationSet
SpriteAnimationImport::importAsepriteJsonFile(std::shared_ptr<Texture> texture,
                                              const std::string& jsonPath) {
    if (jsonPath.empty()) {
        throw std::invalid_argument("SpriteAnimationImport requires a non-empty JSON file path");
    }

    return importAsepriteJson(std::move(texture), readTextFile(jsonPath));
}

}  // namespace vde