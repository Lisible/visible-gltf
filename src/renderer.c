#include "log.h"
#include "renderer.h"
#include "src/platform.h"
#include "vulkan/vulkan_core.h"
#include <assert.h>

static const char *VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static constexpr int VALIDATION_LAYER_COUNT =
    sizeof(VALIDATION_LAYERS) / sizeof(VALIDATION_LAYERS[0]);

#ifdef VGLTF_DEBUG
static constexpr bool enable_validation_layers = true;
#else
static constexpr bool enable_validation_layers = false;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagBitsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
  (void)message_severity;
  (void)message_type;
  (void)user_data;
  VGLTF_LOG_DBG("validation layer: %s", callback_data->pMessage);
  return VK_FALSE;
}

static constexpr int REQUIRED_INSTANCE_EXTENSIONS_ARRAY_CAPACITY = 10;
struct required_instance_extensions {
  const char *extensions[REQUIRED_INSTANCE_EXTENSIONS_ARRAY_CAPACITY];
  uint32_t count;
};
void required_instance_extensions_push(
    struct required_instance_extensions *required_instance_extensions,
    const char *required_instance_extension) {
  if (required_instance_extensions->count ==
      REQUIRED_INSTANCE_EXTENSIONS_ARRAY_CAPACITY) {
    VGLTF_PANIC("required instance extensions array is full");
  }
  required_instance_extensions
      ->extensions[required_instance_extensions->count++] =
      required_instance_extension;
}

static constexpr int SUPPORTED_INSTANCE_EXTENSIONS_ARRAY_CAPACITY = 128;
struct supported_instance_extensions {
  VkExtensionProperties
      properties[SUPPORTED_INSTANCE_EXTENSIONS_ARRAY_CAPACITY];
  uint32_t count;
};
bool supported_instance_extensions_init(
    struct supported_instance_extensions *supported_instance_extensions) {
  if (vkEnumerateInstanceExtensionProperties(
          nullptr, &supported_instance_extensions->count, nullptr) !=
      VK_SUCCESS) {
    goto err;
  }

  if (supported_instance_extensions->count >
      SUPPORTED_INSTANCE_EXTENSIONS_ARRAY_CAPACITY) {
    VGLTF_LOG_ERR("supported instance extensions array cannot fit all the "
                  "VkExtensionProperties");
    goto err;
  }

  if (vkEnumerateInstanceExtensionProperties(
          nullptr, &supported_instance_extensions->count,
          supported_instance_extensions->properties) != VK_SUCCESS) {
    goto err;
  }
  return true;
err:
  return false;
}
void supported_instance_extensions_debug_print(
    const struct supported_instance_extensions *supported_instance_extensions) {
  VGLTF_LOG_DBG("Supported instance extensions:");
  for (uint32_t i = 0; i < supported_instance_extensions->count; i++) {
    VGLTF_LOG_DBG("\t- %s",
                  supported_instance_extensions->properties[i].extensionName);
  }
}
bool supported_instance_extensions_includes(
    const struct supported_instance_extensions *supported_instance_extensions,
    const char *extension_name) {
  for (uint32_t supported_instance_extension_index = 0;
       supported_instance_extension_index <
       supported_instance_extensions->count;
       supported_instance_extension_index++) {
    const VkExtensionProperties *extension_properties =
        &supported_instance_extensions
             ->properties[supported_instance_extension_index];
    if (strcmp(extension_properties->extensionName, extension_name) == 0) {
      return true;
    }
  }

  return false;
}

static constexpr uint32_t SUPPORTED_VALIDATION_LAYERS_ARRAY_CAPACITY = 64;
struct supported_validation_layers {
  VkLayerProperties properties[SUPPORTED_VALIDATION_LAYERS_ARRAY_CAPACITY];
  uint32_t count;
};
bool supported_validation_layers_init(
    struct supported_validation_layers *supported_validation_layers) {
  if (vkEnumerateInstanceLayerProperties(&supported_validation_layers->count,
                                         nullptr) != VK_SUCCESS) {
    goto err;
  }

  if (supported_validation_layers->count >
      SUPPORTED_VALIDATION_LAYERS_ARRAY_CAPACITY) {
    VGLTF_LOG_ERR("supported validation layers array cannot fit all the "
                  "VkLayerProperties");
    goto err;
  }

  if (vkEnumerateInstanceLayerProperties(
          &supported_validation_layers->count,
          supported_validation_layers->properties) != VK_SUCCESS) {
    goto err;
  }

  return true;
err:
  return false;
}

static bool are_validation_layer_supported() {
  struct supported_validation_layers supported_layers = {};
  if (!supported_validation_layers_init(&supported_layers)) {
    goto err;
  }

  for (int requested_layer_index = 0;
       requested_layer_index < VALIDATION_LAYER_COUNT;
       requested_layer_index++) {
    const char *requested_layer_name = VALIDATION_LAYERS[requested_layer_index];
    bool requested_layer_found = false;
    for (uint32_t supported_layer_index = 0;
         supported_layer_index < supported_layers.count;
         supported_layer_index++) {
      VkLayerProperties *supported_layer =
          &supported_layers.properties[supported_layer_index];
      if (strcmp(requested_layer_name, supported_layer->layerName) == 0) {
        requested_layer_found = true;
        break;
      }
    }

    if (!requested_layer_found) {
      goto err;
    }
  }

  return true;
err:
  return false;
}

static bool fetch_required_instance_extensions(
    struct required_instance_extensions *required_extensions,
    struct vgltf_platform *platform) {
  struct supported_instance_extensions supported_extensions = {};
  if (!supported_instance_extensions_init(&supported_extensions)) {
    VGLTF_LOG_ERR(
        "Couldn't fetch supported instance extensions details (OOM?)");
    goto err;
  }
  supported_instance_extensions_debug_print(&supported_extensions);

  uint32_t platform_required_extension_count = 0;
  const char *const *platform_required_extensions =
      vgltf_platform_get_vulkan_instance_extensions(
          platform, &platform_required_extension_count);
  for (uint32_t platform_required_extension_index = 0;
       platform_required_extension_index < platform_required_extension_count;
       platform_required_extension_index++) {
    required_instance_extensions_push(
        required_extensions,
        platform_required_extensions[platform_required_extension_index]);
  }
  required_instance_extensions_push(
      required_extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

  if (enable_validation_layers) {
    required_instance_extensions_push(required_extensions,
                                      VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  bool all_extensions_supported = true;
  for (uint32_t required_extension_index = 0;
       required_extension_index < required_extensions->count;
       required_extension_index++) {
    const char *required_extension_name =
        required_extensions->extensions[required_extension_index];
    if (!supported_instance_extensions_includes(&supported_extensions,
                                                required_extension_name)) {
      VGLTF_LOG_ERR("Unsupported instance extension: %s",
                    required_extension_name);
      all_extensions_supported = false;
    }
  }

  if (!all_extensions_supported) {
    VGLTF_LOG_ERR("Some required extensions are unsupported.");
    goto err;
  }

  return true;
err:
  return false;
}

static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info) {
  *create_info = (VkDebugUtilsMessengerCreateInfoEXT){};
  create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info->messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info->pfnUserCallback = debug_callback;
}

static bool vgltf_renderer_create_instance(struct vgltf_renderer *renderer,
                                           struct vgltf_platform *platform) {
  VGLTF_LOG_INFO("Creating vulkan instance...");
  if (enable_validation_layers && !are_validation_layer_supported()) {
    VGLTF_LOG_ERR("Requested validation layers aren't supported");
    goto err;
  }

  VkApplicationInfo application_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Visible GLTF",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_2};

  struct required_instance_extensions required_extensions = {};
  fetch_required_instance_extensions(&required_extensions, platform);

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &application_info,
      .enabledExtensionCount = required_extensions.count,
      .ppEnabledExtensionNames = required_extensions.extensions,
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR};

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
  if (enable_validation_layers) {
    create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
    create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    populate_debug_messenger_create_info(&debug_create_info);
    create_info.pNext = &debug_create_info;
  }

  if (vkCreateInstance(&create_info, nullptr, &renderer->instance) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create VkInstance");
    goto err;
  }

  return true;
err:
  return false;
}

static VkResult create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *create_info,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *debug_messenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, create_info, allocator, debug_messenger);
  }

  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void
destroy_debug_utils_messenger_ext(VkInstance instance,
                                  VkDebugUtilsMessengerEXT debug_messenger,
                                  const VkAllocationCallbacks *allocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debug_messenger, allocator);
  }
}

static void
vgltf_renderer_setup_debug_messenger(struct vgltf_renderer *renderer) {
  if (!enable_validation_layers)
    return;
  VkDebugUtilsMessengerCreateInfoEXT create_info;
  populate_debug_messenger_create_info(&create_info);
  create_debug_utils_messenger_ext(renderer->instance, &create_info, nullptr,
                                   &renderer->debug_messenger);
}

static constexpr int AVAILABLE_PHYSICAL_DEVICE_ARRAY_CAPACITY = 128;
struct available_physical_devices {
  VkPhysicalDevice devices[AVAILABLE_PHYSICAL_DEVICE_ARRAY_CAPACITY];
  uint32_t count;
};
static bool
available_physical_devices_init(VkInstance instance,
                                struct available_physical_devices *devices) {

  if (vkEnumeratePhysicalDevices(instance, &devices->count, nullptr) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't enumerate physical devices");
    goto err;
  }

  if (devices->count == 0) {
    VGLTF_LOG_ERR("Failed to find any GPU with Vulkan support");
    goto err;
  }

  if (devices->count > AVAILABLE_PHYSICAL_DEVICE_ARRAY_CAPACITY) {
    VGLTF_LOG_ERR("available physical devices array cannot fit all available "
                  "physical devices");
    goto err;
  }

  if (vkEnumeratePhysicalDevices(instance, &devices->count, devices->devices) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't enumerate physical devices");
    goto err;
  }

  return true;
err:
  return false;
}

struct queue_family_indices {
  uint32_t graphics_family;
  uint32_t present_family;
  bool has_graphics_family;
  bool has_present_family;
};
bool queue_family_indices_is_complete(
    const struct queue_family_indices *indices) {
  return indices->has_graphics_family && indices->has_present_family;
}
bool queue_family_indices_for_device(struct queue_family_indices *indices,
                                     VkPhysicalDevice device,
                                     VkSurfaceKHR surface) {
  static constexpr uint32_t QUEUE_FAMILY_PROPERTIES_ARRAY_CAPACITY = 64;
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);

  if (queue_family_count > QUEUE_FAMILY_PROPERTIES_ARRAY_CAPACITY) {
    VGLTF_LOG_ERR(
        "Queue family properties array cannot fit all queue family properties");
    goto err;
  }

  VkQueueFamilyProperties
      queue_family_properties[QUEUE_FAMILY_PROPERTIES_ARRAY_CAPACITY] = {};
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_family_properties);

  for (uint32_t queue_family_index = 0; queue_family_index < queue_family_count;
       queue_family_index++) {
    VkQueueFamilyProperties *queue_family =
        &queue_family_properties[queue_family_index];

    VkBool32 present_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_family_index, surface,
                                         &present_support);

    if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices->graphics_family = queue_family_index;
      indices->has_graphics_family = true;
    }

    if (present_support) {
      indices->present_family = queue_family_index;
      indices->has_present_family = true;
    }

    if (queue_family_indices_is_complete(indices)) {
      break;
    }
  }

  return true;
err:
  return false;
}

static bool is_in_array(uint32_t *array, int length, uint32_t value) {
  for (int i = 0; i < length; i++) {
    if (array[i] == value) {
      return true;
    }
  }

  return false;
}

static constexpr uint32_t SUPPORTED_EXTENSIONS_ARRAY_CAPACITY = 128;
struct supported_extensions {
  VkExtensionProperties properties[SUPPORTED_EXTENSIONS_ARRAY_CAPACITY];
  uint32_t count;
};
bool supported_extensions_init(
    struct supported_extensions *supported_extensions,
    VkPhysicalDevice device) {
  if (vkEnumerateDeviceExtensionProperties(device, nullptr,
                                           &supported_extensions->count,
                                           nullptr) != VK_SUCCESS) {
    goto err;
  }

  if (supported_extensions->count > SUPPORTED_EXTENSIONS_ARRAY_CAPACITY) {
    VGLTF_LOG_ERR(
        "supported extensions aarray cannot fit all the VkExtensionProperties");
    goto err;
  }

  if (vkEnumerateDeviceExtensionProperties(
          device, nullptr, &supported_extensions->count,
          supported_extensions->properties) != VK_SUCCESS) {
    goto err;
  }

  return true;
err:
  return false;
}

static bool supported_extensions_includes_extension(
    struct supported_extensions *supported_extensions,
    const char *extension_name) {
  for (uint32_t supported_extension_index = 0;
       supported_extension_index < supported_extensions->count;
       supported_extension_index++) {
    if (strcmp(supported_extensions->properties[supported_extension_index]
                   .extensionName,
               extension_name) == 0) {
      return true;
    }
  }
  return false;
}

static const char *DEVICE_EXTENSIONS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                          "VK_KHR_portability_subset"};
static constexpr int DEVICE_EXTENSION_COUNT =
    sizeof(DEVICE_EXTENSIONS) / sizeof(DEVICE_EXTENSIONS[0]);
static bool are_device_extensions_supported(VkPhysicalDevice device) {
  struct supported_extensions supported_extensions = {};
  if (!supported_extensions_init(&supported_extensions, device)) {
    goto err;
  }

  for (uint32_t required_extension_index = 0;
       required_extension_index < DEVICE_EXTENSION_COUNT;
       required_extension_index++) {
    if (!supported_extensions_includes_extension(
            &supported_extensions,
            DEVICE_EXTENSIONS[required_extension_index])) {
      VGLTF_LOG_DBG("Unsupported: %s",
                    DEVICE_EXTENSIONS[required_extension_index]);
      goto err;
    }
  }

  return true;

err:
  return false;
}

static constexpr int SWAPCHAIN_SUPPORT_DETAILS_MAX_SURFACE_FORMAT_COUNT = 256;
static constexpr int SWAPCHAIN_SUPPORT_DETAILS_MAX_PRESENT_MODE_COUNT = 256;
struct swapchain_support_details {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR
      formats[SWAPCHAIN_SUPPORT_DETAILS_MAX_SURFACE_FORMAT_COUNT];
  VkPresentModeKHR
      present_modes[SWAPCHAIN_SUPPORT_DETAILS_MAX_PRESENT_MODE_COUNT];
  uint32_t format_count;
  uint32_t present_mode_count;
};
bool swapchain_support_details_query_from_device(
    struct swapchain_support_details *swapchain_support_details,
    VkPhysicalDevice device, VkSurfaceKHR surface) {
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          device, surface, &swapchain_support_details->capabilities) !=
      VK_SUCCESS) {
    goto err;
  }

  if (vkGetPhysicalDeviceSurfaceFormatsKHR(
          device, surface, &swapchain_support_details->format_count, nullptr) !=
      VK_SUCCESS) {
    goto err;
  }

  if (swapchain_support_details->format_count != 0 &&
      swapchain_support_details->format_count <
          SWAPCHAIN_SUPPORT_DETAILS_MAX_SURFACE_FORMAT_COUNT) {
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &swapchain_support_details->format_count,
            swapchain_support_details->formats) != VK_SUCCESS) {
      goto err;
    }
  }

  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          device, surface, &swapchain_support_details->present_mode_count,
          nullptr) != VK_SUCCESS) {
    goto err;
  }

  if (swapchain_support_details->present_mode_count != 0 &&
      swapchain_support_details->present_mode_count <
          SWAPCHAIN_SUPPORT_DETAILS_MAX_PRESENT_MODE_COUNT) {
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &swapchain_support_details->present_mode_count,
            swapchain_support_details->present_modes) != VK_SUCCESS) {
      goto err;
    }
  }

  return true;
err:
  return false;
}

static bool is_physical_device_suitable(VkPhysicalDevice device,
                                        VkSurfaceKHR surface) {
  struct queue_family_indices indices = {};
  queue_family_indices_for_device(&indices, device, surface);

  VGLTF_LOG_DBG("Checking for physical device extension support");
  bool extensions_supported = are_device_extensions_supported(device);
  VGLTF_LOG_DBG("Supported: %d", extensions_supported);

  bool swapchain_adequate = false;
  if (extensions_supported) {

    VGLTF_LOG_DBG("Checking for swapchain support details");
    struct swapchain_support_details swapchain_support_details = {};
    if (!swapchain_support_details_query_from_device(&swapchain_support_details,
                                                     device, surface)) {
      VGLTF_LOG_ERR("Couldn't query swapchain support details from device");
      goto err;
    }

    swapchain_adequate = swapchain_support_details.format_count > 0 &&
                         swapchain_support_details.present_mode_count > 0;
  }

  return queue_family_indices_is_complete(&indices) && extensions_supported &&
         swapchain_adequate;
err:
  return false;
}

static bool
vgltf_renderer_pick_physical_device(struct vgltf_renderer *renderer) {
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;

  struct available_physical_devices available_physical_devices = {};
  if (!available_physical_devices_init(renderer->instance,
                                       &available_physical_devices)) {
    VGLTF_LOG_ERR("Couldn't fetch available physical devices");
    goto err;
  }

  for (uint32_t available_physical_device_index = 0;
       available_physical_device_index < available_physical_devices.count;
       available_physical_device_index++) {
    VkPhysicalDevice available_physical_device =
        available_physical_devices.devices[available_physical_device_index];
    if (is_physical_device_suitable(available_physical_device,
                                    renderer->surface)) {
      physical_device = available_physical_device;
      break;
    }
  }

  if (physical_device == VK_NULL_HANDLE) {
    VGLTF_LOG_ERR("Failed to find a suitable GPU");
    goto err;
  }

  renderer->physical_device = physical_device;

  return true;
err:
  return false;
}

static bool
vgltf_renderer_create_logical_device(struct vgltf_renderer *renderer) {
  struct queue_family_indices queue_family_indices = {};
  queue_family_indices_for_device(&queue_family_indices,
                                  renderer->physical_device, renderer->surface);
  static constexpr int MAX_QUEUE_FAMILY_COUNT = 2;

  uint32_t unique_queue_families[MAX_QUEUE_FAMILY_COUNT] = {};
  int unique_queue_family_count = 0;

  if (!is_in_array(unique_queue_families, unique_queue_family_count,
                   queue_family_indices.graphics_family)) {
    assert(unique_queue_family_count < MAX_QUEUE_FAMILY_COUNT);
    unique_queue_families[unique_queue_family_count++] =
        queue_family_indices.graphics_family;
  }
  if (!is_in_array(unique_queue_families, unique_queue_family_count,
                   queue_family_indices.present_family)) {
    assert(unique_queue_family_count < MAX_QUEUE_FAMILY_COUNT);
    unique_queue_families[unique_queue_family_count++] =
        queue_family_indices.present_family;
  }

  float queue_priority = 1.f;
  VkDeviceQueueCreateInfo queue_create_infos[MAX_QUEUE_FAMILY_COUNT] = {};
  int queue_create_info_count = 0;
  for (int unique_queue_family_index = 0;
       unique_queue_family_index < unique_queue_family_count;
       unique_queue_family_index++) {
    queue_create_infos[queue_create_info_count++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = unique_queue_families[unique_queue_family_index],
        .queueCount = 1,
        .pQueuePriorities = &queue_priority};
  }

  VkPhysicalDeviceFeatures device_features = {};
  VkDeviceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pQueueCreateInfos = queue_create_infos,
      .queueCreateInfoCount = queue_create_info_count,
      .pEnabledFeatures = &device_features,
      .ppEnabledExtensionNames = DEVICE_EXTENSIONS,
      .enabledExtensionCount = DEVICE_EXTENSION_COUNT};
  if (vkCreateDevice(renderer->physical_device, &create_info, nullptr,
                     &renderer->device) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create logical device");
    goto err;
  }

  vkGetDeviceQueue(renderer->device, queue_family_indices.graphics_family, 0,
                   &renderer->graphics_queue);
  vkGetDeviceQueue(renderer->device, queue_family_indices.present_family, 0,
                   &renderer->present_queue);

  return true;
err:
  return false;
}

static bool vgltf_renderer_create_surface(struct vgltf_renderer *renderer,
                                          struct vgltf_platform *platform) {
  if (!vgltf_platform_create_vulkan_surface(platform, renderer->instance,
                                            &renderer->surface)) {
    VGLTF_LOG_ERR("Couldn't create surface");
    goto err;
  }

  return true;
err:
  return false;
}

static VkSurfaceFormatKHR
choose_swapchain_surface_format(VkSurfaceFormatKHR *available_formats,
                                uint32_t available_format_count) {
  for (uint32_t available_format_index = 0;
       available_format_index < available_format_count;
       available_format_index++) {
    VkSurfaceFormatKHR *available_format =
        &available_formats[available_format_index];
    if (available_format->format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_format->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return *available_format;
    }
  }

  return available_formats[0];
}

static VkPresentModeKHR
choose_swapchain_present_mode(VkPresentModeKHR *available_modes,
                              uint32_t available_mode_count) {
  for (uint32_t available_mode_index = 0;
       available_mode_index < available_mode_count; available_mode_index++) {
    VkPresentModeKHR available_mode = available_modes[available_mode_index];
    if (available_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return available_mode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

static uint32_t clamp_uint32(uint32_t min, uint32_t max, uint32_t value) {
  return value < min ? min : value > max ? max : value;
}

static VkExtent2D
choose_swapchain_extent(const VkSurfaceCapabilitiesKHR *capabilities, int width,
                        int height) {
  if (capabilities->currentExtent.width != UINT32_MAX) {
    return capabilities->currentExtent;
  } else {
    VkExtent2D actual_extent = {width, height};
    actual_extent.width =
        clamp_uint32(capabilities->minImageExtent.width,
                     capabilities->maxImageExtent.width, actual_extent.width);
    actual_extent.height =
        clamp_uint32(capabilities->minImageExtent.height,
                     capabilities->maxImageExtent.height, actual_extent.height);
    return actual_extent;
  }
}

static bool vgltf_renderer_create_swapchain(struct vgltf_renderer *renderer,
                                            int window_width_px,
                                            int window_height_px) {
  struct swapchain_support_details swapchain_support_details = {};
  swapchain_support_details_query_from_device(
      &swapchain_support_details, renderer->physical_device, renderer->surface);

  VkSurfaceFormatKHR surface_format =
      choose_swapchain_surface_format(swapchain_support_details.formats,
                                      swapchain_support_details.format_count);
  VkPresentModeKHR present_mode = choose_swapchain_present_mode(
      swapchain_support_details.present_modes,
      swapchain_support_details.present_mode_count);

  VkExtent2D extent =
      choose_swapchain_extent(&swapchain_support_details.capabilities,
                              window_width_px, window_height_px);
  uint32_t image_count =
      swapchain_support_details.capabilities.minImageCount + 1;
  if (swapchain_support_details.capabilities.maxImageCount > 0 &&
      image_count > swapchain_support_details.capabilities.maxImageCount) {
    image_count = swapchain_support_details.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = renderer->surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
  struct queue_family_indices indices = {};
  queue_family_indices_for_device(&indices, renderer->physical_device,
                                  renderer->surface);
  uint32_t queue_family_indices[] = {indices.graphics_family,
                                     indices.present_family};
  if (indices.graphics_family != indices.present_family) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform =
      swapchain_support_details.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(renderer->device, &create_info, nullptr,
                           &renderer->swapchain) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Swapchain creation failed!");
    goto err;
  }

  if (vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                              &renderer->swapchain_image_count,
                              nullptr) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't get swapchain image count");
    goto destroy_swapchain;
  }

  if (renderer->swapchain_image_count >
      VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT) {
    VGLTF_LOG_ERR("Swapchain image array cannot fit all %d swapchain images",
                  renderer->swapchain_image_count);
    goto destroy_swapchain;
  }

  if (vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                              &renderer->swapchain_image_count,
                              renderer->swapchain_images) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't get swapchain images");
    goto destroy_swapchain;
  }

  renderer->swapchain_image_format = surface_format.format;
  renderer->swapchain_extent = extent;

  return true;
destroy_swapchain:
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, nullptr);
err:
  return false;
}

static bool vgltf_renderer_create_image_views(struct vgltf_renderer *renderer) {
  uint32_t swapchain_image_index;
  for (swapchain_image_index = 0;
       swapchain_image_index < renderer->swapchain_image_count;
       swapchain_image_index++) {
    VkImage swapchain_image = renderer->swapchain_images[swapchain_image_index];

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = swapchain_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = renderer->swapchain_image_format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .levelCount = 1,
                             .layerCount = 1}};

    if (vkCreateImageView(
            renderer->device, &create_info, nullptr,
            &renderer->swapchain_image_views[swapchain_image_index]) !=
        VK_SUCCESS) {
      goto err;
    }
  }
  return true;
err:
  for (uint32_t to_remove_index = 0; to_remove_index < swapchain_image_index;
       to_remove_index++) {
    vkDestroyImageView(renderer->device,
                       renderer->swapchain_image_views[to_remove_index],
                       nullptr);
  }
  return false;
}

static bool create_shader_module(VkDevice device, const unsigned char *code,
                                 int size, VkShaderModule *out) {
  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = (const uint32_t *)code,
  };
  if (vkCreateShaderModule(device, &create_info, nullptr, out) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create shader module");
    goto err;
  }
  return true;
err:
  return false;
}

static bool vgltf_renderer_create_render_pass(struct vgltf_renderer *renderer) {
  VkAttachmentDescription color_attachment = {
      .format = renderer->swapchain_image_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };
  VkSubpassDescription subpass = {.pipelineBindPoint =
                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  .pColorAttachments = &color_attachment_ref};
  VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &subpass};

  if (vkCreateRenderPass(renderer->device, &render_pass_info, nullptr,
                         &renderer->render_pass) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create render pass");
    goto err;
  }

  return true;
err:
  return false;
}

static bool
vgltf_renderer_create_graphics_pipeline(struct vgltf_renderer *renderer) {
  static constexpr unsigned char triangle_shader_vert_code[] = {
#embed "../compiled_shaders/triangle.vert.spv"
  };
  static constexpr unsigned char triangle_shader_frag_code[] = {
#embed "../compiled_shaders/triangle.frag.spv"
  };

  VkShaderModule triangle_shader_vert_module;
  if (!create_shader_module(renderer->device, triangle_shader_vert_code,
                            sizeof(triangle_shader_vert_code),
                            &triangle_shader_vert_module)) {
    VGLTF_LOG_ERR("Couldn't create triangle vert shader module");
    goto err;
  }

  VkShaderModule triangle_shader_frag_module;
  if (!create_shader_module(renderer->device, triangle_shader_frag_code,
                            sizeof(triangle_shader_frag_code),
                            &triangle_shader_frag_module)) {
    VGLTF_LOG_ERR("Couldn't create triangle frag shader module");
    goto destroy_vert_shader_module;
  }

  VkPipelineShaderStageCreateInfo triangle_shader_vert_stage_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = triangle_shader_vert_module,
      .pName = "main"};
  VkPipelineShaderStageCreateInfo triangle_shader_frag_stage_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = triangle_shader_frag_module,
      .pName = "main"};
  VkPipelineShaderStageCreateInfo shader_stages[] = {
      triangle_shader_vert_stage_create_info,
      triangle_shader_frag_stage_create_info};

  VkDynamicState dynamic_states[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
      .pDynamicStates = dynamic_states};

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .vertexAttributeDescriptionCount = 0,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.f,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE};

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment};

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };

  if (vkCreatePipelineLayout(renderer->device, &pipeline_layout_info, nullptr,
                             &renderer->pipeline_layout) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create pipeline layout");
    goto destroy_frag_shader_module;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &color_blending,
      .pDynamicState = &dynamic_state,
      .layout = renderer->pipeline_layout,
      .renderPass = renderer->render_pass,
      .subpass = 0,
  };

  if (vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr,
                                &renderer->graphics_pipeline) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create pipeline");
    goto destroy_pipeline_layout;
  }

  vkDestroyShaderModule(renderer->device, triangle_shader_frag_module, nullptr);
  vkDestroyShaderModule(renderer->device, triangle_shader_vert_module, nullptr);
  return true;
destroy_pipeline_layout:
  vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, nullptr);
destroy_frag_shader_module:
  vkDestroyShaderModule(renderer->device, triangle_shader_frag_module, nullptr);
destroy_vert_shader_module:
  vkDestroyShaderModule(renderer->device, triangle_shader_vert_module, nullptr);
err:
  return false;
}

bool vgltf_renderer_init(struct vgltf_renderer *renderer,
                         struct vgltf_platform *platform) {
  if (!vgltf_renderer_create_instance(renderer, platform)) {
    VGLTF_LOG_ERR("instance creation failed");
    goto err;
  }
  vgltf_renderer_setup_debug_messenger(renderer);
  if (!vgltf_renderer_create_surface(renderer, platform)) {
    goto destroy_instance;
  }

  if (!vgltf_renderer_pick_physical_device(renderer)) {
    VGLTF_LOG_ERR("Couldn't pick physical device");
    goto destroy_surface;
  }
  if (!vgltf_renderer_create_logical_device(renderer)) {
    VGLTF_LOG_ERR("Couldn't create logical device");
    goto destroy_device;
  }

  struct vgltf_window_size window_size = {800, 600};
  if (!vgltf_platform_get_window_size(platform, &window_size)) {
    VGLTF_LOG_ERR("Couldn't get window size");
    goto destroy_device;
  }

  if (!vgltf_renderer_create_swapchain(renderer, window_size.width,
                                       window_size.height)) {
    VGLTF_LOG_ERR("Couldn't create swapchain");
    goto destroy_device;
  }

  if (!vgltf_renderer_create_image_views(renderer)) {
    VGLTF_LOG_ERR("Couldn't create image views");
    goto destroy_swapchain;
  }

  if (!vgltf_renderer_create_render_pass(renderer)) {
    VGLTF_LOG_ERR("Couldn't create render pass");
    goto destroy_image_views;
  }

  if (!vgltf_renderer_create_graphics_pipeline(renderer)) {
    VGLTF_LOG_ERR("Couldn't create graphics pipeline");
    goto destroy_render_pass;
  }

  return true;
destroy_render_pass:
  vkDestroyRenderPass(renderer->device, renderer->render_pass, nullptr);
destroy_image_views:
  for (uint32_t swapchain_image_view_index = 0;
       swapchain_image_view_index < renderer->swapchain_image_count;
       swapchain_image_view_index++) {
    vkDestroyImageView(
        renderer->device,
        renderer->swapchain_image_views[swapchain_image_view_index], nullptr);
  }
destroy_swapchain:
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, nullptr);
destroy_device:
  vkDestroyDevice(renderer->device, nullptr);
destroy_surface:
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, nullptr);
destroy_instance:
  if (enable_validation_layers) {
    destroy_debug_utils_messenger_ext(renderer->instance,
                                      renderer->debug_messenger, nullptr);
  }
  vkDestroyInstance(renderer->instance, nullptr);
err:
  return false;
}
void vgltf_renderer_deinit(struct vgltf_renderer *renderer) {
  vkDestroyPipeline(renderer->device, renderer->graphics_pipeline, nullptr);
  vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, nullptr);
  vkDestroyRenderPass(renderer->device, renderer->render_pass, nullptr);
  for (uint32_t swapchain_image_view_index = 0;
       swapchain_image_view_index < renderer->swapchain_image_count;
       swapchain_image_view_index++) {
    vkDestroyImageView(
        renderer->device,
        renderer->swapchain_image_views[swapchain_image_view_index], nullptr);
  }
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, nullptr);
  vkDestroyDevice(renderer->device, nullptr);
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, nullptr);
  if (enable_validation_layers) {
    destroy_debug_utils_messenger_ext(renderer->instance,
                                      renderer->debug_messenger, nullptr);
  }
  vkDestroyInstance(renderer->instance, nullptr);
}
