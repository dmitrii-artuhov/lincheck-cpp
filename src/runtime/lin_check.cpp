
#include <functional>
#include <map>
#include "include/checker.h"

// TODO: using
// TODO: std::any
typedef std::string MethodName;
typedef void* Object; // TODO: must be the trait

struct LinearizabilityChecker : ModelChecker {
  LinearizabilityChecker(std::map<MethodName, std::function<int(Object)>> specification_methods,
                         std::function<Object(Object)> copy);

  bool Check(const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>>& history);

private:
  std::map<MethodName, std::function<int(Object)>> specification_methods;
  std::function<Object(Object)> copy;
};

LinearizabilityChecker::LinearizabilityChecker(std::map<MethodName, std::function<int(Object)>> specification_methods,
                                               std::function<Object(Object)> copy) : specification_methods(specification_methods), copy(copy) {}

bool LinearizabilityChecker::Check(const std::vector<std::variant<StackfulTaskInvoke, StackfulTaskResponse>> &history) {

}
