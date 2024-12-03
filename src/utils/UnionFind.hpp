#pragma once
#include <algorithm>
#include <assert.h>
#include <vector>
struct UnionFind {

  int cnt;
  std::vector<int> parent;
  std::vector<int> size;
  UnionFind(int s) :cnt(s), parent(s), size(s) { clear(); }

  int find_set(int v) {
    if (v == parent[v])
      return v;
    return parent[v] = find_set(parent[v]);
  }

  void union_sets(int a, int b) {
    a = find_set(a);
    b = find_set(b);
    if (a != b) {
      cnt--;
      if (size[a] < size[b])
        std::swap(a, b);
      parent[b] = a;
      size[a] += size[b];
    }
  }
  void clear() {
    for (int i = 0; i < parent.size(); i++) {
      parent[i] = i;
      size[i] = 1;
    }
    cnt = parent.size();
  }
};
