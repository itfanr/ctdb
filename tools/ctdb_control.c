/* 
   ctdb control tool

   Copyright (C) Andrew Tridgell  2007

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "includes.h"
#include "lib/events/events.h"
#include "system/filesys.h"
#include "popt.h"
#include "cmdline.h"
#include "../include/ctdb_private.h"


/*
  show usage message
 */
static void usage(void)
{
	printf("Usage: ctdb_control [options] <control>\n");
	printf("\nControls:\n");
	printf("  ping\n");
	printf("  process-exists <vnn:pid>           see if a process exists\n");
	printf("  status <vnn|all>                   show ctdb status on a node\n");
	printf("  statusreset <vnn|all>              reset status on a node\n");
	printf("  debug <vnn|all> <level>            set ctdb debug level on a node\n");
	printf("  debuglevel                         display ctdb debug levels\n");
	printf("  getvnnmap <vnn>                    display ctdb vnnmap\n");
	printf("  setvnnmap <vnn> <generation> <numslots> <lmaster>*\n");
	printf("  getdbmap <vnn>                     lists databases on a node\n");
	printf("  getnodemap <vnn>                   lists nodes known to a ctdb daemon\n");
	exit(1);
}

/*
  see if a process exists
 */
static int control_process_exists(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn, pid;
	int ret;
	if (argc < 1) {
		usage();
	}

	if (sscanf(argv[0], "%u:%u", &vnn, &pid) != 2) {
		printf("Badly formed vnn:pid\n");
		return -1;
	}

	ret = ctdb_process_exists(ctdb, vnn, pid);
	if (ret == 0) {
		printf("%u:%u exists\n", vnn, pid);
	} else {
		printf("%u:%u does not exist\n", vnn, pid);
	}
	return ret;
}

/*
  display status structure
 */
static void show_status(struct ctdb_status *s)
{
	printf("CTDB version %u\n", CTDB_VERSION);
	printf(" client_packets_sent     %u\n", s->client_packets_sent);
	printf(" client_packets_recv     %u\n", s->client_packets_recv);
	printf("   req_call              %u\n", s->client.req_call);
	printf("   req_message           %u\n", s->client.req_message);
	printf("   req_finished          %u\n", s->client.req_finished);
	printf("   req_register          %u\n", s->client.req_register);
	printf("   req_connect_wait      %u\n", s->client.req_connect_wait);
	printf("   req_shutdown          %u\n", s->client.req_shutdown);
	printf("   req_control           %u\n", s->client.req_control);
	printf(" node_packets_sent       %u\n", s->node_packets_sent);
	printf(" node_packets_recv       %u\n", s->node_packets_recv);
	printf("   req_call              %u\n", s->count.req_call);
	printf("   reply_call            %u\n", s->count.reply_call);
	printf("   req_dmaster           %u\n", s->count.req_dmaster);
	printf("   reply_dmaster         %u\n", s->count.reply_dmaster);
	printf("   reply_error           %u\n", s->count.reply_error);
	printf("   req_message           %u\n", s->count.req_message);
	printf("   req_finished          %u\n", s->count.req_finished);
	printf(" total_calls             %u\n", s->total_calls);
	printf(" pending_calls           %u\n", s->pending_calls);
	printf(" lockwait_calls          %u\n", s->lockwait_calls);
	printf(" pending_lockwait_calls  %u\n", s->pending_lockwait_calls);
	printf(" max_call_latency        %.6f sec\n", s->max_call_latency);
	printf(" max_lockwait_latency    %.6f sec\n", s->max_lockwait_latency);
}

/*
  display remote ctdb status combined from all nodes
 */
static int control_status_all(struct ctdb_context *ctdb)
{
	int ret, i;
	struct ctdb_status status;
	uint32_t *nodes;
	uint32_t num_nodes;

	nodes = ctdb_get_connected_nodes(ctdb, ctdb, &num_nodes);
	CTDB_NO_MEMORY(ctdb, nodes);
	
	ZERO_STRUCT(status);

	for (i=0;i<num_nodes;i++) {
		struct ctdb_status s1;
		int j;
		uint32_t *v1 = (uint32_t *)&s1;
		uint32_t *v2 = (uint32_t *)&status;
		uint32_t num_ints = 
			offsetof(struct ctdb_status, __last_counter) / sizeof(uint32_t);
		ret = ctdb_status(ctdb, nodes[i], &s1);
		if (ret != 0) {
			printf("Unable to get status from node %u\n", nodes[i]);
			return ret;
		}
		for (j=0;j<num_ints;j++) {
			v2[j] += v1[j];
		}
		status.max_call_latency = 
			MAX(status.max_call_latency, s1.max_call_latency);
		status.max_lockwait_latency = 
			MAX(status.max_lockwait_latency, s1.max_lockwait_latency);
	}
	talloc_free(nodes);
	printf("Gathered status for %u nodes\n", num_nodes);
	show_status(&status);
	return 0;
}

/*
  display remote ctdb status
 */
static int control_status(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn;
	int ret;
	struct ctdb_status status;
	if (argc < 1) {
		usage();
	}

	if (strcmp(argv[0], "all") == 0) {
		return control_status_all(ctdb);
	}

	vnn = strtoul(argv[0], NULL, 0);

	ret = ctdb_status(ctdb, vnn, &status);
	if (ret != 0) {
		printf("Unable to get status from node %u\n", vnn);
		return ret;
	}
	show_status(&status);
	return 0;
}


/*
  reset status on all nodes
 */
static int control_status_reset_all(struct ctdb_context *ctdb)
{
	int ret, i;
	uint32_t *nodes;
	uint32_t num_nodes;

	nodes = ctdb_get_connected_nodes(ctdb, ctdb, &num_nodes);
	CTDB_NO_MEMORY(ctdb, nodes);
	
	for (i=0;i<num_nodes;i++) {
		ret = ctdb_status_reset(ctdb, nodes[i]);
		if (ret != 0) {
			printf("Unable to reset status on node %u\n", nodes[i]);
			return ret;
		}
	}
	talloc_free(nodes);
	return 0;
}


/*
  reset remote ctdb status
 */
static int control_status_reset(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn;
	int ret;
	if (argc < 1) {
		usage();
	}

	if (strcmp(argv[0], "all") == 0) {
		return control_status_reset_all(ctdb);
	}

	vnn = strtoul(argv[0], NULL, 0);

	ret = ctdb_status_reset(ctdb, vnn);
	if (ret != 0) {
		printf("Unable to reset status on node %u\n", vnn);
		return ret;
	}
	return 0;
}

/*
  display remote ctdb vnn map
 */
static int control_getvnnmap(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn;
	int i, ret;
	struct ctdb_vnn_map *vnnmap;
	if (argc < 1) {
		usage();
	}

	vnn = strtoul(argv[0], NULL, 0);

	vnnmap = talloc_zero(ctdb, struct ctdb_vnn_map);
	ret = ctdb_getvnnmap(ctdb, vnn, vnnmap);
	if (ret != 0) {
		printf("Unable to get vnnmap from node %u\n", vnn);
		return ret;
	}
	printf("Generation:%d\n",vnnmap->generation);
	printf("Size:%d\n",vnnmap->size);
	for(i=0;i<vnnmap->size;i++){
		printf("hash:%d lmaster:%d\n",i,vnnmap->map[i]);
	}
	return 0;
}

/*
  display a list of the databases on a remote ctdb
 */
static int control_getdbmap(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn;
	int i, ret;
	struct ctdb_dbid_map *dbmap;

	if (argc < 1) {
		usage();
	}

	vnn = strtoul(argv[0], NULL, 0);

	dbmap = talloc_zero(ctdb, struct ctdb_dbid_map);
	ret = ctdb_getdbmap(ctdb, vnn, dbmap);
	if (ret != 0) {
		printf("Unable to get dbids from node %u\n", vnn);
		talloc_free(dbmap);
		return ret;
	}

	printf("Number of databases:%d\n", dbmap->num);
	for(i=0;i<dbmap->num;i++){
		const char *path;

		ctdb_getdbpath(ctdb, dbmap->dbids[i], dbmap, &path);
		printf("dbid:0x%08x path:%s\n", dbmap->dbids[i], path);
	}
	talloc_free(dbmap);
	return 0;
}

/*
  display a list nodes known to a remote ctdb
 */
static int control_getnodemap(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn;
	int i, ret;
	struct ctdb_node_map *nodemap;

	if (argc < 1) {
		usage();
	}

	vnn = strtoul(argv[0], NULL, 0);

	nodemap = talloc_zero(ctdb, struct ctdb_node_map);
	ret = ctdb_getnodemap(ctdb, vnn, nodemap, nodemap);
	if (ret != 0) {
		printf("Unable to get nodemap from node %u\n", vnn);
		talloc_free(nodemap);
		return ret;
	}

	printf("Number of nodes:%d\n", nodemap->num);
	for(i=0;i<nodemap->num;i++){
		printf("vnn:%d %s\n", nodemap->nodes[i].vnn, nodemap->nodes[i].flags&NODE_FLAGS_CONNECTED?"UNAVAILABLE":"CONNECTED");
	}
	talloc_free(nodemap);
	return 0;
}

/*
  set remote ctdb vnn map
 */
static int control_setvnnmap(struct ctdb_context *ctdb, int argc, const char **argv)
{
	uint32_t vnn;
	struct ctdb_vnn_map *vnnmap;
	int i, ret;
	if (argc < 3) {
		usage();
	}

	vnn = strtoul(argv[0], NULL, 0);

	vnnmap = talloc_zero(ctdb, struct ctdb_vnn_map);
	vnnmap->generation = strtoul(argv[1], NULL, 0);
	vnnmap->size = strtoul(argv[2], NULL, 0);
	vnnmap->map = talloc_array(vnnmap, uint32_t, vnnmap->size);
	for (i=0;i<vnnmap->size;i++) {
		vnnmap->map[i] = strtoul(argv[3+i], NULL, 0);
	}

	ret = ctdb_setvnnmap(ctdb, vnn, vnnmap);
	if (ret != 0) {
		printf("Unable to set vnnmap for node %u\n", vnn);
		return ret;
	}
	return 0;
}

/*
  ping all node
 */
static int control_ping(struct ctdb_context *ctdb, int argc, const char **argv)
{
	int ret, i;
	uint32_t *nodes;
	uint32_t num_nodes;

	nodes = ctdb_get_connected_nodes(ctdb, ctdb, &num_nodes);
	CTDB_NO_MEMORY(ctdb, nodes);

	for (i=0;i<num_nodes;i++) {
		struct timeval tv = timeval_current();
		ret = ctdb_ping(ctdb, nodes[i]);
		if (ret == -1) {
			printf("Unable to get ping response from node %u\n", nodes[i]);
		} else {
			printf("response from %u time=%.6f sec  (%d clients)\n", 
			       nodes[i], timeval_elapsed(&tv), ret);
		}
	}
	talloc_free(nodes);
	return 0;
}


/*
  display debug level on all node
 */
static int control_debuglevel(struct ctdb_context *ctdb, int argc, const char **argv)
{
	int ret, i;
	uint32_t *nodes;
	uint32_t num_nodes;

	nodes = ctdb_get_connected_nodes(ctdb, ctdb, &num_nodes);
	CTDB_NO_MEMORY(ctdb, nodes);

	for (i=0;i<num_nodes;i++) {
		uint32_t level;
		ret = ctdb_get_debuglevel(ctdb, nodes[i], &level);
		if (ret != 0) {
			printf("Unable to get debuglevel response from node %u\n", 
				nodes[i]);
		} else {
			printf("Node %u is at debug level %u\n", nodes[i], level);
		}
	}
	talloc_free(nodes);
	return 0;
}

/*
  set debug level on a node
 */
static int control_debug(struct ctdb_context *ctdb, int argc, const char **argv)
{
	int ret;
	uint32_t vnn, level, i;
	uint32_t *nodes;
	uint32_t num_nodes;

	if (argc < 2) {
		usage();
	}

	level = strtoul(argv[1], NULL, 0);

	if (strcmp(argv[0], "all") != 0) {
		vnn = strtoul(argv[0], NULL, 0);
		ret = ctdb_set_debuglevel(ctdb, vnn, level);
		if (ret != 0) {
			printf("Unable to set debug level on node %u\n", vnn);
		}
		
		return 0;
	}

	nodes = ctdb_get_connected_nodes(ctdb, ctdb, &num_nodes);
	CTDB_NO_MEMORY(ctdb, nodes);
	for (i=0;i<num_nodes;i++) {
		ret = ctdb_set_debuglevel(ctdb, nodes[i], level);
		if (ret != 0) {
			printf("Unable to set debug level on node %u\n", nodes[i]);
			break;
		}
	}
	talloc_free(nodes);
	return 0;
}


/*
  attach to a database
 */
static int control_attach(struct ctdb_context *ctdb, int argc, const char **argv)
{
	const char *db_name;
	struct ctdb_db_context *ctdb_db;
	if (argc < 1) {
		usage();
	}
	db_name = argv[0];

	ctdb_db = ctdb_attach(ctdb, db_name);
	if (ctdb_db == NULL) {
		DEBUG(0,("Unable to attach to database '%s'\n", db_name));
		return -1;
	}

	return 0;
}

/*
  main program
*/
int main(int argc, const char *argv[])
{
	struct ctdb_context *ctdb;
	struct poptOption popt_options[] = {
		POPT_AUTOHELP
		POPT_CTDB_CMDLINE
		POPT_TABLEEND
	};
	int opt;
	const char **extra_argv;
	int extra_argc = 0;
	int ret;
	poptContext pc;
	struct event_context *ev;
	const char *control;

	pc = poptGetContext(argv[0], argc, argv, popt_options, POPT_CONTEXT_KEEP_FIRST);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "Invalid option %s: %s\n", 
				poptBadOption(pc, 0), poptStrerror(opt));
			exit(1);
		}
	}

	/* setup the remaining options for the main program to use */
	extra_argv = poptGetArgs(pc);
	if (extra_argv) {
		extra_argv++;
		while (extra_argv[extra_argc]) extra_argc++;
	}

	if (extra_argc < 1) {
		usage();
	}

	control = extra_argv[0];

	ev = event_context_init(NULL);

	/* initialise ctdb */
	ctdb = ctdb_cmdline_client(ev);
	if (ctdb == NULL) {
		printf("Failed to init ctdb\n");
		exit(1);
	}

	if (strcmp(control, "process-exists") == 0) {
		ret = control_process_exists(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "status") == 0) {
		ret = control_status(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "statusreset") == 0) {
		ret = control_status_reset(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "getvnnmap") == 0) {
		ret = control_getvnnmap(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "getdbmap") == 0) {
		ret = control_getdbmap(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "getnodemap") == 0) {
		ret = control_getnodemap(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "setvnnmap") == 0) {
		ret = control_setvnnmap(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "ping") == 0) {
		ret = control_ping(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "debug") == 0) {
		ret = control_debug(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "debuglevel") == 0) {
		ret = control_debuglevel(ctdb, extra_argc-1, extra_argv+1);
	} else if (strcmp(control, "attach") == 0) {
		ret = control_attach(ctdb, extra_argc-1, extra_argv+1);
	} else {
		printf("Unknown control '%s'\n", control);
		exit(1);
	}

	return ret;
}
