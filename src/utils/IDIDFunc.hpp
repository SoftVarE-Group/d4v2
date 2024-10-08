#include <cassert>
#include <vector>
struct IDIDFunc {
  std::vector<int> data;
  int max_val = 0, max_img = 0;
  IDIDFunc(int max_val, int max_img)
      : data(max_val, 0), max_val(max_val), max_img(max_img) {}
  IDIDFunc() {}
  const int &operator[](int v) const {
    assert(v < max_val);
    const int &i = data[v];
    assert(i < max_img);
    return i;
  }
  int &operator[](int v) {
    assert(v < max_val);
    int &i = data[v];
    assert(i < max_img);
    return i;
  }
  IDIDFunc reverse() {
    IDIDFunc out(max_img, max_val);
    for (int i = 0; i < max_val; i++) {
      out.data[data[i]] = i;
    }
    return out;
  }
  template <typename T> std::vector<T> permute(std::vector<T> values) {
    std::vector<T> out(max_img);
    for (int i = 0; i < max_val; i++) {
      out[i] = values[data[i]];
    }
    return out;
  }
};
