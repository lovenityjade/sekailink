#include "tracker_raster_image.hpp"

#include <algorithm>
#include <cctype>
#include <dlfcn.h>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

namespace sekaiemu::spike {
namespace {

std::vector<std::uint8_t> ReadBinaryFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_image_open_failed:" + path.string());
  }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  if (size < 0) {
    throw std::runtime_error("tracker_image_read_failed:" + path.string());
  }
  input.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
  if (!data.empty()) {
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (static_cast<std::size_t>(input.gcount()) != data.size()) {
      throw std::runtime_error("tracker_image_read_failed:" + path.string());
    }
  }
  return data;
}

std::string ReadToken(std::istream& input) {
  std::string token;
  while (input >> token) {
    if (!token.empty() && token[0] == '#') {
      std::string ignored;
      std::getline(input, ignored);
      continue;
    }
    return token;
  }
  return {};
}

void ApplyTransparentKey(TrackerRasterImage& image, bool image_transparent) {
  if (!image_transparent) {
    return;
  }
  const auto pixel_count = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
  for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
    const std::size_t offset = pixel_index * 4u;
    const bool transparent_key =
        image.rgba_pixels[offset + 0] == 255 && image.rgba_pixels[offset + 1] == 0 &&
        image.rgba_pixels[offset + 2] == 255;
    if (transparent_key) {
      image.rgba_pixels[offset + 3] = 0;
    }
  }
}

TrackerRasterImage LoadPortablePixmap(const std::filesystem::path& path,
                                      bool image_transparent) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_image_open_failed:" + path.string());
  }

  const auto magic = ReadToken(input);
  if (magic != "P3" && magic != "P6") {
    throw std::runtime_error("tracker_image_format_unsupported:" + path.string());
  }

  const auto width_token = ReadToken(input);
  const auto height_token = ReadToken(input);
  const auto max_value_token = ReadToken(input);
  if (width_token.empty() || height_token.empty() || max_value_token.empty()) {
    throw std::runtime_error("tracker_image_header_invalid:" + path.string());
  }

  const auto width = static_cast<unsigned>(std::stoul(width_token));
  const auto height = static_cast<unsigned>(std::stoul(height_token));
  const auto max_value = std::stoi(max_value_token);
  if (width == 0 || height == 0 || max_value <= 0 || max_value > 255) {
    throw std::runtime_error("tracker_image_header_unsupported:" + path.string());
  }

  TrackerRasterImage image;
  image.width = width;
  image.height = height;
  image.rgba_pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);

  if (magic == "P3") {
    for (std::size_t pixel_index = 0; pixel_index < static_cast<std::size_t>(width) * height; ++pixel_index) {
      const auto red = ReadToken(input);
      const auto green = ReadToken(input);
      const auto blue = ReadToken(input);
      if (red.empty() || green.empty() || blue.empty()) {
        throw std::runtime_error("tracker_image_data_truncated:" + path.string());
      }
      const std::size_t offset = pixel_index * 4u;
      image.rgba_pixels[offset + 0] = static_cast<std::uint8_t>(std::stoi(red));
      image.rgba_pixels[offset + 1] = static_cast<std::uint8_t>(std::stoi(green));
      image.rgba_pixels[offset + 2] = static_cast<std::uint8_t>(std::stoi(blue));
      image.rgba_pixels[offset + 3] = 255;
    }
    ApplyTransparentKey(image, image_transparent);
    return image;
  }

  input >> std::ws;
  std::vector<char> rgb(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u, 0);
  input.read(rgb.data(), static_cast<std::streamsize>(rgb.size()));
  if (static_cast<std::size_t>(input.gcount()) != rgb.size()) {
    throw std::runtime_error("tracker_image_data_truncated:" + path.string());
  }
  for (std::size_t pixel_index = 0; pixel_index < static_cast<std::size_t>(width) * height; ++pixel_index) {
    const std::size_t rgb_offset = pixel_index * 3u;
    const std::size_t rgba_offset = pixel_index * 4u;
    image.rgba_pixels[rgba_offset + 0] = static_cast<std::uint8_t>(rgb[rgb_offset + 0]);
    image.rgba_pixels[rgba_offset + 1] = static_cast<std::uint8_t>(rgb[rgb_offset + 1]);
    image.rgba_pixels[rgba_offset + 2] = static_cast<std::uint8_t>(rgb[rgb_offset + 2]);
    image.rgba_pixels[rgba_offset + 3] = 255;
  }
  ApplyTransparentKey(image, image_transparent);
  return image;
}

void* OpenSharedLibrary(std::initializer_list<const char*> names) {
  for (const char* name : names) {
    if (void* handle = dlopen(name, RTLD_LAZY | RTLD_LOCAL); handle != nullptr) {
      return handle;
    }
  }
  return nullptr;
}

template <typename Function>
Function LoadSymbol(void* handle, const char* name) {
  return reinterpret_cast<Function>(dlsym(handle, name));
}

struct SdlImageApi {
  void* sdl_handle = nullptr;
  void* image_handle = nullptr;
  SDL_RWops* (*rw_from_const_mem)(const void*, int) = nullptr;
  SDL_Surface* (*convert_surface_format)(SDL_Surface*, Uint32, Uint32) = nullptr;
  void (*free_surface)(SDL_Surface*) = nullptr;
  const char* (*get_error)() = nullptr;
  int (*img_init)(int) = nullptr;
  SDL_Surface* (*img_load_typed_rw)(SDL_RWops*, int, const char*) = nullptr;

  SdlImageApi(const SdlImageApi&) = delete;
  SdlImageApi& operator=(const SdlImageApi&) = delete;
  SdlImageApi(SdlImageApi&&) = default;
  SdlImageApi& operator=(SdlImageApi&&) = default;
  SdlImageApi() = default;
};

const SdlImageApi& GetSdlImageApi() {
  static const SdlImageApi api = [] {
    SdlImageApi loaded;
    loaded.sdl_handle = OpenSharedLibrary({"libSDL2-2.0.so.0", "libSDL2.so"});
    loaded.image_handle = OpenSharedLibrary({"libSDL2_image-2.0.so.0", "libSDL2_image.so"});
    if (loaded.sdl_handle == nullptr || loaded.image_handle == nullptr) {
      return loaded;
    }
    loaded.rw_from_const_mem =
        LoadSymbol<SDL_RWops* (*)(const void*, int)>(loaded.sdl_handle, "SDL_RWFromConstMem");
    loaded.convert_surface_format =
        LoadSymbol<SDL_Surface* (*)(SDL_Surface*, Uint32, Uint32)>(loaded.sdl_handle, "SDL_ConvertSurfaceFormat");
    loaded.free_surface = LoadSymbol<void (*)(SDL_Surface*)>(loaded.sdl_handle, "SDL_FreeSurface");
    loaded.get_error = LoadSymbol<const char* (*)()>(loaded.sdl_handle, "SDL_GetError");
    loaded.img_init = LoadSymbol<int (*)(int)>(loaded.image_handle, "IMG_Init");
    loaded.img_load_typed_rw =
        LoadSymbol<SDL_Surface* (*)(SDL_RWops*, int, const char*)>(loaded.image_handle, "IMG_LoadTyped_RW");
    return loaded;
  }();
  return api;
}

std::string LowerExtension(const std::filesystem::path& path) {
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return extension;
}

const char* SdlImageTypeForExtension(std::string_view extension) {
  if (extension == ".png") {
    return "PNG";
  }
  if (extension == ".jpg" || extension == ".jpeg") {
    return "JPG";
  }
  if (extension == ".webp") {
    return "WEBP";
  }
  return nullptr;
}

int SdlImageInitFlagForType(std::string_view type) {
  if (type == "JPG") {
    return IMG_INIT_JPG;
  }
  if (type == "PNG") {
    return IMG_INIT_PNG;
  }
  if (type == "WEBP") {
    return IMG_INIT_WEBP;
  }
  return 0;
}

std::string SdlImageError(const SdlImageApi& api) {
  if (api.get_error == nullptr) {
    return {};
  }
  const char* error = api.get_error();
  return error == nullptr ? std::string{} : std::string(error);
}

TrackerRasterImage LoadSdlImage(const std::filesystem::path& path,
                                std::string_view type,
                                bool image_transparent) {
  const auto& api = GetSdlImageApi();
  if (api.sdl_handle == nullptr || api.image_handle == nullptr || api.rw_from_const_mem == nullptr ||
      api.convert_surface_format == nullptr || api.free_surface == nullptr ||
      api.img_load_typed_rw == nullptr) {
    throw std::runtime_error("tracker_image_codec_unavailable:sdl2_image:" + path.string());
  }

  const int required_flag = SdlImageInitFlagForType(type);
  if (required_flag != 0 && api.img_init != nullptr && (api.img_init(required_flag) & required_flag) == 0) {
    throw std::runtime_error("tracker_image_codec_unavailable:" + std::string(type) + ":" +
                             path.string());
  }

  const auto encoded = ReadBinaryFile(path);
  if (encoded.empty() || encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("tracker_image_data_invalid:" + path.string());
  }

  SDL_RWops* rw = api.rw_from_const_mem(encoded.data(), static_cast<int>(encoded.size()));
  if (rw == nullptr) {
    throw std::runtime_error("tracker_image_read_failed:" + path.string());
  }

  SDL_Surface* decoded = api.img_load_typed_rw(rw, 1, std::string(type).c_str());
  if (decoded == nullptr) {
    auto error = SdlImageError(api);
    throw std::runtime_error("tracker_image_decode_failed:" + path.string() +
                             (error.empty() ? std::string{} : ":" + error));
  }

  SDL_Surface* converted = api.convert_surface_format(decoded, SDL_PIXELFORMAT_RGBA32, 0);
  api.free_surface(decoded);
  if (converted == nullptr) {
    auto error = SdlImageError(api);
    throw std::runtime_error("tracker_image_convert_failed:" + path.string() +
                             (error.empty() ? std::string{} : ":" + error));
  }

  TrackerRasterImage image;
  image.width = static_cast<unsigned>(converted->w);
  image.height = static_cast<unsigned>(converted->h);
  image.rgba_pixels.resize(static_cast<std::size_t>(image.width) * image.height * 4u, 0);
  const auto* pixels = static_cast<const std::uint8_t*>(converted->pixels);
  const auto row_bytes = static_cast<std::size_t>(image.width) * 4u;
  for (unsigned y = 0; y < image.height; ++y) {
    const auto* source = pixels + (static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch));
    auto* target = image.rgba_pixels.data() + (static_cast<std::size_t>(y) * row_bytes);
    std::copy(source, source + row_bytes, target);
  }
  api.free_surface(converted);

  ApplyTransparentKey(image, image_transparent);
  return image;
}

}  // namespace

TrackerRasterImage LoadTrackerRasterAsset(const std::filesystem::path& path,
                                          bool image_transparent) {
  const auto extension = LowerExtension(path);
  if (extension == ".ppm" || extension == ".pnm") {
    return LoadPortablePixmap(path, image_transparent);
  }
  if (const char* type = SdlImageTypeForExtension(extension); type != nullptr) {
    return LoadSdlImage(path, type, image_transparent);
  }
  throw std::runtime_error("tracker_image_format_unsupported:" + path.string());
}

}  // namespace sekaiemu::spike
