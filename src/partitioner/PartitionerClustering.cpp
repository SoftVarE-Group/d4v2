#include "PartitionerClustering.hpp"
#include "ffi.rs.h"
#include <vector>

namespace d4 {
PartitionerClustering::PartitionerClustering(Config &config): method(config.clustering_method) {}

void PartitionerClustering::computePartition(HyperGraph &hypergraph_in,
                                             Level level,
                                             std::vector<int> &partition_out) {
  auto hypergraph = hypergraph::with_capacity(hypergraph_in.getSize());

  for (auto &edge : hypergraph_in) {
    auto edge_vector = std::vector<size_t>();
    edge_vector.resize(edge.getSize());

    for (auto i = 0; i < edge.getSize(); i++) {
      edge_vector[i] = edge[i];
    }

    hypergraph->add_net(edge_vector);
  }

  rust::Vec<size_t> partition;

  if (method == "bipartite") {
    partition = hypergraph->incidence_graph_louvain_bipartite();
  } else if (method == "min3") {
    partition = hypergraph->incidence_graph_louvain_min10();
  } else if (method == "louvain") {
    partition = hypergraph->incidence_graph_louvain();
  } else {
    partition = hypergraph->incidence_graph_louvain();
  }

  partition_out.resize(partition.size());

  for (auto i = 0; i < partition.size(); i++) {
    partition_out[i] = partition[i];
  }
}
} // namespace d4
