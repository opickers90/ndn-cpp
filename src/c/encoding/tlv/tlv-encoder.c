/**
 * Copyright (C) 2014 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 * Derived from tlv.hpp by Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 * See COPYING for copyright and distribution information.
 */

#include "../../util/ndn_memory.h"
#include "../../util/endian.h"
#include "tlv-encoder.h"

/**
 * Call ndn_DynamicUInt8Array_ensureLength to ensure that there is enough room in the output, and copy
 * array to the output.  This does not write a header.  This assumes self->enableOutput is 1 and always writes.
 * @param self pointer to the ndn_TlvEncoder struct
 * @param array the array to copy
 * @param arrayLength the length of the array
 * @return 0 for success, else an error code.
 */
static ndn_Error 
writeArrayEnabled(struct ndn_TlvEncoder *self, const uint8_t *array, size_t arrayLength)
{
  ndn_Error error;
  if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + arrayLength)))
    return error;

  ndn_memcpy(self->output->array + self->offset, array, arrayLength);
  self->offset += arrayLength;
  
  return NDN_ERROR_success;
}

ndn_Error
ndn_TlvEncoder_writeVarNumberEnabled(struct ndn_TlvEncoder *self, uint64_t varNumber)
{
  ndn_Error error;
  
  if (varNumber < 253) {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 1)))
      return error;
    self->output->array[self->offset] = (uint8_t)varNumber;
    self->offset += 1;  
  }
  else if (varNumber <= 0xffff) {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 3)))
      return error;
    self->output->array[self->offset] = 253;

    uint16_t value = htobe16((uint16_t)varNumber);
    ndn_memcpy(self->output->array + self->offset + 1, (uint8_t *)&value, 2);
    self->offset += 3;  
  }
  else if (varNumber <= 0xffffffff) {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 5)))
      return error;
    self->output->array[self->offset] = 254;

    uint32_t value = htobe32((uint32_t)varNumber);
    ndn_memcpy(self->output->array + self->offset + 1, (uint8_t *)&value, 4);
    self->offset += 5;  
  }
  else {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 9)))
      return error;
    self->output->array[self->offset] = 255;

    uint64_t value = htobe64(varNumber);
    ndn_memcpy(self->output->array + self->offset + 1, (uint8_t *)&value, 8);
    self->offset += 9;  
  }

  return NDN_ERROR_success;
}

ndn_Error
ndn_TlvEncoder_writeNonNegativeIntegerEnabled(struct ndn_TlvEncoder *self, uint64_t value)
{
  ndn_Error error;
  
  if (value < 253) {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 1)))
      return error;
    self->output->array[self->offset] = (uint8_t)value;
    self->offset += 1;  
  }
  else if (value <= 0xffff) {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 2)))
      return error;

    uint16_t value = htobe16((uint16_t)value);
    ndn_memcpy(self->output->array + self->offset, (uint8_t *)&value, 2);
    self->offset += 2;  
  }
  else if (value <= 0xffffffff) {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 4)))
      return error;

    uint32_t value = htobe32((uint32_t)value);
    ndn_memcpy(self->output->array + self->offset, (uint8_t *)&value, 4);
    self->offset += 4;  
  }
  else {
    if ((error = ndn_DynamicUInt8Array_ensureLength(self->output, self->offset + 8)))
      return error;

    uint64_t value = htobe64(value);
    ndn_memcpy(self->output->array + self->offset, (uint8_t *)&value, 8);
    self->offset += 8;  
  }

  return NDN_ERROR_success;
}

ndn_Error 
ndn_TlvEncoder_writeBlobTlvEnabled(struct ndn_TlvEncoder *self, int type, struct ndn_Blob *value)
{
  ndn_Error error;
  if ((error = ndn_TlvEncoder_writeTypeAndLength(self, type, value->length)))
    return error;
  if ((error = writeArrayEnabled(self, value->value, value->length)))
    return error;
  
  return NDN_ERROR_success;  
}

ndn_Error 
ndn_TlvEncoder_writeNestedTlv
  (struct ndn_TlvEncoder *self, int type, ndn_Error (*writeValue)(void *context, struct ndn_TlvEncoder *encoder), 
   void *context, int omitZeroLength)
{
  int originalEnableOutput = self->enableOutput;
  
  // Make a first pass to get the value length by setting enableOutput = 0.
  size_t saveOffset = self->offset;
  self->enableOutput = 0;
  ndn_Error error;
  if ((error = writeValue(context, self)))
    return error;
  size_t valueLength = self->offset - saveOffset;
  
  if (omitZeroLength && valueLength == 0) {
    // Omit the optional TLV.
    self->enableOutput = originalEnableOutput;
    return NDN_ERROR_success;
  }
  
  if (originalEnableOutput) {
    // Restore the offset and enableOutput.
    self->offset = saveOffset;
    self->enableOutput = 1;

    // Now, write the output.
    if ((error = ndn_TlvEncoder_writeTypeAndLength(self, type, valueLength)))
      return error;
    if ((error = writeValue(context, self)))
      return error;
  }
  else
    // The output was originally disabled. Just advance offset further by the type and length.
    ndn_TlvEncoder_writeTypeAndLength(self, type, valueLength);
  
  return NDN_ERROR_success;
}