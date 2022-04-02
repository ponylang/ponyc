#include "lsda.h"
#include <stdlib.h>

#ifdef PLATFORM_IS_VISUAL_STUDIO
#define _Unwind_GetLanguageSpecificData(X) X->HandlerData
#define _Unwind_GetRegionStart(X) X->ImageBase + X->FunctionEntry->BeginAddress
#define _Unwind_GetIP(X) X->ControlPc
#endif

typedef struct lsda_t
{
  uintptr_t region_start;
  uintptr_t ip;
  uintptr_t ip_offset;
  uintptr_t landing_pads;

  const uint8_t* type_table;
  const uint8_t* call_site_table;
  const uint8_t* action_table;

  uint8_t type_table_encoding;
  uint8_t call_site_encoding;
} lsda_t;

enum
{
  DW_EH_PE_absptr = 0x00,
  DW_EH_PE_uleb128 = 0x01,
  DW_EH_PE_udata2 = 0x02,
  DW_EH_PE_udata4 = 0x03,
  DW_EH_PE_udata8 = 0x04,
  DW_EH_PE_sleb128 = 0x09,
  DW_EH_PE_sdata2 = 0x0A,
  DW_EH_PE_sdata4 = 0x0B,
  DW_EH_PE_sdata8 = 0x0C,
  DW_EH_PE_pcrel = 0x10,
  DW_EH_PE_textrel = 0x20,
  DW_EH_PE_datarel = 0x30,
  DW_EH_PE_funcrel = 0x40,
  DW_EH_PE_aligned = 0x50,
  DW_EH_PE_indirect = 0x80,
  DW_EH_PE_omit = 0xFF
};

static intptr_t read_sleb128(const uint8_t** data)
{
  uintptr_t result = 0;
  uintptr_t shift = 0;
  unsigned char byte;
  const uint8_t* p = *data;

  do
  {
    byte = *p++;
    result |= (byte & 0x7F) << shift;
    shift += 7;
  } while(byte & 0x80);

  if((byte & 0x40) && (shift < (sizeof(result) << 3)))
    result |= (~(uintptr_t)0) << shift;

  *data = p;
  return result;
}

static uintptr_t read_uleb128(const uint8_t** data)
{
  uintptr_t result = 0;
  uintptr_t shift = 0;
  unsigned char byte;
  const uint8_t* p = *data;

  do
  {
    byte = *p++;
    result |= (byte & 0x7f) << shift;
    shift += 7;
  } while(byte & 0x80);

  *data = p;
  return result;
}

static uintptr_t read_encoded_ptr(const uint8_t** data, uint8_t encoding)
{
  const uint8_t* p = *data;

  if(encoding == DW_EH_PE_omit)
    return 0;

  // Base pointer.
  uintptr_t result;

  switch(encoding & 0x0F)
  {
    case DW_EH_PE_absptr:
      result = *((uintptr_t*)p);
      p += sizeof(uintptr_t);
      break;

    case DW_EH_PE_udata2:
      result = *((uint16_t*)p);
      p += sizeof(uint16_t);
      break;

    case DW_EH_PE_udata4:
      result = *((uint32_t*)p);
      p += sizeof(uint32_t);
      break;

    case DW_EH_PE_udata8:
      result = (uintptr_t)*((uint64_t*)p);
      p += sizeof(uint64_t);
      break;

    case DW_EH_PE_sdata2:
      result = *((int16_t*)p);
      p += sizeof(int16_t);
      break;

    case DW_EH_PE_sdata4:
      result = *((int32_t*)p);
      p += sizeof(int32_t);
      break;

    case DW_EH_PE_sdata8:
      result = (uintptr_t)*((int64_t*)p);
      p += sizeof(int64_t);
      break;

    case DW_EH_PE_sleb128:
      result = read_sleb128(&p);
      break;

    case DW_EH_PE_uleb128:
      result = read_uleb128(&p);
      break;

    default:
      abort();
      break;
  }

  *data = p;
  return result;
}

static uintptr_t read_with_encoding(const uint8_t** data, uintptr_t def)
{
  uintptr_t start = (uintptr_t)(*data);
  const uint8_t* p = *data;
  uint8_t encoding = *p++;
  *data = p;

  if(encoding == DW_EH_PE_omit)
    return def;

  uintptr_t result = read_encoded_ptr(data, encoding);

  // Relative offset.
  switch(encoding & 0x70)
  {
    case DW_EH_PE_absptr:
      break;

    case DW_EH_PE_pcrel:
      result += start;
      break;

    case DW_EH_PE_textrel:
    case DW_EH_PE_datarel:
    case DW_EH_PE_funcrel:
    case DW_EH_PE_aligned:
    default:
      abort();
      break;
  }

  // apply indirection
  if(encoding & DW_EH_PE_indirect)
    result = *((uintptr_t*)result);

  return result;
}

static bool lsda_init(lsda_t* lsda, exception_context_t* context)
{
  const uint8_t* data =
    (const uint8_t*)_Unwind_GetLanguageSpecificData(context);

  if(data == NULL)
    return false;

  lsda->region_start = _Unwind_GetRegionStart(context);
  //-1 because IP points past the faulting instruction
  lsda->ip = _Unwind_GetIP(context) - 1;
  lsda->ip_offset = lsda->ip - lsda->region_start;

  lsda->landing_pads = read_with_encoding(&data, lsda->region_start);
  lsda->type_table_encoding = *data++;

  if(lsda->type_table_encoding != DW_EH_PE_omit)
  {
    lsda->type_table = (const uint8_t*)read_uleb128(&data);
    lsda->type_table += (uintptr_t)data;
  } else {
    lsda->type_table = NULL;
  }

  lsda->call_site_encoding = *data++;

  uintptr_t length = read_uleb128(&data);
  lsda->call_site_table = data;
  lsda->action_table = data + length;

  return true;
}

bool ponyint_lsda_scan(uintptr_t* ttype_index __attribute__ ((unused)),
  exception_context_t* context,
  uintptr_t* lp)
{
  lsda_t lsda;

  if(!lsda_init(&lsda, context))
    return false;

  const uint8_t* p = lsda.call_site_table;
  //uintptr_t ip = lsda.ip;

  while(p < lsda.action_table)
  {
    uintptr_t start = read_encoded_ptr(&p, lsda.call_site_encoding);
    uintptr_t length = read_encoded_ptr(&p, lsda.call_site_encoding);
    uintptr_t landing_pad = read_encoded_ptr(&p, lsda.call_site_encoding);

    uintptr_t action_index = read_uleb128(&p);

    if((start <= lsda.ip_offset) && (lsda.ip_offset < (start + length)))
    {
      // No landing pad.
      if(landing_pad == 0)
        return false;

      *lp = lsda.landing_pads + landing_pad;
      if(action_index == 0)
        return false;

      while(true)
      {
        // Convert 1-based byte offset into
        const uint8_t* action = lsda.action_table + (action_index - 1);
        int64_t tti = read_sleb128(&action);
        if(tti > 0 || tti < 0) {
          ttype_index = (uintptr_t*)&tti;
          return true;
        }

        const uint8_t* temp = action;
        int64_t action_offset = read_sleb128(&temp);
        if(action_offset == 0)
          return false;

        action += action_offset;
      }
    }
  }

  return false;
}
