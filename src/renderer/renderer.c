#include "renderer.h"
#include "../image.h"
#include "../log.h"
#include "../maths.h"
#include "../platform.h"
#include "vma_usage.h"
#include <math.h>

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "vendor/tiny_obj_loader_c.h"

#include <assert.h>
#include <vulkan/vulkan_core.h>

static const char MODEL_PATH[] = "assets/model.obj";
static const char TEXTURE_PATH[] = "assets/texture.png";

VkVertexInputBindingDescription vgltf_vertex_binding_description() {
  return (VkVertexInputBindingDescription){
      .binding = 0,
      .stride = sizeof(struct vgltf_vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
}
struct vgltf_vertex_input_attribute_descriptions
vgltf_vertex_attribute_descriptions(void) {
  return (struct vgltf_vertex_input_attribute_descriptions){
      .descriptions = {(VkVertexInputAttributeDescription){
                           .binding = 0,
                           .location = 0,
                           .format = VK_FORMAT_R32G32B32_SFLOAT,
                           .offset = offsetof(struct vgltf_vertex, position)},
                       (VkVertexInputAttributeDescription){
                           .binding = 0,
                           .location = 1,
                           .format = VK_FORMAT_R32G32B32_SFLOAT,
                           .offset = offsetof(struct vgltf_vertex, color)},
                       (VkVertexInputAttributeDescription){
                           .binding = 0,
                           .location = 2,
                           .format = VK_FORMAT_R32G32_SFLOAT,
                           .offset = offsetof(struct vgltf_vertex,
                                              texture_coordinates)}},
      .count = 3};
}

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
#ifdef VGLTF_PLATFORM_MACOS
  required_instance_extensions_push(
      required_extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif // VGLTF_PLATFORM_MACOS

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

static bool vgltf_vk_instance_init(struct vgltf_vk_instance *instance,
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

  VkInstanceCreateFlags flags = 0;
#ifdef VGLTF_PLATFORM_MACOS
  flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif // VGLTF_PLATFORM_MACOS

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &application_info,
      .enabledExtensionCount = required_extensions.count,
      .ppEnabledExtensionNames = required_extensions.extensions,
      .flags = flags};

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
  if (enable_validation_layers) {
    create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
    create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    populate_debug_messenger_create_info(&debug_create_info);
    create_info.pNext = &debug_create_info;
  }

  if (vkCreateInstance(&create_info, nullptr, &instance->instance) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create VkInstance");
    goto err;
  }

  return true;
err:
  return false;
}
static void vgltf_vk_instance_deinit(struct vgltf_vk_instance *instance) {
  vkDestroyInstance(instance->instance, nullptr);
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
  create_debug_utils_messenger_ext(renderer->instance.instance, &create_info,
                                   nullptr, &renderer->debug_messenger);
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

static constexpr uint32_t SUPPORTED_EXTENSIONS_ARRAY_CAPACITY = 1024;
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
    VGLTF_LOG_ERR("supported extensions array cannot fit all the supported "
                  "VkExtensionProperties (%u)",
                  supported_extensions->count);
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

static const char *DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef VGLTF_PLATFORM_MACOS
    "VK_KHR_portability_subset",
#endif
};
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

  VkPhysicalDeviceFeatures supported_features;
  vkGetPhysicalDeviceFeatures(device, &supported_features);

  return queue_family_indices_is_complete(&indices) && extensions_supported &&
         swapchain_adequate && supported_features.samplerAnisotropy;
err:
  return false;
}

static bool pick_physical_device(VkPhysicalDevice *physical_device,
                                 struct vgltf_vk_instance *instance,
                                 VkSurfaceKHR surface) {
  VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
  struct available_physical_devices available_physical_devices = {};
  if (!available_physical_devices_init(instance->instance,
                                       &available_physical_devices)) {
    VGLTF_LOG_ERR("Couldn't fetch available physical devices");
    goto err;
  }

  for (uint32_t available_physical_device_index = 0;
       available_physical_device_index < available_physical_devices.count;
       available_physical_device_index++) {
    VkPhysicalDevice available_physical_device =
        available_physical_devices.devices[available_physical_device_index];
    if (is_physical_device_suitable(available_physical_device, surface)) {
      vk_physical_device = available_physical_device;
      break;
    }
  }

  if (vk_physical_device == VK_NULL_HANDLE) {
    VGLTF_LOG_ERR("Failed to find a suitable GPU");
    goto err;
  }

  *physical_device = vk_physical_device;

  return true;
err:
  return false;
}

static bool create_logical_device(VkDevice *device, VkQueue *graphics_queue,
                                  VkQueue *present_queue,
                                  VkPhysicalDevice physical_device,
                                  VkSurfaceKHR surface) {
  struct queue_family_indices queue_family_indices = {};
  queue_family_indices_for_device(&queue_family_indices, physical_device,
                                  surface);
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

  VkPhysicalDeviceFeatures device_features = {
      .samplerAnisotropy = VK_TRUE,
  };
  VkDeviceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pQueueCreateInfos = queue_create_infos,
      .queueCreateInfoCount = queue_create_info_count,
      .pEnabledFeatures = &device_features,
      .ppEnabledExtensionNames = DEVICE_EXTENSIONS,
      .enabledExtensionCount = DEVICE_EXTENSION_COUNT};
  if (vkCreateDevice(physical_device, &create_info, nullptr, device) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create logical device");
    goto err;
  }

  vkGetDeviceQueue(*device, queue_family_indices.graphics_family, 0,
                   graphics_queue);
  vkGetDeviceQueue(*device, queue_family_indices.present_family, 0,
                   present_queue);

  return true;
err:
  return false;
}

static bool create_allocator(VmaAllocator *allocator,
                             struct vgltf_vk_device *device,
                             struct vgltf_vk_instance *instance) {
  VmaAllocatorCreateInfo create_info = {.device = device->device,
                                        .instance = instance->instance,
                                        .physicalDevice =
                                            device->physical_device};

  if (vmaCreateAllocator(&create_info, allocator) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create VMA allocator");
    goto err;
  }
  return true;
err:
  return false;
}

static bool vgltf_vk_surface_init(struct vgltf_vk_surface *surface,
                                  struct vgltf_vk_instance *instance,
                                  struct vgltf_platform *platform) {
  if (!vgltf_platform_create_vulkan_surface(platform, instance->instance,
                                            &surface->surface)) {
    VGLTF_LOG_ERR("Couldn't create surface");
    goto err;
  }

  return true;
err:
  return false;
}

static void vgltf_vk_surface_deinit(struct vgltf_vk_surface *surface,
                                    struct vgltf_vk_instance *instance) {
  vkDestroySurfaceKHR(instance->instance, surface->surface, nullptr);
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

static bool create_swapchain(struct vgltf_vk_swapchain *swapchain,
                             struct vgltf_vk_device *device,
                             struct vgltf_vk_surface *surface,
                             struct vgltf_window_size *window_size) {
  struct swapchain_support_details swapchain_support_details = {};
  swapchain_support_details_query_from_device(
      &swapchain_support_details, device->physical_device, surface->surface);

  VkSurfaceFormatKHR surface_format =
      choose_swapchain_surface_format(swapchain_support_details.formats,
                                      swapchain_support_details.format_count);
  VkPresentModeKHR present_mode = choose_swapchain_present_mode(
      swapchain_support_details.present_modes,
      swapchain_support_details.present_mode_count);

  VkExtent2D extent =
      choose_swapchain_extent(&swapchain_support_details.capabilities,
                              window_size->width, window_size->height);
  uint32_t image_count =
      swapchain_support_details.capabilities.minImageCount + 1;
  if (swapchain_support_details.capabilities.maxImageCount > 0 &&
      image_count > swapchain_support_details.capabilities.maxImageCount) {
    image_count = swapchain_support_details.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface->surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
  struct queue_family_indices indices = {};
  queue_family_indices_for_device(&indices, device->physical_device,
                                  surface->surface);
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

  if (vkCreateSwapchainKHR(device->device, &create_info, nullptr,
                           &swapchain->swapchain) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Swapchain creation failed!");
    goto err;
  }

  if (vkGetSwapchainImagesKHR(device->device, swapchain->swapchain,
                              &swapchain->swapchain_image_count,
                              nullptr) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't get swapchain image count");
    goto destroy_swapchain;
  }

  if (swapchain->swapchain_image_count >
      VGLTF_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT) {
    VGLTF_LOG_ERR("Swapchain image array cannot fit all %d swapchain images",
                  swapchain->swapchain_image_count);
    goto destroy_swapchain;
  }

  if (vkGetSwapchainImagesKHR(device->device, swapchain->swapchain,
                              &swapchain->swapchain_image_count,
                              swapchain->swapchain_images) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't get swapchain images");
    goto destroy_swapchain;
  }

  swapchain->swapchain_image_format = surface_format.format;
  swapchain->swapchain_extent = extent;

  return true;
destroy_swapchain:
  vkDestroySwapchainKHR(device->device, swapchain->swapchain, nullptr);
err:
  return false;
}

static bool create_image_view(struct vgltf_vk_device *device, VkImage image,
                              VkFormat format, VkImageView *image_view,
                              VkImageAspectFlags aspect_flags,
                              uint32_t mip_level_count) {

  VkImageViewCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                     VK_COMPONENT_SWIZZLE_IDENTITY,
                     VK_COMPONENT_SWIZZLE_IDENTITY,
                     VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {.aspectMask = aspect_flags,
                           .levelCount = mip_level_count,
                           .layerCount = 1}};
  if (vkCreateImageView(device->device, &create_info, nullptr, image_view) !=
      VK_SUCCESS) {
    return false;
  }

  return true;
}

static bool create_swapchain_image_views(struct vgltf_vk_swapchain *swapchain,
                                         struct vgltf_vk_device *device) {
  uint32_t swapchain_image_index;
  for (swapchain_image_index = 0;
       swapchain_image_index < swapchain->swapchain_image_count;
       swapchain_image_index++) {
    VkImage swapchain_image =
        swapchain->swapchain_images[swapchain_image_index];

    if (!create_image_view(
            device, swapchain_image, swapchain->swapchain_image_format,
            &swapchain->swapchain_image_views[swapchain_image_index],
            VK_IMAGE_ASPECT_COLOR_BIT, 1)) {
      goto err;
    }
  }
  return true;
err:
  for (uint32_t to_remove_index = 0; to_remove_index < swapchain_image_index;
       to_remove_index++) {
    vkDestroyImageView(device->device,
                       swapchain->swapchain_image_views[to_remove_index],
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

static VkFormat find_supported_format(struct vgltf_renderer *renderer,
                                      const VkFormat *candidates,
                                      int candidate_count, VkImageTiling tiling,
                                      VkFormatFeatureFlags features) {
  for (int candidate_index = 0; candidate_index < candidate_count;
       candidate_index++) {
    VkFormat candidate = candidates[candidate_index];
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(renderer->device.physical_device,
                                        candidate, &properties);
    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (properties.linearTilingFeatures & features) == features) {
      return candidate;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (properties.optimalTilingFeatures & features) == features) {
      return candidate;
    }
  }

  return VK_FORMAT_UNDEFINED;
}

static VkFormat find_depth_format(struct vgltf_renderer *renderer) {
  return find_supported_format(renderer,
                               (const VkFormat[]){VK_FORMAT_D32_SFLOAT,
                                                  VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                  VK_FORMAT_D24_UNORM_S8_UINT},
                               3, VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static bool vgltf_renderer_create_render_pass(struct vgltf_renderer *renderer) {
  VkAttachmentDescription color_attachment = {
      .format = renderer->swapchain.swapchain_image_format,
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
  VkAttachmentDescription depth_attachment = {
      .format = find_depth_format(renderer),
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkAttachmentReference depth_attachment_ref = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .pColorAttachments = &color_attachment_ref,
      .colorAttachmentCount = 1,
      .pDepthStencilAttachment = &depth_attachment_ref};
  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};

  VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};
  int attachment_count = sizeof(attachments) / sizeof(attachments[0]);
  VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = attachment_count,
      .pAttachments = attachments,
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency};

  if (vkCreateRenderPass(renderer->device.device, &render_pass_info, nullptr,
                         &renderer->render_pass) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create render pass");
    goto err;
  }

  return true;
err:
  return false;
}

static bool
vgltf_renderer_create_descriptor_set_layout(struct vgltf_renderer *renderer) {
  VkDescriptorSetLayoutBinding ubo_layout_binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };
  VkDescriptorSetLayoutBinding sampler_layout_binding = {
      .binding = 1,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorSetLayoutBinding bindings[] = {ubo_layout_binding,
                                             sampler_layout_binding};
  int binding_count = sizeof(bindings) / sizeof(bindings[0]);

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = binding_count,
      .pBindings = bindings};

  if (vkCreateDescriptorSetLayout(renderer->device.device, &layout_info,
                                  nullptr, &renderer->descriptor_set_layout) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create descriptor set layout");
    goto err;
  }
  return true;
err:
  return false;
}

static bool
vgltf_renderer_create_graphics_pipeline(struct vgltf_renderer *renderer) {
  static unsigned char triangle_shader_vert_code[] = {
#embed "../../compiled_shaders/triangle.vert.spv"
  };
  static unsigned char triangle_shader_frag_code[] = {
#embed "../../compiled_shaders/triangle.frag.spv"
  };

  VkShaderModule triangle_shader_vert_module;
  if (!create_shader_module(renderer->device.device, triangle_shader_vert_code,
                            sizeof(triangle_shader_vert_code),
                            &triangle_shader_vert_module)) {
    VGLTF_LOG_ERR("Couldn't create triangle vert shader module");
    goto err;
  }

  VkShaderModule triangle_shader_frag_module;
  if (!create_shader_module(renderer->device.device, triangle_shader_frag_code,
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

  VkVertexInputBindingDescription vertex_binding_description =
      vgltf_vertex_binding_description();
  struct vgltf_vertex_input_attribute_descriptions
      vertex_attribute_descriptions = vgltf_vertex_attribute_descriptions();

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .vertexAttributeDescriptionCount = vertex_attribute_descriptions.count,
      .pVertexBindingDescriptions = &vertex_binding_description,
      .pVertexAttributeDescriptions =
          vertex_attribute_descriptions.descriptions};

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
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment};

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &renderer->descriptor_set_layout};

  if (vkCreatePipelineLayout(renderer->device.device, &pipeline_layout_info,
                             nullptr,
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
      .pDepthStencilState = &depth_stencil,
      .pDynamicState = &dynamic_state,
      .layout = renderer->pipeline_layout,
      .renderPass = renderer->render_pass,
      .subpass = 0,
  };

  if (vkCreateGraphicsPipelines(renderer->device.device, VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr,
                                &renderer->graphics_pipeline) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create pipeline");
    goto destroy_pipeline_layout;
  }

  vkDestroyShaderModule(renderer->device.device, triangle_shader_frag_module,
                        nullptr);
  vkDestroyShaderModule(renderer->device.device, triangle_shader_vert_module,
                        nullptr);
  return true;
destroy_pipeline_layout:
  vkDestroyPipelineLayout(renderer->device.device, renderer->pipeline_layout,
                          nullptr);
destroy_frag_shader_module:
  vkDestroyShaderModule(renderer->device.device, triangle_shader_frag_module,
                        nullptr);
destroy_vert_shader_module:
  vkDestroyShaderModule(renderer->device.device, triangle_shader_vert_module,
                        nullptr);
err:
  return false;
}

static bool
vgltf_renderer_create_framebuffers(struct vgltf_renderer *renderer) {
  for (uint32_t i = 0; i < renderer->swapchain.swapchain_image_count; i++) {
    VkImageView attachments[] = {renderer->swapchain.swapchain_image_views[i],
                                 renderer->depth_image_view};
    int attachment_count = sizeof(attachments) / sizeof(attachments[0]);

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderer->render_pass,
        .attachmentCount = attachment_count,
        .pAttachments = attachments,
        .width = renderer->swapchain.swapchain_extent.width,
        .height = renderer->swapchain.swapchain_extent.height,
        .layers = 1};

    if (vkCreateFramebuffer(renderer->device.device, &framebuffer_info, nullptr,
                            &renderer->swapchain_framebuffers[i]) !=
        VK_SUCCESS) {
      VGLTF_LOG_ERR("Failed to create framebuffer");
      goto err;
    }
  }

  return true;
err:
  return false;
}

static bool
vgltf_renderer_create_command_pool(struct vgltf_renderer *renderer) {
  struct queue_family_indices queue_family_indices = {};
  if (!queue_family_indices_for_device(&queue_family_indices,
                                       renderer->device.physical_device,
                                       renderer->surface.surface)) {
    VGLTF_LOG_ERR("Couldn't fetch queue family indices");
    goto err;
  }

  VkCommandPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queue_family_indices.graphics_family};

  if (vkCreateCommandPool(renderer->device.device, &pool_info, nullptr,
                          &renderer->command_pool) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create command pool");
    goto err;
  }

  return true;
err:
  return false;
}

static VkCommandBuffer
begin_single_time_commands(struct vgltf_renderer *renderer) {
  VkCommandBufferAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandPool = renderer->command_pool,
      .commandBufferCount = 1};

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(renderer->device.device, &allocate_info,
                           &command_buffer);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkBeginCommandBuffer(command_buffer, &begin_info);

  return command_buffer;
}

static void end_single_time_commands(struct vgltf_renderer *renderer,
                                     VkCommandBuffer command_buffer) {
  vkEndCommandBuffer(command_buffer);
  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &command_buffer};

  vkQueueSubmit(renderer->device.graphics_queue, 1, &submit_info,
                VK_NULL_HANDLE);
  vkQueueWaitIdle(renderer->device.graphics_queue);
  vkFreeCommandBuffers(renderer->device.device, renderer->command_pool, 1,
                       &command_buffer);
}

static bool vgltf_renderer_copy_buffer(struct vgltf_renderer *renderer,
                                       VkBuffer src_buffer, VkBuffer dst_buffer,
                                       VkDeviceSize size) {
  VkCommandBuffer command_buffer = begin_single_time_commands(renderer);
  VkBufferCopy copy_region = {.size = size};
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
  end_single_time_commands(renderer, command_buffer);
  return true;
}

static void vgltf_renderer_create_image(
    struct vgltf_renderer *renderer, uint32_t width, uint32_t height,
    uint32_t mip_level_count, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    struct vgltf_renderer_allocated_image *image) {

  vmaCreateImage(
      renderer->device.allocator,
      &(const VkImageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType = VK_IMAGE_TYPE_2D,
          .extent = {width, height, 1},
          .mipLevels = mip_level_count,
          .arrayLayers = 1,
          .format = format,
          .tiling = tiling,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .usage = usage,
          .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
          .samples = VK_SAMPLE_COUNT_1_BIT,
      },
      &(const VmaAllocationCreateInfo){.usage = VMA_MEMORY_USAGE_GPU_ONLY,
                                       .requiredFlags = properties},
      &image->image, &image->allocation, &image->info);
}

static bool has_stencil_component(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static bool transition_image_layout(struct vgltf_renderer *renderer,
                                    VkImage image, VkFormat format,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout,
                                    uint32_t mip_level_count) {
  (void)format;
  VkCommandBuffer command_buffer = begin_single_time_commands(renderer);
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = mip_level_count,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .srcAccessMask = 0,
      .dstAccessMask = 0};

  if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (has_stencil_component(format)) {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VkPipelineStageFlags source_stage;
  VkPipelineStageFlags destination_stage;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    goto err;
  }

  vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  end_single_time_commands(renderer, command_buffer);
  return true;
err:
  return false;
}

void copy_buffer_to_image(struct vgltf_renderer *renderer, VkBuffer buffer,
                          VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer command_buffer = begin_single_time_commands(renderer);
  VkBufferImageCopy region = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}};

  vkCmdCopyBufferToImage(command_buffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  end_single_time_commands(renderer, command_buffer);
}

static bool
vgltf_renderer_create_depth_resources(struct vgltf_renderer *renderer) {
  VkFormat depth_format = find_depth_format(renderer);
  vgltf_renderer_create_image(
      renderer, renderer->swapchain.swapchain_extent.width,
      renderer->swapchain.swapchain_extent.height, 1, depth_format,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &renderer->depth_image);
  create_image_view(&renderer->device, renderer->depth_image.image,
                    depth_format, &renderer->depth_image_view,
                    VK_IMAGE_ASPECT_DEPTH_BIT, 1);

  transition_image_layout(renderer, renderer->depth_image.image, depth_format,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
  return true;
}

static bool
vgltf_renderer_create_buffer(struct vgltf_renderer *renderer, VkDeviceSize size,
                             VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             struct vgltf_renderer_allocated_buffer *buffer) {
  VkBufferCreateInfo buffer_info = {.sType =
                                        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                    .size = size,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
  VmaAllocationCreateInfo alloc_info = {
      .usage = VMA_MEMORY_USAGE_AUTO,
      .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      .preferredFlags = properties};

  if (vmaCreateBuffer(renderer->device.allocator, &buffer_info, &alloc_info,
                      &buffer->buffer, &buffer->allocation,
                      &buffer->info) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to create buffer");
    goto err;
  }

  return true;
err:
  return false;
}

static void generate_mipmaps(struct vgltf_renderer *renderer, VkImage image,
                             VkFormat image_format, int32_t texture_width,
                             int32_t texture_height, uint32_t mip_levels) {
  VkFormatProperties format_properties;
  vkGetPhysicalDeviceFormatProperties(renderer->device.physical_device,
                                      image_format, &format_properties);
  if (!(format_properties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    VGLTF_PANIC("Texture image format does not support linear blitting!");
  }

  VkCommandBuffer command_buffer = begin_single_time_commands(renderer);
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .image = image,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseArrayLayer = 0,
                           .layerCount = 1,
                           .levelCount = 1}};

  int32_t mip_width = texture_width;
  int32_t mip_height = texture_height;

  for (uint32_t i = 1; i < mip_levels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    VkImageBlit blit = {
        .srcOffsets = {{0, 0, 0}, {mip_width, mip_height, 1}},
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = i - 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .dstOffsets = {{0, 0, 0},
                       {mip_width > 1 ? mip_width / 2 : 1,
                        mip_height > 1 ? mip_height / 2 : 1, 1}},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = i,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
    };
    vkCmdBlitImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
    if (mip_width > 1)
      mip_width /= 2;
    if (mip_height > 1)
      mip_height /= 2;
  }
  barrier.subresourceRange.baseMipLevel = mip_levels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  end_single_time_commands(renderer, command_buffer);
}

static bool
vgltf_renderer_create_texture_image(struct vgltf_renderer *renderer) {
  struct vgltf_image image;
  if (!vgltf_image_load_from_file(&image, SV(TEXTURE_PATH))) {
    VGLTF_LOG_ERR("Couldn't load image from file");
    goto err;
  }
  renderer->mip_level_count =
      floor(log2(VGLTF_MAX(image.width, image.height))) + 1;

  VkDeviceSize image_size = image.width * image.height * 4;
  struct vgltf_renderer_allocated_buffer staging_buffer = {};
  if (!vgltf_renderer_create_buffer(renderer, image_size,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &staging_buffer)) {
    VGLTF_LOG_ERR("Couldn't create staging buffer");
    goto deinit_image;
  }

  void *data;
  vmaMapMemory(renderer->device.allocator, staging_buffer.allocation, &data);
  memcpy(data, image.data, image_size);
  vmaUnmapMemory(renderer->device.allocator, staging_buffer.allocation);

  vgltf_renderer_create_image(
      renderer, image.width, image.height, renderer->mip_level_count,
      VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &renderer->texture_image);

  transition_image_layout(renderer, renderer->texture_image.image,
                          VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          renderer->mip_level_count);
  copy_buffer_to_image(renderer, staging_buffer.buffer,
                       renderer->texture_image.image, image.width,
                       image.height);

  generate_mipmaps(renderer, renderer->texture_image.image,
                   VK_FORMAT_R8G8B8A8_SRGB, image.width, image.height,
                   renderer->mip_level_count);

  vmaDestroyBuffer(renderer->device.allocator, staging_buffer.buffer,
                   staging_buffer.allocation);
  vgltf_image_deinit(&image);
  return true;
deinit_image:
  vgltf_image_deinit(&image);
err:
  return false;
}

static bool
vgltf_renderer_create_texture_image_view(struct vgltf_renderer *renderer) {
  return create_image_view(
      &renderer->device, renderer->texture_image.image, VK_FORMAT_R8G8B8A8_SRGB,
      &renderer->texture_image_view, VK_IMAGE_ASPECT_COLOR_BIT,
      renderer->mip_level_count);
}

static bool
vgltf_renderer_create_texture_sampler(struct vgltf_renderer *renderer) {
  VkPhysicalDeviceProperties properties = {};
  vkGetPhysicalDeviceProperties(renderer->device.physical_device, &properties);

  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .mipLodBias = 0.f,
      .minLod = 0.f,
      .maxLod = renderer->mip_level_count};

  if (vkCreateSampler(renderer->device.device, &sampler_info, nullptr,
                      &renderer->texture_sampler) != VK_SUCCESS) {
    goto err;
  }

  return true;
err:
  return false;
}

static void get_file_data(void *ctx, const char *filename, const int is_mtl,
                          const char *obj_filename, char **data, size_t *len) {
  (void)ctx;
  (void)is_mtl;

  if (!filename) {
    VGLTF_LOG_ERR("Null filename");
    *data = NULL;
    *len = 0;
    return;
  }
  *data = vgltf_platform_read_file_to_string(obj_filename, len);
}

static bool load_model(struct vgltf_renderer *renderer) {
  tinyobj_attrib_t attrib;
  tinyobj_shape_t *shapes = nullptr;
  size_t shape_count;
  tinyobj_material_t *materials = nullptr;
  size_t material_count;

  if ((tinyobj_parse_obj(&attrib, &shapes, &shape_count, &materials,
                         &material_count, MODEL_PATH, get_file_data, nullptr,
                         TINYOBJ_FLAG_TRIANGULATE)) != TINYOBJ_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't load obj");
    return false;
  }

  for (size_t shape_index = 0; shape_index < shape_count; shape_index++) {
    tinyobj_shape_t *shape = &shapes[shape_index];
    unsigned int face_offset = shape->face_offset;
    for (size_t face_index = face_offset;
         face_index < face_offset + shape->length; face_index++) {
      float v[3][3];
      float t[3][2];

      tinyobj_vertex_index_t idx0 = attrib.faces[face_index * 3 + 0];
      tinyobj_vertex_index_t idx1 = attrib.faces[face_index * 3 + 1];
      tinyobj_vertex_index_t idx2 = attrib.faces[face_index * 3 + 2];

      for (int k = 0; k < 3; k++) {
        int f0 = idx0.v_idx;
        int f1 = idx1.v_idx;
        int f2 = idx2.v_idx;

        v[0][k] = attrib.vertices[3 * (size_t)f0 + k];
        v[1][k] = attrib.vertices[3 * (size_t)f1 + k];
        v[2][k] = attrib.vertices[3 * (size_t)f2 + k];
      }

      for (int k = 0; k < 2; k++) {
        int t0 = idx0.vt_idx;
        int t1 = idx1.vt_idx;
        int t2 = idx2.vt_idx;

        t[0][k] = attrib.texcoords[2 * (size_t)t0 + k];
        t[1][k] = attrib.texcoords[2 * (size_t)t1 + k];
        t[2][k] = attrib.texcoords[2 * (size_t)t2 + k];
      }

      for (int k = 0; k < 3; k++) {
        renderer->vertices[renderer->vertex_count++] = (struct vgltf_vertex){
            .position = {v[k][0], v[k][1], v[k][2]},
            .texture_coordinates = {t[k][0], 1.f - t[k][1]},
            .color = {1.f, 1.f, 1.f}};
        renderer->indices[renderer->index_count++] = renderer->index_count;
      }
    }
    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, shape_count);
    tinyobj_materials_free(materials, material_count);
  }
  return true;
}

static bool
vgltf_renderer_create_vertex_buffer(struct vgltf_renderer *renderer) {
  VkDeviceSize buffer_size =
      renderer->vertex_count * sizeof(struct vgltf_vertex);

  struct vgltf_renderer_allocated_buffer staging_buffer = {};
  if (!vgltf_renderer_create_buffer(renderer, buffer_size,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &staging_buffer)) {
    VGLTF_LOG_ERR("Failed to create transfer buffer");
    goto err;
  }

  void *data;
  vmaMapMemory(renderer->device.allocator, staging_buffer.allocation, &data);
  memcpy(data, renderer->vertices,
         renderer->vertex_count * sizeof(struct vgltf_vertex));
  vmaUnmapMemory(renderer->device.allocator, staging_buffer.allocation);

  if (!vgltf_renderer_create_buffer(
          renderer, buffer_size,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &renderer->vertex_buffer)) {
    VGLTF_LOG_ERR("Failed to create vertex buffer");
    goto destroy_staging_buffer;
  }

  vgltf_renderer_copy_buffer(renderer, staging_buffer.buffer,
                             renderer->vertex_buffer.buffer, buffer_size);
  vmaDestroyBuffer(renderer->device.allocator, staging_buffer.buffer,
                   staging_buffer.allocation);
  return true;
destroy_staging_buffer:
  vmaDestroyBuffer(renderer->device.allocator, staging_buffer.buffer,
                   staging_buffer.allocation);
err:
  return false;
}

static bool
vgltf_renderer_create_index_buffer(struct vgltf_renderer *renderer) {
  VkDeviceSize buffer_size = renderer->index_count * sizeof(uint16_t);
  struct vgltf_renderer_allocated_buffer staging_buffer = {};
  if (!vgltf_renderer_create_buffer(renderer, buffer_size,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &staging_buffer)) {
    VGLTF_LOG_ERR("Failed to create transfer buffer");
    goto err;
  }

  void *data;
  vmaMapMemory(renderer->device.allocator, staging_buffer.allocation, &data);
  memcpy(data, renderer->indices, renderer->index_count * sizeof(uint16_t));
  vmaUnmapMemory(renderer->device.allocator, staging_buffer.allocation);

  if (!vgltf_renderer_create_buffer(
          renderer, buffer_size,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &renderer->index_buffer)) {
    VGLTF_LOG_ERR("Failed to create index buffer");
    goto destroy_staging_buffer;
  }
  vgltf_renderer_copy_buffer(renderer, staging_buffer.buffer,
                             renderer->index_buffer.buffer, buffer_size);
  vmaDestroyBuffer(renderer->device.allocator, staging_buffer.buffer,
                   staging_buffer.allocation);
  return true;

destroy_staging_buffer:
  vmaDestroyBuffer(renderer->device.allocator, staging_buffer.buffer,
                   staging_buffer.allocation);
err:
  return false;
}

static bool
vgltf_renderer_create_command_buffer(struct vgltf_renderer *renderer) {
  VkCommandBufferAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = renderer->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT};

  if (vkAllocateCommandBuffers(renderer->device.device, &allocate_info,
                               renderer->command_buffer) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't allocate command buffers");
    goto err;
  }

  return true;
err:
  return false;
}

static bool
vgltf_renderer_create_sync_objects(struct vgltf_renderer *renderer) {
  VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                  .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  int frame_in_flight_index = 0;
  for (; frame_in_flight_index < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT;
       frame_in_flight_index++) {
    if (vkCreateSemaphore(
            renderer->device.device, &semaphore_info, nullptr,
            &renderer->image_available_semaphores[frame_in_flight_index]) !=
            VK_SUCCESS ||
        vkCreateSemaphore(
            renderer->device.device, &semaphore_info, nullptr,
            &renderer->render_finished_semaphores[frame_in_flight_index]) !=
            VK_SUCCESS ||
        vkCreateFence(renderer->device.device, &fence_info, nullptr,
                      &renderer->in_flight_fences[frame_in_flight_index]) !=
            VK_SUCCESS) {
      VGLTF_LOG_ERR("Couldn't create sync objects");
      goto err;
    }
  }

  return true;
err:
  for (int frame_in_flight_to_delete_index = 0;
       frame_in_flight_to_delete_index < frame_in_flight_index;
       frame_in_flight_to_delete_index++) {
    vkDestroyFence(renderer->device.device,
                   renderer->in_flight_fences[frame_in_flight_index], nullptr);
    vkDestroySemaphore(
        renderer->device.device,
        renderer->render_finished_semaphores[frame_in_flight_index], nullptr);
    vkDestroySemaphore(
        renderer->device.device,
        renderer->image_available_semaphores[frame_in_flight_index], nullptr);
  }
  return false;
}

static bool vgltf_vk_swapchain_init(struct vgltf_vk_swapchain *swapchain,
                                    struct vgltf_vk_device *device,
                                    struct vgltf_vk_surface *surface,
                                    struct vgltf_window_size *window_size) {
  if (!create_swapchain(swapchain, device, surface, window_size)) {
    VGLTF_LOG_ERR("Couldn't create swapchain");
    goto err;
  }

  if (!create_swapchain_image_views(swapchain, device)) {
    VGLTF_LOG_ERR("Couldn't create image views");
    goto destroy_swapchain;
  }

  return true;
destroy_swapchain:
  vkDestroySwapchainKHR(device->device, swapchain->swapchain, nullptr);
err:
  return false;
}

static void vgltf_vk_swapchain_deinit(struct vgltf_vk_swapchain *swapchain,
                                      struct vgltf_vk_device *device) {
  for (uint32_t swapchain_image_view_index = 0;
       swapchain_image_view_index < swapchain->swapchain_image_count;
       swapchain_image_view_index++) {
    vkDestroyImageView(
        device->device,
        swapchain->swapchain_image_views[swapchain_image_view_index], nullptr);
  }
  vkDestroySwapchainKHR(device->device, swapchain->swapchain, nullptr);
}

static void vgltf_renderer_cleanup_swapchain(struct vgltf_renderer *renderer) {
  vkDestroyImageView(renderer->device.device, renderer->depth_image_view,
                     nullptr);
  vmaDestroyImage(renderer->device.allocator, renderer->depth_image.image,
                  renderer->depth_image.allocation);

  for (uint32_t framebuffer_index = 0;
       framebuffer_index < renderer->swapchain.swapchain_image_count;
       framebuffer_index++) {
    vkDestroyFramebuffer(renderer->device.device,
                         renderer->swapchain_framebuffers[framebuffer_index],
                         nullptr);
  }

  vgltf_vk_swapchain_deinit(&renderer->swapchain, &renderer->device);
}

static bool vgltf_renderer_recreate_swapchain(struct vgltf_renderer *renderer) {
  vkDeviceWaitIdle(renderer->device.device);
  vgltf_renderer_cleanup_swapchain(renderer);

  // TODO add error handling
  create_swapchain(&renderer->swapchain, &renderer->device, &renderer->surface,
                   &renderer->window_size);
  create_swapchain_image_views(&renderer->swapchain, &renderer->device);
  vgltf_renderer_create_depth_resources(renderer);
  vgltf_renderer_create_framebuffers(renderer);
  return true;
}

static void vgltf_renderer_triangle_pass(struct vgltf_renderer *renderer,
                                         uint32_t swapchain_image_index) {
  VkRenderPassBeginInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderer->render_pass,
      .framebuffer = renderer->swapchain_framebuffers[swapchain_image_index],
      .renderArea = {.offset = {},
                     .extent = renderer->swapchain.swapchain_extent},
      .clearValueCount = 2,
      .pClearValues =
          (const VkClearValue[]){{.color = {.float32 = {0.f, 0.f, 0.f, 1.f}}},
                                 {.depthStencil = {1.0f, 0}}},

  };

  vkCmdBeginRenderPass(renderer->command_buffer[renderer->current_frame],
                       &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(renderer->command_buffer[renderer->current_frame],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->graphics_pipeline);
  VkViewport viewport = {
      .x = 0.f,
      .y = 0.f,
      .width = (float)renderer->swapchain.swapchain_extent.width,
      .height = (float)renderer->swapchain.swapchain_extent.height,
      .minDepth = 0.f,
      .maxDepth = 1.f};
  vkCmdSetViewport(renderer->command_buffer[renderer->current_frame], 0, 1,
                   &viewport);
  VkRect2D scissor = {.offset = {},
                      .extent = renderer->swapchain.swapchain_extent};
  vkCmdSetScissor(renderer->command_buffer[renderer->current_frame], 0, 1,
                  &scissor);

  VkBuffer vertex_buffers[] = {renderer->vertex_buffer.buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(renderer->command_buffer[renderer->current_frame], 0,
                         1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(renderer->command_buffer[renderer->current_frame],
                       renderer->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(
      renderer->command_buffer[renderer->current_frame],
      VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipeline_layout, 0, 1,
      &renderer->descriptor_sets[renderer->current_frame], 0, nullptr);
  vkCmdDrawIndexed(renderer->command_buffer[renderer->current_frame],
                   renderer->index_count, 1, 0, 0, 0);

  vkCmdEndRenderPass(renderer->command_buffer[renderer->current_frame]);
}

static void update_uniform_buffer(struct vgltf_renderer *renderer,
                                  uint32_t current_frame) {
  static long start_time_nanoseconds = 0;
  if (start_time_nanoseconds == 0) {
    if (!vgltf_platform_get_current_time_nanoseconds(&start_time_nanoseconds)) {
      VGLTF_LOG_ERR("Couldn't get current time");
    }
  }

  long current_time_nanoseconds = 0;
  if (!vgltf_platform_get_current_time_nanoseconds(&current_time_nanoseconds)) {
    VGLTF_LOG_ERR("Couldn't get current time");
  }

  long elapsed_time_nanoseconds =
      current_time_nanoseconds - start_time_nanoseconds;
  float elapsed_time_seconds = elapsed_time_nanoseconds / 1e9f;
  VGLTF_LOG_INFO("Elapsed time: %f", elapsed_time_seconds);

  vgltf_mat4 model_matrix;
  vgltf_mat4_rotate(model_matrix, (vgltf_mat4)VGLTF_MAT4_IDENTITY,
                    elapsed_time_seconds * VGLTF_MATHS_DEG_TO_RAD(90.0f),
                    (vgltf_vec3){0.f, 0.f, 1.f});

  vgltf_mat4 view_matrix;
  vgltf_mat4_look_at(view_matrix, (vgltf_vec3){2.f, 2.f, 2.f},
                     (vgltf_vec3){0.f, 0.f, 0.f}, (vgltf_vec3){0.f, 0.f, 1.f});

  vgltf_mat4 projection_matrix;
  vgltf_mat4_perspective(projection_matrix, VGLTF_MATHS_DEG_TO_RAD(45.f),
                         (float)renderer->swapchain.swapchain_extent.width /
                             (float)renderer->swapchain.swapchain_extent.height,
                         0.1f, 10.f);
  projection_matrix[1 * 4 + 1] *= -1;

  struct vgltf_renderer_uniform_buffer_object ubo = {};
  memcpy(ubo.model, model_matrix, sizeof(vgltf_mat4));
  memcpy(ubo.view, view_matrix, sizeof(vgltf_mat4));
  memcpy(ubo.projection, projection_matrix, sizeof(vgltf_mat4));
  memcpy(renderer->mapped_uniform_buffers[current_frame], &ubo, sizeof(ubo));
}

bool vgltf_renderer_render_frame(struct vgltf_renderer *renderer) {
  vkWaitForFences(renderer->device.device, 1,
                  &renderer->in_flight_fences[renderer->current_frame], VK_TRUE,
                  UINT64_MAX);

  uint32_t image_index;
  VkResult acquire_swapchain_image_result = vkAcquireNextImageKHR(
      renderer->device.device, renderer->swapchain.swapchain, UINT64_MAX,
      renderer->image_available_semaphores[renderer->current_frame],
      VK_NULL_HANDLE, &image_index);
  if (acquire_swapchain_image_result == VK_ERROR_OUT_OF_DATE_KHR ||
      acquire_swapchain_image_result == VK_SUBOPTIMAL_KHR ||
      renderer->framebuffer_resized) {
    renderer->framebuffer_resized = false;
    vgltf_renderer_recreate_swapchain(renderer);
    return true;
  } else if (acquire_swapchain_image_result != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to acquire a swapchain image");
    goto err;
  }

  vkResetFences(renderer->device.device, 1,
                &renderer->in_flight_fences[renderer->current_frame]);

  vkResetCommandBuffer(renderer->command_buffer[renderer->current_frame], 0);
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  if (vkBeginCommandBuffer(renderer->command_buffer[renderer->current_frame],
                           &begin_info) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to begin recording command buffer");
    goto err;
  }

  vgltf_renderer_triangle_pass(renderer, image_index);

  if (vkEndCommandBuffer(renderer->command_buffer[renderer->current_frame]) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to record command buffer");
    goto err;
  }

  update_uniform_buffer(renderer, renderer->current_frame);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
  };

  VkSemaphore wait_semaphores[] = {
      renderer->image_available_semaphores[renderer->current_frame]};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers =
      &renderer->command_buffer[renderer->current_frame];

  VkSemaphore signal_semaphores[] = {
      renderer->render_finished_semaphores[renderer->current_frame]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;
  if (vkQueueSubmit(renderer->device.graphics_queue, 1, &submit_info,
                    renderer->in_flight_fences[renderer->current_frame]) !=
      VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to submit draw command buffer");
    goto err;
  }

  VkPresentInfoKHR present_info = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = signal_semaphores};

  VkSwapchainKHR swapchains[] = {renderer->swapchain.swapchain};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &image_index;
  VkResult result =
      vkQueuePresentKHR(renderer->device.present_queue, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    vgltf_renderer_recreate_swapchain(renderer);
  } else if (acquire_swapchain_image_result != VK_SUCCESS) {
    VGLTF_LOG_ERR("Failed to acquire a swapchain image");
    goto err;
  }
  renderer->current_frame =
      (renderer->current_frame + 1) % VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT;
  return true;
err:
  return false;
}
static bool
vgltf_renderer_create_uniform_buffers(struct vgltf_renderer *renderer) {
  VkDeviceSize buffer_size =
      sizeof(struct vgltf_renderer_uniform_buffer_object);

  for (int i = 0; i < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT; i++) {
    vgltf_renderer_create_buffer(renderer, buffer_size,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 &renderer->uniform_buffers[i]);
    vmaMapMemory(renderer->device.allocator,
                 renderer->uniform_buffers[i].allocation,
                 &renderer->mapped_uniform_buffers[i]);
  }

  return true;
}

static bool
vgltf_renderer_create_descriptor_pool(struct vgltf_renderer *renderer) {
  VkDescriptorPoolSize pool_sizes[] = {
      (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             .descriptorCount =
                                 VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT},
      (VkDescriptorPoolSize){.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             .descriptorCount =
                                 VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT}};
  int pool_size_count = sizeof(pool_sizes) / sizeof(pool_sizes[0]);

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = pool_size_count,
      .pPoolSizes = pool_sizes,
      .maxSets = VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT};

  if (vkCreateDescriptorPool(renderer->device.device, &pool_info, nullptr,
                             &renderer->descriptor_pool) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create uniform descriptor pool");
    goto err;
  }

  return true;
err:
  return false;
}
static bool
vgltf_renderer_create_descriptor_sets(struct vgltf_renderer *renderer) {
  VkDescriptorSetLayout layouts[VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT] = {};
  for (int layout_index = 0;
       layout_index < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT;
       layout_index++) {
    layouts[layout_index] = renderer->descriptor_set_layout;
  }

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = renderer->descriptor_pool,
      .descriptorSetCount = VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT,
      .pSetLayouts = layouts};

  if (vkAllocateDescriptorSets(renderer->device.device, &alloc_info,
                               renderer->descriptor_sets) != VK_SUCCESS) {
    VGLTF_LOG_ERR("Couldn't create descriptor sets");
    goto err;
  }

  for (int set_index = 0; set_index < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT;
       set_index++) {
    VkDescriptorBufferInfo buffer_info = {
        .buffer = renderer->uniform_buffers[set_index].buffer,
        .offset = 0,
        .range = sizeof(struct vgltf_renderer_uniform_buffer_object)};

    VkDescriptorImageInfo image_info = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = renderer->texture_image_view,
        .sampler = renderer->texture_sampler,
    };

    VkWriteDescriptorSet descriptor_writes[] = {
        (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                               .dstSet = renderer->descriptor_sets[set_index],
                               .dstBinding = 0,
                               .dstArrayElement = 0,
                               .descriptorType =
                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               .descriptorCount = 1,
                               .pBufferInfo = &buffer_info},

        (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                               .dstSet = renderer->descriptor_sets[set_index],
                               .dstBinding = 1,
                               .dstArrayElement = 0,
                               .descriptorType =
                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               .descriptorCount = 1,
                               .pImageInfo = &image_info}};
    int descriptor_write_count =
        sizeof(descriptor_writes) / sizeof(descriptor_writes[0]);

    vkUpdateDescriptorSets(renderer->device.device, descriptor_write_count,
                           descriptor_writes, 0, nullptr);
  }

  return true;
err:
  return false;
}

static bool vgltf_vk_device_init(struct vgltf_vk_device *device,
                                 struct vgltf_vk_instance *instance,
                                 struct vgltf_vk_surface *surface) {
  if (!pick_physical_device(&device->physical_device, instance,
                            surface->surface)) {
    VGLTF_LOG_ERR("Couldn't pick physical device");
    goto err;
  }

  if (!create_logical_device(&device->device, &device->graphics_queue,
                             &device->present_queue, device->physical_device,
                             surface->surface)) {
    VGLTF_LOG_ERR("Couldn't pick logical device");
    goto err;
  }

  if (!create_allocator(&device->allocator, device, instance)) {
    VGLTF_LOG_ERR("Couldn't create allocator");
    goto destroy_logical_device;
  }

  return true;
destroy_logical_device:
  vkDestroyDevice(device->device, nullptr);
err:
  return false;
}

static void vgltf_vk_device_deinit(struct vgltf_vk_device *device) {
  vmaDestroyAllocator(device->allocator);
  vkDestroyDevice(device->device, nullptr);
}

bool vgltf_renderer_init(struct vgltf_renderer *renderer,
                         struct vgltf_platform *platform) {
  if (!vgltf_vk_instance_init(&renderer->instance, platform)) {
    VGLTF_LOG_ERR("instance creation failed");
    goto err;
  }
  vgltf_renderer_setup_debug_messenger(renderer);
  if (!vgltf_vk_surface_init(&renderer->surface, &renderer->instance,
                             platform)) {
    goto destroy_instance;
  }

  if (!vgltf_vk_device_init(&renderer->device, &renderer->instance,
                            &renderer->surface)) {
    VGLTF_LOG_ERR("Device creation failed");
    goto destroy_surface;
  }

  struct vgltf_window_size window_size = {800, 600};
  if (!vgltf_platform_get_window_size(platform, &window_size)) {
    VGLTF_LOG_ERR("Couldn't get window size");
    goto destroy_device;
  }
  renderer->window_size = window_size;

  if (!vgltf_vk_swapchain_init(&renderer->swapchain, &renderer->device,
                               &renderer->surface, &renderer->window_size)) {
    VGLTF_LOG_ERR("Couldn't create swapchain");
    goto destroy_device;
  }

  if (!vgltf_renderer_create_render_pass(renderer)) {
    VGLTF_LOG_ERR("Couldn't create render pass");
    goto destroy_swapchain;
  }

  if (!vgltf_renderer_create_descriptor_set_layout(renderer)) {
    VGLTF_LOG_ERR("Couldn't create descriptor set layout");
    goto destroy_render_pass;
  }

  if (!vgltf_renderer_create_graphics_pipeline(renderer)) {
    VGLTF_LOG_ERR("Couldn't create graphics pipeline");
    goto destroy_descriptor_set_layout;
  }

  if (!vgltf_renderer_create_command_pool(renderer)) {
    VGLTF_LOG_ERR("Couldn't create command pool");
    goto destroy_graphics_pipeline;
  }

  if (!vgltf_renderer_create_depth_resources(renderer)) {
    VGLTF_LOG_ERR("Couldn't create depth resources");
    goto destroy_command_pool;
  }

  if (!vgltf_renderer_create_framebuffers(renderer)) {
    VGLTF_LOG_ERR("Couldn't create framebuffers");
    goto destroy_depth_resources;
  }

  if (!vgltf_renderer_create_texture_image(renderer)) {
    VGLTF_LOG_ERR("Couldn't create texture image");
    goto destroy_frame_buffers;
  }

  if (!vgltf_renderer_create_texture_image_view(renderer)) {
    VGLTF_LOG_ERR("Couldn't create texture image view");
    goto destroy_texture_image;
  }

  if (!vgltf_renderer_create_texture_sampler(renderer)) {
    VGLTF_LOG_ERR("Couldn't create texture sampler");
    goto destroy_texture_image_view;
  }

  if (!load_model(renderer)) {
    VGLTF_LOG_ERR("Couldn't load model");
    goto destroy_texture_sampler;
  }

  if (!vgltf_renderer_create_vertex_buffer(renderer)) {
    VGLTF_LOG_ERR("Couldn't create vertex buffer");
    goto destroy_model;
  }

  if (!vgltf_renderer_create_index_buffer(renderer)) {
    VGLTF_LOG_ERR("Couldn't create index buffer");
    goto destroy_vertex_buffer;
  }

  if (!vgltf_renderer_create_uniform_buffers(renderer)) {
    VGLTF_LOG_ERR("Couldn't create uniform buffers");
    goto destroy_index_buffer;
  }

  if (!vgltf_renderer_create_descriptor_pool(renderer)) {
    VGLTF_LOG_ERR("Couldn't create descriptor pool");
    goto destroy_uniform_buffers;
  }

  if (!vgltf_renderer_create_descriptor_sets(renderer)) {
    VGLTF_LOG_ERR("Couldn't create descriptor sets");
    goto destroy_descriptor_pool;
  }

  if (!vgltf_renderer_create_command_buffer(renderer)) {
    VGLTF_LOG_ERR("Couldn't create command buffer");
    goto destroy_descriptor_pool;
  }

  if (!vgltf_renderer_create_sync_objects(renderer)) {
    VGLTF_LOG_ERR("Couldn't create sync objects");
    goto destroy_descriptor_pool;
  }

  return true;

destroy_descriptor_pool:
  vkDestroyDescriptorPool(renderer->device.device, renderer->descriptor_pool,
                          nullptr);
destroy_uniform_buffers:
  for (int i = 0; i < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT; i++) {
    vmaDestroyBuffer(renderer->device.allocator,
                     renderer->uniform_buffers[i].buffer,
                     renderer->uniform_buffers[i].allocation);
  }
destroy_index_buffer:
  vmaDestroyBuffer(renderer->device.allocator, renderer->index_buffer.buffer,
                   renderer->index_buffer.allocation);
destroy_vertex_buffer:
  vmaDestroyBuffer(renderer->device.allocator, renderer->vertex_buffer.buffer,
                   renderer->vertex_buffer.allocation);
destroy_model:
  // TODO
destroy_texture_sampler:
  vkDestroySampler(renderer->device.device, renderer->texture_sampler, nullptr);
destroy_texture_image_view:
  vkDestroyImageView(renderer->device.device, renderer->texture_image_view,
                     nullptr);
destroy_texture_image:
  vmaDestroyImage(renderer->device.allocator, renderer->texture_image.image,
                  renderer->texture_image.allocation);
destroy_depth_resources:
  vkDestroyImageView(renderer->device.device, renderer->depth_image_view,
                     nullptr);
  vmaDestroyImage(renderer->device.allocator, renderer->depth_image.image,
                  renderer->depth_image.allocation);
destroy_command_pool:
  vkDestroyCommandPool(renderer->device.device, renderer->command_pool,
                       nullptr);
destroy_frame_buffers:
  for (uint32_t swapchain_framebuffer_index = 0;
       swapchain_framebuffer_index < renderer->swapchain.swapchain_image_count;
       swapchain_framebuffer_index++) {
    vkDestroyFramebuffer(
        renderer->device.device,
        renderer->swapchain_framebuffers[swapchain_framebuffer_index], nullptr);
  }
destroy_graphics_pipeline:
  vkDestroyPipeline(renderer->device.device, renderer->graphics_pipeline,
                    nullptr);
  vkDestroyPipelineLayout(renderer->device.device, renderer->pipeline_layout,
                          nullptr);
destroy_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device.device,
                               renderer->descriptor_set_layout, nullptr);
destroy_render_pass:
  vkDestroyRenderPass(renderer->device.device, renderer->render_pass, nullptr);
destroy_swapchain:
  vgltf_vk_swapchain_deinit(&renderer->swapchain, &renderer->device);
destroy_device:
  vgltf_vk_device_deinit(&renderer->device);
destroy_surface:
  vgltf_vk_surface_deinit(&renderer->surface, &renderer->instance);
destroy_instance:
  if (enable_validation_layers) {
    destroy_debug_utils_messenger_ext(renderer->instance.instance,
                                      renderer->debug_messenger, nullptr);
  }
  vgltf_vk_instance_deinit(&renderer->instance);
err:
  return false;
}
void vgltf_renderer_deinit(struct vgltf_renderer *renderer) {
  vkDeviceWaitIdle(renderer->device.device);
  vgltf_renderer_cleanup_swapchain(renderer);
  for (int i = 0; i < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT; i++) {
    vmaUnmapMemory(renderer->device.allocator,
                   renderer->uniform_buffers[i].allocation);
    vmaDestroyBuffer(renderer->device.allocator,
                     renderer->uniform_buffers[i].buffer,
                     renderer->uniform_buffers[i].allocation);
  }
  vmaDestroyBuffer(renderer->device.allocator, renderer->index_buffer.buffer,
                   renderer->index_buffer.allocation);
  vmaDestroyBuffer(renderer->device.allocator, renderer->vertex_buffer.buffer,
                   renderer->vertex_buffer.allocation);
  vkDestroySampler(renderer->device.device, renderer->texture_sampler, nullptr);
  vkDestroyImageView(renderer->device.device, renderer->texture_image_view,
                     nullptr);
  vmaDestroyImage(renderer->device.allocator, renderer->texture_image.image,
                  renderer->texture_image.allocation);
  vkDestroyPipeline(renderer->device.device, renderer->graphics_pipeline,
                    nullptr);
  vkDestroyPipelineLayout(renderer->device.device, renderer->pipeline_layout,
                          nullptr);
  vkDestroyDescriptorPool(renderer->device.device, renderer->descriptor_pool,
                          nullptr);
  vkDestroyDescriptorSetLayout(renderer->device.device,
                               renderer->descriptor_set_layout, nullptr);
  vkDestroyRenderPass(renderer->device.device, renderer->render_pass, nullptr);
  for (int i = 0; i < VGLTF_RENDERER_MAX_FRAME_IN_FLIGHT_COUNT; i++) {
    vkDestroySemaphore(renderer->device.device,
                       renderer->image_available_semaphores[i], nullptr);
    vkDestroySemaphore(renderer->device.device,
                       renderer->render_finished_semaphores[i], nullptr);
    vkDestroyFence(renderer->device.device, renderer->in_flight_fences[i],
                   nullptr);
  }
  vkDestroyCommandPool(renderer->device.device, renderer->command_pool,
                       nullptr);
  vgltf_vk_device_deinit(&renderer->device);
  vgltf_vk_surface_deinit(&renderer->surface, &renderer->instance);
  if (enable_validation_layers) {
    destroy_debug_utils_messenger_ext(renderer->instance.instance,
                                      renderer->debug_messenger, nullptr);
  }
  vgltf_vk_instance_deinit(&renderer->instance);
}
void vgltf_renderer_on_window_resized(struct vgltf_renderer *renderer,
                                      struct vgltf_window_size size) {
  if (size.width > 0 && size.height > 0 &&
      size.width != renderer->window_size.width &&
      size.height != renderer->window_size.height) {
    renderer->window_size = size;
    renderer->framebuffer_resized = true;
  }
}
