/**
 * \file cgrapher4gcc.cpp
 * \author Laurent Georget
 * \date 2015-03-03
 * \brief Entry point of the plugin
 */
#include <iostream>
#include <fstream>
#include <string>

#include <gcc-plugin.h>

#include <system.h>
#include <coretypes.h>
#include <tree.h>
#include <tree-pass.h>
#include <intl.h>

#include <tm.h>
#include <diagnostic.h>

#include <function.h>
#include <tree-iterator.h>
#include <gimple.h>

#include <dumpfile.h>

#include "evaluator.h"
#include "debug.h"

DebugMe DebugMe::INSTANCE;

extern "C" {
	/**
	 * \brief Must-be-defined value for license compatibility
	 */
	int plugin_is_GPL_compatible;
	static bool evaluate_paths_gate();
	static unsigned int evaluate_paths();

	/**
	 * \brief Basic information about the plugin
	 */
	static struct plugin_info myplugin_info =
	{
		"1.0", // version
		"KayrebtPathExaminer: a GCC plugin to evaluate the feasibility of execution paths", //help
	};

	/**
	 * \brief Plugin version information
	 */
	static struct plugin_gcc_version myplugin_version =
	{
		"4.8", //basever
		"2016-02-22", // datestamp
		"alpha", // devphase
		"", // revision
		"", // configuration arguments
	};

	/**
	 * \brief Definition of the new pass added by the plugin
	 */
	struct gimple_opt_pass path_evaluation_pass =
	{
		{
			GIMPLE_PASS, /* type */
			"path feasibility", /* name */
			OPTGROUP_NONE, /* optinfo_flags */
			evaluate_paths_gate, /* gate */
			evaluate_paths, /* execute */
			NULL, /* sub */
			NULL, /* next */
			0, /* static_pass_number */
			TV_NONE, /* tv_id */
			PROP_cfg | PROP_ssa | PROP_loops, /* properties_required */
			0, /* properties_provided */
			0, /* properties_destroyed */
			0, /* todo_flags_start */
			0, /* todo_flags_finish */
		}
	};

}

static struct plugin_name_args* functions;

/**
 * \brief Plugin entry point
 * \param plugin_args the command line options passed to the plugin
 * \param version the plugin version information
 * \return 0 if everything went well, -1 if the plugin is incompatible with
 * the active version of GCC
 */
extern "C" int plugin_init (struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	// Check that GCC base version starts with "4.8"
	if (strncmp(version->basever,myplugin_version.basever,3) != 0) {
		std::cerr << "Sadly, the KayrebtPathExaminer GCC plugin is only "
			"available for GCC 4.8 for now." << std::endl;
		return -1; // incompatible
	}

	struct register_pass_info actdiag_extractor_pass_info = {
		.pass				= &path_evaluation_pass.pass,
		.reference_pass_name		= "optimized",
		.ref_pass_instance_number	= 1,
		.pos_op				= PASS_POS_INSERT_AFTER
	};

	int argc = plugin_info->argc;
	struct plugin_argument *argv = plugin_info->argv;
	const char* plugin_name = plugin_info->base_name;

	for (int i = 0; i < argc; ++i)
	{
//		if (!strcmp (argv[i].key, "check-operator-eq"))
//		{
//			if (argv[i].value)
//				warning (0, G_("option '-fplugin-arg-%s-check-operator-eq=%s'"
//							" ignored (superfluous '=%s')"),
//						plugin_name, argv[i].value, argv[i].value);
//			else
//				check_operator_eq = true;
//		}
//		else if (!strcmp (argv[i].key, "no-check-operator-eq"))
//		{
//			if (argv[i].value)
//				warning (0, G_("option '-fplugin-arg-%s-no-check-operator-eq=%s'"
//							" ignored (superfluous '=%s')"),
//						plugin_name, argv[i].value, argv[i].value);
//			else
//				check_operator_eq = false;
//		}
//		else
			warning (0, G_("plugin %qs: unrecognized argument %qs ignored"),
					plugin_name, argv[i].key);
	}

//	fprintf(dump_file, "I'm alive!\n");

	register_callback(plugin_name,
			PLUGIN_PASS_MANAGER_SETUP,
			NULL,
			&actdiag_extractor_pass_info);
	return 0;
}

extern "C" bool evaluate_paths_gate()
{
	return true;//dump_file;
}

/**
 * \brief Evaluate paths
 *
 * If there is a compilation error, no graph is produced.
 * \return 0 even if there is an error, in order to build as many graphs as
 * possible without making GCC crash, except if the error is global
 */
extern "C" unsigned int evaluate_paths()
{
	// If there were errors during compilation,
	// let GCC handle the exit.
	if (errorcount || sorrycount)
		return 0;

	Evaluator ev;
	ev.evaluateAllPaths();

	return 0;
}


