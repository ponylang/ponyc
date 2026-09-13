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
