**12.Dec.2025**
1. Add current location as the first candidate destination using "stay cost" for weight.
2. Collect remaining candidate destinations:
	1. Calculate swim distance.
	2. For each node, calculate shortest path length from start node. Use a variant of Dijkstra walk.
	3. If shortest path to node is within swim range, add to candidate list, using path length to calculate cost for weight.
	4. Optimization: if a node is outside of swim range, do not add any of its connections to the list of remaining nodes to examine.
3. Take a weighted sample from the candidate destinations. Use this as the destination for the timestep.