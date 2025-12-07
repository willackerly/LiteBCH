#include <algorithm>
#include <iostream>
#include <vector>

int main() {
  std::vector<int> v = {3, 1, 4};
  std::sort(v.begin(), v.end());
  std::cout << "Environment OK" << std::endl;
  return 0;
}
