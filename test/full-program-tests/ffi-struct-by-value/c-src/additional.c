#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

struct Point { double x; double y; };

EXPORT_SYMBOL double point_sum(struct Point p) {
    return p.x + p.y;
}

EXPORT_SYMBOL struct Point point_make(double x, double y) {
    struct Point r = {x, y};
    return r;
}

EXPORT_SYMBOL double point_diff_sum(struct Point a, struct Point b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return dx + dy;
}

EXPORT_SYMBOL struct Point scale_point(struct Point p, double factor) {
    struct Point r = {p.x * factor, p.y * factor};
    return r;
}

struct Color { unsigned char r; unsigned char g; unsigned char b; };

EXPORT_SYMBOL unsigned char color_max(struct Color c) {
    unsigned char m = c.r;
    if (c.g > m) m = c.g;
    if (c.b > m) m = c.b;
    return m;
}

EXPORT_SYMBOL struct Color color_make(unsigned char r, unsigned char g,
    unsigned char b) {
    struct Color c = {r, g, b};
    return c;
}

struct Rect { int x; int y; float w; float h; };

EXPORT_SYMBOL float rect_area(struct Rect r) {
    return r.w * r.h;
}

EXPORT_SYMBOL struct Rect rect_move(struct Rect r, int dx, int dy) {
    struct Rect out = {r.x + dx, r.y + dy, r.w, r.h};
    return out;
}
