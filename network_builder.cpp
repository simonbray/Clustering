
#include "tools.hpp"

#include <iostream>
#include <fstream>
#include <set>
#include <string>

#include <boost/program_options.hpp>

namespace {
  bool verbose = false;
  std::ostream devnull(0);
  std::ostream& logger(std::ostream& s) {
    if (verbose) {
      return s;
    } else {
      return devnull; 
    }
  }
} // end local namespace


std::vector<uint>
read_clustered_trajectory(std::string filename) {
  std::vector<uint> traj;
  std::ifstream ifs(filename);
  if (ifs.fail()) {
    std::cerr << "error: cannot open file '" << filename << "'" << std::endl;
    exit(EXIT_FAILURE);
  } else {
    while (ifs.good()) {
      uint buf;
      ifs >> buf;
      if ( ! ifs.fail()) {
        traj.push_back(buf);
      }
    }
  }
  return traj;
}


void
write_clustered_trajectory(std::string filename, std::vector<uint> traj) {
  std::ofstream ofs(filename);
  if (ofs.fail()) {
    std::cerr << "error: cannot open file '" << filename << "'" << std::endl;
    exit(EXIT_FAILURE);
  } else {
    for (uint c: traj) {
      ofs << c << "\n";
    }
  }
}


////////

int main(int argc, char* argv[]) {
  namespace b_po = boost::program_options;
  b_po::variables_map args;
  b_po::options_description desc (std::string(argv[0]).append(
    "\n\n"
    "build network information from density based clustering."
    "\n"
    "options"));
  desc.add_options()
    ("help,h", b_po::bool_switch()->default_value(false), "show this help.")
    // optional
    ("basename,b", b_po::value<std::string>()->default_value("clust.\%0.1f"), "(optional): basename of input files (default: clust.\%0.1f).")
    ("min", b_po::value<float>()->default_value(0.1f), "(optional): minimum free energy (default: 0.1).")
    ("max", b_po::value<float>()->default_value(8.0f), "(optional): maximum free energy (default: 8.0).")
    ("step", b_po::value<float>()->default_value(0.1f), "(optional): minimum free energy (default: 0.1).")
    ("minpop,p", b_po::value<uint>()->default_value(1), "(optional): minimum population of node to be considered for network (default: 1).")
    // defaults
    ("verbose,v", b_po::bool_switch()->default_value(false), "verbose mode: print runtime information to STDOUT.")
  ;
  // parse cmd arguments
  try {
    b_po::store(b_po::command_line_parser(argc, argv).options(desc).run(), args);
    b_po::notify(args);
  } catch (b_po::error& e) {
    if ( ! args["help"].as<bool>()) {
      std::cout << "\n" << e.what() << "\n\n" << std::endl;
    }
    std::cout << desc << std::endl;
    return 2;
  }
  if (args["help"].as<bool>()) {
    std::cout << desc << std::endl;
    return 1;
  }
  // setup general flags / options
  verbose = args["verbose"].as<bool>();

  float d_min = args["min"].as<float>();
  float d_max = args["max"].as<float>();
  float d_step = args["step"].as<float>();
  std::string basename = args["basename"].as<std::string>();
  uint minpop = args["minpop"].as<uint>();

  std::set<std::pair<uint, uint>> network;
  std::map<uint, uint> pops;
  std::map<uint, float> free_energies;

  std::vector<uint> cl_next = read_clustered_trajectory(stringprintf(basename, d_min));
  std::vector<uint> cl_now;
  uint max_id;
  std::size_t n_rows = cl_next.size();
  for (float d=d_min; d < d_max; d += d_step) {
    logger(std::cout) << "free energy level: " << d << std::endl;
    cl_now = cl_next;
    write_clustered_trajectory("remapped_" + stringprintf(basename, d), cl_now);
    max_id = *std::max_element(cl_now.begin(), cl_now.end());
    cl_next = read_clustered_trajectory(stringprintf(basename, d + d_step));
    for (std::size_t i=0; i < n_rows; ++i) {
      if (cl_next[i] != 0) {
        cl_next[i] += max_id;
        if (cl_now[i] != 0) {
          network.insert({cl_next[i], cl_now[i]});
          ++pops[cl_now[i]];
          free_energies[cl_now[i]] = d;
        }
      }
    }
  }
  // handle last trajectory
  logger(std::cout) << "free energy level: " << d_max << std::endl;
  cl_now = cl_next;
  write_clustered_trajectory("remapped_" + stringprintf(basename, d_max), cl_now);
  for (std::size_t i=0; i < n_rows; ++i) {
    if (cl_now[i] != 0) {
      ++pops[cl_now[i]];
      free_energies[cl_now[i]] = d_max;
    }
  }
  // if minpop given: delete nodes and edges not fulfilling min. population criterium
  //TODO: ineffective, needs optimization
  if (minpop > 1) {
    auto pop_it = pops.begin();
    while (pop_it != pops.end()) {
      logger(std::cout) << "cleaning from low pop. states: " << pop_it->first << std::endl;
      if (pop_it->second < minpop) {
        uint node_id = pop_it->first;
        auto net_it = network.begin();
        while (net_it != network.end()) {
          if (net_it->first == node_id || net_it->second == node_id) {
            network.erase(net_it++); // first increment, then remove with old address
          } else {
            ++net_it;
          }
        }
        pops.erase(pop_it++); // as above
      } else {
        ++pop_it;
      }
    }
  }
  // save network links
  {
    std::ofstream ofs("network_links.dat");
    if (ofs.fail()) {
      std::cerr << "error: cannot open file 'network_links.dat'" << std::endl;
      exit(EXIT_FAILURE);
    } else {
      for (auto p: network) {
        ofs << p.first << " " << p.second << "\n";
      }
    }
  }
  // save node info
  {
    std::ofstream ofs("network_nodes.dat");
    if (ofs.fail()) {
      std::cerr << "error: cannot open file 'network_nodes.dat'" << std::endl;
      exit(EXIT_FAILURE);
    } else {
      for (auto node_pop: pops) {
        uint key = node_pop.first;
        ofs << key << " " << free_energies[key] << " " << node_pop.second << "\n";
      }
    }
  }
  return 0;
}

