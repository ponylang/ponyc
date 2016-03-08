struct Inner
{
	int x;
};

struct Outer
{
	struct Inner inner_embed;
	struct Inner* inner_var;
};

void modify_via_outer(struct Outer* s)
{
	s->inner_embed.x = 10;
	s->inner_var->x = 15;
}

void modify_inner(struct Inner* s)
{
	s->x = 5;
}
