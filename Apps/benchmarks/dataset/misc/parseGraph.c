#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  char graphname[256];
  FILE *graph_fp;
  int nodes, edges, max_node;
  int *degrees;
  int max_degree, max_deg_node;
  char line[256];
  int src, dest;

  graph_fp = NULL;
  nodes = 0;
  edges = 0;
  max_node = 0;
  max_degree = 0;
  max_deg_node = -1;
  degrees = NULL;
  src = -1;
  dest = -1;

  if (argc != 2) {
    fprintf(stderr, "Usage: ./parse_graph <INPUT GRAPH>\n");
    exit(-1);
  }

  strcpy(graphname, argv[1]);
  graph_fp = fopen(graphname, "r");

  if (!graph_fp) {
    fprintf(stderr, "Error: cannot open %s\n", graphname);
    exit(0);
  }

  // first scan to get the node size
  while (fgets(line, sizeof(line), graph_fp)) {
    if (line[0] == '#') continue;
    sscanf(line, "%d%*[^0-9]%d\n", &src, &dest);
    if ((src+1) > max_node) max_node = src+1;
    if ((dest+1) > max_node) max_node = dest+1;
  }
  
  degrees = (int *) calloc(max_node, sizeof(int));
  if (degrees == NULL) {
    fprintf(stderr, "Failure: memory allocation failed!\n");
    exit(-1);
  }

  // second scan
  rewind(graph_fp);
  while (fgets(line, sizeof(line), graph_fp)) {
    if (line[0] == '#') {
      if (strstr(line, "Nodes") != NULL) {
        fprintf(stdout, "%s\n", line);
        sscanf(line, "%*[^0-9]%d%*[^0-9]%d", &nodes, &edges);
      }
    } else {
      sscanf(line, "%d%*[^0-9]%d\n", &src, &dest);
      degrees[src]++;
      if (degrees[src] > max_degree) {
        max_degree = degrees[src];
        max_deg_node = src;
      }
    }
  }

  fprintf(stdout, "Nodes: %d Edges: %d\n", nodes, edges);
  fprintf(stdout, "# NodeIDs: %d Degree: %d max_deg_node: %d\n", max_node, max_degree, max_deg_node);

  fclose(graph_fp);
  free(degrees);

  return 0;
} 

