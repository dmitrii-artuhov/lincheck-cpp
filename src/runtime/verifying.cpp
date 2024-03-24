#include "include/verifying.h"

#include <algorithm>

namespace ltest {

std::string toLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> res{""};
  for (char c : s) {
    if (c == delim) {
      res.push_back("");
    } else {
      res.back() += c;
    }
  }
  return res;
}

const std::string kRR = "rr";
const std::string kRandom = "random";

// Extracts required opts, returns the rest of args.
std::vector<std::string> parse_opts(std::vector<std::string> args, Opts &opts) {
  if (args.size() < 5) {
    throw std::invalid_argument("all required opts should be specified");
  }
  opts.threads = std::stoul(args[0]);  // Throws if can't transform.
  opts.tasks = std::stoul(args[1]);
  opts.rounds = std::stoul(args[2]);
  opts.verbose = std::stoi(args[3]) == 1;
  std::string strategy_name = args[4];
  strategy_name = toLower(std::move(strategy_name));
  if (strategy_name == kRR) {
    opts.typ = RR;
  } else if (strategy_name == kRandom) {
    opts.typ = RND;
  } else {
    throw std::invalid_argument("unsupported strategy");
  }
  args.erase(args.begin(), args.begin() + 5);
  return args;
}

}  // namespace ltest