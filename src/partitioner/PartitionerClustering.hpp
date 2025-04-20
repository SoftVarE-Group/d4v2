#pragma once

#include <vector>

#include "PartitionerManager.hpp"

namespace d4 {
class PartitionerClustering final : public PartitionerManager {
public:
  PartitionerClustering(Config &config);

  void computePartition(HyperGraph &hypergraph, Level level,
                        std::vector<int> &partition) override;

private:
  string method;
};
} // namespace d4
