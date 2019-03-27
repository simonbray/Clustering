/*
Copyright (c) 2015, Florian Sittel (www.lettis.net)
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "coords_file/coords_file.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <iomanip>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include "state_filter.hpp"
#include "tools.hpp"
#include "logger.hpp"


namespace Clustering {
namespace Filter {
  void
  fixedprint(double num, std::size_t prec, std::size_t width){
    std::cout.width(width);
    std::cout << std::setprecision(prec);
    std::cout << num;
    std::cout.width(0);
    return;
  }

  void
  main(boost::program_options::variables_map args) {
    using namespace Clustering::Tools;
    // load states
    Clustering::logger(std::cout) << "~~~ reading files\n"
                                  << "    trajectory from: "
                                  << args["states"].as<std::string>()
                                  << std::endl;
    std::string fname_states = args["states"].as<std::string>();
    std::vector<std::size_t> states = Clustering::Tools::read_clustered_trajectory(args["states"].as<std::string>());
    std::size_t n_frames = states.size();
    if (args["list"].as<bool>()) { // mode stats with verbose true
      std::priority_queue<std::pair<std::size_t, std::size_t>> pops;
      // list states with pops
      std::set<std::size_t> state_ids(states.begin(), states.end());
      for (std::size_t id: state_ids) {
        std::size_t pop = std::count(states.begin(), states.end(), id);
        pops.push({pop, id});
      }
      // get number of entering each state
      std::map<std::size_t, std::size_t> entered;
      entered[states[0]] = 1;
//      std::size_t diff(n_frames);
      for(std::size_t i=0; i < n_frames-1; ++i){
        if (states[i+1] != states[i]){
          std::map<std::size_t, std::size_t>::iterator it(entered.find(states[i+1]));
          if (it != entered.end()){
            it->second++;
          } else {
            entered[states[i+1]] = 1;
          }
        }
      }
      std::cout << "~~~ state stats\n"
                << "    state  population  pop [%]  tot [%]  entered" << std::endl;
      std::cout << std::fixed;
      double total_pop = 0.;
      std::size_t total_entered = 0;
      while ( ! pops.empty()) {
        auto pop_id = pops.top(); // get top element
        pops.pop(); // remove top element
        std::cout << "    ";
        // state id
        fixedprint(pop_id.second, 0, 5);
        // absolute pop
        fixedprint(pop_id.first, 0, 12);
        // relative pop
        fixedprint(100.*pop_id.first/(float)n_frames, 3, 9);
        // total pop
        total_pop += 100.*pop_id.first/(float)n_frames;
        fixedprint(total_pop, 3, 9);
        // entered
        std::map<std::size_t, std::size_t>::iterator it(entered.find(pop_id.second));
        if (it != entered.end()){
          total_entered += it->second;
          fixedprint(it->second, 0, 9);
        } else { // the following should never be the case
          fixedprint(0, 0, 9);
        }
        std::cout << std::endl;
      }
      std::cout << "\n~~~ total number of microstates: " << entered.size()
                << "\n                    transitions: " << total_entered
                << std::endl;
    } else {
      // filter data
      std::size_t selected_state = args["state"].as<std::size_t>();
      CoordsFile::FilePointer coords_in = CoordsFile::open(args["coords"].as<std::string>(), "r");
      CoordsFile::FilePointer coords_out = CoordsFile::open(args["output"].as<std::string>(), "w");
      for (std::size_t s: states) {
        if (s == selected_state) {
          coords_out->write(coords_in->next());
        } else {
          coords_in->next();
        }
      }
    }
  }
} // end namespace Filter
} // end namespace Clustering

