/// additional.c
#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

#pragma pack(1)
typedef struct Vector2i {
  int x;
  int y;
} Vector2i;

int add(int a, int b) {
  return a + b;
}

int add_struct(Vector2i v) {
  return v.x + v.y;
}

Vector2i divmod(int arg, int divisor)
{
  return (Vector2i){ arg / divisor, arg % divisor };
}
