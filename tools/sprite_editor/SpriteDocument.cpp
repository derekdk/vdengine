/**
 * @file SpriteDocument.cpp
 * @brief Implementation of the SpriteDocument in-memory model and TOML
 *        serialization for the Sprite Editor.
 */

#include "SpriteDocument.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <toml++/toml.hpp>

namespace vde {
namespace tools {

// ── AnimationSequence helpers ────────────────────────────────────

float AnimationSequence::getTotalDuration() const {
    float total = 0.0f;
    for (const auto& f : frames) {
        total += f.duration;
    }
    return total;
}

const std::string& AnimationSequence::getFrameAt(float elapsed) const {
    static const std::string kEmpty;
    if (frames.empty())
        return kEmpty;

    float totalDur = getTotalDuration();
    if (totalDur <= 0.0f)
        return frames.front().spriteName;

    // Wrap elapsed time for looping animations.
    if (looping && elapsed >= totalDur) {
        elapsed = std::fmod(elapsed, totalDur);
    }

    // Clamp for non-looping.
    if (elapsed < 0.0f)
        elapsed = 0.0f;
    if (!looping && elapsed >= totalDur)
        return frames.back().spriteName;

    float accum = 0.0f;
    for (const auto& f : frames) {
        accum += f.duration;
        if (elapsed < accum) {
            return f.spriteName;
        }
    }
    return frames.back().spriteName;
}

// ── Source image ─────────────────────────────────────────────────

void SpriteDocument::setSourceImage(const std::string& path, int width, int height) {
    if (width <= 0 || height <= 0)
        return;
    m_imagePath = path;
    m_imageWidth = width;
    m_imageHeight = height;
}

// ── Sprite regions ──────────────────────────────────────────────

int SpriteDocument::gridSlice(int cellWidth, int cellHeight, int spacingX, int spacingY,
                              int offsetX, int offsetY) {
    if (!hasImage() || cellWidth <= 0 || cellHeight <= 0)
        return 0;

    m_sprites.clear();

    int cols = 0;
    for (int x = offsetX; x + cellWidth <= m_imageWidth; x += cellWidth + spacingX) {
        ++cols;
    }
    int rows = 0;
    for (int y = offsetY; y + cellHeight <= m_imageHeight; y += cellHeight + spacingY) {
        ++rows;
    }

    int index = 0;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            SpriteRegion r;
            r.name = "sprite_" + std::to_string(index);
            r.x = offsetX + col * (cellWidth + spacingX);
            r.y = offsetY + row * (cellHeight + spacingY);
            r.w = cellWidth;
            r.h = cellHeight;
            m_sprites.push_back(r);
            ++index;
        }
    }
    return index;
}

bool SpriteDocument::addSprite(const SpriteRegion& region) {
    if (findSprite(region.name))
        return false;
    if (region.w <= 0 || region.h <= 0)
        return false;
    if (hasImage()) {
        if (region.x < 0 || region.y < 0 || region.x + region.w > m_imageWidth ||
            region.y + region.h > m_imageHeight)
            return false;
    }
    m_sprites.push_back(region);
    return true;
}

bool SpriteDocument::removeSprite(const std::string& name) {
    auto it = std::find_if(m_sprites.begin(), m_sprites.end(),
                           [&](const SpriteRegion& r) { return r.name == name; });
    if (it == m_sprites.end())
        return false;
    m_sprites.erase(it);
    return true;
}

bool SpriteDocument::renameSprite(const std::string& oldName, const std::string& newName) {
    if (findSprite(newName))
        return false;
    auto* s = findSpriteMut(oldName);
    if (!s)
        return false;
    s->name = newName;
    return true;
}

bool SpriteDocument::setAnchor(const std::string& name, float ax, float ay) {
    auto* s = findSpriteMut(name);
    if (!s)
        return false;
    s->anchorX = std::clamp(ax, 0.0f, 1.0f);
    s->anchorY = std::clamp(ay, 0.0f, 1.0f);
    return true;
}

const SpriteRegion* SpriteDocument::findSprite(const std::string& name) const {
    for (const auto& s : m_sprites) {
        if (s.name == name)
            return &s;
    }
    return nullptr;
}

void SpriteDocument::clearSprites() {
    m_sprites.clear();
}

SpriteRegion* SpriteDocument::findSpriteMut(const std::string& name) {
    for (auto& s : m_sprites) {
        if (s.name == name)
            return &s;
    }
    return nullptr;
}

// ── Animations ──────────────────────────────────────────────────

bool SpriteDocument::createAnimation(const std::string& name, bool looping) {
    if (findAnimation(name))
        return false;
    AnimationSequence seq;
    seq.name = name;
    seq.looping = looping;
    m_animations.push_back(std::move(seq));
    return true;
}

bool SpriteDocument::deleteAnimation(const std::string& name) {
    auto it = std::find_if(m_animations.begin(), m_animations.end(),
                           [&](const AnimationSequence& a) { return a.name == name; });
    if (it == m_animations.end())
        return false;
    m_animations.erase(it);
    return true;
}

bool SpriteDocument::addFrame(const std::string& animName, const std::string& spriteName,
                              float duration) {
    if (duration <= 0.0f)
        return false;
    auto* anim = findAnimationMut(animName);
    if (!anim)
        return false;
    if (!findSprite(spriteName))
        return false;
    AnimationFrame frame;
    frame.spriteName = spriteName;
    frame.duration = duration;
    anim->frames.push_back(frame);
    return true;
}

bool SpriteDocument::removeFrame(const std::string& animName, int index) {
    auto* anim = findAnimationMut(animName);
    if (!anim)
        return false;
    if (index < 0 || index >= static_cast<int>(anim->frames.size()))
        return false;
    anim->frames.erase(anim->frames.begin() + index);
    return true;
}

bool SpriteDocument::setFrameDuration(const std::string& animName, int index, float duration) {
    if (duration <= 0.0f)
        return false;
    auto* anim = findAnimationMut(animName);
    if (!anim)
        return false;
    if (index < 0 || index >= static_cast<int>(anim->frames.size()))
        return false;
    anim->frames[static_cast<size_t>(index)].duration = duration;
    return true;
}

const AnimationSequence* SpriteDocument::findAnimation(const std::string& name) const {
    for (const auto& a : m_animations) {
        if (a.name == name)
            return &a;
    }
    return nullptr;
}

void SpriteDocument::clearAnimations() {
    m_animations.clear();
}

AnimationSequence* SpriteDocument::findAnimationMut(const std::string& name) {
    for (auto& a : m_animations) {
        if (a.name == name)
            return &a;
    }
    return nullptr;
}

// ── Serialization ───────────────────────────────────────────────

std::string SpriteDocument::serializeToString() const {
    toml::table root;

    // [sheet]
    toml::table sheet;
    sheet.insert("image", m_imagePath);
    sheet.insert("image_width", m_imageWidth);
    sheet.insert("image_height", m_imageHeight);
    if (!m_author.empty())
        sheet.insert("author", m_author);
    if (!m_notes.empty())
        sheet.insert("notes", m_notes);
    root.insert("sheet", std::move(sheet));

    // [[sprites]]
    toml::array spriteArr;
    for (const auto& s : m_sprites) {
        toml::table t;
        t.insert("name", s.name);
        t.insert("x", s.x);
        t.insert("y", s.y);
        t.insert("w", s.w);
        t.insert("h", s.h);
        if (s.anchorX != 0.5f || s.anchorY != 0.5f) {
            t.insert("anchor_x", static_cast<double>(s.anchorX));
            t.insert("anchor_y", static_cast<double>(s.anchorY));
        }
        spriteArr.push_back(std::move(t));
    }
    root.insert("sprites", std::move(spriteArr));

    // [[animations]]
    toml::array animArr;
    for (const auto& a : m_animations) {
        toml::table at;
        at.insert("name", a.name);
        at.insert("looping", a.looping);

        toml::array frameArr;
        for (const auto& f : a.frames) {
            toml::table ft;
            ft.insert("sprite", f.spriteName);
            ft.insert("duration", static_cast<double>(f.duration));
            frameArr.push_back(std::move(ft));
        }
        at.insert("frames", std::move(frameArr));
        animArr.push_back(std::move(at));
    }
    root.insert("animations", std::move(animArr));

    std::ostringstream oss;
    oss << root;
    return oss.str();
}

bool SpriteDocument::deserializeFromString(const std::string& tomlStr) {
    try {
        auto root = toml::parse(tomlStr);

        // [sheet]
        if (auto* sheet = root["sheet"].as_table()) {
            m_imagePath = sheet->get("image")->value_or(std::string{});
            m_imageWidth = sheet->get("image_width")->value_or(0);
            m_imageHeight = sheet->get("image_height")->value_or(0);
            if (auto* a = sheet->get("author"))
                m_author = a->value_or(std::string{});
            if (auto* n = sheet->get("notes"))
                m_notes = n->value_or(std::string{});
        }

        // [[sprites]]
        m_sprites.clear();
        if (auto* sprites = root["sprites"].as_array()) {
            for (auto& elem : *sprites) {
                if (auto* t = elem.as_table()) {
                    SpriteRegion r;
                    r.name = t->get("name")->value_or(std::string{});
                    r.x = t->get("x")->value_or(0);
                    r.y = t->get("y")->value_or(0);
                    r.w = t->get("w")->value_or(0);
                    r.h = t->get("h")->value_or(0);
                    if (auto* ax = t->get("anchor_x"))
                        r.anchorX = std::clamp(static_cast<float>(ax->value_or(0.5)), 0.0f, 1.0f);
                    if (auto* ay = t->get("anchor_y"))
                        r.anchorY = std::clamp(static_cast<float>(ay->value_or(0.5)), 0.0f, 1.0f);
                    m_sprites.push_back(r);
                }
            }
        }

        // [[animations]]
        m_animations.clear();
        if (auto* anims = root["animations"].as_array()) {
            for (auto& elem : *anims) {
                if (auto* at = elem.as_table()) {
                    AnimationSequence seq;
                    seq.name = at->get("name")->value_or(std::string{});
                    seq.looping = at->get("looping")->value_or(true);

                    if (auto* frames = at->get("frames")->as_array()) {
                        for (auto& fElem : *frames) {
                            if (auto* ft = fElem.as_table()) {
                                AnimationFrame frame;
                                frame.spriteName = ft->get("sprite")->value_or(std::string{});
                                frame.duration =
                                    static_cast<float>(ft->get("duration")->value_or(0.1));
                                seq.frames.push_back(frame);
                            }
                        }
                    }
                    m_animations.push_back(std::move(seq));
                }
            }
        }

        return true;
    } catch (const toml::parse_error&) {
        return false;
    }
}

bool SpriteDocument::saveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << serializeToString();
    return file.good();
}

bool SpriteDocument::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::ostringstream oss;
    oss << file.rdbuf();
    return deserializeFromString(oss.str());
}

}  // namespace tools
}  // namespace vde
