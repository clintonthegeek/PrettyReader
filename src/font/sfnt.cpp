/*
 * sfnt.cpp — TrueType/OpenType font subsetting via HarfBuzz
 *
 * Extracted from Scribus fonts/sfnt.cpp and simplified.
 * Uses HarfBuzz subset API (hb-subset.h) exclusively.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sfnt.h"

#include <hb.h>
#include <hb-subset.h>

namespace sfnt {

namespace {

struct HbBlobDeleter {
    void operator()(hb_blob_t *b) const { if (b) hb_blob_destroy(b); }
};
struct HbFaceDeleter {
    void operator()(hb_face_t *f) const { if (f) hb_face_destroy(f); }
};
struct HbSubsetInputDeleter {
    void operator()(hb_subset_input_t *i) const { if (i) hb_subset_input_destroy(i); }
};

} // anonymous namespace

SubsetResult subsetFace(const QByteArray &fontData, const QList<uint> &glyphIds,
                        int faceIndex)
{
    SubsetResult result;

    std::unique_ptr<hb_blob_t, HbBlobDeleter> blob(
        hb_blob_create(fontData.data(), fontData.size(),
                       HB_MEMORY_MODE_READONLY, nullptr, nullptr));
    if (!blob)
        return result;

    std::unique_ptr<hb_face_t, HbFaceDeleter> face(
        hb_face_create(blob.get(), faceIndex));
    if (!face)
        return result;

    std::unique_ptr<hb_subset_input_t, HbSubsetInputDeleter> input(
        hb_subset_input_create_or_fail());
    if (!input)
        return result;

    hb_set_t *glyphSet = hb_subset_input_glyph_set(input.get());
    if (!glyphSet)
        return result;

    // Always include .notdef (glyph 0)
    hb_set_add(glyphSet, 0);
    for (uint gid : glyphIds)
        hb_set_add(glyphSet, gid);

    // Retain original glyph IDs so our glyph map is identity
    uint32_t flags = static_cast<uint32_t>(hb_subset_input_get_flags(input.get()));
    flags |= HB_SUBSET_FLAGS_RETAIN_GIDS;
    flags &= ~HB_SUBSET_FLAGS_NO_HINTING;
    flags |= HB_SUBSET_FLAGS_NAME_LEGACY;
    hb_subset_input_set_flags(input.get(), flags);

    std::unique_ptr<hb_face_t, HbFaceDeleter> subsetFace(
        hb_subset_or_fail(face.get(), input.get()));
    if (!subsetFace)
        return result;

    std::unique_ptr<hb_blob_t, HbBlobDeleter> subsetBlob(
        hb_face_reference_blob(subsetFace.get()));
    if (!subsetBlob)
        return result;

    unsigned int length = 0;
    const char *data = hb_blob_get_data(subsetBlob.get(), &length);
    if (!data || length == 0)
        return result;

    result.fontData = QByteArray(data, static_cast<int>(length));

    // With RETAIN_GIDS, the mapping is identity
    for (uint gid : glyphIds)
        result.glyphMap[gid] = gid;
    result.glyphMap[0] = 0;

    result.success = true;
    return result;
}

} // namespace sfnt
