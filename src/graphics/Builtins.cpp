#include "Builtins.h"
#include "Graphics.h"
#include "foxlang/Runtime.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace foxlang::graphics {
const std::vector<Signature>& signatures() {
    static const std::vector<Signature> result = {
        {"gfx_open", "open_window", "void", {{"int","width"},{"int","height"},{"string","title"}}, "Открыть нативное графическое окно. Одно окно на интерпретатор, 64..4096 пикселей по каждой оси, не более 8388608 пикселей."},
        {"gfx_poll", "window_poll", "bool", {}, "Обработать события кадра. Возвращает false после закрытия окна. Вызывать один раз в начале кадра."},
        {"gfx_close", "close_window", "void", {}, "Закрыть окно и освободить ресурсы. Повторный вызов допустим."},
        {"gfx_clear", "clear_window", "void", {{"int","color"}}, "Очистить буфер цветом rgb(r,g,b)."},
        {"gfx_rect", "draw_rect", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","color"}}, "Нарисовать заполненный прямоугольник с отсечением по границам окна."},
        {"gfx_circle", "draw_circle", "void", {{"int","x"},{"int","y"},{"int","radius"},{"int","color"}}, "Нарисовать заполненный круг. Радиус 0..8192."},
        {"gfx_text", "draw_text", "void", {{"int","x"},{"int","y"},{"string","text"},{"int","scale"},{"int","color"}}, "Встроенный пиксельный шрифт: латиница, русский алфавит, цифры. Масштаб 1..32; строчные отображаются прописными."},
        {"gfx_present", "present_window", "void", {}, "Показать буфер кадра в нативном окне."},
        {"gfx_delta", "frame_delta", "float", {}, "Время между window_poll в секундах, ограниченное 0.1 секунды."},
        {"gfx_down", "key_down", "bool", {{"string","key"}}, "Проверить удержание клавиши: A..Z, 0..9, LEFT/RIGHT/UP/DOWN, SPACE/ENTER/ESCAPE/TAB/BACKSPACE, MOUSE_LEFT/MOUSE_RIGHT."},
        {"gfx_pressed", "key_pressed", "bool", {{"string","key"}}, "Проверить новое нажатие в текущем кадре; не сбрасывается при повторном чтении."},
        {"gfx_mouse_x", "mouse_x", "int", {}, "Положение мыши по X относительно окна."},
        {"gfx_mouse_y", "mouse_y", "int", {}, "Положение мыши по Y относительно окна."},
        {"gfx_focused", "window_focused", "bool", {}, "Имеет ли окно фокус. Потеря фокуса сбрасывает удерживаемые клавиши."},
        {"gfx_rgb", "rgb", "int", {{"int","red"},{"int","green"},{"int","blue"}}, "Создать цвет 0xRRGGBB. Каждый канал должен быть в диапазоне 0..255."}
    };
    return result;
}
bool isBuiltin(const std::string& name) {
    if (name.compare(0, 4, "gfx_") != 0) return false;
    const auto& list = signatures();
    return std::any_of(list.begin(), list.end(), [&](const Signature& s) { return s.builtin == name; });
}
Value callBuiltin(const std::string& name, const std::vector<Value>& args, Context& context) {
    const auto& list = signatures();
    auto signature = std::find_if(list.begin(), list.end(), [&](const Signature& s) { return s.builtin == name; });
    if (signature == list.end() || args.size() != signature->params.size())
        throw std::runtime_error("Graphics Error: incorrect arguments for " + name);
    for (size_t i = 0; i < args.size(); ++i)
        if (args[i].type != signature->params[i].type && !(signature->params[i].type == "int" && args[i].type == "float"))
            throw std::runtime_error("Graphics Error: invalid type for " + signature->params[i].name);
    auto integer = [&](size_t i) {
        double number = std::stod(args[i].value);
        if (!std::isfinite(number) || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
            throw std::runtime_error("Graphics Error: numeric argument out of range");
        return static_cast<int>(number);
    };
    auto color = [&](size_t i) {
        int value = integer(i);
        if (value < 0 || value > 0xffffff) throw std::runtime_error("Graphics Error: color must be 0..16777215");
        return static_cast<uint32_t>(value);
    };
    auto& window = context.getRoot()->graphics;
    if (name == "gfx_rgb") {
        int r = integer(0), g = integer(1), b = integer(2);
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) throw std::runtime_error("Graphics Error: RGB channels must be 0..255");
        return {"int", std::to_string((r << 16) | (g << 8) | b)};
    }
    if (name == "gfx_close") { window.reset(); return {"void", ""}; }
    if (name == "gfx_open") {
        if (window) throw std::runtime_error("Graphics Error: close the existing window before opening another");
        window = std::make_shared<Window>(integer(0), integer(1), args[2].value);
        return {"void", ""};
    }
    if (!window) throw std::runtime_error("Graphics Error: call open_window first");
    if (name == "gfx_poll") return {"bool", window->poll() ? "true" : "false"};
    if (name == "gfx_delta") return {"float", runtime::formatNumber(window->delta())};
    if (name == "gfx_mouse_x") return {"int", std::to_string(window->mouseX())};
    if (name == "gfx_mouse_y") return {"int", std::to_string(window->mouseY())};
    if (name == "gfx_focused") return {"bool", window->focused() ? "true" : "false"};
    if (name == "gfx_down") return {"bool", window->keyDown(args[0].value) ? "true" : "false"};
    if (name == "gfx_pressed") return {"bool", window->keyPressed(args[0].value) ? "true" : "false"};
    if (name == "gfx_clear") window->surface().clear(color(0));
    else if (name == "gfx_rect") window->surface().rectangle(integer(0), integer(1), integer(2), integer(3), color(4));
    else if (name == "gfx_circle") window->surface().circle(integer(0), integer(1), integer(2), color(3));
    else if (name == "gfx_text") window->surface().text(integer(0), integer(1), args[2].value, integer(3), color(4));
    else if (name == "gfx_present") window->present();
    return {"void", ""};
}
} // namespace foxlang::graphics
