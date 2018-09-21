#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

#include <read_files.h>

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

std::unordered_map<std::vector<bool>, long unsigned> read_ec_ids(const std::string &ec_path, const std::unordered_map<std::vector<bool>, std::vector<std::string>> &reads_in_ec) {
  std::unordered_map<std::vector<bool>, long unsigned> ec_to_id;
  unsigned n_refs = reads_in_ec.begin()->first.size();
  std::ifstream ec_file(ec_path);
  if (ec_file.is_open()) {
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

std::vector<double> read_abundances(const std::string &abundances_path, std::vector<std::string> &ref_names) {
  std::ifstream abundances_file(abundances_path);
  std::vector<double> abundances;

  if (abundances_file.is_open()) {
    std::string line;
    getline(abundances_file, line); // 2 first lines are header
    getline(abundances_file, line); // todo: identify header by first char == #

    while (getline(abundances_file, line)) {
      bool abundance = false;
      std::string part;
      std::stringstream partition(line);
      while (getline(partition, part, '\t')) {
	if (abundance) {
	  abundances.emplace_back(std::stod(part));
	} else {
	  ref_names.push_back(part);
	  abundance = true;
	}
      }
    }
  }
  return abundances;
}

std::unordered_map<long unsigned, std::vector<bool>> read_probs(const std::string &probs_path, const std::string &abundances_path, std::vector<std::string> &ref_names) {
  std::vector<double> abundances = read_abundances(abundances_path, ref_names);
  std::ifstream probs_file(probs_path);

  std::unordered_map<long unsigned, std::vector<bool>> ec_to_cluster;
  if (probs_file.is_open()) {
    std::string line;
    getline(probs_file, line); // 1st line is header
    while (getline(probs_file, line)) {
      std::string part;
      std::stringstream partition(line);
      short unsigned ref_id = 0;
      long unsigned ec_id;
      while(getline(partition, part, ',')) {
	if (ref_id == 0) {
	  ec_id = std::stoi(part);
	  std::vector<bool> refs(abundances.size());
	  ec_to_cluster[ec_id] = refs;
	  ++ref_id;
	} else {
	  double abundance = std::stod(part);
	  ec_to_cluster[ec_id][ref_id - 1] = (abundance >= abundances[ref_id - 1]);
	  ++ref_id;
	}
      }
    }
  }
  return ec_to_cluster;
}

std::unordered_map<std::vector<bool>, std::vector<std::string>> read_sam(const std::string &sam_path) {
  std::ifstream sam_file(sam_path);
  std::unordered_map<std::string, long unsigned> ref_to_id;
  std::unordered_map<std::string, std::vector<bool>> read_to_ec;
  std::unordered_map<std::vector<bool>, std::vector<std::string>> reads_in_ec;
  if (sam_file.is_open()) {
    std::string line;
    long unsigned ref_id = 0;
    while (getline(sam_file, line)) {
      if (line.at(0) == '@') {
	read_header(line, ref_to_id, ref_id);
      } else {
	read_alignment(line, ref_to_id, read_to_ec);
      }
    }
    sam_file.close();
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
