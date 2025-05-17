#include "engine.h"
#include "log.h"
#include "platform.h"
#include "src/str.h"

struct program_arguments {
  struct vgltf_string_view model_path;
};

static void print_usage(void) { VGLTF_LOG_INFO("Usage: vgltf <model_name>"); }

static bool parse_program_arguments(struct program_arguments *arguments,
                                    int argc, char **argv) {
  if (argc != 2) {
    print_usage();
    goto err;
  }

  int model_path_length = strlen(argv[1]);
  char *model_path = argv[1];
  arguments->model_path = (struct vgltf_string_view){
      .data = model_path, .length = model_path_length};

  VGLTF_LOG_DBG("Model path: %.*s", model_path_length, model_path);

  return true;
err:
  return false;
}

int main(int argc, char **argv) {

  struct program_arguments args = {};
  if (!parse_program_arguments(&args, argc, argv)) {
    goto err;
  }

  struct vgltf_platform platform = {};
  if (!vgltf_platform_init(&platform)) {
    VGLTF_LOG_ERR("Platform initialization failed");
    goto err;
  }

  struct vgltf_engine engine = {};
  if (!vgltf_engine_init(&engine, &platform, args.model_path)) {
    VGLTF_LOG_ERR("Couldn't initialize the engine");
    goto deinit_platform;
  }

  VGLTF_LOG_INFO("Starting main loop");
  while (true) {
    struct vgltf_event event;
    while (vgltf_platform_poll_event(&platform, &event)) {
      if (event.type == VGLTF_EVENT_QUIT ||
          (event.type == VGLTF_EVENT_KEY_DOWN &&
           event.key.key == VGLTF_KEY_ESCAPE)) {
        goto out_main_loop;
      }
    }

    vgltf_engine_run_frame(&engine);
  }
out_main_loop:
  VGLTF_LOG_INFO("Exiting main loop");
  vgltf_engine_deinit(&engine);
  vgltf_platform_deinit(&platform);
  return 0;
deinit_platform:
  vgltf_platform_deinit(&platform);
err:
  return -1;
}
