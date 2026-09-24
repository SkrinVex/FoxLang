#include "Builtins.h"
#include "Graphics.h"
#include "foxlang/Runtime.h"
#include "../core/builtins/Builtin.h"
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
        {"gfx_text", "draw_text", "void", {{"int","x"},{"int","y"},{"string","text"},{"int","scale"},{"int","color"}}, "Встроенный пиксельный шрифт: латиница, русский алфавит, цифры. Строчные и прописные буквы. Масштаб 1..32."},
        {"gfx_present", "present_window", "void", {}, "Показать буфер кадра в нативном окне."},
        {"gfx_delta", "frame_delta", "float", {}, "Время между window_poll в секундах, ограниченное 0.1 секунды."},
        {"gfx_down", "key_down", "bool", {{"string","key"}}, "Проверить удержание клавиши: A..Z, 0..9, LEFT/RIGHT/UP/DOWN, SPACE/ENTER/ESCAPE/TAB/BACKSPACE, SHIFT/CTRL/ALT, MOUSE_LEFT/MOUSE_RIGHT."},
        {"gfx_pressed", "key_pressed", "bool", {{"string","key"}}, "Проверить новое нажатие в текущем кадре; не сбрасывается при повторном чтении."},
        {"gfx_mouse_x", "mouse_x", "int", {}, "Положение мыши по X относительно окна."},
        {"gfx_mouse_y", "mouse_y", "int", {}, "Положение мыши по Y относительно окна."},
        {"gfx_focused", "window_focused", "bool", {}, "Имеет ли окно фокус. Потеря фокуса сбрасывает удерживаемые клавиши."},
        {"gfx_line", "draw_line", "void", {{"int","x1"},{"int","y1"},{"int","x2"},{"int","y2"},{"int","color"}}, "Нарисовать отрезок толщиной в пиксель между двумя точками."},
        {"gfx_frame", "draw_frame", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","thickness"},{"int","color"}}, "Нарисовать контур прямоугольника заданной толщины."},
        {"gfx_ring", "draw_ring", "void", {{"int","x"},{"int","y"},{"int","radius"},{"int","thickness"},{"int","color"}}, "Нарисовать окружность (кольцо) заданной толщины."},
        {"gfx_text_width", "text_width", "int", {{"string","text"},{"int","scale"}}, "Ширина текста в пикселях при данном масштабе — для выравнивания по центру. Окно не требуется."},
        {"gfx_text_input", "text_input", "string", {}, "Текст, набранный с прошлого кадра: учитывает раскладку (в том числе русскую), Shift, Caps Lock и автоповтор. Для полей ввода."},
        {"gfx_repeat", "key_repeat", "bool", {{"string","key"}}, "Клавиша нажата в этом кадре или повторяется системой при удержании — для Backspace, стрелок и прокрутки списков."},
        {"gfx_wheel", "mouse_wheel", "int", {}, "Поворот колеса мыши с прошлого кадра в щелчках: больше нуля — от себя (вверх), меньше — на себя."},
        {"gfx_double_click", "double_clicked", "bool", {}, "Был ли в этом кадре двойной щелчок левой кнопкой (две нажатия быстрее 0.45 с на одном месте)."},
        {"gfx_clipboard", "clipboard_text", "string", {}, "Текст из системного буфера обмена или пустая строка."},
        {"gfx_set_clipboard", "set_clipboard_text", "void", {{"string","text"}}, "Помещает текст в системный буфер обмена."},
        {"gfx_rect_alpha", "draw_rect_alpha", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"int","color"},{"int","alpha"}}, "Полупрозрачный прямоугольник: alpha 0 — не виден, 255 — непрозрачный. Для затемнения фона под диалогом."},
        {"gfx_width", "window_width", "int", {}, "Ширина области рисования окна в пикселях."},
        {"gfx_height", "window_height", "int", {}, "Высота области рисования окна в пикселях."},
        {"gfx_clip_begin", "clip_begin", "void", {{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Дальше рисование и щелчки по элементам интерфейса ограничены прямоугольником (внутри уже открытого). Сбрасывается в начале кадра."},
        {"gfx_clip_end", "clip_end", "void", {}, "Возвращает прямоугольник рисования, действовавший до clip_begin."},
        {"gfx_ui_layer_begin", "ui_layer_begin", "void", {{"bool","modal"}}, "Начинает слой интерфейса поверх нарисованного раньше. Модальный слой отключает все элементы ниже него: клики и ввод до них не доходят."},
        {"gfx_ui_layer_end", "ui_layer_end", "void", {}, "Закрывает слой, начатый ui_layer_begin."},
        {"gfx_ui_hover", "ui_hover", "bool", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Регистрирует элемент интерфейса и возвращает true, если мышь над ним и его не закрывает элемент выше или модальный слой."},
        {"gfx_ui_click", "ui_click", "bool", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Как ui_hover, но true только в кадре щелчка. Один щелчок достаётся одному элементу — верхнему."},
        {"gfx_ui_text", "ui_text_content", "string", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"},{"string","text"},{"int","scale"},{"int","color"}}, "Содержимое однострочного поля ввода: щелчок даёт полю фокус и ставит курсор, набор текста, Backspace/Delete, стрелки, Home/End, Ctrl+V и Ctrl+C. Рисует текст и мигающий курсор, возвращает новый текст. Рамку рисует вызывающий код (см. ui_text_field в using ui;)."},
        {"gfx_ui_focused", "ui_focused", "bool", {{"string","id"}}, "Есть ли у элемента фокус клавиатуры."},
        {"gfx_ui_focus", "ui_focus", "void", {{"string","id"}}, "Передаёт фокус клавиатуры элементу; пустая строка снимает фокус."},
        {"gfx_ui_typing", "ui_typing", "bool", {}, "Идёт ли ввод в поле: пока true, горячие клавиши-буквы приложения обрабатывать не стоит."},
        {"gfx_ui_drag", "ui_drag", "bool", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Элемент, который тянут мышью: true от нажатия на нём до отпускания кнопки, даже если мышь ушла за его край. Пока его тянут, другие элементы не подсвечиваются."},
        {"gfx_ui_drag_x", "ui_drag_x", "int", {}, "На каком расстоянии от левого края перетаскиваемого элемента его схватили."},
        {"gfx_ui_drag_y", "ui_drag_y", "int", {}, "На каком расстоянии от верхнего края перетаскиваемого элемента его схватили."},
        {"gfx_ui_wheel", "ui_wheel", "int", {{"string","id"},{"int","x"},{"int","y"},{"int","width"},{"int","height"}}, "Поворот колеса для прокручиваемой области: достаётся самой внутренней области под мышью, не закрытой модальным окном или слоем выше; остальным 0."},
        {"gfx_rgb", "rgb", "int", {{"int","red"},{"int","green"},{"int","blue"}}, "Создать цвет 0xRRGGBB. Каждый канал должен быть в диапазоне 0..255."}
    };
    return result;
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
        return {"int", Text::integer((r << 16) | (g << 8) | b)};
    }
    if (name == "gfx_close") { window.reset(); return {"void", ""}; }
    if (name == "gfx_text_width") return {"int", Text::integer(Surface::textWidth(args[0].value, integer(1)))};
    if (name == "gfx_open") {
        if (window) throw std::runtime_error("Graphics Error: close the existing window before opening another");
        window = std::make_shared<Window>(integer(0), integer(1), args[2].value);
        return {"void", ""};
    }
    if (!window) throw std::runtime_error("Graphics Error: call open_window first");
    if (name == "gfx_poll") return {"bool", window->poll() ? "true" : "false"};
    if (name == "gfx_delta") return {"float", runtime::realResult(window->delta())};
    if (name == "gfx_mouse_x") return {"int", Text::integer(window->mouseX())};
    if (name == "gfx_mouse_y") return {"int", Text::integer(window->mouseY())};
    if (name == "gfx_focused") return {"bool", window->focused() ? "true" : "false"};
    if (name == "gfx_down") return {"bool", window->keyDown(args[0].value) ? "true" : "false"};
    if (name == "gfx_pressed") return {"bool", window->keyPressed(args[0].value) ? "true" : "false"};
    auto flag = [](bool value) { return Value{"bool", value ? "true" : "false"}; };
    auto id = [&](size_t i) -> const std::string& {
        if (args[i].value.str().empty()) throw std::runtime_error("Graphics Error: an interface element needs a non-empty id");
        return args[i].value.str();
    };
    if (name == "gfx_repeat") return flag(window->keyRepeat(args[0].value));
    if (name == "gfx_text_input") return {"string", window->text()};
    if (name == "gfx_wheel") return {"int", Text::integer(window->wheel())};
    if (name == "gfx_double_click") return flag(window->doubleClicked());
    if (name == "gfx_clipboard") return {"string", window->clipboard()};
    if (name == "gfx_set_clipboard") { window->setClipboard(args[0].value); return {"void", ""}; }
    if (name == "gfx_width") return {"int", Text::integer(window->surface().width())};
    if (name == "gfx_height") return {"int", Text::integer(window->surface().height())};
    auto& ui = window->ui();
    if (name == "gfx_ui_layer_begin") { ui.layerBegin(args[0].value == "true"); return {"void", ""}; }
    if (name == "gfx_ui_layer_end") { ui.layerEnd(); return {"void", ""}; }
    if (name == "gfx_ui_hover") return flag(ui.hover(id(0), integer(1), integer(2), integer(3), integer(4), *window));
    if (name == "gfx_ui_click") return flag(ui.click(id(0), integer(1), integer(2), integer(3), integer(4), *window));
    if (name == "gfx_ui_text")
        return {"string", ui.text(id(0), integer(1), integer(2), integer(3), integer(4), args[5].value, integer(6), color(7), *window)};
    if (name == "gfx_ui_focused") return flag(ui.focused(args[0].value));
    if (name == "gfx_ui_focus") { ui.setFocus(args[0].value); return {"void", ""}; }
    if (name == "gfx_ui_typing") return flag(ui.typing());
    if (name == "gfx_ui_drag") return flag(ui.drag(id(0), integer(1), integer(2), integer(3), integer(4), *window));
    if (name == "gfx_ui_drag_x") return {"int", Text::integer(ui.dragX())};
    if (name == "gfx_ui_drag_y") return {"int", Text::integer(ui.dragY())};
    if (name == "gfx_ui_wheel") return {"int", Text::integer(ui.wheel(id(0), integer(1), integer(2), integer(3), integer(4), *window))};
    if (name == "gfx_clip_begin") { window->surface().pushClip(integer(0), integer(1), integer(2), integer(3)); return {"void", ""}; }
    if (name == "gfx_clip_end") { window->surface().popClip(); return {"void", ""}; }
    if (name == "gfx_clear") window->surface().clear(color(0));
    else if (name == "gfx_rect") window->surface().rectangle(integer(0), integer(1), integer(2), integer(3), color(4));
    else if (name == "gfx_circle") window->surface().circle(integer(0), integer(1), integer(2), color(3));
    else if (name == "gfx_text") window->surface().text(integer(0), integer(1), args[2].value, integer(3), color(4));
    else if (name == "gfx_rect_alpha") {
        long long alpha = integer(5);
        if (alpha < 0 || alpha > 255) throw std::runtime_error("Graphics Error: alpha must be 0..255");
        window->surface().blend(integer(0), integer(1), integer(2), integer(3), color(4), static_cast<int>(alpha));
    }
    else if (name == "gfx_line") window->surface().line(integer(0), integer(1), integer(2), integer(3), color(4));
    else if (name == "gfx_frame") window->surface().frame(integer(0), integer(1), integer(2), integer(3), integer(4), color(5));
    else if (name == "gfx_ring") window->surface().ring(integer(0), integer(1), integer(2), integer(3), color(4));
    else if (name == "gfx_present") window->present();
    return {"void", ""};
}
} // namespace foxlang::graphics

namespace foxlang::runtime {
void addGraphicsBuiltins(std::vector<Builtin>& out) {
    for (const auto& signature : graphics::signatures()) {
        BuiltinSpec spec{signature.builtin, signature.result, signature.params, signature.params.size(),
                         false, "graphics", signature.documentation};
        out.push_back({std::move(spec), [](Call& c) { return graphics::callBuiltin(c.spec.name, c.args, c.ctx); }});
    }
}
} // namespace foxlang::runtime
