typedef struct Inner
{
  int x;
} Inner;

typedef struct Outer
{
  struct Inner inner_embed;
  struct Inner* inner_var;
} Outer;

void modify_via_outer(Outer* s)
{
  s->inner_embed.x = 10;
  s->inner_var->x = 15;
}

void modify_inner(Inner* s)
{
  s->x = 5;
}
