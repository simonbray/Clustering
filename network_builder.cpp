
#include "tools.hpp"
#include "logger.hpp"

#include <iostream>
#include <fstream>
#include <set>
#include <unordered_set>

#include <boost/program_options.hpp>

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
    ("basename,b", b_po::value<std::string>()->default_value("clust.\%0.1f"),
          "(optional): basename of input files (default: clust.\%0.1f).")
    ("min", b_po::value<float>()->default_value(0.1f, "0.1"), "(optional): minimum free energy (default: 0.1).")
    ("max", b_po::value<float>()->default_value(8.0f, "8.0"), "(optional): maximum free energy (default: 8.0).")
    ("step", b_po::value<float>()->default_value(0.1f, "0.1"), "(optional): minimum free energy (default: 0.1).")
    ("minpop,p", b_po::value<std::size_t>()->default_value(1),
          "(optional): minimum population of node to be considered for network (default: 1).")
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
  Clustering::verbose = args["verbose"].as<bool>();

  float d_min = args["min"].as<float>();
  float d_max = args["max"].as<float>();
  float d_step = args["step"].as<float>();
  std::string basename = args["basename"].as<std::string>();
  std::size_t minpop = args["minpop"].as<std::size_t>();

  std::set<std::pair<std::size_t, std::size_t>> network;
  std::map<std::size_t, std::size_t> pops;
  std::map<std::size_t, float> free_energies;

  std::vector<std::size_t> cl_next = read_clustered_trajectory(stringprintf(basename, d_min));
  std::vector<std::size_t> cl_now;
  std::size_t max_id;
  std::size_t n_rows = cl_next.size();
  for (float d=d_min; d < d_max; d += d_step) {
    Clustering::logger(std::cout) << "free energy level: " << d << std::endl;
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
  Clustering::logger(std::cout) << "free energy level: " << stringprintf("%0.2f", d_max) << std::endl;
  cl_now = cl_next;
  write_clustered_trajectory("remapped_" + stringprintf(basename, d_max), cl_now);
  for (std::size_t i=0; i < n_rows; ++i) {
    if (cl_now[i] != 0) {
      ++pops[cl_now[i]];
      free_energies[cl_now[i]] = d_max;
    }
  }
  // if minpop given: delete nodes and edges not fulfilling min. population criterium
  if (minpop > 1) {
    Clustering::logger(std::cout) << "cleaning from low pop. states ..." << std::endl;
    std::unordered_set<std::size_t> removals;
    auto pop_it = pops.begin();
    Clustering::logger(std::cout) << "  ... search nodes to remove" << std::endl;
    while (pop_it != pops.end()) {
      if (pop_it->second < minpop) {
        removals.insert(pop_it->first);
        pops.erase(pop_it++); // as above
      } else {
        ++pop_it;
      }
    }
    Clustering::logger(std::cout) << "  ... search edges to remove" << std::endl;
    auto net_it = network.begin();
    while (net_it != network.end()) {
      std::size_t a = net_it->first;
      std::size_t b = net_it->second;
      if (removals.count(a) > 0 || removals.count(b) > 0) {
        network.erase(net_it++);
      } else {
        ++net_it;
      }
    }
    Clustering::logger(std::cout) << "  ... finished." << std::endl;
  }
  // save network links
  {
    Clustering::logger(std::cout) << "saving links" << std::endl;
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
    Clustering::logger(std::cout) << "saving nodes" << std::endl;
    std::ofstream ofs("network_nodes.dat");
    if (ofs.fail()) {
      std::cerr << "error: cannot open file 'network_nodes.dat'" << std::endl;
      exit(EXIT_FAILURE);
    } else {
      for (auto node_pop: pops) {
        std::size_t key = node_pop.first;
        ofs << key << " " << free_energies[key] << " " << node_pop.second << "\n";
      }
    }
  }
  // TODO: save 'leafs'
  {
    Clustering::logger(std::cout) << "saving leafs, i.e. tree end nodes" << std::endl;
    std::ofstream ofs("network_leafs.dat");
    if (ofs.fail()) {
      std::cerr << "error: cannot open file 'network_leafs.dat'" << std::endl;
      exit(EXIT_FAILURE);
    } else {
/////////
//for to_from pair in nodes:
//  insert from -> leafs-set
//  if 'to' in leafs-set:
//    del from leafs-set
//    insert to -> leafs-set
    }
  }
  return 0;
}

