#pragma once

#include <QImage>
#include <QString>
#include <QUrl>
#include <cstdint>
#include <span>

class QTemporaryDir;

namespace cyan::gui {

[[nodiscard]] QImage decode_application_icon(std::span<const std::uint8_t> data);
[[nodiscard]] QUrl cache_application_icon(std::span<const std::uint8_t> data, QTemporaryDir& cache);

}  // namespace cyan::gui
