#include "image.h"

#if defined(PLATFORM_IS_WINDOWS)
#  include <windows.h>
#elif defined(PLATFORM_IS_MACOSX)
#  include <dlfcn.h>
#  include <mach-o/loader.h>
#  include <string.h>
#elif defined(__ELF__)
#  include <link.h>
#  include <unistd.h>
#endif

PONY_EXTERN_C_BEGIN

uintptr_t ponyint_image_ro_start;
uintptr_t ponyint_image_ro_end;

#if defined(PLATFORM_IS_WINDOWS)

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

// The read-only part of the image runs from its base up to the first writable
// section.
void ponyint_image_init(void)
{
  uintptr_t base = (uintptr_t)&__ImageBase;
  IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + __ImageBase.e_lfanew);
  IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
  uintptr_t end = base + nt->OptionalHeader.SizeOfImage;

  for(WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
  {
    uintptr_t start = base + sec[i].VirtualAddress;

    if(((sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) != 0) && (start < end))
      end = start;
  }

  ponyint_image_ro_start = base;
  ponyint_image_ro_end = end;
}

#elif defined(PLATFORM_IS_MACOSX)

#ifndef SG_READ_ONLY
#  define SG_READ_ONLY 0x10
#endif

// The read-only part of the image runs from its lowest mapped segment up to
// the first segment that stays writable. The loader makes __DATA_CONST
// read-only once it has fixed up the pointers in it. Every segment slides by
// the same amount.
void ponyint_image_init(void)
{
  Dl_info info;

  if((dladdr((void*)&ponyint_image_init, &info) == 0) ||
    (info.dli_fbase == NULL))
    return;

  const struct mach_header_64* mh =
    (const struct mach_header_64*)info.dli_fbase;
  const struct load_command* lc = (const struct load_command*)(mh + 1);
  uintptr_t slide = 0;
  uintptr_t lo = UINTPTR_MAX;
  uintptr_t hi = 0;
  uintptr_t ro_end = UINTPTR_MAX;

  for(uint32_t i = 0; i < mh->ncmds; i++)
  {
    if(lc->cmd == LC_SEGMENT_64)
    {
      const struct segment_command_64* seg =
        (const struct segment_command_64*)lc;

      if(strncmp(seg->segname, SEG_TEXT, sizeof(seg->segname)) == 0)
        slide = (uintptr_t)mh - (uintptr_t)seg->vmaddr;
    }

    lc = (const struct load_command*)((const char*)lc + lc->cmdsize);
  }

  lc = (const struct load_command*)(mh + 1);

  for(uint32_t i = 0; i < mh->ncmds; i++)
  {
    if(lc->cmd == LC_SEGMENT_64)
    {
      const struct segment_command_64* seg =
        (const struct segment_command_64*)lc;

      if((seg->vmsize != 0) && (seg->maxprot != VM_PROT_NONE))
      {
        uintptr_t start = (uintptr_t)seg->vmaddr + slide;
        uintptr_t end = start + (uintptr_t)seg->vmsize;
        bool read_only = ((seg->initprot & VM_PROT_WRITE) == 0) ||
          ((seg->flags & SG_READ_ONLY) != 0) ||
          (strncmp(seg->segname, "__DATA_CONST", sizeof(seg->segname)) == 0);

        if(start < lo)
          lo = start;

        if(end > hi)
          hi = end;

        if(!read_only && (start < ro_end))
          ro_end = start;
      }
    }

    lc = (const struct load_command*)((const char*)lc + lc->cmdsize);
  }

  if(lo >= hi)
    return;

  ponyint_image_ro_start = lo;
  ponyint_image_ro_end = (ro_end < hi) ? ro_end : hi;
}

#elif defined(__ELF__)

#ifndef PT_GNU_RELRO
#  define PT_GNU_RELRO 0x6474e552
#endif

typedef struct image_bounds_t
{
  uintptr_t self;
  uintptr_t page_size;
  uintptr_t start;
  uintptr_t end;
} image_bounds_t;

// The read-only part of the image runs from its lowest loaded segment up to
// the first page that stays writable. The loader makes the RELRO region
// read-only once it has relocated the pointers in it.
bool ponyint_image_elf_bounds(struct dl_phdr_info* info, uintptr_t self,
  uintptr_t page_size, uintptr_t* ro_start, uintptr_t* ro_end)
{
  uintptr_t lo = UINTPTR_MAX;
  uintptr_t hi = 0;
  uintptr_t relro_start = 0;
  uintptr_t relro_end = 0;
  bool contains = false;

  for(size_t i = 0; i < info->dlpi_phnum; i++)
  {
    uintptr_t start = (uintptr_t)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    uintptr_t end = start + (uintptr_t)info->dlpi_phdr[i].p_memsz;

    if(info->dlpi_phdr[i].p_type == PT_GNU_RELRO)
    {
      relro_start = start;
      relro_end = end;
    } else if(info->dlpi_phdr[i].p_type == PT_LOAD) {
      if((self >= start) && (self < end))
        contains = true;

      if(start < lo)
        lo = start;

      if(end > hi)
        hi = end;
    }
  }

  if(!contains)
    return false;

  uintptr_t end_ro = hi;

  // Nothing of the object is mapped between two loaded segments that are more
  // than a page apart, and other memory can be mapped there later, so the
  // read-only part ends at the first such gap.
  for(size_t i = 0; i < info->dlpi_phnum; i++)
  {
    if(info->dlpi_phdr[i].p_type != PT_LOAD)
      continue;

    uintptr_t start = (uintptr_t)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    uintptr_t end = (start + (uintptr_t)info->dlpi_phdr[i].p_memsz +
      page_size - 1) & ~(page_size - 1);
    bool followed = (end >= hi);

    for(size_t j = 0; !followed && (j < info->dlpi_phnum); j++)
    {
      if(info->dlpi_phdr[j].p_type != PT_LOAD)
        continue;

      uintptr_t next =
        (uintptr_t)(info->dlpi_addr + info->dlpi_phdr[j].p_vaddr);

      if((next >= start) && ((next & ~(page_size - 1)) <= end) && (j != i))
        followed = true;
    }

    if(!followed && (end < end_ro))
      end_ro = end;
  }

  for(size_t i = 0; i < info->dlpi_phnum; i++)
  {
    if((info->dlpi_phdr[i].p_type != PT_LOAD) ||
      ((info->dlpi_phdr[i].p_flags & PF_W) == 0))
      continue;

    uintptr_t start = (uintptr_t)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    uintptr_t end = start + (uintptr_t)info->dlpi_phdr[i].p_memsz;

    // The writable part of a segment that begins with the RELRO region
    // starts where that region ends. The bytes of the RELRO region on the
    // page holding its end stay writable, but only relocation writes them,
    // before the program runs.
    if((start >= relro_start) && (start < relro_end))
    {
      start = relro_end;
    } else {
      // Protection is per page, so the page holding a writable segment's start
      // is writable from its first byte. A loader can hand the part of that
      // page before the segment to malloc.
      start &= ~(page_size - 1);
    }

    if(start >= end)
      continue;

    if(start < end_ro)
      end_ro = start;
  }

  *ro_start = lo;
  *ro_end = end_ro;
  return true;
}

static int find_image(struct dl_phdr_info* info, size_t size, void* data)
{
  (void)size;
  image_bounds_t* b = (image_bounds_t*)data;
  return ponyint_image_elf_bounds(info, b->self, b->page_size, &b->start,
    &b->end) ? 1 : 0;
}

void ponyint_image_init(void)
{
  long page_size = sysconf(_SC_PAGESIZE);

  if(page_size <= 0)
    return;

  image_bounds_t b = {(uintptr_t)&ponyint_image_init, (uintptr_t)page_size,
    0, 0};

  if(dl_iterate_phdr(find_image, &b) == 0)
    return;

  ponyint_image_ro_start = b.start;
  ponyint_image_ro_end = b.end;
}

#else

// The bounds stay empty, so nothing counts as a constant.
void ponyint_image_init(void)
{
}

#endif

PONY_EXTERN_C_END
