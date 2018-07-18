/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ua_types.h>
#include "ua_server_internal.h"
#include "ua_config_default.h"
#include "ua_log_stdout.h"
#include "ua_types_encoding_json.h"


 int LLVMFuzzerTestJson(const uint8_t *Data, size_t Size) {
     UA_ByteString buf;
     buf.data = Data;
     buf.length = Size;
     
     UA_Variant *out = UA_Variant_new();
     UA_Variant_init(out);
     size_t offset = 0;
     
     
     UA_decodeJson(&buf, &offset, out, &UA_TYPES[UA_TYPES_VARIANT], 0, 0);
     UA_Variant_delete(out);
     
  return 0;
}

