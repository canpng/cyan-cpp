#include "cyan/image/icon_processor.hpp"

#include <Windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <string>

#include "cyan/platform/utf.hpp"

namespace cyan {
namespace {

using Microsoft::WRL::ComPtr;

class ComApartment {
 public:
  ComApartment() { result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }

  ~ComApartment() {
    if (SUCCEEDED(result_)) {
      CoUninitialize();
    }
  }

  [[nodiscard]] bool usable() const noexcept {
    return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
  }

 private:
  HRESULT result_{E_FAIL};
};

Result<ComPtr<IWICImagingFactory>> create_factory() {
  ComPtr<IWICImagingFactory> factory;
  const HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(factory.GetAddressOf()));
  if (FAILED(result)) {
    return Result<ComPtr<IWICImagingFactory>>::failure(
        {ErrorCode::feature_unavailable, "Windows Imaging Component is unavailable", {}});
  }
  return Result<ComPtr<IWICImagingFactory>>::success(std::move(factory));
}

Result<void> encode_png(IWICImagingFactory* factory, IWICBitmapSource* source,
                        const std::filesystem::path& output, UINT width, UINT height) {
  ComPtr<IWICBitmapScaler> scaler;
  HRESULT result = factory->CreateBitmapScaler(scaler.GetAddressOf());
  if (SUCCEEDED(result)) {
    result = scaler->Initialize(source, width, height, WICBitmapInterpolationModeFant);
  }

  ComPtr<IWICFormatConverter> converter;
  if (SUCCEEDED(result)) {
    result = factory->CreateFormatConverter(converter.GetAddressOf());
  }
  if (SUCCEEDED(result)) {
    result =
        converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                              nullptr, 0.0, WICBitmapPaletteTypeCustom);
  }

  std::error_code error;
  std::filesystem::remove(output, error);
  ComPtr<IWICStream> stream;
  if (SUCCEEDED(result)) {
    result = factory->CreateStream(stream.GetAddressOf());
  }
  if (SUCCEEDED(result)) {
    result = stream->InitializeFromFilename(output.c_str(), GENERIC_WRITE);
  }

  ComPtr<IWICBitmapEncoder> encoder;
  if (SUCCEEDED(result)) {
    result = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
  }
  if (SUCCEEDED(result)) {
    result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
  }

  ComPtr<IWICBitmapFrameEncode> frame;
  ComPtr<IPropertyBag2> properties;
  if (SUCCEEDED(result)) {
    result = encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf());
  }
  if (SUCCEEDED(result)) {
    result = frame->Initialize(properties.Get());
  }
  if (SUCCEEDED(result)) {
    result = frame->SetSize(width, height);
  }
  WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
  if (SUCCEEDED(result)) {
    result = frame->SetPixelFormat(&pixel_format);
  }
  if (SUCCEEDED(result)) {
    result = frame->WriteSource(converter.Get(), nullptr);
  }
  if (SUCCEEDED(result)) {
    result = frame->Commit();
  }
  if (SUCCEEDED(result)) {
    result = encoder->Commit();
  }
  if (FAILED(result)) {
    std::filesystem::remove(output, error);
    return Result<void>::failure(
        {ErrorCode::filesystem_error, "could not decode, resize, or encode icon", output});
  }
  return Result<void>::success();
}

std::wstring unique_icon_prefix() {
  GUID identifier{};
  if (FAILED(CoCreateGuid(&identifier))) {
    return L"cyan_icon_a";
  }
  std::array<wchar_t, 40> text{};
  const int length = StringFromGUID2(identifier, text.data(), static_cast<int>(text.size()));
  std::wstring compact = length > 0 ? std::wstring(text.data()) : L"cyanicon";
  compact.erase(std::remove_if(compact.begin(), compact.end(),
                               [](wchar_t character) {
                                 return character == L'{' || character == L'}' || character == L'-';
                               }),
                compact.end());
  if (compact.size() > 7U) {
    compact.resize(7U);
  }
  return L"cyan_" + compact + L"a";
}

}  // namespace

Result<void> IconProcessor::replace_icon(const std::filesystem::path& source,
                                         const std::filesystem::path& app_bundle,
                                         PlistDocument& info_plist) const {
  ComApartment apartment;
  if (!apartment.usable()) {
    return Result<void>::failure(
        {ErrorCode::feature_unavailable, "could not initialize COM for icon processing", source});
  }
  auto factory_result = create_factory();
  if (!factory_result) {
    return Result<void>::failure(factory_result.error());
  }
  auto factory = factory_result.take_value();

  ComPtr<IWICBitmapDecoder> decoder;
  HRESULT decoded = factory->CreateDecoderFromFilename(
      source.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
  ComPtr<IWICBitmapFrameDecode> frame;
  if (SUCCEEDED(decoded)) {
    decoded = decoder->GetFrame(0U, frame.GetAddressOf());
  }
  if (FAILED(decoded)) {
    return Result<void>::failure(
        {ErrorCode::invalid_argument, "icon is not a supported image", source});
  }

  const std::wstring icon_name = unique_icon_prefix();
  const std::wstring phone_base = icon_name + L"60x60";
  const std::wstring tablet_base = icon_name + L"76x76";
  const auto phone_output = app_bundle / (phone_base + L"@2x.png");
  const auto tablet_output = app_bundle / (tablet_base + L"@2x~ipad.png");

  auto phone = encode_png(factory.Get(), frame.Get(), phone_output, 120U, 120U);
  if (!phone) {
    return phone;
  }
  auto tablet = encode_png(factory.Get(), frame.Get(), tablet_output, 152U, 152U);
  if (!tablet) {
    std::error_code error;
    std::filesystem::remove(phone_output, error);
    return tablet;
  }

  auto narrow_icon = platform::utf8_from_wide(icon_name);
  auto narrow_phone = platform::utf8_from_wide(phone_base);
  auto narrow_tablet = platform::utf8_from_wide(tablet_base);
  if (!narrow_icon || !narrow_phone || !narrow_tablet) {
    return Result<void>::failure(
        {ErrorCode::invalid_utf8, "could not encode generated icon names", app_bundle});
  }
  return info_plist.set_icon_configuration(narrow_icon.value(), narrow_phone.value(),
                                           narrow_tablet.value());
}

}  // namespace cyan
