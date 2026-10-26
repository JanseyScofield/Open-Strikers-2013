#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

using stbi_uc = unsigned char;

stbi_uc* stbi_load(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels);
void stbi_image_free(void* retval_from_stbi_load);
void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
const char* stbi_failure_reason(void);

#ifdef __cplusplus
}
#endif

#ifdef STB_IMAGE_IMPLEMENTATION

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace stbi_detail {

inline int g_flip_vertically_on_load = 0;
inline const char* g_failure_reason = "";

inline std::uint32_t ReadBE32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24u) |
           (static_cast<std::uint32_t>(p[1]) << 16u) |
           (static_cast<std::uint32_t>(p[2]) << 8u)  |
           static_cast<std::uint32_t>(p[3]);
}

inline std::uint8_t PaethPredictor(int a, int b, int c) noexcept {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
    if (pb <= pc) return static_cast<std::uint8_t>(b);
    return static_cast<std::uint8_t>(c);
}

} // namespace stbi_detail

extern "C" {

void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip) {
    stbi_detail::g_flip_vertically_on_load = flag_true_if_should_flip;
}

const char* stbi_failure_reason(void) {
    return stbi_detail::g_failure_reason;
}

void stbi_image_free(void* retval_from_stbi_load) {
    std::free(retval_from_stbi_load);
}

stbi_uc* stbi_load(const char* filename, int* x, int* y, int* channels_in_file, int desired_channels) {
    if (filename == nullptr) {
        stbi_detail::g_failure_reason = "Filename is null";
        return nullptr;
    }

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        stbi_detail::g_failure_reason = "Unable to open image file";
        return nullptr;
    }

    const std::streamsize file_size = file.tellg();
    if (file_size < 33) {
        stbi_detail::g_failure_reason = "File too small to be a valid PNG";
        return nullptr;
    }

    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), file_size)) {
        stbi_detail::g_failure_reason = "Failed to read image bytes";
        return nullptr;
    }

    static constexpr std::uint8_t kPngSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (std::memcmp(bytes.data(), kPngSig, 8) != 0) {
        stbi_detail::g_failure_reason = "Invalid PNG signature";
        return nullptr;
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t bit_depth = 0;
    std::uint8_t color_type = 0;
    std::vector<std::uint8_t> idat_stream;
    std::vector<std::uint8_t> palette_rgb;
    std::vector<std::uint8_t> palette_trns;

    std::size_t pos = 8;
    while (pos + 12 <= bytes.size()) {
        const std::uint32_t chunk_len = stbi_detail::ReadBE32(&bytes[pos]);
        const char* chunk_type = reinterpret_cast<const char*>(&bytes[pos + 4]);
        const std::size_t data_off = pos + 8;

        if (data_off + chunk_len + 4 > bytes.size()) {
            stbi_detail::g_failure_reason = "Truncated PNG chunk";
            return nullptr;
        }

        if (std::memcmp(chunk_type, "IHDR", 4) == 0 && chunk_len >= 13) {
            width = stbi_detail::ReadBE32(&bytes[data_off]);
            height = stbi_detail::ReadBE32(&bytes[data_off + 4]);
            bit_depth = bytes[data_off + 8];
            color_type = bytes[data_off + 9];
        } else if (std::memcmp(chunk_type, "PLTE", 4) == 0) {
            palette_rgb.assign(&bytes[data_off], &bytes[data_off + chunk_len]);
        } else if (std::memcmp(chunk_type, "tRNS", 4) == 0) {
            palette_trns.assign(&bytes[data_off], &bytes[data_off + chunk_len]);
        } else if (std::memcmp(chunk_type, "IDAT", 4) == 0) {
            idat_stream.insert(idat_stream.end(), &bytes[data_off], &bytes[data_off + chunk_len]);
        } else if (std::memcmp(chunk_type, "IEND", 4) == 0) {
            break;
        }

        pos = data_off + chunk_len + 4;
    }

    if (width == 0 || height == 0 || bit_depth != 8 || idat_stream.empty()) {
        stbi_detail::g_failure_reason = "Unsupported PNG dimensions or bit depth";
        return nullptr;
    }

    int src_channels = 0;
    switch (color_type) {
        case 0: src_channels = 1; break; // Grayscale
        case 2: src_channels = 3; break; // RGB
        case 3: src_channels = 1; break; // Indexed Palette
        case 4: src_channels = 2; break; // Grayscale + Alpha
        case 6: src_channels = 4; break; // RGBA
        default:
            stbi_detail::g_failure_reason = "Unsupported PNG color type";
            return nullptr;
    }

    const std::size_t bpp = static_cast<std::size_t>(src_channels);
    const std::size_t stride = static_cast<std::size_t>(width) * bpp;
    const std::size_t expected_raw_size = static_cast<std::size_t>(height) * (1 + stride);

    std::vector<std::uint8_t> raw_scanlines(expected_raw_size);
    uLongf dest_len = static_cast<uLongf>(expected_raw_size);
    const int z_res = uncompress(
        raw_scanlines.data(),
        &dest_len,
        idat_stream.data(),
        static_cast<uLong>(idat_stream.size())
    );
    if (z_res != Z_OK || dest_len < expected_raw_size) {
        stbi_detail::g_failure_reason = "zlib decompression of PNG IDAT failed";
        return nullptr;
    }

    std::vector<std::uint8_t> unfiltered(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bpp);
    for (std::size_t row = 0; row < height; ++row) {
        const std::uint8_t filter_type = raw_scanlines[row * (1 + stride)];
        const std::uint8_t* src_row = &raw_scanlines[row * (1 + stride) + 1];
        std::uint8_t* dst_row = &unfiltered[row * stride];
        const std::uint8_t* prev_row = (row > 0) ? &unfiltered[(row - 1) * stride] : nullptr;

        for (std::size_t i = 0; i < stride; ++i) {
            const int a = (i >= bpp) ? dst_row[i - bpp] : 0;
            const int b = (prev_row != nullptr) ? prev_row[i] : 0;
            const int c = (prev_row != nullptr && i >= bpp) ? prev_row[i - bpp] : 0;

            switch (filter_type) {
                case 0: dst_row[i] = src_row[i]; break;
                case 1: dst_row[i] = static_cast<std::uint8_t>(src_row[i] + a); break;
                case 2: dst_row[i] = static_cast<std::uint8_t>(src_row[i] + b); break;
                case 3: dst_row[i] = static_cast<std::uint8_t>(src_row[i] + ((a + b) / 2)); break;
                case 4: dst_row[i] = static_cast<std::uint8_t>(src_row[i] + stbi_detail::PaethPredictor(a, b, c)); break;
                default:
                    stbi_detail::g_failure_reason = "Invalid PNG scanline filter type";
                    return nullptr;
            }
        }
    }

    const int file_ch = (color_type == 3) ? 4 : src_channels;
    const int out_ch = (desired_channels > 0) ? desired_channels : file_ch;

    const std::size_t total_pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    auto* out_buf = static_cast<stbi_uc*>(std::malloc(total_pixels * static_cast<std::size_t>(out_ch)));
    if (out_buf == nullptr) {
        stbi_detail::g_failure_reason = "Out of memory allocating image buffer";
        return nullptr;
    }

    for (std::size_t py = 0; py < height; ++py) {
        const std::size_t dst_y = stbi_detail::g_flip_vertically_on_load ? (height - 1 - py) : py;
        for (std::size_t px = 0; px < width; ++px) {
            const std::size_t src_idx = (py * width + px) * bpp;
            std::uint8_t r = 255, g = 255, b_val = 255, a_val = 255;

            if (color_type == 6) {
                r = unfiltered[src_idx + 0];
                g = unfiltered[src_idx + 1];
                b_val = unfiltered[src_idx + 2];
                a_val = unfiltered[src_idx + 3];
            } else if (color_type == 2) {
                r = unfiltered[src_idx + 0];
                g = unfiltered[src_idx + 1];
                b_val = unfiltered[src_idx + 2];
            } else if (color_type == 0) {
                r = g = b_val = unfiltered[src_idx];
            } else if (color_type == 4) {
                r = g = b_val = unfiltered[src_idx + 0];
                a_val = unfiltered[src_idx + 1];
            } else if (color_type == 3) {
                const std::size_t pidx = unfiltered[src_idx];
                if (pidx * 3 + 2 < palette_rgb.size()) {
                    r = palette_rgb[pidx * 3 + 0];
                    g = palette_rgb[pidx * 3 + 1];
                    b_val = palette_rgb[pidx * 3 + 2];
                }
                if (pidx < palette_trns.size()) {
                    a_val = palette_trns[pidx];
                }
            }

            const std::size_t dst_idx = (dst_y * width + px) * static_cast<std::size_t>(out_ch);
            if (out_ch == 4) {
                out_buf[dst_idx + 0] = r;
                out_buf[dst_idx + 1] = g;
                out_buf[dst_idx + 2] = b_val;
                out_buf[dst_idx + 3] = a_val;
            } else if (out_ch == 3) {
                out_buf[dst_idx + 0] = r;
                out_buf[dst_idx + 1] = g;
                out_buf[dst_idx + 2] = b_val;
            } else if (out_ch == 2) {
                out_buf[dst_idx + 0] = r;
                out_buf[dst_idx + 1] = a_val;
            } else if (out_ch == 1) {
                out_buf[dst_idx + 0] = r;
            }
        }
    }

    if (x != nullptr) *x = static_cast<int>(width);
    if (y != nullptr) *y = static_cast<int>(height);
    if (channels_in_file != nullptr) *channels_in_file = file_ch;

    return out_buf;
}

} // extern "C"

#endif // STB_IMAGE_IMPLEMENTATION
#endif // STBI_INCLUDE_STB_IMAGE_H
