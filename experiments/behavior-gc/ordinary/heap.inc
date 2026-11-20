_Thread_local heap_t* ordinary_watched;
_Thread_local size_t ordinary_reused;

static size_t ordinary_list_capacity(chunk_t* c)
{
  size_t total = 0;
  for(; c != NULL; c = c->next)
    total += get_chunk_is_small_chunk(c) ? sizeof(block_t) : c->large.size;
  return total;
}

size_t ordinary_capacity(heap_t* h)
{
  size_t total = ordinary_list_capacity(h->large);
  for(size_t i = 0; i < HEAP_SIZECLASSES; i++)
    total += ordinary_list_capacity(h->small_free[i]) +
      ordinary_list_capacity(h->small_full[i]);
  for(size_t i = 0; i < HEAP_RECYCLE_SIZECLASSES; i++)
    total += ordinary_list_capacity(h->recyclable[i]);
  return total;
}
