#include "input_icon_cache.hpp"

#include <zlib.h>

#include <QFile>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

namespace cyan::gui {
namespace {

constexpr std::array<std::uint8_t, 8> kPngSignature{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
constexpr std::uint32_t kMaximumDimension = 4096;
constexpr std::size_t kMaximumDecodedBytes = 64U * 1024U * 1024U;

std::uint32_t read_big_endian(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < 4) {
    return 0;
  }
  return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
         (static_cast<std::uint32_t>(bytes[1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[2]) << 8U) | static_cast<std::uint32_t>(bytes[3]);
}

int paeth_predictor(int left, int above, int upper_left) {
  const int estimate = left + above - upper_left;
  const int left_distance = std::abs(estimate - left);
  const int above_distance = std::abs(estimate - above);
  const int diagonal_distance = std::abs(estimate - upper_left);
  if (left_distance <= above_distance && left_distance <= diagonal_distance) {
    return left;
  }
  return above_distance <= diagonal_distance ? above : upper_left;
}

bool undo_filter(std::span<std::uint8_t> row, std::span<const std::uint8_t> previous,
                 std::uint8_t filter, std::size_t bytes_per_pixel) {
  for (std::size_t index = 0; index < row.size(); ++index) {
    const int left = index >= bytes_per_pixel ? row[index - bytes_per_pixel] : 0;
    const int above = previous.empty() ? 0 : previous[index];
    const int upper_left =
        previous.empty() || index < bytes_per_pixel ? 0 : previous[index - bytes_per_pixel];
    switch (filter) {
      case 0:
        break;
      case 1:
        row[index] = static_cast<std::uint8_t>(row[index] + left);
        break;
      case 2:
        row[index] = static_cast<std::uint8_t>(row[index] + above);
        break;
      case 3:
        row[index] = static_cast<std::uint8_t>(row[index] + ((left + above) / 2));
        break;
      case 4:
        row[index] =
            static_cast<std::uint8_t>(row[index] + paeth_predictor(left, above, upper_left));
        break;
      default:
        return false;
    }
  }
  return true;
}

QImage decode_png(std::span<const std::uint8_t> data) {
  bool has_cgbi = false;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t bit_depth = 0;
  std::uint8_t color_type = 0;
  std::uint8_t compression_method = 0;
  std::uint8_t filter_method = 0;
  std::uint8_t interlace = 0;
  std::vector<std::uint8_t> compressed;
  std::vector<std::uint8_t> palette;
  std::vector<std::uint8_t> transparency;

  std::size_t position = kPngSignature.size();
  while (position + 12 <= data.size()) {
    const std::uint32_t chunk_size = read_big_endian(data.subspan(position, 4));
    const std::size_t payload = position + 8;
    if (chunk_size > data.size() - payload ||
        static_cast<std::size_t>(chunk_size) + 4 > data.size() - payload) {
      return {};
    }
    const auto type = data.subspan(position + 4, 4);
    const auto chunk = data.subspan(payload, chunk_size);
    if (std::memcmp(type.data(), "CgBI", 4) == 0) {
      has_cgbi = true;
    } else if (std::memcmp(type.data(), "IHDR", 4) == 0 && chunk.size() == 13) {
      width = read_big_endian(chunk.first(4));
      height = read_big_endian(chunk.subspan(4, 4));
      bit_depth = chunk[8];
      color_type = chunk[9];
      compression_method = chunk[10];
      filter_method = chunk[11];
      interlace = chunk[12];
    } else if (std::memcmp(type.data(), "PLTE", 4) == 0) {
      palette.assign(chunk.begin(), chunk.end());
    } else if (std::memcmp(type.data(), "tRNS", 4) == 0) {
      transparency.assign(chunk.begin(), chunk.end());
    } else if (std::memcmp(type.data(), "IDAT", 4) == 0) {
      if (chunk.size() > kMaximumDecodedBytes - compressed.size()) {
        return {};
      }
      compressed.insert(compressed.end(), chunk.begin(), chunk.end());
    } else if (std::memcmp(type.data(), "IEND", 4) == 0) {
      break;
    }
    position = payload + static_cast<std::size_t>(chunk_size) + 4;
  }

  if (width == 0 || height == 0 || width > kMaximumDimension || height > kMaximumDimension ||
      bit_depth != 8 || compression_method != 0 || filter_method != 0 || interlace != 0 ||
      (color_type != 0 && color_type != 2 && color_type != 3 && color_type != 4 &&
       color_type != 6) ||
      (has_cgbi && color_type != 6)) {
    return {};
  }
  const std::size_t bytes_per_pixel = color_type == 6   ? 4U
                                      : color_type == 2 ? 3U
                                      : color_type == 4 ? 2U
                                                        : 1U;
  const std::size_t row_size = static_cast<std::size_t>(width) * bytes_per_pixel;
  if (row_size > kMaximumDecodedBytes ||
      static_cast<std::size_t>(height) > kMaximumDecodedBytes / (row_size + 1U)) {
    return {};
  }
  std::vector<std::uint8_t> inflated((row_size + 1U) * height);
  z_stream stream{};
  stream.next_in = compressed.data();
  stream.avail_in = static_cast<uInt>(compressed.size());
  stream.next_out = inflated.data();
  stream.avail_out = static_cast<uInt>(inflated.size());
  if (inflateInit2(&stream, has_cgbi ? -MAX_WBITS : MAX_WBITS) != Z_OK) {
    return {};
  }
  const int status = inflate(&stream, Z_FINISH);
  inflateEnd(&stream);
  if (status != Z_STREAM_END || stream.total_out != inflated.size()) {
    return {};
  }

  std::vector<std::uint8_t> pixels(row_size * height);
  for (std::uint32_t row_index = 0; row_index < height; ++row_index) {
    const std::size_t source_offset = static_cast<std::size_t>(row_index) * (row_size + 1U);
    const std::size_t target_offset = static_cast<std::size_t>(row_index) * row_size;
    std::span<std::uint8_t> row(pixels.data() + target_offset, row_size);
    std::copy_n(inflated.data() + source_offset + 1U, row_size, row.data());
    const std::span<const std::uint8_t> previous =
        row_index == 0
            ? std::span<const std::uint8_t>{}
            : std::span<const std::uint8_t>(pixels.data() + target_offset - row_size, row_size);
    if (!undo_filter(row, previous, inflated[source_offset], bytes_per_pixel)) {
      return {};
    }
  }

  QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBA8888);
  if (image.isNull()) {
    return {};
  }
  for (std::uint32_t y = 0; y < height; ++y) {
    const auto* source = pixels.data() + static_cast<std::size_t>(y) * row_size;
    auto* target = image.scanLine(static_cast<int>(y));
    for (std::uint32_t x = 0; x < width; ++x) {
      const auto* input = source + x * bytes_per_pixel;
      auto* output = target + x * 4U;
      if (color_type == 6 && has_cgbi) {
        const std::uint8_t alpha = input[3];
        const auto unpremultiply = [alpha](std::uint8_t channel) {
          return alpha == 0
                     ? std::uint8_t{0}
                     : static_cast<std::uint8_t>(
                           (std::min)(255U,
                                      (static_cast<unsigned int>(channel) * 255U + alpha / 2U) /
                                          alpha));
        };
        output[0] = unpremultiply(input[2]);
        output[1] = unpremultiply(input[1]);
        output[2] = unpremultiply(input[0]);
        output[3] = alpha;
      } else if (color_type == 6) {
        std::copy_n(input, 4, output);
      } else if (color_type == 2) {
        std::copy_n(input, 3, output);
        output[3] = 255;
      } else if (color_type == 3) {
        const std::size_t palette_offset = static_cast<std::size_t>(input[0]) * 3U;
        if (palette_offset + 2U >= palette.size()) {
          return {};
        }
        std::copy_n(palette.data() + palette_offset, 3, output);
        output[3] = input[0] < transparency.size() ? transparency[input[0]] : 255;
      } else if (color_type == 4) {
        output[0] = input[0];
        output[1] = input[0];
        output[2] = input[0];
        output[3] = input[1];
      } else {
        output[0] = input[0];
        output[1] = input[0];
        output[2] = input[0];
        output[3] = 255;
      }
    }
  }
  return image;
}

}  // namespace

QImage decode_application_icon(std::span<const std::uint8_t> data) {
  if (data.empty()) {
    return {};
  }
  const QByteArray encoded(reinterpret_cast<const char*>(data.data()),
                           static_cast<qsizetype>(data.size()));
  QImage image = QImage::fromData(encoded);
  if (!image.isNull()) {
    return image;
  }
  if (data.size() < kPngSignature.size() ||
      !std::equal(kPngSignature.begin(), kPngSignature.end(), data.begin())) {
    return {};
  }
  return decode_png(data);
}

QUrl cache_application_icon(std::span<const std::uint8_t> data, QTemporaryDir& cache) {
  if (!cache.isValid()) {
    return {};
  }
  QImage image = decode_application_icon(data);
  if (image.isNull()) {
    return {};
  }
  const QString destination =
      cache.filePath(QStringLiteral("application-icon-%1.bmp")
                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
  if (!image.save(destination, "BMP")) {
    return {};
  }
  return QUrl::fromLocalFile(destination);
}

}  // namespace cyan::gui
