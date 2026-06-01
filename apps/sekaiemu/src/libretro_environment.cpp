#include "libretro_environment.hpp"

#include <sstream>

namespace sekaiemu::spike {

bool HandleLibretroEnvironmentCommand(LibretroEnvironmentContext& context,
                                      unsigned cmd,
                                      void* data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      auto* format = static_cast<const retro_pixel_format*>(data);
      if (context.pixel_format) {
        *context.pixel_format = format ? *format : RETRO_PIXEL_FORMAT_XRGB8888;
      }
      return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
      auto* callback = static_cast<retro_log_callback*>(data);
      if (!callback || !context.log_callback) {
        return false;
      }
      *callback = *context.log_callback;
      return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
      auto* can_dupe = static_cast<bool*>(data);
      if (can_dupe) {
        *can_dupe = true;
      }
      return true;
    }
    case RETRO_ENVIRONMENT_GET_LANGUAGE: {
      auto* language = static_cast<unsigned*>(data);
      if (language) {
        *language = RETRO_LANGUAGE_ENGLISH;
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
      auto* version = static_cast<unsigned*>(data);
      if (version) {
        *version = 2;
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: {
      auto* sample_rate = static_cast<unsigned*>(data);
      if (sample_rate && context.audio_output) {
        *sample_rate = context.audio_output->EffectiveSampleRate(48000);
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
      auto* updated = static_cast<bool*>(data);
      if (updated && context.core_option_manager) {
        *updated = context.core_option_manager->ConsumeVariableUpdate();
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
      auto* variable = static_cast<retro_variable*>(data);
      if (variable && context.core_option_manager) {
        if (const char* value = context.core_option_manager->GetValue(variable->key)) {
          variable->value = value;
          return true;
        }
      }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
      auto** path = static_cast<const char**>(data);
      if (path && context.options && context.system_directory_string) {
        *context.system_directory_string = context.options->system_directory.string();
        *path = context.system_directory_string->c_str();
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
      auto** path = static_cast<const char**>(data);
      if (path && context.options && context.save_directory_string) {
        *context.save_directory_string = context.options->save_directory.string();
        *path = context.save_directory_string->c_str();
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
      auto* map = static_cast<const retro_memory_map*>(data);
      if (!context.memory_domains) {
        return false;
      }
      context.memory_domains->SetMemoryMaps(nullptr);
      if (map && map->descriptors && map->num_descriptors > 0) {
        context.memory_domains->SetMemoryMaps(map);
        if (context.trace) {
          context.trace("[sekaiemu] SET_MEMORY_MAPS received " + std::to_string(map->num_descriptors) +
                        " descriptors");
        }
      } else if (context.trace) {
        context.trace("[sekaiemu] SET_MEMORY_MAPS received empty map");
      }
      return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
      auto* callback = static_cast<retro_hw_render_callback*>(data);
      if (!callback || !context.hw_render_callback || !context.hw_render_requested) {
        return false;
      }

      *context.hw_render_requested = true;
      *context.hw_render_callback = *callback;

      if (context.trace) {
        std::ostringstream trace;
        trace << "[sekaiemu] SET_HW_RENDER"
              << " type=" << static_cast<int>(context.hw_render_callback->context_type)
              << " version=" << context.hw_render_callback->version_major
              << "." << context.hw_render_callback->version_minor
              << " depth=" << context.hw_render_callback->depth
              << " stencil=" << context.hw_render_callback->stencil;
        context.trace(trace.str());
      }

      if (context.hw_render_callback->context_type == RETRO_HW_CONTEXT_VULKAN) {
        if (context.trace) {
          context.trace("[sekaiemu] Vulkan backend not implemented yet.");
        }
        return false;
      }

      if (context.options && !context.options->probe_only && context.prepare_hardware_video_backend) {
        std::string error;
        if (!context.prepare_hardware_video_backend(context.current_geometry(), error)) {
          if (context.trace) {
            context.trace("[sekaiemu] SET_HW_RENDER prepare failed: " + error);
          }
          return false;
        }
      }

      *callback = *context.hw_render_callback;
      return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
      auto* options_v2 = static_cast<const retro_core_options_v2*>(data);
      if (!options_v2 || !options_v2->definitions || !context.core_option_manager) {
        return false;
      }
      context.core_option_manager->RegisterCoreOptionsV2(options_v2->definitions);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
      auto* intl = static_cast<const retro_core_options_v2_intl*>(data);
      if (!intl || !intl->us || !intl->us->definitions || !context.core_option_manager) {
        return false;
      }
      context.core_option_manager->RegisterCoreOptionsV2(intl->us->definitions);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
      auto* intl = static_cast<const retro_core_options_intl*>(data);
      if (!intl || !intl->us || !context.core_option_manager) {
        return false;
      }
      context.core_option_manager->RegisterCoreOptionsLegacy(intl->us);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
      auto* definitions = static_cast<const retro_core_option_definition*>(data);
      if (!definitions || !context.core_option_manager) {
        return false;
      }
      context.core_option_manager->RegisterCoreOptionsLegacy(definitions);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
      auto* variables = static_cast<const retro_variable*>(data);
      if (!variables || !context.core_option_manager) {
        return false;
      }
      context.core_option_manager->RegisterVariables(variables);
      return true;
    }
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: {
      if (!context.audio_buffer_status_callback) {
        return false;
      }
      if (!data) {
        *context.audio_buffer_status_callback = nullptr;
        return true;
      }
      auto* callback = static_cast<const retro_audio_buffer_status_callback*>(data);
      *context.audio_buffer_status_callback = callback ? callback->callback : nullptr;
      return true;
    }
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
      auto* rumble = static_cast<retro_rumble_interface*>(data);
      if (!rumble || !context.rumble_callback) {
        return false;
      }
      rumble->set_rumble_state = context.rumble_callback;
      return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
      auto* info = static_cast<const retro_system_av_info*>(data);
      if (info && context.av_info) {
        *context.av_info = *info;
        if (context.maybe_update_video_backend_geometry) {
          context.maybe_update_video_backend_geometry();
        }
        return true;
      }
      return false;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE:
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
    case RETRO_ENVIRONMENT_SET_VARIABLE:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
      return true;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: {
      auto* version = static_cast<unsigned*>(data);
      if (version) {
        *version = 1;
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

}  // namespace sekaiemu::spike
