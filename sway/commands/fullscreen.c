#include <strings.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "util.h"
#include "sway/output.h"

struct cmd_results *cmd_fullscreen_present(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	struct sway_workspace *ws = config->handler_context.workspace;
	if (!con->view){
		return cmd_results_new(CMD_FAILURE,
			"Can't run this command if there's no view connected to the container.");
	}
	if (!ws->output)
	{
		return cmd_results_new(CMD_FAILURE,
			"Can't run this command if no output is connected to the workspace.");
	}

	int natural_width = con->view->natural_width;
	int natural_height = con->view->natural_height;
	int output_width = ws->output->width;
	int output_height = ws->output->height;


	float scalefactor_x = output_width / (float)natural_width;
	float scalefactor_y = output_height / (float)natural_height;

	// Always pick the lower scale factor
	float scalefactor = (scalefactor_x < scalefactor_y ? scalefactor_x : scalefactor_y);
	char scalefactor_str[16];
	char *output_name = ws->output->wlr_output->name;
	char* scale_argv[] = {
		output_name,
		"scale",
		scalefactor_str,
	};
	sprintf(scalefactor_str, "%f", scalefactor);

	// Send command to change scale of output
	struct cmd_results *result = cmd_output(3, scale_argv);
	if (result)
		printf("What?");

	// Send command to toggle fullscreen mode for the active container
	cmd_fullscreen(0, argv);

	// Send command to center container output on the screen
	container_set_floating(con, true);

	// TODO: Store old scale and new scale somewhere (in the container?)
	// TODO: Implement an is_valid method for old scale value
	// TODO: Restore old scale value if it's valid after container dies

	return cmd_results_new(CMD_SUCCESS, NULL);
}

// fullscreen [enable|disable|toggle] [global]
struct cmd_results *cmd_fullscreen(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "fullscreen", EXPECTED_AT_MOST, 2))) {
		return error;
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_FAILURE,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_node *node = config->handler_context.node;
	struct sway_container *container = config->handler_context.container;
	struct sway_workspace *workspace = config->handler_context.workspace;
	if (node->type == N_WORKSPACE && workspace->tiling->length == 0) {
		return cmd_results_new(CMD_FAILURE,
				"Can't fullscreen an empty workspace");
	}

	// If in the scratchpad, operate on the highest container
	if (container && !container->workspace) {
		while (container->parent) {
			container = container->parent;
		}
	}

	bool is_fullscreen = false;
	for (struct sway_container *curr = container; curr; curr = curr->parent) {
		if (curr->fullscreen_mode != FULLSCREEN_NONE) {
			container = curr;
			is_fullscreen = true;
			break;
		}
	}

	bool global = false;
	bool enable = !is_fullscreen;

	if (argc >= 1) {
		if (strcasecmp(argv[0], "global") == 0) {
			global = true;
		} else {
			enable = parse_boolean(argv[0], is_fullscreen);
		}
	}

	if (argc >= 2) {
		global = strcasecmp(argv[1], "global") == 0;
	}

	if (enable && node->type == N_WORKSPACE) {
		// Wrap the workspace's children in a container so we can fullscreen it
		container = workspace_wrap_children(workspace);
		workspace->layout = L_HORIZ;
		seat_set_focus_container(config->handler_context.seat, container);
	}

	enum sway_fullscreen_mode mode = FULLSCREEN_NONE;
	if (enable) {
		mode = global ? FULLSCREEN_GLOBAL : FULLSCREEN_WORKSPACE;
	}

	container_set_fullscreen(container, mode);
	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
