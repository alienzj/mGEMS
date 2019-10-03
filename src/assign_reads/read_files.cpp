#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <iostream>
#include <utility>

#include "assign_reads/read_files.h"
#include "zstr/zstr.hpp"

void read_header(const std::string &header_line, std::unordered_map<std::string, long unsigned> &ref_to_id, long unsigned &ref_id) {
  if (header_line.at(1) == 'S') {
    std::stringstream parts(header_line);
    std::string part;
    
    short firstel = 0;
    while (getline(parts, part, '\t')) {
      if (firstel == 1) {
	ref_to_id[part.substr(3)] = ref_id;
	++ref_id;
      }
      ++firstel;
    }
  }
}

void read_alignment(const std::string &alignment_line, const std::unordered_map<std::string, long unsigned> &ref_to_id, std::unordered_map<std::string, std::vector<bool>> &read_to_ec) {
  std::stringstream parts(alignment_line);
  std::string part;
  unsigned n_refs = ref_to_id.size();
  
  short firstel = 0;
  std::string read_id;
  while (getline(parts, part, '\t') && firstel < 3) { // we only want the read (0) and reference (2)
    if (firstel == 0) {
      read_id = part;
      if (read_to_ec.find(part) == read_to_ec.end()) {
	std::vector<bool> ec_config(n_refs, 0);
	read_to_ec[part] = ec_config;
      }
    } else if (firstel == 2) {
      if (part != "*") { // star signifies no alignment
	read_to_ec[read_id][ref_to_id.at(part)] = 1;
      }
    }
    ++firstel;
  }
}

std::unordered_map<std::vector<bool>, long unsigned> read_ec_ids(std::istream &ec_file, const std::unordered_map<std::vector<bool>, std::vector<std::string>> &reads_in_ec) {
  std::unordered_map<std::vector<bool>, long unsigned> ec_to_id;
  unsigned n_refs = reads_in_ec.begin()->first.size();
  if (ec_file.good()) {
    std::string line;
    while (getline(ec_file, line)) {
      std::string part;
      std::stringstream partition(line);
      bool firstel = true;
      long unsigned key = 0;
      std::vector<bool> config(n_refs, 0);
      while (getline(partition, part, '\t')) {
	if (firstel) {
	  key = std::stoi(part);
	  firstel = false;
	} else {
	  std::string one;
	  std::stringstream ones(part);
	  while (getline(ones, one, ',')) {
	    unsigned makeone = std::stoi(one);
	    config[makeone] = 1;
	  }
	}
      }
      if (reads_in_ec.find(config) != reads_in_ec.end()) {
        ec_to_id[config] = key;
      }
    }
  }
  return ec_to_id;
}

std::vector<long double> read_abundances(std::istream &abundances_file, std::vector<std::string> &ref_names) {
  std::vector<long double> abundances;
  if (abundances_file.good()) {
    std::string line;
    while (getline(abundances_file, line)) {
      if (line.at(0) != '#') { // skip header lines
	bool abundance = false;
	std::string part;
	std::stringstream partition(line);
	while (getline(partition, part, '\t')) {
	  if (abundance) {
	    abundances.emplace_back(std::stold(part));
	  } else {
	    ref_names.push_back(part);
	    abundance = true;
	  }
	}
      }
    }
  }
  return abundances;
}

void read_probs(const std::vector<long double> &abundances, std::istream &probs_file, std::unordered_map<long unsigned, std::vector<bool>> *ec_to_cluster) {
  uint16_t num_refs = abundances.size();
  if (probs_file.good()) {
    std::string line;
    getline(probs_file, line); // 1st line is header
    uint64_t linenr = 0;
    while (getline(probs_file, line)) {
      std::string part;
      std::stringstream partition(line);
      uint16_t ref_id = 0;
      uint64_t ec_id;
      ++linenr;

      while(getline(partition, part, ',')) {
	if (ref_id == 0) {
	  ec_id = std::stoul(part);
	  std::vector<bool> refs(num_refs, false);
	  ec_to_cluster->insert(std::make_pair(ec_id, refs));
	  ++ref_id;
	} else {
	  long double abundance = std::stold(part);
	  ec_to_cluster->at(ec_id)[ref_id - 1] = (abundance >= abundances.at(ref_id - 1));
	  ++ref_id;
	}
      }
    }
  }
}

std::unordered_map<std::vector<bool>, std::vector<std::string>> read_sam(std::istream &sam_file) {
  std::unordered_map<std::string, long unsigned> ref_to_id;
  std::unordered_map<std::string, std::vector<bool>> read_to_ec;
  std::unordered_map<std::vector<bool>, std::vector<std::string>> reads_in_ec;
  if (sam_file.good()) {
    std::string line;
    long unsigned ref_id = 0;
    while (getline(sam_file, line)) {
      if (line.at(0) == '@') {
	read_header(line, ref_to_id, ref_id);
      } else {
	read_alignment(line, ref_to_id, read_to_ec);
      }
    }
  }

  // Assign reads to equivalence classes.
  for (auto kv : read_to_ec) {
    if (reads_in_ec.find(kv.second) == reads_in_ec.end()) {
      std::vector<std::string> reads;
      reads_in_ec[kv.second] = reads;
    }
    reads_in_ec[kv.second].emplace_back(kv.first);
  }

  return reads_in_ec;
}

void reads_in_ec(std::istream &sam_file, const std::string &ec_path, std::unordered_map<long unsigned, std::vector<std::string>> *reads_in_ec_num) {
  const std::unordered_map<std::vector<bool>, std::vector<std::string>> &reads_in_ec = read_sam(sam_file);
  zstr::ifstream ec_file(ec_path);
  const std::unordered_map<std::vector<bool>, long unsigned> &ec_to_id = read_ec_ids(ec_file, reads_in_ec);
  reads_in_ec_num->reserve(ec_to_id.bucket_count());
  for (auto kv : reads_in_ec) {
    if (ec_to_id.find(kv.first) != ec_to_id.end()) {
      reads_in_ec_num->insert(make_pair(ec_to_id.at(kv.first), kv.second));
    }
  }
}

void read_assignments(std::istream &assignments_file, std::unordered_map<long unsigned, std::vector<std::string>> *assignments) {
  if (assignments_file.good()) {
    std::string line;
    while(getline(assignments_file, line)) {
      std::string part;
      std::stringstream partition(line);
      bool at_ec_id = true;
      uint64_t current_ec_id;
      while (getline(partition, part, ',')) {	
	if (at_ec_id) {
	  current_ec_id = std::stoul(part);
	  std::vector<std::string> reads;
	  assignments->insert(std::make_pair(current_ec_id, reads));
	  at_ec_id = false;
	} else {
	  assignments->at(current_ec_id).push_back(part);
	}
      }
    }
  }
}

void read_groups(const std::vector<std::string> &ref_names, std::istream &groups_file, std::vector<short unsigned> *group_indices) {
  std::set<std::string> groups;
  if (groups_file.good()) {
    std::string line;
    while(getline(groups_file, line)) {
      std::string part;
      std::stringstream partition(line);
      while (getline(partition, part, '\t')) {
	groups.insert(part);
      }
    }
  }
  for (size_t i = 0; i < ref_names.size(); ++i) {
    if (groups.find(ref_names.at(i)) != groups.end()) {
      group_indices->push_back(i);
    }
  }
}
