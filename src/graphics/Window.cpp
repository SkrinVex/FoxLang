#include "Graphics.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <mutex>
#else
#include <xcb/xcb.h>
#endif

namespace foxlang::graphics {
namespace {
[[noreturn]] void fail(const std::string& message) { throw std::runtime_error("Graphics Error: " + message); }
}

#ifdef _WIN32
struct Window::Native {
    Window& owner;
    HWND hwnd = nullptr;
    BITMAPINFO bitmap{};
    explicit Native(Window& window) : owner(window) {}
    ~Native() { if (hwnd) DestroyWindow(hwnd); }
    void paint(HDC dc) {
        auto& surface = owner.surface();
        StretchDIBits(dc, 0, 0, surface.width(), surface.height(), 0, 0, surface.width(), surface.height(),
                     surface.pixels().data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
    }
    static LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Native*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Native*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(handle, message, wparam, lparam);
        switch (message) {
            case WM_CLOSE: self->owner.closeEvent(); return 0;
            case WM_DESTROY: self->owner.closeEvent(); return 0;
            case WM_SETFOCUS: self->owner.focusEvent(true); return 0;
            case WM_KILLFOCUS: self->owner.focusEvent(false); return 0;
            case WM_KEYDOWN: case WM_SYSKEYDOWN: self->owner.keyEvent(static_cast<int>(wparam), true); break;
            case WM_KEYUP: case WM_SYSKEYUP: self->owner.keyEvent(static_cast<int>(wparam), false); break;
            case WM_MOUSEMOVE: self->owner.mouseEvent(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)); return 0;
            case WM_LBUTTONDOWN:
                self->owner.mouseEvent(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                SetCapture(handle); self->owner.keyEvent(1, true); return 0;
            case WM_LBUTTONUP:
                self->owner.mouseEvent(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                self->owner.keyEvent(1, false);
                if (!self->owner.keyDown("MOUSE_RIGHT")) ReleaseCapture();
                return 0;
            case WM_RBUTTONDOWN:
                self->owner.mouseEvent(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                SetCapture(handle); self->owner.keyEvent(2, true); return 0;
            case WM_RBUTTONUP:
                self->owner.mouseEvent(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
                self->owner.keyEvent(2, false);
                if (!self->owner.keyDown("MOUSE_LEFT")) ReleaseCapture();
                return 0;
            case WM_CAPTURECHANGED:
                self->owner.keyEvent(1, false); self->owner.keyEvent(2, false); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(handle, &ps);
                self->paint(dc);
                EndPaint(handle, &ps);
                return 0;
            }
        }
        return DefWindowProcW(handle, message, wparam, lparam);
    }
    void open(const std::string& title) {
        static std::once_flag registration;
        std::call_once(registration, [] {
            WNDCLASSW cls{};
            cls.lpfnWndProc = procedure;
            cls.hInstance = GetModuleHandleW(nullptr);
            cls.lpszClassName = L"FoxLangGraphicsWindow";
            cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
            if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) fail("cannot register Win32 window class");
        });
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), static_cast<int>(title.size()), nullptr, 0);
        if (length <= 0) fail("window title must be valid UTF-8");
        std::wstring wide(size_t(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), static_cast<int>(title.size()), wide.data(), length);
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = owner.surface().width();
        bitmap.bmiHeader.biHeight = -owner.surface().height();
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT rect{0, 0, owner.surface().width(), owner.surface().height()};
        AdjustWindowRect(&rect, style, FALSE);
        hwnd = CreateWindowExW(0, L"FoxLangGraphicsWindow", wide.c_str(), style,
                               CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (!hwnd) fail("cannot create Win32 window");
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    void poll() {
        MSG message;
        while (PeekMessageW(&message, hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    void present() {
        HDC dc = GetDC(hwnd);
        if (!dc) fail("cannot acquire Win32 device context");
        paint(dc);
        ReleaseDC(hwnd, dc);
    }
};
#else
struct Window::Native {
    Window& owner;
    xcb_connection_t* connection = nullptr;
    xcb_window_t window = XCB_NONE;
    xcb_pixmap_t back = XCB_NONE;
    xcb_gcontext_t gc = XCB_NONE;
    xcb_atom_t protocols = XCB_NONE, deleteWindow = XCB_NONE;
    uint8_t depth = 0, bits = 0, byteOrder = 0;
    uint32_t red = 0, green = 0, blue = 0;
    size_t stride = 0, rowsPerRequest = 0;
    std::vector<uint8_t> image;
    std::array<int, 256> keymap{};
    explicit Native(Window& w) : owner(w) {}
    ~Native() {
        if (connection) {
            if (gc) xcb_free_gc(connection, gc);
            if (back) xcb_free_pixmap(connection, back);
            if (window) xcb_destroy_window(connection, window);
            xcb_flush(connection);
            xcb_disconnect(connection);
        }
    }
    template<class T> using Reply = std::unique_ptr<T, decltype(&std::free)>;
    void check(xcb_void_cookie_t cookie) {
        Reply<xcb_generic_error_t> error(xcb_request_check(connection, cookie), &std::free);
        if (error) fail("X11 request failed (code " + std::to_string(error->error_code) + ")");
        if (xcb_connection_has_error(connection)) fail("X11 connection lost");
    }
    xcb_atom_t atom(const char* name) {
        auto cookie = xcb_intern_atom(connection, 0, static_cast<uint16_t>(std::strlen(name)), name);
        Reply<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(connection, cookie, nullptr), &std::free);
        if (!reply) fail("cannot obtain X11 window property");
        return reply->atom;
    }
    // Codes follow Win32 virtual keys. Only letters, digits and space come from their
    // ASCII keysym: the apostrophe keysym is 39, which is also the code of RIGHT.
    static int key(uint32_t symbol) {
        if (symbol >= 'a' && symbol <= 'z') return int(symbol - 'a' + 'A');
        if ((symbol >= 'A' && symbol <= 'Z') || (symbol >= '0' && symbol <= '9') || symbol == ' ') return int(symbol);
        switch (symbol) {
            case 0xff1b: return 27; case 0xff0d: return 13; case 0xff8d: return 13;
            case 0xff51: return 37; case 0xff52: return 38;
            case 0xff53: return 39; case 0xff54: return 40;
            case 0xff09: return 9; case 0xff08: return 8;
            case 0xffe1: case 0xffe2: return 16;                     // Shift
            case 0xffe3: case 0xffe4: return 17;                     // Control
            case 0xffe9: case 0xffea: case 0xff7e: return 18;        // Alt, AltGr
            default: return 0;
        }
    }
    void refreshKeys() {
        const auto* setup = xcb_get_setup(connection);
        uint8_t count = static_cast<uint8_t>(setup->max_keycode - setup->min_keycode + 1);
        auto cookie = xcb_get_keyboard_mapping(connection, setup->min_keycode, count);
        Reply<xcb_get_keyboard_mapping_reply_t> reply(xcb_get_keyboard_mapping_reply(connection, cookie, nullptr), &std::free);
        if (!reply) fail("cannot read X11 keyboard mapping");
        keymap.fill(0);
        const auto* symbols = xcb_get_keyboard_mapping_keysyms(reply.get());
        // A non-Latin primary layout keeps its Latin keysym in a later group, so WASD
        // must be looked up across all groups instead of group 0 only.
        for (unsigned code = setup->min_keycode; code <= setup->max_keycode; ++code)
            for (unsigned slot = 0; slot < reply->keysyms_per_keycode && !keymap[code]; ++slot)
                keymap[code] = key(symbols[(code - setup->min_keycode) * reply->keysyms_per_keycode + slot]);
    }
    void open(const std::string& title) {
        int screenIndex = 0;
        connection = xcb_connect(nullptr, &screenIndex);
        if (!connection || xcb_connection_has_error(connection)) fail("cannot connect to X11; run in a desktop session with DISPLAY and X11/XWayland");
        const auto* setup = xcb_get_setup(connection);
        auto screens = xcb_setup_roots_iterator(setup);
        for (int i = 0; i < screenIndex && screens.rem; ++i) xcb_screen_next(&screens);
        if (!screens.rem) fail("X11 screen not found");
        auto* screen = screens.data;
        depth = screen->root_depth;
        byteOrder = setup->image_byte_order;
        uint8_t pad = 0;
        for (auto formats = xcb_setup_pixmap_formats_iterator(setup); formats.rem; xcb_format_next(&formats))
            if (formats.data->depth == depth) { bits = formats.data->bits_per_pixel; pad = formats.data->scanline_pad; }
        for (auto depths = xcb_screen_allowed_depths_iterator(screen); depths.rem; xcb_depth_next(&depths))
            for (auto visuals = xcb_depth_visuals_iterator(depths.data); visuals.rem; xcb_visualtype_next(&visuals))
                if (visuals.data->visual_id == screen->root_visual && visuals.data->_class == XCB_VISUAL_CLASS_TRUE_COLOR) {
                    red = visuals.data->red_mask; green = visuals.data->green_mask; blue = visuals.data->blue_mask;
                }
        if ((bits != 16 && bits != 24 && bits != 32) || !pad || !red || !green || !blue) fail("X11 display must use a 16/24/32-bit TrueColor visual");
        int width = owner.surface().width(), height = owner.surface().height();
        stride = ((size_t(width) * bits + pad - 1) / pad) * (pad / 8);
        size_t requestBytes = size_t(xcb_get_maximum_request_length(connection)) * 4;
        if (requestBytes <= 64 || stride > requestBytes - 64) fail("X11 maximum image request is too small");
        rowsPerRequest = (requestBytes - 64) / stride;
        image.resize(stride * height);
        window = xcb_generate_id(connection);
        uint32_t values[] = {screen->black_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY};
        check(xcb_create_window_checked(connection, depth, window, screen->root, 0, 0,
            static_cast<uint16_t>(width), static_cast<uint16_t>(height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
            XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values));
        protocols = atom("WM_PROTOCOLS"); deleteWindow = atom("WM_DELETE_WINDOW");
        check(xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, protocols, XCB_ATOM_ATOM, 32, 1, &deleteWindow));
        check(xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, atom("_NET_WM_NAME"), atom("UTF8_STRING"), 8, static_cast<uint32_t>(title.size()), title.data()));
        check(xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, static_cast<uint32_t>(title.size()), title.data()));
        uint32_t hints[18]{};
        hints[0] = (1 << 4) | (1 << 5);
        hints[5] = hints[7] = static_cast<uint32_t>(width); hints[6] = hints[8] = static_cast<uint32_t>(height);
        check(xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32, 18, hints));
        back = xcb_generate_id(connection);
        check(xcb_create_pixmap_checked(connection, depth, back, window, static_cast<uint16_t>(width), static_cast<uint16_t>(height)));
        gc = xcb_generate_id(connection);
        uint32_t noExposure = 0;
        check(xcb_create_gc_checked(connection, gc, window, XCB_GC_GRAPHICS_EXPOSURES, &noExposure));
        refreshKeys();
        check(xcb_map_window_checked(connection, window));
        present();
    }
    void poll() {
        std::vector<Reply<xcb_generic_event_t>> events;
        while (auto* event = xcb_poll_for_event(connection)) events.emplace_back(event, &std::free);
        for (size_t i = 0; i < events.size(); ++i) {
            auto* event = events[i].get();
            unsigned type = event->response_type & 127;
            if (type == XCB_KEY_PRESS || type == XCB_KEY_RELEASE) {
                auto* e = reinterpret_cast<xcb_key_press_event_t*>(event);
                // X11 autorepeat emits release+press with the same timestamp. It is not a new press.
                if (type == XCB_KEY_RELEASE && i + 1 < events.size() && (events[i+1]->response_type & 127) == XCB_KEY_PRESS) {
                    auto* next = reinterpret_cast<xcb_key_press_event_t*>(events[i+1].get());
                    if (next->detail == e->detail && next->time == e->time) { ++i; continue; }
                }
                if (keymap[e->detail]) owner.keyEvent(keymap[e->detail], type == XCB_KEY_PRESS);
            } else if (type == XCB_BUTTON_PRESS || type == XCB_BUTTON_RELEASE) {
                auto* e = reinterpret_cast<xcb_button_press_event_t*>(event);
                if (e->detail == 1 || e->detail == 3) owner.keyEvent(e->detail == 1 ? 1 : 2, type == XCB_BUTTON_PRESS);
                owner.mouseEvent(e->event_x, e->event_y);
            } else if (type == XCB_MOTION_NOTIFY) {
                auto* e = reinterpret_cast<xcb_motion_notify_event_t*>(event); owner.mouseEvent(e->event_x, e->event_y);
            } else if (type == XCB_FOCUS_IN) owner.focusEvent(true);
            else if (type == XCB_FOCUS_OUT) owner.focusEvent(false);
            else if (type == XCB_MAPPING_NOTIFY) { refreshKeys(); owner.focusEvent(owner.focused()); }
            else if (type == XCB_CLIENT_MESSAGE) {
                auto* e = reinterpret_cast<xcb_client_message_event_t*>(event);
                if (e->type == protocols && e->format == 32 && e->data.data32[0] == deleteWindow) owner.closeEvent();
            } else if (type == XCB_DESTROY_NOTIFY) { window = XCB_NONE; owner.closeEvent(); }
            else if (type == XCB_EXPOSE) copy();
            else if (type == 0) fail("X11 reported an asynchronous protocol error");
        }
        if (xcb_connection_has_error(connection)) fail("X11 connection lost");
    }
    static uint32_t channel(uint32_t value, uint32_t mask) {
        unsigned shift = 0;
        while (!(mask & 1)) { mask >>= 1; ++shift; }
        return static_cast<uint32_t>((uint64_t(value) * mask + 127) / 255) << shift;
    }
    void copy() {
        if (!window) return;
        xcb_copy_area(connection, back, window, gc, 0, 0, 0, 0, static_cast<uint16_t>(owner.surface().width()), static_cast<uint16_t>(owner.surface().height()));
        xcb_flush(connection);
    }
    void present() {
        const auto& surface = owner.surface();
        if (bits == 32 && byteOrder == XCB_IMAGE_ORDER_LSB_FIRST && red == 0xff0000 && green == 0xff00 && blue == 0xff && stride == size_t(surface.width()) * 4) {
            std::memcpy(image.data(), surface.pixels().data(), image.size());
        } else {
            for (int y = 0; y < surface.height(); ++y) for (int x = 0; x < surface.width(); ++x) {
                uint32_t c = surface.pixels()[size_t(y) * surface.width() + x];
                uint32_t pixel = channel((c >> 16) & 255, red) | channel((c >> 8) & 255, green) | channel(c & 255, blue);
                for (unsigned b = 0; b < bits / 8; ++b) image[size_t(y) * stride + size_t(x) * (bits / 8) + b] = static_cast<uint8_t>(pixel >> (8 * (byteOrder == XCB_IMAGE_ORDER_LSB_FIRST ? b : bits / 8 - 1 - b)));
            }
        }
        for (size_t y = 0; y < size_t(surface.height()); y += rowsPerRequest) {
            auto rows = std::min(rowsPerRequest, size_t(surface.height()) - y);
            xcb_put_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, back, gc, static_cast<uint16_t>(surface.width()), static_cast<uint16_t>(rows), 0, static_cast<int16_t>(y), 0, depth, static_cast<uint32_t>(rows * stride), image.data() + y * stride);
        }
        copy();
        if (xcb_connection_has_error(connection)) fail("X11 connection lost during presentation");
    }
};
#endif

Window::Window(int width, int height, const std::string& title) : surface_(width, height), native_(std::make_unique<Native>(*this)) {
    if (width < 64 || height < 64) fail("window dimensions must be at least 64 pixels");
    if (title.empty() || title.size() > 4096 || title.find('\0') != std::string::npos) fail("window title must contain 1..4096 bytes without NUL");
    native_->open(title);
}
Window::~Window() = default;
bool Window::poll() {
    pressed_.fill(false);
    if (!open_) return false;
    auto now = std::chrono::steady_clock::now();
    delta_ = std::clamp(std::chrono::duration<double>(now - lastPoll_).count(), 0.0, 0.1);
    lastPoll_ = now;
    native_->poll();
    return open_;
}
void Window::present() { if (open_) native_->present(); }
void Window::keyEvent(int key, bool down) {
    if (key <= 0 || key >= 256) return;
    if (down && !down_[key]) pressed_[key] = true;
    down_[key] = down;
}
void Window::focusEvent(bool focused) { focused_ = focused; down_.fill(false); pressed_.fill(false); }
int Window::keyCode(const std::string& key) {
    if (key.size() == 1) {
        unsigned char c = static_cast<unsigned char>(key[0]);
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') return c;
    }
    if (key == "LEFT") return 37;
    if (key == "UP") return 38;
    if (key == "RIGHT") return 39;
    if (key == "DOWN") return 40;
    if (key == "SPACE") return 32;
    if (key == "ENTER") return 13;
    if (key == "ESCAPE") return 27;
    if (key == "TAB") return 9;
    if (key == "BACKSPACE") return 8;
    if (key == "SHIFT") return 16;
    if (key == "CTRL") return 17;
    if (key == "ALT") return 18;
    if (key == "MOUSE_LEFT") return 1;
    if (key == "MOUSE_RIGHT") return 2;
    fail("unknown key '" + key + "'");
}
bool Window::keyDown(const std::string& key) const { return down_[keyCode(key)]; }
bool Window::keyPressed(const std::string& key) const { return pressed_[keyCode(key)]; }
} // namespace foxlang::graphics
