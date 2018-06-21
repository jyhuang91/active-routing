## Graph Processing for PageRank

This tool is used to process original graphs so that it can be used by the modified PageRank benchmark.

#### 1.	Supported Graphs

The parseGraph is tied with Stanford SNAP graphs, they can be downloaded from snap.stanford.edu


#### 2.	Process the graph

```bash
$ make
$ ./parseGraph <graph>
```

The execution will output to stdout with a line like this:

`# NodeIDs: 9912294 Degree: 562 max_deg_node: 9905111`

Then add this line to the top of the file:

```bash
$ ./addline.sh "# NodeIDs: 9912294 Degree: 562 max_deg_node: 9905111" <graph>
```

The processed graph with this line now can be used by the PageRank.
