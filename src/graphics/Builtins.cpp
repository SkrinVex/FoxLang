#include "Builtins.h"
#include "Graphics.h"
#include "foxlang/Runtime.h"
#include "foxlang/Platform.h"
#include "../core/builtins/Builtin.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace foxlang::graphics {
namespace {
using runtime::Call;

[[noreturn]] void fail(const std::string& message) { throw std::runtime_error("Graphics Error: " + message); }

int number(const Value& value) {
    if (value.isInt() && value.asInt() >= std::numeric_limits<int>::min() && value.asInt() <= std::numeric_limits<int>::max())
        return static_cast<int>(value.asInt());
    double result = 0;
    if (!runtime::tryNumber(value, result)) fail("expected a number, got '" + value.typeName() + "'");
    if (!std::isfinite(result) || result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
        fail("numeric argument out of range");
    return static_cast<int>(result);
}
int integer(const Call& c, size_t i) { return number(c.args[i]); }
uint32_t checkedColor(int value) {
    if (value < 0 || value > 0xffffff) fail("color must be 0..16777215");
    return static_cast<uint32_t>(value);
}
uint32_t color(const Call& c, size_t i) { return checkedColor(integer(c, i)); }
int level(const Call& c, size_t i, const char* what) {
    int value = integer(c, i);
    if (value < 0 || value > 255) fail(std::string(what) + " must be 0..255");
    return value;
}
const std::string& id(const Call& c, size_t i) {
    if (c.args[i].str().empty()) fail("an interface element needs a non-empty id");
    return c.args[i].str();
}

Window& window(Call& c) {
    auto& slot = c.ctx.getRoot()->graphics;
    if (!slot) fail("call open_window first");
    return *slot;
}
Surface& surface(Call& c) { return window(c).surface(); }

// Pictures live apart from the window: they can be loaded before it opens.
std::vector<std::shared_ptr<Image>>& images() {
    static std::vector<std::shared_ptr<Image>> loaded;
    return loaded;
}
Image& picture(const Call& c, size_t i) {
    int handle = integer(c, i);
    auto& loaded = images();
    if (handle < 1 || size_t(handle) > loaded.size() || !loaded[size_t(handle) - 1])
        fail(std::to_string(handle) + " is not a loaded image");
    return *loaded[size_t(handle) - 1];
}
Value loadImage(const std::string& path) {
    std::ifstream file(platform::pathFromUtf8(path), std::ios::binary);
    if (!file) fail("cannot open image '" + path + "'");
    std::stringstream bytes;
    bytes << file.rdbuf();
    auto image = std::make_shared<Image>(decodeImage(bytes.str()));
    auto& loaded = images();
    for (size_t slot = 0; slot < loaded.size(); ++slot)
        if (!loaded[slot]) {
            loaded[slot] = image;
            return Value::integer(static_cast<long long>(slot + 1));
        }
    loaded.push_back(image);
    return Value::integer(static_cast<long long>(loaded.size()));
}
uint32_t pixel(const Call& c) {
    Image& image = picture(c, 0);
    int x = integer(c, 1), y = integer(c, 2);
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) fail("pixel is outside the image");
    return image.pixels[size_t(y) * image.width + x];
}

// The numbers of a batch, checked to come in whole groups.
const std::vector<Value>& groups(Call& c, size_t i, size_t size, const char* what) {
    const auto& items = c.array(i);
    if (items.size() % size != 0)
        fail(std::string(what) + " must hold groups of " + std::to_string(size) + " numbers, got " + std::to_string(items.size()));
    return items;
}

// An atlas cut into equal frames, numbered left to right and top to bottom from 0.
struct Atlas {
    Image& image;
    int frameWidth, frameHeight, columns, count;
    Atlas(Image& picture, int width, int height) : image(picture), frameWidth(width), frameHeight(height) {
        if (width <= 0 || height <= 0) fail("frame size must be positive");
        columns = picture.width / width;
        count = columns * (picture.height / height);
        if (count == 0) fail("the image is smaller than one frame");
    }
    void draw(Surface& surface, int frame, int x, int y) const {
        if (frame >= count) fail("frame " + std::to_string(frame) + " is outside the atlas of " + std::to_string(count));
        surface.image(image, (frame % columns) * frameWidth, (frame / columns) * frameHeight, frameWidth, frameHeight,
                      x, y, frameWidth, frameHeight, 255);
    }
};

void drawRects(Call& c) {
    Surface& s = surface(c);
    const auto& items = groups(c, 0, 5, "rects");
    for (size_t i = 0; i < items.size(); i += 5)
        s.rectangle(number(items[i]), number(items[i + 1]), number(items[i + 2]), number(items[i + 3]), checkedColor(number(items[i + 4])));
}

void drawSprites(Call& c) {
    Surface& s = surface(c);
    Atlas atlas(picture(c, 0), integer(c, 1), integer(c, 2));
    const auto& items = groups(c, 3, 3, "sprites");
    for (size_t i = 0; i < items.size(); i += 3) {
        int frame = number(items[i]);
        if (frame < 0) fail("frame numbers start at 0, got " + std::to_string(frame));
        atlas.draw(s, frame, number(items[i + 1]), number(items[i + 2]));
    }
}

void drawTiles(Call& c) {
    Surface& s = surface(c);
    Atlas atlas(picture(c, 0), integer(c, 1), integer(c, 2));
    const auto& map = c.array(3);
    int columns = integer(c, 4);
    if (columns <= 0) fail("a tile map needs at least one column");
    int64_t x = integer(c, 5), y = integer(c, 6);
    size_t rows = (map.size() + size_t(columns) - 1) / size_t(columns);
    // Only the tiles that reach into the drawing area are looked at.
    Surface::Clip area = s.clip();
    auto first = [](int64_t edge, int64_t origin, int64_t step) { return std::max<int64_t>(0, (edge - origin) / step); };
    int64_t rowFrom = first(area.top, y, atlas.frameHeight), colFrom = first(area.left, x, atlas.frameWidth);
    for (int64_t row = rowFrom; row < int64_t(rows); ++row) {
        int64_t top = y + row * atlas.frameHeight;
        if (top >= area.bottom) break;
        for (int64_t col = colFrom; col < columns; ++col) {
            size_t at = size_t(row) * size_t(columns) + size_t(col);
            if (at >= map.size()) break;
            int64_t left = x + col * atlas.frameWidth;
            if (left >= area.right) break;
            int tile = number(map[at]);
            if (tile >= 0) atlas.draw(s, tile, static_cast<int>(left), static_cast<int>(top));
        }
    }
}
} // namespace

const std::vector<Signature>& signatures() {
    static const std::vector<Signature> result = {
        {"gfx_open", "open_window", "void", {{"int","width"},{"int","height"},{"string","title"}}, "Открыть нативное графическое окно. Одно окно на интерпретатор, 64..4096 пикселей по каждой оси, не более 8388608 пикселей.",
         [](Call& c) { auto& slot = c.ctx.getRoot()->graphics; if (slot) fail("close the existing window before opening another"); slot = std::make_shared<Window>(integer(c, 0), integer(c, 1), c.args[2].str()); return Value(); }},
        {"gfx_poll", "window_poll", "bool", {}, "Обработать события кадра. Возвращает false после закрытия окна. Вызывать один раз в начале кадра.",
         [](Call& c) { return Value::boolean(window(c).poll()); }},
        {"gfx_close", "close_window", "void", {}, "Закрыть окно и освободить ресурсы. Повторный вызов допустим.",
         [](Call& c) { c.ctx.getRoot()->graphics.reset(); return Value(); }},
        {"gfx_clear", "clear_window", "void", {{"int","color"}}, "Очистить буфер цветом rgb(r,g,b).",
         [](Call& c) { Surface& s = surface(c); s.clear(color(c, 0)); return Value(); }},
        {"gfx_rect", "draw_rect", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","color"}}, "Нарисовать заполненный прямоугольник с отсечением по границам окна.",
         [](Call& c) { Surface& s = surface(c); s.rectangle(integer(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), color(c, 4)); return Value(); }},
        {"gfx_circle", "draw_circle", "void", {{"int","x"},{"int","y"},{"int","radius"},{"int","color"}}, "Нарисовать заполненный круг. Радиус 0..8192.",
         [](Call& c) { Surface& s = surface(c); s.circle(integer(c, 0), integer(c, 1), integer(c, 2), color(c, 3)); return Value(); }},
        {"gfx_text", "draw_text", "void", {{"int","x"},{"int","y"},{"string","text"},{"int","scale"},{"int","color"}}, "Встроенный пиксельный шрифт: латиница, русский алфавит, цифры. Строчные и прописные буквы. Масштаб 1..32.",
         [](Call& c) { Surface& s = surface(c); s.text(integer(c, 0), integer(c, 1), c.args[2].str(), integer(c, 3), color(c, 4)); return Value(); }},
        {"gfx_present", "present_window", "void", {}, "Показать буфер кадра в нативном окне.",
         [](Call& c) { window(c).present(); return Value(); }},
        {"gfx_delta", "frame_delta", "float", {}, "Время между window_poll в секундах, ограниченное 0.1 секунды.",
         [](Call& c) { return runtime::realResult(window(c).delta()); }},
        {"gfx_down", "key_down", "bool", {{"string","key"}}, "Проверить удержание клавиши: A..Z, 0..9, LEFT/RIGHT/UP/DOWN, SPACE/ENTER/ESCAPE/TAB/BACKSPACE, SHIFT/CTRL/ALT, MOUSE_LEFT/MOUSE_RIGHT.",
         [](Call& c) { return Value::boolean(window(c).keyDown(c.args[0].str())); }},
        {"gfx_pressed", "key_pressed", "bool", {{"string","key"}}, "Проверить новое нажатие в текущем кадре; не сбрасывается при повторном чтении.",
         [](Call& c) { return Value::boolean(window(c).keyPressed(c.args[0].str())); }},
        {"gfx_mouse_x", "mouse_x", "int", {}, "Положение мыши по X относительно окна.",
         [](Call& c) { return Value::integer(window(c).mouseX()); }},
        {"gfx_mouse_y", "mouse_y", "int", {}, "Положение мыши по Y относительно окна.",
         [](Call& c) { return Value::integer(window(c).mouseY()); }},
        {"gfx_focused", "window_focused", "bool", {}, "Имеет ли окно фокус. Потеря фокуса сбрасывает удерживаемые клавиши.",
         [](Call& c) { return Value::boolean(window(c).focused()); }},
        {"gfx_line", "draw_line", "void", {{"int","x1"},{"int","y1"},{"int","x2"},{"int","y2"},{"int","color"}}, "Нарисовать отрезок толщиной в пиксель между двумя точками.",
         [](Call& c) { Surface& s = surface(c); s.line(integer(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), color(c, 4)); return Value(); }},
        {"gfx_frame", "draw_frame", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","thickness"},{"int","color"}}, "Нарисовать контур прямоугольника заданной толщины.",
         [](Call& c) { Surface& s = surface(c); s.frame(integer(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), color(c, 5)); return Value(); }},
        {"gfx_ring", "draw_ring", "void", {{"int","x"},{"int","y"},{"int","radius"},{"int","thickness"},{"int","color"}}, "Нарисовать окружность (кольцо) заданной толщины.",
         [](Call& c) { Surface& s = surface(c); s.ring(integer(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), color(c, 4)); return Value(); }},
        {"gfx_text_width", "text_width", "int", {{"string","text"},{"int","scale"}}, "Ширина текста в пикселях при данном масштабе — для выравнивания по центру. Окно не требуется.",
         [](Call& c) { return Value::integer(Surface::textWidth(c.args[0].str(), integer(c, 1))); }},
        {"gfx_text_input", "text_input", "string", {}, "Текст, набранный с прошлого кадра: учитывает раскладку (в том числе русскую), Shift, Caps Lock и автоповтор. Для полей ввода.",
         [](Call& c) { return Value::string(window(c).text()); }},
        {"gfx_repeat", "key_repeat", "bool", {{"string","key"}}, "Клавиша нажата в этом кадре или повторяется системой при удержании — для Backspace, стрелок и прокрутки списков.",
         [](Call& c) { return Value::boolean(window(c).keyRepeat(c.args[0].str())); }},
        {"gfx_wheel", "mouse_wheel", "int", {}, "Поворот колеса мыши с прошлого кадра в щелчках: больше нуля — от себя (вверх), меньше — на себя.",
         [](Call& c) { return Value::integer(window(c).wheel()); }},
        {"gfx_double_click", "double_clicked", "bool", {}, "Был ли в этом кадре двойной щелчок левой кнопкой (две нажатия быстрее 0.45 с на одном месте).",
         [](Call& c) { return Value::boolean(window(c).doubleClicked()); }},
        {"gfx_clipboard", "clipboard_text", "string", {}, "Текст из системного буфера обмена или пустая строка.",
         [](Call& c) { return Value::string(window(c).clipboard()); }},
        {"gfx_set_clipboard", "set_clipboard_text", "void", {{"string","text"}}, "Помещает текст в системный буфер обмена.",
         [](Call& c) { window(c).setClipboard(c.args[0].str()); return Value(); }},
        {"gfx_rect_alpha", "draw_rect_alpha", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","color"},{"int","alpha"}}, "Полупрозрачный прямоугольник: alpha 0 — не виден, 255 — непрозрачный. Для затемнения фона под диалогом.",
         [](Call& c) { Surface& s = surface(c); s.blend(integer(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), color(c, 4), level(c, 5, "alpha")); return Value(); }},
        {"gfx_width", "window_width", "int", {}, "Ширина области рисования окна в пикселях.",
         [](Call& c) { return Value::integer(window(c).surface().width()); }},
        {"gfx_height", "window_height", "int", {}, "Высота области рисования окна в пикселях.",
         [](Call& c) { return Value::integer(window(c).surface().height()); }},
        {"gfx_resizable", "set_window_resizable", "void", {{"bool","resizable"}}, "Разрешает (true) или запрещает менять размер окна мышью. Размер области рисования следует за окном: 64..4096 точек по каждой оси.",
         [](Call& c) { window(c).setResizable(c.args[0].asBool()); return Value(); }},
        {"gfx_set_size", "set_window_size", "void", {{"int","width"},{"int","height"}}, "Меняет размер окна и области рисования: 64..4096 по каждой оси, не более 8388608 пикселей.",
         [](Call& c) { Window& w = window(c); w.setSize(integer(c, 0), integer(c, 1)); return Value(); }},
        {"gfx_resized", "window_resized", "bool", {}, "Изменился ли размер окна с прошлого кадра — пора заново расставить элементы.",
         [](Call& c) { return Value::boolean(window(c).resized()); }},
        {"gfx_image_load", "load_image", "int", {{"string","path"}}, "Загружает картинку PNG или BMP и возвращает её номер для рисования. Окно не требуется. Поддерживаются все виды PNG, в том числе с прозрачностью.",
         [](Call& c) { return loadImage(c.args[0].str()); }},
        {"gfx_image_width", "image_width", "int", {{"int","image"}}, "Ширина загруженной картинки в пикселях.",
         [](Call& c) { return Value::integer(picture(c, 0).width); }},
        {"gfx_image_height", "image_height", "int", {{"int","image"}}, "Высота загруженной картинки в пикселях.",
         [](Call& c) { return Value::integer(picture(c, 0).height); }},
        {"gfx_image_draw", "draw_image_scaled", "void", {{"int","image"},{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","opacity"}}, "Рисует картинку в прямоугольнике (растягивая по ближайшему пикселю) с её прозрачностью и общей непрозрачностью 0..255.",
         [](Call& c) { Surface& s = surface(c); int opacity = level(c, 5, "opacity"); Image& image = picture(c, 0); s.image(image, 0, 0, image.width, image.height, integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), opacity); return Value(); }},
        {"gfx_image_draw_part", "draw_image_part", "void", {{"int","image"},{"int","source_x"},{"int","source_y"},{"int","source_width"},{"int","source_height"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Рисует часть картинки — кадр спрайта или плитку атласа — в прямоугольнике окна.",
         [](Call& c) { Surface& s = surface(c); s.image(picture(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), integer(c, 5), integer(c, 6), integer(c, 7), integer(c, 8), 255); return Value(); }},
        {"gfx_image_pixel", "image_pixel", "int", {{"int","image"},{"int","x"},{"int","y"}}, "Цвет пикселя картинки как rgb(r, g, b) — например, для карты столкновений.",
         [](Call& c) { return Value::integer(pixel(c) & 0xFFFFFF); }},
        {"gfx_image_alpha", "image_alpha", "int", {{"int","image"},{"int","x"},{"int","y"}}, "Непрозрачность пикселя картинки: 0 — прозрачный, 255 — сплошной.",
         [](Call& c) { return Value::integer(pixel(c) >> 24); }},
        {"gfx_image_free", "free_image", "void", {{"int","image"}}, "Освобождает память картинки; её номер больше не действует.",
         [](Call& c) { picture(c, 0); images()[size_t(integer(c, 0)) - 1].reset(); return Value(); }},
        {"gfx_clip_begin", "clip_begin", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Дальше рисование и щелчки по элементам интерфейса ограничены прямоугольником (внутри уже открытого). Сбрасывается в начале кадра.",
         [](Call& c) { Surface& s = surface(c); s.pushClip(integer(c, 0), integer(c, 1), integer(c, 2), integer(c, 3)); return Value(); }},
        {"gfx_clip_end", "clip_end", "void", {}, "Возвращает прямоугольник рисования, действовавший до clip_begin.",
         [](Call& c) { surface(c).popClip(); return Value(); }},
        {"gfx_ui_layer_begin", "ui_layer_begin", "void", {{"bool","modal"}}, "Начинает слой интерфейса поверх нарисованного раньше. Модальный слой отключает все элементы ниже него: клики и ввод до них не доходят.",
         [](Call& c) { window(c).ui().layerBegin(c.args[0].asBool()); return Value(); }},
        {"gfx_ui_layer_end", "ui_layer_end", "void", {}, "Закрывает слой, начатый ui_layer_begin.",
         [](Call& c) { window(c).ui().layerEnd(); return Value(); }},
        {"gfx_ui_hover", "ui_hover", "bool", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Регистрирует элемент интерфейса и возвращает true, если мышь над ним и его не закрывает элемент выше или модальный слой.",
         [](Call& c) { Window& w = window(c); return Value::boolean(w.ui().hover(id(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), w)); }},
        {"gfx_ui_click", "ui_click", "bool", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Как ui_hover, но true только в кадре щелчка. Один щелчок достаётся одному элементу — верхнему.",
         [](Call& c) { Window& w = window(c); return Value::boolean(w.ui().click(id(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), w)); }},
        {"gfx_ui_text", "ui_text_content", "string", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"string","text"},{"int","scale"},{"int","color"}}, "Содержимое однострочного поля ввода: щелчок даёт полю фокус и ставит курсор, набор текста, Backspace/Delete, стрелки, Home/End, Ctrl+V и Ctrl+C. Рисует текст и мигающий курсор, возвращает новый текст. Рамку рисует вызывающий код (см. ui_text_field в using ui;).",
         [](Call& c) { Window& w = window(c); return Value::string(w.ui().text(id(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), c.args[5].str(), integer(c, 6), color(c, 7), w)); }},
        {"gfx_ui_focused", "ui_focused", "bool", {{"string","id"}}, "Есть ли у элемента фокус клавиатуры.",
         [](Call& c) { return Value::boolean(window(c).ui().focused(c.args[0].str())); }},
        {"gfx_ui_focus", "ui_focus", "void", {{"string","id"}}, "Передаёт фокус клавиатуры элементу; пустая строка снимает фокус.",
         [](Call& c) { window(c).ui().setFocus(c.args[0].str()); return Value(); }},
        {"gfx_ui_typing", "ui_typing", "bool", {}, "Идёт ли ввод в поле: пока true, горячие клавиши-буквы приложения обрабатывать не стоит.",
         [](Call& c) { return Value::boolean(window(c).ui().typing()); }},
        {"gfx_ui_drag", "ui_drag", "bool", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Элемент, который тянут мышью: true от нажатия на нём до отпускания кнопки, даже если мышь ушла за его край. Пока его тянут, другие элементы не подсвечиваются.",
         [](Call& c) { Window& w = window(c); return Value::boolean(w.ui().drag(id(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), w)); }},
        {"gfx_ui_drag_x", "ui_drag_x", "int", {}, "На каком расстоянии от левого края перетаскиваемого элемента его схватили.",
         [](Call& c) { return Value::integer(window(c).ui().dragX()); }},
        {"gfx_ui_drag_y", "ui_drag_y", "int", {}, "На каком расстоянии от верхнего края перетаскиваемого элемента его схватили.",
         [](Call& c) { return Value::integer(window(c).ui().dragY()); }},
        {"gfx_ui_wheel", "ui_wheel", "int", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Поворот колеса для прокручиваемой области: достаётся самой внутренней области под мышью, не закрытой модальным окном или слоем выше; остальным 0.",
         [](Call& c) { Window& w = window(c); return Value::integer(w.ui().wheel(id(c, 0), integer(c, 1), integer(c, 2), integer(c, 3), integer(c, 4), w)); }},
        {"gfx_rects", "draw_rects", "void", {{"array","rects"}}, "Рисует много заполненных прямоугольников одним вызовом. Массив — подряд идущие пятёрки чисел x, y, ширина, высота, цвет.",
         [](Call& c) { drawRects(c); return Value(); }},
        {"gfx_sprites", "draw_sprites", "void", {{"int","image"},{"int","frame_width"},{"int","frame_height"},{"array","sprites"}}, "Рисует много кадров из картинки-атласа одним вызовом. Атлас делится на кадры frame_width × frame_height, они нумеруются слева направо и сверху вниз с 0. Массив — подряд идущие тройки: номер кадра, x, y.",
         [](Call& c) { drawSprites(c); return Value(); }},
        {"gfx_tiles", "draw_tiles", "void", {{"int","image"},{"int","tile_width"},{"int","tile_height"},{"array","tiles"},{"int","columns"},{"int","x"},{"int","y"}}, "Рисует карту плиток из атласа одним вызовом: tiles — номера плиток по строкам, columns плиток в строке, левый верхний угол в (x, y). Номер меньше 0 — пустая клетка. Плитки за краем окна не рисуются.",
         [](Call& c) { drawTiles(c); return Value(); }},
        {"gfx_rgb", "rgb", "int", {{"int","red"},{"int","green"},{"int","blue"}}, "Создать цвет 0xRRGGBB. Каждый канал должен быть в диапазоне 0..255.",
         [](Call& c) { int r = integer(c, 0), g = integer(c, 1), b = integer(c, 2); if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) fail("RGB channels must be 0..255"); return Value::integer((r << 16) | (g << 8) | b); }}
    };
    return result;
}
} // namespace foxlang::graphics

namespace foxlang::runtime {
void addGraphicsBuiltins(std::vector<Builtin>& out) {
    for (const auto& signature : graphics::signatures()) {
        BuiltinSpec spec{signature.builtin, signature.result, signature.params, signature.params.size(),
                         false, "graphics", signature.documentation};
        out.push_back({std::move(spec), signature.handler});
    }
}
} // namespace foxlang::runtime
