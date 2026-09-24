#include "Graphics.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace foxlang::graphics {
namespace {

std::vector<std::string> characters(const std::string& text) {
    std::vector<std::string> out;
    for (size_t i = 0; i < text.size();) {
        unsigned char lead = static_cast<unsigned char>(text[i]);
        size_t length = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
        length = std::min(length, text.size() - i);
        out.push_back(text.substr(i, length));
        i += length;
    }
    return out;
}

std::string join(const std::vector<std::string>& parts, size_t from, size_t to) {
    std::string out;
    for (size_t i = from; i < to && i < parts.size(); ++i) out += parts[i];
    return out;
}

bool inside(int px, int py, int x, int y, int width, int height) {
    return px >= x && py >= y && px < int64_t(x) + width && py < int64_t(y) + height;
}

} // namespace

void Ui::beginFrame(Window& window) {
    previous_ = std::move(current_);
    current_.clear();
    stack_.clear();
    stackModal_.clear();
    clickTaken_ = false;

    // A modal layer seen in the last frame disables every layer drawn before it.
    blockBelow_ = modalLayer_;
    modalLayer_ = 0;
    nextLayer_ = 0;

    hot_.clear();
    int best = -1;
    for (const auto& region : previous_) {
        if (region.layer < blockBelow_) continue;
        if (!inside(window.mouseX(), window.mouseY(), region.x, region.y, region.width, region.height)) continue;
        if (region.layer >= best) {
            best = region.layer;
            hot_ = region.id;
        }
    }

    bool fresh = focusFresh_;
    focusFresh_ = false;
    if (!focus_.empty()) {
        auto owner = std::find_if(previous_.begin(), previous_.end(), [&](const Region& r) { return r.id == focus_; });
        // The field is gone, hidden behind a modal, or the click went somewhere else.
        // Inside a dialog only another field takes the keyboard away; elsewhere any click
        // outside the field does.
        auto hot = std::find_if(previous_.begin(), previous_.end(), [&](const Region& r) { return r.id == hot_; });
        bool hotIsField = hot != previous_.end() && hot->focusable;
        bool clickedAway = window.keyPressed("MOUSE_LEFT") && hot_ != focus_ && (blockBelow_ == 0 || hotIsField);
        bool missing = owner == previous_.end() && !fresh;
        if (missing || (owner != previous_.end() && owner->layer < blockBelow_) || clickedAway) {
            focus_.clear();
        } else if (owner != previous_.end() && window.keyPressed("TAB")) {
            std::vector<const Region*> ring;
            for (const auto& region : previous_)
                if (region.focusable && region.layer == owner->layer) ring.push_back(&region);
            auto at = std::find_if(ring.begin(), ring.end(), [&](const Region* r) { return r->id == focus_; });
            if (ring.size() > 1 && at != ring.end()) {
                size_t index = static_cast<size_t>(at - ring.begin());
                index = window.keyDown("SHIFT") ? (index + ring.size() - 1) % ring.size() : (index + 1) % ring.size();
                focus_ = ring[index]->id;
                field(focus_).cursor = static_cast<size_t>(-1); // caret to the end of the newly focused text
            }
        }
    }
}

void Ui::layerBegin(bool modal) {
    stack_.push_back(++nextLayer_);
    stackModal_.push_back(modal);
    if (modal) modalLayer_ = nextLayer_;
}

void Ui::layerEnd() {
    if (stack_.empty()) throw std::runtime_error("Graphics Error: ui layer ended without a matching begin");
    stack_.pop_back();
    stackModal_.pop_back();
}

void Ui::add(const std::string& id, int x, int y, int width, int height, bool focusable) {
    if (id.empty()) throw std::runtime_error("Graphics Error: an interface element needs a non-empty id");
    // Asking about the same element twice in a frame (clicked, then hovered) is one element.
    for (auto it = current_.rbegin(); it != current_.rend() && it - current_.rbegin() < 8; ++it)
        if (it->id == id && it->layer == layer()) return;
    if (current_.size() < 100000) current_.push_back({id, x, y, width, height, layer(), focusable});
}

// Under the mouse and not covered: the topmost element of the last frame, or, for an
// element that just appeared, any element whose layer is not disabled by a modal.
bool Ui::active(const std::string& id, int x, int y, int width, int height, Window& window) const {
    if (!inside(window.mouseX(), window.mouseY(), x, y, width, height)) return false;
    if (layer() < blockBelow_ || layer() < modalLayer_) return false;
    return hot_ == id;
}

bool Ui::hover(const std::string& id, int x, int y, int width, int height, Window& window) {
    add(id, x, y, width, height, false);
    return active(id, x, y, width, height, window);
}

bool Ui::click(const std::string& id, int x, int y, int width, int height, Window& window) {
    add(id, x, y, width, height, false);
    // One click, one element: the button that opens a dialog does not also press
    // whatever the dialog shows at the same place in the same frame.
    if (clickTaken_ || !window.keyPressed("MOUSE_LEFT") || !active(id, x, y, width, height, window)) return false;
    clickTaken_ = true;
    return true;
}

Ui::Field& Ui::field(const std::string& id) {
    for (auto& entry : fields_) if (entry.first == id) return entry.second;
    if (fields_.size() > 4096) fields_.erase(fields_.begin());
    fields_.push_back({id, {}});
    return fields_.back().second;
}

std::string Ui::text(const std::string& id, int x, int y, int width, int height, const std::string& value,
                     int scale, uint32_t color, Window& window) {
    if (scale < 1 || scale > 32) throw std::runtime_error("Graphics Error: text scale must be 1..32");
    add(id, x, y, width, height, true);
    auto chars = characters(value);
    Field& state = field(id);
    const int pad = 4 * scale, cell = 6 * scale;
    size_t visible = static_cast<size_t>(std::max(1, (width - 2 * pad) / cell));

    if (!clickTaken_ && window.keyPressed("MOUSE_LEFT") && active(id, x, y, width, height, window)) {
        clickTaken_ = true;
        focus_ = id;
        int column = (window.mouseX() - x - pad + cell / 2) / cell;
        state.cursor = std::min(chars.size(), state.scroll + static_cast<size_t>(std::max(0, column)));
    }
    state.cursor = std::min(state.cursor, chars.size());

    // Keyboard events reach only the window they belong to, so focus inside it is enough.
    bool editing = focus_ == id;
    if (editing) {
        auto insert = [&](const std::string& typed) {
            for (auto& ch : characters(typed)) {
                if (ch == "\n" || ch == "\r" || ch == "\t") continue;
                if (chars.size() >= 65536) break;
                chars.insert(chars.begin() + static_cast<std::ptrdiff_t>(state.cursor), ch);
                ++state.cursor;
            }
        };
        // Replay the frame's typing in order: "x", Backspace, "y" gives "y".
        for (const auto& step : window.edits()) {
            if (!step.text.empty()) {
                if (!step.control) insert(step.text);
                continue;
            }
            switch (step.key) {
                case 8: if (state.cursor > 0) chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(--state.cursor)); break;
                case 46: if (state.cursor < chars.size()) chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(state.cursor)); break;
                case 37: if (state.cursor > 0) --state.cursor; break;
                case 39: if (state.cursor < chars.size()) ++state.cursor; break;
                case 36: state.cursor = 0; break;
                case 35: state.cursor = chars.size(); break;
                case 'V': if (step.control) insert(window.clipboard()); break;
                case 'C': if (step.control) window.setClipboard(join(chars, 0, chars.size())); break;
                default: break;
            }
        }
    }

    // Keep the caret inside the visible part of a long text.
    if (state.cursor < state.scroll) state.scroll = state.cursor;
    if (state.cursor > state.scroll + visible) state.scroll = state.cursor - visible;
    if (state.scroll > chars.size()) state.scroll = chars.size();

    int textY = y + (height - 7 * scale) / 2;
    window.surface().text(x + pad, textY, join(chars, state.scroll, state.scroll + visible), scale, color);
    // The caret blinks twice a second while the field has the keyboard.
    auto phase = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (editing && (phase / 500) % 2 == 0) {
        int caretX = x + pad + static_cast<int>(state.cursor - state.scroll) * cell - std::max(1, scale / 2);
        window.surface().rectangle(caretX, textY - scale, std::max(1, scale), 9 * scale, color);
    }
    return join(chars, 0, chars.size());
}

} // namespace foxlang::graphics
