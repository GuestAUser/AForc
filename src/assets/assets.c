/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/*
 * Owns release/reset operations for heap-backed asset results. Callers transfer
 * only buffers returned by this subsystem; empty binary blobs may own nothing.
 */

#include "../../include/aforc/assets.h"

#include <stdlib.h>

void aforc_asset_blob_release(AFORC_AssetBlob *blob)
{
    if (blob == NULL)
    {
        return;
    }
    free(blob->data);
    blob->data = NULL;
    blob->size = 0u;
}

void aforc_asset_text_release(AFORC_AssetText *text)
{
    if (text == NULL)
    {
        return;
    }
    free(text->data);
    text->data = NULL;
    text->size = 0u;
}
