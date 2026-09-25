"""Small OS window driver used by tests; no dependency inside packaged programs."""
import ctypes as C
import ctypes.util
import os


class NativeWindow:
    def __init__(self, title):
        self.title = title
        self.handle = None
        if os.name == 'nt':
            self.api = C.WinDLL('user32', use_last_error=True)
            self.gdi = C.WinDLL('gdi32', use_last_error=True)
            self.api.FindWindowW.argtypes = [C.c_wchar_p, C.c_wchar_p]
            self.api.FindWindowW.restype = C.c_void_p
            self.api.PostMessageW.argtypes = [C.c_void_p, C.c_uint, C.c_size_t, C.c_ssize_t]
            self.api.GetDC.argtypes = [C.c_void_p]
            self.api.GetDC.restype = C.c_void_p
            self.api.ReleaseDC.argtypes = [C.c_void_p, C.c_void_p]
            self.gdi.GetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            self.gdi.GetPixel.restype = C.c_uint32
            self.api.ClientToScreen.argtypes = [C.c_void_p, C.c_void_p]
            self.api.SetCursorPos.argtypes = [C.c_int, C.c_int]
        else:
            self.api = C.CDLL(ctypes.util.find_library('X11') or 'libX11.so.6')
            def declare(name, result, args):
                function = getattr(self.api, name)
                function.restype = result
                function.argtypes = args
            pointer, ulong, integer = C.c_void_p, C.c_ulong, C.c_int
            declare('XOpenDisplay', pointer, [C.c_char_p])
            declare('XDefaultRootWindow', ulong, [pointer])
            declare('XQueryTree', integer, [pointer, ulong, C.POINTER(ulong), C.POINTER(ulong), C.POINTER(C.POINTER(ulong)), C.POINTER(C.c_uint)])
            declare('XFetchName', integer, [pointer, ulong, C.POINTER(pointer)])
            declare('XFree', integer, [pointer])
            declare('XFlush', integer, [pointer])
            declare('XCloseDisplay', integer, [pointer])
            declare('XKeysymToKeycode', C.c_ubyte, [pointer, ulong])
            declare('XSendEvent', integer, [pointer, ulong, integer, C.c_long, pointer])
            declare('XInternAtom', ulong, [pointer, C.c_char_p, integer])
            declare('XGetImage', pointer, [pointer, ulong, integer, integer, C.c_uint, C.c_uint, ulong, integer])
            declare('XGetPixel', ulong, [pointer, integer, integer])
            declare('XDestroyImage', integer, [pointer])
            self.display = self.api.XOpenDisplay(None)
            if not self.display:
                raise RuntimeError('Cannot open test X11 display')
            self.root = self.api.XDefaultRootWindow(self.display)

    def find(self):
        if os.name == 'nt':
            self.handle = self.api.FindWindowW('FoxLangGraphicsWindow', self.title)
        else:
            pending = [self.root]
            while pending:
                handle = pending.pop()
                name = C.c_void_p()
                if self.api.XFetchName(self.display, handle, C.byref(name)) and name.value:
                    title = C.string_at(name.value).decode('utf-8', 'replace')
                    self.api.XFree(name)
                    if title == self.title:
                        self.handle = handle
                        break
                root, parent = C.c_ulong(), C.c_ulong()
                children, count = C.POINTER(C.c_ulong)(), C.c_uint()
                if self.api.XQueryTree(self.display, handle, C.byref(root), C.byref(parent), C.byref(children), C.byref(count)):
                    pending.extend(children[i] for i in range(count.value))
                    if children: self.api.XFree(children)
        return bool(self.handle)

    # Windows virtual keys and X11 keysyms for the named keys tests use.
    VIRTUAL_KEYS = {'SPACE': 32, 'LEFT': 37, 'RIGHT': 39, 'ESCAPE': 27, 'ENTER': 13, 'BACKSPACE': 8, 'TAB': 9,
                    'DELETE': 46, 'HOME': 36, 'END': 35, 'SHIFT': 16, 'CTRL': 17}
    KEYSYMS = {'SPACE': 32, 'LEFT': 0xff51, 'RIGHT': 0xff53, 'ESCAPE': 0xff1b, 'ENTER': 0xff0d, 'BACKSPACE': 0xff08,
               'TAB': 0xff09, 'DELETE': 0xffff, 'HOME': 0xff50, 'END': 0xff57, 'SHIFT': 0xffe1, 'CTRL': 0xffe3, '/': 0x2f, '.': 0x2e}

    def send_key(self, name, down, shift=False, repeat=False):
        if os.name == 'nt':
            key = self.VIRTUAL_KEYS.get(name, ord(name[0].upper()))
            # lParam as a real keyboard sends it: a repeat count of 1, bit 30 for a key that
            # was already down (auto-repeat or release) and bit 31 for a release. Without
            # them TranslateMessage treats a release as another press and types it twice.
            if down:
                flags = 1 | ((1 << 30) if repeat else 0)
            else:
                flags = 1 | (1 << 30) | (1 << 31)
            self.api.PostMessageW(self.handle, 0x100 if down else 0x101, key, flags)
        else:
            class KeyEvent(C.Structure):
                _fields_ = [('type',C.c_int),('serial',C.c_ulong),('send_event',C.c_int),('display',C.c_void_p),
                    ('window',C.c_ulong),('root',C.c_ulong),('subwindow',C.c_ulong),('time',C.c_ulong),
                    ('x',C.c_int),('y',C.c_int),('x_root',C.c_int),('y_root',C.c_int),
                    ('state',C.c_uint),('keycode',C.c_uint),('same_screen',C.c_int)]
            symbol = self.KEYSYMS.get(name, ord(name[0].lower()))
            event = KeyEvent(type=2 if down else 3, display=self.display, window=self.handle, root=self.root,
                             keycode=self.api.XKeysymToKeycode(self.display,symbol), state=1 if shift else 0, same_screen=1)
            self._send(event, 1 if down else 2)

    def type_text(self, text):
        """Types text: key presses on X11 (ASCII only), WM_CHAR on Windows (any character)."""
        for ch in text:
            if os.name == 'nt':
                self.api.PostMessageW(self.handle, 0x102, ord(ch), 0)
            else:
                shift = ch.isupper()
                self.send_key(ch, True, shift)
                self.send_key(ch, False, shift)

    def screen(self, x, y):
        """A point of the window's client area in screen coordinates (Windows)."""
        class Point(C.Structure):
            _fields_ = [('x', C.c_long), ('y', C.c_long)]
        point = Point(x, y)
        self.api.ClientToScreen(self.handle, C.byref(point))
        return point.x, point.y

    def wheel(self, x, y, steps):
        """Positive steps turn the wheel away from the user."""
        for _ in range(abs(steps)):
            if os.name == 'nt':
                # A real wheel message carries the cursor in screen coordinates, and the
                # real cursor must be there too: Windows reports its position on its own.
                sx, sy = self.screen(x, y)
                self.api.SetCursorPos(sx, sy)
                delta = 120 if steps > 0 else -120
                self.api.PostMessageW(self.handle, 0x20A, (delta & 0xffff) << 16, ((sy & 0xffff) << 16) | (sx & 0xffff))
            else:
                self.mouse(x, y, True, button=4 if steps > 0 else 5)
                self.mouse(x, y, False, button=4 if steps > 0 else 5)

    def mouse(self, x, y, down=None, button=1):
        if os.name == 'nt':
            # Keep the real cursor on the same point, or Windows moves the mouse back.
            self.api.SetCursorPos(*self.screen(x, y))
            message = 0x200 if down is None else (0x201 if down else 0x202) if button == 1 else (0x204 if down else 0x205)
            self.api.PostMessageW(self.handle, message, button if down else 0, (y << 16) | x)
        else:
            class MouseEvent(C.Structure):
                _fields_ = [('type',C.c_int),('serial',C.c_ulong),('send_event',C.c_int),('display',C.c_void_p),
                    ('window',C.c_ulong),('root',C.c_ulong),('subwindow',C.c_ulong),('time',C.c_ulong),
                    ('x',C.c_int),('y',C.c_int),('x_root',C.c_int),('y_root',C.c_int),
                    ('state',C.c_uint),('button',C.c_uint),('same_screen',C.c_int)]
            event = MouseEvent(type=6 if down is None else 4 if down else 5, display=self.display,
                window=self.handle, root=self.root, x=x, y=y,
                button={1: 1, 2: 3, 4: 4, 5: 5}[button] if down is not None else 0, same_screen=1)
            self._send(event, 64 if down is None else 4 if down else 8)

    def _send(self, event, mask=0):
        # XSendEvent reads a full XEvent union, not just the populated structure.
        buffer = C.create_string_buffer(C.sizeof(C.c_long)*24)
        C.memmove(buffer, C.byref(event), C.sizeof(event))
        if not self.api.XSendEvent(self.display,self.handle,0,mask,buffer):
            raise RuntimeError('XSendEvent failed')
        self.api.XFlush(self.display)

    def pixel(self, x, y):
        if os.name == 'nt':
            dc = self.api.GetDC(self.handle)
            try: color = self.gdi.GetPixel(dc,x,y)
            finally: self.api.ReleaseDC(self.handle,dc)
            return ((color & 255) << 16) | (color & 0xff00) | ((color >> 16) & 255)
        image = self.api.XGetImage(self.display,self.handle,x,y,1,1,C.c_ulong(-1).value,2)
        if not image: raise RuntimeError('XGetImage failed')
        try:return self.api.XGetPixel(image,0,0) & 0xffffff
        finally:self.api.XDestroyImage(image)

    def close_window(self):
        if os.name == 'nt':
            self.api.PostMessageW(self.handle,0x10,0,0)
        else:
            class Message(C.Structure):
                _fields_=[('type',C.c_int),('serial',C.c_ulong),('send_event',C.c_int),('display',C.c_void_p),
                    ('window',C.c_ulong),('message_type',C.c_ulong),('format',C.c_int),('data',C.c_long*5)]
            event=Message(type=33,display=self.display,window=self.handle,
                message_type=self.api.XInternAtom(self.display,b'WM_PROTOCOLS',0),format=32)
            event.data[0]=self.api.XInternAtom(self.display,b'WM_DELETE_WINDOW',0)
            self._send(event)

    def dispose(self):
        if os.name!='nt' and getattr(self,'display',None):
            self.api.XCloseDisplay(self.display)
            self.display=None
